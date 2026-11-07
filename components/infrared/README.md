# Explicit NEC infrared software reference

`Nexus::Infrared` is an optional allocation-free C11 decoder. It consumes copied
`nx_exti_event_t` facts from one caller-owned active-low, demodulated, both-edge
source. A deterministic `feed` accepts another equivalent event source; `poll`
reads at most one event through the public EXTI face. The same state machine is
used in both paths. No ISR, GPIO configuration, receiver power, task, heap,
blocking wait or event queue is hidden inside this component.

The default `NX_INFRARED_SOURCE_STRICT` policy rejects every COALESCED edge.
The actual F407 EXTI provider reports IRQ timestamps and sampled GPIO levels,
marks every event COALESCED and may lose intermediate edges. It therefore cannot
feed a strict NEC frame. The separate, explicit
`NX_INFRARED_SOURCE_IRQ_OBSERVATION` software reference accepts that observation
quality. Every published result is `NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN`, with
the original OR of `source_flags` preserved. Even flag-free observations in this
policy cannot upgrade its quality. Repeats inherit predecessor flags as well as
their own. NEC inverse checks do not prove capture completeness or physical
timing. Strict flag-free input is a declared source contract, not hardware proof.
The independently checked IRQ-observation reference is the F407 V4.1 PE2 source
only. The generic decoder's configurable line is not a support claim for another
Board, provider, pin, timestamp source or remote.

The V4.1 schematic labels PE2 `HS0038`. That name does not prove an installed
receiver manufacturer, carrier response or remote protocol. NEC is an explicitly
selected software reference; the actual remote remains unknown. This component
does not add hardware qualification, calibrate timings, map product keys or infer
RC5, NEC2 repeat semantics or another protocol.

The caller supplies a stable readonly `nx_infrared_nec_config_t`. Its six
inclusive microsecond windows define leader mark, full/repeat spaces, bit mark
and zero/one spaces. The two pairs of competing spaces must not overlap. Every
window is nonzero and at most 20000 microseconds. A complete frame budget must
cover its worst permitted 32-bit duration and trailer, at most one second.
Repeat age is explicit and at most five seconds; these are configuration bounds,
not measured timing or receiver tolerance claims.

Standard NEC validates the eight-bit address and its inverse. Separately chosen
extended address mode interprets those two bytes as a little-endian 16-bit
address, without an inverse requirement. Both modes require the command and its
inverse, all 32 least-significant-first bits and the final mark. A failed inverse
never causes a fallback to another address policy. Each ordinary valid full
frame is a new frame; repeated full packets are not relabelled as NEC2 repeats.

`feed` and `poll` return a value containing status and, only on success, a complete
frame. Partial frames never escape. A repeat requires an earlier validated
command within the authored age; accepted repeats extend that age. Malformed
waveforms, LOSS and failed or inconsistent port reads clear both the current
frame and repeat eligibility. STRICT also rejects coalescing. The explicit IRQ
reference preserves that uncertainty instead of clearing the flag. A timed-out
partial frame can resynchronize
at a real falling edge but never publishes its old bits. An idle decoder retains
no external resources when no further edge arrives.

Accepted edges or explicitly allowed IRQ observations advance a strictly
monotonic timestamp history. LOSS detection/report timestamps do not reconstruct
a missing edge. Equal or backward accepted timestamps latch a clock fault until explicit software stop and
reinitialization. The caller must drain/rebind its source before changing clock
epochs. Configuration declarations do not attest the time domain or precision.

One serialized task owns the decoder. Callback reentry into init, feed, poll or
stop returns BUSY. Input spans must be aligned, finite and disjoint from decoder
and configuration; known face, operations and opaque-context anchor aliases are
rejected before callbacks. The full extent and validity of opaque contexts remain
the caller's responsibility. No input or output pointer survives a return.
Software `stop` clears decoder state only: it never stops the EXTI source or
establishes IRQ quiescence. The external owner drains and joins that source.

[References](references.json) bind the actual manufacturer document bytes and
reviewed sections. PDFs remain outside this repository. GoogleTest exercises the
real decoder; GoogleMock replaces only the external EXTI read boundary. Host
models and ARM compilation do not establish physical receiver or remote support.
The independent F407 integration model executes the actual PE2 provider's
initialize, IRQ dispatch, ring-loss reporting and public read path. Its external
MMIO/NVIC/time model does not remove COALESCED. It verifies strict refusal,
explicit sampled decoding, inherited repeat quality, real ring overflow and
clock regression; it does not establish a measured interrupt latency bound.

