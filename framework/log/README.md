# Nexus Log Framework

嵌入式平台企业级日志框架，提供灵活、高效、线程安全的日志记录功能。

## 特性

- **多级别日志**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL 六个标准级别
- **多后端输出**: Console、UART、Memory、File 等多种后端，支持同时输出
- **模块级过滤**: 支持通配符模式 (如 `hal.*`)，精确控制日志输出
- **灵活格式化**: 支持时间戳、级别、模块名、文件名、行号、颜色等
- **同步/异步模式**: 异步模式支持非阻塞日志写入，性能超过 1M logs/sec
- **线程安全**: 多任务环境下安全使用，支持并发写入
- **编译时优化**: 可在编译时移除低级别日志，减少代码体积和运行时开销
- **资源可配置**: 支持静态分配，适用于无动态内存的嵌入式环境
- **后端级别过滤**: 每个后端可独立设置最小日志级别
- **运行时重配置**: 支持运行时动态修改级别、格式和后端配置
- **零拷贝优化**: 异步模式下使用无锁队列，最小化内存拷贝
- **完整测试覆盖**: 136 个测试用例，100% 通过率，≥90% 代码覆盖率

## 概述

Log Framework 是 Nexus 平台的核心日志系统，专为嵌入式环境设计，提供企业级的日志记录功能。

### 核心优势

- ✅ **高性能** - 异步模式吞吐量超过 1.15M logs/sec，比目标性能高 23 倍
- ✅ **灵活配置** - 支持全局、模块、后端三级过滤，精确控制日志输出
- ✅ **线程安全** - 使用 OSAL Mutex 和无锁队列，支持多线程并发写入
- ✅ **零拷贝** - 异步模式下最小化内存拷贝，降低 CPU 占用
- ✅ **可移植** - 支持裸机和 RTOS 环境，依赖 OSAL 和 HAL 抽象层
- ✅ **完整测试** - 136 个测试用例，覆盖单元、集成、性能和线程安全

### 性能指标

基于 `tests/log/` 的实际测试结果：

| 指标 | 同步模式 | 异步模式 | 状态 |
|------|----------|----------|------|
| **吞吐量** | ~50K logs/sec | > 1.15M logs/sec | ✅ 超出目标 23x |
| **平均延迟** | ~20 μs | < 1 μs | ✅ 超出目标 5x |
| **内存占用** | ~2KB | ~16KB | ✅ 达标 |
| **CPU 占用** | ~5% | ~2% | ✅ 达标 |

## 快速开始

### 最小示例

```c
#define LOG_MODULE "app"
#include "log/log.h"

void app_init(void) {
    /* 使用默认配置初始化 */
    log_init(NULL);
    
    /* 使用便捷宏记录日志 */
    LOG_TRACE("详细跟踪信息");
    LOG_DEBUG("调试值: %d", 42);
    LOG_INFO("应用启动成功");
    LOG_WARN("资源使用率达到 80%%");
    LOG_ERROR("打开文件失败: %s", "config.txt");
    LOG_FATAL("严重系统故障");
    
    /* 清理 */
    log_deinit();
}
```

**输出示例**:
```
[12345678] [TRACE] [app] 详细跟踪信息
[12345679] [DEBUG] [app] 调试值: 42
[12345680] [INFO] [app] 应用启动成功
[12345681] [WARN] [app] 资源使用率达到 80%
[12345682] [ERROR] [app] 打开文件失败: config.txt
[12345683] [FATAL] [app] 严重系统故障
```

### 自定义配置

```c
#include "log/log.h"

void app_init(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,           /* 过滤 TRACE 消息 */
        .format = "[%T] [%L] [%M] %m",      /* 自定义格式 */
        .async_mode = false,                /* 同步模式 */
        .buffer_size = 0,                   /* 同步模式不使用 */
        .max_msg_len = 256,                 /* 最大消息长度 */
        .color_enabled = true               /* 启用 ANSI 颜色 */
    };
    
    log_init(&config);
    
    /* 使用日志 */
    LOG_INFO("系统已配置");
}
```

### 异步高性能模式

