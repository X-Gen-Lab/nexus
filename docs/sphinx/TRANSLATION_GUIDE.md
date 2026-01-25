# Nexus 文档翻译指南

## 当前状态

- **总条目**: 6,796
- **已翻译**: 3,801 (55.9%)
- **待翻译**: 2,995 (44.1%)

## 翻译策略说明

### 已完成的自动化工作

1. ✅ **技术术语翻译** (65 条目)
   - 常用技术词汇已自动翻译
   - 使用扩展词典（500+ 术语）

2. ✅ **不需翻译内容标记** (319 条目)
   - 代码块、命令、常量等已标记为保持原文
   - 这是技术文档的标准做法

3. ✅ **核心文档翻译** (3,417 条目)
   - 用户指南核心部分已翻译
   - Shell、OSAL、HAL 等关键模块完成度高

### 剩余工作分析

待翻译的 2,995 个条目主要是：

1. **完整句子和段落** (~70%)
   - 详细的技术说明
   - 教程步骤描述
   - 示例代码解释
   
2. **复杂技术描述** (~20%)
   - 架构设计说明
   - 实现细节描述
   - 调试和故障排除指南

3. **平台特定内容** (~10%)
   - STM32、GD32 等平台详细配置
   - 特定硬件的使用说明

## 推荐的翻译方法

### 方法 1: 使用专业翻译工具 (推荐)

**Poedit** - 专业的 .po 文件编辑器

```bash
# 安装 Poedit
# Windows: https://poedit.net/download
# Linux: sudo apt install poedit
# macOS: brew install poedit

# 使用 Poedit 打开 .po 文件
poedit locale/zh_CN/LC_MESSAGES/user_guide/shell.po
```

**优点**:
- 图形界面，易于使用
- 支持翻译记忆
- 自动检查格式
- 可以导入/导出翻译

### 方法 2: 使用在线翻译服务

**DeepL** (推荐，质量最高)

```bash
# 1. 导出未翻译文本
python export_untranslated.py > untranslated.txt

# 2. 使用 DeepL 翻译
#    https://www.deepl.com/translator

# 3. 人工审核和调整

# 4. 导入翻译结果
python import_translations.py translated.txt
```

**Google Translate** (免费，速度快)

类似流程，但质量略低，需要更多人工审核。

### 方法 3: 分模块逐步翻译

按优先级翻译：

**高优先级** (用户最常访问):
```bash
# 1. 教程 (tutorials/)
poedit locale/zh_CN/LC_MESSAGES/tutorials/first_application.po
poedit locale/zh_CN/LC_MESSAGES/tutorials/gpio_control.po

# 2. 快速开始 (getting_started/)
poedit locale/zh_CN/LC_MESSAGES/getting_started/quick_start.po
poedit locale/zh_CN/LC_MESSAGES/getting_started/environment_setup.po

# 3. 用户指南 (user_guide/)
poedit locale/zh_CN/LC_MESSAGES/user_guide/build_system.po
poedit locale/zh_CN/LC_MESSAGES/user_guide/hal.po
```

**中优先级** (开发者参考):
```bash
# 4. API 参考 (api/)
poedit locale/zh_CN/LC_MESSAGES/api/hal.po
poedit locale/zh_CN/LC_MESSAGES/api/osal.po

# 5. 平台指南 (platform_guides/)
poedit locale/zh_CN/LC_MESSAGES/platform_guides/stm32f4.po
```

**低优先级** (高级内容):
```bash
# 6. 开发指南 (development/)
poedit locale/zh_CN/LC_MESSAGES/development/testing.po
poedit locale/zh_CN/LC_MESSAGES/development/contributing.po
```

### 方法 4: 团队协作翻译

使用 Git 分支管理：

```bash
# 1. 创建翻译分支
git checkout -b translate-tutorials

# 2. 翻译特定模块
poedit locale/zh_CN/LC_MESSAGES/tutorials/*.po

# 3. 提交翻译
git add locale/zh_CN/LC_MESSAGES/tutorials/
git commit -m "翻译教程模块"

# 4. 推送并创建 PR
git push origin translate-tutorials
```

## 翻译质量标准

### 必须遵守的规则

1. **保持技术术语一致**
   ```
   ✓ Hardware Abstraction Layer → 硬件抽象层 (HAL)
   ✗ Hardware Abstraction Layer → 硬件抽象层 / 硬件抽象接口 (不一致)
   ```

2. **代码和命令不翻译**
   ```
   ✓ 运行 `python build.py` 命令
   ✗ 运行 `python 构建.py` 命令
   ```

3. **保持格式标记**
   ```
   ✓ 这是 **重要** 的内容
   ✗ 这是 重要 的内容 (丢失了 ** 标记)
   ```

4. **保持占位符**
   ```
   ✓ 配置文件位于 `{path}/config.h`
   ✗ 配置文件位于 路径/config.h (丢失了 {path})
   ```

### 翻译风格指南

1. **使用"您"而不是"你"** (正式文档)
   ```
   ✓ 您可以使用以下命令...
   ✗ 你可以使用以下命令...
   ```

2. **保持简洁明了**
   ```
   ✓ 初始化系统
   ✗ 对系统进行初始化操作
   ```

