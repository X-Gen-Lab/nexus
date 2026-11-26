# F407 HS Device 软件参考

这些 C11 装配消费公共 Device、endpoint、profile、activation 和 setup-decision
face。OTG_HS 内置 PHY 仍以全速运行，使用 PB14/PB15 AF12；外部 ULPI 使用十二根
AF10 pad 和明确交接的 60 MHz 时钟。ULPI 高速及全速回退各有选中的 ROM profile，
factory 按实际枚举速度返回相应描述，不扩大回退时的 MPS。

PIO 和 AHB DMA 共用现有控制阶段、epoch、取消和完整排空机制。AHB DMA 只在 HS
core 选入，使用所选 SRAM 域中的每方向独立 staging 及 24 字节 SETUP staging；
它不占用 DMA2 stream。公共 packet payload 由 CPU 复制，保持已有精确借用范围。
硬件 endpoint、AHB master、DMA 地址、FIFO 和 IRQ 引用退出前，不发布 SETTLED。
DMA FIFO 预算另留 32 words，未使用的低编号 IN FIFO 留至少 16 words。

内置 PHY 需要来自 HSE 的精确 48 MHz PLL48 和大于 30 MHz 的 AHB；外部 ULPI
需要大于 30 MHz 的 AHB 和交接的 60 MHz PHY 时钟，不把 HSI 的 nominal 48 MHz
当作物理 USB 精度。初始化与撤回保留独立的 HS/ULPI gate receipt。挂起、reset、
EP0 地址和 ZLP 仍由同一 controller owner 处理。

TOML 和 Board facts 是软件 pad 合同，不证明实际 PHY、供电、VBUS、连接器或 PCB
布线。这些夹具只编译和链接；软件 model 与 ARM 预算不建立高速电气或真实枚举资格，
物理 HIL 为 `not_executed`。
