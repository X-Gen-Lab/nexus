# 文档维护入口

当前站点从 [任务导航](index.md)进入。平台生产范围只有 STM32F4，现有实现限定
F407VE／ZG 与四个独立 Board 包；Native 是显式测试支撑。全部 vendor 来源保留，
不由依赖库存推导生产支持。

| 阅读或维护任务 | 入口 |
| --- | --- |
| 审查架构与源码归属 | [当前架构](delivery/architecture.md)、[源码地图](explanation/source-map.md) |
| 选择芯片、板卡和模式 | [支持参考](reference/platforms/support.md)、[详细支持](delivery/support.md) |
| 修改合同与实现 | [设计导航](design/README.md)、[工程工作流](development/workflow.md) |
| 编写页面与图 | [内容标准](design/documentation-standards.md)、[模板](design/documentation-templates.md) |
| 核对软件与物理边界 | [交付入口](delivery/README.md)、[当前状态](status/index.md) |

正文、图源和中英文译文随源码一起维护。实际 gettext 消息集、源码摘要、语义审查、
本地链接和严格双语构建全部通过后才可交付。图可渲染不等于设计正确，文档构建也不
建立软件候选或物理 HIL。

原执行页、CSV 及派生视图保留原字节与身份。退役范围从
[固定重构前源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs)
追溯；当前站点不复写历史结果，也不继续维护退役芯片的独立当前页面。
