# STM32F103RB 精确芯片实现

维护型号为 STM32F103RBT6、LQFP64、128 KiB Flash、20 KiB SRAM，CPU 为无 FPU／
DSP／MPU 的 Cortex-M3。43 个外部 IRQ slot 和 4 个优先级位来自官方 Device 头；
不推广到其他 F1 密度、封装或 connectivity-line 器件。

## 架构与基础模式

官方 startup、主 RAM 前 CPU-only `SystemInit`、复位 HSI8 准入、独立 TIM2、GPIOA
PA5、USART2 PA2/PA3 IRQ bytes、IWDG、PA0/PA1 EXTI 和 PA4 CS 的有限 SPI1 polling
构成基础切片。扩展另选 USART2 有限 DMA TX／duplex 有限 RX block，以及默认
PB6/PB7 I2C1 IRQ message。其他模式配置时拒绝。

`soc.json`／`routes.json` 描述事实和路线，`configure/` 生成精确状态、有限 RX ring、
只读 face 和调用。factory 返回预存对象，不引入 heap、任务、registry、lock 或
scheduler。硬件行为由 `system.c`、`drivers/` 与私有 cold C owner 维护。

## 官方来源、电气与身份

私有 CMSIS、GPIO／USART／I2C LL 使用 STM32CubeF1 v1.8.7 锁定配对，两份依赖锁
保留原 notice 和选定文件身份。I2C RX 通过私有编译期 LL port 复用，command／ACK
例外见 [官方驱动合同](../../../docs/design/official-stm32-provider.md)。厂商类型不
进入公共接口；不链接完整 HAL C engine、可变 Handle、HAL tick 或 `HAL_Init`。
有限 direct control write 保留借用并避免 LL exclusive retry loop。F1 CRL/CRH
四位字段及 SR/DR 清除序列不同于现代 MODER/AFR、ICR/TEACK，不能套用。

NUCLEO-F103RB 根据 UM1724 Rev17，PA5/LD2 和 USART2 PA2/PA3 VCP 是资料路线，
实板须核对 SB13/SB14 ON、SB62/SB63 OFF。PCB 修订／焊桥未知，不假设外部振荡器；
名义 HSI8 和 baud 保留数据手册容差，不声称已校准测量。

独立 startup 读取 `0x1FFFF7E0` factory Flash-size 半字，要求 128 KiB；RM0008
Rev21 §30.1.1 允许 CPU 读取。ES096 Rev15 §2.2.2 规定普通固件不能读取有效
`DBGMCU_IDCODE`／`DBGMCU_CR`，非 debug 下读零。因此启动不以 debug visibility
准入，device group／silicon revision 仍未观察，密度匹配也不证明料号、封装或 PCB。
严格 revision 策略不能把不可读当 revision A、把配置字面值当观察，或凭较新的编码
推断许可。

同一勘误也排除普通固件的 `DBGMCU_CR` 操作。IWDG 仅支持 `debug_freeze=false`；
请求 true 在 context／output 校验后、任何硬件访问或激活前返回 `UNSUPPORTED`。
enable/feed/state/失败准备清理均不读写 debug control。返回 false 是支持的 CPU
配置，不是 debugger 设置观察；外部 debugger 独立拥有其 freeze，provider 不接管
或清除。HAL／LL freeze 宏也会直接访问该寄存器，不能绕过勘误。

## 时钟、引脚和异步所有权

冷准入在取得资源前拒绝外来 clock profile、timer、vector、USART gate、remap、
locked pad 或 driven pin。F103 TIM2 是 **16-bit**，PSC7、ARR65535 形成名义 1 MHz、
65.536 ms 回绕，overflow handler 必须在每圈执行；不支持长 mask、NMI/HardFault
读取或 deep STOP 连续性。时间快照修正一次 pending wrap。PSC UG 在 CEN=0、MMS=CEN
时发出，不导出未选 TRGO 脉冲。W0C 回读失败使 epoch 失效，保留全部 owner，观察
故障后永久停止。

