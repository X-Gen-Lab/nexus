# 维护中的 STM32 SDK

当前 Nexus STM32 实现只维护 **STM32F407**，使用两项锁定的官方子模块：

| 依赖 | 用途 |
| --- | --- |
| `cmsis_device_f4` | STM32F4 器件定义、系统与启动源码 |
| `stm32f4xx_hal_driver` | STM32F4 HAL 驱动实现 |

共同 CPU 定义来自 `vendors/arm/CMSIS_5`。开发板与 PCB 连接由 `boards/`
提供；芯片参数与内部 Flash 由 `soc/` 提供；控制器适配在 `platforms/stm32/`。
其他 STM32 family 的历史 Kconfig、时钟文件或接口声明不构成维护支持，
也不会自动引入另一套 SDK。增加家族必须同时提供锁定依赖、实际 SoC/Board
接入、控制器实现和对应验证。当前 STM32/GD32 硬件 typed I2C 未实现。

在仓库根初始化现有锁定依赖：

```sh
git submodule update --init --recursive ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
```

Gitlink 记录精确 commit；不要以更新到上游分支最新版本代替可评审的依赖
升级。配置不联网下载 SDK，缺失必要文件会拒绝构建。依赖升级需检查来源、
许可证、内容身份和实际编译、链接、设备契约回归。保留上游文件及版权通知，
板级接线与产品逻辑写在 Nexus 适配层或外部应用，不能修改导入 SDK 实现。

普通固件通过 `Nexus::Firmware` 和公开 HAL/OSAL 类型消费平台，厂商头与
宏只进入实现目标。需要直接使用 ST HAL 的专项 bring-up 应用必须显式链接
`Nexus::STM32SDK`；该入口不改变普通应用的 SDK 隔离边界。构建命令与
当前验证范围以[根 README](../../README.md)、[供应商依赖](../README.md)
和[维护支持矩阵](../../docs/strategy/support-matrix.yaml)为准。
