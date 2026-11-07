# STM32F4 系列支持包

本包按系列复用官方 STM32F4 HAL／LL 与 CMSIS Device。当前行为实现限定
**STM32F407VET6**（LQFP100、512 KiB Flash）和 **STM32F407ZGT6**
（LQFP144、1 MiB Flash）的固定类型 C 接口。公共接口在 `io/`，厂商类型与可变状态
留在本包私有目录；Board 只授权已审接线，产品任务、分区和恢复策略由外部工程提供。

## 源码与状态归属

| 入口 | 责任 |
| --- | --- |
| `soc.json`、`routes.json`、`references.json` | 已维护精确型号、路线与资料来源 |
| `devices.json` | 官方器件宏、启动文件、IRQ 与时基存在性的编译事实 |
| 根目录 `tools/configure/stm32f4/` | 当前集中配置适配、静态存储与固定连接 |
| `system.c` | 启动、固定时钟、TIM2 时间域与失败退出 |
| `drivers/`、`private/` | 具体模式、实例状态、硬件生命周期与独立回读 |
| `CMakeLists.txt` | 所选源码、官方库与私有编译依赖 |

![FIG-31：共享 IP 与精确 SoC 的源码责任](../../../../docs/assets/diagrams/driver-source-ownership.svg)

图表示构建期源码归属，不增加运行时转发层。公共 face 共享只读方法表，各实例保有
独立 context；factory 只返回已有身份，不初始化硬件。

`devices.json` 的 SDK 编译覆盖不代表其他 F4 型号已具备资源准入、驱动、
Board 或运行时模式；当前具体支持以 F407VE／ZG 配置和执行范围为准。
Board／SoC 中没有 Python 插件，集中主机工具与后续 Host C11、手写 C 生命周期
收敛分别说明，不把目录移动标为语言或行为重构完成。

## 内存、启动与时钟

主 SRAM 为 128 KiB；独立 64 KiB CPU-only CCM 通过 `[memory.regions.ccm]` 显式选择。
`NX_MEMORY_CCM_DATA`、`NX_MEMORY_CCM_BSS`、`NX_MEMORY_CCM_NOINIT` 使用同源生成的
linker／startup 计划，`NEXUS_MEMORY_CCM` 表达所选域。默认装配不增加 CCM 初始化
存储或放置；所有 DMA 域均排除 CCM，不预留产品 Flash 分区。

主 RAM 初始化前先启用并观察所选 CCM clock，再在第一次写入前验证完整初始化计划。
该路径仅依赖 Flash 常量和冷态本地栈，不使用 libc、global 或 heap。`.noinit` 只表示
不复制／清零，不保证复位或断电保留。

`system.c` 建立已审 8 MHz HSE／168 MHz PLL、APB1 42 MHz、APB2 84 MHz。保留 TIM2
以 1 MHz 运行，通过 overflow IRQ 扩展 32-bit counter，IRQ 至少每 2^32 微秒服务
一次，不占 RTOS SysTick。USB FS 另行显式取得独立 48 MHz clock owner，基本启动
不会开启它。初始化分别记录 primary、cleanup、remaining effects；HSI 切换失败
保留责任，不误关活动 PLL。单调时钟尚不存在的 bootstrap 阶段使用明确 poll 界限。
不支持动态变频。

## 官方 HAL／LL 复用与 FPU 约束

官方 ST Device 与 Arm CMSIS 子模块提供寄存器布局、mask、完整 GCC vector／startup。
STM32F4 HAL Driver 为私有状态机提供所选 LL 操作，并仅在 UART `dma-tx` 模式提供
HAL 有限 TX。审核版本、精确 Git、CMSIS 配对、文件摘要和许可证由
[stm32f4-hal.lock.json](../../../../dependencies/stm32f4-hal.lock.json)固定。

厂商头只进入实现与生成 binding。只有 `dma-tx` 选入私有 UART/DMA Handle 与官方
UART／DMA／RCC C 单元；其他 UART 保留各自 provider。不调用全局 HAL 初始化，不启动
HAL SysTick，不宣称全 HAL 支持。[官方驱动合同](../../../../docs/design/official-stm32-provider.md)
定义完整模式的所有权、时间、取消、成本比较和交付要求。

