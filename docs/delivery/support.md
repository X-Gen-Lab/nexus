---
id: delivery.support
type: reference
audience: consumer
source_language: zh_CN
---

# 当前精确支持范围

当前生产入口为 STM32F4，迁入的具体实现仍限定 STM32F407VET6／STM32F407ZGT6。
两者共享系列源码，Flash 密度、封装和允许引脚来自精确事实。其他 F4 型号尚需逐项
接入与验证，不能由官方 HAL 或器件宏存在推定支持。

## 型号、板卡与后端

| 对象 | 当前边界 |
|---|---|
| F407VET6 | LQFP100、512 KiB Flash、128 KiB SRAM 与 64 KiB 非 DMA CCM |
| F407ZGT6 | LQFP144、1 MiB Flash、128 KiB SRAM 与 64 KiB 非 DMA CCM |
| 启明 V3.1／核心板／V4.1 | 三个独立 ZG 来源与装配身份，不能互相代替 |
| 天空星青春版 | 独立 VE 板卡事实，不推广到 VG 高配板 |
| 裸机／FreeRTOS | 按实际 assembly、IRQ ceiling 和模式选择 |
| ThreadX | 明确维护的 M4 子集；不继承 FreeRTOS 全部 profile |
| Native | 显式 host 测试 scope，不是生产芯片或 PCB |

精确资源权威是 [soc.json](../../soc/stm32/stm32f4/soc.json)、
[routes.json](../../soc/stm32/stm32f4/routes.json)和各 [Board 包](../../boards/README.md)。
当前 F407 时钟仍为受审 HSE8／168 MHz profile；不能配置任意时钟或运行期变频。

[器件目录](../../soc/stm32/stm32f4/devices.json)只描述官方宏、启动和 IRQ 的
SDK 编译事实，不为其他 F4 型号建立驱动、Board 或运行时模式资格。

## 已有模式与限制

下表是源码维护范围摘要，完整选择须通过实际 resolver；不表示本次重构全部软件
退出或物理测试已经通过。

| 类别 | 维护范围与必要限制 |
|---|---|
| GPIO／EXTI | 受审引脚、静态电气与事件存储；不存在自动消抖政策 |
| UART | IRQ byte/event、word 与明确 block／DMA 模式分别选择；有限 TX、有限 RX、连续 RX 和双向模式不自动互相授权 |
| SPI／I2C | SPI1／3 polling 与 DMA、SPI2 DMA；I2C1／3 polling／IRQ／DMA、I2C2 IRQ／DMA；显式 endpoint 与受审路线 |
| ADC／Timer | ADC1／2 外部 0--15、ADC3 按封装选择外部输入，独立 shot／scan 最多 16 ranks；三个转换器均有有限／连续 DMA，分别独占 TIM3 |
| DAC | 单／双通道 direct、软件或独占 TIM6／7 触发、noise／triangle；有限／循环触发 DMA、三种输入格式与显式空闲码 |
| Flash／IWDG／RTC | 精确几何与生命周期；构造不隐式启狗，产品分区与备份域政策外置 |
| CRC／芯片身份 | 固定多项式 CRC-32 字序列；实际 UID、device／revision ID 和半字 Flash 容量；不建立认证或订货号推断 |
| Classic CAN | 所选 mailbox 与共享 filter domain；不提供 CAN FD |
| USB | 所选 OTG FS device PIO；不由此授予 Host、HS、DMA 或全部 class stack |
| Ethernet | 所选 RMII／MDIO 与帧模式；协议栈、PHY 接线及实物行为分别验证 |
| SDIO／FSMC | 所选 polling SDIO、外部窗口与 parallel16；启动 SRAM 为永久 boot owner |
| 可选器件 | ILI9341 parallel16、XPT2046、W25 等通过公共端口访问，板级供电和产品策略外置 |

F407 `dma-tx` 的所选 UART transport 使用官方 HAL 有限 TX 与分离 LL IRQ RX，
Handle 私有且由同一 owner 管理。其他模式保留各自明确实现，不能宣称整套 UART
或全部外设已完成 HAL 替代。

ADC1／2 通道对应 PA0--7、PB0--1、PC0--5。ADC3 在 VE 开放 PA0--3 的通道 0--3
和 PC0--3 的通道 10--13；ZG 另有 PF6--10 的通道 4--8、PF3 的通道 9 和
PF4／5 的通道 14／15。序列可以重新排序或重复同一输入，最多 16 ranks，重复
输入的 sample time 必须一致；每个使用的引脚都须有明确 Board route。内部输入、
注入组和同步多 ADC 模式尚未接入。16-rank 与三独立实例参考装配见
`tests/contracts/adc_external_assembly/`，不授予真实 PCB 或物理采样资格。

三个 ADC 的 `resolution_bits` 均可选择 6／8／10／12，默认 12。各转换器的 poll、
有限和连续 DMA 共用该静态选择，转换周期与触发速率准入随之变化。DMA 仍使用
halfword，样本仍是右对齐的 `uint16_t` 原始计数；分辨率选择不建立模拟精度或实际吞吐资格。

