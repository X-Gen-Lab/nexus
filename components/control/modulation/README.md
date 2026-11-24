# 输出与采样联合规划

`Nexus::ModulationControl` 提供纯 C11 的三相 PWM、电流采样窗口与最终电压规划。
公共入口为 `nexus/control/modulation.h`，不访问寄存器、OS、堆或隐式时钟。
调用者提供不可变 profile，将完整结果关联到实际 cycle／epoch 后交给唯一 provider。

普通两／三分流使用 `nx_modulation_phase_plan`，单分流使用
`nx_modulation_bus_plan`；两条实现与共同几何分别位于独立翻译单元。
显式全拓扑消费者才使用 `nx_modulation_plan`，避免普通相电流路径链接单分流规划。

| 模式 | 观测要求 |
| --- | --- |
| 三低侧分流 | 动态选择两个有效相，或在三相均可观测时采集三个值 |
| 双低侧分流 | 两个固定传感相均处于有效窗口 |
| 单母线分流 | 两个时间有序、观察不同相的有效开关矢量 |

`period_ticks` 是完整展开载波周期，不能直接当作厂商 ARR。
每相由 `start_ticks` 和高脉冲总宽度 `duty_ticks` 表达，允许跨周期边界。
中心对齐的周期和宽度为偶数；实际计数方向、CCR、ADC 和 IRQ 映射属于 provider。

规划共同确定量化脉冲、采样点和相位／矢量映射，分别检查模拟稳定、ADC 占用及
IRQ／计算提交预算。`aperture_ticks` 是所选采样保持集合的包络，
`conversion_ticks` 是从触发到完整结果可用的占用上界，包含该包络。
这些值是经调用者审核的声明，不是程序测得的 WCET 或采样精度。

`capture_begin_ticks`／`capture_end_ticks` 由 provider 提供，限制它实际支持的
半载波或共享 ADC 预留槽；end 为零表示完整周期。触发、转换、IRQ 抽取及 rearm
均须落在槽内，并同时满足最终提交预算。业务代码不猜测 TIM ARR 或计数斜坡。

无观测窗口返回 `UNSUPPORTED`，输出保持不变。单分流可以显式选中一个固定相移候选，
该候选保持每相平均脉宽，并重新检查两个有效矢量。它要求 mapper 支持全部脉冲起点；
只支持普通三 CCR 的实现必须拒绝，不能忽略 `start_ticks`。
纯 C 规划存在不构成该相移模式的 MCU 或实板资格。

`voltage` 由最终量化宽度回算，仅表示理想平均 alpha/beta 电压，
不包含未测得的死区和器件压降。控制器在最终计划后做一次对应周期的 tracking。
provider 应提交整份计划，禁止再次静默裁剪 PWM 或保留旧采样映射。
