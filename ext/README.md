# 通用外部依赖

本目录保存组织托管、精确选版的通用外部库，覆盖操作系统、协议栈、图形、存储和
宿主开发依赖。LVGL 与 CANopenNode 的上游源码分别位于 `lvgl/` 和 `canopennode/`。

| 目录 | 维护内容 |
| --- | --- |
| `ext/` | 外部通用库的上游源码与原始许可证 |
| [vendors/](../vendors/README.md) | CPU、SoC、具体硬件器件的官方支持及架构配套库 |
| [components/](../components/README.md) | Nexus 公共接口、适配、选定 profile、存储和执行合同 |

应用通过组件 target 和公共头消费选定能力。组件叶目录选择实际源码、私有编译要求
和端口；外部库自身的实现与许可证按来源保留。产品页面、对象字典内容、任务和恢复
策略由外部应用工程维护。

来源登记与精确版本由 `tools/maintenance/source_catalogue.py`、`.gitmodules` 和
`dependencies/` 中的锁文件管理。准备、来源校验、按需编译和 Source SDK 导出使用
同一选择闭包。目录存在只表示依赖库存，实际构建由所选后端和组件决定。

新增或升级依赖时，按[依赖维护](../docs/development/dependencies.md)核对官方来源、
组织获取地址、gitlink、文件摘要和许可，并验证受影响组件及 SDK 消费。