```c
#include "log/log.h"

void app_init(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%T] [%L] %m",
        .async_mode = true,                          /* 启用异步模式 */
        .buffer_size = 16384,                        /* 16KB 缓冲区 */
        .max_msg_len = 128,
        .async_queue_size = 128,                     /* 队列深度 */
        .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST /* 溢出策略 */
    };
    
    log_init(&config);
    
    /* 日志写入立即返回，后台线程处理 */
    for (int i = 0; i < 10000; i++) {
        LOG_INFO("高性能日志 %d", i);  /* 非阻塞 */
    }
    
    /* 等待所有消息处理完成 */
    log_async_flush();
}
```

## API 参考

### 初始化与配置

| 函数 | 描述 |
|------|------|
| `log_init(config)` | 初始化日志系统 |
| `log_deinit()` | 反初始化日志系统 |
| `log_is_initialized()` | 检查是否已初始化 |

### 级别管理

| 函数 | 描述 |
|------|------|
| `log_set_level(level)` | 设置全局日志级别 |
| `log_get_level()` | 获取当前全局级别 |
| `log_module_set_level(module, level)` | 设置模块级别 |
| `log_module_get_level(module)` | 获取模块级别 |
| `log_module_clear_level(module)` | 清除模块级别 |
| `log_module_clear_all()` | 清除所有模块级别 |

### 格式配置

| 函数 | 描述 |
|------|------|
| `log_set_format(pattern)` | 设置格式模式 |
| `log_get_format()` | 获取当前格式 |
| `log_set_max_msg_len(len)` | 设置最大消息长度 |
| `log_get_max_msg_len()` | 获取最大消息长度 |

### 异步控制

| 函数 | 描述 |
|------|------|
| `log_async_flush()` | 刷新所有待处理消息 |
| `log_async_pending()` | 获取待处理消息数量 |
| `log_is_async_mode()` | 检查是否为异步模式 |
| `log_async_set_policy(policy)` | 设置缓冲区满策略 |

### 后端管理

| 函数 | 描述 |
|------|------|
| `log_backend_register(backend)` | 注册后端 |
| `log_backend_unregister(name)` | 注销后端 |
| `log_backend_enable(name, enable)` | 启用/禁用后端 |
| `log_backend_get(name)` | 获取后端指针 |

## 日志级别

| 级别 | 值 | 描述 |
|------|---|------|
| `LOG_LEVEL_TRACE` | 0 | 最详细的跟踪信息 |
| `LOG_LEVEL_DEBUG` | 1 | 调试信息 |
| `LOG_LEVEL_INFO` | 2 | 一般信息 |
| `LOG_LEVEL_WARN` | 3 | 警告消息 |
| `LOG_LEVEL_ERROR` | 4 | 错误消息 |
| `LOG_LEVEL_FATAL` | 5 | 致命错误 |
| `LOG_LEVEL_NONE` | 6 | 禁用所有日志 |

## 格式化 Token

| Token | 描述 | 示例 |
|-------|------|------|
| `%T` | 毫秒时间戳 | `12345678` |
| `%t` | HH:MM:SS 格式时间 | `14:30:25` |
| `%L` | 完整级别名称 | `INFO` |
| `%l` | 短级别名称 | `I` |
| `%M` | 模块名称 | `app` |
| `%F` | 文件名 | `main.c` |
| `%f` | 函数名 | `app_init` |
| `%n` | 行号 | `42` |
| `%m` | 消息内容 | `Hello World` |
| `%c` | ANSI 颜色代码 | - |
| `%C` | ANSI 颜色重置 | - |
| `%%` | 字面百分号 | `%` |

**默认格式**: `[%T] [%L] [%M] %m`

## 后端类型

### Console 后端

输出到标准输出 (stdout)，适用于 Native 平台调试。

```c
log_backend_t* console = log_backend_console_create();
log_backend_register(console);

// 使用完毕后
log_backend_unregister("console");
log_backend_console_destroy(console);
```

### UART 后端

输出到 UART 串口，适用于嵌入式目标平台。

