# 精确支持范围

[派生能力矩阵](capabilities.md) 自动核对 SoC/route 的 mode、参考 Board binding
与实际 resolver/emitter 选择的生产源码。叙述表说明运行合同，派生表说明精确
身份与有限源码一致性；两者均不继承旧 HEAD 的资格或替代实板执行。

## 芯片、板卡与系统

| 目标 | 软件实现 | 未完成的物理事实 |
|---|---|---|
| STM32F407ZGT6 / 启明欣欣 V3.1 | Cortex-M4F；1 MiB Flash；主 SRAM 128 KiB、CCM 64 KiB；声明 clock/profile 与 reviewed routes | 实际 PCB 修订、晶振、供电、连续性、波形与负载 |
| STM32F407VET6 / 天空星青春版 | Cortex-M4F；512 KiB Flash；主 SRAM 128 KiB、CCM 64 KiB；独立 Board 资源 | 相同类别的实际 PCB／电气／HIL 验收 |
| GD32F470ZGT6 / 梁山派 | Cortex-M4F；1 MiB Flash；主 SRAM 192 KiB、ADDSRAM 256 KiB、TCM 64 KiB；25 MHz 声明输入、200 MHz profile | 未观察 PCB/晶振/电气；默认 linker 只用主 SRAM；USB 48 MHz 未承诺 |
| Native | Linux/POSIX 行为模型、实际线程与持久化文件故障模型 | 不构成 MCU 或电气资格 |

裸机和锁定 FreeRTOS 均为显式选择的薄适配。启动只建立平台时钟／接口，不创建
scheduler、产品任务或工作流。FreeRTOS POSIX host 运行验证软件生命周期，不能
替代 Cortex-M4F IRQ ceiling、FPU context 与 stack high-water 的实板验证。

CPU 软件层另支持 M0／M0+／M3／M4／M7／M23／M33／M55／M85，统一严格事实
解析器选择 Core/Arch、原子操作和裸机/FreeRTOS runtime。M0/M0+ RMW 使用
bounded saved-PRIMASK；可选 cache/MPU/SAU 有真实 CPU 算法，FPU/MVE 与安全
状态决定精确 kernel port 和 ABI。没有新增 SoC/Board/factory，以上完整平台列表
保持独立。编译、寄存器模型与真实 FP/MVE/cache/MPU/安全世界验收分别记录；
可选 restricted MPU task 和 split Secure/NonSecure runtime 已有独立软件实现；
窄 delay/tick SVC、Secure context lease 与调用者静态存储分别验证。默认仍为
privileged single-world；MPU+split 明确拒绝，未新增实板 HIL。范围和配置例子见
[Cortex-M 软件支持](cortex-m.md) 与 [Arch 契约](../design/arch-contracts.md)。

## 首发模式

| 接口 | 已实现模式与限制 |
|---|---|
| GPIO | authorized mask、原子 set/reset、快照 read、single-writer toggle |
| UART | 8N1 IRQ TX 或有限 TX DMA；caller-owned request、真实 TC；IRQ RX blocks／IDLE、有限及连续 RX DMA、双向 DMA；显式 RTS/CTS 与外部静态 RS485 适配；STM baud 最低 1282、GD 最低 1526，最高 1 Mbaud |
| SPI | 8-bit MSB-first；短轮询每笔最多 256 bytes；有限 DMA 最多 65535 bytes；双向递增或调用者单字节固定 dummy/discard；单 wire-active、完整 CS interval；独立 CS endpoint |
| I2C | 100 kHz；最多 8 条消息、每条 read/write 1–256 bytes；轮询、中断及 DMA 消息；1/2/末三字节 ACK、repeated START/NACK/仲裁/STOP；恢复检查 BUSY 与所选 SCL/SDA 电平；独立有界 GPIO bus-clear 工具 |
| Flash | STM 精确 16/64/128 KiB sector 几何与 x32 program（4-byte alignment/length，声明供电 2.7–3.6 V）；GD 官方独立 4 KiB page erase 与 halfword program；全物理空间、外部受限 region |
| Watchdog | 独立 enable/feed/reset cause；启用不可逆，late failure 不擦除责任；GD IRC32K 物理 timeout bounds 未保证 |
| EXTI | selected line、共享 vector、caller-sized event queue、丢失／coalescing 可见；无 debounce 政策 |
| PWM | 固定 timer/channel/base、CCR shadow、0/100%、safe inactive；公共 stop 可以 restart，平台 teardown 释放 owner |
| ADC | reviewed channel、single-shot/低速 scan、有限及连续双缓冲定时触发 DMA blocks；显式 sample time/reference；只发布完整扫描；可选外部序列与软件观测时间记录 |

有限 DMA 的精确首发路线如下，其他 controller／通道组合由 resolver 拒绝。

| 模式 | STM32F407 | GD32F470 |
|---|---|---|
| UART TX DMA | USART1；DMA2 stream7 channel4 | USART0；DMA1 channel7 selector4 |
| UART RX DMA／双向 DMA | USART1；RX DMA2 stream2 channel4；TX 同上 | USART0；RX DMA1 channel2 selector4；TX 同上 |
| SPI 全双工 DMA | SPI1；DMA2 RXstream0／TXstream3 channel3 | SPI4；DMA1 RXchannel3／TXchannel4 selector2 |
| I2C 消息 DMA | I2C1；DMA1 RXstream0／TXstream6 channel1 | I2C0；DMA0 RXchannel0／TXchannel6 selector1 |
| ADC 有限／连续 trigger blocks | ADC1；TIM3 TRGO；DMA2 stream4 channel0 | ADC0；TIMER2 TRGO；DMA1 channel0 selector0 |

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
仅接收模式使用独立只读方法表，不驻留 TX DMA 状态和方法。RTS/CTS 并不代替
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
capture／advanced PWM、动态 clock／低功耗、CAN、USB、Ethernet 与安全启动／升级
未作为本批实现。
Unsupported 配置明确拒绝，不通过成功 stub 声称支持。

Flash pulse 不能被 deadline 撤销；BUSY 未清不能释放责任。产品负责 Flash 执行
停顿、电压、watchdog 窗口、掉电、认证、布局与持久化格式迁移。公共通用 storage
的 CRC/commit 机制不等于安全升级协议。

GD32 Flash 的重新上锁读回失败会保留静态 provider 并拒绝继续操作，需要受控
复位；普通 stop 不能恢复此状态。`tests/google/gd32_flash_fault_test.cpp` 专项模型
验证故障后拒绝读取／写入／重新取得控制器，不构成实板资格。
生命周期与扫描诊断的解释见[安全审查](security-review.md)。

硬件未接入，三板 clock、reset、GPIO safe output、UART TC/abort/RX、SPI/I2C
电气、Flash 掉电、watchdog reset、EXTI 波形、PWM 与 ADC 精度均未执行。当前
支持声明不能替代物理 HIL 或产品 release promotion。
