# 当前设计与实施台账

当前代码已按第一性原理蓝图重构到统一物理目录。公共C11路径是静态装配、固定
typed I/O、caller-owned request；SDK和provider storage私有；应用产品工程独立。
这里的设计合同与 [当前交付](../delivery/README.md) 一起使用，历史文档不建立当前资格。

| 文档 | 用途 |
|---|---|
| [架构蓝图](next-generation-platform.md) | 原理、执行、所有权、性能/空间与支持边界 |
| [工程手册](engineering-handbook.md) | 既有格式/注释、工具、自动化、证据与发布 |
| [接入契约](integration-contracts.md) | SoC/Board/器件/组件/外部工程的严格资源合同 |
| [33项实施台账](next-generation-execution.csv) | 实现文件、证据入口、软件与物理状态分列 |
| [实际架构](../delivery/architecture.md) | 最终目录和依赖方向，旧factory/Kconfig迁移 |
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
