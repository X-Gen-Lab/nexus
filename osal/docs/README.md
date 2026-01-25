# OSAL 模块文档索引

欢迎查阅 Nexus OSAL (Operating System Abstraction Layer) 的完整文档。

## 文档结构

### 📖 入门文档

- **[../README.md](../README.md)** - 模块概述和快速开始
  - OSAL 简介和主要特性
  - 支持的 RTOS 列表
  - 快速开始示例
  - API 参考速查

### 🏗️ 架构文档

- **[DESIGN.md](DESIGN.md)** - 架构设计文档
  - 设计目标和核心理念
  - 系统架构（分层设计）
  - 适配器模式实现
  - 各 RTOS 适配策略
  - 裸机模式设计
  - 线程安全设计
  - 错误处理机制
  - 性能优化方案
  - 设计权衡和未来改进方向

### 📚 使用指南

- **[USER_GUIDE.md](USER_GUIDE.md)** - 详细使用指南
  - 快速开始
  - OSAL 初始化
  - 任务管理
    - 创建和删除任务
    - 任务优先级
    - 任务状态查询
  - 同步机制
    - 互斥锁（Mutex）
    - 信号量（Semaphore）
    - 事件标志（Event）
  - 通信机制
    - 消息队列（Queue）
  - 定时器
    - 软件定时器
    - 周期和单次定时器
  - 内存管理
    - 动态内存分配
    - 内存池
  - 诊断和调试
    - 任务统计
    - 堆栈使用情况
  - 最佳实践
  - 常见问题

### 🧪 测试文档

- **[TEST_GUIDE.md](TEST_GUIDE.md)** - 测试文档
  - 测试策略
    - 测试层次
    - 测试目标
    - 覆盖率要求
  - 单元测试
    - 任务管理测试
    - 同步机制测试
    - 队列测试
    - 定时器测试
  - 集成测试
    - 多任务协作测试
    - 同步原语组合测试
    - 压力测试
  - RTOS 适配测试
    - FreeRTOS 适配测试
    - RT-Thread 适配测试
    - 裸机模式测试
  - 性能测试
    - 上下文切换时间
    - 同步原语性能
    - 内存分配性能
  - 测试工具和辅助函数
  - 持续集成测试
  - 测试最佳实践

### 📝 版本管理

- **[CHANGELOG.md](CHANGELOG.md)** - 版本变更记录
  - 版本历史
  - 新增功能
  - 变更说明
  - 问题修复
  - 已知限制
  - 升级指南
  - 贡献指南

### 🔧 移植指南

- **[PORTING_GUIDE.md](PORTING_GUIDE.md)** - 移植指南
  - 移植概述
    - 依赖项
    - 可移植性设计
    - 工作量评估
  - RTOS 适配
    - 创建适配器目录
    - 实现核心接口
    - 任务管理适配
    - 同步机制适配
    - 定时器适配
  - 编译配置
    - CMake 配置
    - Kconfig 配置
  - 移植步骤
    - 准备工作
    - 实现步骤
    - 验证清单
  - 适配器优化
    - 性能优化
    - 内存优化
  - 故障排查
  - 示例项目

### 🔍 故障排查

- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化问题
  - 任务创建失败
  - 死锁问题
  - 优先级反转
  - 堆栈溢出
  - 内存泄漏
  - 定时器问题
  - 性能问题
  - RTOS 适配问题
  - 调试技巧
  - 常见错误码速查
  - 获取帮助

## 文档使用建议

### 🚀 新手入门路径

1. 阅读 [README.md](../README.md) 了解 OSAL 概述
2. 运行快速开始示例
3. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的基本操作部分
4. 查看 `applications/` 示例应用
5. 根据需要查阅其他文档

### 🏗️ 架构理解路径

1. 阅读 [DESIGN.md](DESIGN.md) 了解整体架构
2. 理解适配器模式和各 RTOS 适配策略
3. 学习同步机制的实现原理
4. 了解裸机模式的设计
5. 学习设计决策和权衡

### 🔧 RTOS 适配路径

1. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)
2. 评估依赖项和工作量
3. 创建适配器目录结构
4. 实现核心接口
5. 按步骤完成适配
6. 运行测试验证

### 🐛 问题排查路径

1. 查看 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. 根据错误码查找对应章节
3. 按照诊断步骤排查
4. 应用解决方案
5. 如仍未解决，查看"获取帮助"部分

### 🧪 测试开发路径

1. 阅读 [TEST_GUIDE.md](TEST_GUIDE.md)
2. 了解测试策略和覆盖率要求
3. 参考测试示例编写测试
4. 运行测试并查看报告
5. 集成到 CI/CD 流程

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
- 遵循 Nexus 代码注释规范

## 相关资源

### 示例代码

- `applications/freertos_demo/` - FreeRTOS 示例
- `applications/shell_demo/` - Shell 示例（使用 OSAL）
- `tests/validation/` - 验证测试用例
- `osal/adapters/` - 各 RTOS 适配器实现

### 外部参考

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [RT-Thread Documentation](https://www.rt-thread.org/document/site/)
- [Zephyr RTOS Documentation](https://docs.zephyrproject.org/)
- [CMSIS-RTOS2 API](https://arm-software.github.io/CMSIS_5/RTOS2/html/index.html)

## 反馈和支持

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus-embedded/nexus/issues
- 💬 Discussions: https://github.com/nexus-embedded/nexus/discussions
- 📖 Wiki: https://github.com/nexus-embedded/nexus/wiki

---

**最后更新**: 2026-01-24  
**文档版本**: 1.0.0  
**模块版本**: 1.0.0
