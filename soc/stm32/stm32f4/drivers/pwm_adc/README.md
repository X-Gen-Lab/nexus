# PWM 与 ADC 的单一硬件 owner

本目录提供 STM32F407ZGT6 的两个显式软件参考模式。一个装配只选择其中一个，
同时独占 TIM8、ADC1／2／3 共享域和所需 IRQ；有限模式另占 DMA2 Stream4。
公共 factory 返回已经存在的只读 face，不启动采样、输出或任务。

```mermaid
flowchart LR
    T[TIM8 CH1～3 PWM] --> P[固定周期与内部 CH4 相位]
    P --> F[有限：TRGO → ADC1 regular → DMA]
    P --> L[连续：CH4 → ADC1 injected → IRQ]
    L --> C[调用者 compute → 完整 duty 组]
    C --> S[UDIS 保护的 shadow 提交]
    B[BKIN／超限／错误] --> Q[撤回输出与采样源]
    F --> J[Thread service 排空与 join]
    Q --> J
```

箭头表示选定生产 C 的硬件事件和执行关系。它不表示已执行实板控制、FOC、采样精度
或电气保护资格。当前软件路线使用 PC6／PC7／PC8 的 TIM8 CH1～3、PA6 BKIN 和
PA0 ADC1_IN0；公开 Board 没有因此获得功率板接线授权。

## 两种模式

| 选择 | 公共接口 | 行为与存储 |
| --- | --- | --- |
| `finite-regular-dma` | `pwm_adc.h` | 一个完整固定 duty 窗口；DMA 借用调用者 raw sample 数组，结束后停止输出；不自动重启 |
| `injected-control` | `pwm_adc_loop.h` | 一个有有限 deadline 的连续 session；ADC IRQ 取得完整 scan，执行调用者 compute，再提交完整 duty 组 |

有限模式采用 RM0090 Rev22 Table68 的 TIM8 TRGO 与 Table44 的 DMA2 Stream4
Channel0。连续模式采用 Table69 的 TIM8 CH4 injected trigger，不使用 DMA。
当前声明路线只选择 ADC1_IN0、12 位无符号原始码和三个非互补 PWM 输出，deadtime
为零；其他通道、互补功率级、传感拓扑和电机算法保持各自明确的后续接入范围。

两模式分别持有可变状态，共用 `analog.c` 的 ADC cold／部分失败撤回和已有 timer
waveform 引擎。连续实例不保留有限 request、DMA loan 或 DMA buffer。复用的
waveform 子状态仍有既有 pulse／sequence 字段成本；实际 ARM 布局应按当前构建
测量，不能将链接夹具全部 `.bss` 当成每实例成本。

## 有限窗口

1. Platform 在 GPIO／共享 DMA 时钟就绪后取得 ADC 与 timer，保持输出和采样关闭。
2. 提交前验证完整 request、duty、sample 数量、DMA 内存域、deadline 与别名。
3. ACCEPTED 后借用 request、duty 和 samples；ADC warmup 后由显式 service 启动。
4. IRQ 只锁存真实 TC／error／break 并抑制源；Thread service 完成输出、ADC、DMA
   地址与 IRQ 退出后发布 `SETTLED`。

DMA TC 不证明精确输出脉冲数。没有可信完成时间时，首次在 deadline 之后观察到
完成仍报告 TIMEOUT。取消或停止失败不允许复用 sample、duty 或 request；恢复
service 必须使用原 owner，无法证明退出时保持 `QUARANTINED`。

## 连续快环与时间前提

连续模式固定 timer tick 为 1 MHz，`period_ticks` 因而也是名义微秒数。配置显式给出
`trigger_ticks`、`commit_before_ticks`、`compute_budget_ticks` 和
`max_irq_delay_us`。后两者是调用方必须满足的上界，软件声明不建立物理 WCET。

配置期与 C 准入共同检查：

```text
conversion_us = ceil(sum(sample_cycles + 12) * 1000000 / adc_clock_hz)

trigger_ticks + conversion_us + max_irq_delay_us
    + compute_budget_ticks + 4 < commit_before_ticks

commit_before_ticks <= period_ticks - 2
```

