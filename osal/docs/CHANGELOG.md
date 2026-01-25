# OSAL 模块变更记录

本文档记录 Nexus OSAL 模块的版本变更历史。

## 版本规范

本项目遵循[语义化版本](https://semver.org/lang/zh-CN/)规范：

- **主版本号**: 不兼容的 API 变更
- **次版本号**: 向后兼容的功能新增
- **修订号**: 向后兼容的问题修复

## [未发布]

### 计划功能
- ThreadX RTOS 适配器
- embOS RTOS 适配器
- 多核支持（SMP/AMP）
- 低功耗模式 API
- 实时跟踪支持（SystemView、Tracealyzer）

## [1.0.0] - 2026-01-24

### 新增功能

#### 核心功能
- ✅ 统一的 RTOS 抽象接口
- ✅ 任务管理（创建、删除、挂起、恢复、延时）
- ✅ 互斥锁（Mutex）支持
- ✅ 信号量（Semaphore）支持
- ✅ 事件标志（Event）支持
- ✅ 消息队列（Queue）支持
- ✅ 软件定时器支持
- ✅ 内存管理（动态分配、内存池）
- ✅ 诊断和调试接口

#### RTOS 适配器
- ✅ **FreeRTOS 适配器**
  - 支持 FreeRTOS 10.x
  - 完整的任务管理
  - 互斥锁、信号量、队列
  - 软件定时器
  - 静态内存分配支持
- ✅ **RT-Thread 适配器**
  - 支持 RT-Thread 4.x
  - 完整的线程管理
  - 互斥锁、信号量、事件
  - 消息队列
  - 内存池支持
- ✅ **Zephyr 适配器**
  - 支持 Zephyr 3.x
  - 线程管理
  - 同步原语
  - 消息队列
- ✅ **裸机（Baremetal）适配器**
  - 简化的单任务实现
  - 基于中断禁用的互斥锁
  - 环形缓冲区队列
  - 轮询定时器

#### 平台支持
- ✅ ARM Cortex-M (FreeRTOS)
- ✅ ARM Cortex-M (RT-Thread)
- ✅ Native (x86/x64) 模拟
- ✅ 裸机模式

#### 文档
- ✅ 架构设计文档
- ✅ 用户使用指南
- ✅ 移植指南
- ✅ 测试文档
- ✅ 故障排查指南

### 已知限制

1. **裸机模式**: 
   - 不支持真正的多任务
   - 互斥锁通过禁用中断实现
   - 不支持阻塞操作
   - 定时器基于轮询

2. **优先级映射**: 
   - OSAL 优先级 (0-31) 映射到 RTOS 原生优先级
   - 可能存在精度损失

3. **静态分配**: 
   - 仅 FreeRTOS 适配器完全支持静态分配
   - 其他 RTOS 使用动态分配

4. **内存池**: 
   - 裸机模式内存池功能有限
   - 不支持可变大小块

5. **定时器精度**: 
   - 取决于底层 RTOS 的 tick 频率
   - 裸机模式精度较低

### 性能指标

| 操作 | FreeRTOS | RT-Thread | Zephyr | Baremetal |
|------|----------|-----------|--------|-----------|
| 任务创建 | ~50 µs | ~60 µs | ~70 µs | ~5 µs |
| 上下文切换 | ~2 µs | ~2.5 µs | ~3 µs | N/A |
| 互斥锁获取 | ~1 µs | ~1.2 µs | ~1.5 µs | ~0.5 µs |
| 队列发送 | ~3 µs | ~3.5 µs | ~4 µs | ~2 µs |
| 内存分配 | ~5 µs | ~6 µs | ~7 µs | ~4 µs |

*测试平台: STM32F4 @ 168MHz*

### 内存占用

| 组件 | 代码大小 | RAM 使用 |
|------|---------|---------|
| OSAL 核心 | ~8 KB | ~1 KB |
| FreeRTOS 适配器 | ~4 KB | ~0.5 KB |
| RT-Thread 适配器 | ~5 KB | ~0.6 KB |
| Zephyr 适配器 | ~5 KB | ~0.6 KB |
| Baremetal 适配器 | ~2 KB | ~0.3 KB |

### 兼容性

- **编译器**: GCC 4.8+, Clang 3.5+, ARM Compiler 5/6, IAR
- **C 标准**: C99 或更高
- **RTOS**:
  - FreeRTOS 10.0+
  - RT-Thread 4.0+
  - Zephyr 3.0+
- **平台**: ARM Cortex-M, x86/x64

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
feat(task): add task priority inheritance support

- Implement priority inheritance for mutexes
- Add API to query task priority
- Update documentation

Closes #45
```

## 路线图

### v1.1.0 (Q2 2026)
- [ ] ThreadX 适配器
- [ ] 性能分析工具
- [ ] 更多示例应用
- [ ] 改进文档

### v1.2.0 (Q3 2026)
- [ ] embOS 适配器
- [ ] 低功耗 API
- [ ] 实时跟踪支持
- [ ] 静态分析工具

### v2.0.0 (Q4 2026)
- [ ] 多核支持
- [ ] 安全认证（MISRA C）
- [ ] 形式化验证
- [ ] 完整的 CMSIS-RTOS2 兼容

---

**维护者**: Nexus Team  
**最后更新**: 2026-01-24
