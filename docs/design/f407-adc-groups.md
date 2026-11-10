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
`buffer_alignment` 表示真实 DMA 访问对齐，可能大于一个样本。request 的 `bytes`
必须精确等于 `frame_count * scan_stride`；NDTR 不超过 65535 个实际 DMA 访问。
DMA 目的地必须属于所选可写 SRAM 域，CCM 不可达。注入只接受一个完整 frame，
停止并排空后才按 JDR1–4 顺序写调用者缓冲。

`resolution_bits` 可选 6／8／10／12；重复通道必须保持一致采样时间。模拟看门狗
显式选择原始码上下界，越界作为本次 IO 错误处理，不执行产品保护政策。减少位数
时，C provider 按硬件比较器规则移位阈值；它不改变数字输出的原始码含义。

## 触发、时间与有限完成

同步 regular 由独占 timer 触发，timer period 必须严格大于所选序列的名义转换预算。
可选 TIM1 CH1／2／3、TIM3 CH1／TRGO、TIM4 CH4、TIM5 CH1／2／3、
TIM8 CH1／TRGO。配置期计算真实 APB timer clock 和整除分频，不隐式改慢触发。

注入支持软件启动，或 TIM1 CH4／TRGO、TIM3 CH2／4、TIM4 CH1／2／3／TRGO、
TIM5 CH4／TRGO、TIM8 CH2／3／4。硬件触发采用 one-pulse，避免第二次事件覆盖 JDR。
交错采用软件启动、有限 DMA 和显式 5–20 ADCCLK 间隔。交错输出不把名义 spacing
误报为完整 frame 时间；`physical_scan_halfcycles=0` 表示未提供该完整时长。

调用发生在同一特权、IRQ 未屏蔽的 Thread 执行者中。外部串行化由调用方负责，
不建立 OS 锁、task、queue 或隐藏自动 rearm。deadline 是当前 Core 单调时间域的
有限绝对值；重启时间域后不能复用旧 deadline。ADON 后的 warmup 包含微秒取整
裕量。IRQ 只锁存所选 ADC／DMA 事实、撤事件源，并尽快门控采样；任务推进 service
承担完整排空和最终发布。

## 借用、故障和关闭

拒绝不取得 request 或输出借用。接受后，request 与整个输出由 provider 独占至
acquire 观察到 SETTLED。取消或超时只确定终止原因，不撤销借用。成功恰好报告请求
的完整 frame 数；错误报告零个有效 frame，不能把 DMA 前缀当作成功数据。

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

当前不提供 regular 与 injected 同时运行、自动注入、DMA 循环多 ADC、运行期换
sequence、外部 trigger owner、内部通道多 ADC 或模拟工程量换算。所列模式不承诺
实测同步偏差、吞吐、IRQ 延迟、模拟精度或物理看门狗响应；实板为 `not_executed`。
