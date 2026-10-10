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

## 首发模式

| 接口 | 已实现模式与限制 |
|---|---|
| GPIO | authorized mask、原子 set/reset、快照 read、single-writer toggle |
| UART | 8N1 IRQ TX 或有限 TX DMA；caller-owned request、真实 TC、byte/event RX；独立 IRQ RX blocks／IDLE 模式；STM baud 最低 1282、GD 最低 1526，最高 1 Mbaud |
| SPI | 8-bit MSB-first；短轮询每笔最多 256 bytes；全双工有限 DMA 最多 65535 bytes；单 wire-active、完整 CS interval；明确的独立 CS endpoint |
| I2C | 100 kHz；最多 4 条消息、每条写最多 256 bytes；末条 read 长度 1/2；repeated START/NACK/仲裁/STOP；恢复检查实际 bus/line 状态 |
| Flash | STM 精确 16/64/128 KiB sector 几何与 x32 program（4-byte alignment/length，声明供电 2.7–3.6 V）；GD 官方独立 4 KiB page erase 与 halfword program；全物理空间、外部受限 region |
| Watchdog | 独立 enable/feed/reset cause；启用不可逆，late failure 不擦除责任；GD IRC32K 物理 timeout bounds 未保证 |
| EXTI | selected line、共享 vector、caller-sized event queue、丢失／coalescing 可见；无 debounce 政策 |
| PWM | 固定 timer/channel/base、CCR shadow、0/100%、safe inactive；公共 stop 可以 restart，平台 teardown 释放 owner |
| ADC | reviewed channel、single-shot/低速 scan、定时触发 DMA blocks；显式 sample time/reference；轮询超时有效 prefix；块模式只发布完整扫描 |

有限 DMA 的精确首发路线如下，其他 controller／通道组合由 resolver 拒绝。

| 模式 | STM32F407 | GD32F470 |
|---|---|---|
| UART TX DMA | USART1；DMA2 stream7 channel4 | USART0；DMA1 channel7 selector4 |
| SPI 全双工 DMA | SPI1；DMA2 RXstream0／TXstream3 channel3 | SPI4；DMA1 RXchannel3／TXchannel4 selector2 |
| ADC trigger blocks | ADC1；TIM3 TRGO；DMA2 stream4 channel0 | ADC0；TIMER2 TRGO；DMA1 channel0 selector0 |

SPI DMA 要求非空、不重叠的 TX／RX 缓冲，不分配 scratch 或隐式复制池。实际
memory domain 与读写权限由 provider 再次校验；STM CCM、GD TCM 均不能作为 DMA
缓冲。DMA TC 表示内存传输事实，UART TC／SPI idle 表示线上事实；两者排空证明
都满足后才发布 SETTLED。取消、超时或 EN 拒绝关闭保留借用，进入 QUARANTINED。

UART block 模式使用 IRQ 接收与 IDLE／满块边界，不是循环 DMA RX。块由调用者
提供，消费者 acquire 后保持不可变，直到匹配 epoch 的 release。无空闲块时可见
丢失，stop 保留最后 prefix 和消费者借用。TX DMA＋RX blocks 的组合当前未实现，
配置明确拒绝，不为一个模式隐式驻留另一个模式的接收环。

ADC block 模式使用普通有限 DMA；每块停止触发、转换与内存写入，再发布完整块。
调用者必须显式 service，完成上电稳定／GD 校准后重新触发下一块。无空闲 loan 时
保持暂停，不覆写、不伪造暂停期间的精确丢样数。该模式有块间间隙，不承诺连续
无间隙采样。频率需满足精确 timer divider 和整段 scan 转换时间；GD block 使用
15-cycle sample，GD 低速轮询使用 480-cycle sample。stop 在持有 loan 时返回 BUSY。

这些是 **SoC 实现能力**。参考 Board 默认 binding、软件测试夹具路线、现场测量结果
各自独立。STM32 默认参考 Board 未声明可资格化的 SPI CS，资源矩阵只测
empty/GPIO/UART；软件 fixture 的 SPI 不能继承为参考板 SPI 支持。GD32 默认
Board 的 SPI4 PF6/7/8/9 有公开来源，外接 NOR 具体料号仍未知。

## 明确边界

UART 循环 DMA RX、无间隙 ADC 环形采样、I2C DMA／任意长 read、capture／advanced
PWM、动态 clock／低功耗、CAN、USB、Ethernet 与安全启动／升级未作为首发实现。
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