`compute_budget_ticks` 覆盖完整 scan 取得、compute、整组 shadow 写入与检查。
4 微秒预留 Core 微秒量化、初始锚点和提交检查裕量；它不能代替实际 IRQ、总线和
回调预算验证。频率与可用计算时间由所选 period／采样时间共同限制。

启动后，C owner 在两次 Core 时间读取之间关联 TIM8 CNT；读取间隔超过 1 微秒、
已经发生 update 或首个 trigger 已经过时即拒绝继续运行。由该区间建立持久 Core
时间锚点，每个有效周期严格前移一个 period。JEOC 先通过绝对锚点与 modulo CNT
检查，compute 后再次检查 Core 时间、phase 和完整 shadow 更新。因此，延迟整
个 period 后 CNT 恰好落回窗口也不能被当成新鲜周期。

JEOC 是合并状态，不是硬件转换计数。此模式还要求实际 IRQ 延迟符合声明，Core
时间源与所选 System epoch 持续有效。读取期间或 compute 后再次出现 JEOC、
迟到、callback 超限、break、部分 shadow 写失败均锁存错误并抑制输出，不发布
混合 duty 组。`completed_cycles` 是 64 位已接纳计算／提交计数，不是物理脉冲计数。

## 上下文与借用

prepare／start／service／stop／state／close 由一个 privileged、未屏蔽的 Thread
串行调用。compute 仅运行在选定 ADC IRQ，必须有界；它不能调用 Thread-only
encoder／waveform／文件／网络／OS 方法，不能保存本次 scan／output 的临时指针。

session、sample、duty、callback 及其 context 借用持续到 acquire 观察 `SETTLED`。
context 可以引用独立有界 queue／原子 mailbox，按这些对象自己的合同交接目标；
任务不能并发逐字段修改被借用的 PID、算法历史或 session。opaque context 的
完整跨度由调用方证明，公共 API 和 provider 只拒绝已知起始地址别名。

先停止新目标与使用者，再 stop／service 退出 callback 和 IRQ，最后 Platform
close 归还 cold 资源。输出已关闭与借用已退出分别判断；失败 IRQ join 保留整个
session。显式 stop 后通过 cold close 和重新初始化建立新会话，不自动恢复输出。

## 验证与消费者

- `tests/google/soc/f407_pwm_adc_test.cpp` 编译生产 C，以外部 MMIO／IRQ／时间叶模型
  验证两模式、拒绝、取消、迟到 IRQ、覆盖、借用别名和部分失败。
- `tests/contracts/f407_pwm_adc/` 与 `tests/contracts/f407_pwm_adc_loop/` 验证真实
  TOML、生成 factory 和 ARM 链接／预算；冷态链接消费者不执行实板快环。
- 公共消费者继续通过 `nexus_factory_strong.h` 取得选定 face；有限与连续强类型
  namespace 分开。产品拥有任务、功率板、控制政策和实板测量。

物理测试状态保持 `not_executed`。

## 电驱完整软件参考：injected-power-stage

`power_stage.h/.c` 提供独立的 `nx_power_stage_t` face，直接拥有选定硬件，
不在另一个 `pwm_adc_loop` 实例上转发。它与前述两种模式互斥。当前精确软件路线为：

| 资源 | 本模式选择 |
| --- | --- |
| MCU | STM32F407ZGT6；没有关联真实功率 PCB |
| 输出 | TIM8，边沿对齐 PWM1，CH1／2／3 及三个互补输出 |
| 输出引脚 | PC6／PA7、PC7／PB0、PC8／PB1，AF3 |
| 故障 | PA6 TIM8 BKIN，显式极性；禁止自动输出重新使能 |
| ADC | ADC1 injected，PA0／1／2／3，即 IN0／1／2／3 |
| 数据顺序 | A／B／C 原始电流及原始母线电压，四个**顺序** rank |
| 触发 | TIM8 CH4，官方 LL 定义的 injected selector 14 |
| 编码器 | TIM4 x4，PD12／PD13 AF2，16-bit 原始模数计数 |

