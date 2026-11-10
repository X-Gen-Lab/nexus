# 文档工具：源码检查与严格双语构建

本目录检查当前正文、翻译、示例配置和架构图，使用锁定工具生成公共 API 与中英文
HTML。文档输出不建立固件或实板资格；历史报告保留在固定 Git revision。

## 模块职责

| 模块 | 职责 |
|---|---|
| `cli.py` | `check / doctor / test / build / preview` 统一入口和实际命令记录 |
| `check.py`、`rst.py` | 页面、引用、输入闭包、配置示例和翻译准入 |
| `install.py` | hash 锁定的 Python、Doxygen、Graphviz 与字体工具准备和核验 |
| `sphinx_ext.py` | 源语言、已审 catalog、精确 revision 链接和页面状态 |
| `diagrams.py`、`diagram_localization.py` | 可编辑 DOT、本地 SVG 与双语标签的一致性 |
| `breathe_i18n.py` | 真实公共头 API 消息的双语提取和渲染 |
| `test_*.py` | 当前解析、链接、配置、翻译、渲染和失败边界回归 |

## 日常使用

在仓库根执行，先按[构建配置](../../docs/sphinx/README.md)准备锁定工具，已有工具
不重复安装：

```sh
python tools/documentation/cli.py doctor --tool-prefix build/docs-tools
python tools/documentation/cli.py check --report build/docs-check.json
python tools/documentation/cli.py test --tool-prefix build/docs-tools \
    --report build/docs-tests.json
python tools/documentation/cli.py build --tool-prefix build/docs-tools \
    --language all --output build/documentation
python tools/documentation/cli.py preview --build build/documentation
```

构建实际执行源码与图检查、fresh Doxygen、gettext、精确消息集核对及严格双语 HTML。
成功要求 exit 0 与 `documentation-build.json` 的 passed 结果，旧 HTML 不代替本次执行。
离线 commit hook 核对完整 staged 文档和配置示例，不联网、不自动修改或 stage。

源码旁 README、站点正文和 include 闭包范围不同；检查不能代替说明内容与架构箭头的
语义审查。工具行为修改遵守实际 RED／GREEN，纯文档变更执行文档检查和内容审查。
文档、Source SDK 和 HIL 各有独立执行边界，见 [FIG-12](../../docs/assets/diagrams/release-recovery.svg)。
写作与图示遵守[内容标准](../../docs/design/documentation-standards.md)、
[页面模板](../../docs/design/documentation-templates.md)与[翻译合同](../../docs/locale/README.md)。