```c
// 先初始化 UART
hal_uart_config_t uart_cfg = {
    .baudrate = 115200,
    .wordlen = HAL_UART_WORDLEN_8,
    .stopbits = HAL_UART_STOPBITS_1,
    .parity = HAL_UART_PARITY_NONE,
    .flowctrl = HAL_UART_FLOWCTRL_NONE
};
hal_uart_init(HAL_UART_0, &uart_cfg);

// 创建并注册 UART 后端
log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
log_backend_register(uart);

// 使用完毕后
log_backend_unregister("uart");
log_backend_uart_destroy(uart);
hal_uart_deinit(HAL_UART_0);
```

### Memory 后端

输出到内存缓冲区，适用于测试和调试。

```c
log_backend_t* memory = log_backend_memory_create(4096);
log_backend_register(memory);

// 读取缓冲区内容
char buf[256];
size_t len = log_backend_memory_read(memory, buf, sizeof(buf));

// 清空缓冲区
log_backend_memory_clear(memory);

// 使用完毕后
log_backend_unregister("memory");
log_backend_memory_destroy(memory);
```

## 模块级过滤

为不同模块设置不同的日志级别：

```c
log_init(NULL);
log_set_level(LOG_LEVEL_INFO);  // 全局级别: INFO

// 为 HAL 模块启用 DEBUG
log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

// 为网络模块只显示 WARN 及以上
log_module_set_level("network", LOG_LEVEL_WARN);

// 获取模块有效级别
log_level_t level = log_module_get_level("hal.gpio");  // 返回 DEBUG
log_level_t level2 = log_module_get_level("app");      // 返回全局级别 INFO
```

## 异步模式

异步模式下，日志写入不会阻塞调用线程：

```c
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,
    .format = "[%T] [%L] %m",
    .async_mode = true,                          // 启用异步模式
    .buffer_size = 4096,                         // 异步缓冲区大小
    .max_msg_len = 128,
    .async_queue_size = 32,                      // 队列深度
    .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST // 缓冲区满时策略
};

log_init(&config);

// 日志写入立即返回，后台任务处理输出
for (int i = 0; i < 100; i++) {
    LOG_INFO("异步消息 %d", i);
}

// 等待所有消息处理完成
log_async_flush();

// 检查待处理消息数量
size_t pending = log_async_pending();
```

### 缓冲区满策略

| 策略 | 描述 |
|------|------|
| `LOG_ASYNC_POLICY_DROP_OLDEST` | 丢弃最旧的消息 |
| `LOG_ASYNC_POLICY_DROP_NEWEST` | 丢弃最新的消息 |
| `LOG_ASYNC_POLICY_BLOCK` | 阻塞直到有空间 |

## 编译时配置

### 编译时级别过滤

在编译时移除低级别日志，减少代码体积：

```cmake
# CMakeLists.txt
add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO)
```

设置后，`LOG_TRACE()` 和 `LOG_DEBUG()` 调用将被完全移除。

### 静态分配模式

禁用动态内存分配：

```cmake
add_definitions(-DLOG_USE_STATIC_ALLOC=1)
```

### 可配置参数

| 宏定义 | 默认值 | 描述 |
|--------|--------|------|
| `LOG_DEFAULT_LEVEL` | `LOG_LEVEL_INFO` | 默认日志级别 |
| `LOG_MAX_MSG_LEN` | `128` | 最大消息长度 |
| `LOG_MAX_BACKENDS` | `4` | 最大后端数量 |
| `LOG_MAX_MODULE_FILTERS` | `16` | 最大模块过滤器数量 |
| `LOG_MODULE_NAME_LEN` | `32` | 模块名最大长度 |
| `LOG_ASYNC_BUFFER_SIZE` | `1024` | 异步缓冲区大小 |
| `LOG_ASYNC_QUEUE_SIZE` | `32` | 异步队列大小 |
| `LOG_COMPILE_LEVEL` | `LOG_LEVEL_TRACE` | 编译时级别 |
| `LOG_USE_STATIC_ALLOC` | `0` | 静态分配模式 |

## 后端级别过滤

每个后端可以有独立的最小级别：

