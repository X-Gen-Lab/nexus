---
id: design.f407_storage
type: reference
audience: provider
source_language: zh_CN
---

# F407 物理存储与制造维护

本页规定 F407VE/ZG 的普通运行和显式制造维护边界。实例、只读 face 与共享方法表的
关系见 [factory 图](../explanation/factory.md)；协议和存储所有权仍由真实 C provider
承担。主机模型、ARM 链接和物理 HIL 是独立证据，HIL 为 `not_executed`。

## 主阵列和 OTP

VE 物理主阵列为 512 KiB、8 个 sector，ZG 为 1 MiB、12 个 sector。前四块各为
16 KiB，第五块 64 KiB，余下各为 128 KiB。普通 `flash` face 暴露完整几何，以
2.7–3.6 V 假设执行 x32 编程；擦除必须精确对齐实际 sector，不隐含产品保留区、
认证、升级、回滚或恢复策略。F407 是单 bank，Flash 取指和中断可能在物理脉冲期间
停顿，有限 deadline 不建立中断响应或调用返回的时间上界。

显式 `[flash.<id>] maintenance=true` 才生成可选维护 face；它与普通 face 使用同一
controller ID 和物理 owner。查询不执行任何维护动作，也不构成制造授权。测试授权
只准许软件模型执行，不能作为真实设备上不可逆动作的许可。

OTP 数据区为 `0x1fff7800..0x1fff79ff`，16 块各 32 字节；紧接着的 16 个独立
锁字节只允许 `0xff` 或 `0x00`。数据编程只能清零位，完整范围、锁状态和所有
zero-to-one 错误在第一脉冲之前检查。锁定单块用 x8 写入精确锁字节，不能用 x32
写入连带锁定相邻块。OTP 无擦除，主阵列 mass erase 不清除 OTP。

## Option Bytes 和保护

完整目标 image 包括语义 RDP 级别、按物理密度限定的写保护 sector 集合，以及
BOR/USER 字段。`brownout` 使用固定宽度语义 profile：DISABLED、SUPPLY_2V1、
SUPPLY_2V4 或 SUPPLY_2V7，provider 独立映射到寄存器编码。零值 UNKNOWN 表示
未选择，在任何硬件效果之前拒绝；关闭 BOR 必须显式选择 DISABLED。所选官方 HAL 将三个
供电 profile 分别关联到 2.10–2.40 V、2.40–2.70 V 和 2.70–3.60 V 工作范围；
这些名称不表示精确复位阈值或实测电压，具体器件限值与容差仍须遵守。USER 明确选择
硬件 watchdog、STOP reset 和 STANDBY reset，不推导产品策略。F407 不提供 PCROP
或双 bank 选项；不能把其它 F4 型号的功能套用到这两个型号。

每次操作单独声明已确认的效果：OTP_PROGRAM、OTP_LOCK、OPTIONS、MASS_ERASE、
PERMANENT。缺失对应声明时，在解锁或第一脉冲之前拒绝。这些位只是调用者显式
承担效果的合同，不是认证方案。读取与修改必须由同一特权、未屏蔽中断的 Thread
执行者串行推进，不能与普通 Flash、调试器或另一 HAL owner 并行操作。

RDP1 退回 RDP0 会擦除整个用户 Flash 和 backup SRAM。外部制造映像必须把完整
调用闭包、向量、常量、栈、状态以及返回目的地放在能独立存活的 RAM；本平台不把
普通运行映像悄悄重定位到 RAM，也不承诺擦除自身后仍可返回。RDP2 永久关闭调试、
RAM/system boot 及后续 option 修改，没有降级路径；该动作要求 PERMANENT 声明。

物理脉冲启动后无法取消。超时可能已经产生永久效果，不回放、不回滚，也不自动
重启。软件完成后通过 snapshot 报告 `power_on_reset_required`；调用者负责真正
的 POR。该标记只描述当前软件会话内的写入，重新启动后的清零不是已完成 POR 的
硬件证据。特别是保护变更不能用普通软件复位来替代所需的 POR。

首次错误和实际排空分别处理。必须观察脉冲结束、撤编程模式、清除完成源、排空
禁用的 Flash IRQ 并验证重新锁定；拒绝锁定或 IRQ 排空时保留同一 active owner，
普通 Flash 访问和 Platform 停止均不能回收它。没有隐藏后台任务或隐式恢复策略。

