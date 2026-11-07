# H747 外部双核启动 AMP 适配

`Nexus::H747AMP` 将既有 `Nexus::AMPIPC` 接到精确 STM32H747 M7／M4 的 HSEM
及调用者预留 SRAM4。首个软件参考依赖外部已建立的 baremetal paired boot，不配置
供电／system clocks，不启动另一核、不安装 scheduler，也不提供生产 SoC startup。

两个独立链接夹具 `tests/contracts/h747_amp_pair` 使用真实组件、官方 device
startup 和各自 `CORE_CM7`／`CORE_CM4`；空 external-boot context 不是实板启动
配方。未知 PCB revision、option bytes 与物理执行保持未认证。此组件存在不代表
已增加完整 H747 SoC／Board 接入。

[FIG-23 所有权图](../../../../docs/assets/diagrams/io-extension-ownership.svg) 的 AMP
分支表示通用消息借用与 join 责任，不表示 H747 paired boot 已经完成物理验证。

## 每核状态与共享内存

每镜像拥有独立 local C runtime、stack、endpoint slots 和 port state。pair owner
预留双方共用的 power-of-two SRAM4 subwindow，置于两个镜像 startup clearing 之外。
RM0399 Table6 的 SRAM4 为 `0x38000000..0x3800FFFF`，默认 cacheable。因此首模式
要求 enabled MPU region 精确覆盖 reservation：full RW、XN、shareable Normal
noncacheable，没有 disabled subregions，也没有重叠的更高优先级 enabled region。

外部 boot 在任何 endpoint 访问前建立属性，直到两端与 observers 全部 join 前禁止
其他 aliases 或动态 MPU／Cache 变更。CM7 attach 拒绝已启 D-cache，直到 final join
保持关闭；新的 MPU 属性不能证明旧 dirty lines 已清，attach 不清理未知旧 cache。
cached payload 和动态 Cache transition 是未来独立模式。

## Epoch 准入与 HSEM 所有权

调用者提供绑定精确 lane／role 的持久 epoch-admission authority，跨单核及整片
reset 拒绝复用／倒退 epoch。清零 RAM counter、random timeout 或 local static
都不能提供该 authority。回调在本核 IRQ exclusion 下有界且不可重入；持久化／
provisioning 由外部 boot owner 预先完成，不能在回调里阻塞写 Flash。缺失 authority
会在 semaphore claim 前拒绝。没有 registry、allocator 或默认 pool。

lane role 持有唯一 pair-assigned HSEM 到 final join，另一 HSEM 只做可选 outgoing
doorbell pulse。官方 LL 使用真实编码：M7 CPU1、M4 CPU2，不是 ordinal 0／1。
每次 try 有界，本核 IRQ exclusion 防止同核重入 claim。doorbell release 失败
保留 receipt 供重试，不撤销 payload loan。最终 unclaim 不能证明释放时，保持 mask
并进入既有 fault observer／park，不伪造存储可回收。peer 管 notification enable
和 IRQ routing，共享 publication predicate 始终权威，不隐装 IRQ handler 或 worker。

## 构造、发布与退出

`nx_h747_amp_construct` 在公共 constructor 读取 control word 前，验证双方 boot
words、完整 payload 和精确 wire capacity 都在已准入 window 内。local endpoint／
slot state 位于 window 外。输入 common config 的 methods／context 必须为 NULL，
cold adapter 验证后才绑定 private table；复制此 table 不能授权绕过准入的新 raw
common construction。

wire records 使用 offsets／lengths，不传跨核 pointer。自然对齐 32 位 control
访问和架构 barriers 排序 publication。common mechanism 保留 sender payload 到
精确 epoch／sequence ACK；receiver 存储保留到调用者 join readers 并 finish。

reset join 返回 `UNSUPPORTED`：换 epoch、HSEM release、reset request 或 heartbeat
timeout 都不能证明旧 CPU、DMA、IRQ 和 observers 停止访问共享 memory。endpoint／
port close 后，system clock、HSEM gate 和 SRAM 所有权仍属于 external boot owner。

## 源码与验证范围

公共头是 `include/nexus/components/stm32h747_amp.h`，`src/` 和
`private/hardware.h` 持有精确桥接。通用借用说明见
[AMP IPC](../../../amp_ipc/README.md)。

Host GoogleTest／GoogleMock 以硬件边界模型执行真实 bridge 和 common message
借用；两个真实独立 ARM ELF 链接建立软件闭包、ABI、memory reservations。它们都
不证明物理 HSEM／Cache／boot／reset、latency、throughput 或 Board 支持。
