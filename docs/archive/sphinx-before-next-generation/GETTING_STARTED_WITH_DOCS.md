# 📚 Nexus 文档系统快速上手指南

欢迎使用全新优化的 Nexus 文档系统！本指南将帮助你快速上手。

## 🎯 5 分钟快速开始

### 1. 安装依赖

```bash
# 进入文档目录
cd docs/sphinx

# 安装 Python 依赖
pip install -r requirements.txt
```

### 2. 构建文档

```bash
# 构建所有语言的文档
python build_docs.py

# 或者只构建英文
python build_docs.py --lang en

# 或者只构建中文
python build_docs.py --lang zh_CN
```

### 3. 查看文档

```bash
# 启动本地服务器
python build_docs.py --serve

# 在浏览器中打开
# http://localhost:8000
```

就这么简单！🎉

## 📖 文档结构

```
docs/sphinx/
├── 📄 index.rst                    # 主页
├── 📖 DOCUMENTATION_GUIDE.rst      # 完整导航指南
├── ⚡ QUICK_REFERENCE.rst          # 快速参考卡片
├── 📝 README.md                    # 构建系统文档
│
├── 📚 getting_started/             # 快速入门
├── 📖 user_guide/                  # 用户指南
├── 🎓 tutorials/                   # 教程
├── 🔧 platform_guides/             # 平台指南
├── 📋 api/                         # API 参考
├── 📚 reference/                   # 参考文档
├── 🛠️ development/                 # 开发指南
│
├── 🎨 _static/                     # 静态文件
│   └── custom.css                 # 自定义样式
├── 📄 _templates/                  # 模板
│   └── language_switcher.html     # 语言切换器
│
└── 🌍 locale/                      # 翻译文件
    └── zh_CN/                     # 中文翻译
        └── LC_MESSAGES/           # .po 文件
```

## 🌍 多语言支持

### 查看翻译统计

```bash
python translate_helper.py zh_CN --stats
```

输出示例：
```
============================================================
Translation Statistics for zh_CN
============================================================
Total strings:       500
Translated:          350 (70.0%)
Untranslated:        150 (30.0%)

Per-file breakdown:
------------------------------------------------------------
✓ user_guide/hal.po                                  45/45 (100.0%)
○ user_guide/osal.po                                 30/50 ( 60.0%)
✗ tutorials/gpio_control.po                          10/40 ( 25.0%)
```

### 自动翻译常用术语

```bash
python translate_helper.py zh_CN --auto-translate
```

这会自动翻译 70+ 常用技术术语，如：
- Hardware Abstraction Layer → 硬件抽象层
- Configuration → 配置
- Platform → 平台
- GPIO, UART, SPI, I2C 等

### 验证翻译质量

```bash
python translate_helper.py zh_CN --validate
```

检查：
- RST 标记是否完整
- 代码块是否被误翻译
- 格式是否正确

### 更新翻译文件

```bash
# 当英文文档更新后，运行此命令更新 .po 文件
python build_docs.py --update-po
```

### 手动编辑翻译

```bash
# 使用任何文本编辑器
notepad locale/zh_CN/LC_MESSAGES/user_guide/hal.po

# 或使用专业工具 Poedit（推荐）
poedit locale/zh_CN/LC_MESSAGES/user_guide/hal.po
```

## 🔧 常用命令

### 使用 Python 脚本（推荐）

```bash
# 构建所有语言
python build_docs.py

# 构建特定语言
python build_docs.py --lang en
python build_docs.py --lang zh_CN

# 清理并重建
python build_docs.py --clean

# 构建并启动服务器
python build_docs.py --serve

# 运行 Doxygen 后构建
python build_docs.py --doxygen

# 更新翻译文件
python build_docs.py --update-po

# 初始化新语言
python build_docs.py --init-po ja  # 日语
```

### 使用 Makefile

```bash
# 查看所有可用命令
make help

# 构建所有语言
make html-all

# 构建特定语言
make html          # 英文
make html-zh_CN    # 中文

# 启动服务器
make serve

# 完整构建（清理 + Doxygen + 构建）
make full

# 翻译相关
make stats         # 翻译统计
make validate      # 验证翻译
make auto-trans    # 自动翻译

# 其他
make clean         # 清理
make doxygen       # 运行 Doxygen
make linkcheck     # 检查链接
```

## 📝 编写文档

### 创建新文档

```bash
# 1. 创建 .rst 文件
touch user_guide/my_new_module.rst

# 2. 编辑文件
# 使用 reStructuredText 格式

# 3. 添加到目录树
# 编辑 user_guide/index.rst，在 toctree 中添加：
#   my_new_module
```

### RST 语法速查

**标题：**
```rst
主标题
======

二级标题
--------

三级标题
~~~~~~~~
```

**文本格式：**
```rst
**粗体**
*斜体*
``代码``
```

**代码块：**
```rst
.. code-block:: c

   int main(void) {
       return 0;
   }
```

