# Nexus 文档快速参考

## 🚀 快速开始

### 构建文档
```bash
cd docs/sphinx
python build_docs.py --clean    # 构建中英文双语文档
```

### 本地预览
```bash
cd docs/sphinx
python build_docs.py --serve    # 启动本地服务器
# 访问 http://localhost:8000
```

## 📚 文档位置

### 在线访问
- **语言选择**: `_build/html/index.html`
- **英文文档**: `_build/html/en/index.html`
- **中文文档**: `_build/html/zh_CN/index.html`

### Kconfig 文档
| 文档类型 | 英文 | 中文 |
|---------|------|------|
| 用户指南 | `en/user_guide/kconfig.html` | `zh_CN/user_guide/kconfig.html` |
| API 参考 | `en/api/kconfig_tools.html` | `zh_CN/api/kconfig_tools.html` |
| 开发指南 | `en/development/kconfig_guide.html` | `zh_CN/development/kconfig_guide.html` |

## 🔧 常用命令

### 构建命令
```bash
# 只构建英文
python build_docs.py --lang en

# 只构建中文
python build_docs.py --lang zh_CN

# 构建双语（推荐）
python build_docs.py --clean

# 包含 Doxygen API 文档
python build_docs.py --doxygen --clean
```

### 翻译命令
```bash
# 提取可翻译字符串
python build_docs.py --update-po

# 自动翻译常用术语
python translate_kconfig_docs.py

# 修复翻译问题
python fix_translations.py

# 重新构建中文文档
python build_docs.py --lang zh_CN
```

## 📝 翻译文件

### 位置
```
locale/zh_CN/LC_MESSAGES/
├── user_guide/kconfig.po
├── api/kconfig_tools.po
└── development/kconfig_guide.po
```

### 编辑翻译
1. 用文本编辑器打开 .po 文件
2. 找到 `msgstr ""` （空翻译）
3. 在引号中添加中文翻译
4. 保存文件
5. 运行 `python build_docs.py --lang zh_CN`

## ✅ 验证清单

- [x] 英文文档构建成功
- [x] 中文文档构建成功
- [x] 语言选择页面生成
- [x] 语言切换功能正常
- [x] 所有链接正常工作
- [x] 搜索功能正常
- [x] 代码高亮正常

## 📖 详细文档

- **构建指南**: `README_BUILD.md`
- **翻译指南**: `locale/README_zh.md`
- **完成报告**: `TRANSLATION_COMPLETE.md`
- **验证报告**: `I18N_VERIFICATION_REPORT.md`

## 🆘 常见问题

### Q: 构建失败怎么办？
A: 检查依赖是否安装：`pip install sphinx sphinx-intl breathe`

### Q: 翻译没有显示？
A: 确保 .po 文件使用 UTF-8 编码，msgstr 不为空

### Q: 如何添加新语言？
A: `python build_docs.py --init-po <语言代码>`

## 📊 当前状态

| 项目 | 状态 |
|------|------|
| 英文文档 | ✅ 完成 |
| 中文翻译 | ✅ 完成 |
| 构建系统 | ✅ 正常 |
| 语言切换 | ✅ 正常 |
| 文档质量 | ✅ 优秀 |

---

**最后更新**: 2026-01-18  
**文档版本**: 1.0.0  
**支持语言**: English, 中文
