# 精确支持范围

[派生能力矩阵](capabilities.md) 自动核对 SoC/route 的 mode、参考 Board binding
与实际 resolver/emitter 选择的生产源码。叙述表说明运行合同，派生表说明精确
身份与有限源码一致性；两者均不继承旧 HEAD 的资格或替代实板执行。
本轮有限芯片接入与平台反馈流程见
[主流 MCU 接入](../design/mainstream-mcu-adaptation.md)；同系列其他型号、封装和模式
不能自动继承本页范围。

## 芯片、板卡与系统

| 目标 | 软件实现 | 未完成的物理事实 |
|---|---|---|
| STM32F407ZGT6 / 启明欣欣 V3.1 | Cortex-M4F；1 MiB Flash；主 SRAM 128 KiB、CCM 64 KiB；声明 clock/profile 与 reviewed routes | 实际 PCB 修订、晶振、供电、连续性、波形与负载 |
| STM32F407VET6 / 天空星青春版 | Cortex-M4F；512 KiB Flash；主 SRAM 128 KiB、CCM 64 KiB；独立 Board 资源 | 相同类别的实际 PCB／电气／HIL 验收 |
| GD32F470ZGT6 / 梁山派 | Cortex-M4F；1 MiB Flash；主 SRAM 192 KiB、ADDSRAM 256 KiB、TCM 64 KiB；25 MHz 声明输入、200 MHz profile；可选 USB 独立 48 MHz owner | 未观察 PCB/晶振/电气；默认 linker 只用主 SRAM；实际 USB clock/PHY 仍未测量 |
| STM32G474RET6 / MB1367 来源参考配置 | Cortex-M4F；512 KiB Flash；96 KiB SRAM 加 32 KiB CCM 的连续 CPU alias；HSE24/170 MHz、独立 TIM2；封装有效 GPIO、六个 UART 的 IRQ bytes、SPI1/2 有限轮询、TIM3/4 CH1 PWM、IWDG、PA0/PA1 EXTI；静态 factory，裸机／FreeRTOS 夹具 | 52 个封装 GPIO 不等于六个完整 bank 或 PCB 接线；PF0/PF1 为晶振保留，PG10/NRST 保留；实际 PCB／晶振／焊桥／电气／HIL 未观察；无 DMA |
| STM32H743ZIT6 / NUCLEO-H743ZI2 来源参考配置 | Cortex-M7F；2 MiB Flash；默认 AXI SRAM 512 KiB；pre-RAM LDO、固定 HSI32／HSI64 × Cache off/on、TIM2；PB0 GPIO、USART3 PD8/PD9 IRQ bytes、有限 TX DMA 或有限 RX DMA／IDLE blocks；裸机／FreeRTOS 静态装配 | PCB／供电／硅片修订／焊桥／Cache 一致性未观察；不含连续／双缓冲 RX、其他 DMA 引擎、其他 RAM 放置或运行状态接管；最终软件资格独立记录 |
| STM32U575ZIT6Q / NUCLEO-U575ZI-Q 单状态参考配置 | Cortex-M33F；2 MiB Flash；默认 SRAM1 192 KiB；外部预先确认 TZEN=0／SWAP_BANK=0，MSIS4 MHz、TIM2；封装有效 GPIOA..H、USART1 PA9/PA10 与 USART2 PA2/PA3 IRQ bytes、IWDG；裸机／FreeRTOS 静态装配 | PCB／SMPS／硅片修订／option bytes／VDDIO2 未测量；Cache 关闭；此 Platform 不开放 DMA、SPI、I2C、ADC、PWM、Flash 编程、双域 factory 或其他 RAM 放置 |
| STM32U575 独立 TZEN=1 双域参考 | 外部预先配置 option bytes；两个 Runtime 镜像、冷启动 SAU／MPU／SRAM 归属 owner、真实 CMSE SG；单一非特权 NonSecure Thread 通过同步复制消息访问受限 PC7 GPIO | 未执行实板隔离验收；无 OS、通用双域 factory、NS 任务身份、异步借用／跨域 DMA 或产品镜像认证；不改变上列 TZEN=0 默认装配 |
| STM32F103RBT6 / NUCLEO-F103RB 来源参考配置 | Cortex-M3，无 FPU；LQFP64、128 KiB Flash、20 KiB SRAM；HSI8、独立 16-bit TIM2；GPIOA PA5、USART2 PA2/PA3 IRQ bytes、SPI1 PA5/PA6/PA7 与 PA4 CS、PA0/PA1 EXTI、IWDG；裸机／FreeRTOS 静态装配 | PCB／ST-LINK VCP 焊桥／引脚负载／HSI 精度未观察；PA5 LED 与 SPI 时钟冲突；无 DMA、I2C、ADC、PWM、Flash 编程或其他 F1 型号支持 |
| GD32F303RCT6 / 芯片软件参考 | Cortex-M4F；LQFP64、256 KiB Flash、48 KiB SRAM；IRC8M、独立 16-bit TIMER1；GPIOA PA4/PA5、USART0 PA9/PA10 IRQ bytes、SPI0 PA5/PA6/PA7 与 PA4 CS、FWDGT；裸机／FreeRTOS 静态装配 | 没有已审查 PCB、LED、外部晶振、串口适配器或 connector 接线；无 DMA、I2C、ADC、PWM、EXTI、Flash 编程或其他 F30x 型号支持 |
| Native | Linux/POSIX 行为模型、实际线程与持久化文件故障模型 | 不构成 MCU 或电气资格 |

