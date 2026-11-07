# 文档工具：同源正文、双语构建与可恢复交付

本目录提供文档检查、锁定工具诊断、严格中英文构建、离线打包和恢复的统一入口。
正文根为 `docs/`，源码相邻 README 通过 include 复用；API 来自真实公共头的 Doxygen
XML，图来自登记的 DOT／SVG。工具不会生成平台支持、固件资格或实板结果。

## 输入与模块

| 模块 | 职责 |
|---|---|
| `cli.py` | `check / doctor / test / build / package / preview / verify / restore` 及实际执行收条 |
| `check.py`、`rst.py` | 页面与输入闭包、Markdown／RST 引用、示例配置、翻译准入 |
| `install.py` | 固定 Python／Doxygen／Graphviz／字体工具的准备与身份诊断 |
| `sphinx_ext.py` | 源语言、已审 gettext、精确 revision 链接与页面状态 |
| `diagrams.py`、`diagram_localization.py` | 图源／输出、SVG 安全和图语言核对 |
| `breathe_i18n.py` | 公共 API 消息提取与渲染的双语边界 |
| `status.py` | 从原台账派生可读视图，不另维护任务状态 |
| `delivery.py`、`results.py` | 不可变文档包、manifest、恢复与来源绑定 |
| `verify_publication.py` | 核对实际发布字节与远端观察，不从 workflow 存在推导部署成功 |
| `test_*.py` | 配置、翻译、链接、身份、失败与恢复的工具合同回归 |

站点独立正文、include 依赖和普通源码 README 是不同扫描范围。一次 `check` 不能
自动证明所有仓库 README 的语义正确；新增或改写入口仍要审查相对链接、真实命令、
借用和失败前提。图渲染成功也不能证明其箭头或实际硬件关系正确。

## 日常工作流

在仓库根执行；首次联网准备需要 CPython 3.12 和新工具 prefix，详见
[构建配置](../../docs/sphinx/README.md)。工具已经准备时不重复安装：

```sh
python tools/documentation/cli.py doctor --tool-prefix build/docs-tools
python tools/documentation/cli.py check --report build/docs-check.json
python tools/documentation/cli.py test --report build/docs-tests.json
python tools/documentation/cli.py build --tool-prefix build/docs-tools \
    --language all --output build/documentation
python tools/documentation/cli.py package --build build/documentation \
    --output ../nexus-documentation-packages
```

命令和参数以统一 CLI 为准；输出目录须由工具明确拥有，不覆盖源码或他人目录。完整
build 顺序为 source／diagram check、fresh Doxygen、实际 gettext、精确消息集合与
已审 PO、严格 English／中文 HTML、来源后验。错误和原始命令流保留，旧 HTML 不能
挽救失败，成功需实际 exit 0 与 passed report。`preview` 只服务实际已构建输出。

commit 的 offline docs hook 读取完整 staged 输入和 include 闭包，不联网安装工具，
不自动 stage／修正文档。正文变化需重新提取实际消息并审查翻译；不可只更新 PO
摘要、复制中文到英文 msgstr，或把 fuzzy／missing 记为 reviewed。

## 维护、恢复与资格

纯文档改写使用既有检查与独立内容审查；修改工具行为则先执行失败回归，GREEN 后
保留原始收条。测试非空、不跳过，完整工具测试使用维护的测试入口。`verify`／
`restore` 核对包身份与原成员，不执行归档脚本，不建立新的固件软件候选。

编写规则见[内容与图示标准](../../docs/design/documentation-standards.md)、
[页面模板](../../docs/design/documentation-templates.md)、
[翻译合同](../../docs/locale/README.md)和[工具参数参考](../../docs/reference/documentation-tools.md)。
文档包流程见[FIG-12](../../docs/assets/diagrams/release-recovery.svg)：这是交付参考，
实际发布、下载和恢复需要各自原始证据，不能据此声称已经发生。
