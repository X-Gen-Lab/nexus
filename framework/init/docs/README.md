# Init Framework 文档索引

**模块**: Init Framework  
**版本**: 1.0.0  
**维护者**: Nexus Team

---

## 文档概览

本目录包含 Nexus Init Framework 的完整文档，涵盖设计、使用、测试和故障排查。

### 核心文档

| 文档 | 描述 | 适用对象 |
|------|------|----------|
| [USER_GUIDE.md](USER_GUIDE.md) | 用户使用指南 | 应用开发者 |
| [DESIGN.md](DESIGN.md) | 架构设计文档 | 系统架构师、高级开发者 |
| [TEST_GUIDE.md](TEST_GUIDE.md) | 测试指南 | 测试工程师、QA |

### 参考文档

| 文档 | 描述 | 适用对象 |
|------|------|----------|
| [PORTING_GUIDE.md](PORTING_GUIDE.md) | 移植指南 | BSP 开发者 |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | 故障排查指南 | 所有开发者 |
| [CHANGELOG.md](CHANGELOG.md) | 版本变更记录 | 所有用户 |

---

## 快速导航

### 我想...

#### 开始使用 Init Framework
→ 阅读 [USER_GUIDE.md - 快速开始](USER_GUIDE.md#快速开始)

#### 了解自动初始化机制
→ 阅读 [DESIGN.md - 自动初始化机制](DESIGN.md#自动初始化机制)

#### 移植到新平台
→ 阅读 [PORTING_GUIDE.md](PORTING_GUIDE.md)

#### 解决初始化问题
→ 阅读 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

#### 编写测试用例
→ 阅读 [TEST_GUIDE.md](TEST_GUIDE.md)

#### 查看版本历史
→ 阅读 [CHANGELOG.md](CHANGELOG.md)

---

## 模块概述

Init Framework 提供三个核心功能：

### 1. 自动初始化机制 (nx_init)

基于链接器段的分级初始化系统，支持编译期自动注册初始化函数。

**核心特性**:
- ✅ 零运行时注册开销
- ✅ 模块完全解耦
- ✅ 6 级初始化顺序
- ✅ 多编译器支持

**典型用例**:
```c
static int uart_init(void) {
    /* 初始化 UART */
    return 0;
}
NX_INIT_DRIVER_EXPORT(uart_init);
```

### 2. 启动框架 (nx_startup)

统一的系统启动序列，拦截程序入口点并按定义顺序执行初始化。

**启动流程**:
```
Reset → nx_startup() → board_init → os_init → auto_init → main()
```

**核心特性**:
- ✅ 统一启动入口
- ✅ 弱符号覆盖机制
- ✅ RTOS/裸机双模式
- ✅ 可配置参数

### 3. 固件信息 (nx_firmware_info)

嵌入式固件元数据，支持外部工具提取版本信息。

**核心特性**:
- ✅ 编译期嵌入
- ✅ 版本编码/解码
- ✅ 独立链接器段
- ✅ 工具可提取

---

## 文档结构

### USER_GUIDE.md - 用户使用指南

**内容**:
- 快速开始示例
- API 详细说明
- 配置选项
- 常见用例
- 最佳实践

**适合**:
- 首次使用 Init Framework
- 需要 API 参考
- 寻找使用示例

### DESIGN.md - 架构设计文档

**内容**:
- 系统架构
- 设计决策
- 实现细节
- 性能分析
- 安全考虑

**适合**:
- 理解内部机制
- 扩展功能
- 性能优化
- 安全审计

### TEST_GUIDE.md - 测试指南

**内容**:
- 测试策略
- 测试用例
- 覆盖率要求
- 测试工具
- CI/CD 集成

**适合**:
- 编写测试
- 验证功能
- 质量保证

### PORTING_GUIDE.md - 移植指南

**内容**:
- 编译器适配
- 链接器配置
- 平台差异
- 移植检查清单

**适合**:
- 新平台移植
- 编译器切换
- BSP 开发

### TROUBLESHOOTING.md - 故障排查指南

**内容**:
- 常见问题
- 错误诊断
- 解决方案
- 调试技巧

**适合**:
- 遇到问题时
- 调试初始化
- 性能问题

### CHANGELOG.md - 版本变更记录

**内容**:
- 版本历史
- 新增功能
- Bug 修复
- 破坏性变更

**适合**:
- 升级版本
- 了解变更
- 兼容性检查

---

## 相关资源

### 代码示例

- [examples/init_basic](../../../examples/init_basic/) - 基础使用示例
- [examples/init_rtos](../../../examples/init_rtos/) - RTOS 模式示例
- [examples/firmware_info](../../../examples/firmware_info/) - 固件信息示例

### 测试代码

- [tests/init](../../../tests/init/) - 单元测试和集成测试

### API 参考

- [nx_init.h](../include/nx_init.h) - 自动初始化 API
- [nx_startup.h](../include/nx_startup.h) - 启动框架 API
- [nx_firmware_info.h](../include/nx_firmware_info.h) - 固件信息 API

---

## 支持的平台

### 编译器

| 编译器 | 版本 | 状态 |
|--------|------|------|
| GCC | ≥ 7.0 | ✅ 完全支持 |
| Arm Compiler 5 | ≥ 5.06 | ✅ 完全支持 |
| Arm Compiler 6 | ≥ 6.0 | ✅ 完全支持 |
| IAR | ≥ 8.0 | ✅ 完全支持 |
| MSVC | ≥ 2019 | ⚠️ 测试模式 |

### 架构

| 架构 | 状态 |
|------|------|
| ARM Cortex-M | ✅ 完全支持 |
| ARM Cortex-A | ✅ 完全支持 |
| RISC-V | ✅ 完全支持 |
| x86/x64 | ⚠️ 测试模式 |

### RTOS

| RTOS | 状态 |
|------|------|
| FreeRTOS | ✅ 完全支持 |
| RT-Thread | ✅ 完全支持 |
| Zephyr | ✅ 完全支持 |
| 裸机 | ✅ 完全支持 |

---

## 获取帮助

### 问题反馈

如果遇到问题，请按以下顺序查找解决方案：

1. **查看 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 常见问题和解决方案
2. **搜索已知问题** - 检查 GitHub Issues
3. **提交新问题** - 提供详细的复现步骤

### 贡献文档

欢迎改进文档！请参考 [CONTRIBUTING.md](../../../CONTRIBUTING.md)。

---

## 文档版本

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-01-24 | 初始版本 |

---

**最后更新**: 2026-01-24  
**维护者**: Nexus Team