3. **技术术语中英混用**
   ```
   ✓ GPIO 引脚配置
   ✓ 配置 GPIO (General Purpose Input/Output) 引脚
   ✗ 通用输入输出引脚配置 (过于冗长)
   ```

4. **保持原文的语气**
   ```
   原文: Note: This is important.
   ✓ 注意：这很重要。
   ✗ 请注意：这是非常非常重要的内容。(过度强调)
   ```

## 翻译工作流程

### 完整流程

```bash
# 1. 更新翻译模板
cd docs/sphinx
python build_docs.py --update-po

# 2. 翻译 .po 文件
# 使用 Poedit 或文本编辑器

# 3. 验证翻译
python build_docs.py --lang zh_CN

# 4. 检查构建结果
# 打开 _build/html/zh_CN/index.html

# 5. 提交更改
git add locale/zh_CN/LC_MESSAGES/
git commit -m "更新中文翻译"
```

### 快速测试单个文件

```bash
# 1. 翻译单个 .po 文件
poedit locale/zh_CN/LC_MESSAGES/user_guide/shell.po

# 2. 只构建中文文档
python build_docs.py --lang zh_CN

# 3. 查看结果
start _build/html/zh_CN/user_guide/shell.html
```

## 常见问题

### Q: 为什么有些内容没有翻译？

A: 技术文档中，以下内容通常保持英文：
- 代码示例
- API 函数名
- 配置项名称
- 命令行指令
- 技术缩写 (GPIO, UART, SPI 等)

这是国际标准做法，有助于：
- 保持技术准确性
- 便于查找文档
- 与代码保持一致

### Q: 如何处理不确定的翻译？

A: 使用 `fuzzy` 标记：

```po
#, fuzzy
msgid "This is uncertain"
msgstr "这个翻译不确定"
```

构建时会显示警告，提醒需要审核。

### Q: 翻译后文档构建失败？

A: 检查以下问题：
1. RST 格式标记是否完整
2. 代码块标记是否正确
3. 链接引用是否保持
4. 特殊字符是否转义

### Q: 如何保持翻译一致性？

A: 使用翻译记忆工具：
1. Poedit 自动记忆翻译
2. 创建术语表 (glossary)
3. 定期审核已翻译内容

## 翻译术语表

### 核心概念
| English | 中文 | 说明 |
|---------|------|------|
| Hardware Abstraction Layer | 硬件抽象层 (HAL) | 保留缩写 |
| OS Abstraction Layer | 操作系统抽象层 (OSAL) | 保留缩写 |
| Peripheral | 外设 | |
| Interrupt | 中断 | |
| Task | 任务 | |
| Thread | 线程 | |
| Mutex | 互斥锁 | |
| Semaphore | 信号量 | |
| Queue | 队列 | |

### 操作动词
| English | 中文 |
|---------|------|
| Initialize | 初始化 |
| Configure | 配置 |
| Enable | 启用 |
| Disable | 禁用 |
| Read | 读取 |
| Write | 写入 |
| Send | 发送 |
| Receive | 接收 |

### 状态描述
| English | 中文 |
|---------|------|
| Ready | 就绪 |
| Busy | 忙碌 |
| Idle | 空闲 |
| Active | 活动 |
| Enabled | 已启用 |
| Disabled | 已禁用 |

## 贡献翻译

欢迎贡献翻译！

### 提交翻译的步骤

1. **Fork 项目**
   ```bash
   # 在 GitHub 上 Fork 项目
   git clone https://github.com/YOUR_USERNAME/nexus.git
   cd nexus
   ```

2. **创建翻译分支**
   ```bash
   git checkout -b translate-module-name
   ```

3. **翻译文件**
   ```bash
   cd docs/sphinx
   poedit locale/zh_CN/LC_MESSAGES/path/to/file.po
   ```

4. **测试构建**
   ```bash
   python build_docs.py --lang zh_CN
   ```

5. **提交更改**
   ```bash
   git add locale/zh_CN/LC_MESSAGES/
   git commit -m "翻译: 模块名称"
   git push origin translate-module-name
   ```

6. **创建 Pull Request**
   - 在 GitHub 上创建 PR
   - 描述翻译的模块和范围
   - 等待审核

### 翻译审核标准

PR 审核会检查：
- ✅ 翻译准确性
- ✅ 术语一致性
- ✅ 格式完整性
- ✅ 构建成功
- ✅ 无警告错误

## 工具和资源

### 推荐工具

1. **Poedit** - .po 文件编辑器
   - https://poedit.net/

2. **DeepL** - 高质量翻译
   - https://www.deepl.com/

3. **Lokalize** - KDE 翻译工具
   - https://apps.kde.org/lokalize/

4. **Gtranslator** - GNOME 翻译工具
   - https://wiki.gnome.org/Apps/Gtranslator

### 参考资源

1. **Sphinx i18n 文档**
   - https://www.sphinx-doc.org/en/master/usage/advanced/intl.html

2. **GNU gettext 手册**
   - https://www.gnu.org/software/gettext/manual/

3. **技术文档翻译指南**
   - https://developers.google.com/style/translation

## 联系方式

如有翻译相关问题：
- 提交 Issue: https://github.com/nexus/nexus/issues
- 讨论区: https://github.com/nexus/nexus/discussions

---

**最后更新**: 2026-01-25  
**文档版本**: 1.0.0
