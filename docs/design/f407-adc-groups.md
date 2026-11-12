---
id: design.f407_adc_groups
type: reference
audience: provider
source_language: zh_CN
---

# F407 多 ADC 与注入采集

本页描述 STM32F407VET6／ZGT6 的可选 `adc_group`，依据 RM0090 Rev22 的多 ADC、
注入序列和 DMA 模式，以及 DS8626 Rev12 的封装引脚。普通 ADC、PWM/ADC 和本接口
共用资源图；同一硬件域不会由两个接口同时操作。静态实例、只读方法表和 factory
的关系见 [factory 图](../explanation/factory.md)，请求借用见
[生命周期图](../assets/diagrams/request-lifecycle.svg)。

## 唯一责任者与装配

`ADC123` 是一个逻辑 controller，独占 ADC1／2／3、公共 CCR、ADC IRQ，以及选中
模式需要的 timer 和 DMA2 stream0 channel0。双 ADC 或单 ADC 注入也独占这个公共域，
以防另一 owner 改变 MULTI、DMA 或分频。TIM2 留给 Core；不隐式接纳外部 EXTI、
动态 timer 或另一个 ADC controller。普通三个独立实例仍通过各自的 `adc` 接口
选择，它们不具有多 ADC 同步语义。

一份 TOML 选择操作、分辨率、采样序列、触发与有限排空预算。Host resolver 核验
精确 Board 授权、通道、相同物理 pad、DMA 和 IRQ 冲突，生成静态常量、实例、
强类型 factory 与薄 IRQ 连接。采样、寄存器时序、等待、停止和恢复全部由 C11
provider 执行。生成器没有硬件操作脚本或另一套运行时状态机。

Platform prepare 验证空闲的公共域与中断，取得并回读所需时钟，再重复检查原先可能
被门控的寄存器，最后配置授权输入为 analog／no-pull。它不启动转换。factory
查询不初始化硬件。Platform 记录引脚原值，完整关闭后才恢复；局部失败继续保留
相应资源，不能把无法读回的 gate 或 pad 当作已释放。

## 模式与内存格式

| 配置模式／operation | 转换与输出 |
| --- | --- |
| `regular-sequence-dma`／`dual-simultaneous` | ADC1／2 同步，每组 1–16 ranks，CDR mode2，word DMA |
| `regular-sequence-dma`／`triple-simultaneous` | ADC1／2／3 同步，每组 1–16 ranks，CDR mode1，halfword DMA |
| `regular-sequence-dma`／`dual-interleaved` | ADC1／2 交错，同一物理输入，每 frame 两个结果 |
| `regular-sequence-dma`／`triple-interleaved` | ADC1／2／3 交错，同一物理输入，每 frame 六个结果 |
| `injected-sequence`／`single` | ADC1 单次注入，1–4 ranks，无 DMA |
| `injected-sequence`／`dual` | ADC1／2 同步单次注入，1–4 ranks，无 DMA |
| `injected-sequence`／`triple` | ADC1／2／3 同步单次注入，1–4 ranks，无 DMA |
| `regular-blocks`／双／三 ADC 同步或交错 | 调用者预投递的完整 CDR block，DBM 循环，无隐藏 buffer |
| `regular-software-injected-blocks` | 同步 regular blocks 与一个显式软件注入 request 共用 owner |
| `regular-timer-injected-blocks` | 同步 regular blocks 与独占 Table 69 timer 的单次注入共用 owner |
| `regular-auto-injected` | 一个 regular 序列后自动注入，两个结果 span 联合结清一次 |


同步序列按 rank 排列，每个 rank 内依次 ADC1、ADC2、ADC3；同一 rank 的采样时间
相同。不同转换器不能在同一时刻采同一物理 pad；比较依据实际引脚，而非仅比较
channel 数字。ADC3 channel4 对应 PF6，与 ADC1 channel4 的 PA4 不同。交错模式
反而要求同一物理输入；三 ADC 只开放三个转换器确实共同拥有的输入。

交错的三 ADC frame 为 `1,2,3,1,2,3`，保持完整的三个 CDR 传输单位。12／10 位
采用 mode2，每样本两个字节、DMA 每次四字节；8／6 位采用 mode3，每样本一个字节、
DMA 每次两字节。同步模式和注入模式始终保留两个字节的原始整数。没有 CPU 搬运、
重排、补齐或隐藏采样缓冲。

通过 `info.format` 查询实际 storage、sample／scan stride、有效位数、组、触发和
顺序；`info.converters` 与 `channels` 是每个逻辑结果的静态身份。
`buffer_alignment` 表示真实 DMA 访问对齐，可能大于一个样本。raw request 的 `bytes`
必须精确等于 `frame_count * scan_stride`；NDTR 不超过 65535 个实际 DMA 访问。
显式 paired request 的 `bytes` 则是结果 descriptor 大小，两个数据 span 各按独立
layout 核验，不能把 descriptor 当作 DMA 数据。
DMA 目的地必须属于所选可写 SRAM 域，CCM 不可达。独立有限注入只接受一个完整 frame，
停止并排空后才按 JDR1–4 顺序写调用者缓冲；共用 owner 的注入见下节。

