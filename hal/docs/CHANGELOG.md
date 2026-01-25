# HAL 模块变更记录

本文档记录 Nexus HAL 模块的版本变更历史。

## 版本规范

本项目遵循[语义化版本](https://semver.org/lang/zh-CN/)规范：

- **主版本号**: 不兼容的 API 变更
- **次版本号**: 向后兼容的功能新增
- **修订号**: 向后兼容的问题修复

## [未发布]

### 计划功能
- 设备热插拔支持
- 设备引用计数管理
- 设备树（Device Tree）支持
- 完整的异步 API

## [1.0.0] - 2026-01-24

### 新增功能

#### 核心功能
- ✅ 统一的设备接口抽象
- ✅ Kconfig 驱动的编译时设备注册
- ✅ 工厂模式的设备获取机制
- ✅ 接口适配器模式
- ✅ 统一的错误处理系统

#### 外设支持
- ✅ GPIO 接口（读、写、读写）
- ✅ UART 接口（配置、读写、回调）
- ✅ SPI 接口（主从模式、DMA 支持）
- ✅ I2C 接口（主从模式、多主机）
- ✅ Timer 接口（基础定时器、PWM、编码器）
- ✅ ADC 接口（单次采样、连续采样、DMA）
- ✅ DAC 接口
- ✅ CAN 接口
- ✅ USB 接口
- ✅ Flash 接口（内部 Flash 读写擦除）
- ✅ RTC 接口
- ✅ Watchdog 接口
- ✅ SDIO 接口
- ✅ CRC 接口
- ✅ Option Bytes 接口

#### 资源管理
- ✅ DMA 管理器（通道分配、冲突检测）
- ✅ 中断管理器（向量表管理、优先级配置）

#### 平台支持
- ✅ ARM Cortex-M 平台（STM32F4、STM32H7）
- ✅ Native 平台（x86/x64 模拟）

#### 文档
- ✅ 架构设计文档
- ✅ 用户使用指南
- ✅ 移植指南
- ✅ 测试文档
- ✅ 故障排查指南

### 已知限制

1. **设备注册**: 仅支持编译时注册，不支持运行时动态注册
2. **引用计数**: 工厂函数的 release 操作当前是 no-op
3. **设备树**: 暂不支持设备树配置
4. **异步 API**: 部分外设的异步 API 尚未完善
5. **电源管理**: 细粒度的设备电源管理尚未实现

### 性能指标

- **代码大小**: ~15KB（启用所有外设）
- **RAM 占用**: ~2KB（设备注册表和状态）
- **设备查找**: O(n)，n 为设备数量
- **工厂函数**: 零开销（静态内联）

### 兼容性

- **编译器**: GCC 4.8+, Clang 3.5+, ARM Compiler 5/6, IAR
- **C 标准**: C99 或更高
- **平台**: ARM Cortex-M, x86/x64
- **RTOS**: 无依赖（可选 OSAL 支持）

## 升级指南

### 从 0.x 升级到 1.0

本版本是首个正式版本，无需升级操作。

## 贡献指南

欢迎贡献代码和文档！请遵循以下步骤：

1. Fork 项目仓库
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- 遵循 [Nexus 代码注释规范](../../.kiro/steering/comment-standards.md)
- 使用 clang-format 格式化代码
- 添加单元测试
- 更新相关文档

### 提交信息规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

类型（type）:
- `feat`: 新功能
- `fix`: 问题修复
- `docs`: 文档更新
- `style`: 代码格式
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关

示例:
```
feat(gpio): add interrupt support for GPIO

- Add interrupt callback registration
- Support rising/falling/both edge triggers
- Add interrupt priority configuration

Closes #123
```

---

**维护者**: Nexus Team  
**最后更新**: 2026-01-24
