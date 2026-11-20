# 文档工具

本目录核对当前正文、翻译、配置示例与架构图，并生成中英文离线站点。
统一入口为 `python tools/dev/dev.py docs`；准备、构建与写作步骤见
[文档维护](../../docs/development/documentation.md)，精确参数见
[工具参考](../../docs/reference/documentation-tools.md)。

| 模块 | 责任 |
|---|---|
| `cli.py` | 参数与暂存输入检查，诊断、构建、服务与测试入口 |
| `build.py` | 构建阶段、输入身份、缓存、语言隔离、受控输出和本次报告 |
| `publication.py` | 完整站点切换、已发布视图与最新尝试分离、HTTP 读取协调 |
| `check.py` | 当前页面、元数据、导航、引用、配置与翻译检查 |
| `install.py` | 锁定工具准备、离线核验与运行环境 |
| `sphinx_ext.py` | 语言视图、精确源码链接、搜索与页面状态 |
| `diagrams.py`、`diagram_localization.py` | DOT、同名标签与实际 SVG 资源 |
| `breathe_i18n.py` | 当前 Breathe API 节点的消息与源码位置 |
| `search_language.py` | 中文与 C 符号检索所需的分词 |
| `test_*.py` | 当前合同与实际故障路径回归 |

源码使用 Markdown/MyST；API 从公共头经 Doxygen XML 和 Breathe 呈现。
预览允许增量编辑，正式验证要求本次输入与完整译文。缓存只复用输入匹配的中间
产物，已有成功报告不能替代新执行。文档构建、固件验证与实板 HIL 各自有独立范围。
