# STM32F4 系列支持包

本包按系列复用官方 STM32F4 HAL／LL 与 CMSIS Device。精确器件事实覆盖八个
料号、七个器件视图，内存与封装分别选择：

| 精确料号 | 封装 | 内部 Flash（KiB） | 主 SRAM／CPU-only CCM（KiB） |
| --- | --- | --- | --- |
| STM32F405ZGT6 | LQFP144 | 1024 | 128／64 |
| STM32F407VET6 | LQFP100 | 512 | 128／64 |
| STM32F407ZGT6 | LQFP144 | 1024 | 128／64 |
| STM32F415ZGT6 | LQFP144 | 1024 | 128／64 |
| STM32F417ZGT6 | LQFP144 | 1024 | 128／64 |
| STM32F411RET6 | LQFP64 | 512 | 128／无 |
| STM32F446ZET6 | LQFP144 | 512 | 128／无 |
| STM32F479NIH6 | TFBGA216 | 2048 | 320／64 |

公共接口在 `io/`，厂商类型与可变状态私有。各料号按
[精确支持](../../../docs/delivery/support.md)选择当前软件模式和验证范围；Board
只授权已审接线，产品策略在外部。事实库存不等于每个料号的全部外设均已适配。

本页的 `docs/`、`tests/` 和软件参考链接以完整 checkout 为上下文。Source SDK
只携带 manifest 声明的所选源码；使用 SDK 时，按根 `.nexus-source-sdk.json` 的
`source_revision` 在原仓库阅读这些合同，不推定所有相对链接都包含在离线 payload 中。

[references.json](references.json)将器件分别绑定到 RM0090／ES0182、
RM0383／ES0287、RM0390／ES0298、RM0386／ES0321 的独立资料审核。ES0182 Rev19
的 92 条勘误保留八种文档硅片标识的适用性、当前模式、C 实现、回归入口与外部
约束；其他器件使用自己的审核范围。运行期型号／revision 准入由
[唯一 C 规则](private/stm32f407_silicon.h)执行，未知 revision 在硬件效果前拒绝。
软件审计不表示所有硬件限制都有修复，也不表示已观察到实板 cut 或完成 HIL。

## 源码与状态归属

| 入口 | 责任 |
| --- | --- |
| `soc.json`、`routes.json`、`references.json`、`review_profiles.json` | 已维护精确型号、路线、资料来源与独立审核范围 |
| `devices.json` | 官方器件宏、启动文件、IRQ 与时基存在性的编译事实 |
| `package.json`、`bindings.json`、`bindings/` | 事实入口、pad 坐标、领域能力、有限静态对象与固定连接 |
| 根目录 `tools/configure/` | 通用 DataProvider、领域政策、不可变 IR 与静态生成；无家族导入注册表 |
| `system.c` | 启动、所选静态时钟、显式 timebase 角色与失败退出 |
| `private/stm32f4_platform_lifecycle_impl.h`、`drivers/platform_coordinator.c` | System 与共享资源的同源 C 协调实现及通用入口 |
| `drivers/`、`private/` | 具体模式、实例状态、硬件生命周期与独立回读 |
| `CMakeLists.txt` | 所选源码、官方库与私有编译依赖 |

![FIG-31：共享 IP 与精确 SoC 的源码责任](../../../docs/assets/diagrams/driver-source-ownership.svg)

图表示构建期源码归属，不增加运行时转发层。公共 face 共享只读方法表，各实例保有
独立 context；factory 只返回已有身份，不初始化硬件。

