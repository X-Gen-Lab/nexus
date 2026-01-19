# Nexus 文档翻译指南

本目录包含 Nexus 文档的国际化翻译文件。

## 目录结构

```
locale/
└── zh_CN/                    # 中文（简体）翻译
    └── LC_MESSAGES/
        ├── api/              # API 参考翻译
        │   ├── kconfig_tools.po
        │   ├── hal.po
        │   ├── osal.po
        │   └── ...
        ├── user_guide/       # 用户指南翻译
        │   ├── kconfig.po
        │   ├── architecture.po
        │   └── ...
        └── development/      # 开发指南翻译
            ├── kconfig_guide.po
            ├── contributing.po
            └── ...
```

## 翻译文件格式

翻译文件使用 GNU gettext 的 .po 格式：

```po
#: ../../user_guide/kconfig.rst:2
msgid "Kconfig Configuration System"
msgstr "Kconfig 配置系统"

#: ../../user_guide/kconfig.rst:5
msgid "Overview"
msgstr "概述"
```

- `msgid`: 英文原文
- `msgstr`: 中文翻译
- `#:`: 源文件位置（用于追踪）

## 翻译工作流程

### 1. 提取可翻译字符串

当英文文档更新后，运行以下命令提取新的可翻译字符串：

```bash
cd docs/sphinx
python build_docs.py --update-po
```

这会：
- 从 .rst 文件提取所有可翻译文本到 .pot 文件
- 更新所有语言的 .po 文件
- 保留已有的翻译
- 标记新增和过时的字符串

### 2. 自动翻译常用术语

运行自动翻译脚本来翻译常见术语和标题：

```bash
cd docs/sphinx
python translate_kconfig_docs.py
```

这会自动翻译：
- 章节标题
- 技术术语
- 常用短语
- 平台名称

### 3. 手动翻译详细内容

使用文本编辑器或专业的 .po 编辑器（如 Poedit）编辑 .po 文件：

```bash
# 使用任何文本编辑器
notepad locale/zh_CN/LC_MESSAGES/user_guide/kconfig.po

# 或使用 Poedit（推荐）
poedit locale/zh_CN/LC_MESSAGES/user_guide/kconfig.po
```

**翻译提示：**
- 保持技术术语的一致性
- 不要翻译代码示例中的代码
- 保留 reStructuredText 标记（如 `**`, `:doc:`, 等）
- 保持原文的格式和结构

### 4. 构建翻译后的文档

翻译完成后，构建中文文档：

```bash
cd docs/sphinx
python build_docs.py --lang zh_CN
```

输出位置：`_build/html/zh_CN/`

### 5. 验证翻译

在浏览器中打开生成的 HTML 文件验证翻译：

```bash
cd docs/sphinx
python build_docs.py --serve
# 访问 http://localhost:8000/zh_CN/
```

## 翻译状态

### Kconfig 相关文档

| 文件 | 总字符串 | 已翻译 | 状态 |
|------|---------|--------|------|
| user_guide/kconfig.po | ~200 | 70 | 🟡 部分完成 |
| api/kconfig_tools.po | ~150 | 43 | 🟡 部分完成 |
| development/kconfig_guide.po | ~180 | 30 | 🟡 部分完成 |

### 其他文档

其他文档的翻译状态请参考各自的 .po 文件。

## 翻译规范

### 术语对照表

| English | 中文 |
|---------|------|
| Configuration | 配置 |
| Platform | 平台 |
| Chip | 芯片 |
| Peripheral | 外设 |
| Instance | 实例 |
| Backend | 后端 |
| Bare-metal | 裸机 |
| Validation | 验证 |
| Migration | 迁移 |
| Build System | 构建系统 |

### 代码和命令

- **不翻译**：命令、文件名、函数名、变量名
- **保留格式**：代码块、内联代码、命令行示例

示例：
```po
msgid "Run ``python build_docs.py`` to build documentation"
msgstr "运行 ``python build_docs.py`` 来构建文档"
```

### reStructuredText 标记

保留所有 RST 标记：

```po
msgid "See :doc:`user_guide/kconfig` for details"
msgstr "详见 :doc:`user_guide/kconfig`"

msgid "**Important**: Always validate configuration"
msgstr "**重要**：始终验证配置"
```

## 工具推荐

### Poedit
- 官网：https://poedit.net/
- 功能：可视化编辑、翻译记忆、术语表
- 支持：Windows, macOS, Linux

### VS Code 插件
- **gettext**: .po 文件语法高亮
- **i18n Ally**: 翻译管理和预览

## 贡献翻译

欢迎贡献翻译！请遵循以下步骤：

1. Fork 项目仓库
2. 更新 .po 文件中的翻译
3. 运行 `python build_docs.py --lang zh_CN` 验证
4. 提交 Pull Request

## 常见问题

### Q: 如何查看哪些字符串需要翻译？

A: 在 .po 文件中搜索 `msgstr ""`（空的翻译字符串）。

### Q: 翻译后构建失败怎么办？

A: 检查是否破坏了 RST 标记或代码格式。常见问题：
- 删除了 `:doc:`, `:ref:` 等标记
- 修改了代码块中的代码
- 破坏了列表或表格格式

### Q: 如何更新过时的翻译？

A: 运行 `python build_docs.py --update-po` 后，查找标记为 `#, fuzzy` 的条目，这些是需要更新的翻译。

### Q: 可以添加其他语言吗？

A: 可以！运行：
```bash
python build_docs.py --init-po ja  # 日语
python build_docs.py --init-po ko  # 韩语
```

## 联系方式

如有翻译相关问题，请：
- 提交 Issue：https://github.com/X-Gen-Lab/nexus/issues
- 参与讨论：https://github.com/X-Gen-Lab/nexus/discussions

---

感谢您为 Nexus 文档翻译做出的贡献！🎉
