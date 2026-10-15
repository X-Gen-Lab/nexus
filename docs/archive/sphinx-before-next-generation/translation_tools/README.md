# Nexus 文档翻译工具

本目录包含所有用于 Nexus 文档翻译的工具和脚本。

## 📁 目录结构

```
translation_tools/
├── batch_translators/      # 批量翻译工具（14个）
│   ├── batch_translate_part1.py              # 教程和入门指南
│   ├── batch_translate_part2.py              # 构建系统和开发指南
│   ├── batch_translate_part3.py              # API 文档和配置说明
│   ├── batch_translate_part4.py              # 参数和配置项
│   ├── batch_translate_sentences.py          # 完整句子
│   ├── batch_translate_kconfig.py            # Kconfig 配置文档
│   ├── batch_translate_development.py        # 开发指南
│   ├── batch_translate_platforms.py          # 平台指南
│   ├── batch_translate_tutorials.py          # 教程
│   ├── batch_translate_comprehensive.py      # 综合翻译
│   ├── batch_translate_mega.py               # 超大型翻译
│   ├── batch_translate_common_patterns.py    # 常见模式
│   ├── batch_translate_script_validation.py  # 脚本验证系统
│   └── batch_translate_examples.py           # 示例和演示
│
├── utilities/              # 辅助工具（3个）
│   ├── analyze_untranslated.py               # 分析未翻译内容
│   ├── generate_translation_report.py        # 生成翻译报告
│   └── mark_no_translate.py                  # 标记不需翻译内容
│
└── archived/               # 已归档的旧脚本
    ├── auto_translate_po.py
    ├── comprehensive_translate.py
    ├── mark_notranslate.py
    └── translate_helper.py
```

## 🚀 快速开始

### 使用主工具（推荐）

```bash
cd docs/sphinx
python translate.py
```

这将启动交互式菜单，提供以下选项：
1. 查看翻译统计
2. 运行所有批量翻译工具
3. 分析未翻译内容
4. 生成翻译报告
5. 标记不需翻译的内容
6. 构建中文文档
7. 运行特定翻译工具

### 直接使用工具

#### 查看翻译统计
```bash
python translate_docs.py --stats
```

#### 运行所有翻译工具
```bash
python final_batch_translate.py
```

#### 运行特定翻译工具
```bash
# 翻译教程
python translation_tools/batch_translators/batch_translate_part1.py

# 翻译 Kconfig 文档
python translation_tools/batch_translators/batch_translate_kconfig.py

# 翻译示例
python translation_tools/batch_translators/batch_translate_examples.py
```

#### 分析未翻译内容
```bash
python translation_tools/utilities/analyze_untranslated.py
```

#### 生成翻译报告
```bash
python translation_tools/utilities/generate_translation_report.py
```

## 📊 工具说明

### 批量翻译工具

每个批量翻译工具都包含特定类型内容的翻译词典：

| 工具 | 用途 | 翻译内容 |
|------|------|---------|
| part1 | 教程和入门指南 | 教程句子、学习目标、步骤说明 |
| part2 | 构建系统和开发指南 | 构建配置、测试说明、CI/CD |
| part3 | API 文档和配置说明 | API 参考、配置选项、数据结构 |
| part4 | 参数和配置项 | 参数名称、配置描述、短语 |
| sentences | 完整句子 | 常见完整句子和段落 |
| kconfig | Kconfig 配置文档 | Kconfig 选项、配置说明 |
| development | 开发指南 | 开发流程、工具、最佳实践 |
| platforms | 平台指南 | 平台特性、硬件配置 |
| tutorials | 教程 | 教程步骤、示例、练习 |
| comprehensive | 综合翻译 | 平台描述、配置说明 |
| mega | 超大型翻译 | 完整句子、状态消息 |
| common_patterns | 常见模式 | 高频句子、常见表达 |
| script_validation | 脚本验证系统 | 验证系统相关内容 |
| examples | 示例和演示 | 示例应用、演示说明 |

### 辅助工具

| 工具 | 用途 |
|------|------|
| analyze_untranslated.py | 分析未翻译内容的类型和分布 |
| generate_translation_report.py | 生成详细的翻译进度报告 |
| mark_no_translate.py | 标记不需要翻译的内容（代码、命令等） |

## 📝 使用建议

### 首次翻译

1. 运行标记工具：
   ```bash
   python translation_tools/utilities/mark_no_translate.py
   ```

2. 运行所有批量翻译工具：
   ```bash
   python final_batch_translate.py
   ```

3. 查看翻译统计：
   ```bash
   python translate_docs.py --stats
   ```

### 针对性翻译

如果只想翻译特定类型的内容：

```bash
# 只翻译教程
python translation_tools/batch_translators/batch_translate_part1.py
python translation_tools/batch_translators/batch_translate_tutorials.py
python translation_tools/batch_translators/batch_translate_examples.py

# 只翻译配置文档
python translation_tools/batch_translators/batch_translate_kconfig.py
python translation_tools/batch_translators/batch_translate_part3.py
```

### 分析和报告

```bash
# 分析剩余未翻译内容
python translation_tools/utilities/analyze_untranslated.py

# 生成详细报告
python translation_tools/utilities/generate_translation_report.py
```

## 🔧 工具开发

### 添加新的翻译工具

1. 在 `batch_translators/` 目录创建新脚本
2. 使用现有工具作为模板
3. 添加翻译词典
4. 更新 `final_batch_translate.py` 中的脚本列表

### 翻译词典格式

```python
TRANSLATIONS = {
    "English text": "中文翻译",
    "Another text": "另一个翻译",
}
```

## 📚 相关文档

- `../TRANSLATION_GUIDE.md` - 完整翻译指南
- `../TRANSLATION_TOOLS_README.md` - 工具使用指南
- `../../docs/TRANSLATION_FINAL_SUMMARY.md` - 翻译工作总结

## ⚠️ 注意事项

1. 运行翻译工具前建议备份 .po 文件
2. 翻译后务必测试文档构建
3. 自动翻译需要人工审核
4. 保持术语翻译一致性

## 🐛 故障排除

### 问题：找不到模块

确保在 `docs/sphinx` 目录下运行脚本：
```bash
cd docs/sphinx
python translate.py
```

### 问题：翻译不生效

重新构建文档：
```bash
python build_docs.py --clean
python build_docs.py --lang zh_CN
```

---

**最后更新**: 2026-01-25  
**工具版本**: 2.0.0  
**工具数量**: 18 个
