# F407 FSMC 私有传输配置与所有权

本目录为 F407 的官方 LL NOR/SRAM 源文件提供独立编译配置，不是公共组件接口。
芯片级入口见 [F407 实现](../../README.md)，公共端点合同由 `io/` 维护。

## 私有库与共享资源

此私有配置选入锁定且未修改的官方 STM32F4 `stm32f4xx_ll_fsmc.c` NOR/SRAM
引擎；只有静态选择 `nand` 端点时才定义 `NEXUS_STM32_FSMC_NAND`，选入 NAND
初始化、common/attribute 时序及 ECC enable/disable。最小 HAL 配置仅用于厂商
源文件，不覆盖 UART 的 HAL 配置，不调用全局 HAL 初始化。带 tick 等待的
`FSMC_NAND_GetECC` 不被引用，function-section 链接排除该路径；ECC 原始码由同一
owner 有界观察 FEMPT/ECCR。最终 ARM ELF 不含 `HAL_GetTick`。PCCARD 仍未开放；
外来已运行 PCCARD/NAND 或 IRQ consumer 会阻止取得共享控制域。NE1..4 始终是
NOR/SRAM 片选，与 NAND2/3 的独立寄存器 bank 不混淆。

新 PSRAM/NOR/NAND 的精确参数、封装限制、有限请求和软件参考范围见
[F407 FSMC 合同](../../../../../docs/design/f407-fsmc.md)。所有端点继续共享本页的
唯一 bus owner；NOR 异步 pulse 不削弱公共同步 Flash 的返回即 idle 合同。

实际 owner 为 [`drivers/fsmc.c`](../../drivers/fsmc.c)，一个 bus 持有一份 AHB3 门控收条和精确数量的
bank 状态；普通 GPIO 原值和门控只由外层 Platform 保存。LCD／SRAM／两者的
AF12 引脚并集为 20／40／41；只选 LCD 不取得 SRAM 地址线或 GPIOF。各 bank
旧 SRAM/LCD 独立声明访问模式 A 的读写周期；新显式参数另外选择 A..D、EXTMOD、
复用地址、8/16 位、WAIT、burst 与 CRAM 页边界。官方 LL 保留 BWTR 的保留位，配置核验只比较
有定义的字段，清理时仍恢复捕获的完整原值。

## Bank 端点和借用

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

## 启动 SRAM 的永久责任

另行选入的 `drivers/fsmc_boot.c` 必须先调用实际启动证明，再接管启动 SRAM；
它和复位 BootPlan 引用同一个只读 NE3 bank 描述符。40 个启动持有引脚使用 NULL
运行期快照，只有可选 LCD 专属 PG12 取得普通快照，运行期不重写启动 NE3 或引脚。
所选 bus 必须在 main 之前使用静态初始化器
`NX_STM32_FSMC_BOOT_OWNER`，因此永久收条早于 Platform 启动或运行期接管，证明
拒绝或更早的启动失败也不能清除此既有责任。stop 在修改任何译码器／门控之前返回
BUSY；即使 LCD 借用全部结束或可选 LCD 初始化失败，也不能释放启动全局对象依赖。整个镜像通过复位退出。
整块 SRAM 的普通运行期借用返回 UNSUPPORTED，避免覆盖启动 DATA/BSS/NOINIT。
只读 geometry 以 startup_backing=true 描述事实，不授予使用权。启动计划、启动钩子
和链接脚本必须显式接入；查询几何或普通运行期借用不会启用段放置。

Native 模型和 ARM 编译只证明软件行为，不证明 SRAM 实装／速度、屏幕电气时序、
可见像素、实际复位来源、吞吐、DMA 支持或实板验收。
