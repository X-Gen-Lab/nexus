# 当前设计与实施台账

当前代码采用统一物理目录、TOML 静态装配、typed factory 和共享多态方法表。
公共 C11 face 只保存 readonly ops 与私有 context；每个实例拥有独立状态，request 和
block storage 由调用者提供。SDK 和 provider storage 私有，应用产品工程独立。
这里的设计合同与 [当前交付](../delivery/README.md) 一起使用，历史文档不建立当前资格。

| 文档 | 用途 |
|---|---|
| [架构蓝图](next-generation-platform.md) | 原理、执行、所有权、性能/空间与支持边界 |
| [工程手册](engineering-handbook.md) | 既有格式/注释、工具、自动化、证据与发布 |
| [接入契约](integration-contracts.md) | SoC/Board/器件/组件/外部工程的严格资源合同 |
| [33项实施台账](next-generation-execution.csv) | 实现文件、证据入口、软件与物理状态分列 |
| [本次 P0–P5 台账](factory-platform-execution.csv) | factory、TOML、TDD、多实例与 IRQ/DMA/block 实现 |
| [实际架构](../delivery/architecture.md) | 当前目录、公共 factory 和多态接口、依赖方向 |
| [当前资格边界](../delivery/qualification.md) | 开发执行、clean软件候选、实板pending的区别 |

台账保留B0–B6和原始退出条件，新增implementation_paths/evidence_paths、software_status、
hardware_status。implemented表示机制已经存在，不表示所有退出条件已passed；开发验证
不能替代clean-source SDK、正式离线双构建和候选封存。NG028/030的正式执行以新生成
证据更新，硬件未接入，全部适用物理项not_executed。Glob表示同一机制的明确目录族，
不代表递归发现或缺省构建；X-Gen-Lab/nexus-examples路径是独立外部仓库。

首发为启明STM32F407ZGT6、天空星STM32F407VET6和梁山派GD32F470ZGT6，裸机/
FreeRTOS及Native模型。三Board未审实物PCB事实仍标unknown；软件测试夹具不升格真实
Board路线。角色沿用十人配置：TL1、Core/I/O/OS2、BSP2、通用应用/通信2、QA/HIL2、
构建发布1；实际人员与branch protection另行配置。

行为变更采用 RED → GREEN → REFACTOR；主机 GoogleTest/GoogleMock 测试真实生产
provider、factory 与 owner，配置工具用 Python unittest。commit hook 和 CI 执行同一
门禁，生产 MCU target 不链接测试框架。构建期资源验证、实际寄存器模型、ARM ELF
预算与物理 HIL 分别建立证据，方法表开销由实际 linked image 测量。
