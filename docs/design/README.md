# Nexus 下一代设计

状态：**PROPOSED，待实现**。本目录从第一性原理定义下一代平台，不将现有API、目录、Kconfig、factory、OSAL或工作流视为必须保留的约束。用户已授权破坏性重构。

这里交付的是设计与执行计划。生产实现、现有支持矩阵和历史证据未因新增文档而改变。现状参见 [当前架构](../strategy/target-architecture.md) 和 [当前支持范围](../strategy/support-matrix.yaml)。

| 文档 | 作用 |
|---|---|
| [架构蓝图](next-generation-platform.md) | 原理、静态运行模型、设备/请求、并发、内存、性能与交付边界 |
| [工程手册](engineering-handbook.md) | 工具、风格、注释、代码评审、自动化、证据和发布流程 |
| [接入契约](integration-contracts.md) | SoC/Board/器件/组件/外部工程接入、资源校验与资格范围 |
| [执行清单](next-generation-execution.csv) | 分批任务、依赖、角色、软件/物理退出条件；全部初始为planned |

默认方向是编译期静态装配、固定typed端口、caller-owned请求、真实执行状态、按实例资源预算。配置采用SoC/Board/外部assembly三类输入，一份resolved结果；CMake仍是代码依赖权威。

首阶段针对STM32F407ZG启明欣欣V3.1、STM32F407VE天空星青春版和GD32F470ZG梁山派，裸机/FreeRTOS及Native模型。业务任务、产品策略与私有Board继续位于外部工程。硬件未接入，物理验证状态保持未执行。

阅读文档不等于接受所有设计决策。实施以垂直切片验证成本和契约，再逐批替换旧实现；新源码必须获得新的软件和产物证据。

执行清单的角色沿用十人配置：TL为技术负责人；K1/K2为core、IO和OS；B1/B2分别负责STM32/GD32与Board；I1/I2负责器件和通信；Q1/Q2负责软件QA与HIL；R1负责构建发布。尚未指派实际人员。

清单采用B0–B6批次，33项初始状态均为planned。多值字段以分号分隔；`primary_role`的首项承担任务关闭责任，其余项共同实现，`review_roles`列出独立审查所需领域。`legacy_rf_refs`仅用于定位已有需求和风险，不继承旧实现状态或验收结果。
