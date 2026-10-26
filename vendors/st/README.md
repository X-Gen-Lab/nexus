# 维护中的 STM32 SDK

当前五个 STM32 SoC 接入为 F103、F407、G474、H743 与 U575。每个接入保留
独立锁定的官方 CMSIS Device 和 HAL/LL 子模块；具体型号、外设模式和硬件边界
以各 `soc/stm32/<family>/README.md` 与当前支持矩阵为准，不表示整个系列均受支持。

| Nexus family | Device 仓库目录 | HAL/LL 仓库目录 |
| --- | --- | --- |
| F103 | `cmsis_device_f1` | `stm32f1_hal` |
| F407 | `cmsis_device_f4` | `stm32f4_hal` |
| G474 | `cmsis_device_g4` | `stm32g4_hal` |
| H743 | `cmsis_device_h7` | `stm32h7_hal` |
| U575 | `cmsis_device_u5` | `stm32u5_hal` |

共同 CPU 头迁移为 `vendors/arm/CMSIS_6` 的官方 v6.4.0 pin；Device、startup
与 HAL/LL 的原 gitlink 保持。版本、PDSC 原始字段及许可证边界见
[供应商版本](../VERSIONS.md)。CMake 内部 `nexus_cmsis_core` 与
`nexus_use_cmsis_core(target)` 维护统一 include 来源；普通应用不链接这个内部目标，
也不把厂商类型写入公共 Nexus 接口。它与公共 `Nexus::Core` 中性机制不是同一层。

Board 的只读 `board.json` 拥有已审接线及未知 PCB 事实；SoC facts、startup、
系统时钟和 controller provider 位于 `soc/stm32/<family>/`。现有 typed I/O 的
具体 I2C 模式、恢复责任与电气验证须按所选 family 合同解释，不能从一个 SDK
目录或公共接口声明推导其他系列的支持。

在仓库根初始化提交锁定的依赖：

```sh
git submodule update --init --recursive
```

缺失必要头或不同内容身份会拒绝配置／SDK 消费，配置不联网补取依赖。保留原
供应商字节、行尾与版权通知；板级接线和产品逻辑写在 Nexus adapter 或外部工程。
Core 升级的实际 include 闭包、ARM 链接、设备契约与移位 SDK 消费分别验收，
不继承旧源码的成功结果，不建立新的实板资格。

普通固件通过 `Nexus::Platform`、typed factory 与 `nexus_add_firmware()` 消费
平台；SDK headers 与 provider storage 保持实现私有。产品任务、worker、布局、
Flash 分区和恢复策略由外部应用维护。精确范围见
[当前支持](../../docs/delivery/support.md)与
[官方驱动合同](../../docs/design/official-stm32-provider.md)。
