---
id: reference.contracts.io.usb-host
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.io.usb-host)=
# USB Host、HS DMA 与角色切换

共享准入与停止见[USB生命周期](usb-lifecycle.md)。HS Device与Host共享控制器的AHB DMA限制在本页一次维护；上层协议栈接线见[中间件](usb-middleware.md)。

## HS AHB DMA、Host 与 managed role

HS Device DMA 使用每方向静态 SRAM staging 和独立 24 B SETUP staging；CPU 复制
公共 payload，不要求它额外声明 DMA word padding。AHB master 不占 DMA2 stream，
CCM 不可选。完成还必须证明 endpoint disabled、AHB idle、DMA 地址撤回读回和 IRQ
会合；失败保持 QUARANTINED，不能只看 XFRC 或 timeout 释放调用者存储。

Host 公共操作使用装配局部的 `nx_usb_host_pipe_id_t`，与 endpoint 地址和物理 HCD
channel 独立。默认逻辑 pipe 与所选 channel 同序，不生成映射数组；可选
`pipe_channels` 是长度为 `channel_count`、元素为 0 至 `channel_count-1` 的排列。
固定 Host 和 managed role 的 Host 共用该配置。映射为只读数据，必须存活到完整冷态
停止后；改变映射前须退出全部 packet、IRQ 和角色使用者。

F4 在 open、close、submit、cancel 边界执行一次逻辑到物理转换；HAL callback、
硬件排空和保存的 request 始终使用物理 slot，不进行反向搜索。每个所选 Host 的只读
配置增加一个指针；只有显式映射生成每 pipe 一字节的只读数组，没有新增可变队列、
反向表或全局设备注册。准入拒绝 request／payload 与活跃配置或映射的已知重叠。

Host 每个所选 pipe 同时借用一个 packet。打开 pipe 后，调用者按 payload 长度查询
`nx_usb_host_storage_requirements`，取得包含完整访问范围的 capacity 和地址 alignment；
零长 OUT 也查询真实最小范围。查询不访问外设 MMIO，不准入或借用存储，也不授予
DMA 可达性。缺少可选查询返回 UNSUPPORTED，调用者不得猜测默认几何。

F4 provider 的 PIO 地址 alignment 为 1，OUT 范围按四字节取整，IN 覆盖四字节取整的
MPS；HS DMA 的地址 alignment 为 4，零长 OUT 仍需四字节可读范围。实际 DMA 地址还
必须通过原有内存域检查。查询与 submit 共享同一范围计算；查询输出不得覆盖活跃
配置、映射、request、payload 或 managed role 保留的非活动 Device 元数据。
普通公共 prepare 合同不包含这些 F4 编码规则。

官方 Host bridge 在每次 packet 的复制／补零／准入前消费查询结果，验证既有 staging
的容量和对齐；OUT 只补零声明的额外访问范围。调用者继续提供精确存储预算，桥接
不自动分配或扩容，也不强制全部后端使用 1024 B 最小存储或四字节对齐。此查询增加
一个可选只读 ops 项及所选代码，没有新增 provider 可变字段、队列或缓冲区。

NAK／NYET 返回 NACK，由协议 owner 明确重试。锁定 HCD 的回调只锁存事实；
取消／超时保守排空整个共享 FIFO controller。旧物理 slot、DMA 地址、FIFO 和 IRQ
全部退出才 SETTLED。Host 没有自动 hub split 策略。

Host control pipe 的 MPS 按实际速度准入：low-speed 为 8，full-speed 为
8／16／32／64，high-speed 为 64；非法组合在 HAL 和 channel 副作用前拒绝。
reset／resume 在任何 HPRT 写入前分别要求严格多于 10 ms／20 ms 加 1 µs 的剩余
deadline；额外一 tick 覆盖整数微秒采样隐藏的相位。不足、已到期或恰好耗尽预算
时返回 TIMEOUT，不发出短复位／恢复脉冲。

已经开始的 reset／resume 必须保持最低信号时间。抢占越过 deadline 时，先完成
最低保持与撤信号读回，再返回 TIMEOUT；实际已恢复时仍发布 resumed 状态。
撤信号失败进入 closing，保留原 controller／clock 责任，stop 首先重试信号撤回，
成功前不调用 HAL stop 或释放 gate。已开始信号的时钟倒退、无效或持续无进展，
以有限观察预算进入 fault observer 和不返回的 park，保留信号而不伪造最低保持。
不增加实例字段、后台任务或另一套时钟；这些软件检查不构成物理脉宽测量。

`Nexus::USBRole` 是通用供电和排空屏障，F407 managed OTG 将它接入真实 cold
Device／Host。一个 composite 独占 core、PHY、pins、clock 和 IRQ；OFF prepare
不启动 USB。显式 generation-bound 请求先真实 drain，再 source-off 读回和独立
VBUS 放电确认，才进行新角色的供电／cold prepare。切回 Device 等待至少 50 ms
并确认实际 CMOD；inactive face 纯拒绝，旧 SETUP epoch 不跨角色复用。过流通过
显式 service 进入同一撤回屏障；硬件限流与实际放电路径由外部电源 owner 提供。
详见 `tests/contracts/usb_otg_assembly/README.md` 的软件信号合同。
