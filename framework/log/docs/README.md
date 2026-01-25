# Log Framework 文档索引

欢迎查阅 Nexus Log Framework 的完整文档。

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
    - 日志实例结构
    - 后端接口结构
    - 消息队列结构
    - 格式化缓冲区
  - 关键流程
    - 初始化流程
    - 同步日志流程
    - 异步日志流程
    - 后端输出流程
    - 格式化流程
  - 后端接口设计
    - 后端注册机制
    - 后端生命周期
    - 后端优先级
  - 线程安全设计
    - 互斥锁保护
    - 无锁队列
    - 原子操作
  - 内存管理策略
    - 静态分配
    - 动态分配
    - 零拷贝优化
  - 性能优化方案
    - 异步输出
    - 批量刷新
    - 格式化缓存
    - 编译期优化
  - 设计权衡和未来改进方向

### 📚 使用指南

- **[USER_GUIDE.md](USER_GUIDE.md)** - 详细使用指南
  - 快速开始
    - 基本配置
    - 第一个日志
    - 编译和运行
  - 基本操作
    - 日志级别（TRACE/DEBUG/INFO/WARN/ERROR/FATAL）
    - 日志宏使用
    - 模块标签
    - 格式化输出
  - 后端管理
    - Console 后端
    - UART 后端
    - Memory 后端
    - File 后端
    - 自定义后端
    - 后端使能/禁用
    - 后端优先级
  - 日志过滤
    - 全局级别过滤
    - 模块级别过滤
    - 后端级别过滤
    - 运行时动态调整
  - 格式化配置
    - 时间戳格式
    - 颜色输出
    - 文件名和行号
    - 函数名
    - 自定义格式模板
  - 异步模式
    - 异步队列配置
    - 异步刷新策略
    - 溢出处理
    - 性能优化
  - 高级功能
    - 日志重定向
    - 日志归档
    - 日志压缩
    - 远程日志
    - 日志统计
  - 最佳实践
    - 日志级别选择
    - 性能优化建议
    - 内存使用优化
    - 线程安全注意事项
    - 错误处理
  - 常见问题

### 🧪 测试文档

- **[TEST_GUIDE.md](TEST_GUIDE.md)** - 测试文档
  - 测试策略
    - 测试层次（单元/集成/性能/线程安全）
    - 测试目标
    - 覆盖率要求（≥90% 行覆盖率，≥85% 分支覆盖率）
  - 单元测试
    - 初始化测试
    - 基本日志操作测试
    - 后端管理测试
    - 级别过滤测试
    - 格式化测试
    - 边界条件测试
    - 错误处理测试
  - 集成测试
    - 多后端协同测试
    - 异步模式测试
    - 运行时配置测试
    - 模块过滤测试
    - 格式化令牌测试
  - 性能测试
    - 吞吐量测试（>1M logs/sec）
    - 延迟测试
    - 内存使用测试
    - CPU 占用测试
    - 压力测试
  - 线程安全测试
    - 并发写入测试
    - 并发配置测试
    - 后端并发测试
    - 竞态条件测试
  - 属性测试（Property-Based Testing）
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
    - OSAL 互斥锁接口
    - OSAL 线程接口
    - HAL UART 接口
    - HAL 文件系统接口
    - 时间戳获取
  - 后端实现
    - 后端接口规范
    - Console 后端实现
    - UART 后端实现
    - File 后端实现
    - 自定义后端开发
  - 编译配置
    - CMake 配置
    - Kconfig 配置
    - 编译宏定义
    - 链接器配置
  - 移植步骤
    - 准备工作
    - 实现步骤
    - 验证清单
    - 性能调优
  - 平台特定优化
    - 内存优化
    - 性能优化
    - 功耗优化
  - 故障排查
  - 示例项目

### 🔍 故障排查

- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化问题
    - 初始化失败
    - 后端注册失败
    - 内存分配失败
  - 日志输出问题
    - 日志不输出
    - 日志丢失
    - 日志乱码
    - 日志格式错误
  - 性能问题
    - 日志输出慢
    - CPU 占用高
    - 内存占用高
    - 异步队列溢出
  - 后端问题
    - 后端无输出
    - 后端输出错误
    - 多后端冲突
  - 线程安全问题
    - 并发崩溃
    - 数据竞争
    - 死锁
  - 编译和链接问题
  - 调试技巧
    - 使能调试日志
    - 使用内存后端
    - 性能分析工具
    - 日志统计分析
  - 常见错误码速查
  - 获取帮助

## 文档使用建议

### 🚀 新手入门路径

1. 阅读 [README.md](../README.md) 了解模块概述
2. 运行快速开始示例
3. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的基本操作部分
4. 查看 `applications/log_demo/` 示例应用（如果有）
5. 根据需要查阅其他文档

### 🏗️ 架构理解路径

