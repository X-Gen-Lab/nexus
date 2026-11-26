# F407 电驱静态装配软件参考

本消费者验证 `injected-power-stage` 的真实 TOML、不可变选项、强类型 factory、
静态构造和 ARM 链接。`main.c` 只取得只读接口并查询、关闭 cold 资源，不启动电机。

`control.toml` 使用 STM32F407ZGT6 的 TIM8 三对互补输出、PA6 BKIN、ADC1 四个顺序
injected ranks，以及 TIM4 PD12/PD13 编码器。前三个 ADC rank 表示 raw 相电流，
第四个表示 raw 母线输入；没有默认零偏、增益、电机或功率板。

`control.toml` 是边沿对齐。四个 ADC rank 不同时采样，TIM4 计数也是 IRQ 中的软件观察，
不能据此声明同步采样孔径或精确转子角。完整输出必须落在声明的 duty 范围；deadtime
换算、模拟稳定窗口、转换、IRQ 与计算预算都需要符合所选模式。

`board/board.json` 明确标记为软件参考，没有已审 PCB、功率级接线或电气安全声明。
模型、配置与链接分别建立软件事实；物理采样、输出、保护、转矩和时延仍未执行。

## 静态共同域与可选测量

| 装配 | 选择与边界 |
| --- | --- |
| `single.toml` | TIM8／ADC1 两路顺序电流；不要求母线 rank 或编码器 |
| `dual.toml` | TIM8／ADC1 与 TIM1／ADC2 两轴，每轴两路顺序电流；共同 ADC 配置和 IRQ 只有一个 owner |
| `fast.toml` | 单轴 20 kHz 配置与预算参考；5 µs 计算、2 µs IRQ 预算是声明，尚无实测依据 |
| `pair.toml` | 单轴 ADC1／ADC2 injected simultaneous，两相同采样时间；不要求母线或角度 |
| `center.toml` | 完整上下数载波，下降段采样；显式占用 TIM8_UP_TIM13 IRQ 和 TIM13 peer |
| `tdm.toml` | 两个中心载波相差半周期，共用 ADC1；显式启动容差和不重叠的捕获至重臂区间 |
| `dc.toml` | 边沿载波、单个 DC-link ADC1 channel；两次 active-vector 观察后才完成一个控制周期 |

`domain_board/board.json` 只描述精确芯片的候选软件路线。`domain = "current"`
显式连接两轴共同域，生成的成员与配置指针表按实际数量定容；单轴只生成一个成员。
普通 ADC 或另一个域不能取得已经归属该域的 ADC／IRQ。

共同域先于首个成员准备，在最后一个成员关闭后释放；公共 factory 仍直接返回各轴
静态 face。`dual.toml` 使用不同 ADC 的独立序列，`pair.toml` 则选择单轴双 ADC 同时采样。
每种模式都通过精确路线选取；角度和母线来源由调用者另行组合。

所有装配复用本目录的 `CMakeLists.txt` 和 `main.c`，通过
`NEXUS_ASSEMBLY_FILE` 选择。`main.c` 只验证冷态装配和公共查询，不发出运动授权。

中心配置使用完整载波 ticks：硬件 ARR 为 `period_ticks / 2`，偶数输出宽度映射到
CCR 的一半。update IRQ 在下降段开始后打开 injected trigger，JEOC 后关闭，避免
同一个 CCR 在另一斜坡产生第二次观察。`trigger_ticks` 必须晚于声明的 update
IRQ 延迟预算，转换及稳定窗口必须早于下一个高侧导通边界。

边沿路线仍拒绝 `center_aligned = true`。中心能力由专用路线选择，未选模式不占用
update IRQ 或 TIM13；模型、ARM 链接与真实波形分别验证。

`tdm.toml` 首个成员的 `phase_offset_ticks = 0` 建立实际 Core 时间原点，第二个成员
为半周期。两者的 `start_tolerance_us` 必须一致；资源预留在该容差外增加 2 µs，
覆盖两个 Core／carrier 锚点的量化区间。每个区间包含 ADC 转换、IRQ 延迟、计算和
重臂 guard，跨周期相交也会拒绝。运行时仍检查真实 slot 和 capture owner；这些
声明与软件模型不等于已测 WCET 或模拟稳定时间。此配置选择顺序 ADC1，两轴与
同时 ADC pair 的组合没有随本参考开放。

`dc.toml` 的两个原始 DC code 不代表两相直接测量，公共相电流 count/mask 均为零。
实际会话计划必须携带两个独立 active-vector 的时点和开关状态，描述符放在会话声明
的长期算法 context 内；provider 复制计划值，再以真实一次/二次捕获顺序发布完整帧。
第一次完成只重臂第二个未来 compare，第二次完成才调用一次计算并提交下一组计划。
不提供脉冲相移、自动造窗或 neutral 同占空比 fallback；没有足够窗口就拒绝计划。
初始非零驱动计划需要外部已完成调试与授权，本消费者不会生成该授权或启动会话。
软件模型覆盖两个周期的 sequence 和停止排空；真实电流、采样精度和控制稳定性尚未验收。

## 实板工装准备

当前所有模式的 HIL 状态均为 `not_executed`。此目录的 `main.c` 不启动控制会话，
不能单靠烧录该消费者验收电流环。实板工装需另行提供有界控制回调、采样输入、
测量探头与外部保护；验证记录绑定精确芯片、装配 TOML、源码和 ELF 身份。

1. 按所选 TOML 编译并保存生成配置、ELF、map 与资源报告，核对实际引脚接线。
2. 先在无动力输出条件下测量时钟、互补输出、死区、BKIN 和关闭初始状态。
3. 接入受控 ADC 信号源，按下表观察实际采样顺序、载波坐标和组完成次数。
4. 注入 BKIN、晚到 IRQ、超预算回调与停止/恢复，核对输出抑制、borrow 排空及
   未完成会话的保留行为；全程使用独立测量，不能把配置预算当成实测时延。

| 装配 | 需要采集的物理证据 | 当前状态 |
| --- | --- | --- |
| `control.toml` | 四 rank 顺序、通道 skew、母线 rank 和独立 TIM4 计数 | `not_executed` |
| `single.toml`／`fast.toml` | 两 rank 电流、一次载波一次完成；20 kHz 的 IRQ/计算余量 | `not_executed` |
| `dual.toml` | 两个独立 ADC 的样本来源、共享 IRQ 排序、单轴停止不破坏另一轴 | `not_executed` |
| `pair.toml` | ADC1/2 同次 injected 请求及真实采样孔径、部分完成与错误退出 | `not_executed` |
| `center.toml` | 上下数周期、下降段唯一触发、update gate 与实际低侧窗口 | `not_executed` |
| `tdm.toml` | 两轴半周期相位、启动容差、ADC1 slot 归属、错 slot 时组抑制 | `not_executed` |
| `dc.toml` | 两个 active-vector 窗口、首捕获后 CCR4 重臂、code/开关状态来源、恰一次 compute、窗口不足抑制 | `not_executed` |
