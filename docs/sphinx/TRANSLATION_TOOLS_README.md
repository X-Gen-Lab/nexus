# Nexus 文档翻译工具使用指南

本目录包含用于 Nexus 文档翻译的自动化工具集。

## 🚀 快速开始

### 一键翻译（推荐）
```bash
cd docs/sphinx
python final_batch_translate.py
```

这将自动运行所有翻译工具并生成报告。

## 📦 工具列表

### 批量翻译工具

#### 1. batch_translate_part1.py
**用途**: 翻译教程和入门指南  
**包含**: 教程句子、学习目标、步骤说明等  
```bash
python batch_translate_part1.py
```

#### 2. batch_translate_part2.py
**用途**: 翻译构建系统和开发指南  
**包含**: 构建配置、测试说明、CI/CD 等  
```bash
python batch_translate_part2.py
```

#### 3. batch_translate_part3.py
**用途**: 翻译 API 文档和配置说明  
**包含**: API 参考、配置选项、数据结构等  
```bash
python batch_translate_part3.py
```

#### 4. batch_translate_part4.py
**用途**: 翻译参数和配置项  
**包含**: 参数名称、配置描述、短语等  
```bash
python batch_translate_part4.py
```

#### 5. batch_translate_sentences.py
**用途**: 翻译完整句子  
**包含**: 常见完整句子和段落  
```bash
python batch_translate_sentences.py
```

#### 6. advanced_translate.py
**用途**: 高级短语翻译  
**包含**: 扩展的短语词典  
```bash
python advanced_translate.py
```

#### 7. mark_no_translate.py
**用途**: 标记不需要翻译的内容  
**包含**: 代码块、命令行、URL 等  
```bash
python mark_no_translate.py
```

### 分析和管理工具

#### 8. analyze_untranslated.py
**用途**: 分析未翻译内容的类型和分布  
```bash
python analyze_untranslated.py
```

**输出**:
- 按类型分类统计
- 按模块分布统计
- 翻译建议

#### 9. generate_translation_report.py
**用途**: 生成详细的翻译进度报告  
```bash
python generate_translation_report.py
```

**输出**: `docs/TRANSLATION_PROGRESS_REPORT.md`

#### 10. translate_docs.py
**用途**: 翻译统计和管理  
```bash
# 查看统计
python translate_docs.py --stats

# 自动翻译术语
python translate_docs.py --auto

# 处理单个文件
python translate_docs.py --file locale/zh_CN/LC_MESSAGES/user_guide/shell.po
```

#### 11. complete_translation.py
**用途**: 使用扩展词典翻译  
**包含**: 500+ 技术术语  
```bash
# 查看统计
python complete_translation.py --dry-run

# 执行翻译
python complete_translation.py
```

### 批量执行工具

#### final_batch_translate.py
**用途**: 一键运行所有翻译工具  
```bash
python final_batch_translate.py
```

**功能**:
- 运行所有翻译脚本
- 显示进度和统计
- 测试文档构建
- 生成最终报告

#### run_all_translations.py
**用途**: 批量执行翻译脚本  
```bash
python run_all_translations.py
```

## 📊 查看进度

### 查看翻译统计
```bash
python translate_docs.py --stats
```

### 生成详细报告
```bash
python generate_translation_report.py
```

### 分析未翻译内容
```bash
python analyze_untranslated.py
```

## 🔧 构建文档

### 构建中文文档
```bash
python build_docs.py --lang zh_CN
```

### 构建所有语言
```bash
python build_docs.py --clean
```

### 更新翻译模板
```bash
python build_docs.py --update-po
```

### 启动本地服务器
```bash
python build_docs.py --serve
```

## 📝 工作流程

### 标准翻译流程

1. **运行批量翻译**
   ```bash
   python final_batch_translate.py
   ```

2. **查看进度**
   ```bash
   python translate_docs.py --stats
   ```

3. **分析剩余内容**
   ```bash
   python analyze_untranslated.py
   ```

4. **构建测试**
   ```bash
   python build_docs.py --lang zh_CN
   ```

5. **生成报告**
   ```bash
   python generate_translation_report.py
   ```

### 针对性翻译流程

1. **分析特定模块**
   ```bash
   python analyze_untranslated.py | grep "tutorials"
   ```

2. **运行特定翻译工具**
   ```bash
   python batch_translate_part1.py  # 教程
   ```

3. **验证结果**
   ```bash
   python translate_docs.py --stats
   ```

## 🎯 翻译策略

### 自动翻译（快速）
使用批量翻译工具处理常见内容：
```bash
python final_batch_translate.py
```

**优点**: 快速、一致、自动化  
**适用**: 术语、短语、常见句子

### 手动翻译（精确）
使用 Poedit 等工具手动翻译：
```bash
poedit locale/zh_CN/LC_MESSAGES/tutorials/first_application.po
```

**优点**: 精确、灵活、质量高  
**适用**: 复杂句子、段落、特殊内容

### 混合翻译（推荐）
1. 先运行自动翻译工具
2. 再手动翻译剩余内容
3. 最后审核和润色

## 📚 相关文档

- **工作总结**: `docs/TRANSLATION_WORK_SUMMARY.md`
- **最终报告**: `docs/TRANSLATION_FINAL_REPORT.md`
- **进度报告**: `docs/TRANSLATION_PROGRESS_REPORT.md`
- **翻译指南**: `docs/sphinx/TRANSLATION_GUIDE.md`
- **完成总结**: `docs/TRANSLATION_COMPLETE_SUMMARY.md`

## ⚠️ 注意事项

1. **备份**: 翻译前建议备份 .po 文件
2. **测试**: 翻译后务必测试构建
3. **审核**: 自动翻译需要人工审核
4. **一致性**: 保持术语翻译一致
5. **格式**: 保持 RST 格式标记完整

## 🐛 故障排除

### 问题: 翻译后构建失败
**解决**: 检查 RST 格式标记是否完整
```bash
python build_docs.py --lang zh_CN 2>&1 | grep "warning"
```

### 问题: 翻译不生效
**解决**: 重新构建文档
```bash
python build_docs.py --clean
python build_docs.py --lang zh_CN
```

### 问题: 工具运行出错
**解决**: 检查 Python 版本和依赖
```bash
python --version  # 需要 3.7+
pip install -r requirements.txt
```

## 💡 提示

- 使用 `--dry-run` 参数预览翻译效果
- 使用 `--stats` 参数查看统计信息
- 使用 `--verbose` 参数查看详细日志
- 定期运行 `generate_translation_report.py` 跟踪进度

## 🤝 贡献

欢迎改进翻译工具！

1. 添加新的翻译词典
2. 改进翻译规则
3. 优化工具性能
4. 修复 bug

---

**最后更新**: 2026-01-25  
**工具版本**: 1.0.0  
**工具数量**: 11 个
