# SFUD 明确选择的 W25Q128JV profile

`nexus_add_sfud()` 选择锁定的真实 SFUD `sfud/src/sfud.c`。公共 `nx_sfud_t`、
workspace 与 `nx_flash_port_t` 使用 Nexus 类型；private vendor 头不传播给普通业务。
当前仅接纳产品明确选择的 **W25Q128JV**，16 MiB、EF4018、单线 mode 0/3、最多
50 MHz。JEDEC 只作必要核对，不证明订货后缀、焊接或未知 PCB 路线。

上游 `sfud_device_init()` 会 reset，且对特定非本 profile 器件可能清保护。本 adapter
不调用该自动发现/初始化路径：只读核对 identity、idle、SUS、BP/CMP/WPS，填充精确
已选 metadata，再执行真实上游 read/page-program/sector-erase 算法。没有 reset、
status/protection 写、SFDP 自动发现、quad、四字节地址或型号回退。

## 所有权与预算

既有 SPI endpoint 继续持有 controller/pin 所有权，同芯片不能同时运行 W25 与 SFUD
两个协议 owner。上游 page-program 使用一个 static command buffer，因此**本 link
profile 的所有 SFUD 实例调用也必须由产品在同一执行者串行**；不同 SPI bus 不解除
这项上游约束。没有隐藏全局锁、工作线程或 runtime current-device 注册表。

所有传输是完整 CS，短 transfer 作为失败；read/program 分为最多 252-byte 数据块，
不偷偷发超过 endpoint 256-byte 上限的事务。Full physical Flash face 暴露完整
16 MiB 几何，产品通过原 `nx_flash_region_t` 选择授权范围，不在驱动内预留分区。

program/erase 使用传入的完整操作绝对 deadline；每个 vendor exchange 检查原预算。
02h/20h 发出后，私有桥先按同一 deadline 结清 WIP，再返回上游；program 状态探测
至少间隔 100 µs，sector erase 至少间隔 1 ms，每个完整 CS 事务结束后才等待。
等待为 CPU-active clock 检查，不增加 OS、worker 或锁。profile 接纳已验收的
1 ms 或更细 clock 量化；连续 1048576 次采样无可观察进展才返回 STATE 并保留
uncertain owner。此有限采样保护不是精确停钟诊断；所选 clock 必须在该采样上限内
产生可观察进展。回归包含每 100 次读取才推进 1 µs、每 1 ms 才发布新时间的 clock，
不把相邻相同读数当作停钟，也不让真正固定的读数造成无限等待。

上游固定 32 次 retry 保留作异常路径保护，不能作为真实擦写时间预算；其
`retry.delay` 是无 context 的 `void(void)` callback，不能直接承接每实例绝对
deadline，因此本桥不使用它或增加隐式全局当前设备。
[官方 RevM 手册](https://www.winbond.com/resource-files/W25Q128JV%20RevM%2012242024%20Plus.pdf)
第 66 页给出 page program 典型 0.4 ms／最大 3 ms，4 KiB erase 典型 45 ms／最大
400 ms；产品须为完整数据库事务提供相应总预算，不能逐 pulse 续期。

read face 原 API 没有 deadline 参数，因此产品
在整次 FAL/数据库/文件操作前调用 `nx_sfud_set_read_deadline()`，使用与消费者相同的
有限绝对 deadline；不能按每次 read 续期。clock/provider epoch 的有效性由产品建立。

写命令或 WREN 后的错误保留 uncertain owner，禁止新工作和 stop。`nx_sfud_service()`
使用独立 cleanup deadline，至多四次状态/disarm 交换证明 WIP0/SUS0/WEL0；不 reset、
resume、回放或升级原写结果。Flash face recover 只观察既有清理状态，不暗自发 SPI 或
发明新的 deadline。SPI controller 自身的保留故障仍按其原 owner 合同处理。

## 资源与证据边界

vendor metadata 存在 caller workspace，大小/对齐由查询函数报告，无 heap。上游
page-program static buffer 为 261 bytes；SPI framing 局部 TX/RX 各 256 bytes，另加
真实调用栈与上游算法状态。源码尺寸不是实测栈峰值。构建、软件 W25 模型和实际 PCB/
Flash 的时序、掉电、耐久各自验证，physical HIL 未执行。