裸机和锁定 FreeRTOS 均为显式选择的薄适配。启动只建立平台时钟／接口，不创建
scheduler、产品任务或工作流。FreeRTOS POSIX host 运行验证软件生命周期，不能
替代对应 MCU 的 IRQ ceiling、FPU context 与 stack high-water 实板验证。

H743 使用官方 Cortex-M7 启动与有界 LDO 电源入口，在 ACTVOSRDY 前禁止 RAM
写入；从 HSI64、Cache off 的可信冷状态建立所选固定时钟与 Cache 配置。Cache
关闭位本身不能证明外来 dirty 数据已经清理；要求可信复位或此前成功完成自有
clean/disable。U575 默认 Platform 使用官方 Cortex-M33 启动并维护 TZEN=0
单地址状态；独立 TZEN=1 参考有自己的入口和安全归属 owner。启动后读取 option
register 不能使不匹配的启动变成合法状态。两者都不修改 option bytes，不自动
接管 bootloader、DMA 或既有 clock/IRQ owner。原始 CMSIS Device H7/U5 源码、
许可和版本固定在 source lock。
精确入口见 [H743](../../soc/stm32h743/README.md) 与
[U575](../../soc/stm32u575/README.md)。

ThreadX 是额外显式后端，锁定官方 Eclipse ThreadX 6.5.2.202603 release。当前
配置维护三块 F407/F470 Board 的 Cortex-M4 GNU port，以及 Native Linux GNU。
独立 CPU runtime 维护 M4 integer、M4F hard/softfp，BASEPRI 或 PRIMASK 政策；
其他 Cortex-M ThreadX port 明确拒绝。生产 graph、真实 Linux 内核 task/queue/join
与三板 ARM 链接分别验证，Linux 自身 pthread 栈与内部 host 分配不是 MCU 成本。
ThreadX 不实现本项目 FreeRTOS 的 MPU、split-security 或 tickless 模式。详见
[ThreadX 接入](../design/threadx.md)。平台启动不进入任何内核，产品显式进入内核；
仍运行的 scheduler 不允许普通 I/O shutdown 关闭其 clock/Tick。

CPU 软件层另支持 M0／M0+／M3／M4／M7／M23／M33／M55／M85，统一严格事实
解析器选择 Core/Arch、原子操作和裸机/FreeRTOS runtime。M0/M0+ RMW 使用
bounded saved-PRIMASK；可选 cache/MPU/SAU 有真实 CPU 算法，FPU/MVE 与安全
状态决定精确 kernel port 和 ABI。CPU-only runtime 自身不生成 SoC/Board/factory；
上述 H743/U575 纵切片分别拥有独立装配与验收。编译、寄存器模型与真实
FP/MVE/cache/MPU/安全世界验收分别记录；
`cache.h` 的范围维护与仅 M7 维护的冷 Cache 启停分别提供；CMSE 范围权限观察
本身不建立用户内存 pin、复制或 transport。
可选 restricted MPU task 和 split Secure/NonSecure runtime 已有独立软件实现；
窄 delay/tick SVC、Secure context lease 与调用者静态存储分别验证。默认仍为
privileged single-world；MPU+split 明确拒绝，未新增实板 HIL。范围和配置例子见
[Cortex-M 软件支持](cortex-m.md) 与 [Arch 契约](../design/arch-contracts.md)。

