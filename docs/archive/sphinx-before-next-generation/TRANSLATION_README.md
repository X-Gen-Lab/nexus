# Nexus 文档翻译工具使用指南

## 🚀 快速开始

### 方法 1：使用交互式工具（推荐）

```bash
cd docs/sphinx
python translate.py
```

这将启动一个交互式菜单，提供所有翻译功能。

### 方法 2：直接运行命令

```bash
# 查看翻译统计
python translate_docs.py --stats

# 运行所有翻译工具
python final_batch_translate.py

# 构建中文文档
python build_docs.py --lang zh_CN
```

## 📁 目录结构

```
docs/sphinx/
├── translate.py                    # 主工具（交互式菜单）
├── translate_docs.py               # 翻译统计工具
├── final_batch_translate.py        # 批量运行所有翻译工具
├── build_docs.py                   # 文档构建工具
├── advanced_translate.py           # 高级翻译工具
├── complete_translation.py         # 完整术语翻译
│
└── translation_tools/              # 翻译工具目录
    ├── README.md                   # 详细说明文档
    ├── batch_translators/          # 批量翻译工具（14个）
    ├── utilities/                  # 辅助工具（3个）
    └── archived/                   # 已归档的旧脚本
```

## 📊 当前翻译进度

**完成度：67.0%** (4,553/6,796)

```
[█████████████████████████████████░░░░░░░░░░░░░░░] 67.0%
```

## 🛠️ 常用命令

### 查看统计
```bash
python translate_docs.py --stats
```

### 运行所有翻译工具
```bash
python final_batch_translate.py
```

### 运行特定翻译工具
```bash
# 翻译教程
python translation_tools/batch_translators/batch_translate_part1.py

# 翻译 Kconfig
python translation_tools/batch_translators/batch_translate_kconfig.py

# 翻译示例
python translation_tools/batch_translators/batch_translate_examples.py
```

### 分析和报告
```bash
# 分析未翻译内容
python translation_tools/utilities/analyze_untranslated.py

# 生成翻译报告
python translation_tools/utilities/generate_translation_report.py
```

### 构建文档
```bash
# 构建中文文档
python build_docs.py --lang zh_CN

# 构建所有语言
python build_docs.py --clean

# 启动本地服务器
python build_docs.py --serve
```

## 📚 详细文档

- `translation_tools/README.md` - 工具详细说明
- `TRANSLATION_GUIDE.md` - 完整翻译指南
- `../docs/TRANSLATION_FINAL_SUMMARY.md` - 翻译工作总结

## 💡 使用建议

### 首次使用

1. 查看当前进度：`python translate_docs.py --stats`
2. 运行所有工具：`python final_batch_translate.py`
3. 查看结果：`python translate_docs.py --stats`
4. 构建文档：`python build_docs.py --lang zh_CN`

### 针对性翻译

使用交互式工具选择特定类型的内容进行翻译：
```bash
python translate.py
# 选择 "7. 运行特定翻译工具"
```

### 手动翻译

对于自动工具无法处理的内容，推荐使用 Poedit：
```bash
poedit locale/zh_CN/LC_MESSAGES/tutorials/first_application.po
```

## ⚠️ 注意事项

1. 运行翻译工具前建议备份 .po 文件
2. 翻译后务必测试文档构建
3. 自动翻译需要人工审核
4. 保持术语翻译一致性

## 🎯 翻译目标

- **短期目标**：提升到 75% (约 700 条，15-20 小时)
- **中期目标**：提升到 85% (约 1,400 条，40-50 小时)
- **长期目标**：提升到 95%+ (约 2,000 条，60-80 小时)

---

**最后更新**: 2026-01-25  
**当前进度**: 67.0%  
**工具数量**: 23 个 (14 个第1阶段 + 5 个第2阶段 + 4 个辅助工具)