C Platform owner 只租用选定 CRL 字段／latch，生成不可变选择和一份 8-byte receipt；
preflight、取得、回读、恢复和失败重试均在 C。GPIO 先 latch 后 output；timer stop
先撤源并确认 flag，再最终证明 NVIC disabled/pending 清除。stop 先排 controller／
notify 引用，再恢复 pin 和自有 gate；失败保留清理责任。全部时钟消费者退出后才能
停 timer，新 epoch 不能继续使用旧 deadline。

TX request／payload 直到真实 TC 和 IRQ 源撤离后才以 release 发布 SETTLED。timeout／
cancel 停止装载新字节，排空失败保留 QUARANTINED。RX 显式 byte ring，不覆盖未读
数据，loss 可观察；notify 仅提示，已捕获 callback 须结束后才能归还其 target。

EXTI0/1 是独立 vector6/7、AFIO EXTICR GPIOA；PA0/PA1 是资料 CN8 A0/A1 pin1/2。
外部激励、电压、pull、debounce 外置。每 IRQ W1C 一次，记录一个可合并时间事实；
both-edge 方向只采样实时电平。ring 满保留旧事实，之后报告 loss boundary。stop
证明源／vector 退出后，仅恢复选中的 mux field。

IWDG 按 DS5319 名义 40 kHz、LSI 30..60 kHz，不套 F4 界限。PR/RLR 可独立 latch，
更新失败保留保守交叉组合 timeout envelope。activation 不可逆，启用时 Platform
stop 返回 STATE；激活前 LSI/debug rollback 失败保留 lease，需普通 stop 重试。
观察 hardware option 不改写；自动 PR/RLR 更新未完成时报告 enabled/irreversible、
unknown bounds 和 UNSUPPORTED。产品负责喂狗、健康、复位和 debug 策略，未进行破坏性
硬件启用。

## SPI 有限 polling

SPI1 默认 PA5 SCK、PA6 MISO、PA7 MOSI，加显式 PA4 低有效 endpoint。8 MHz APB2
分频向下选择 31,250..4,000,000 Hz。每次特权 task 调用为 1..256 个全双工字节、
有限绝对 deadline 和完整 CS interval，支持 mode0..3、精确原地 buffer，拒绝部分
重叠。实际 BSY 排空、CS-high 回读和最终 deadline 检查后才成功。

失败先 reset 并检查 controller，再归还 buffer，恢复需显式调用。无法 reset 或
无法证明 CS 释放时保留 executor 和 buffer，mask IRQ、调用 fault observer；即使
observer 返回也继续 park，不报告完成。PA5 与 LD2 冲突，不可同选，负载需实板验证。
无 SPI DMA 或 async。

## UART channel DMA 与 I2C IRQ

`dma-tx` 使用 DMA1 channel7＋IRQ byte ring，不取 channel6；`dma-duplex` 额外使用
channel6 接收调用者普通 RX block。TX／每块 1..65535 bytes，完整位于 20 KiB SRAM
DMA 域；本 profile 不准入 Flash TX。SDK wrapper 检查实际 USART/DMA ABI、gate 和
所有选定 vector，外来 channel 在 admission 前拒绝。

每个 mode 生成共享只读 ops 和精确实例。TX-only 只含 compact TX context／byte ring；
duplex 在同一 TX prefix 后加 RX block extension。直接 initializer／vector 使 section
GC 能移除 TX-only 未用 RX 代码，不新增运行时转发或配置开关，借用／排空／wake
合同不因裁剪改变。

DMA memory complete 不等于 wire complete。TX 直到 DMA request/channel/IRQ 撤源
并观察 TC 才结清；deadline 后第一次观察完成报告 timeout。排空被拒仍 QUARANTINED。
RX 在 DMAR/EN 撤离、最终 count 稳定、ACK 后发布；IDLE 是观察到的 prefix，不是精确
逐字节边界。错误丢弃该块并报 loss，有限边界丢弃的 DATA 字节也报 loss。service
重新接纳 free block，可能有间隙，不复用消费者借用。RX stop 在 producer、READY、
consumer loan 未退出前保持 BUSY。

