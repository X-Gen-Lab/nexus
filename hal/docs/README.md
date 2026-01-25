# HAL 模块文档索引

欢迎查阅 Nexus HAL (Hardware Abstraction Layer) 的完整文档。

## 文档结构

### 📖 入门文档

- **[../README.md](../README.md)** - 模块概述和快速开始
  - HAL 简介和主要特性
  - 模块结构说明
  - 快速开始示例
  - 核心概念介绍
  - 支持的外设列表

### 🏗️ 架构文档

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - 架构设计文档
  - 设计目标和核心理念
  - 系统架构（分层设计）
  - 设备注册机制
  - 工厂模式实现
  - 接口适配器设计
  - 资源管理器
  - 错误处理机制
  - 跨平台支持策略
  - 性能优化方案
  - 设计权衡和未来改进方向

### 📚 使用指南

- **[USER_GUIDE.md](USER_GUIDE.md)** - 详细使用指南
  - 快速开始
  - HAL 初始化和配置
  - 外设使用指南
    - GPIO 操作
    - UART 通信
    - SPI 通信
    - I2C 通信
    - Timer/PWM 控制
    - ADC 采样
    - Flash 读写
    - CAN 总线
    - USB 通信
    - 其他外设
  - 接口适配器使用
  - 资源管理
    - DMA 管理
    - 中断管理
  - 电源管理
  - 错误处理和调试
  - 高级用法
  - 最佳实践
  - 常见问题

### 🧪 测试文档

- **[TEST_GUIDE.md](TEST_GUIDE.md)** - 测试文档
  - 测试策略
    - 测试层次
    - 测试目标
    - 覆盖率要求
  - 单元测试
    - 设备注册测试
    - 工厂函数测试
    - 接口功能测试
    - 错误处理测试
  - 集成测试
    - 外设交互测试
    - DMA 传输测试
    - 中断处理测试
  - 硬件在环测试
    - 真实硬件测试
    - 性能基准测试
  - 模拟测试
    - Native 平台测试
    - Mock 设备测试
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
  - 平台适配
    - 创建平台目录结构
    - 实现设备驱动
    - 注册设备
    - 实现平台初始化
  - 外设驱动实现
    - GPIO 驱动
    - UART 驱动
    - SPI 驱动
    - I2C 驱动
    - 其他外设驱动
  - 编译配置
    - CMake 配置
    - Kconfig 配置
    - 链接器脚本
  - 移植步骤
    - 准备工作
    - 实现步骤
    - 验证清单
  - 平台特定优化
    - DMA 优化
    - 中断优化
    - 性能优化
  - 故障排查
  - 示例项目

### 🔍 故障排查

- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化问题
  - 设备获取失败
  - 外设通信错误
  - DMA 传输问题
  - 中断处理问题
  - 性能问题
  - 内存问题
  - 平台移植问题
  - 调试技巧
  - 常见错误码速查
  - 获取帮助

## 文档使用建议

### 🚀 新手入门路径

1. 阅读 [README.md](../README.md) 了解 HAL 概述
2. 运行快速开始示例
3. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的基本操作部分
4. 查看 `applications/` 示例应用
5. 根据需要查阅其他文档

### 🏗️ 架构理解路径

1. 阅读 [ARCHITECTURE.md](ARCHITECTURE.md) 了解整体架构
2. 理解设备注册机制和工厂模式
3. 学习接口适配器设计
4. 了解资源管理器的作用
5. 学习设计决策和权衡

### 🔧 移植开发路径

1. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)
2. 评估依赖项和工作量
3. 创建平台目录结构
4. 实现必需的外设驱动
5. 按步骤完成移植
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

- `applications/blinky/` - GPIO LED 闪烁示例
- `applications/shell_demo/` - UART Shell 示例
- `tests/validation/` - 验证测试用例
- `platforms/` - 各平台实现参考

### 外部参考

- [CMSIS (Cortex Microcontroller Software Interface Standard)](https://arm-software.github.io/CMSIS_5/)
- [Zephyr Device Driver Model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)
- [Linux Device Model](https://www.kernel.org/doc/html/latest/driver-api/driver-model/)
- [STM32 HAL](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)

## 反馈和支持

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus-embedded/nexus/issues
- 💬 Discussions: https://github.com/nexus-embedded/nexus/discussions
- 📖 Wiki: https://github.com/nexus-embedded/nexus/wiki

---

**最后更新**: 2026-01-24  
**文档版本**: 1.0.0  
**模块版本**: 1.0.0
