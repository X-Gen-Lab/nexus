# Shell Framework 文档索引

欢迎查阅 Nexus Shell Framework 的完整文档。

## 文档结构

### 📖 入门文档

- **[../README.md](../README.md)** - 模块概述和快速开始
  - 特性介绍
  - 快速开始示例
  - API 参考速查
  - 配置选项说明

### 🏗️ 架构文档

- **[DESIGN.md](DESIGN.md)** - 架构设计文档
  - 设计目标和核心特性
  - 系统架构（分层设计）
  - 模块职责说明
  - 核心数据结构
    - Shell 上下文结构
    - 命令注册表
    - 行编辑器状态
    - 历史管理器
    - 后端接口
  - 关键流程
    - 初始化流程
    - 输入处理流程
    - 命令解析流程
    - 命令执行流程
    - 转义序列处理
  - 命令注册机制
    - 静态注册
    - 动态注册
    - 命令查找
  - 行编辑器设计
    - 光标管理
    - 插入和删除
    - 显示刷新
  - 历史管理设计
    - 环形缓冲区
    - 历史浏览
    - 历史持久化
  - 自动补全设计
    - 命令补全
    - 参数补全
    - 公共前缀计算
  - 后端接口设计
    - 读写接口
    - UART 后端
    - Console 后端
    - 自定义后端
  - 线程安全设计
  - 内存管理策略
  - 性能优化方案
  - 设计权衡和未来改进方向

### 📚 使用指南

- **[USER_GUIDE.md](USER_GUIDE.md)** - 详细使用指南
  - 快速开始
    - 基本配置
    - 第一个命令
    - 编译和运行
  - 基本操作
    - 命令注册
    - 命令处理函数
    - 参数解析
    - 输出函数
  - 命令管理
    - 内置命令
    - 自定义命令
    - 命令查找
    - 命令注销
  - 行编辑功能
    - 光标移动
    - 字符编辑
    - 快捷键
    - 行清除
  - 历史管理
    - 历史浏览
    - 历史添加
    - 历史清除
    - 历史配置
  - 自动补全
    - 命令补全
    - 参数补全
    - 自定义补全函数
  - 后端配置
    - UART 后端
    - Console 后端
    - Mock 后端（测试用）
    - 自定义后端
  - 高级功能
    - 转义序列处理
    - ANSI 颜色输出
    - 多行输入
    - 脚本执行
  - 最佳实践
    - 命令设计原则
    - 错误处理
    - 性能优化
    - 内存使用优化
  - 常见问题

### 🧪 测试文档

- **[TEST_GUIDE.md](TEST_GUIDE.md)** - 测试文档
  - 测试策略
    - 测试层次（单元/集成/功能）
    - 测试目标
    - 覆盖率要求
  - 单元测试
    - 初始化测试
    - 命令注册测试
    - 行编辑器测试
    - 历史管理测试
    - 自动补全测试
    - 参数解析测试
  - 集成测试
    - 完整命令执行测试
    - 多命令交互测试
    - 后端集成测试
  - 功能测试
    - 用户交互测试
    - 转义序列测试
    - 边界条件测试
  - 性能测试
    - 命令执行延迟
    - 内存使用
    - 响应时间
  - Mock 后端测试
  - 测试工具和辅助函数
  - 持续集成测试
  - 测试最佳实践

### 📝 版本管理

- **[CHANGELOG.md](CHANGELOG.md)** - 版本变更记录
  - 版本历史
  - 新增功能
  - 变更说明
  - 问题修复
  - 性能改进
  - 已知限制
  - 升级指南
  - 贡献指南

### 🔧 移植指南

- **[PORTING_GUIDE.md](PORTING_GUIDE.md)** - 移植指南
  - 移植概述
    - 依赖项（OSAL、HAL）
    - 可移植性设计
    - 工作量评估
  - 平台适配
    - HAL UART 接口
    - 终端配置
    - 字符编码
  - 后端实现
    - 后端接口规范
    - UART 后端实现
    - Console 后端实现
    - 自定义后端开发
  - 编译配置
    - CMake 配置
    - Kconfig 配置
    - 编译宏定义
  - 移植步骤
    - 准备工作
    - 实现步骤
    - 验证清单
  - 平台特定优化
    - 内存优化
    - 响应时间优化
  - 故障排查
  - 示例项目

### 🔍 故障排查

- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化问题
    - 初始化失败
    - 后端配置失败
    - 内存分配失败
  - 输入输出问题
    - 无输入响应
    - 输出乱码
    - 回显错误
  - 命令执行问题
    - 命令未找到
    - 命令执行失败
    - 参数解析错误
  - 行编辑问题
    - 光标位置错误
    - 删除功能异常
    - 显示刷新问题
  - 历史功能问题
    - 历史记录丢失
    - 历史浏览异常
  - 自动补全问题
    - 补全无响应
    - 补全结果错误
  - 后端问题
    - UART 通信失败
    - 数据丢失
  - 调试技巧
    - 使用 Mock 后端
    - 日志输出
    - 单步调试
  - 常见错误码速查
  - 获取帮助

## 文档使用建议

### 🚀 新手入门路径

1. 阅读 [README.md](../README.md) 了解模块概述
2. 运行快速开始示例
3. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的基本操作部分
4. 查看 `applications/shell_demo/` 示例应用
5. 根据需要查阅其他文档

### 🏗️ 架构理解路径

1. 阅读 [DESIGN.md](DESIGN.md) 了解整体架构
2. 查看核心数据结构和关键流程
3. 理解各模块职责和接口设计
4. 学习命令注册和执行机制
5. 了解设计决策和权衡

### 🔧 移植开发路径

1. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)
2. 评估依赖项和工作量
3. 实现 HAL 接口
4. 实现或适配后端
5. 按步骤完成移植
6. 运行测试验证

### 🐛 问题排查路径

1. 查看 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. 根据症状查找对应章节
3. 按照诊断步骤排查
4. 应用解决方案
5. 如仍未解决，查看"获取帮助"部分

### 🧪 测试开发路径

1. 阅读 [TEST_GUIDE.md](TEST_GUIDE.md)
2. 了解测试策略和覆盖率要求
3. 参考测试示例编写测试
4. 运行测试并查看报告
5. 集成到 CI/CD 流程

### 🎯 命令开发路径

1. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的命令管理章节
2. 了解命令结构和处理函数
3. 参考内置命令实现
4. 实现自定义命令
5. 添加自动补全功能
6. 编写命令测试

## 模块概述

Shell Framework 提供功能丰富的交互式命令行界面（CLI），支持命令注册、行编辑、历史管理和自动补全。

### 核心特性

#### 1. 命令管理

灵活的命令注册和管理：

- ✅ 动态命令注册
- ✅ 命令查找和执行
- ✅ 帮助文本和用法说明
- ✅ 内置命令支持

**典型用例**:
```c
static int cmd_led(int argc, char* argv[]) {
    if (argc < 2) {
        shell_printf("Usage: led <on|off>\r\n");
        return -1;
    }
    /* 处理命令 */
    return 0;
}

static const shell_command_t led_cmd = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LED state",
    .usage = "led <on|off>"
};

shell_register_command(&led_cmd);
```

#### 2. 行编辑

强大的行编辑功能：

- ✅ 光标移动（左右、Home、End）
- ✅ 字符插入和删除
- ✅ 行清除（Ctrl+K、Ctrl+U）
- ✅ 单词删除（Ctrl+W）
- ✅ 显示刷新

**快捷键**:
- `←/→`: 光标移动
- `Home/End`: 行首/行尾
- `Backspace/Delete`: 删除字符
- `Ctrl+A/E`: 行首/行尾
- `Ctrl+K/U`: 删除到行尾/行首

#### 3. 历史管理

命令历史记录：

- ✅ 环形缓冲区存储
- ✅ 上下箭头浏览
- ✅ 可配置历史深度
- ✅ 历史命令重复执行

**典型用例**:
```c
shell_config_t config = {
    .history_depth = 16  /* 保存 16 条历史 */
};
```

#### 4. 自动补全

智能命令补全：

- ✅ Tab 键触发补全
- ✅ 命令名补全
- ✅ 参数补全（可选）
- ✅ 公共前缀自动完成
- ✅ 多匹配显示

**典型用例**:
```c
/* 输入 "le" 按 Tab */
nexus> le<Tab>
led

/* 输入 "h" 按 Tab，多个匹配 */
nexus> h<Tab>
help  history
nexus> h
```

#### 5. 后端支持

可插拔的 I/O 后端：