共享 ADCCLK 固定为 PCLK2／4，即 21 MHz。冷态改分频前先取得数字接口观察责任，
确认所有 ADC 已关闭，再撤回本次临时 gate；匹配分频时不写 CCR。准备与其他 ADC
启动必须串行，不支持并发热接入或动态调频。软件采样停止回读失败保留设备清理责任，
平台关闭成功前禁止再次采样，但返回后不保留调用者的样本或 count。

DMA buffer 必须位于所选可达域，CCM 不可用于该 DMA。连续 producer 不覆盖已借用
块，缺替换存储时停止并报告 loss；模型或静态预算不证明任意负载下无丢失。
取消、超时和停止失败继续保留 buffer 与恢复责任，直到真实排空并发布 SETTLED。

DAC 由一个 controller owner 独占，按所选通道取得 PA4／PA5；与 ADC 或 SPI 的
引脚冲突必须拒绝。公共 CPU 写入始终接收未移位原始码，`u12-left` 由 C 编码，
数字 DOR 始终为右对齐 12 位，`u8` 观察值为输入码乘 16。TIM6／7 触发只开放
独占的 84 MHz 固定时钟源，API 的 1 MHz 上限是数字配置政策，不是模拟性能保证；
实际分频可查询。noise 是确定性波形，不能用于熵源。stop 保持所声明空闲码，
完整 Platform stop 才禁用 DAC 并恢复原引脚字段；失败保留责任且可重试。两种密度的
`tests/contracts/dac_assembly/` 只提供软件参考，不代表 PCB 接线、负载、精度或
模拟 settling 资格。外部共享触发与物理验收仍未完成。

DAC 的 `trigger-dma`／`trigger-dma-circular` 是可选静态 TX 分面，共用同一个
controller ID；CPU 模式不预留 DMA 状态。调用方提供最多 65535 个编码帧，明确
字节数、对齐和 DMA 可读域；左对齐与双通道 lane 已由调用方编码，保留位须为零。
通道 1／双通道使用 DMA1 stream5 channel7，单独通道 2 使用 stream6 channel7，
同时独占所选 TIM6／7、stream IRQ 与 TIM6_DAC IRQ。IRQ 只锁存事实和可选 wake，
调用方推进 service；不会隐式建立 task、queue 或复制缓冲区。

有限 DMA 成功包含最后数字锁存；首触发输出 idle，尾部可能短暂停 timer、插入
间隔或重复末码，不承诺恰好 N 次模拟转换。循环模式持续借用至取消、超时或故障后
完成排空。SETTLED 之前 request 与帧保持有效且不可改写；无法排空时保留
QUARANTINED，next_service 返回 NEVER，由调用方明确恢复和重试。欠载状态需显式
stop 与 W1C 回读恢复。软件模型和 ARM 链接不授予物理输出性能资格。

静态装配指定唯一 CRC controller owner；factory 只查询既有接口。
`crc32-words` 使用多项式 0x04C11DB7、
初值 0xffffffff、MSB 优先、不反射且无 final XOR。输入为 32 位数值字，长度以字
计；不隐式拼装或补齐字节流。空序列返回初值。每 16 字和最终读回检查绝对 deadline；
失败不改输出、不保留借用，下一次计算重新 reset。输入／输出与 state、CRC 和其
时钟控制区隔离；关闭只撤回新增 gate，失败保持可重试 receipt，CRC_IDR 保持不变。

`nx_chip_info_read` 无需启动外设，直接读取实际 UID、device／revision ID 和
16 位 Flash 容量，不补用配置值、不开时钟、不分配常驻状态。身份数据不代替硅片
启动准入，也不证明完整订货号／封装或认证秘密。Native 显式返回 UNSUPPORTED；
模型与 ARM 链接仍不能证明物理身份、吞吐或实板功能。

SPI3 的受审 AF6 路线是 PC10／11／12 或 PB3／4／5；I2C3 是 PA8 SCL 与 PC9 SDA
AF4，恢复检查两条真实输入，不用同一 GPIO bank 的邻位代替。SPI3 使用 DMA1
stream5 TX／stream0 RX channel0；I2C3 使用 stream4 TX／stream2 RX channel3。
与其他实例重用物理 stream 时配置拒绝，不自动替换路线。

ADC1 使用 DMA2 stream4 channel0，ADC2 使用 stream2 channel1，ADC3 使用
stream1 channel2；有限和连续模式均按所选 lane 解释并清除标志。TIM3 是独占触发源，
不同 ADC 的 DMA stream 不冲突也不能同时取得同一 TIM3。软件参考装配见
`tests/contracts/multi_controller_assembly/*tertiary*.toml`，覆盖 VE／ZG 和裸机／
FreeRTOS；它们不授权实际 Board 接线。

## 选择、证据与保留依赖

[派生矩阵](capabilities.md)关联声明模式、受审路线、Board、装配与实际生产 ops。
查询通过只建立配置一致性；模型、ARM、SDK、软件候选和物理 station 记录分别验证。
当前物理 HIL 为 `not_executed`，未知 PCB、跳线、电源与波形仍保持未知。

所有 vendor、子模块和许可证保留；未选 SDK 不进入生产支持或默认构建闭包。
退出的其他 MCU 适配仅保留历史追溯，原始范围见
[重构前固定源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)。
本次重构不能继承旧候选、软件计数或实板结果。