**链接：**
```rst
:doc:`path/to/document`           # 内部文档
:ref:`label-name`                 # 内部引用
`外部链接 <https://example.com>`_  # 外部链接
```

**警告框：**
```rst
.. note::

   重要信息

.. warning::

   警告信息

.. tip::

   提示信息
```

更多语法请查看 :doc:`QUICK_REFERENCE`

## 🎨 自定义样式

编辑 `_static/custom.css` 来自定义文档外观：

```css
/* 修改主色调 */
a {
    color: #0366d6;  /* 链接颜色 */
}

/* 修改代码块样式 */
.highlight {
    border-radius: 6px;
    border: 1px solid #e1e4e8;
}

/* 修改警告框样式 */
.admonition.note {
    border-left-color: #0366d6;
    background: #f1f8ff;
}
```

## 🔍 查找信息

### 使用文档导航

1. **按角色查找**：
   - 新用户 → Getting Started
   - 应用开发者 → User Guide
   - 贡献者 → Development

2. **按任务查找**：
   - 环境设置 → getting_started/environment_setup
   - 使用 GPIO → tutorials/gpio_control
   - 配置系统 → user_guide/kconfig

3. **按主题查找**：
   - 架构 → user_guide/architecture
   - HAL → user_guide/hal
   - OSAL → user_guide/osal

### 使用搜索功能

在构建的文档中使用侧边栏的搜索框：
- 搜索关键词：`GPIO`, `UART`, `Kconfig`
- 搜索函数：`nx_factory_gpio`, `nx_hal_init`
- 搜索错误码：`NX_ERR_PARAM`

## 🐛 故障排除

### 构建失败

**问题：** 缺少依赖
```bash
# 解决：安装依赖
pip install -r requirements.txt
```

**问题：** Doxygen 未找到
```bash
# 解决：安装 Doxygen
# Windows: choco install doxygen
# Linux: sudo apt install doxygen
# macOS: brew install doxygen
```

### 翻译不显示

**问题：** 翻译文件未更新
```bash
# 解决：更新 .po 文件
python build_docs.py --update-po
python build_docs.py --lang zh_CN
```

### 链接失效

**问题：** 文档链接失效
```bash
# 解决：检查所有链接
make linkcheck
cat _build/linkcheck/output.txt
```

## 📚 学习资源

### 文档内资源

- **DOCUMENTATION_GUIDE.rst** - 完整的文档导航指南
- **QUICK_REFERENCE.rst** - 快速参考卡片
- **README.md** - 构建系统详细文档
- **locale/README_zh.md** - 翻译指南

### 外部资源

- [Sphinx 官方文档](https://www.sphinx-doc.org/)
- [reStructuredText 入门](https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html)
- [Sphinx i18n](https://www.sphinx-doc.org/en/master/usage/advanced/intl.html)
- [Doxygen 手册](https://www.doxygen.nl/manual/)

## 🤝 贡献文档

### 报告问题

在 [GitHub Issues](https://github.com/X-Gen-Lab/nexus/issues) 中报告：
- 文档错误
- 缺失的信息
- 改进建议

### 提交改进

1. Fork 项目
2. 创建分支：`git checkout -b docs/improve-hal-guide`
3. 修改文档
4. 构建验证：`python build_docs.py`
5. 提交：`git commit -m "docs: improve HAL guide"`
6. 推送：`git push origin docs/improve-hal-guide`
7. 创建 Pull Request

### 贡献翻译

1. 查看统计：`python translate_helper.py zh_CN --stats`
2. 选择未翻译的文件
3. 编辑 .po 文件
4. 验证：`python translate_helper.py zh_CN --validate`
5. 构建测试：`python build_docs.py --lang zh_CN`
6. 提交 Pull Request

## 💡 最佳实践

### 编写文档

- ✅ 使用清晰的标题层级
- ✅ 包含代码示例
- ✅ 添加交叉引用
- ✅ 使用警告框突出重要信息
- ✅ 保持段落简短

### 翻译文档

- ✅ 翻译内容，不翻译代码
- ✅ 保留 RST 标记
- ✅ 使用一致的术语
- ✅ 验证后再提交

### 维护文档

- ✅ 定期更新翻译
- ✅ 检查链接有效性
- ✅ 审查代码示例
- ✅ 保持依赖最新

## 🎉 下一步

现在你已经了解了文档系统的基础，可以：

1. **浏览文档**：`python build_docs.py --serve`
2. **查看导航指南**：阅读 DOCUMENTATION_GUIDE.rst
3. **尝试翻译**：`python translate_helper.py zh_CN --stats`
4. **贡献文档**：选择一个主题开始编写

## 📞 获取帮助

- **文档问题**：[GitHub Issues](https://github.com/X-Gen-Lab/nexus/issues)
- **讨论**：[GitHub Discussions](https://github.com/X-Gen-Lab/nexus/discussions)
- **快速参考**：查看 QUICK_REFERENCE.rst

---

**祝你使用愉快！** 🚀

如有任何问题，欢迎随时联系 Nexus Team。
