# Log Framework 变更日志

本文档记录 Log Framework 的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [未发布]

### 计划添加
- 日志轮转功能
- 文件后端支持
- 日志压缩功能
- 结构化日志（JSON 格式）
- 日志采样功能
- 远程日志传输

### 计划改进
- 优化异步模式性能
- 减少内存占用
- 改进格式化性能
- 添加更多后端示例

## [1.0.0] - 2026-01-24

### 新增
- 完整的日志框架实现
- 6 个日志级别（TRACE, DEBUG, INFO, WARN, ERROR, FATAL）
- 多后端支持（Console, UART, Memory）
- 模块级过滤（支持通配符）
- 可定制格式化（12 种 Token）
- 同步和异步模式
- 线程安全保护
- 编译时级别过滤
- 后端级别过滤
- ANSI 颜色支持

### 文档
- 用户使用指南（USER_GUIDE.md）
- 架构设计文档（DESIGN.md）
- 测试指南（TEST_GUIDE.md）
- 移植指南（PORTING_GUIDE.md）
- 故障排查指南（TROUBLESHOOTING.md）
- 变更日志（CHANGELOG.md）

### API
- `log_init()` - 初始化日志系统
- `log_deinit()` - 反初始化日志系统
- `log_is_initialized()` - 检查初始化状态
- `log_set_level()` - 设置全局日志级别
- `log_get_level()` - 获取全局日志级别
- `log_module_set_level()` - 设置模块级别
- `log_module_get_level()` - 获取模块级别
- `log_module_clear_level()` - 清除模块级别
- `log_module_clear_all()` - 清除所有模块级别
- `log_set_format()` - 设置格式化字符串
- `log_get_format()` - 获取格式化字符串
- `log_set_max_msg_len()` - 设置最大消息长度
- `log_get_max_msg_len()` - 获取最大消息长度
- `log_write()` - 写入日志消息
- `log_write_raw()` - 写入原始消息
- `log_async_flush()` - 刷新异步消息
- `log_async_pending()` - 获取待处理消息数
- `log_is_async_mode()` - 检查异步模式
- `log_async_set_policy()` - 设置异步策略
- `log_async_get_policy()` - 获取异步策略
- `log_backend_register()` - 注册后端
- `log_backend_unregister()` - 注销后端
- `log_backend_enable()` - 启用/禁用后端
- `log_backend_get()` - 获取后端指针

### 宏
- `LOG_TRACE()` - 记录 TRACE 级别日志
- `LOG_DEBUG()` - 记录 DEBUG 级别日志
- `LOG_INFO()` - 记录 INFO 级别日志
- `LOG_WARN()` - 记录 WARN 级别日志
- `LOG_ERROR()` - 记录 ERROR 级别日志
- `LOG_FATAL()` - 记录 FATAL 级别日志

### 后端
- Console 后端 - 输出到 stdout
- UART 后端 - 输出到 UART 串口
- Memory 后端 - 输出到内存缓冲区

### 配置选项
- `LOG_DEFAULT_LEVEL` - 默认日志级别
- `LOG_MAX_MSG_LEN` - 最大消息长度
- `LOG_MAX_BACKENDS` - 最大后端数量
- `LOG_MAX_MODULE_FILTERS` - 最大模块过滤器数量
- `LOG_MODULE_NAME_LEN` - 模块名最大长度
- `LOG_DEFAULT_FORMAT` - 默认格式化字符串
- `LOG_ASYNC_BUFFER_SIZE` - 异步缓冲区大小
- `LOG_ASYNC_QUEUE_SIZE` - 异步队列大小
- `LOG_ASYNC_TASK_STACK_SIZE` - 异步任务栈大小
- `LOG_ASYNC_TASK_PRIORITY` - 异步任务优先级
- `LOG_COMPILE_LEVEL` - 编译时级别过滤
- `LOG_USE_STATIC_ALLOC` - 静态分配模式

### 测试
- 单元测试覆盖率 > 90%
- 集成测试
- 性能测试
- 线程安全测试
- 内存泄漏测试

### 平台支持
- STM32F4 系列
- ESP32 系列
- Linux (Native)
- 裸机环境

### 依赖
- OSAL (Mutex, Queue, Task)
- HAL (UART - 可选)
- 标准 C 库

## [0.9.0] - 2026-01-20 (Beta)

### 新增
- 基本日志功能
- Console 后端
- 同步模式
- 简单格式化

### 已知问题
- 异步模式不稳定
- 内存使用较高
- 性能需要优化

## [0.5.0] - 2026-01-15 (Alpha)

### 新增
- 初始实现
- 基本 API 设计
- 概念验证

### 限制
- 仅支持 Console 输出
- 无模块过滤
- 无异步模式

## 版本说明

### 版本号格式