## 首发模式

以下完整外设表只对应 F407/F470 首发实现。F103、GD303、G474、H743、U575
仅维护上表与各 SoC 合同明确列出的切片，不继承这张完整 I/O 表。U575 双域同步
GPIO 参考单独维护；不从芯片型号或通用接口推断其他外设、DMA、ThreadX 或低功耗模式。

| 接口 | 已实现模式与限制 |
|---|---|
| GPIO | authorized mask、原子 set/reset、快照 read、single-writer toggle |
| UART | 8N1 IRQ TX 或有限 TX DMA；caller-owned request、真实 TC；IRQ RX blocks／IDLE、有限及连续 RX DMA、双向 DMA；显式 RTS/CTS 与外部静态 RS485 适配；STM baud 最低 1282、GD 最低 1526，最高 1 Mbaud |
| SPI | 8-bit MSB-first；短轮询每笔最多 256 bytes；有限 DMA 最多 65535 bytes；双向递增或调用者单字节固定 dummy/discard；单 wire-active、完整 CS interval；独立 CS endpoint |
| I2C | 100 kHz；最多 8 条消息、每条 read/write 1–256 bytes；轮询、中断及 DMA 消息；1/2/末三字节 ACK、repeated START/NACK/仲裁/STOP；恢复检查 BUSY 与所选 SCL/SDA 电平；独立有界 GPIO bus-clear 工具 |
| Flash | STM 精确 16/64/128 KiB sector 几何与 x32 program（4-byte alignment/length，声明供电 2.7–3.6 V）；GD 官方独立 4 KiB page erase 与 halfword program；全物理空间、外部受限 region |
| Watchdog | 独立 enable/feed/reset cause；名义配置成功返回 SUCCESS，启用不可逆，启用后的失败保留责任；F407 和 GD470 当前物理 timeout bounds 均为 `0/0 unknown`，已启用 state 查询返回 UNSUPPORTED 并保留 enabled／irreversible／实际 debug 事实；失败的启用前 LSI／debug 清理可经平台 stop 重试 |
| EXTI | selected line、共享 vector、caller-sized event queue、丢失／coalescing 可见；无 debounce 政策 |
| PWM | 固定 timer/channel/base、CCR shadow、0/100%、safe inactive；公共 stop 可以 restart，平台 teardown 释放 owner |
| ADC | reviewed channel、single-shot/低速 scan、有限及连续双缓冲定时触发 DMA blocks；显式 sample time/reference；只发布完整扫描；可选外部序列与软件观测时间记录 |
| Classic CAN | 11/29-bit DATA/RTR、0–8 bytes、真实三邮箱、共享 filter domain、显式 abort/bus-off recovery；没有 CAN FD 或自动重发 |
| USB FS device | internal PHY PIO、EP0 control IN/OUT64、EP1–3 bulk8/16/32/64、packet loan、SETUP epoch、reset/stop drain、独立 48 MHz owner；没有 Host/HS/DMA/ISO/class stack |

F407 名义周期使用 `(RLR + 1) × divisor / 32000`，GD470 使用
`(RLD × divisor + 1) / 32000`，不能套用同一公式。F407 数据手册的 17..47 kHz
是 VDD=3 V、TA=-40..105 °C 的表征条件，当前没有声明满足条件的 operating profile；
GD470 数据手册只给 IRC32K 典型频率，没有已审查的保证 Min/Max。因此两者当前均报告
未知物理边界。SUCCESS 表示名义寄存器配置完成，不证明物理超时；启用后的
UNSUPPORTED 查询也不表示计数器已关闭。需要保证时限的产品必须在不可逆启用前
拒绝不满足其时限合同的 profile。

USB 配置/factory 已在两 STM 封装与 GD470 的软件 Board 夹具执行真实 ARM 链接，
默认参考 Board 不自动暴露 USB connector/VBUS 电气。`vbus_sensing=true` 缺少
reviewed 电气时拒绝。PA11/PA12 AF10 的硅片路线不证明实际跳帽、供电、VBUS 或
ESD；它与 CAN1/USART1 flow-control 的资源冲突必须拒绝。详见
[USB packet 合同](../design/usb-device.md)。

