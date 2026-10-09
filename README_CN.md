# Nexus 嵌入式平台

项目介绍、当前支持范围、构建命令、公共接口与验证边界以[根 README](README.md)
为权威入口。应用实例与产品实现位于独立的
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples) 仓库。

平台使用锁定的源码依赖，维护 STM32F407 与 GD32F470 接入。SDK、Kconfig
条目或历史文件存在不代表对应硬件能力已实现；当前两款 MCU 的 typed I2C
provider 返回不支持。实板启动、电气时序与断电资格需独立 HIL 证据。

普通应用通过 `Nexus::Firmware`、公开组件目标及 Nexus 类型消费平台。
厂商 SDK include 与宏保留在实现目标内，直接 SDK bring-up 应用必须显式
选择 `Nexus::STM32SDK` 或 `Nexus::GD32SDK`。应用入口、任务、调度、私有
协议、分区与产品策略由外部仓库负责，平台初始化不自动创建业务 worker。

依赖初始化与升级规则见[供应商来源](vendors/README.md)和
[STM32 维护依赖](vendors/st/README.md)。源码 SDK 使用方法见
[源码包文档](cmake/package/README.md)。
