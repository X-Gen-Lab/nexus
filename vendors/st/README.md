# STM32 厂商依赖

当前生产 SoC 入口为 [STM32F4 系列支持包](../../soc/stm32/stm32f4/README.md)。
Device 与 HAL／LL 由系列 SDK 适配私有选择；精确料号、模式与 Board 支持以
[当前支持范围](../../docs/delivery/support.md)为准。

## 保留的源码库存

| 系列 | Device 目录 | HAL／LL 目录 |
| --- | --- | --- |
| F1 | `cmsis_device_f1` | `stm32f1_hal` |
| F4 | `cmsis_device_f4` | `stm32f4_hal` |
| G4 | `cmsis_device_g4` | `stm32g4_hal` |
| H5 | `cmsis_device_h5` | `stm32h5_hal` |
| H7 | `cmsis_device_h7` | `stm32h7_hal` |
| U5 | `cmsis_device_u5` | `stm32u5_hal` |

库存目录不建立对应系列的生产支持。H5 的 I3C 软件参考边界见
[I3C controller 合同](../../docs/design/i3c-controller.md)。CPU Core 使用独立锁定的
`vendors/arm/CMSIS_6`；版本、上游字段和许可证见[厂商版本](../VERSIONS.md)。

## 集成边界

按[开发工作流](../../docs/guides/developer-workflow.md)准备所选依赖；配置与编译
不自动联网补取。未选择的库存不进入默认准备、校验、编译或 Source SDK。
原始 SDK 字节、gitlink、行尾、版权与许可证保持；PCB 接线写在 Board，硬件适配
写在 F4 支持包，产品任务与策略由外部工程维护。

厂商 Handle、寄存器与 provider 存储保持私有。普通固件通过 `Nexus::Platform`、
typed factory 和 `nexus_add_firmware()` 消费 Nexus 接口。具体选择与退出要求见
[官方驱动合同](../../docs/design/official-stm32-provider.md)；源码库存、ARM 链接与
物理验收分别提供证据。
