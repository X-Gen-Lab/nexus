# Config Manager 文档索引

欢迎查阅 Nexus Config Manager 的完整文档。

## 文档结构

### 📖 入门文档

- **[../README.md](../README.md)** - 模块概述和快速开始
  - 特性介绍
  - 快速开始示例
  - API 参考速查
  - 数据类型说明

### 🏗️ 架构文档

- **[DESIGN.md](DESIGN.md)** - 架构设计文档
  - 设计目标和核心特性
  - 系统架构（分层设计）
  - 模块职责说明
  - 核心数据结构
  - 关键流程（初始化、读写、持久化等）
  - 存储后端接口
  - 线程安全设计
  - 内存管理策略
  - 加密设计
  - 错误处理机制
  - 性能优化方案
  - 设计权衡和未来改进方向

### 📚 使用指南

- **[USER_GUIDE.md](USER_GUIDE.md)** - 详细使用指南
  - 快速开始
  - 基本操作
    - 整数类型（int32/64, uint32）
    - 浮点和布尔类型
    - 字符串类型
    - 二进制数据（blob）
    - 查询操作
  - 命名空间
    - 基本用法
    - 命名空间管理
    - 使用场景
  - 默认值管理
    - 单个默认值
    - 批量注册
    - 最佳实践
  - 变更通知
    - 监听特定键
    - 监听所有变更
    - 回调注意事项
  - 存储后端
    - RAM 后端
    - Flash 后端
    - 自动/手动提交模式
  - 导入/导出
    - 二进制格式
    - JSON 格式
    - 命名空间导出
    - 配置备份与恢复
  - 加密功能
    - 设置加密密钥
    - 加密存储
    - 密钥轮换
    - 最佳实践
  - 高级用法
    - 配置验证
    - 配置模板
    - 配置分组管理
    - 配置迁移
  - 最佳实践
    - 键命名规范
    - 错误处理
    - 性能优化
    - 线程安全
  - 常见问题

### 🧪 测试文档

- **[TEST_GUIDE.md](TEST_GUIDE.md)** - 测试文档
  - 测试策略
    - 测试层次
    - 测试目标
    - 覆盖率要求
  - 单元测试
    - 初始化测试
    - 基本操作测试
    - 边界条件测试
    - 错误处理测试
  - 集成测试
    - 命名空间测试
    - 回调测试
    - 默认值测试
    - 持久化测试
    - 导入导出测试
    - 加密测试
  - 性能测试
    - 基准测试
    - 内存使用测试
    - 压力测试
  - 线程安全测试
    - 并发读写测试
    - 回调线程安全测试
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
    - OSAL 互斥锁接口
    - Flash HAL 接口
    - 加密库接口
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
    - Flash 优化
    - 性能优化
  - 故障排查
  - 示例项目

### 🔍 故障排查

- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 故障排查指南
  - 初始化问题
  - 配置操作错误
  - 持久化问题
  - 内存问题
  - 性能问题
  - 加密问题
  - 线程安全问题
  - 调试技巧
  - 常见错误码速查
  - 获取帮助

## 文档使用建议

### 🚀 新手入门路径

1. 阅读 [README.md](../README.md) 了解模块概述
2. 运行快速开始示例
3. 阅读 [USER_GUIDE.md](USER_GUIDE.md) 的基本操作部分
4. 查看 `applications/config_demo/` 示例应用
5. 根据需要查阅其他文档

### 🏗️ 架构理解路径

1. 阅读 [DESIGN.md](DESIGN.md) 了解整体架构
2. 查看核心数据结构和关键流程
3. 理解各模块职责
4. 学习设计决策和权衡

### 🔧 移植开发路径

1. 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)
2. 评估依赖项和工作量
3. 实现 OSAL 和 HAL 接口
4. 按步骤完成移植
5. 运行测试验证

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

## 相关资源

### 示例代码

- `applications/config_demo/` - 完整示例应用
- `tests/validation/test_config.py` - Python 测试用例
- `tests/kconfig/` - Kconfig 属性测试

### 外部参考

- [ESP-IDF NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [Zephyr Settings](https://docs.zephyrproject.org/latest/services/settings/index.html)
- [Linux Kernel Configuration](https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html)

## 反馈和支持

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus/config/issues
- 💬 Discussions: https://github.com/nexus/config/discussions
- 📖 Wiki: https://github.com/nexus/config/wiki

---

**最后更新**: 2026-01-24  
**文档版本**: 1.0.0  
**模块版本**: 1.0.0