- ✅ UART 后端（串口）
- ✅ Console 后端（标准输入输出）
- ✅ Mock 后端（测试用）
- ✅ 自定义后端

**典型用例**:
```c
/* UART 后端 */
shell_set_backend(&shell_uart_backend);

/* 自定义后端 */
static const shell_backend_t my_backend = {
    .read = my_read,
    .write = my_write
};
shell_set_backend(&my_backend);
```

### 内置命令

| 命令 | 描述 | 用法 |
|------|------|------|
| `help` | 显示可用命令 | `help [command]` |
| `version` | 显示 Shell 版本 | `version` |
| `clear` | 清除终端屏幕 | `clear` |
| `history` | 显示命令历史 | `history` |
| `echo` | 打印参数 | `echo [text...]` |

### 性能指标

| 指标 | 值 |
|------|---|
| 命令执行延迟 | < 1 ms |
| 内存占用 | ~4KB（默认配置） |
| 最大命令数 | 32（可配置） |
| 最大参数数 | 8（可配置） |
| 历史深度 | 4-32（可配置） |

### 资源需求

| 资源 | 最小 | 推荐 |
|------|------|------|
| RAM | 2KB | 4KB |
| Flash | 8KB | 12KB |
| 栈空间 | 512B | 1KB |

## 文档维护

### 更新原则

- 代码变更时同步更新文档
- 保持文档与代码一致
- 及时记录已知问题和限制
- 更新版本变更记录

### 贡献指南

欢迎贡献文档改进：

1. 发现错误或不清晰的地方
2. 提交 Issue 或 Pull Request
3. 遵循文档格式规范
4. 提供清晰的示例代码

### 文档格式规范

- 使用 Markdown 格式
- 代码示例使用 C 语言
- 保持一致的标题层级
- 提供清晰的目录结构
- 使用表格和列表提高可读性
- 代码注释遵循 Nexus 注释规范

## 相关资源

### 示例代码

- `applications/shell_demo/` - 完整示例应用
- `tests/shell/` - 测试用例
- `framework/shell/src/` - 实现代码
- `framework/shell/include/shell/` - API 头文件

### 外部参考

- [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html) - 行编辑库
- [linenoise](https://github.com/antirez/linenoise) - 轻量级行编辑库
- [Zephyr Shell](https://docs.zephyrproject.org/latest/services/shell/index.html) - Zephyr RTOS Shell
- [FreeRTOS CLI](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_Command_Line_Interface.html) - FreeRTOS 命令行接口

## 支持的平台

### 编译器

| 编译器 | 版本 | 状态 |
|--------|------|------|
| GCC | ≥ 7.0 | ✅ 完全支持 |
| Clang | ≥ 8.0 | ✅ 完全支持 |
| MSVC | ≥ 2019 | ✅ 完全支持 |
| Arm Compiler 5 | ≥ 5.06 | ✅ 完全支持 |
| Arm Compiler 6 | ≥ 6.0 | ✅ 完全支持 |
| IAR | ≥ 8.0 | ✅ 完全支持 |

### 架构

| 架构 | 状态 |
|------|------|
| ARM Cortex-M | ✅ 完全支持 |
| ARM Cortex-A | ✅ 完全支持 |
| RISC-V | ✅ 完全支持 |
| x86/x64 | ✅ 完全支持 |

### RTOS

| RTOS | 状态 |
|------|------|
| FreeRTOS | ✅ 完全支持 |
| RT-Thread | ✅ 完全支持 |
| Zephyr | ✅ 完全支持 |
| 裸机 | ✅ 完全支持 |

## 反馈和支持

### 问题反馈

如果遇到问题，请按以下顺序查找解决方案：

1. **查看 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 常见问题和解决方案
2. **查看 [USER_GUIDE.md](USER_GUIDE.md)** - 使用指南相关章节
3. **查看 [DESIGN.md](DESIGN.md)** - 了解内部机制
4. **搜索已知问题** - 检查 GitHub Issues
5. **提交新问题** - 提供详细的复现步骤

### 联系方式

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus/shell/issues
- 💬 Discussions: https://github.com/nexus/shell/discussions
- 📖 Wiki: https://github.com/nexus/shell/wiki

---

**最后更新**: 2026-01-24  
**文档版本**: 1.0.0  
**模块版本**: 1.0.0  
**维护者**: Nexus Team

**许可证**: Copyright (c) 2026 Nexus Team
