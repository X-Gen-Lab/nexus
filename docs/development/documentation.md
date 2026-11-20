---
id: development.documentation
type: how-to
audience: maintainer
source_language: zh_CN
---

# 编写、翻译和验证文档

每页只维护一份权威正文。先确定读者要完成的事，再选择教程、任务指南、参考或
原理解释。内容与图示规范见[文档标准](../reference/contracts/documentation-standards.md)
和[页面模板](../reference/contracts/documentation-templates.md)。

## 编写正文

开头说明结果、前提和适用范围。命令写明工作目录、所需工具、完整输入、预期输出
及失败处理。当前实现、目标设计、软件参考和实际执行分别表达；历史记录用固定
源码链接读取，不把旧计数写进当前操作步骤。

每页 front matter 明确 `id`、`type`、`audience`、`source_language`。新教程引用维护的
真实 fixture 或固定外部消费者；源码拥有的输入优先 literalinclude，片段说明缺少
什么。普通文档构建不执行任意 Markdown 命令、不获取 latest、不操作硬件。

图示有明确范围、图注、替代文字和箭头语义。修改编辑源后更新本地 SVG，并检查
文字、交叉线、窄屏、缩放及离线显示。绘制流程见[图示参考](../reference/diagrams.md)。

## 准备工具与预览

正文根是 `docs/`，`docs/sphinx/` 只维护配置和模板。按
[工具参考](../reference/documentation-tools.md)显式准备一次锁定工具，然后从仓库根执行：

```sh
python tools/dev/dev.py docs check --report build/docs-check.json
build/docs-tools/venv/bin/python tools/dev/dev.py docs build \
  --tool-prefix build/docs-tools --mode preview --language zh_CN \
  --output build/documentation-preview
build/docs-tools/venv/bin/python tools/dev/dev.py docs serve \
  --build build/documentation-preview
```

预览为日常写作提供实际语言状态，不充当完整双语验收。commit 检查只检查完整 staged
文档输入及需要的配置，既不联网安装工具，也不自动 stage 修复。

## 翻译当前正文

`source_language` 表示 canonical 正文语言，不是读者选择的站点语言。相反语言采用
`docs/locale/<language>/LC_MESSAGES/<docname>.po`；同语言使用原文。代码符号、TOML 键、
状态、单位和原始证据值保持不变，不在译文中改变义务强度或借用结束条件。

从锁定工具本次实际提取的 POT 审查消息，不手写第二份消息清单。PO header 记录
`Language`、UTF-8、`X-Nexus-Source-SHA256` 和独立语义审查后的
`X-Nexus-Reviewed: true`。每个实际 msgid 恰好出现一次、译文非空、无 fuzzy，消息
集合与源摘要同时匹配。include 的 README 变化也属于输入，不能只改 header 装成新译文。

当前术语见[术语表](../locale/terminology.csv)。banner 区分 original、reviewed、
stale、missing；预览回退到原文时保留真实语言，旧 MO 不维持已撤销的审查状态。

## 完整验证

```sh
build/docs-tools/venv/bin/python tools/dev/dev.py docs build \
  --tool-prefix build/docs-tools --mode verify --language all \
  --output build/documentation
build/docs-tools/venv/bin/python tools/dev/dev.py docs test \
  --tool-prefix build/docs-tools --report build/docs-tests.json
```

verify 要求当前消息、公开 API、链接、图和两语言完整。按字节身份验证的阶段输入及
产物可复用，成功报告不能缓存为新验收。读取本次退出码与
`documentation-build.json`；旧 HTML 不挽救当前失败。

审查正文语义、示例和译文后，更新导航与删除入口的当前引用。文档来源、描述的源码、
已验证软件和实际硬件记录分别保持身份；本地 HTML 构建不代表部署或 MCU 验收。