`SystemInit` 开启 CP10/CP11，此 hard-float 平台不关闭 lazy FP context。FreeRTOS
CM4F port 显式开启 ASPEN/LSPEN。因此 ES0182 Rev19 §2.1.2（Arm776924）对所列全部
F407 cut 都是集成约束：当受影响 VDIV/VSQRT 条件可能出现时，中断 handler 在异常
返回之外必须执行超过两条指令。须沿生成尾分支进入真实 callee，不能计算不可达填充
或 literal。外部 handler 同样遵守，修改编译器／优化／handler 后重新检查实际 image。
平台不会绕过内核 ABI 修改 FPCCR，详见
[短 ISR 合同](../../../../docs/design/official-stm32-provider.md#cortex-m4f-short-isr-constraint)。

## 已维护模式

| 接口 | 执行与限制 |
| --- | --- |
| GPIO | 单 port 的 16-bit 授权 mask；原子 BSRR set/reset；串行 toggle；先初值后 mode |
| UART | USART1 IRQ bytes/events、IRQ RX blocks、有限 TX DMA、RX DMA/IDLE、有限／连续 duplex 和连续 RX blocks；USART2 有限 TX DMA 与 IRQ bytes/events；USART3/UART4/UART5/USART6 已审 IRQ bytes/events；真实 TC、显式 loss、drain、quarantine |
| SPI | SPI1 短 polling／有限全双工 DMA，SPI2 有限 DMA；8-bit MSB-first master、mode0–3、完整 CS interval；polling 最多 256 bytes，DMA 最多 65535 bytes |
| I2C | I2C1 polling/IRQ/DMA、I2C2 IRQ/DMA；42 MHz／100 kHz；7-bit 地址、最多 8 条 read/write message，各 1–256 bytes；repeated START、末字节 ACK、NACK／仲裁／STOP 错误 |
| Flash | VE 8-sector／ZG 12-sector 精确几何；声明 2.7–3.6 V 的 x32 program；范围／对齐、完整 pulse 和 cache invalidation；C deadline 不能抢占 erase |
| IWDG | 4–256 预分频、12-bit reload、名义 32 kHz；观察 option 已启用和不可逆软件 enable，debug freeze 显式；当前物理 timeout bounds 为 0/0 unknown |
| EXTI | 静态 line、有界共享 vector 分派、独立事件存储和 overflow；pending 可合并边沿，无 debounce 策略 |
| PWM | TIM3 通道、固定共享 PSC/ARR、period1–65535、duty0–period、shadow update；stop 建立 GPIO inactive |
| ADC | ADC1 12-bit 软件 shot/scan、TIM3 触发有限／连续 DMA block；显式 sample time/reference、完整 scan 和 consumer loan 背压 |
| CAN | CAN1/CAN2 Classic CAN IRQ、三邮箱、共享 filter domain；无 CAN FD |
| USB | OTG FS device PIO endpoint，独立 USB48 owner；无 Host/HS/DMA/class stack |
| Ethernet | Raw RMII frame/MDIO、调用者 descriptor／RX loan；IP／协议策略外置 |
| Timer capture | 已审 TIM4 edge-capture 路线和有界事件 |
| Advanced PWM | 已审 TIM1 互补输出／deadtime／break，功率级电气安全外部验收 |

IWDG 数据手册 17–47 kHz 是 VDD=3 V、TA=-40..105 °C 条件下的表征；当前没有
声明满足条件的 operating profile，不能直接据此保证时限。名义配置可成功，但启用后
物理界限查询仍可 `UNSUPPORTED`，同时保留 enabled／irreversible／debug 事实。

独立核心板和 V4.1 包各自保留原理图与跳线身份，不替换旧 V3.1。内部 Flash／IWDG
可以显式选择，但不会预留存储或在 startup 启狗。SoC AF 路线不证明 Board 接线、
外部上拉、器件实装或物理支持，用户必须提供已审绑定。I2C GPIO bus-clear 为独立
通用组件；内部 ADC channel 和未声明模式仍拒绝或不在 schema 内。

已选 erase-aligned offset image 有明确 startup／vector／linker／ELF/BIN 检查，
不包含 boot 认证、激活或运行镜像 handoff 资格。精确 mode／Board／生产源码见
[派生矩阵](../../../../docs/delivery/capabilities.md)。

## UART、DMA 与执行进度

额外 byte/event 路线为 USART3 PB10/PB11 AF7、UART4 PA0/PA1 AF8、UART5 PC12/PD2
AF8、USART6 PC6/PC7 AF8。USART2/3、UART4/5 从 APB1 取得 42 MHz，USART1/6 从
APB2 取得 84 MHz。冷准入在取得资源前检查精确引脚、两个 GPIO bank、clock、gate 和
IRQ。小型只读 cold descriptor 表不在字节热路径使用，不增加可变实例状态。
DMA 仍只开放 USART1 和所选 USART2 有限 TX；新增 IRQ 路线不授予其他 DMA 模式。

`dma-tx` 的一个 owner 组合官方 HAL 有限 TX 与分离 LL IRQ-byte/event RX，所选
单元 callback ABI 都为 1，实例共享只读 ops。RX 不交 HAL，TX abort 不改变 RX
责任。短 CPU guard 延后抢占的所选 vector，不在 HAL 调用外套长全局 IRQ mask。
HAL DMA handler 每次进入都要求 EN 清除；HT-only 使用有界官方 acknowledgement。
错误撤回被拒时先 mask 此 vector，再交 task 推进 HAL，避免锁定 IRQ poll。
排空／deinit 失败保留 request、pad、gate 和清理责任。私有 bounded atomic 与
nearest-BRR 编译配置依赖受审单 CPU owner，不修改厂商源码。`HAL_GetTick` 使用既有
TIM2 epoch，`SystemCoreClock` 在失败清理中追踪确认的 HCLK，不启动 HAL 或 SysTick。

有限 UART／SPI DMA 只借用已审 DMA 可达域中的调用者 buffer，排除 CCM。DMA TC
不能释放借用，还须 UART TC／SPI idle、memory channel disabled 和 IRQ detached。
排空失败保留 QUARANTINED。`irq-blocks` 是逐字节 IRQ＋IDLE，不是 DMA RX；DMA RX
另选有限或连续双缓冲 provider。拒绝 `dma-tx`＋IRQ blocks；双向显式选 `dma-duplex`
或 `dma-duplex-continuous`。连续模式不覆盖 READY/BORROWED，缺 replacement 时停止
并报告 loss，不虚构错过事件的准确次数。

有限 ADC `trigger-dma` 在发布完整块前停 trigger／conversion／DMA，稳定后由显式
service rearm，块间可有间隙。消费者借用不能被覆盖，未释放时 stop 保持 BUSY；
TIM3 与 PWM 独占冲突。独立 `trigger-dma-continuous` 维护双缓冲和 replacement，
进度不足时停止而非覆盖消费者。

每个 port 只有一个执行者，不引入 heap、registry、字符串查找、默认 worker、OS
lock、通用设备引用或预留 copy pool。UART IRQ 搬运字节和事实，task service 观察
传输 deadline，并在 cancel／stop 期间持续运行。晚到 TC 可以结清隔离传输，但在此
之前 request 保持借用。SETTLED 后无 IRQ／provider 引用，adapter／observer 还需
独立排空。

I2C recover 同时检查 BUSY 与已保留 SCL/SDA GPIO 实时输入；即使 BUSY 清除，只要
线路低就返回 IO。局部 reset 不打 GPIO 恢复脉冲、不重放事务、不保证未来总线可用；
缺线路事实时在寄存器写入前拒绝。

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

## 手写 C 生命周期集成切片

`private/stm32f4_lifecycle.h` 与 `drivers/lifecycle_{pins,gpio,uart,spi}.c` 为
Core 生命周期引擎提供独立集成切片：外部 System owner 先建立时钟，共享 pins
先取得；目前只覆盖 GPIO input／output、8N1 IRQ UART byte／event 和带静态 CS
的 SPI short-poll。只读配置／ops 与可变资源收条分开，每步只有一份真实资源收条。

这些 C 路径已由实际模型回归，尚未替换默认 Python 生成装配；Host C11 工具也是
独立切片。UART DMA／word／block、SPI DMA 和其余 I/O 仍保留当前路径。源码存在
不等于所有模式已经切换，也不建立候选或物理 HIL。