失败后的 owner 可显式调用普通 Flash face 的 `nx_flash_port_recover()`；Platform
close 也执行同一有限清理重试。BSY 未退出、外来 enabled/active vector 或外来模式
仍返回 BUSY；只有实际模式撤回、状态清除、IRQ 排空与重新锁定全部读回成立后才
释放责任。仅本 owner 尝试解锁的 Option Bytes 才重试 OPTLOCK，不改写外来 options，
不重新发出编程脉冲、不回滚或掩盖原失败，也不替代所需 POR。

ES0182 Rev19 §2.2.9 限制无复位的连续 BOR 编程。本实现不保留额外启动历史，
因此修改 BOR 必须使用实际 AHB 分频 1 或 2；分频大于 2 在任何解锁之前拒绝。
BOR 不变的其他 options 维护不受此限制，平台不会自动修改系统时钟。

## 外部总线与介质

现有 FSMC 的 NE3 SRAM、CPU 窗口、启动内存与 NE4 parallel16 合同见
[模块 README](../../soc/stm32/stm32f4/private/fsmc_hal/README.md)。启动内存的永久
收条早于 Platform 运行，失败或停止均不能撤回承载全局对象的 bank、引脚和时钟。
CPU 窗口本身不授予 DMA、堆、外部栈或执行权限。NOR/NAND 的芯片命令、页操作和
板级时序必须由完整选中模式维护，不能由 async16 名称推导。

SDIO 的有限 IRQ/DMA、CRC、取消、排空及拔卡责任见
[SDIO 模块合同](../../soc/stm32/stm32f4/private/sdio_hal/README.md)。所有介质发现均为
显式运行期动作；冷态启动和 factory 查询不替换卡片或运行产品文件系统。

RX 使用 PFCTRL，NDTR 在启用后从 `0xffff` 计数；TX 使用 DMA-controller flow 与
精确 word count，不依赖 LAST 限制源预取，也不要求未声明 padding。两方向的 DMA
TC 都可能由 software disable 产生，不表示数据、CRC 或卡编程成功。RX 在 EN=0
后仍可能有 FIFO 到 RAM 的写入，
须等真实 TC、FIFO_EMPTY 与全部 IRQ／地址退出后才结清借用。TX 的 RAM 引用退出
后，FIFO 内部副本仍可能使冷态 owner、后续 arm 和 close 保持 BUSY；平台不自动
清副本、重发或复位共享 DMA2。上层系统恢复与普通 stop 是不同责任。

memory 单块使用 CMD17／24，2..max_blocks（上限 32）使用一次 CMD18／25 和
`N * 512` DLEN，保持 512 字节 packet；多块以 CMD12 和 CMD13 READY 收口，写入
还须实际编程 ready。命令可能生效后丢响应保留 repair 收条，不重放 payload。
CMD53 仍按 function unit 推进，不发 memory-only CMD12／13；完整 burst、16 字节
对齐和短 CMD53 DMA 拒绝保持不变。

所选静态 ClockPlan 要求精确 PLL48=48 MHz，并联合满足 RM0090 的 PCLK2／
SDIO_CK 下界与 ES0182 的按宽度计算的严格 FIFO 约束。最小合法偶数分频使数据
不超过 12 MHz、识别／low-speed 严格低于 400 kHz；低 PCLK2 选择更慢时钟，非法
组合直接拒绝。只在无 host／DMA 借用时变更 CLKCR，控制间隔使用所选真实域的
生成常量与微秒量化余量；不提供运行期 DVFS 或高速协议切换。

`protocol="mmc"` 可显式选择 1／4／8 位 legacy SDR，数据时钟最多 12 MHz。八位模式必须由
Board 授权完整十个 pad，按所选路线生成六或十份快照；SD memory 与 SDIO I/O
functions 仍只接纳 1／4 位。MMC 在卡片 SWITCH 与 ready 检查成功后才提交主机宽度，
失败不隐式降宽。精确命令、错误位与引脚合同沿用上述 SDIO 模块说明；
`tests/contracts/mmc8_assembly/` 是软件参考，不声明现有开发板 SD socket 已接八线。