## 中文合同

`Nexus::Infrared` 是可选的无动态分配 C11 解码器，接收调用者拥有的低有效、已解调、
双边沿 `nx_exti_event_t` 副本。`feed` 可接入等价事件来源；`poll` 每次最多读取一个
公共 EXTI 事件，两者共用同一状态机。组件不隐藏 ISR、GPIO 配置、接收器供电、
任务、heap、阻塞等待或事件队列。

默认 `NX_INFRARED_SOURCE_STRICT` 拒绝所有 COALESCED 事件。真实 F407 EXTI
provider 记录 IRQ 时间和采样 GPIO 电平，每条事件都标为 COALESCED，也可能丢失
中间边沿，因此无法供严格模式完成 NEC 帧。另行显式选择的
`NX_INFRARED_SOURCE_IRQ_OBSERVATION` 只建立软件观测参考；所有成功结果继续标为
`NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN`，保留原始 `source_flags` 的按位或。
即使某次输入无标志，该策略也不能升级来源质量；重复帧继承前一命令及本帧的来源
标志。NEC 反码校验不能证明没有丢边沿或建立实测时序。严格模式的无标志输入同样
只是来源合同声明，不是硬件证明。
当前独立检查的 IRQ 观测参考仅为 F407 V4.1 PE2；通用组件可配置线路，不表示支持
另一块 Board、provider、引脚、时间戳来源或遥控器。

V4.1 图纸将 PE2 标为 `HS0038`，不能据此确定已装接收器厂家、载波响应或遥控器
协议。NEC 是显式选定的软件参考，实际遥控器仍未知。组件不建立硬件资格，也不包含
时序校准、产品键值映射、RC5、NEC2 重复语义或协议自动识别。

调用者提供稳定只读配置，六组包含端点的微秒窗口分别描述引导脉冲、完整帧/重复帧
间隔、bit 脉冲及零/一间隔。相互竞争的两对间隔窗口不能重叠；每组为非零且不超过
20000 微秒。完整帧预算必须覆盖最坏 32 位及尾脉冲，不超过一秒；重复帧有效期显式
声明且不超过五秒。这些是配置边界，不是测得的接收器误差或时序保证。

标准 NEC 检查 8 位地址及反码；另行显式选择的扩展地址模式将这两个字节解释为
低字节在前的 16 位地址。两种模式均要求命令/反码、低位在前的完整 32 位及尾脉冲。
反码错误不会触发模式回退。完整帧始终作为新帧发布，不自动标记为 NEC2 重复。

接口按值返回状态及完整帧，成功之前不发布部分命令。重复帧须关联有效期内已经验证
的命令，成功重复帧延长其有效期。畸形波形、LOSS 及读取失败/数量矛盾会清除当前帧
与重复资格。严格模式还拒绝 COALESCED，IRQ 参考模式保留不确定质量，不清除标志。
超时部分帧可以在真实下降沿重新同步，但不能发布旧数据。
后续没有边沿时，解码器不保留任何外部资源。

接受的边沿或显式允许的 IRQ 观测推进严格单调时间历史；LOSS 检测/报告时间不是
缺失边沿。相同或回退时间戳锁存时钟故障，直到显式软件 stop/init。更换时间 epoch 前须由
调用者排空并重新绑定来源；配置声明不证明时钟身份或精度。

同一任务串行使用；回调重入 init/feed/poll/stop 返回 BUSY。已知跨度与控制对象别名
在调用前拒绝；不猜测 opaque context 的完整范围。返回后不保留参数指针。软件 stop
仅清除解码历史，不停止 EXTI，不证明 IRQ 静止；来源排空和会合由外部 owner 负责。
参考文档身份见同目录 JSON，原 PDF 不再分发。主机模型及 ARM 编译不建立实物资格。
独立 F407 模型执行真实 PE2 provider 的初始化、中断派发、环形缓冲丢失报告与公共
读取，不抹除 COALESCED；覆盖严格拒绝、显式采样解码、重复质量继承、真实缓冲溢出
和时钟回退，不建立实测 IRQ 延迟界限。
