# 同源正文与 gettext 翻译

每页只维护一份 Markdown／RST 正文，`source_language` 表示原文语言，不是读者选择的
站点语言。源码旁 README 以中文组织架构；被站点包含的正文也由同一管线提取翻译。
C 符号、TOML 键、状态常量、CSV／JSON 收条和原始证据值保持不变。

## 正文、POT 与 PO 的关系

在仓库根运行 `tools/documentation/cli.py build --language all`，锁定工具先提取各页
实际 POT 消息，再构建 `en` 和 `zh_CN`。命令收条记录真实提取目录。维护者根据该次
POT 审查翻译，不另建手写消息清单，也不使用旧 POT 冒充当前提取。

PO 位于 `<language>/LC_MESSAGES/<docname>.po`，header 包括：

- `Language: en` 或 `Language: zh_CN`；
- `Content-Type: text/plain; charset=UTF-8`；
- `X-Nexus-Source-SHA256`：精确 canonical source 字节的 SHA-256；
- `X-Nexus-Reviewed: true`：仅在完成独立语义审查后声明。

每个非 header 的提取 `msgid` 必须恰好出现一次，并有非空 `msgstr`。PO 消息集合必须
与实际 POT 一致，不含 fuzzy 项，source digest 与原文对应。被 include 的 README
变化也会改变完整消息集合；只改 header 摘要不能使旧翻译合格。构建期间的 reviewed
catalog stage 属于输出，不修改手写 PO，也不把生成 MO 提交到仓库。

## 页面状态与严格站点准入

页面 banner 显示 `original`、`reviewed`、`stale` 或 `missing`。`original` 表示实际
使用原文语言；源变化或 fuzzy 为 `stale`；缺失、不完整、未审或不支持的 catalog 为
`missing`。扩展不会加载 stale／missing catalog，单页预览可以显示带真实状态的原文。
每次构建清理自有 catalog stage，旧 MO 不能保留已撤销的审查状态。

内容区域的 HTML `lang` 表示实际语言，选择 locale 不会把原文伪装成译文。当前完整
中英文站点要求所有当前页面的提取消息通过审查与完整性门禁；预览的回退机制不放宽
正式双语准入。历史资料保留自己的冻结语言范围。

首页、SDK／异步教程、装配、factory、所有权、支持、资格、迁移、Board 接入和 OS
停机是重要阅读路径。维护术语见 [terminology.csv](terminology.csv)，精确规范链接
到各行的权威合同，不在译文中改变义务强度、单位、借用结束条件或支持范围。

## 验证与输出

扩展测试使用真实 gettext 和两种 HTML locale，覆盖中文译英文、英文译中文、原文
变化、fuzzy／不完整审查和缺译回退。单语言预览只声明真实构建的语言；完整 CLI 设置
`NEXUS_DOCS_LANGUAGES=en,zh_CN`，拒绝未知、重复或不含请求语言的选择，语言切换
只指向实际输出。翻译与文档构建不会重新建立固件资格或实板 HIL。