1. 阅读 [DESIGN.md](DESIGN.md) 了解整体架构
2. 查看核心数据结构和关键流程
3. 理解各模块职责和后端接口设计
4. 学习线程安全和性能优化设计
5. 了解设计决策和权衡

### 🔧 移植开发路径

1. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)
2. 评估依赖项和工作量
3. 实现 OSAL 和 HAL 接口
4. 实现或适配后端
5. 按步骤完成移植
6. 运行测试验证
7. 性能调优

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
4. 运行测试并查看报告（136 个测试，100% 通过率）
5. 集成到 CI/CD 流程

### 🎯 后端开发路径

1. 阅读 [DESIGN.md](DESIGN.md) 的后端接口设计章节
2. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md) 的后端实现章节
3. 参考现有后端实现（Console、UART、Memory）
4. 实现自定义后端
5. 编写后端测试
6. 性能测试和优化

## 模块概述

Log Framework 提供企业级的日志记录功能，支持多后端、多级别、异步输出和灵活的格式化。

### 核心特性

#### 1. 多级别日志

支持 6 个标准日志级别：

- **TRACE**: 最详细的跟踪信息
- **DEBUG**: 调试信息
- **INFO**: 一般信息
- **WARN**: 警告信息
- **ERROR**: 错误信息
- **FATAL**: 致命错误

**典型用例**:
```c
LOG_INFO("System", "Application started");
LOG_ERROR("Network", "Connection failed: %d", error_code);
```

#### 2. 多后端支持

支持同时输出到多个后端：

- ✅ Console 后端（标准输出）
- ✅ UART 后端（串口输出）
- ✅ Memory 后端（内存缓冲）
- ✅ File 后端（文件系统）
- ✅ 自定义后端（用户扩展）

**核心特性**:
- 后端独立使能/禁用
- 后端优先级控制
- 后端级别过滤
- 零拷贝优化

#### 3. 异步输出

高性能异步日志输出：

- ✅ 无锁环形队列
- ✅ 后台线程刷新
- ✅ 溢出策略可配置
- ✅ 性能超过 1M logs/sec

**典型用例**:
```c
log_set_async_mode(true);
LOG_INFO("App", "This is async log");  /* 非阻塞返回 */
```

#### 4. 灵活格式化

强大的格式化功能：

- ✅ 时间戳（绝对/相对）
- ✅ 颜色输出（ANSI）
- ✅ 文件名和行号
- ✅ 函数名
- ✅ 模块标签
- ✅ 自定义格式模板

**格式化令牌**:
```
%T - 时间戳
%L - 级别
%M - 模块
%F - 文件名
%l - 行号
%f - 函数名
%m - 消息
```

#### 5. 运行时配置

支持运行时动态配置：

- ✅ 全局级别调整
- ✅ 模块级别过滤
- ✅ 后端使能控制
- ✅ 格式模板切换

### 性能指标

基于 `tests/log/` 的测试结果：

| 指标 | 同步模式 | 异步模式 |
|------|----------|----------|
| 吞吐量 | ~50K logs/sec | >1.15M logs/sec |
| 平均延迟 | ~20 μs | <1 μs |
| 内存占用 | ~2KB | ~16KB |
| CPU 占用 | ~5% | ~2% |

### 测试覆盖率

- ✅ 136 个测试用例
- ✅ 100% 通过率
- ✅ ≥90% 行覆盖率
- ✅ ≥85% 分支覆盖率
- ✅ 100% 函数覆盖率

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

- `tests/log/` - 完整测试用例（136 个测试）
- `framework/log/src/` - 实现代码
- `framework/log/include/log/` - API 头文件

### 测试报告

- [TEST_EXECUTION_REPORT.md](../../tests/log/TEST_EXECUTION_REPORT.md) - 测试执行报告
- [TEST_SUCCESS_SUMMARY.md](../../tests/log/TEST_SUCCESS_SUMMARY.md) - 测试成功总结
- [TEST_FINAL_REPORT.md](../../tests/log/TEST_FINAL_REPORT.md) - 最终测试报告

### 外部参考

- [spdlog](https://github.com/gabime/spdlog) - 高性能 C++ 日志库
- [log4c](http://log4c.sourceforge.net/) - C 语言日志库
- [Zephyr Logging](https://docs.zephyrproject.org/latest/services/logging/index.html) - Zephyr RTOS 日志系统
- [NLog](https://nlog-project.org/) - .NET 日志框架

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
- 🐛 Issues: https://github.com/nexus/log/issues
- 💬 Discussions: https://github.com/nexus/log/discussions
- 📖 Wiki: https://github.com/nexus/log/wiki

---

**最后更新**: 2026-01-24  
**文档版本**: 1.0.0  
**模块版本**: 1.0.0  
**维护者**: Nexus Team

**许可证**: Copyright (c) 2026 Nexus Team