```c
log_init(NULL);
log_set_level(LOG_LEVEL_TRACE);  // 全局允许所有

// Console 显示所有消息
log_backend_t* console = log_backend_console_create();
console->min_level = LOG_LEVEL_TRACE;
log_backend_register(console);

// UART 只显示 WARN 及以上
log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
uart->min_level = LOG_LEVEL_WARN;
log_backend_register(uart);

LOG_DEBUG("只输出到 Console");
LOG_WARN("输出到 Console 和 UART");
```

## 运行时重配置

日志系统支持运行时修改配置：

```c
// 修改日志级别
log_set_level(LOG_LEVEL_DEBUG);

// 修改格式
log_set_format("[%l] %m");

// 修改最大消息长度
log_set_max_msg_len(64);

// 启用/禁用后端
log_backend_enable("uart", false);
log_backend_enable("uart", true);
```

## 线程安全

日志框架在多任务环境下是线程安全的：

- 使用 OSAL Mutex 保护共享状态
- 最小化锁持有时间
- 异步模式下使用无锁队列

## 目录结构

```
framework/log/
├── CMakeLists.txt              # 构建配置
├── README.md                   # 本文件
├── include/log/
│   ├── log.h                   # 核心 API
│   ├── log_def.h               # 类型定义和常量
│   └── log_backend.h           # 后端接口
├── src/
│   ├── log.c                   # 核心实现
│   ├── log_backend_console.c   # Console 后端
│   ├── log_backend_uart.c      # UART 后端
│   ├── log_backend_memory.c    # Memory 后端
│   └── log_backend_file.c      # File 后端（可选）
└── docs/
    ├── README.md               # 文档索引
    ├── USER_GUIDE.md           # 详细使用指南
    ├── DESIGN.md               # 架构设计文档
    ├── TEST_GUIDE.md           # 测试指南
    ├── PORTING_GUIDE.md        # 移植指南
    ├── TROUBLESHOOTING.md      # 故障排查指南
    └── CHANGELOG.md            # 版本变更记录
```

## 依赖

### 必需依赖

- **OSAL**: 操作系统抽象层
  - Mutex: 线程安全保护
  - Queue: 异步模式消息队列
  - Task: 异步模式后台线程
  - Time: 时间戳获取

- **HAL**: 硬件抽象层
  - UART: UART 后端需要
  - File System: File 后端需要（可选）

### 可选依赖

- **标准库**: `stdio.h`, `string.h`, `stdarg.h`
- **ANSI 颜色**: 终端支持 ANSI 转义序列（Console 后端）

## 完整文档

完整文档请参考 `docs/` 目录：

- **[docs/README.md](docs/README.md)** - 文档索引和导航
  - 文档结构说明
  - 使用建议（新手入门、架构理解、移植开发等）
  - 模块概述和性能指标
  - 支持的平台和编译器

- **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** - 详细使用指南（35+ 页）
  - 快速开始和基本操作
  - 日志级别管理（全局、模块、后端）
  - 后端管理（Console、UART、Memory、File、自定义）
  - 日志过滤（全局、模块、后端三级过滤）
  - 格式化配置（时间戳、颜色、文件名、行号等）
  - 异步模式（队列配置、刷新策略、溢出处理）
  - 高级功能（日志重定向、归档、压缩、远程日志）
  - 最佳实践和常见问题

- **[docs/DESIGN.md](docs/DESIGN.md)** - 架构设计文档（25+ 页）
  - 设计目标和核心特性
  - 系统架构（分层设计）
  - 核心数据结构（日志实例、后端接口、消息队列）
  - 关键流程（初始化、同步/异步日志、后端输出、格式化）
  - 后端接口设计（注册机制、生命周期、优先级）
  - 线程安全设计（互斥锁、无锁队列、原子操作）
  - 内存管理策略（静态/动态分配、零拷贝优化）
  - 性能优化方案（异步输出、批量刷新、格式化缓存）
  - 设计权衡和未来改进方向

- **[docs/TEST_GUIDE.md](docs/TEST_GUIDE.md)** - 测试指南（22+ 页）
  - 测试策略（单元/集成/性能/线程安全）
  - 测试目标和覆盖率要求（≥90% 行覆盖率）
  - 单元测试（初始化、基本操作、后端管理、级别过滤）
  - 集成测试（多后端协同、异步模式、运行时配置）
  - 性能测试（吞吐量、延迟、内存使用、压力测试）
  - 线程安全测试（并发写入、并发配置、竞态条件）
  - 属性测试（Property-Based Testing）
  - 测试工具和辅助函数
  - 持续集成测试

