# 显式 NEC 红外解码参考

`Nexus::Infrared` 是可选、无动态分配 C11 解码器，接收调用者拥有的低有效、已解调、
双边沿 `nx_exti_event_t` 副本。确定性 `feed` 可接等价事件源，`poll` 每次最多读取
一个公共 EXTI 事件，两者执行同一状态机。组件不隐藏 ISR、GPIO 配置、receiver
供电、task、heap、阻塞等待或事件队列。

## 来源质量与精确范围

默认 `NX_INFRARED_SOURCE_STRICT` 拒绝每个 COALESCED edge。实际 F407 EXTI 记录
IRQ 时间与采样 GPIO 电平，每条事件均标 COALESCED，也可能丢中间边沿，因此不能
供严格模式完成 NEC 帧。单独显式选择 `NX_INFRARED_SOURCE_IRQ_OBSERVATION` 才
接纳该观测质量，所有成功结果保持 `NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN`，并
保留原 `source_flags` 的按位或。该策略即使遇到无标志输入也不升级质量，repeat
继承前一命令与自身标志。NEC 反码不证明捕获完整或物理时序，严格无标志输入同样
只是来源合同声明，不是硬件证明。

独立检查的 IRQ 观测参考仅为 F407 V4.1 PE2；通用可配置 line 不授权另一 Board、
provider、pin、timestamp source 或 remote。V4.1 图纸将 PE2 标为 `HS0038`，不证明
实际 receiver 厂家、载波响应或遥控器协议。NEC 是显式软件参考，真实 remote 未知；
不提供校准、产品 key 映射、RC5、NEC2 repeat 语义或协议自动识别。

## 配置与解码

调用者提供稳定只读 `nx_infrared_nec_config_t`。六组含端点 µs window 表达 leader
mark、完整／repeat space、bit mark 与 zero／one space。竞争的两对 space 不重叠，
每组非零且最多 20000 µs。完整 frame budget 须覆盖最坏允许的 32 位及 trailer，
最多 1 秒。repeat age 显式且最多 5 秒；这些是配置边界，不是 receiver 容差或实测。

标准 NEC 验证 8 位地址及反码；独立选择 extended address 后，两字节解释为低字节
在前的 16 位地址，不要求该反码。两种模式都要求 command／inverse、全部 32 个
LSB-first bit 与 final mark。inverse 错误不触发地址模式回退；普通有效完整包始终
发布为新帧，不自动改名为 NEC2 repeat。

`feed`／`poll` 按值返回 status，仅成功时携带完整 frame，不泄漏 partial frame。
repeat 需要 age 内已验证命令，成功 repeat 延长其 age。畸形波形、LOSS、失败或
矛盾 port read 清除当前 frame 和 repeat 资格。STRICT 另拒绝 coalescing，IRQ 参考
保留不确定性。超时 partial frame 可在真实下降沿重新同步，不能发布旧 bits。
没有新 edge 时，空闲 decoder 不保留外部资源。

接受的 edge 或明确允许的 IRQ observation 推进严格单调 timestamp history。LOSS
检测／报告时间不能重建漏边沿。相等或回退的已接受时间锁存 clock fault，直到显式
软件 stop／init；更换 clock epoch 前，调用者先排空并重新绑定 source。配置不能
证明 clock 身份或精度。

## 所有权、退出与证据

一个串行 task 拥有 decoder；callback 重入 init／feed／poll／stop 返回 BUSY。输入
span 必须自然对齐、有限且不与 decoder／config 重叠；已知 face、ops、opaque
context 首地址别名在 callback 前拒绝。完整 opaque context 有效性和范围仍由调用者
保证。返回后不保留输入／输出指针。stop 只清 decoder，不停 EXTI、不证明 IRQ
静止；外部 owner 排空并会合 source。

[来源索引](references.json)绑定真实厂商文档字节与审核章节，不再分发 PDF。
GoogleTest 执行真实 decoder，GoogleMock 只替换外部 EXTI 读取。独立 F407 模型
执行真实 PE2 初始化、IRQ dispatch、ring loss 和公共 read，保留 COALESCED；覆盖
严格拒绝、显式采样、repeat 质量继承、真实 ring overflow 与 clock regression。
主机／ARM 证据不建立 receiver／remote 支持或实测 IRQ 延迟界限。
