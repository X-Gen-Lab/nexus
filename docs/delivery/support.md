# 精确支持范围

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
| UART | 8N1 IRQ TX、caller-owned request、真实 TC、byte/event RX、可见 loss；STM baud 最低 1282、GD 最低 1526，最高 1 Mbaud |
| SPI | 8-bit MSB-first；有限 deadline；每笔最多 256 bytes；单 wire-active、完整 CS interval；GD SPI4 固定 PF6 CS |
| I2C | 100 kHz；最多 4 条消息、每条写最多 256 bytes；末条 read 长度 1/2；repeated START/NACK/仲裁/STOP；恢复检查实际 bus/line 状态 |
| Flash | STM 精确 16/64/128 KiB sector 几何与字节 program；GD 官方独立 4 KiB page erase 与 halfword program；全物理空间、外部受限 region |
| Watchdog | 独立 enable/feed/reset cause；启用不可逆，late failure 不擦除责任；GD IRC32K 物理 timeout bounds 未保证 |
| EXTI | selected line、共享 vector、caller-sized event queue、丢失／coalescing 可见；无 debounce 政策 |
| PWM | 固定 timer/channel/base、CCR shadow、0/100%、safe inactive；公共 stop 可以 restart，平台 teardown 释放 owner |
| ADC | reviewed channel、single-shot/低速 scan、显式 sample time/reference、超时有效 prefix；GD 固定 480 cycles |

这些是 **SoC 实现能力**。参考 Board 默认 binding、软件测试夹具路线、现场测量结果
各自独立。STM32 默认参考 Board 未声明可资格化的 SPI CS，资源矩阵只测
empty/GPIO/UART；软件 fixture 的 SPI 不能继承为参考板 SPI 支持。GD32 默认
Board 的 SPI4 PF6/7/8/9 有公开来源，外接 NOR 具体料号仍未知。

## 明确边界

UART/SPI/I2C/ADC DMA、UART block/stream、任意长 I2C read、capture/advanced
PWM/ADC trigger、动态 clock/低功耗、CAN、USB、Ethernet 与安全启动／升级未作为
首发实现。Unsupported 配置明确拒绝，不通过成功 stub 声称支持。

Flash pulse 不能被 deadline 撤销；BUSY 未清不能释放责任。产品负责 Flash 执行
停顿、电压、watchdog 窗口、掉电、认证、布局与持久化格式迁移。公共通用 storage
的 CRC/commit 机制不等于安全升级协议。

GD32 Flash 的重新上锁读回失败会保留静态 provider 并拒绝继续操作，需要受控
复位；普通 stop 不能恢复此状态。该 relock-fault 当前没有专项模型或实板资格。
生命周期与扫描诊断的解释见[安全审查](security-review.md)。

硬件未接入，三板 clock、reset、GPIO safe output、UART TC/abort/RX、SPI/I2C
电气、Flash 掉电、watchdog reset、EXTI 波形、PWM 与 ADC 精度均未执行。当前
支持声明不能替代物理 HIL 或产品 release promotion。