这是边沿对齐、单 ADC 顺序采样模式，不声明中心对齐、双／三 ADC 同时采样、
模拟前端精度或真实电机运行。当前通用 waveform 与 analog C 核心仍只有一个
PowerStage owner；同一 TIM4／TIM8／ADC 共享域不能再装配独立接口。

### 固定低侧窗口与预算

调用者明确声明最小／最大 duty、AFE `settling_ticks`、deadtime、触发、提交边界、
最大 IRQ 延迟和 compute 预算。配置与生产 C 均检查：

```text
deadtime_pwm_ticks = ceil(deadtime_tDTS * pwm_tick_hz / timer_hz)
scan_us = ceil(sum(sample_cycles[rank] + 12) * 1000000 / adc_clock_hz)

maximum_duty_ticks + deadtime_pwm_ticks + settling_ticks < trigger_ticks
trigger_ticks + scan_us * ticks_per_us < period_ticks
trigger_ticks + (scan_us + max_irq_delay_us + compute_budget_us + 4)
    * ticks_per_us < commit_before_ticks
commit_before_ticks <= period_ticks - 2 * ticks_per_us
```

三个 PWM1 上侧均关闭且低侧经过死区及声明稳定时间后，才进入四 rank 采样窗口。
最大 duty 限制持续保持这个窗口；不能接纳一个绕过限制的任意调制结果。整数 MHz
计数时钟允许小于微秒的 duty 量化，但 period、trigger 和提交边界须为整数微秒，
以便与当前 Core 时间建立有界锚点。4 µs 裕量覆盖声明的量化与软件检查范围，
不证明实际 IRQ 或回调 WCET。

`sample_at` 是计划触发的时间参考，`scan_span_us` 是四次顺序转换的名义上界；
它们不是实测采样孔径。三相原始值不代表同一瞬时的三个模拟量。控制消费者必须为
选定的电流变化率、AFE、rank 偏移和误差预算证明可接受性，不能只把一个时间戳
复制给三相后声称完成同步测量。

`observed_at` 是完整读取后的软件观察，`angle_at` 是 TIM4 CNT 读取完成的观察；
计数读取前后 Core 间隔超过 1 µs 即拒绝本周期。`planned_apply_at` 是下一次预期
UEV 边界，不是已测得的电气输出生效时间。consumer 应保留这些来源，按明确模型
处理采样角和输出角。raw 到安培／伏特／弧度的标定保持外部显式配置。

### 会话、故障和最终退出

`nx_power_stage_prepare` 只复制完整初始 plan、callback 及其 context 的完整跨度。
`startup_deadline` 有限，分别约束 warmup；Core request 的整体 deadline 为 NEVER，
表示非阻塞、长期借用的运行会话，直到显式 stop 或故障。运动命令的有效期由
Axis／compute 独立监督；不得通过修改已借用的 deadline 或 context 延长旧目标。

所有控制方法由同一 privileged、未屏蔽的 Task 串行调用。ADC IRQ 才调用 compute。
callback 不保存临时 sample／plan，不调用 task-only I/O、OS、Flash 或网络；软件
只能在返回后发现预算超限，不能抢占一个不返回的 callback。

错误首先尽力抑制仍有门钟的 PWM／ADC。门钟或 epoch 失效不阻止对其他仍可访问
硬件的抑制；缺少实际退出证明时继续保留 QUARANTINED。恢复相同的时钟数值不恢复
运行准入，必须完成真实源撤回和 IRQ join。清故障不自动开始新会话。

停止要分别证明输出、ADC、编码器源、ADC IRQ 和 break publisher 退出。只有最终
退出后才 SETTLED，输出已关闭不结束借用。冷态 close 逐项保存恢复进度；timer 已经
释放后，后续 ADC／优先级恢复失败仍可从原 receipt 重试，不重复热态准入。开启门钟
后发现的外部 encoder 状态不会被 cold close 清除，只归还本次增加的门钟。

`tests/google/soc/power_stage_test.cpp` 执行生产 C 和真实公共分发，只有物理 MMIO、
CPU 与时间叶边界由模型提供。`tests/contracts/f407_power_stage/` 检查实际配置、
factory 和 ARM 链接。物理波形、采样精度、转矩、时延与保护测试仍为 `not_executed`。