以下纵切片拥有独立生产机制和开发证据，最终配置、SDK 与完整镜像接入分别验收。
不能将其中一项的模型通过记为整个平台或参考 PCB 通过。

| 可选机制 | 精确范围与当前边界 |
|---|---|
| Raw Ethernet / MDIO / DP83848 | Native/ST/GD normal chained descriptor、RMII、14–1514-byte untagged Ethernet II、caller-owned RX loan；ETH/MDIO face 共用一个 MAC context；ST ZG/GD ZG 软件夹具已有 factory 与裸机/FreeRTOS ELF；无协议栈、PTP、checksum offload、VLAN、jumbo |
| Input capture | STM TIM4 CH1 / GD TIMER3 CH0，PB6 AF2、16-bit modulo count、caller event ring；软件观测时间不是 edge aperture；装配接入独立验收 |
| Complementary PWM / break | STM TIM1 / GD TIMER0，PA8/PB13/PB12 AF1；不可变精确 deadtime 与 BKIN，禁止自动重启；没有功率级安全资格 |
| User I/O broker | 特权 kernel 调用的 scalar GPIO read/set-reset、真实 principal/pin/copy/unpin 窄端口；U575 独立无 OS 参考接入同步 CMSE gateway；无通用 SVC、多任务 userspace transport 或异步用户 DMA |

参考 Board 默认不声明 Ethernet PHY/REFCLK/reset 接线。启明 ZG 可选 RMII 来源
涉及 PG11/13/14，因此不授权 VE100 封装；天空星和梁山派无 reviewed PHY 路线。
协议、电缆、PHY power/reset、产品 MAC 地址和恢复策略留在外部。详见
[Ethernet](../design/ethernet.md)、[高级 timer](../design/timer-advanced.md)、
[用户 I/O broker](../design/user-io-broker.md) 与 [G474](../design/stm32g474.md)。

G474 的封装 GPIO、六个 UART、SPI1/2、IWDG 与 PA0/PA1 EXTI 分别具有明确的
生产实现和生成夹具。LPUART1、USART1/2/3、UART4/5 只维护 IRQ bytes；SPI 为
8-bit、mode 0..3、显式 active-low CS，每笔最多 256 bytes，当前 170 MHz profile
接纳 requested SCK 664063 Hz..42.5 MHz；无 DMA、RX event/block 或任意 AF 自动选择。
实际接线仍须 Board 授权；源码、预算、方法保留和向量检查不证明实物的 MCU、X3
晶振、VCP／LD2 焊桥或 SPI 电气。
[初次 SoC 执行记录](soc-refinement-verification.md) 保留其历史 code revision；
[扩展执行记录](soc-extensions-verification.md) 单独绑定后续扩展的源码和证据。
离线复现与候选封存仍有独立
退出条件，不能继承此前 F407/F470 候选的资格。

G474 的 `fixed-pwm` 仅维护 TIM3 CH1／PA6 AF2 与 TIM4 CH1／PB6 AF2，分别独占整个
controller。`tick_hz` 必须与 170 MHz 时钟形成 1..65536 的整数分频；固定 period
为 1..65535 ticks，duty 为 0..period，active-high、edge-aligned，显式处理 0/100%。
stop 先观察 CCER/CEN 关闭，再建立 Board 声明的 GPIO inactive；无法证明停止时保留
fault 与 clock lease，交给私有 close 重试。PA6 与 SPI1 MISO 冲突；不提供共享 timer
base、互补输出、capture、break、PWM IRQ 或 DMA。模型、factory 和 ARM 夹具不能
证明真实输出频率、负载或电气安全。

H743 USART3 的 `dma-tx` 使用 DMA1 Stream0、DMAMUX1 channel0/request46，RX
保持 IRQ byte ring；`rx-dma-idle` 使用 DMA1 Stream1、DMAMUX1 channel1/request45，
TX 保持 IRQ。二者是显式替代模式，不自动组合成 DMA duplex。仅接纳所选 AXI SRAM；
Cache off 每块／每笔为 1..65535 bytes，Cache on 地址和长度必须按 32 bytes 对齐、
长度最多 65504 bytes，调用者独占完整 cache line。TX 在 clean 后启动；RX 在 DMA
前及排空后 invalidate，稳定剩余计数和维护完成后才发布完整块或 IDLE prefix。
引用、通道、DMAR 或 cache 维护未排空都保留借用；消费者持有块时保留 stream 与
平台依赖。owner service 发布并重新接纳下一块，可能出现间隙；不提供 circular、
continuous、双缓冲 RX、其他 DMA engine、其他 RAM 放置或运行中变频。Cache
配置在平台生命周期内固定，全部 provider 排空后才能关闭时基及 Cache。