`resolution_bits` 可选 6／8／10／12；重复通道必须保持一致采样时间。模拟看门狗
显式选择原始码上下界，越界作为本次 IO 错误处理，不执行产品保护政策。减少位数
时，C provider 按硬件比较器规则移位阈值；它不改变数字输出的原始码含义。

## 触发、时间与有限完成

周期性同步 regular 由独占 timer 触发，timer period 必须严格大于所选序列的名义
转换预算。`regular-auto-injected` 只产生一个 one-pulse，`period_us` 表示首次边沿
延迟，不冒充周期性采样间隔；它可以短于序列转换时间。
可选 TIM1 CH1／2／3、TIM3 CH1／TRGO、TIM4 CH4、TIM5 CH1／2／3、
TIM8 CH1／TRGO。配置期计算真实 APB timer clock 和整除分频，不隐式改慢触发。

注入支持软件启动，或 TIM1 CH4／TRGO、TIM3 CH2／4、TIM4 CH1／2／3／TRGO、
TIM5 CH4／TRGO、TIM8 CH2／3／4。硬件触发采用 one-pulse，避免第二次事件覆盖 JDR。
有限交错模式采用软件启动、有限 DMA 和显式 5–20 ADCCLK 间隔。交错输出不把名义 spacing
误报为完整 frame 时间；`physical_scan_halfcycles=0` 表示未提供该完整时长。

调用发生在同一特权、IRQ 未屏蔽的 Thread 执行者中。外部串行化由调用方负责，
不建立 OS 锁、task、queue 或隐藏自动 rearm。deadline 是当前 Core 单调时间域的
有限绝对值；重启时间域后不能复用旧 deadline。ADON 后的 warmup 包含微秒取整
裕量。IRQ 锁存 ADC／DMA 事实并关闭已完成的发布源，只在所选终端条件成立后
门控采样；任务推进 service 承担完整排空和最终发布。

## 循环 blocks 与规则／注入共用 owner

`regular-blocks` 不提供有限 `submit`。调用者通过同一个 `nx_adc_group_port_t` 的
`start_blocks` 提供至少三个相等、FREE 且完整落在 DMA 域内的 slot。普通有限实例
不保留 stream、producer token 或 observer；可选 blocks state 仍是 168 B 的实测
ARM 布局。SUCCESS 表示接纳会话，warmup 后仍需显式 service 才开始采样。

两个 DMA bank 与下一次替代 slot 共用现有 posted producer。先验证 CT／TC，替换
非活动 bank 的地址并回读，再发布旧 bank；不会把消费者持有的旧数据覆盖。没有
足够 FREE slot 时停止转换、完成排空，再等待归还；恢复经过 MULTI=0，产生新的
不连续边界，不保证暂停期间不丢样。stop 不能撤销已发布 consumer loan；归还后
才能完成 stream／observer 的 join。observer 使用已有调用者记录，只提供软件
完成观察时间与 epoch，不能解释为精确采样孔径或硬件触发计数。

交错 blocks 保持有限模式的原生 CDR 几何：三 ADC 的完整 frame 为六个结果，低／高
lane 的循环为 `1,2 / 3,1 / 2,3`。12／10 位每次 DMA word，8／6 位每次 halfword；
所有参与 ADC 都置 CONT，从 ADC1 软件启动，没有 timer 资源。RM0090 Rev22
§13.9.3 pp409–410 的 DELAY 是最小转换间隔，sampling 冲突会使间隔更长，因此
`physical_scan_halfcycles=0`，不承诺均匀 aperture。

`max_service_gap_us` 是调用者必须满足的上界。交错准入用各 ADC 的最快转换吞吐
上界，不用 DELAY 假造固定周期。若一个 bank 有 N 个原生样本、k 个 ADC、S 个采样
时钟、分辨率对应的 C 个转换时钟、f 个 ADCCLK／s、G 微秒服务间隔，则要求
`(N-B)*(S+C)*1000000 > (2*G+2)*k*f` 且 `N>B`。
B 保守预留每个 DR 与进行中的转换、四 word DMA FIFO 及两个 SINGLE AHB 在途
word，即 `B=2*k+24/storage_bytes`。`storage_bytes` 是每个原生样本的内存容器字节数：
12／10 位为 2，8／6 位为 1；依据 RM0090 §10.3.6、§10.3.18。FIFO 直接模式
也不能按无缓冲处理。预算不足直接拒绝，不分配更大的 buffer；时间预算丢失时
不发布无法证明的 bank。

两个 `regular-*-injected-blocks` 模式仅允许同步 dual／triple，不能与交错混用。
regular 与 injected 的 ranks／采样时间在 ADC 关闭时一起安装；它们不是两个 ADC
owner。`submit_injected` 接受一个按 `injected_info` 布局准备的 raw request。
软件模式发出一次 JSWSTART；timer 模式独占另一条 Table 69 timer，先 UG、后
JEXTEN，最后提交 one-pulse。规则与注入 timer 的真实 APB 时钟分别解析，不互相
覆盖；TIM2、EXTI15 以及同一 timer bank 的双重使用都拒绝。