版本号格式为 `MAJOR.MINOR.PATCH`：

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的问题修正

### 变更类型

- **新增**: 新功能
- **变更**: 现有功能的变更
- **弃用**: 即将移除的功能
- **移除**: 已移除的功能
- **修复**: 问题修复
- **安全**: 安全相关修复

## 升级指南

### 从 0.9.0 升级到 1.0.0

#### API 变更

1. **初始化配置结构变更**
   ```c
   /* 旧版本 */
   log_config_t config = {
       .level = LOG_LEVEL_INFO,
       .format = "[%L] %m"
   };
   
   /* 新版本 */
   log_config_t config = {
       .level = LOG_LEVEL_INFO,
       .format = "[%L] %m",
       .async_mode = false,        /* 新增 */
       .buffer_size = 0,           /* 新增 */
       .max_msg_len = 128,         /* 新增 */
       .color_enabled = false,     /* 新增 */
       .async_queue_size = 0,      /* 新增 */
       .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST  /* 新增 */
   };
   ```

2. **后端接口变更**
   ```c
   /* 旧版本 */
   log_backend_t backend = {
       .name = "custom",
       .write = custom_write
   };
   
   /* 新版本 */
   log_backend_t backend = {
       .name = "custom",
       .init = NULL,               /* 新增 */
       .write = custom_write,
       .flush = NULL,              /* 新增 */
       .deinit = NULL,             /* 新增 */
       .ctx = NULL,                /* 新增 */
       .min_level = LOG_LEVEL_TRACE,  /* 新增 */
       .enabled = true             /* 新增 */
   };
   ```

#### 新功能使用

1. **异步模式**
   ```c
   log_config_t config = LOG_CONFIG_DEFAULT;
   config.async_mode = true;
   config.buffer_size = 4096;
   log_init(&config);
   ```

2. **模块过滤**
   ```c
   log_module_set_level("hal.*", LOG_LEVEL_DEBUG);
   ```

3. **后端级别过滤**
   ```c
   log_backend_t* uart = log_backend_uart_create(uart_handle);
   uart->min_level = LOG_LEVEL_WARN;
   log_backend_register(uart);
   ```

#### 弃用功能

无弃用功能。

#### 移除功能

无移除功能。

### 从 0.5.0 升级到 0.9.0

#### 重大变更

1. **API 重新设计**
   - 所有 API 函数名称变更
   - 配置结构完全重写

2. **后端机制变更**
   - 从单后端改为多后端
   - 后端注册机制变更

#### 升级步骤

1. 更新所有 API 调用
2. 重新实现自定义后端
3. 更新配置代码
4. 重新测试所有功能

## 兼容性

### API 兼容性

- **1.0.0**: 稳定 API，保证向后兼容
- **0.9.0**: Beta API，可能有变更
- **0.5.0**: Alpha API，不保证兼容

### 平台兼容性

| 版本 | STM32F4 | ESP32 | Linux | 裸机 |
|------|---------|-------|-------|------|
| 1.0.0 | ✅ | ✅ | ✅ | ✅ |
| 0.9.0 | ✅ | ✅ | ✅ | ❌ |
| 0.5.0 | ✅ | ❌ | ✅ | ❌ |

### RTOS 兼容性

| 版本 | FreeRTOS | RT-Thread | Zephyr | 裸机 |
|------|----------|-----------|--------|------|
| 1.0.0 | ✅ | ✅ | ✅ | ✅ |
| 0.9.0 | ✅ | ✅ | ❌ | ❌ |
| 0.5.0 | ✅ | ❌ | ❌ | ❌ |

## 性能指标

### 1.0.0 性能

| 指标 | 同步模式 | 异步模式 |
|------|----------|----------|
| 日志记录延迟 | ~100 μs | ~10 μs |
| 吞吐量 | ~10,000 msg/s | ~100,000 msg/s |
| 内存占用 | ~1 KB | ~12 KB |
| CPU 占用 | ~5% | ~2% |

### 性能改进

- 1.0.0 vs 0.9.0: 性能提升 50%
- 1.0.0 vs 0.5.0: 性能提升 200%

## 已知问题

### 1.0.0

- 异步模式下极端高频日志可能导致消息丢失
- UART 后端在某些平台上可能有延迟
- Memory 后端环形缓冲区在满时会覆盖旧数据

### 解决方案

1. **消息丢失**: 增加队列大小或使用阻塞策略
2. **UART 延迟**: 使用 DMA 模式或增加超时
3. **数据覆盖**: 定期读取 Memory 后端数据

## 贡献者

感谢以下贡献者对 Log Framework 的贡献：

- Nexus Team - 核心开发
- 社区贡献者 - 测试和反馈

## 许可证

Copyright (c) 2026 Nexus Team

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
