# Translation Guide / 翻译指南

This document explains how to contribute translations to the Nexus documentation using Sphinx's official i18n (internationalization) mechanism.

本文档说明如何使用 Sphinx 官方的 i18n（国际化）机制为 Nexus 文档贡献翻译。

## Overview / 概述

The documentation uses **gettext** for translations:
- Source documents are written in English (`.rst` files)
- Translations are stored in `.po` files under `locale/<lang>/LC_MESSAGES/`
- Sphinx builds localized documentation by merging source with translations

文档使用 **gettext** 进行翻译：
- 源文档使用英文编写（`.rst` 文件）
- 翻译存储在 `locale/<lang>/LC_MESSAGES/` 下的 `.po` 文件中
- Sphinx 通过合并源文件和翻译来构建本地化文档

## Directory Structure / 目录结构

```
docs/sphinx/
├── conf.py                 # Sphinx configuration
├── index.rst               # Source document (English)
├── getting_started/        # Source documents
├── user_guide/             # Source documents
├── locale/                 # Translation files
│   └── zh_CN/              # Chinese translations
│       └── LC_MESSAGES/
│           ├── index.po    # Translation for index.rst
│           ├── sphinx.po   # Sphinx UI translations
│           └── getting_started/
│               └── introduction.po
└── _build/
    ├── gettext/            # Extracted .pot files
    └── html/
        ├── en/             # English HTML output
        └── zh_CN/          # Chinese HTML output
```

## Translation Workflow / 翻译工作流程

### 1. Update Translation Files / 更新翻译文件

When source documents change, update the `.po` files:

当源文档更改时，更新 `.po` 文件：

```bash
# Using Python script
python build_docs.py --update-po

# Or using Make
make update-po
```

### 2. Edit Translation Files / 编辑翻译文件

Edit the `.po` files in `locale/zh_CN/LC_MESSAGES/`. Each entry has:

编辑 `locale/zh_CN/LC_MESSAGES/` 中的 `.po` 文件。每个条目包含：

```po
#: ../../index.rst:50
msgid "Features"
msgstr "功能特性"
```

- `#:` - Source file location (reference)
- `msgid` - Original English text (DO NOT MODIFY)
- `msgstr` - Your translation (EDIT THIS)

### 3. Build and Verify / 构建并验证

Build the translated documentation to verify:

构建翻译后的文档进行验证：

```bash
# Build Chinese only
python build_docs.py --lang zh_CN

# Build all languages
python build_docs.py

# Build and serve locally
python build_docs.py --serve
```

## Adding a New Language / 添加新语言

To add support for a new language (e.g., Japanese):

添加新语言支持（例如日语）：

```bash
# Initialize translation files
python build_docs.py --init-po ja

# Or using Make
make init-po LANG=ja
```

Then:
1. Edit `conf.py` to add the language to `html_context['languages']`
2. Update `build_docs.py` to add the language to `LANGUAGES`
3. Translate the `.po` files in `locale/ja/LC_MESSAGES/`

## Translation Tips / 翻译技巧

### Preserve RST Markup / 保留 RST 标记

Keep RST syntax intact in translations:

在翻译中保持 RST 语法完整：

```po
# Correct ✓
msgid "See :doc:`installation` for details."
msgstr "详情请参阅 :doc:`installation`。"

# Wrong ✗
msgid "See :doc:`installation` for details."
msgstr "详情请参阅安装指南。"  # Lost the cross-reference!
```

### Handle Placeholders / 处理占位符

Keep placeholders like `%s`, `{0}`, etc.:

保留 `%s`、`{0}` 等占位符：

```po
msgid "Found %s results"
msgstr "找到 %s 个结果"
```

### Fuzzy Translations / 模糊翻译

When source text changes, translations may be marked as "fuzzy":

当源文本更改时，翻译可能被标记为"模糊"：

```po
#, fuzzy
msgid "Updated text here"
msgstr "旧的翻译"
```

Review and update the translation, then remove the `#, fuzzy` line.

### Empty Translations / 空翻译

Empty `msgstr` means the original English will be used:

空的 `msgstr` 表示将使用原始英文：

```po
msgid "Some text"
msgstr ""  # Will show "Some text" in output
```

## Tools / 工具

### Recommended Editors / 推荐编辑器

- **Poedit** - GUI editor for .po files (https://poedit.net/)
- **Lokalize** - KDE translation tool
- **VS Code** with gettext extension

### Command Reference / 命令参考

```bash
# Extract messages (create/update .pot files)
python build_docs.py --update-po

# Build specific language
python build_docs.py --lang zh_CN

# Build all languages
python build_docs.py

# Clean and rebuild
python build_docs.py --clean

# Serve locally
python build_docs.py --serve --port 8080
```

## Supported Languages / 支持的语言

| Code    | Language | Status      |
|---------|----------|-------------|
| en      | English  | Source      |
| zh_CN   | 简体中文  | In Progress |

## Contributing / 贡献

1. Fork the repository
2. Update/add translations in `locale/<lang>/LC_MESSAGES/`
3. Build and test locally
4. Submit a pull request

Thank you for helping make Nexus documentation accessible to more developers!

感谢您帮助让更多开发者能够访问 Nexus 文档！
