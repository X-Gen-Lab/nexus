# F407 FSMC transport and ownership

This private profile selects the pinned, unmodified official STM32F4
`stm32f4xx_ll_fsmc.c` NOR/SRAM engine. The CMake helper applies this minimal HAL
configuration to the vendor translation unit only. It does not replace a UART
HAL configuration, call global HAL initialization, or introduce a HAL tick.
NAND ECC and PCCARD engines are omitted; their exported functions are unavailable
in this selection. NE3 and NE4 remain ordinary NOR/SRAM bank selects.

The real Nexus owner is `drivers/fsmc.c`. One bus owns one AHB3 gate receipt and
caller-sized bank state. The enclosing Platform alone owns ordinary GPIO
snapshots and gates. Exact selected AF12 unions are LCD 20 pads, SRAM 40 pads,
or both 41 pads; selecting an LCD does not acquire SRAM address lines or GPIOF.
Each selected bank has independent read/write access-mode-A cycle fields.
Reserved BWTR bits are preserved by the official LL and excluded from meaningful
configuration readback. Cleanup restores the captured complete register values.

The LCD bank is NE4: 16-bit command at `0x6c000000`, data at `0x6c002000` because
external A12 is CPU byte-address bit 13. The readonly public parallel face points
directly to the shared operations table and its exact caller-owned endpoint.
`lcd_prepare` acquires its bank; `lcd_close` releases after component users join.
No buffer survives a method return. PIO checks the finite clock deadline every
32 words and after the final word; the observation can lag by this bounded word
quantum. This is not a wall-clock bound when a physical bus access stalls.

The plain NE3 CPU-window profile grants an exact caller-owned, epoch-checked
1 MiB lease at `0x68000000`. CPU loads/stores use the address directly. Release
orders prior caller-joined CPU access with a compiler/hardware DSB before
publishing the release; stop has another DSB before clearing the first decoder
or gate. A barrier never proves DMA completion or revokes a pointer. Copies,
stale epochs and a different face cannot release the original lease.

Control, lease, snapshot and progress storage remain internal RAM. Known
physical Flash, MMIO, external-bank and system-control apertures are rejected
for those mutable objects. Transfer payload rules differ: readonly internal
Flash pixels are permitted; readback cannot write Flash. A payload in exact
NE3 SRAM is permitted only with the same owner's live runtime loan or permanent
startup backing and observed live bank/gate. Peripheral, LCD and other external
apertures are never ordinary payloads. These guards reject known hazards; they
do not prove that an arbitrary caller pointer is valid or establish an MPU
security boundary.

A separate selected `drivers/fsmc_boot.c` bridge calls the real reset-plan proof
before adopting startup SRAM. It references the exact same immutable NE3 bank
descriptor as the reset BootPlan. The boot-owned 40 pads have NULL runtime
snapshots, and only a selected NE4-only PG12 gets an ordinary snapshot. The
runtime never rewrites boot-owned NE3/pads. The selected bus uses the static
`NX_STM32_FSMC_BOOT_OWNER` initializer before main: the lifetime receipt already
exists before Platform startup or runtime adoption, and survives rejected proof
or any earlier startup failure. It makes stop return BUSY before any decoder/gate
withdrawal, even after all transient LCD
loans close or optional LCD setup fails. Reset is the supported whole-image
withdrawal. A generic whole-bank loan is UNSUPPORTED because it would overlap
startup DATA/BSS/NOINIT. Readonly geometry reports `startup_backing=true` without
granting access. BootPlan/startup/linker integration is explicit; a geometry
query or plain runtime loan never enables section placement.

Hardware and protocol facts are checked by Native models and ARM compilation.
They do not establish SRAM fitting/speed, panel electrical timings, visible
pixels, physical reset provenance, throughput, DMA support or board HIL.

## 中文合同

此私有配置只选入锁定且未修改的官方 STM32F4 `stm32f4xx_ll_fsmc.c` NOR/SRAM
引擎，最小 HAL 配置仅用于厂商源文件，不覆盖 UART 的 HAL 配置，不调用全局 HAL
初始化，也不引入 HAL tick。NAND ECC／PCCARD 引擎及其函数未选入；NE3／NE4
仍是 NOR/SRAM 的片选 bank。

实际 owner 为 `drivers/fsmc.c`，一个 bus 持有一份 AHB3 门控收条和精确数量的
bank 状态；普通 GPIO 原值和门控只由外层 Platform 保存。LCD／SRAM／两者的
AF12 引脚并集为 20／40／41；只选 LCD 不取得 SRAM 地址线或 GPIOF。各 bank
独立声明访问模式 A 的读写周期。官方 LL 保留 BWTR 的保留位，配置核验只比较
有定义的字段，清理时仍恢复捕获的完整原值。

NE4 的 16 位命令地址为 0x6c000000，数据地址为 0x6c002000，因为外部 A12
对应 CPU 字节地址 bit13。只读公共并行接口直接关联共享操作表和调用者端点；
lcd_prepare 取得 bank，组件用户会合后由 lcd_close 归还。每次返回前结束缓冲
访问。PIO 每 32 个半字及最终半字后检查有限绝对 deadline，观察最多滞后这一批次；
物理总线访问若停滞，该批次数不代表真实时间上界。

普通 NE3 CPU 窗口在 0x68000000 提供一 MiB 的原对象／epoch 借用；CPU 通过地址
直接访问。release 在发布归还之前执行含编译器屏障的 DSB，stop 在首次关闭译码器
或门控前也执行 DSB。屏障不能证明 DMA 已完成，也不能撤销派生指针。副本、旧代数
或其它接口不能释放原始借用。

owner、lease、快照和进度输出必须放在内部 RAM；这些可变对象拒绝已知物理 Flash、
MMIO、外部 bank 和系统控制空间。传输负载规则不同：允许从内部 Flash 读像素，
禁止回读写入 Flash；NE3 负载只有在同一 owner 持有活动运行期窗口或永久启动背书，
且 bank／门控仍有效时才允许。外设、LCD 和其它外部地址窗不能当作普通负载。
这些规则拒绝已知危险范围，不证明任意调用者指针有效，也不形成 MPU 安全边界。

另行选入的 `drivers/fsmc_boot.c` 必须先调用实际启动证明，再接管启动 SRAM；
它和复位 BootPlan 引用同一个只读 NE3 bank 描述符。40 个启动持有引脚使用 NULL
运行期快照，只有可选 LCD 专属 PG12 取得普通快照，运行期不重写启动 NE3 或引脚。所选 bus 必须在 main 之前使用静态初始化器
`NX_STM32_FSMC_BOOT_OWNER`，因此永久收条早于 Platform 启动或运行期接管，证明
拒绝或更早的启动失败也不能清除此既有责任。stop 在修改任何译码器／门控之前返回
BUSY；即使 LCD 借用全部结束，
或可选 LCD 初始化失败，也不能释放启动全局对象依赖。整个镜像通过复位退出。
整块 SRAM 的普通运行期借用返回 UNSUPPORTED，避免覆盖启动 DATA/BSS/NOINIT。
只读 geometry 以 startup_backing=true 描述事实，不授予使用权。启动计划、启动钩子
和链接脚本必须显式接入；查询几何或普通运行期借用不会启用段放置。

Native 模型和 ARM 编译只证明软件行为，不证明 SRAM 实装／速度、屏幕电气时序、
可见像素、实际复位来源、吞吐、DMA 支持或实板验收。
