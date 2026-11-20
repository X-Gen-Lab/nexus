---
id: reference.documentation-tools
type: reference
audience: maintainer
source_language: zh_CN
---

# 文档工具与执行边界

文档只有一个工具入口 `tools/documentation/cli.py`，`tools/dev/dev.py docs` 调用它。
选定源码的工具锁、Python 哈希要求和许可记录共同确定实际环境，正文不另维护版本表。

## 工具来源与准备

`dependencies/docs-tools.lock.json` 维护目标约束、原生归档和许可；
`dependencies/docs-python-linux-cp312.txt` 是精确版本及 SHA-256 的完整 Python 闭包，
`docs/sphinx/requirements.txt` 只引用它。工具包括 Sphinx／MyST、Breathe／Doxygen、
Graphviz、polib 和锁定 CJK 字体。公开 API 来自当前公共头，不扫描私有 SDK 或测试实现。

正式工具 tuple 为 Linux x86_64、CPython 3.12。kernel、CPython／venv bootstrap、
`dpkg-deb` 和 POSIX shell 为明确主机前提，其实际值记录在安装收条中。
从平台根选择新的 prefix 与缓存目录：

```sh
python3.12 tools/documentation/install.py \
  --prefix build/docs-tools --cache build/docs-tool-cache
```

准备阶段允许 HTTPS 获取已锁定归档，逐项校验后才建立 prefix；Python 安装使用
`--no-index`、`--find-links` 与 `--require-hashes`。原生库和字体在本地解包，无需
sudo，不静默发现宿主的其他 Graphviz renderer。loader／库／font 与许可闭包都需匹配。

已准备缓存可离线恢复到另一个新 prefix：

```sh
python3.12 tools/documentation/install.py \
  --prefix build/docs-tools-restored --cache build/docs-tool-cache --offline
```

成功为退出零和新 prefix 的 `installation.json`。缺归档、变更字节、错误字体或
许可不完整必须拒绝；不覆盖已存在 prefix，不用在线下载静默修复损坏缓存。

## 入口与范围

| 命令 | 责任 |
| --- | --- |
| `docs doctor --tool-prefix <prefix>` | 核对工具、锁与许可；不安装或下载 |
| `docs check` | 本地源、链接、四元数据和所需配置输入检查 |
| `docs build --mode preview --language zh_CN` | 当前写作预览，保留缺译与原文回退状态 |
| `docs build --mode verify --language all` | 公开 API、当前消息与两语言完整性验证 |
| `docs test` | 运行文档工具的实际测试，拒绝空、跳过或陈旧结果 |
| `docs serve --build <output>` | 浏览最后完整发布的视图，标明最新构建状态 |

正文根固定为 `docs/`。生成 XML、POT、翻译 stage、语言 HTML 和收条放在拥有的输出
目录，不能写回作者 PO。配置、公共头、工具锁或翻译改变时按真实输入使阶段缓存失效。
公开头未改变时可复用验证过字节身份的 API 中间产物，不复用前次成功报告。

构建先在 `work/site` 完成候选，再通过短发布锁切换 `site`。HTTP 请求在同一锁下
取得完整视图，发送已打开的文件内容时不持发布锁。`documentation-build.json` 记录最新
构建尝试，`last-published.json` 记录最后完整发布的来源和文件身份。
重建或失败时，`serve` 可以继续显示上版，但页面会标明当前状态和上版提交；这不改变
本次失败的退出码，也不产生新的验证通过。首次构建未完成或发布身份不一致时拒绝服务。

## 浏览、搜索与许可

主题和搜索资源为本地文件。两语言使用各自索引；中文 tokenizer 无外部词典依赖，
查询可混合 C 标识符和汉字。语言切换只指向实际语言输出；搜索与正文展示当前来源、
模式和实际语言，不把 preview 或旧索引标成正式内容。

可选布局回归使用已有 Chromium／Playwright，脚本
`tools/documentation/check_browser_layout.py` 不安装浏览器。它只证明所执行页面、
浏览器和视口；完整站点或硬件验收分别记录。

每份归档保留 wheel、原生包、Doxygen 和字体许可；共享许可引用须在 prefix 内闭合。
工具升级先审查上游变化与许可，再更新唯一锁与要求，执行在线准备、独立离线恢复、
相应工具测试、双语验证和图形检查。工具版本号本身不是成功证据。

CI 使用只读仓库权限和同一入口；上传结果不表示已部署网站。具体写作与翻译流程见
[文档维护](../development/documentation.md)。