`injected.min_interval_us` 连同 regular、injected 和被抢占的一个 regular conversion
共同约束转换预算。取消注入后仍等待所有 JEOC，或停止并 join 整个 controller；
失败不能只释放注入缓冲而保留仍可能访问它的 regular／IRQ owner。数值恢复时钟
不能修补已丢失的硬件 epoch。

## 有限 JAUTO 双结果

`regular-auto-injected` 只支持一次同步 dual／triple regular→injected acquisition。
TOML 的 `injected` 只包含 `channels`／`sample_times`，不接纳独立 trigger、
`min_interval_us` 或循环 service-gap。软件在 ADON 关闭时配置 MULTI、两个序列及
JAUTO；JEXTEN／CONT／discontinuous 保持关闭。开始新请求前，实际读回 ADC_SR
写零已清除旧状态，旧 JEOC 不能满足新请求。依据 RM0090 §13.3.9 p396、
§13.9.1／§13.9.5，以及 ADC_SR 的 rc_w0 定义。

调用者用 `nx_adc_group_prepare_results` 准备一个具名的
`nx_adc_group_results_t`，其中 regular／injected 各含 data 与 bytes，随后通过同一
`submit` 提交。两个 `info` 返回各自的整数布局；injected 的 trigger 为 EVENT，
表示 regular 序列结束引发的 JAUTO，不表示另一条独立 timer。descriptor、request
和两个 span 必须互不重叠，也不能覆盖 System、provider 或静态事实。原 raw
prepare 会设置 RAW discriminant；不具备 paired 能力的 provider 明确拒绝 paired
request，不根据运行态重新解释内存。

DMA TC 只证明 regular 存储完成，不能提前关闭仍在转换的 injected ADC；所有
JEOC 也不能代替 DMA 退出。只有 TC 与全部 JEOC 都成立，且 source／ADC／DMA／
IRQ／地址联合退出后，才能读取 JDR 并共同发布一个成功 frame。任意错误保证零个
有效联合 frame；取消、超时或拒绝排空持续借用 descriptor 和两个完整 span。
当前实测 ARM request 和共享 ops 均维持 40 B，只有所选 JAUTO 实例保留 64 B state。
这些是结构和软件证据，不是模拟吞吐或电气资格。

## 借用、故障和关闭

拒绝不取得 request 或输出借用。接受后，request 与整个输出由 provider 独占至
acquire 观察到 SETTLED。取消或超时只确定终止原因，不撤销借用。成功恰好报告请求
的完整 frame 数；错误报告零个有效 frame，不能把 DMA 前缀当作成功数据。

submit 返回 SUCCESS 只表示接纳请求。接纳后，prime 的有限 warmup 无法表示时，
同一 request 转入排空，service 在实际退出后以 EXHAUSTED 和零进度结清；不返回
虚假的无借用拒绝。有限排空预算无法表示时保持立即到期，不改成 NEVER；先尝试
真实硬件退出，仍不能退出才保留 QUARANTINED、request 和完整缓冲供同一 owner 重试。

排空依次停止触发与转换、撤 DMA 请求、禁用并回读 DMA EN、稳定 NDTR、清除实际
硬件地址、join ADC／DMA IRQ，然后把 MULTI 撤回 independent。最终 ADC 错误和
DMA 错误仍参与结果判定；最后一次观察不能因进入清理而被忽略。没有可信完成时间戳
时，完成首次在 deadline 之后被观察仍返回 TIMEOUT。

无法停止、清除地址或 join IRQ 时保留 QUARANTINED、缓冲与配置。调用者可以在原
实例上重试 stop／service；不得改写或回收缓冲。普通 stop 排空当前请求并清除可恢复
故障，保留静态资源供下一次请求；Platform close 则封闭准入，恢复公共配置并归还
时钟、DMA 和引脚。关闭失败不能再次开放采样，也不能换配置继续清理。

## 软件参考与边界

完整软件装配位于 `tests/contracts/adc_group_assembly/`，涵盖 VE／ZG、双／三 ADC、
同步／交错／注入和裸机／FreeRTOS。它们声明测试用片内路线，不能代替真实 PCB
原理图。GoogleTest 执行真实 provider 与生成 factory 路径；ARM ELF 检查所选编译、
链接与资源；Source SDK 检查外部消费闭包。各类证据独立记录，不继承旧计数。

当前不提供连续 JAUTO 的逐次 JDR 结果保证、交错与注入并行、运行期换 sequence、
未建模的外部 trigger owner、内部通道多 ADC 或模拟工程量换算。上述 timer 注入
不代表接纳任意外部事件；有限 paired JAUTO 不代表连续自动采集。所列模式不承诺
实测同步偏差、吞吐、IRQ 延迟、模拟精度或物理看门狗响应；实板为 `not_executed`。
