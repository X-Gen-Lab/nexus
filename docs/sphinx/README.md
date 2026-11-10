# 文档构建配置入口

本目录维护 Sphinx 配置、模板和共享代码风格源。正文根是 `docs/`；直接构建
`docs/sphinx` 会得到不同且不完整的页面集合。先按[源码取得指南](../guides/source-checkout.md)
取得精确文档 revision，并初始化其锁定依赖。

## 准备与构建

以下命令在仓库根执行，需要可用的 CPython 3.12，并允许准备固定的 Linux x86_64
文档工具。使用新的工具 prefix 和构建输出目录：

```sh
python3.12 tools/documentation/install.py \
    --prefix build/docs-tools --cache build/docs-tool-cache
build/docs-tools/venv/bin/python tools/dev/dev.py docs doctor \
    --tool-prefix build/docs-tools
build/docs-tools/venv/bin/python tools/dev/dev.py docs check \
    --report build/docs-check.json
build/docs-tools/venv/bin/python tools/dev/dev.py docs build \
    --tool-prefix build/docs-tools --language all --output build/documentation
```

统一 CLI 生成新的公共头 Doxygen XML，提取 gettext 消息，核对已审 catalog，并在
warnings-as-errors 条件下构建中英文 HTML。图、链接、来源与本地资源共同接受检查。
成功要求实际 exit 0 和 `documentation-build.json` 为 passed；已有
HTML 不能挽救本次失败。清理前保存收条，仅移除自己创建且确认角色的输出。

## 架构与维护边界

正文、源码旁 README、API XML 和本地 DOT／SVG 共同组成站点输入；Sphinx 配置不拥有
平台能力、版本或资格的另一份定义。源码相邻架构入口见
[source-map](../explanation/source-map.md)，翻译规则见[locale](../locale/README.md)。

[文档工具参考](../reference/documentation-tools.md)维护工具身份、准备、离线安装、
权限和升级要求；[文档维护](../development/documentation.md)维护正文与语义审查。
文档构建成功不建立固件资格或开发板实物验收。

旧 HAL／OSAL／Kconfig 教程及翻译工具通过[固定历史 revision](../reference/history.md)
保留，不描述当前支持。现有格式与反斜杠 Doxygen 规则仍由
[代码标准](development/coding_standards.rst)维护。
