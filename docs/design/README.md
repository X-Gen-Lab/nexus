---
id: design.readme
type: reference
audience: maintainer
source_language: zh_CN
---

# 当前架构与工程合同

从本页选择要维护的合同：先核对[精确型号与模式](../delivery/support.md)、Board 和
资源责任，再进入对应源码。设计页、公共头和厂商 SDK 不单独建立新型号、软件候选
或物理资格；新增料号的内部 CRC 参考也不扩大 F407 专属模式的执行范围。
当前 physical HIL 为 `not_executed`。

## 平台与维护路径

| 要完成的工作 | 权威入口 |
| --- | --- |
| 选择型号、Board、模式与后端 | [支持矩阵](../delivery/support.md)、[派生事实](../delivery/capabilities.md)、[Board 包](../../boards/README.md) |
| 理解目录、实例和依赖 | [当前架构](../delivery/architecture.md)、[源码定位](../explanation/source-map.md)、[factory](../explanation/factory.md) |
| 审查本轮架构任务与剩余范围 | [A01–H06 接入重构任务](../delivery/architecture.md#本轮架构迭代任务)、[验收边界](../delivery/qualification.md) |
| 修改接口、借用、停止或恢复 | [所有权](../explanation/ownership.md)、[I/O 机制](io-evolution.md)、[观察与恢复](io-observation-recovery.md)、[公共声明](../reference/api/io.md) |
| 修改冷启动、时钟或 STOP | [F407 System](f407-system-power.md)、[ClockDomain 屏障](clock-domain-sleep.md)、[时间基与 OS](timebase-lowpower.md) |
| 接入 F4 型号或硬件模式 | [官方库边界](official-stm32-provider.md)、[接入合同](integration-contracts.md)、[多控制器](multiple-controllers.md) |
| 维护 CPU、权限与 OS | [Arch](arch-contracts.md)、[OS 合同](os-contracts.md)、[后端适配](os-backend-integration.md) |
| 构建、打包与外部消费 | [构建系统](build-system.md)、[Source SDK 教程](../tutorials/source-sdk.md)、[兼容边界](source-compatibility.md) |
| 修改代码、测试或文档 | [工程手册](engineering-handbook.md)、[开发流程](../development/workflow.md)、[内容标准](documentation-standards.md)、[页面模板](documentation-templates.md) |

## 外设合同入口

| 任务域 | 机制与精确实现 |
| --- | --- |
| 串行传输与线模式 | [UART DMA／RS485](uart-dma-rs485.md)、[USART 线模式](uart-wire-modes.md)、[SPI 帧](spi-frame-modes.md)、[I2C target／SMBus](f407-i2c-target.md)、[I2C 恢复](i2c-bus-clear.md) |
| 采样、控制与捕获 | [多 ADC／注入组](f407-adc-groups.md)、[高级定时器](timer-advanced.md)、[有限脉冲／波形序列](f407-timer-waveform.md)、[DCMI／I2S2／3](f407-capture-audio.md) |
| 有限／连续音频与外部时钟协调 | [DCMI／I2S 主合同](f407-capture-audio.md)、[Platform 有限 slave TX 路径](f407-i2s2-slave-tx.md)、[帧与流所有权](io-evolution.md) |
| 日历、唤醒与备份域 | [RTC 与辅助源](f407-rtc.md)、[System／epoch 恢复](f407-system-power.md) |
| 物理存储与介质 | [内部 Flash／OTP／保护维护](f407-storage.md)、[SDIO memory／MMC／functions](../../soc/stm32/stm32f4/private/sdio_hal/README.md)、[FSMC](f407-fsmc.md) |
| 网络与 USB | [Classic CAN](classic-can.md)、[Ethernet 帧](ethernet-frames.md)、[MAC／MDIO](ethernet.md)、[USB Device／Host／显式角色](usb-device.md) |

这些入口各自维护调用上下文、存储、deadline、硬件退出与失败后的责任；概述页不再
复制 pin／DMA 表或公共签名。修改一种模式时，同时核对其资源冲突、消费者、图示与
译文。分组回归只证明所执行的源码和范围，不自动合并成全候选或实板通过。

旧专项 CSV、实现和执行记录保留原始身份，范围从
[固定重构前源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/design)
追溯；不据旧计数或退役平台文档扩大当前支持。