I2C1 默认 PB6 SCL／PB7 SDA legacy AF open-drain，APB8 MHz、CCR50/TRISE9，名义
80 kHz，event/error IRQ priority0。ES096 §2.8.1 要求最高优先级，§2.8.2 接收窗口
保存中断 mask，§2.8.5 排除受影响 88..100 kHz repeated-START。拒绝这些 IRQ 的 kernel
callback，FreeRTOS owner 可轮询已锁存 request 或使用非内核通知。产品须预算全局
mask 并实测总线时序。

I2C 冷 owner 独立租用 PB6/PB7 与新增 GPIOB/AFIO/I2C1 gate，先 release 线路，保留
邻接 pin，core IRQ/wake 撤离后才恢复 nibble/latch。外部上拉、电压、电容、endpoint
和 revision 待验证；低／busy 总线拒绝，不自动 GPIO clear，不加入产品恢复、SMBus、
slave 或 I2C channel DMA。`tests/contracts/legacy_transport_assembly` 分别在
baremetal／FreeRTOS 选择这些模式，ARM 和寄存器模型不证明实物波形。

## Flash、ADC 与 PWM

Flash face 暴露完整 128 个 1 KiB page、2-byte program unit；`poll-word` 名称不代表
32-bit program。Board 必须声明 `supply_mv`，参考 3300 mV 是假设，准入 2700..3600 mV。
产品提供可写 erase-aligned region，并让运行 code/vector/live data 避开破坏性操作。
BUSY 未清即使 deadline 到期也等待，timeout 只阻止下一 pulse，不取消当前 pulse。
半字须已擦除，all-zero 例外；all-0xff 输入跳过编程。ES096 §2.2.11 delayed BSY
通过 erase start 后控制寄存器读＋barrier 处理。relock 失败保留 owner 供 close 重试。

同 bank code/vector fetch 可停顿，DS5319 最大 page erase 为 40 ms；TIM2 65.536 ms
服务预算还须包括该停顿、其他 IRQ 延迟和 mask。不保证紧 IRQ deadline、RAM 执行
闭包或喂狗策略；永不 idle 的 pulse 不归还调用者存储。FreeRTOS SysTick 可合并，
本参考不提供 kernel tick catch-up。

ADC1 只接受 PA0/channel0，12-bit 独立软件 shot、APB2/2=4 MHz、sample code7：
239.5 sample＋12.5 conversion cycles。启动后名义等 4 µs，再 reset calibration 和
完成 calibration 才准入采样，converter 到 close 前保持供电。整数 sequence API 将
两个半周期各向上取整，报告保守 253-cycle 调度界限；物理总和为 252 cycles。
reference 名义值由调用者声明，输入阻抗、PA0 glitch、精度和接线须实板验证，无 DMA／
stream 承诺。

TIM3 CH1 默认 PA6，固定 8 MHz 分频，period1..65535，Board `initial` 声明非活动
电平。start 在 TRGO 跟随禁用 CEN 时加载 shadow，再启 AF output 和 timer。stop
先预置并选择 GPIO inactive，再独立尝试各 timer 撤源；某 timer leaf 失败不跳过
pad detach，失败保留 timer／pin lease，全部退出后 Platform 才恢复原 pin。零／满
占空比为 CCR0／CCR=period。ADC/PWM/Flash 与 GD303 复用受审 legacy engine，各自
核对真实官方 ABI 和静态资源，不增加运行时转发表。

## 验证范围

GoogleTest/GoogleMock 对外部硬件模型运行真实生产 C，Python 检查 facts／route／
拒绝／构造，ARM 夹具消费 baremetal／FreeRTOS typed factory。以上是软件与链接
证据；HIL 为 `not_executed`。原资料与精确观察索引在 `references.json`。