`variants.<part>.device` 选择唯一器件 profile，再显式组合不重叠的 controller groups。
F405／F407／F415／F417 的 67 项基础事实共享维护，ETH／DCMI 由 F407／F417 选择；
F415／F417 的基础 CRYP／HASH 分别选择 `irq-blocks` 或 `dma-blocks`，共用原有
typed factory 和 controller owner；DMA 扩展状态只在选中时分配。精确通道、整字约束、
停止与密钥清理见[基础加密 transport 合同](../../../docs/design/official-stm32-provider.md#f415f417-basic-crypto-transports)。F479 的增强
加密 IP 继续保留在厂商库存，未由基础实现授予模式。F411／F446／F479 选择各自
经过审核的 controller groups，不继承上述全部基础外设。

DS8626 Rev12 对应 F405／F407，DS8597 Rev10 对应 F415／F417；新增 F411、F446、
F479 分别使用 DS10314、DS10693、DS11118。芯片、封装与所需 pad 分别校验，
不把组的并集授权给单颗器件。[内部时钟／CRC 软件参考](../../../tests/contracts/stm32f4_family/README.md)
为精确器件建立基础入口，图形、音频等模式仍按各自合同与配置验证，不证明外部 PCB
接线。System、预 C 内存与 handoff 使用所选硅片准入；资料、C 实现、模型、
ARM／SDK 及物理资格分别判断。Board／SoC 没有 Python 插件，集中主机工具只校验
并连接静态事实，硬件算法与生命周期由 C 维护。

`package.json` 显式声明事实文件，不由公共工具猜文件名。`bindings.json` 顶层
`pads` 是 pad 坐标的唯一来源，DataProvider 复用同一 BindingPolicy，禁止
`hardware.pads` 的第二份坐标。`review_profiles.json` 经同一输入快照形成不可变
ProfileSet；通用工具不内置 F4 的手册或勘误精确表。生产默认路径不再加载旧 F4
Python 构造模块；各模式与 SDK 仍须本批最终集成验收。

C 准入对 F405／F407／F415／F417 要求 DEV_ID=`0x413`，仅接纳已审 REV_ID
`0x1001`、`0x1003`、`0x1007`、`0x100f`、`0x101f`。F411、F446、F479 的
DEV_ID 分别为 `0x431`、`0x421`、`0x434`，当前均只接纳 REV_ID=`0x1000`。
未知值在硬件效果前拒绝。文档列出某个 cut 不代表运行期自动接纳，订货号与封装
也不能仅由芯片 ID 寄存器证明。

## 内存、启动与时钟

主 SRAM 与可选 CCM 采用前表的精确器件事实。具备 CCM 的料号通过
`[memory.regions.ccm]` 显式选择其独立 64 KiB CPU-only 域；F411／F446 不提供该域。
`NX_MEMORY_CCM_DATA`、`NX_MEMORY_CCM_BSS`、`NX_MEMORY_CCM_NOINIT` 使用同源生成的
linker／startup 计划，`NEXUS_MEMORY_CCM` 表达所选域。默认装配不增加 CCM 初始化
存储或放置；所有 DMA 域均排除 CCM，不预留产品 Flash 分区。

主 RAM 初始化前先启用并观察所选 CCM clock，再在第一次写入前验证完整初始化计划。
该路径仅依赖 Flash 常量和冷态本地栈，不使用 libc、global 或 heap。`.noinit` 只表示
不复制／清零，不保证复位或断电保留。

`system.c` 按唯一静态 ClockPlan 启动。F405／F407／F415／F417 的默认 profile
为 HSE8／PLL168、APB1 42 MHz、APB2 84 MHz，也可选择经校验的 HSE／HSI、
直接 SYSCLK、PLL 和总线分频。F411／F446／F479 保留直接 HSI16，并分别接纳显式
HSI PLL96／PLL168／PLL168 高速 RUN 参数；总线、Flash latency、VOS 与辅助输出
仍按精确器件校验。F446 的独立 M PLLSAI Q 可供所选 SAI1／2 master；F479 的
PLLSAI 与主 PLL 共用 M，可同时提供 SAI1 的 Q 与 LTDC 的 R。F479 的 M10/N210
高速参数保留既有 M10 辅助计划的 12.288 MHz 音频时钟与 4.8 MHz 像素时钟。
这些输出由唯一 System 管理；高速参考不授予其他外设、180 MHz overdrive 或新料号
STOP／Standby 支持。完整参数和失败顺序见下方 System 合同。下文未另说明的
MHz 数字均是原 HSE8／PLL168 示例，实际 C 与生成常量使用所选计划。包内 timebase
角色显式选择 timer，当前生产绑定为 TIM2；TIM5 仅有替代路径的模型验证，不扩大
板卡资格。所选 timer 整数分频为 1 MHz，通过 overflow IRQ 扩展 32-bit counter，
IRQ 至少每 2^32 微秒服务一次，不占 RTOS SysTick。PLL48 与 USB 功能
owner 分开取得；没有消费者的直接输入 profile 不开启主 PLL。

初始化分别记录 primary、cleanup、remaining effects；HSI 切换失败保留责任，
不误关活动 PLL。单调时钟尚不存在的 bootstrap 阶段使用明确 poll 界限。不提供
运行期 DVFS。静态参数、Flash latency、CSS、STOP 新 epoch 和监督详见
[System 合同](README-system-power.md)。

## 官方 HAL／LL 复用与 FPU 约束

官方 ST Device 与 Arm CMSIS 子模块提供寄存器布局、mask、完整 GCC vector／startup。
STM32F4 HAL Driver 为私有实现提供所选 HAL／LL；完整模式由唯一硬件 owner 推进。
审核版本、精确 Git、CMSIS 配对、文件摘要和许可证由
[stm32f4-hal.lock.json](../../../dependencies/stm32f4-hal.lock.json)固定。

厂商头和 Handle 留在私有闭包；CMake 按模式选择实际 C 单元。UART DMA、DCMI、
SAI、LTDC／DMA2D 和基础 CRYP／HASH 的所选模式复用经审阅的官方路径；其他模式
根据其取消、排空和 erratum 合同选用 LL 或局部 C。
不调用全局 HAL 初始化，不启动 HAL SysTick，也不由官方函数存在推导 Nexus 支持。
[官方驱动合同](../../../docs/design/official-stm32-provider.md)规定完整模式的
所有权、时间、成本和交付边界。

`SystemInit` 开启 CP10/CP11，此 hard-float 平台不关闭 lazy FP context。FreeRTOS
CM4F port 显式开启 ASPEN/LSPEN。因此 ES0182 Rev19 §2.1.2（Arm776924）对所列全部
F407 cut 都是集成约束：当受影响 VDIV/VSQRT 条件可能出现时，中断 handler 在异常
返回之外必须执行超过两条指令。须沿生成尾分支进入真实 callee，不能计算不可达填充
或 literal。外部 handler 同样遵守，修改编译器／优化／handler 后重新检查实际 image。
平台不会绕过内核 ABI 修改 FPCCR，详见
[短 ISR 合同](../../../docs/design/official-stm32-provider.md#cortex-m4f-short-isr-constraint)。

## 模式和维护入口

唯一当前模式矩阵见[精确支持](../../../docs/delivery/support.md)，机器生成的
路线／装配／生产源码关联见[能力矩阵](../../../docs/delivery/capabilities.md)。
本页保留原有基础驱动的硬件顺序；扩展模式的权威合同就近维护，不复制第二份列表。

| 模块 | 具体合同 |
| --- | --- |
| System／内存／监督／低功耗 | [System](README-system-power.md) |
| UART 协议与收发 | [USART 线模式](../../../docs/design/uart-wire-modes.md)、[UART DMA](../../../docs/design/uart-dma-rs485.md) |
| SPI master／slave、TI、CRC | [帧事务](../../../docs/design/spi-frame-modes.md) |
| I2C target／SMBus device | [有限 target 交换](README-i2c-target.md) |
| 多通道 PWM／单脉冲／DMA 序列 | [定时器波形](timer_waveform.md) |
| USB Device／Host／显式 OTG | [USB](../../../docs/design/usb-device.md) |
| 同步／交错／注入多 ADC | [ADC group](../../../docs/design/f407-adc-groups.md) |
| DCMI 与 I2S2／3 | [capture／audio](capture_audio.md) |
| F479 LTDC／DMA2D | [图形与归还合同](../../../docs/design/official-stm32-provider.md#f479-ltdc-scanout-and-dma2d-memory-operations) |
| F415／F417 基础 CRYP／HASH | [官方 transport 与请求所有权](../../../docs/design/official-stm32-provider.md#responsibility-boundary) |
| RTC／备份／自主唤醒 | [RTC](../../../docs/design/f407-rtc.md) |
| Flash／OTP／Options、SD／MMC／SDIO | [物理存储](../../../docs/design/f407-storage.md) |
| SRAM／PSRAM／NOR／NAND／8080 | [FSMC](../../../docs/design/f407-fsmc.md) |
| Ethernet 帧与时间戳 | [原始帧](../../../docs/design/ethernet.md)、[增强帧](../../../docs/design/ethernet-frames.md) |

IWDG 数据手册 17–47 kHz 是 VDD=3 V、TA=-40..105 °C 条件下的表征；当前没有
声明满足条件的 operating profile，不能直接据此保证时限。名义配置可成功，但启用后
物理界限查询仍可 `UNSUPPORTED`，同时保留 enabled／irreversible／debug 事实。

独立核心板和 V4.1 包各自保留原理图与跳线身份，不替换旧 V3.1。内部 Flash／IWDG
可以显式选择，但不会预留存储或在 startup 启狗。SoC AF 路线不证明 Board 接线、
外部上拉、器件实装或物理支持，用户必须提供已审绑定。I2C GPIO bus-clear 为独立
通用组件；未声明的 ADC 模式仍拒绝或不在 schema 内。

已选 erase-aligned offset image 有明确 startup／vector／linker／ELF/BIN 检查，
不包含 boot 认证、激活或运行镜像 handoff 资格。精确 mode／Board／生产源码见
[派生矩阵](../../../docs/delivery/capabilities.md)。

## 窗口看门狗与复位来源

`watchdog` 的 `window` 模式通过同一公共接口与 typed factory 提供 WWDG。
无写 admit 位于 System 启动前；prepare 在 System 建立真实 ON／RDY／SW／SWS 时钟
profile 后取得 gate 与 Sleep clock。它拒绝未取得的 gate、reset、vector、WDGA 或外来 EWI，
不会在 startup 启狗。复位原因从 IWDG 拆入 SoC `reset.c`，只选择 WWDG 也能调用
`nx_reset_cause()`，不需要链接 IWDG transport。

`window_ticks` 为 reload 后至少经过的 counter decrement 数，范围 1–62，默认 1。
显式 enable 以 64-bit 向上取整选择 1／2／4／8 分频和 1–64 steps；
`R=0x3f+steps`、`W=R-window_ticks+1`，只准入 `0x40<T<W` 的喂狗窗口。
`steps>=window_ticks+2` 保证至少一个合法计数；过短或过长的 timeout 返回
UNSUPPORTED，不静默延长。prescaler 相位未知，ticks 不能换成保证的微秒；查询保留
enabled／irreversible 事实，而物理 timeout bounds 为 0/0 unknown。

enable 先写并核对 CFR 与 CPU debug freeze，最后一次 CR 写同时提供 WDGA 与
有效 T6／reload。官方 HAL 的先激活顺序和 LL_Enable 的旧 counter RMW 不满足该合同，
因此原子激活与带回读的资源事务直接由 C 维护；读数与位定义继续复用官方 LL／CMSIS。
写前记录不可逆责任，不能以失败回读推定可关停，也不隐式重试。feed 在最短 IRQ mask
窗口内读取并检查实际 counter／window：早喂返回 BUSY，迟喂返回 STATE，均不写入或等待。

未激活的 close 依次恢复 CFR、debug、Sleep clock、运行 gate；回读失败保留对应收条，
并立即撤销再次激活的准入。已激活或未证实激活的 stop 返回 UNSUPPORTED，真实 Core
生命周期因此保留 System。只有产品显式选择喂狗／复位策略；没有默认任务、自动喂狗、
自动 EWI 恢复或 STOP／Standby 保证。可选 EWI 的独占与不可逆责任见
[监督合同](README-system-power.md)。软件参考为 `*wwdg*.toml` 与 `*watchdogs*.toml`，不建立
物理复位、窗口时限或安全认证。

外来 EWI enable 为 sticky 位，只能由硬件 reset 清除；该模式不会把普通写入伪装成
可撤回 EWI 的恢复。遇到未取得的 EWI／vector 时保持 BUSY 与现有责任。

## GPIO 电气与外部 EXTI

普通 GPIO input／output 和 EXTI0–15 分别复用既有 GPIO／EXTI C 驱动，共用静态
factory 与唯一 Platform 冷态引脚 owner／快照。路线覆盖所维护封装实际引出的 pad；
接线和负载仍须由 Board 授权。两个不同 port 的同号 EXTI 线不能同时取得，因为它们
共享同一 EXTICR 选择。
`mode="edge-event"` 的 EXTI5–9 和 EXTI10–15 分别使用共享 IRQ；只有最后一个
IRQ owner 退出才撤回该向量。

`mode="hardware-event"` 选择同一 GPIO／EXTICR 线的 EMR 硬件事件脉冲，
显式选择 `edge="rising"`、`"falling"` 或 `"both"` 与可选 `pull`。
它不接纳 `irq`／`event_capacity`，不声明 NVIC vector、不生成 handler，也不分配
事件队列或 wake callback。强类型 factory 仍返回已有的 `nx_exti_port_t`；
`level` 采样 pad，`stop` 撤回同一个 EMR owner，`read` 与 `attach_wake` 明确返回
UNSUPPORTED。硬件事件可合并，CPU 事件锁存与 WFE 协议由调用者拥有，不伪造边沿
历史、计数或时间戳。Platform suspend 默认关闭该 owner；本模式不自动保留为
STOP／Standby 唤醒源。

硬件事件的 C owner 在 GPIO／SYSCFG gate 已取得后配置输入与 mux，最后启用 EMR。
它只保存本线的原 EXTICR nibble，失败保留静态 config 与清理责任；stop 确认本线
EMR、触发源及 pending 退出，再恢复 mux，随后由唯一 Platform 快照恢复 GPIO。
同一共享 vector 的 IRQ peer 与其他 EMR 线保持不变。对单线字段直接写入的边界是
精确写入顺序、回读及失败保留；GPIO 电气操作复用官方 LL，不引入第二套寄存器脚本。

普通 `mode="output"` 的 `speed` 可选 `low`、`medium`、`high` 或 `very-high`。
省略该字段表示保持已有 OSPEEDR，不接受用户填写 `preserve`。`input` 和 EXTI
不接纳这个字段。PC13／14／15 的普通输出只接纳 `low`，省略也采用该档位。
速度是硬件驱动档位，不是实际输出频率、上升沿或负载能力的测量保证。

所选 HSE、RTC 的 LSE 与 Board 声明的 SWD 通过同一资源计划保留对应 pad。
C 无写准入还拒绝实际外来 HSEON／HSERDY 或 LSEON／LSERDY 尚未退出的占用，
不关闭另一 owner 的振荡器。显式速度由官方 LL 写入，执行屏障并确认回读后才写
对应 pin 的输出模式。部分失败可能已改变前面的 pin，仍由 Platform 保留 bound
owner／clock 并负责清理。普通 stop 维持声明的 inactive 电平并撤销 face；所有
publisher 退出后，最终 OSPEEDR、AF、pull 和 latch 恢复仍由唯一冷态快照完成，
不增加另一套恢复存储。

ES0182 §2.2.18 的 PC13／LSE 耦合独立于输出速度：晶振 LSE 的 ON 或 RDY 任一仍
置位时，普通 GPIO、IRQ EXTI 和硬件事件的 PC13 冷态准入均无写拒绝；明确的 LSE
bypass 不受此项耦合限制。RTC 的晶振模式另外保留 PC13 静默责任，不能与 PC13
GPIO／AF／EXTI 同时选择；LSE bypass 的板级授权与 RTC 生命周期由 RTC owner 维护。

软件参考见 `tests/contracts/gpio_exti_assembly/`，分别选择 VE 裸机与 ZG FreeRTOS
装配。它们验证静态实例、实际 ARM 链接、资源预算与方法保留，不证明调度器已经
运行、真实边沿已经采集或 PCB 电气已经验收。事件仍按既有合同保留 COALESCED／LOSS，
不提供自动消抖或精确物理边沿时间。
同目录 `ve-hardware-baremetal.toml`／`zg-hardware-freertos.toml` 复用相同 Board
与 C 消费者，验证全部十六条硬件事件线的静态 factory、无 IRQ／队列及真实 ARM 链接。

## PVD 电压检测与内部 EXTI

`exti` 的 `pvd-level` 与 `pvd-events` 共用内部 `pvd` Board binding，
`threshold_code` 必须显式选择 0–7，不提供默认电压政策。编码与迟滞边界依据
RM0090 §5.2.3、DS8626 Rev12 Table19；官方 LL 注释中的简写电压不作为精确阈值。
例如 code5 的典型上升／下降阈值为 2.93／2.84 V，仍有器件、温度和电源条件下的
min／max，不是可编程 ADC 或电压计。`.level=true` 始终表示 PVDO=1，即低于阈值；
PVDO rising 表示电压下降，falling 表示电压恢复，不翻转为 power-good。

轮询只保留 3 字节比较器收条，不分配队列，也不选编 GPIO 或 EXTI 事件驱动。
事件模式增加现有 EXTI state 与显式 `event_capacity`，由 `PVD_IRQHandler` 直接
分派；复用同一 read／stop／wake 合同，不增加 worker、运行时 registry 或接口转发层。
IRQ 先精确 W1C 清除 PR16，再发布 COALESCED 事件与 wake；溢出通过 LOSS 暴露。
启动前的低电压可由 level 读到，不伪造历史边沿；不启用 EXTI EMR。

无写 admit 在 System 启动前检查 EXTI16／PVD vector 的外来占用；PWR clock 尚未
开启时不读取 gated 比较器状态。prepare 在 System 取得 PWREN 后重查 PVDE、保存
PLS，复用 LL 配置并观察回读，再清 pending、开放实际 IRQ。PVD 不取得或关闭 PWREN，
不复位整个 PWR，不修改 VOS／DBP。只提供当前原始信号，不承诺资料未定义的启动稳定时间。

关闭先撤销 level 的 READY 资格，再撤回 EXTI source／NVIC 并观察 handler 排空，
最后关闭 PVDE、恢复原 PLS 与 IRQ priority。任一步失败保留收条、存储与 System
清理责任，允许显式重试。ring、完整 owner 与冷配置必须分离，不能覆盖清理状态。
本模式不提供低压保存任务、BOR／安全保护、STOP 唤醒或物理阈值资格。

## UART、DMA 与执行进度

有限 `dma-tx` 复用同一个 ST HAL owner 与只读 ops；冷态 C 事实集中维护精确的时钟、
pad、TX／RX stream、IRQ 和 selector，HAL admission、接收引擎与资源取得复用
`uart_hardware.c` 中的只读事实，不维护每实例路由副本。
六个实例没有新增可变字段，不使用 TX-only 模式中未选择的 RX DMA／continuous
配置。GPIO 快照只属于冷态 lease；已经取得资源的直接 HAL 初始化无需重复持有快照。

| 实例 | TX／RX DMA stream | selector | 默认 profile 的 APB 时钟 |
| --- | --- | --- | --- |
| USART1 | DMA2 stream7／2 | 4 | 84 MHz |
| USART2 | DMA1 stream6／5 | 4 | 42 MHz |
| USART3 | DMA1 stream3／1 | 4 | 42 MHz |
| UART4 | DMA1 stream4／2 | 4 | 42 MHz |
| UART5 | DMA1 stream7／0 | 4 | 42 MHz |
| USART6 | DMA2 stream6／1 | 5 | 84 MHz |

`tests/contracts/multi_controller_assembly/*uart6*.toml` 覆盖两种封装、裸机／FreeRTOS 和四种 RX DMA／duplex
模式的六实例链接参考。每种模式共享一张只读 ops 表，六个 context 独立；不授权 PCB 接线。

额外 byte/event 路线为 USART3 PB10/PB11 AF7、UART4 PA0/PA1 AF8、UART5 PC12/PD2
AF8、USART6 PC6/PC7 AF8。USART2/3、UART4/5 从 APB1 取得 42 MHz，USART1/6 从
APB2 取得 84 MHz。冷准入在取得资源前检查精确引脚、两个 GPIO bank、clock、gate 和
IRQ。小型只读 cold descriptor 表不在字节热路径使用，不增加可变实例状态。
RX DMA 根据实际 lane 选择低／高 status bank、W1C mask、channel 与 vector；
duplex 的 TX 和 RX 独立清理，停止失败不释放借用。表中只开放固定路线，不能据此
推导未选择的 UART 协议、其他 DMA stream 或引脚已具备资格。

`dma-tx` 的一个 owner 组合官方 HAL 有限 TX 与分离 LL IRQ-byte/event RX，所选
单元 callback ABI 都为 1，实例共享只读 ops。RX 不交 HAL，TX abort 不改变 RX
责任。短 CPU guard 延后抢占的所选 vector，不在 HAL 调用外套长全局 IRQ mask。
HAL DMA handler 每次进入都要求 EN 清除；HT-only 使用有界官方 acknowledgement。
错误撤回被拒时先 mask 此 vector，再交 task 推进 HAL，避免锁定 IRQ poll。
排空／deinit 失败保留 request、pad、gate 和清理责任。私有 bounded atomic 与
nearest-BRR 编译配置依赖受审单 CPU owner，不修改厂商源码。`HAL_GetTick` 使用既有
所选 timebase epoch，`SystemCoreClock` 在失败清理中追踪确认的 HCLK，不启动 HAL 或 SysTick。

有限 UART／SPI DMA 只借用已审 DMA 可达域中的调用者 buffer，排除 CCM。DMA TC
不能释放借用，还须 UART TC／SPI idle、memory channel disabled 和 IRQ detached。
排空失败保留 QUARANTINED。`irq-blocks` 是逐字节 IRQ＋IDLE，不是 DMA RX；DMA RX
另选有限或连续双缓冲 provider。拒绝 `dma-tx`＋IRQ blocks；双向显式选 `dma-duplex`
或 `dma-duplex-continuous`。连续模式不覆盖 READY/BORROWED，缺 replacement 时停止
并报告 loss，不虚构错过事件的准确次数。

SPI endpoint 可通过 `[spi_device.<id>]` 的 `lsb_first = true` 选择硬件 LSB-first，
默认 false 为 MSB-first。三个控制器的普通 byte face、8／16 位增强 polling 和 DMA
事务均使用同一冷配置；端点间可以有不同位序，只在线端与引擎空闲后写 CR1，不在
传输中改变格式。内存中的字节或 `uint16_t` 数值保持不变，不隐式反转、复制或改动
DMA 宽度。该选择不用于 I2S／TI；固定 MSB 的 BMP280 驱动拒绝 LSB 配置。
新字段使用既有 padding，ARM polling endpoint state／只读配置仍分别为 20／16 字节。

SPI 每次 CS 命令都在原子 BSRR 写入后以 DSB 和官方 LL 读取 ODR，核验本 CS latch；
这不证明外部引脚电压。通用 GPIO 写热路径不增加屏障。polling 返回后不再访问调用者
payload，仅保留静态 endpoint 身份与 fault，供显式 recover／close 重试撤回。
普通 DMA 在受理后遇到 assert 失败仍保持 ACCEPTED，未启动两个 engine；release
失败不发布 SETTLED，保留原 request／buffer，逾期隔离后继续 service。增强 DMA
复用既有整链 owner，在最终 CS 释放前保留全部段；关闭跳过已完成撤回的 CS。
原 byte face 的 length 仍按字节计；增强 16 位数据是本机存储的 `uint16_t` 数值，
长度与 NDTR 按 word 计，不做字节序转换或隐式复制。

SPI1／2／3 的 DMA 冷态连接统一使用 `nx_stm32_spi_dma_prepare` 与
`nx_stm32_spi_dma_close`，不再因无关 peer 是否存在切换另一套行为。foreign IRQ 和 DMA EN 先于 pin／clock 效果
拒绝；保存两条实际 IRQ 的原始优先级，撤回与恢复均读回。prepare 失败通过共用
cold receipt 恢复原 CR／GPIO／gate，拒撤时保留收条供同一 owner 重试。成功 close
排空后撤回 CS／信号驱动和 SPI 责任，最终原 pad 与 GPIO clock 仍由唯一 Platform
pin lease 归还。当前维护的
M4F hard-float／short-enum ABI 下，DMA state／endpoint 保持 64／24 B，统一冷态
收条为 32 B；相对旧 single 28 B、旧 pair 12 B 分别增加 4／20 B，用于完整快照和
失败恢复。此项是对象布局测量，不代表整固件 Flash 或物理 IRQ 时延。

ADC1／2 外部输入按 DS8626 Rev12 Table7 对应 PA0--7、PB0--1、PC0--5 的通道
0--15。Board 必须明确授权相应 SoC route；新增 silicon route 不修改既有 PCB
接线。序列只能选择 Board 授权 route 中的通道，资源计划仅取得实际使用的唯一
引脚；rank 可重新排序或重复，最多 16 个。重复通道共享同一个 SMPR 字段，
sample time 不一致时在取得资源前拒绝。
软件 shot/scan 保持逐 rank 转换语义，不承诺通道同时采样。

ADC3 的通道 0--3 使用 PA0--3，10--13 使用 PC0--3；ZG 另有 PF6--10 的通道
4--8、PF3 的通道 9、PF4／5 的通道 14／15。VE 没有这些 PF 引脚，故只开放八种
外部输入，仍允许用重复输入组成最多 16 ranks。封装差异由事实和 Board route
验证，不能从芯片头文件中存在 GPIOF 推导引脚已引出。三个转换器共用同一套软件
shot/scan C、只读 ops 和类型化 factory，实例 context 保持独立。

RM0090 §13.3.2／§13.13.16 区分每个转换器的数字接口 gate 和共享模拟 ADCCLK。
公共分频来自唯一静态 ClockPlan，默认 PCLK2／4，即 21 MHz。普通独立 ADC
拒绝继承 MULTI、公共 DMA 或 DDS；多 ADC 使用独占公共域的 `adc_group`。匹配的配置不再
写 CCR；调整前临时打开尚未可读的数字接口，逐项记录责任，确认三个 ADON 都为零后
才设置分频，再撤回本次临时 gate。启钟或撤回回读失败保留 receipt，由 close 重试。
整个冷态准备与其他 ADC 启动必须串行；不提供动态调频、并发热接入或共享 ADCRST。
停止一个实例不改写 CCR，也不关闭其他实例的 gate。

regular 软件采样在 ADON 写入经过 DSB 和回读确认后，遵循下述外部 4 µs／
内部 11 µs 稳定期。结束时独立回读停止
结果；失败返回 IO 并保留设备清理责任，新采样返回 BUSY，平台责任者须重试关闭。
返回时不再访问调用者的样本或 count，已完成前缀仍由 count 描述。转换器、接口
gate、临时观察 gate、TIM3 和 DMA IRQ 分开记录责任，复用现有状态字节，不增加
可变字段；只读配置新增控制器身份。TIM3 或 DMA IRQ 已被占用时拒绝取得，未取得
的 IRQ 不参与失败清理。

`resolution_bits` 在主机配置时确定，默认 12，可显式选择 6／8／10。静态 C 配置
保存对应的 RES 编码，C owner 在 ADON 清除后通过官方 LL 设置；poll、有限 DMA
和连续 DMA 使用同一选择。依据 RM0090 Rev22 §13.7，每 rank 的转换时间为
所选 sample cycles 加分辨率位数。速率准入和 sequence metadata 使用该完整成本；
降低分辨率不改变 DMA halfword 宽度和 `uint16_t` 原始样本；块容量与长度仍以字节
计，且必须包含整数个完整 scan。分辨率选择不证明实际信号的精度、有效位数或可达
采样率。

`tests/contracts/adc_external_assembly/` 提供两种密度的软件参考装配，使用三个
GPIO bank 的 16-rank 反向序列，以及 VE／ZG 的三转换器独立实例装配；不代表实际
板卡接线。本节为独立转换器模式；同步／交错／注入组使用
[ADC group](../../../docs/design/f407-adc-groups.md) 的唯一组 owner，不能和
独立 ADC 实例同时取得同一个转换器。ADC1／2／3 分别使用
DMA2 stream4 channel0／stream2 channel1／stream1 channel2，复用有限和连续 C
实现与只读 ops。静态 `trigger` 可选择 TIM1 CH1–3、TIM3 CH1／TRGO、TIM4 CH4、
TIM5 CH1–3、TIM8 CH1／TRGO，默认 TIM3 TRGO。只读 C 事实表同时提供精确 timer、
APB gate、kernel clock、EXTSEL 与 compare channel；两个 DMA 模式共用 C 配置函数。
trigger 字段使用现有 state／config padding，不引入额外实例分配。默认 ARM 短 enum
ABI 下，有限 state／连续 state／只读 cold config 分别保持 128／200／36 字节，
face、stream 与调用方 payload 另计。compare 在 ARR，
TRGO 为 update；配置 UG 在开启 ADC EXTEN 之前完成，不取得输出 pad 或 timer IRQ。
TIM1／8 为 168 MHz，其余为 84 MHz；ADC clock 仍为 21 MHz。
每个 ADC 独占所选 timer，三者使用不同 timer 时可独立运行；相同 timer 的其他
personality 仍互斥。原配置选择存入 cold owner，在撤回失败后保留精确 timer 身份，
重试不能误关原 TIM3 或另一个 timer。所选 Core timebase 不可重复取得，当前绑定为
TIM2；EXTI11 暂未接入。
软件参考为 `tests/contracts/multi_controller_assembly/*adc-timers*.toml`，不建立
物理采样资格、PWM 相位关系或硬件同步多 ADC 支持。

F407 的 ADC1 内部通道 16 为温度、17 为 VREFINT、18 为 VBAT／2。pinless
`adc1-internal` route 共用现有 shot／scan／有限和连续 DMA，不取得 GPIO，也不新增
运行时字段；静态 channels 与已有 cold receipt bit 表达路径身份和清理责任。
21 MHz ADCCLK 下，内部输入只准入 480-cycle sample time。温度、电压和校准不进入
驱动，返回值保持原始计数，`reference_mv` 不代表实测 VDDA。

冷态 prepare 先拒绝已有的所需 TSVREFE／VBATE，取得前记录责任，写后独立回读。
close 先证明 ADC 停止，再只撤回所取模拟路径，最后释放 clock；任一步拒绝保留责任，
其他未取得路径保持原值。为覆盖整微秒时间戳的取整，ADON 后外部输入等待 4 μs，
含内部输入等待 11 μs，三个执行模式使用同一 helper，不保留额外启动时间戳。
这遵循 RM0090 Rev22 §13.10／13.11 与 DS8626 Rev12 的启动和采样时间要求，
不承诺实际模拟精度、功耗或 VBAT 测量条件。

`drivers/adc_injected.c` 以同一 ADC state／cold config 和共享只读 ops 实现
`injected-shot`。一次软件触发完成 1–4 ranks，短组按 RM0090 的 JSQR 位置排列，
通过官方 LL 取得 JDR1–4，offset 归零；不使用 regular SQR／DR，也不取得 TIM3。
采样值暂存于有界的 8 字节栈数组；全部采样、deadline 和停止均成功后才发布。
超时或停止失败不发布部分数据，不保留调用者指针；停止失败保留 controller 清理责任。
该模式不增加实例 RAM，不与同一 ADC 的 regular／DMA 同时使用，不提供自动注入、
外部注入 trigger 或非零 offset。内部输入沿用原有模拟路径 owner 与启动等待。

有限 ADC `trigger-dma` 在发布完整块前停 trigger／conversion／DMA，稳定后由显式
service rearm，块间可有间隙。消费者借用不能被覆盖，未释放时 stop 保持 BUSY；
所选 timer 与其他 timer owner 独占冲突。独立 `trigger-dma-continuous` 维护双缓冲和 replacement，
进度不足时停止而非覆盖消费者。

stream、slot 表和每个 block 的完整容量必须独立于具体 provider 状态、DMA 域表、
channel／sample-time 事实及 wake；连续模式的检查包含 base 之后的扩展状态。
`stream_start` 的 SUCCESS 表示已接纳会话，warmup 和 trigger 可能尚未启动。
首个 producer loan 前无法表示有限 warmup 时返回 EXHAUSTED，不取得存储；借用后
ADON 回读或实际启动时间戳失败则锁存故障，由 service 观察，不能伪装成无借用拒绝。
单块模式保留 FILLING，连续模式保留 posted targets；DMA 未证明停止时不归还 loan。
发生过停止尝试后，继续使用同一 stop 完成退出，再回收 stream、buffer 或 owner。
regular／injected CPU sample 的有限 warmup 也不会饱和成无限等待；返回前执行停止
回读，不保留调用者的 sample／count 指针。

普通 PWM 共用 TIM3／4／5 的四通道、TIM9／12 的双通道，以及 TIM10／11／13／14
的单通道实现。精确 pad 与 AF2／3／9 路线以 `routes.json` 和所选 Board 绑定为准。
TIM9／10／11 使用 APB2 timer，其他所列 timer 使用 APB1 timer，
默认分别为 168／84 MHz。C 从 controller 身份派生 APB、AF 与 gate，不新增实例字段；配置生成只连接
精确静态事实。所选 ADC trigger、PWM、capture 与 encoder 按同一 timer personality
独占。TIM1／TIM8 advanced PWM
分别保留 TIM9／TIM12 共享 vector。Core 独占所选 timebase，当前为 TIM2；
TIM6／7 为 DAC trigger。

互补输出共用一套 C 算法，以两个编译期 wrapper 直接操作 TIM1／TIM8；CMake 只选中
所需 controller 实现，不增加运行时设备派发。TIM1／TIM8 均可选择 CH1–3 的一组
互补输出，使用所选 APB2 timer clock，默认为 168 MHz；通道路线见
[定时器合同](../../../docs/design/timer-advanced.md)。
break IRQ 分别为 TIM1_BRK_TIM9／TIM8_BRK_TIM12，后者的 TIM12 peer gate 属于
APB1，TIM8 gate 属于 APB2。资源冲突在配置期拒绝，冷态也检查实际 peer gate。

共享冷态流程只引用静态硬件事实，不改变热路径。完整配置身份与 pin／IRQ／gate 收条
保留到最后一次清理成功，包含热态 release 后的引脚回滚失败；不能换配置、snapshot
存储或 timer wrapper 重试。snapshot、控制器、冷态与只读配置区域必须互不重叠。
关闭先观察同组所有输出 inactive，再逆序恢复引脚；拒绝撤源或回读失败保留原责任。
启停、set、break 故障锁存和显式 rearm 使用原公共接口，不增加 heap、worker 或自动重启。
`*advanced-pair*.toml` 是两路并存的软件参考，不授权 PCB 接线或功率级安全。

输入捕获以同一 C 引擎接入 TIM1／3／4／5／8 的四个通道、TIM9／12 的两个通道和
TIM10／11／13／14 的单通道。只有 TIM5 使用完整 32-bit，其余为 16-bit modulo count。
小 timer 的 wrapper 在编译期剪除未实现的 CR2／SMCR／CCMR2／CCR 寄存器访问。
通道来自 Board route：controller 事实维护时钟、IRQ 和共享资源，channel 事实维护
pad；生成器只写静态配置和连接。`stm32f407_timer_pins.h` 在 C 中统一冷态 pin 设置
与热态准入，IRQ 不扫描其他通道，只处理所选 CCR、CCIF 和 CCxOF。
冷配置记录精确 timer 身份，阻止另一 controller 的 wrapper 清理原会话。
TIM1／8 capture 使用独立 CC IRQ27／46；small timer 的共享 vector 仅按真实 peer
BIE／UIE／TIE／COMIE 源判别，不因 peer clock 开启就拒绝合法并存。失去 RCC gate 时，
stop／release／迟到 IRQ 不访问 gated MMIO，也不解除原借用；恢复 gate 后允许继续
清理，不因 PPRE 或 pin 字段变化挡住撤回。精确规则见上述定时器合同。
`*timer-channels*.toml` 同时选择三路独立 capture 和两路 advanced PWM；ring 在
capture start 时由调用方提供，不在 startup 分配。原公共接口与可变实例布局保持不变。

`period_ticks` 表示完整周期，ARR 为 period−1。上限保留了 CCR 表达 100% duty 的
空间：TIM5 为 UINT32_MAX，其他已审 timer 为 65535。set 只更新 duty 的预装载，不改变周期
或 PSC；state 报告已暂存值，实际输出在 update event 生效。长周期不会自动插入
更新事件或额外任务。冷配置含精确 timer／GPIO／channel／pin，C 在启钟前校验；
启钟或撤回失败保留选中 timer 的清理责任，不修改其他 timer。已初始化的外部 GPIO
对象不能被重新绑定。运行时 state 未增加字段；该扩展增加只读静态配置。

每个 port 只有一个执行者，不引入 heap、registry、字符串查找、默认 worker、OS
lock、通用设备引用或预留 copy pool。UART IRQ 搬运字节和事实，task service 观察
传输 deadline，并在 cancel／stop 期间持续运行。晚到 TC 可以结清隔离传输，但在此
之前 request 保持借用。SETTLED 后无 IRQ／provider 引用，adapter／observer 还需
独立排空。

SPI3 的 AF6 pad 是 PC10／11／12 或 PB3／4／5，APB1 为 42 MHz；固定 DMA1
TX stream5 channel0、RX stream0 channel0。初始化先检查选中 vector 的 enable、
pending 和 active，再取得 publisher；恢复只脉冲 SPI3 的 APB1 reset 位。

I2C 的 `max_hz` 默认 100000，允许名义 5129–400000 Hz，表示上界而非精确速率。
公共 C 时序计算向上取整 12-bit CCR；Standard mode 的计算上界统一不超过 88 kHz，
覆盖 ES0182 Rev19 §2.10.4 的所有 repeated START，而不是仅限制 10-bit endpoint。
超过 100 kHz 时显式使用 Fast-mode 2:1 duty 和 300 ns TRISE 公式，不自动变更模式。
固定 42 MHz PCLK 下默认配置选择 CCR=239，名义约 87.866 kHz；只读 trait 保留
`configured_max_hz`，并用 `nominal_hz` 报告整数分频后的名义速率（向下取整 Hz）。
该字段为零表示缺少实际时钟集成，不能作为测量结果。依据 RM0090 §27.6.8，
固定 42 MHz PCLK 下
`max_hz=400000` 选择 CCR=36，名义约 388.9 kHz，不声称达到精确 400 kHz。
该计算不保证实际时钟误差、上拉和总线电容满足规范。
冷态取得后保存 CCR；局部 recover 复用该配置并保留已挂接的 wake，不继承可能损坏的
寄存器内容。请求、通知发布者或 DMA 尚未退出时先返回 BUSY，不复位硬件。
新增 CCR 使用现有 state padding，ARM 可变状态仍为 64 字节；每个只读冷配置增加
4 字节。polling 与异步模式不增加 heap、队列或 worker。

I2C endpoint 的 `address_bits` 默认 7，地址限于 8–119；显式 10 时允许 0–1023。
同一 bus 可连接两种宽度，重复身份包含宽度。依据 RM0090 §27.3.3，10-bit read
先发送 header-W 与低字节，清除 transmitter ADDR 后再请求内部 repeated START，
最后发送 header-R；仅最终 ADDR 进入既有 ACK／POS／DMA 接收窗口。地址字节不计入
payload，内部 restart 不重新编程 DMA stream。NACK、仲裁、取消和 timeout 继续
进入同一 drain；硬件或 publisher 未退出时保留借用，不隐式重放事务。

ES0182 Rev19 §2.10.4 的 Standard-mode repeated START 限制与 PCLK、地址宽度无关。
默认 100 kHz 上界可以同时接入 7／10-bit endpoint；7-bit 多消息与 10-bit 内部
restart 在任何借用或传输效果前再次检查实际 CCR，拒绝外部改成不安全速率的配置。
不在事务中动态改分频，不依赖 target 不拉伸时钟或小于 300 ns 的上升时间假设。
该处理不建立物理时序资格；F4 V1 尚不接受 V2 的 `bus_hz_min` 时序 envelope。
ARM 实际编译比较：endpoint 仍为 8 字节、异步 state 仍为 64 字节。控制器 trait 的
`address_bits=0` 表示宽度归 endpoint；具名 endpoint 的 selected contract 返回 7 或 10。
Native 未因此取得 10-bit 支持，I2C target／SMBus／PEC 也未由 master 扩展推定实现。

I2C3 固定 PA8 SCL／PC9 SDA AF4、APB1 42 MHz，DMA1 RX stream2／TX stream4
channel3，EV／ER IRQ 为 72／73。C 冷态 owner 从 controller 身份取得两个 GPIO
bank，逆序恢复每个引脚，再恢复原有三个 gate 位；6 字节 receipt 与热路径状态均
不增长。失败保留原 gate／snapshot 和撤回责任，后续 close 可重试。

I2C recover 同时检查 BUSY 与已保留 SCL/SDA GPIO 实时输入；即使 BUSY 清除，只要
线路低就返回 IO。局部 reset 不打 GPIO 恢复脉冲、不重放事务、不保证未来总线可用；
缺线路事实时在寄存器写入前拒绝。

## 原始 Encoder 与资源归属

`drivers/encoder.c` 复用官方 LL，提供 TIM1／3／4／5／8 的硬件 x2 TI1／x2 TI2／x4 TI12
原始计数。TIM1 使用 PA8／PA9 AF1，TIM8 使用 PC6／PC7 AF3；TIM3／4／5 分别使用
PA6／PA7、PB6／PB7、PA0／PA1 AF2。filter code 为 0–15，filter clock 来自所选
APB timer，默认 APB2／APB1 分别为 168／84 MHz。counter 在 prepare 后保持停止，
stop 不清零 CNT；只有 TIM5 返回 32-bit，其余为 16-bit modulo count，另带软件观察
时间，不维护累计位置或速度。

每个实例只保留只读 descriptor 指针、cold receipt 和 initialized。当前 ARM ABI 下
可变 state 为 8 字节，只读 config 为 12 字节；face／共享 ops 与公共 pin lease 另计。
编码器不创建 IRQ、DMA、ring 或 worker，与相同 timer 的 PWM／capture 互斥。
取得前拒绝外来 clock／对应 vector／活动 controller；TIM1／8 还拒绝任何外来 E／NE
channel enable、DIER 与非零 BDTR，不接管 MOE／LOCK，也不使用或修改 RCR。
部分失败和关闭失败保留精确责任。
完整 close 证明 counter／inputs／DIER 已退出后再撤 clock，最终 GPIO 恢复仍由
Platform 的两份 pin lease 负责。APB／AF／gate 事实与 PWM 共用同一私有 C 头，
不增加运行时表、查找层或实例字段。

## DAC 输出与触发所有权

`dac.h` 暴露固定格式、物理通道授权、原始码写入、双通道暂存、start／trigger／stop
及数字状态；公共接口不泄露厂商 Handle。`drivers/dac.c` 使用已锁定官方 LL，
`private/stm32f407_dac.h` 保存只读配置和按实例定容的 CPU 状态，不预留 DMA buffer、
worker 或请求队列。生成器只声明静态对象、factory 和 prepare／close 调用。

PA4 对应通道 1，PA5 对应通道 2。单通道只取得对应引脚；双通道通过一次官方 dual
DHR 写入暂存两个值。公共 CPU 输入始终是未移位原始码；`u12-left` 在 C 中左移，
`u8` 的数字 DOR 回读为输入码乘 16。无论格式，状态均返回右对齐 12 位数字计数，
不测量电压。noise／triangle 的偏移加幅度不得超过 4095，noise 不提供随机熵。

软件触发不产生 DMA 请求。当前 CPU／wave 实现没有 DAC IRQ；TIM6／7 为独占
trigger owner，从所选 APB1 timer clock（默认 84 MHz）取得向下限速的整数分频。API 只准入
1 Hz--1 MHz 数字请求，状态返回实际分频；此政策不是实测模拟更新率或负载保证。
修改 PSC／ARR 时先禁用并观察 DAC TEN，再生成 UG，避免配置过程输出额外样本；
明确清除继承的 OPM／UDIS／URS／ARPE，并回读 divider／MMS。startup 延时从最后一个
输出 enable 之后计，结束前再次检查绝对 deadline。

stop 撤回触发与波形后安装所声明空闲码，完整 close 才关闭输出并撤回新增 gate。
拒绝已活动的 DAC、欠载中断源或 TIM6／7；不清除其他 owner 的 IRQ／状态。停止、
配置回读或 gate 撤回失败继续保留 cleanup receipt，可由同一 owner 重试。Platform
随后恢复选中引脚和 GPIO clock；完整释放不承诺持续驱动空闲电平。

`tests/contracts/dac_assembly/` 提供 VE 左对齐／ZG 8 位软件参考、真实 factory 与 ARM
链接消费者；它们不授权任何实际 PCB。当前没有 DAC 实板或模拟负载资格，物理 HIL
为 `not_executed`。已接入的 EXTI9 硬件事件必须来自显式 event owner；不接纳未知共享触发。

`trigger-dma` 与 `trigger-dma-circular` 通过独立 `dac_tx.h` 分面借用编码帧，
factory 的 `nx_factory_dac_tx_ref` 复用同一个 DAC ID 和 controller state；CPU 模式
返回 NULL，不增加 DMA 状态。单通道 8 位用 byte、12 位用 halfword；双通道 8 位
用 halfword、12 位用 word，通道 1 位于低 lane。左对齐码须由调用方预编码，保留位
为零。每次请求最多 65535 帧；缓冲区必须处于明确的 DMA 可读域，不能使用 CCM。

通道 1／双通道只使用 DMA1 stream5 channel7；仅通道 2 使用 stream6 channel7。
静态配置同时独占所选 TIM6／TIM7、DMA stream、其 IRQ 与 TIM6_DAC IRQ。显式
`irq` 指定优先级和是否调用 OS；可选 wake 绑定时核对两个实际发布源的 syscall
ceiling。IRQ 只锁存事实并发送提示；调用方推进 service 和处理绝对 deadline，
平台不创建 worker 或队列。

首个 trigger 输出声明的 idle，随后 DMA 填入第一个 frame。有限 TC 仅表示内存读取
完成；C 代码短暂停独占 timer、建立新的尾部周期，直到新 update 和最后 DOR 均被
观察后才结清。尾部可插入间隔或重复末码，不承诺恰好 N 次模拟转换或模拟 settling。
循环模式跨回卷持续借用，直到 cancel／timeout／故障完成排空；transferred 是最后
一轮已经消耗的字节前缀，不是总播放量。

EXTI9 支持 CPU／noise／triangle 和 `trigger-dma-circular`。外部速率返回 unknown，
不伪造 timer 频率；运行期间保留 TIM6／7 只供勘误恢复，不能把该 timer 同时启动
为输出时基。当前拒绝有限 EXTI9 DMA：RM0090 Rev22 §14.3.4／§14.3.6／§14.3.7
pp438–440 将 DHR→DOR、触发和 DMA 请求分开，DMA TC 不能证明最后 payload 已由
后续外部边沿输出；重复原始码的 DOR 相等也不能证明新边沿已经发生。当前替代为
循环会话加显式 cancel／stop，或使用独占 timer 的有限 DMA。若产品要求有限外部
尾部，应先提供可计数的触发／完成握手及其归属，再另行设计合同，不用延时猜测。

每次 DMA arm 还须处理 ES0182 Rev19 §2.6.1 的跨请求 pending 边界。请求已经
ACCEPTED 后，先在旧 DMA、IRQ 和地址均退出时安装声明的 idle，再由独占 TIM6／7
发出硬件触发并观察 UDR，随后撤回 DMAEN／输出／clock，重新配置 DAC、DMA 和
trigger。恢复可能在波形计时开始前转换 idle，并短暂关闭输出；这些转换不计入
payload 进度。两次请求之间不承诺持续电压保持、无毛刺或连续模拟波形。软件模型
与 ARM 链接不建立 silicon workaround 或模拟负载资格，详见[官方库与恢复边界](../../../docs/design/official-stm32-provider.md#f407-dac-dma-restart-boundary)。

只有显式受控恢复阶段诱发的内部 UDR 归属恢复，不计 payload，也不单独把已经
到期的 TIMEOUT 改成 IO。正常运行的真实欠载仍须显式 stop 恢复；W1C 清零失败或
真实 DMA 错误仍是 IO 并保留借用，不能将此归属规则扩大为忽略所有 UDR。

排空必须回读请求／触发／中断源已撤销、DMA EN 已清除、计数稳定、内存地址已清零、
NVIC 发布者已 join。失败保留 request、payload、clock 与 cleanup receipt，超过
排空预算进入 QUARANTINED。此时 next_service 返回 NEVER，调用方显式恢复并重试
service／stop；不得回收存储或忙等。欠载后须显式 stop 并观察 W1C 清零才能复用。

## CRC 与芯片身份

CRC 是有可变硬件状态的 controller，使用同一静态 typed factory 和 prepare／close
生命周期。四个 Board 包提供片内 `crc` binding；`[crc.signature]` 选择该 binding 和
`mode="crc32-words"` 即可，无需新增 GPIO 或生成行为代码。实例只保留 initialized
和 clock_added 两个标志；官方 LL 执行 reset、32 位 feed 和 read，C 负责时钟归属、
上下文、绝对 deadline 与输入／输出隔离。CRC_IDR 和原有共享时钟保持不变。

输入为明确的 32 位数值字，MSB 优先；初值为 0xffffffff，不反射、不做 final XOR。
空序列返回初值，字节流的拼装和尾部补齐由调用方决定。每 16 个字检查进度，最后
读取后再次检查 deadline；失败不发布部分签名，也不保留输入。输入／输出不得覆盖
provider state、CRC 或其时钟控制寄存器。`tests/contracts/crc_assembly/` 提供 VE／ZG
真实 factory 与 ARM／SDK 链接夹具，不能据此认定实板或吞吐资格。

芯片身份是片内永久只读数据，`nx_chip_info_read` 直接查询，不建立实例、factory
ID、时钟 lease 或缓存。UID、device ID、revision 复用官方 LL；Flash 容量按 RM0090
Rev22 第 39.2 节读取半字，避开锁定 LL helper 的非对齐 uint32_t lvalue。返回实际
容量，即使与装配期望不同也不补成配置值；启动准入仍由 System 的硅片策略负责。
device ID 不足以识别完整订货号／封装，UID 不作为认证秘密。Native 返回 UNSUPPORTED
并保持输出，不生成物理身份；CPU-only Runtime 没有 SoC 后端。

## LTDC 与 DMA2D

F479 软件参考使用 `display/LTDC/rgb-scanout` 与 `image/DMA2D/irq-blit`。
LTDC 采用已审 AF14 RGB565／RGB888 接线，内存格式独立选择 RGB565／ARGB8888；
DMA2D 不占外部引脚或 DMA1／2 stream。System 是 PLLSAI 的唯一 owner，LTDC
只消费 R 输出；Platform 保存一次原始 pad snapshot。两个 C owner 分别取得本外设
时钟门和 IRQ，退出后才由 Platform 恢复原 pad。

一个 display face 管理两个硬件 layer，以及共享 shadow bank 上最多一个 pending。
可见不表示借用结束；实际 VBlank／active 配置退休证据由 IRQ 按本次 generation
采集，Task 再校验并排空引用后发布 SETTLED，不要求 Task 高频命中短同步脉冲。
单 layer 取消不误停另一层，controller stop 必须全部 join。DMA2D 使用官方 HAL
进行 fill／copy／convert／blend，完整跨度、stride、alpha、别名及错误退出均明确；
没有 GUI、默认 framebuffer、CPU 像素 fallback 或后台任务。

详细时钟、资源、失败保留和模式边界见
[官方 F4 transport 与 graphics 合同](../../../docs/design/official-stm32-provider.md#f479-ltdc-scanout-and-dma2d-memory-operations)。
软件参考不等于实际屏幕、PCB、FMC DMA 可达性、总线带宽或 HIL 资格。

## 冷态取得、逆序撤回与验证

构造是无其他 controller writer 的冷态 task 操作，明确安装 GPIO 电平和已审 AF。
`platform_cold.c`、`uart_cold.c` 和模式专属 cold 头以 C 实现准入、取得、回滚、停止
和回读。被拒 owner 的空 receipt 不能停止或擦除其他 live provider；部分效果保留
receipt，直到同一 close 证明退出。这不建立实测 IRQ、吞吐或物理 Board 资格。

高级构造仅在启动栈保存受影响 pin field／clock bit，后续取得失败先恢复这些字段，
再由外层回滚早先接口。局部快照与 Platform 持久 lease 分开：每个选中 pin 一个
`uint16_t`，另有有效性与 gate 责任。所有 pin mode／lock 在第一次修改前检查，
持有存储按实际装配定容，对齐由所选 ELF 计入。

已有 enabled channel／EXTI line 在改引脚前拒绝。停止逆序排空，先建立 driver
CS／PWM inactive；完整 Platform release 再恢复选中原 pin field／latch，观察恢复
后才撤新增 gate。失败保留精确未完成 lease，不重复恢复已释放 pin，因此完整释放
不承诺持续驱动产品安全电平。最后一个 EXTI vector owner 才关闭 vector；已启 IWDG
是阻止干净关闭的不可逆效果。

`tests/contracts/stm32_model` 用官方寄存器结构和外部事件模型编译真实生产 C，覆盖
时钟回滚、定时器回绕、GPIO 位掩码、UART TC／取消／迟到 IRQ／RX 丢失、SPI 复位／CS、
I2C 接收／重复 START 自动清除／NACK／仲裁／STOP／恢复、Flash 边界与编程脉冲、
IWDG 不可逆失败、EXTI 隔离及 PWM／ADC 错误。
构造模型运行与固件相同的生成 C，在前一 pin 已改后注入 GPIO clock readback 失败，
并检查 shared IRQ release 和 timer 冲突。两种精确密度的独立软件夹具链接全部维护
controller，外部连接器不据此获得 Board 资格。模型、ARM ELF 和资源证据分别记录；
实板断开时，clock／IRQ／电气波形／Flash 掉电／长期负载都未执行。

## 默认平台协调与可选资源切片

默认装配通过 Core 私有 `platform_lifecycle_impl.h` 协调整个平台，F4 私有
`stm32f4_platform_lifecycle_impl.h` 提供 system start、resources acquire、resources
release 和 system release 的手写 C 实现。生成器只写只读配置与固定调用，使编译器
在同一编译单元看到已选配置，裁剪恒定校验、未选 owner 和无诊断路径的元数据操作。
`platform_coordinator.c` 的四个通用入口复用同一实现并保留运行期检查，产品不包含
私有头。硬件算法、顺序、错误映射和撤回控制流只维护在 C 中。

System 路径依次处理基础／所选资源准入、芯片身份、pins 取得与 system start；
失败按实际责任合并 pin abort 与 System 结果。共享取得记录 Ethernet 责任后建立
DMA／SYSCFG；释放先检查永久 startup SRAM，再处理 CAN、Ethernet、pins 和共享
clock，全部完成后才释放 System。可选 owner 用固定 typed hook 连接，未选项不引入
相应链接依赖；没有 opcode、动作解释器或第二份可变收条。

`private/stm32f4_lifecycle.h` 与 `drivers/lifecycle_{pins,gpio,uart,spi}.c` 为
可选 `nx_lifecycle_*` 资源事务引擎提供独立集成切片：外部 System owner 先建立时钟，共享 pins
先取得；目前只覆盖 GPIO input／output、8N1 IRQ UART byte／event 和带静态 CS
的 SPI short-poll。只读配置／ops 与可变资源收条分开，每步只有一份真实资源收条。

该可选切片不替代默认整平台协调器。默认协调器连接原有各维护模式的 C owner，
不要求把 UART DMA／word／block、SPI DMA 等模式改成该切片的 ops。未维护模式仍
明确拒绝。具体模型与实际生成 factory、ARM 镜像分别验证；源码存在不建立全模式
通过、软件候选或物理 HIL。
