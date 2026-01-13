# Nexus Log Framework

嵌入式平台统一日志框架，提供灵活、高效、线程安全的日志功能。

## 特性

- **多级别日志**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **多后端输出**: Console (stdout), UART, Memory (测试用)
- **模块级过滤**: 支持通配符模式 (如 `hal.*`)
- **可定制格式**: 支持时间戳、级别、模块名、文件名、行号等
- **同步/异步模式**: 异步模式支持非阻塞日志写入
- **线程安全**: 多任务环境下安全使用
- **编译时优化**: 可在编译时移除低级别日志，减少代码体积
- **资源可配置**: 支持静态分配，适用于无动态内存的环境

## 快速开始

### 基本使用

```c
#define LOG_MODULE "app"
#include "log/log.h"

void app_init(void) {
    // 使用默认配置初始化
    log_init(NULL);
    
    // 使用便捷宏记录日志
    LOG_TRACE("详细跟踪信息");
    LOG_DEBUG("调试值: %d", 42);
    LOG_INFO("应用启动成功");
    LOG_WARN("资源使用率达到 80%%");
    LOG_ERROR("打开文件失败: %s", "config.txt");
    LOG_FATAL("严重系统故障");
    
    // 清理
    log_deinit();
}
```

### 自定义配置

```c
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,           // 过滤 TRACE 消息
    .format = "[%T] [%L] [%M] %m",      // 自定义格式
    .async_mode = false,                // 同步模式
    .buffer_size = 0,                   // 同步模式不使用
    .max_msg_len = 256,                 // 最大消息长度
    .color_enabled = true               // 启用 ANSI 颜色
};

log_init(&config);
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
├── include/log/
│   ├── log.h           # 核心 API
│   ├── log_def.h       # 类型定义和常量
│   └── log_backend.h   # 后端接口
├── src/
│   ├── log.c           # 核心实现
│   └── log_backend_uart.c  # UART 后端
├── CMakeLists.txt
└── README.md
```

## 依赖

- **OSAL**: 操作系统抽象层 (Mutex, Queue, Task)
- **HAL**: 硬件抽象层 (UART)

## 示例代码

完整示例请参考 `docs/examples/log_example.c`。

## 文档

- **用户指南**: `docs/sphinx/user_guide/log.rst` (英文) / `log_cn.rst` (中文)
- **API 参考**: `docs/sphinx/api/log.rst`
- **使用示例**: `docs/examples/log_example.c` / `log_example_cn.c`

构建 Sphinx 文档:

```bash
cd docs/sphinx
sphinx-build -b html . _build/html
```

## 许可证

Copyright (c) 2026 Nexus Team