U575 仅开放所选封装 GPIO 与 USART1/2 IRQ bytes、IWDG；GPIOH 只接纳 PH0/PH1/PH3。
PG2..PG15 还要求外部电源 owner 已建立 VDDIO2、PWR clock 和 IO2SV；平台不打开
未知外部电源，也不把 IO2SV 当作电压测量。独立 TZEN=1 参考的分区、固定 principal
和同步借用前提见 [TrustZone 合同](../design/u575-trustzone-contract.md)，不能扩大
TZEN=0 factory 或授予异步跨域 DMA。

F103 与 GD303 的 1 MHz timebase 都基于 16-bit timer，overflow IRQ 的服务和
中断屏蔽间隔必须小于名义 65.536 ms；单一 pending wrap 不能补回多次未观察溢出。
两者的 SPI 均为 8-bit、mode 0..3、有界轮询，每笔最多 256 bytes，31.25 kHz..4 MHz；
F103 使用 SPI1，GD303 使用 SPI0。F103 的 PA5 同时连接 LED 与 SPI SCK，不能冲突
选择。具体接入与失败停止合同见 [F103](../../soc/stm32f103/README.md) 和
[GD303](../../soc/gd32f303/README.md)；没有把这些切片提升为全系列或全外设支持。

Ethernet 生成夹具保留精确 TX/RX descriptors 与 RX buffers，只接纳 STM 主 SRAM
128 KiB 或 GD 主 SRAM192 KiB。四个 ZG ARM graph 的开发预算/retention 通过不
授权 GD ADDSRAM/TCM、STM CCM、VE100 的 PG route 或某个产品 PHY。独立 cold
review、clean-source SDK 与实板资格仍各自建立，参考默认镜像不选入 ETH/MDIO。

有限 DMA 的精确首发路线如下，其他 controller／通道组合由 resolver 拒绝。

| 模式 | STM32F407 | GD32F470 |
|---|---|---|
| UART TX DMA | USART1；DMA2 stream7 channel4 | USART0；DMA1 channel7 selector4 |
| UART RX DMA／双向 DMA | USART1；RX DMA2 stream2 channel4；TX 同上 | USART0；RX DMA1 channel2 selector4；TX 同上 |
| SPI 全双工 DMA | SPI1；DMA2 RXstream0／TXstream3 channel3 | SPI4；DMA1 RXchannel3／TXchannel4 selector2 |
| I2C 消息 DMA | I2C1；DMA1 RXstream0／TXstream6 channel1 | I2C0；DMA0 RXchannel0／TXchannel6 selector1 |
| ADC 有限／连续 trigger blocks | ADC1；TIM3 TRGO；DMA2 stream4 channel0 | ADC0；TIMER2 TRGO；DMA1 channel0 selector0 |

已审查的软件 Board 配置支持 STM32F407 I2C1/I2C2 与 GD32F470 I2C0/I2C1 的
IRQ/DMA 装配及 typed factory；双控制器夹具已有独立实例隔离与真实 SDK 链接证据。
参考 Board 只开放明确声明的 binding；其他硅片路线与实际接线需单独审查。详见
[多控制器所有权](../design/multiple-controllers.md)。

SPI DMA 要求非空、不重叠的 TX／RX 实际访问跨度；固定地址方向只访问调用者的一字节。
不分配 scratch 或隐式复制池。实际
memory domain 与读写权限由 provider 再次校验；STM CCM、GD TCM 均不能作为 DMA
缓冲。DMA TC 表示内存传输事实，UART TC／SPI idle 表示线上事实；两者排空证明
都满足后才发布 SETTLED。取消、超时或 EN 拒绝关闭保留借用，进入 QUARANTINED。

