# 完整记录的 SEGGER RTT 服务

`nexus_add_rtt()` 才核验并选择锁定的 SEGGER RTT，提供 `Nexus::RTT`；未选择的库存
不初始化、不编译，也不进入 SDK payload。Nexus 保持上游引擎和协议。私有 profile
关闭 assembly 替换与上游 lock 宏，所有 target 调用经过同一个非阻塞 atomic try-guard，
保留进入时的 mask 状态，不获取 OS 锁或等待 debugger。支持同一 CPU／安全域的特权
任务调用，排除 IRQ、NMI/HardFault 和绕过此适配的并发 vendor API 调用。

调用方独占分配通道 1～3，提供不可变名称和 up/down ring；通道 0 保留为上游编译期
通道，当前 profile 的默认 ring 各 16 字节。容量包括上游预留的一个字节。通道、缓冲
及其生存期属于镜像预算，不创建后台任务或动态对象。

现有 `nx_log_sink_port_t` 调用真实 `WriteSkipNoLock`：完整复制记录，或不改变旧数据
并返回 OVERFLOW，不输出截断前缀。accepted 和 skipped 记录计数饱和。SUCCESS 表示
复制到 RTT 内存，不表示 probe 收到或已持久化；主机未连接时，ring 最终满并有限拒绝。
读取只复制当前可用 downlink 一次，不轮询等待。

Flush／stop 不等待。Stop 关闭 target admission，uplink 仍有数据时返回 BUSY。
即使本地关闭完成，上游控制块仍引用 ring 和名称，应保持整个镜像／debugger 生存期。
空 ring 不建立 host join 或在途 debugger 内存访问撤回证明。RTT 是普通诊断传输，
不能取代原始故障记录。