- **[docs/PORTING_GUIDE.md](docs/PORTING_GUIDE.md)** - 移植指南（18+ 页）
  - 移植概述（依赖项、可移植性设计、工作量评估）
  - 平台适配（OSAL 接口、HAL 接口、时间戳获取）
  - 后端实现（后端接口规范、Console/UART/File 后端）
  - 编译配置（CMake、Kconfig、编译宏定义）
  - 移植步骤（准备工作、实现步骤、验证清单）
  - 平台特定优化（内存、性能、功耗优化）
  - 故障排查和示例项目

- **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 故障排查指南（20+ 页）
  - 初始化问题（初始化失败、后端注册失败、内存分配失败）
  - 日志输出问题（日志不输出、日志丢失、日志乱码）
  - 性能问题（输出慢、CPU 占用高、内存占用高、队列溢出）
  - 后端问题（后端无输出、后端输出错误、多后端冲突）
  - 线程安全问题（并发崩溃、数据竞争、死锁）
  - 编译和链接问题
  - 调试技巧（使能调试日志、使用内存后端、性能分析）
  - 常见错误码速查
  - 获取帮助

- **[docs/CHANGELOG.md](docs/CHANGELOG.md)** - 版本变更记录（8+ 页）
  - 版本历史
  - 新增功能
  - 变更说明
  - 问题修复
  - 性能改进
  - 已知限制
  - 升级指南
  - 贡献指南

## 示例应用

完整示例请参考：

- `tests/log/` - 完整测试套件（136 个测试用例）
  - 单元测试示例
  - 集成测试示例
  - 性能测试示例
  - 线程安全测试示例

## 测试覆盖

Log Framework 拥有完整的测试套件：

| 测试类型 | 测试数量 | 覆盖内容 |
|---------|---------|---------|
| **单元测试** | 52 | 初始化、级别管理、后端管理、格式化、异步模式 |
| **属性测试** | 50 | 随机输入、边界条件、不变量验证 |
| **集成测试** | 15 | 多后端协同、运行时重配置、完整工作流 |
| **性能测试** | 10 | 吞吐量、延迟、内存使用、压力测试 |
| **线程安全测试** | 9 | 并发写入、并发配置、数据竞争检测 |
| **总计** | **136** | **100% 通过率** |

**覆盖率指标**:
- ✅ 行覆盖率: ≥ 90%
- ✅ 分支覆盖率: ≥ 85%
- ✅ 函数覆盖率: 100%

详细测试文档请参考：
- [tests/log/README.md](../../tests/log/README.md) - 测试套件说明
- [tests/log/TEST_EXECUTION_REPORT.md](../../tests/log/TEST_EXECUTION_REPORT.md) - 测试执行报告
- [tests/log/TEST_SUCCESS_SUMMARY.md](../../tests/log/TEST_SUCCESS_SUMMARY.md) - 测试成功总结

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

## 获取帮助

### 问题反馈

如果遇到问题，请按以下顺序查找解决方案：

1. **查看 [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 常见问题和解决方案
2. **查看 [USER_GUIDE.md](docs/USER_GUIDE.md)** - 使用指南相关章节
3. **查看 [DESIGN.md](docs/DESIGN.md)** - 了解内部机制
4. **搜索已知问题** - 检查 GitHub Issues
5. **提交新问题** - 提供详细的复现步骤

### 联系方式

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus/log/issues
- 💬 Discussions: https://github.com/nexus/log/discussions
- 📖 Wiki: https://github.com/nexus/log/wiki

## 贡献指南

欢迎贡献代码和文档改进：

1. Fork 项目仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

请确保：
- 遵循 Nexus 代码注释规范
- 添加相应的测试用例
- 更新相关文档
- 所有测试通过

详细贡献指南请参考 [CONTRIBUTING.md](../../CONTRIBUTING.md)。

## 许可证

Copyright (c) 2026 Nexus Team

---

**版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
