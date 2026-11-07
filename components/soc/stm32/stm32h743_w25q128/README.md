# H743 与 W25Q128JV 单线映射存储参考

可选软件参考将既有 mapped-storage owner 的调用者 slots、request、clock 与
backend state 接到 H743 QUADSPI。器件身份是 Winbond W25Q128JV Rev M、
2024-12-24 的 EF4018 Q／N／JQ；EF7018 M／JM 需要独立 DTR profile。本组件不
建立 Board 接线或物理资格。

## 架构与固定协议

首个 profile 是 mode 0、单线 `03h` read、`02h` page program、`20h` 4 KiB sector
erase，最高 16 MHz。program 按 256 字节 page 边界拆分，要求目标已擦除并回读
验证；erase 完成也验证整个 sector。每次 owner service 推进一个命令或有限 FIFO
步骤，verification 使用 16 字节私有 buffer。

本组件属于 H743 Platform，不是 Runtime-only HAL 包。消费者使用
`nexus_add_firmware` 或等价提供真实 SoC startup／clock owner。私有 supplier
prelude 读取实际 System `core_hz`，满足官方 MDMA clock 依赖，不创建第二个全局
clock 或猜测 peripheral frequency。vendor 与 construction headers 保持私有。

从 `CMakeLists.txt`、`src/` 与 `private/` 进入；通用 owner 合同见
[映射存储组件](../../../mapped_storage/README.md)，本页只规定精确硬件 adapter。

[FIG-23 所有权图](../../../../docs/assets/diagrams/io-extension-ownership.svg) 的映射
分支表示通用读借用与撤回责任；本页精确 MPU／Flash 准入仍须分别满足。

## 冷态准入与 MPU

SoC cold owner 在 prepare 前取得并保留真实 QUADSPI clock 和精确 pin leases。
CPU 两种 caches 均关闭，QUADSPI NVIC disabled／inactive，排除全部外部 CPU／DMA／
XIP users。startup 已开启含 HardFault／NMI coverage 的 MPU；cold owner 提供两份
独占 MPU region claims，可读区域优先级更高，两区域初始均 disabled。

adapter 在完整 QUADSPI alias window 建立 256 MiB no-access／XN guard，只在映射
期间对已验证 16 MiB CPU aperture 授予 read-only／XN／noncacheable。它拒绝外来
更高优先级 aperture override，不改 global MPU control。MPU 不负责 join DMA、
debugger 或其他 masters。kernel frequency 和 clock domain 全程稳定；假设 Flash
power 已稳定后，prepare 仍至少等待 5 ms 才 discovery。没有默认 pinout、外部供电
控制或隐式 recovery reset。

## 借用、撤回与恢复

只发布 CPU read loans。最后一次 release 必须撤销 mapped aperture，失败保留
loan。cancel 阻止未来变更命令，对已发出的 page／sector 操作完成或保留责任。
deadline、HAL callback 或 HAL READY 都不足以证明 drain；不确定 controller、
Flash BUSY／WEL、IRQ 或 buffer 引用都保留借用。

最终 release 先删除可读 MPU permission、执行 CPU fence，再 controller abort。
失败后旧 view 永远不可再访问，消费者保持 joined，cleanup 重试期间不开放新准入。
map 失败必须证明 deny 和 rollback，或进入 retained platform fault。stop 在
controller 撤回后恢复 cold descriptors 与 selector，恢复拒绝继续持有责任。

generic owner 先 stop，全部 consumers／controls join 后，私有 controller stop
才释放状态。clock／pin 归还仍由显式 SoC cold owner 负责。

## 官方 HAL 复用与已审例外

adapter 复用完整官方 HAL command、transmit／receive interrupt、IRQ dispatch、
mapping 和 abort 函数。串行 task 轮询 HAL peripheral interrupt facts，不启 NVIC、
worker 或 MDMA。MDMA 源仅因官方 QSPI IRQ 引用其 abort entry 而链接，不授予 DMA 能力。

需要有界 Nexus C 的已审 SDK 缺口为：

- HAL init 在 BUSY wait 前把 reset handle timeout 换为 5000 ms；cold adapter
  改为验证 reset／idle 硬件，使用官方 CMSIS masks，仅写所选 CR／DCR 并 readback。
- HAL command／mapping 虽名含 IT，内部仍等待。私有 handle 使用 timeout zero
  和独立 idle admission；HAL abort／READY／deinit 不证明全部撤源，adapter 分别
  检查 BUSY／ABORT／FIFO、mode／source withdrawal、pending IRQ 和 HAL retained pointers。
- 官方 command config 不在不可逆命令前验证地址。adapter 先 disarm 旧 CCR，stage
  并回读 address，再调用完整 HAL 函数并检查结果。普通 RX callback 仍可能计数不全，
  同时 error 也可能在 HAL dispatch 中低于 completion；解析响应前先看精确 remaining
  与 error flags。

不可变 datasheet 和官方 HAL audit 见交付证据。Cache、任意 MPU policy、quad／QE
变化、mapped DMA、XIP、erase suspend、status／protection 写入和掉电持久性不属于
首个 profile。模型和 ARM 链接不建立电气或实板资格。
