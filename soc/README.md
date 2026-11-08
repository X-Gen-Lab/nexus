# SoC：STM32F4 系列支持包

当前生产入口只有 [STM32F4](stm32/stm32f4/README.md)。已迁入的实现限定 F407VE／ZG，
其他 F4 型号仍需精确事实、模式实现与验证，不能由目录名或 SDK 宏推定支持。

SoC 拥有启动、时钟、内存、IRQ／DMA、资源收条和控制器私有状态；公共产品接口使用
Nexus 类型，官方 HAL／LL、Handle 与 Device 类型保持私有。共享 IP 实现已归入
`stm32/stm32f4/drivers/ip/`，不会产生额外运行时转发。

芯片与路线数据由 JSON 维护，PCB 接线由 [Board](../boards/README.md) 声明。配置工具
集中在 `tools/configure/`；本目录不再包含 Python 生成插件。当前工具语言及尚待迁入
手写 C 的冷态装配流程见[当前架构](../docs/delivery/architecture.md)。

`vendors/` 原位保留全部来源，只有当前选择的依赖进入构建。Native 模型已迁入
[测试支撑](../tests/support/native/README.md)，必须显式启用测试 scope。

新增器件先维护精确型号、器件宏、封装、存储、启动与资源路线，再复用已验证硬件
profile；新增硬件行为写 C 和合同测试。配置成功、ARM 链接和物理 HIL 各自提供证据，
不互相替代。当前 HIL 为 `not_executed`，详见[支持范围](../docs/delivery/support.md)。