UART 的 `irq-blocks` 保留独立 IRQ 接收实现；`rx-dma-idle` 使用有限 DMA 与 IDLE，
`rx-dma-continuous` 使用硬件双缓冲，另有显式 `dma-duplex` 和
`dma-duplex-continuous`。块由调用者提供，消费者 acquire 后保持不可变，直到匹配
epoch 的 release。连续模式必须预留两个硬件目标及替换块；替换地址读回、目标银行
和新错误检查成立后才能发布旧块。没有替换块就停止，不覆写消费者借用。
仅接收模式使用独立只读方法表，不保留 TX DMA 方法与对应链接代码；当前共享
接收状态布局仍包含 TX DMA 基类字段，不能宣称冷态字段开销为零。RTS/CTS 并不代替
容量和排空合同；RS485 的 DE 引脚、极性及 setup/hold 时间由外部工程明确提供，
到达真实 UART TC 后才开始 hold 并撤销 DE。详见
[UART DMA 与 RS485](../design/uart-dma-rs485.md)。

ADC 的 `trigger-dma` 使用普通有限 DMA；每块停止触发、转换与内存写入，再发布完整块。
调用者必须显式 service，完成上电稳定／GD 校准后重新触发下一块。无空闲 loan 时
保持暂停，不覆写、不伪造暂停期间的精确丢样数。该模式有块间间隙，不承诺连续
无间隙采样。`trigger-dma-continuous` 则显式选择硬件双缓冲及预投递块，地址替换
成功后发布完整块；至少需要三个调用者 slot。频率需满足精确 timer divider 和
整段 scan 转换时间；GD 连续模式显式选择 SDK 对应的 sample-time code，有限模式
保持原有 15-cycle sample。可选 observer 的时间戳是软件发布观测时间，不是采样
孔径时间。单个锁存 TC/CT 事实无法计算未观测的多轮丢样。stop 在持有 loan 时返回 BUSY。

I2C 中断及 DMA 共享经过两家 SDK 布局与位定义检查的 V1 寄存器算法，MCU 热路径
直接访问寄存器。DMA TC 不代表线上完成；最后字节、BTF、STOP/MSL、DMA EN 和
IRQ 发布者必须排空后才 SETTLED。bus-clear 独立要求静止外设、独占开漏 GPIO 和
有界等待，最多九个完成的恢复脉冲；STOP 或释放失败保留 lease，可继续清理。
平台没有为未经审查的 Board 自动切换引脚。详见
[I2C 总线恢复](../design/i2c-bus-clear.md)。

这些是 **SoC 实现能力**。参考 Board 默认 binding、软件测试夹具路线、现场测量结果
各自独立。STM32 默认参考 Board 未声明可资格化的 SPI CS，资源矩阵只测
empty/GPIO/UART；软件 fixture 的 SPI 不能继承为参考板 SPI 支持。GD32 默认
Board 的 SPI4 PF6/7/8/9 有公开来源，外接 NOR 具体料号仍未知。

## 明确边界

连续模式不能承诺任意 IRQ 延迟与消费者速度下无丢失。I2C 超出 256-byte 的 read、
动态 clock、deep sleep/STOP、安全启动／升级及上述未维护模式明确不提供。
FreeRTOS 可选 `lowpower_port="timebase"` 只在固定 F407/GD470 profile 使用浅 Sleep；
要求 SysTick、tickless、single privileged、无 MPU。默认 `custom` 不新增 sleep
机制；CPU-only runtime 缺少 SoC timebase 时拒绝该选择。实际相位、clock 连续性、
wake latency 与功耗保持物理未执行，见[浅睡眠](../design/timebase-lowpower.md)。
Unsupported 配置明确拒绝，不通过成功 stub 声称支持。

Flash pulse 不能被 deadline 撤销；BUSY 未清不能释放责任。产品负责 Flash 执行
停顿、电压、watchdog 窗口、掉电、认证、布局与持久化格式迁移。公共通用 storage
的 CRC/commit 机制不等于安全升级协议。

GD32 Flash 的重新上锁读回失败会保留静态 provider 并拒绝继续操作，需要受控
复位；普通 stop 不能恢复此状态。`tests/google/gd32_flash_fault_test.cpp` 专项模型
验证故障后拒绝读取／写入／重新取得控制器，不构成实板资格。
生命周期与扫描诊断的解释见[安全审查](security-review.md)。

硬件未接入，全部 MCU 目标的 clock、reset、GPIO safe output、UART TC/abort/RX、SPI/I2C
电气、Flash 掉电、watchdog reset、EXTI 波形、PWM 与 ADC 精度均未执行。当前
支持声明不能替代物理 HIL 或产品 release promotion。
