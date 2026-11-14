# SoC：STM32F4 系列支持包

当前生产入口只有 [STM32F4](stm32/stm32f4/README.md)。精确事实覆盖八个料号、
七个器件视图；四个 F407 Board 与其他软件参考分别声明接线和模式。事实、源码、
ARM 链接与物理资格分开，不能由系列目录名或 SDK 宏推定全部支持。

SoC 拥有启动、时钟、内存、IRQ／DMA、资源收条和控制器私有状态；公共产品接口使用
Nexus 类型，官方 HAL／LL、Handle 与 Device 类型保持私有。共享 IP 实现已归入
`stm32/stm32f4/drivers/ip/`，不会产生额外运行时转发。

芯片与路线数据由 JSON 维护，PCB 接线由 [Board](../boards/README.md) 声明。配置工具
集中在 `tools/configure/`；本目录不包含 Python 生成插件。硬件算法和生命周期由
手写 C 维护，主机工具负责静态装配。接入重构目标与实际任务状态见
[当前架构](../docs/delivery/architecture.md)。

`vendors/` 原位保留全部来源，只有当前选择的依赖进入构建。Native 模型已迁入
[测试支撑](../tests/support/native/README.md)，必须显式启用测试 scope。

新增器件先维护精确型号、器件宏、封装、存储、启动与资源路线，再复用已验证硬件
profile；新增硬件行为写 C 和合同测试。配置成功、ARM 链接和物理 HIL 各自提供证据，
不互相替代。当前 HIL 为 `not_executed`，详见[支持范围](../docs/delivery/support.md)。
