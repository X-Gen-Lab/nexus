# Log Framework 使用指南

**版本**: 1.0.0  
**最后更新**: 2026-01-24

---

## 目录

1. [快速开始](#1-快速开始)
2. [基本操作](#2-基本操作)
3. [日志级别](#3-日志级别)
4. [格式化输出](#4-格式化输出)
5. [后端管理](#5-后端管理)
6. [模块级过滤](#6-模块级过滤)
7. [异步模式](#7-异步模式)
8. [高级用法](#8-高级用法)
9. [性能优化](#9-性能优化)
10. [线程安全](#10-线程安全)
11. [最佳实践](#11-最佳实践)
12. [完整示例](#12-完整示例)
13. [常见问题](#13-常见问题)

---

## 概述

Log Framework 是 Nexus 嵌入式平台的统一日志框架，提供灵活、高效、线程安全的日志记录能力。

### 核心特性

- **多级别日志**: 6 个日志级别（TRACE, DEBUG, INFO, WARN, ERROR, FATAL）
- **多后端输出**: Console、UART、Memory、自定义后端
- **模块级过滤**: 支持通配符模式（如 `hal.*`）
- **可定制格式**: 12 种格式化 Token
- **同步/异步模式**: 异步模式支持非阻塞日志写入
- **线程安全**: 多任务环境下安全使用
- **编译时优化**: 可在编译时移除低级别日志
- **后端级别过滤**: 每个后端独立的级别控制
- **ANSI 颜色支持**: 彩色日志输出

### 设计目标

- **灵活性**: 支持多种输出后端和格式化选项
- **高效性**: 最小化性能开销，支持异步模式
- **可靠性**: 线程安全，错误处理完善
- **易用性**: 简洁的 API 设计，便捷的宏接口
- **可移植性**: 跨平台设计，易于适配新平台
- **可扩展性**: 模块化架构，易于添加新功能

---

## 1. 快速开始

### 1.1 最小示例

```c
#define LOG_MODULE "app"
#include "log/log.h"

void app_main(void) {
    /* 初始化日志系统 */
    log_init(NULL);
    
    /* 记录不同级别的日志 */
    LOG_TRACE("详细跟踪信息");
    LOG_DEBUG("调试信息: %d", 42);
    LOG_INFO("应用启动成功");
    LOG_WARN("资源使用率达到 80%%");
    LOG_ERROR("打开文件失败: %s", "config.txt");
    LOG_FATAL("严重系统故障");
    
    /* 清理 */
    log_deinit();
}
```

### 1.2 自定义配置

```c
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,           /* 过滤 TRACE 消息 */
    .format = "[%T] [%L] [%M] %m",      /* 自定义格式 */
    .async_mode = false,                /* 同步模式 */
    .buffer_size = 0,                   /* 同步模式不使用 */
    .max_msg_len = 256,                 /* 最大消息长度 */
    .color_enabled = true               /* 启用 ANSI 颜色 */
};

log_init(&config);
```

## 2. 基本操作

### 2.1 初始化与反初始化

```c
/* 使用默认配置初始化 */
log_status_t status = log_init(NULL);
if (status != LOG_OK) {
    /* 处理初始化失败 */
}

/* 检查是否已初始化 */
if (log_is_initialized()) {
    /* 日志系统已就绪 */
}

/* 反初始化（释放资源）*/
log_deinit();
```

### 2.2 记录日志

```c
/* 使用便捷宏 */
LOG_TRACE("进入函数 %s", __func__);
LOG_DEBUG("变量值: x=%d, y=%d", x, y);
LOG_INFO("连接建立成功");
LOG_WARN("缓冲区使用率: %d%%", usage);
LOG_ERROR("操作失败，错误码: %d", err_code);
LOG_FATAL("无法恢复的错误");

/* 使用底层 API */
log_write(LOG_LEVEL_INFO, "mymodule", __FILE__, __LINE__, 
          __func__, "自定义消息: %s", msg);

/* 写入原始消息（不格式化）*/
log_write_raw("Raw message\n", 12);
```

### 2.3 模块定义

```c
/* 在文件开头定义模块名 */
#define LOG_MODULE "network"
#include "log/log.h"

void network_connect(void) {
    LOG_INFO("正在连接服务器...");
    /* 日志输出: [timestamp] [INFO] [network] 正在连接服务器... */
}
```

## 3. 日志级别

### 3.1 级别说明

| 级别 | 值 | 用途 | 示例 |
|------|---|------|------|
| TRACE | 0 | 最详细的跟踪信息 | 函数进入/退出 |
| DEBUG | 1 | 调试信息 | 变量值、状态 |
| INFO | 2 | 一般信息 | 启动、连接成功 |
| WARN | 3 | 警告消息 | 资源不足、重试 |
| ERROR | 4 | 错误消息 | 操作失败 |
| FATAL | 5 | 致命错误 | 系统崩溃 |
| NONE | 6 | 禁用所有日志 | - |

### 3.2 设置全局级别

```c
/* 设置全局日志级别 */
log_set_level(LOG_LEVEL_INFO);

/* 获取当前级别 */
log_level_t level = log_get_level();

/* 只有 INFO 及以上级别的日志会被输出 */
LOG_TRACE("不会输出");
LOG_DEBUG("不会输出");
LOG_INFO("会输出");
LOG_WARN("会输出");
LOG_ERROR("会输出");
```

### 3.3 模块级别控制

```c
/* 为特定模块设置级别 */
log_module_set_level("hal.uart", LOG_LEVEL_DEBUG);
log_module_set_level("network", LOG_LEVEL_WARN);

/* 使用通配符 */
log_module_set_level("hal.*", LOG_LEVEL_DEBUG);  /* 所有 HAL 模块 */

/* 获取模块级别 */
log_level_t level = log_module_get_level("hal.uart");

/* 清除模块级别（恢复全局级别）*/
log_module_clear_level("hal.uart");

/* 清除所有模块级别 */
log_module_clear_all();
```

### 3.4 级别使用场景

```c
/* TRACE: 详细的函数调用跟踪 */
void process_data(const uint8_t* data, size_t len) {
    LOG_TRACE("进入 process_data, len=%zu", len);
    
    /* 处理逻辑 */
    
    LOG_TRACE("退出 process_data");
}

/* DEBUG: 调试信息 */
void calculate(int a, int b) {
    LOG_DEBUG("输入参数: a=%d, b=%d", a, b);
    int result = a + b;
    LOG_DEBUG("计算结果: %d", result);
}

/* INFO: 重要事件 */
void connect_server(void) {
    LOG_INFO("正在连接服务器 %s:%d", host, port);
    /* ... */
    LOG_INFO("连接成功");
}

/* WARN: 警告但可继续 */
void allocate_buffer(void) {
    if (free_memory < threshold) {
        LOG_WARN("内存不足，剩余: %zu bytes", free_memory);
    }
}

/* ERROR: 错误但可恢复 */
int open_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        LOG_ERROR("打开文件失败: %s, errno=%d", path, errno);
        return -1;
    }
    return 0;
}

/* FATAL: 致命错误 */
void critical_init(void) {
    if (hardware_init() != 0) {
        LOG_FATAL("硬件初始化失败，系统无法启动");
        /* 进入安全模式或重启 */
    }
}
```

## 4. 格式化输出

### 4.1 格式化 Token

| Token | 描述 | 示例输出 |
|-------|------|----------|
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

### 4.2 自定义格式

```c
/* 简洁格式 */
log_set_format("[%l] %m");
/* 输出: [I] Application started */

/* 详细格式 */
log_set_format("[%T] [%L] [%M] %F:%n %m");
/* 输出: [12345678] [INFO] [app] main.c:42 Application started */

/* 带颜色的格式 */
log_set_format("%c[%L]%C [%M] %m");
/* 输出: [INFO] [app] Application started (INFO 为绿色) */

/* 时间格式 */
log_set_format("[%t] %m");
/* 输出: [14:30:25] Application started */

/* 获取当前格式 */
const char* format = log_get_format();
```

### 4.3 消息长度控制

```c
/* 设置最大消息长度 */
log_set_max_msg_len(128);

/* 获取最大消息长度 */
size_t max_len = log_get_max_msg_len();

/* 超长消息会被截断 */
LOG_INFO("很长的消息...");  /* 超过 128 字符会被截断并添加 "..." */
```

## 5. 后端管理

### 5.1 Console 后端

```c
#include "log/log_backend.h"

/* 创建 Console 后端 */
log_backend_t* console = log_backend_console_create();
if (console != NULL) {
    /* 注册后端 */
    log_backend_register(console);
}

/* 使用完毕后 */
log_backend_unregister("console");
log_backend_console_destroy(console);
```

### 5.2 UART 后端

```c
#include "log/log_backend.h"
#include "hal/interface/nx_uart.h"

/* 初始化 UART */
nx_uart_t* uart = nx_uart_open(0);  /* UART0 */
if (uart != NULL) {
    /* 创建 UART 后端 */
    log_backend_t* uart_backend = log_backend_uart_create(uart);
    
    /* 设置超时 */
    log_backend_uart_set_timeout(uart_backend, 1000);  /* 1 秒 */
    
    /* 注册后端 */
    log_backend_register(uart_backend);
}

/* 使用完毕后 */
log_backend_unregister("uart");
log_backend_uart_destroy(uart_backend);
nx_uart_close(uart);
```

### 5.3 Memory 后端

```c
/* 创建 Memory 后端（用于测试）*/
log_backend_t* memory = log_backend_memory_create(4096);  /* 4KB 缓冲区 */
log_backend_register(memory);

/* 记录一些日志 */
LOG_INFO("Test message 1");
LOG_INFO("Test message 2");

/* 读取缓冲区内容 */
char buffer[256];
size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
printf("日志内容:\n%.*s\n", (int)len, buffer);

/* 获取缓冲区大小 */
size_t size = log_backend_memory_size(memory);
printf("缓冲区使用: %zu bytes\n", size);

/* 清空缓冲区 */
log_backend_memory_clear(memory);

/* 清理 */
log_backend_unregister("memory");
log_backend_memory_destroy(memory);
```

### 5.4 多后端输出

```c
/* 同时使用多个后端 */
log_backend_t* console = log_backend_console_create();
log_backend_t* uart = log_backend_uart_create(uart_handle);
log_backend_t* memory = log_backend_memory_create(2048);

log_backend_register(console);
log_backend_register(uart);
log_backend_register(memory);

/* 日志会同时输出到所有后端 */
LOG_INFO("This message goes to all backends");
```

### 5.5 后端级别过滤

```c
/* 创建后端 */
log_backend_t* console = log_backend_console_create();
log_backend_t* uart = log_backend_uart_create(uart_handle);

/* 设置不同的最小级别 */
console->min_level = LOG_LEVEL_TRACE;  /* Console 显示所有 */
uart->min_level = LOG_LEVEL_WARN;      /* UART 只显示警告及以上 */

log_backend_register(console);
log_backend_register(uart);

/* DEBUG 消息只输出到 Console */
LOG_DEBUG("Debug message");

/* ERROR 消息输出到两个后端 */
LOG_ERROR("Error message");
```

### 5.6 启用/禁用后端

```c
/* 临时禁用后端 */
log_backend_enable("uart", false);

/* 重新启用 */
log_backend_enable("uart", true);

/* 获取后端指针 */
log_backend_t* backend = log_backend_get("uart");
if (backend != NULL) {
    printf("后端状态: %s\n", backend->enabled ? "启用" : "禁用");
}
```

## 6. 模块级过滤

### 6.1 基本用法

```c
/* 设置全局级别为 INFO */
log_set_level(LOG_LEVEL_INFO);

/* 为特定模块启用 DEBUG */
log_module_set_level("network", LOG_LEVEL_DEBUG);

/* 模块 "network" 的 DEBUG 消息会输出 */
#define LOG_MODULE "network"
LOG_DEBUG("Network debug info");  /* 会输出 */

/* 其他模块的 DEBUG 消息不会输出 */
#define LOG_MODULE "app"
LOG_DEBUG("App debug info");  /* 不会输出 */
```

### 6.2 通配符模式

```c
/* 为所有 HAL 模块启用 DEBUG */
log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

/* 匹配的模块 */
#define LOG_MODULE "hal.uart"
LOG_DEBUG("UART debug");  /* 会输出 */

#define LOG_MODULE "hal.spi"
LOG_DEBUG("SPI debug");  /* 会输出 */

#define LOG_MODULE "hal.i2c"
LOG_DEBUG("I2C debug");  /* 会输出 */

/* 不匹配的模块 */
#define LOG_MODULE "network"
LOG_DEBUG("Network debug");  /* 不会输出（使用全局级别）*/
```

### 6.3 层次化模块命名

```c
/* 推荐的模块命名规范 */
#define LOG_MODULE "app.network.tcp"
#define LOG_MODULE "app.network.udp"
#define LOG_MODULE "app.storage.flash"
#define LOG_MODULE "app.storage.sd"
#define LOG_MODULE "hal.uart.0"
#define LOG_MODULE "hal.uart.1"

/* 使用通配符控制 */
log_module_set_level("app.network.*", LOG_LEVEL_DEBUG);  /* 所有网络模块 */
log_module_set_level("app.storage.*", LOG_LEVEL_INFO);   /* 所有存储模块 */
log_module_set_level("hal.uart.*", LOG_LEVEL_WARN);      /* 所有 UART */
```

### 6.4 动态调整

```c
/* 运行时动态调整模块级别 */
void enable_debug_for_module(const char* module) {
    log_module_set_level(module, LOG_LEVEL_DEBUG);
    LOG_INFO("已为模块 %s 启用 DEBUG", module);
}

void disable_debug_for_module(const char* module) {
    log_module_clear_level(module);
    LOG_INFO("已为模块 %s 禁用 DEBUG", module);
}

/* 通过命令行或配置文件控制 */
if (strcmp(cmd, "debug") == 0) {
    enable_debug_for_module(module_name);
}
```

## 7. 异步模式

### 7.1 启用异步模式

```c
log_config_t config = {
    .level = LOG_LEVEL_DEBUG,
    .format = "[%T] [%L] [%M] %m",
    .async_mode = true,                          /* 启用异步 */
    .buffer_size = 4096,                         /* 缓冲区大小 */
    .max_msg_len = 128,
    .async_queue_size = 32,                      /* 队列深度 */
    .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST /* 满时策略 */
};

log_init(&config);
```

### 7.2 异步策略

```c
/* 策略 1: 丢弃最旧的消息 */
log_async_set_policy(LOG_ASYNC_POLICY_DROP_OLDEST);

/* 策略 2: 丢弃最新的消息 */
log_async_set_policy(LOG_ASYNC_POLICY_DROP_NEWEST);

/* 策略 3: 阻塞直到有空间 */
log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);

/* 获取当前策略 */
log_async_policy_t policy = log_async_get_policy();
```

### 7.3 刷新和查询

```c
/* 刷新所有待处理消息 */
log_async_flush();

/* 获取待处理消息数量 */
size_t pending = log_async_pending();
printf("待处理消息: %zu\n", pending);

/* 检查是否为异步模式 */
if (log_is_async_mode()) {
    printf("异步模式已启用\n");
}
```

### 7.4 异步模式使用场景

```c
/* 场景 1: 高频日志 */
void high_frequency_task(void) {
    for (int i = 0; i < 10000; i++) {
        LOG_DEBUG("处理数据包 %d", i);  /* 不会阻塞 */
    }
}

/* 场景 2: 中断中记录日志 */
void interrupt_handler(void) {
    LOG_INFO("中断触发");  /* 快速返回 */
}

/* 场景 3: 关键路径 */
void critical_path(void) {
    LOG_TRACE("进入关键路径");
    /* 执行关键操作 */
    LOG_TRACE("退出关键路径");
}
```

## 8. 高级用法

### 8.1 条件日志

```c
/* 只在调试模式下记录 */
#ifdef DEBUG
    LOG_DEBUG("调试信息: %d", value);
#endif

/* 使用条件表达式 */
if (verbose_mode) {
    LOG_INFO("详细信息: %s", details);
}

/* 使用宏简化 */
#define LOG_IF(cond, level, ...) \
    do { \
        if (cond) { \
            LOG_##level(__VA_ARGS__); \
        } \
    } while (0)

LOG_IF(error_occurred, ERROR, "错误发生: %d", error_code);
```

### 8.2 性能测量

```c
#include <time.h>

void measure_performance(void) {
    clock_t start = clock();
    
    /* 执行操作 */
    perform_operation();
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    LOG_INFO("操作耗时: %.3f 秒", elapsed);
}
```

### 8.3 十六进制转储

```c
void log_hex_dump(const uint8_t* data, size_t len) {
    char hex_str[256];
    size_t offset = 0;
    
    for (size_t i = 0; i < len && offset < sizeof(hex_str) - 3; i++) {
        offset += snprintf(hex_str + offset, sizeof(hex_str) - offset,
                          "%02X ", data[i]);
    }
    
    LOG_DEBUG("数据转储: %s", hex_str);
}

/* 使用示例 */
uint8_t packet[16] = {0x01, 0x02, 0x03, /* ... */};
log_hex_dump(packet, sizeof(packet));
```

### 8.4 结构化日志

```c
typedef struct {
    const char* event;
    int user_id;
    const char* action;
} log_event_t;

void log_structured_event(const log_event_t* event) {
    LOG_INFO("事件: %s, 用户: %d, 操作: %s",
             event->event, event->user_id, event->action);
}

/* 使用示例 */
log_event_t event = {
    .event = "user_login",
    .user_id = 12345,
    .action = "success"
};
log_structured_event(&event);
```

### 8.5 日志上下文

```c
/* 使用线程局部存储保存上下文 */
static __thread int g_request_id = 0;

void set_log_context(int request_id) {
    g_request_id = request_id;
}

#define LOG_CTX_INFO(fmt, ...) \
    LOG_INFO("[REQ:%d] " fmt, g_request_id, ##__VA_ARGS__)

/* 使用示例 */
void handle_request(int request_id) {
    set_log_context(request_id);
    LOG_CTX_INFO("开始处理请求");
    /* ... */
    LOG_CTX_INFO("请求处理完成");
}
```

## 9. 性能优化

### 9.1 编译时优化

#### 编译时级别过滤

通过设置 `LOG_COMPILE_LEVEL` 可以在编译时完全移除低级别日志，减少代码体积和运行时开销。

```c
/* 在编译选项中定义 */
/* CMakeLists.txt */
add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO)

/* 或在代码中定义（在包含 log.h 之前）*/
#define LOG_COMPILE_LEVEL LOG_LEVEL_INFO
#include "log/log.h"
```

**效果**:
```c
/* 当 LOG_COMPILE_LEVEL=LOG_LEVEL_INFO 时 */
LOG_TRACE("trace");  /* 编译后变为 ((void)0)，完全移除 */
LOG_DEBUG("debug");  /* 编译后变为 ((void)0)，完全移除 */
LOG_INFO("info");    /* 正常编译 */
LOG_WARN("warn");    /* 正常编译 */
```

**代码体积对比**:
```
LOG_COMPILE_LEVEL=TRACE:  100% (所有日志)
LOG_COMPILE_LEVEL=DEBUG:   85% (移除 TRACE)
LOG_COMPILE_LEVEL=INFO:    70% (移除 TRACE, DEBUG)
LOG_COMPILE_LEVEL=WARN:    50% (移除 TRACE, DEBUG, INFO)
LOG_COMPILE_LEVEL=ERROR:   30% (只保留 ERROR, FATAL)
```

#### 条件编译

```c
/* 只在调试构建中包含详细日志 */
#ifdef DEBUG_BUILD
    LOG_DEBUG("详细调试信息: %d", value);
    LOG_TRACE("函数调用: %s", __func__);
#endif

/* 生产构建中完全移除 */
#ifndef PRODUCTION_BUILD
    LOG_INFO("开发环境信息");
#endif
```

### 9.2 运行时优化

#### 早期级别过滤

日志系统在格式化之前就进行级别过滤，避免不必要的开销。

```c
/* 内部实现（简化版）*/
log_status_t log_write(log_level_t level, ...) {
    /* 早期返回，避免格式化 */
    if (level < g_log_ctx.global_level) {
        return LOG_OK;  /* 立即返回，零开销 */
    }
    
    /* 只有通过过滤的日志才会格式化 */
    vsnprintf(msg_buf, fmt, args);
    /* ... */
}
```

#### 避免复杂计算

```c
/* ❌ 不好：在日志参数中进行复杂计算 */
LOG_DEBUG("Result: %d", expensive_calculation());

/* ✅ 好：先计算，再记录 */
int result = expensive_calculation();
LOG_DEBUG("Result: %d", result);

/* ✅ 更好：使用条件编译 */
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_DEBUG
    int result = expensive_calculation();
    LOG_DEBUG("Result: %d", result);
#endif
```

#### 缓存模块级别查询

```c
/* 使用线程局部存储缓存查询结果 */
static __thread log_level_t cached_level = LOG_LEVEL_NONE;
static __thread char cached_module[LOG_MODULE_NAME_LEN] = "";

log_level_t get_module_level_cached(const char* module) {
    if (strcmp(module, cached_module) == 0) {
        return cached_level;  /* 使用缓存 */
    }
    
    /* 查询并更新缓存 */
    cached_level = log_module_get_level(module);
    strncpy(cached_module, module, sizeof(cached_module) - 1);
    
    return cached_level;
}
```

### 9.3 异步模式优化

#### 选择合适的队列大小

```c
/* 低频日志（每秒 < 10 条）*/
config.async_queue_size = 16;

/* 中频日志（每秒 10-100 条）*/
config.async_queue_size = 32;

/* 高频日志（每秒 > 100 条）*/
config.async_queue_size = 64;

/* 极高频日志（每秒 > 1000 条）*/
config.async_queue_size = 128;
```

#### 选择合适的策略

```c
/* 关键日志：不能丢失 */
log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);

/* 一般日志：可以丢弃旧消息 */
log_async_set_policy(LOG_ASYNC_POLICY_DROP_OLDEST);

/* 调试日志：可以丢弃新消息 */
log_async_set_policy(LOG_ASYNC_POLICY_DROP_NEWEST);
```

#### 批量刷新

```c
/* ❌ 不好：频繁刷新 */
for (int i = 0; i < 100; i++) {
    LOG_INFO("Message %d", i);
    log_async_flush();  /* 每次都刷新，性能差 */
}

/* ✅ 好：批量刷新 */
for (int i = 0; i < 100; i++) {
    LOG_INFO("Message %d", i);
}
log_async_flush();  /* 一次性刷新 */
```

### 9.4 格式化优化

#### 简化格式字符串

```c
/* 复杂格式（慢）*/
log_set_format("[%T] [%L] [%M] %F:%n %f() %m");

/* 简化格式（快）*/
log_set_format("[%l] %m");

/* 性能对比 */
/* 复杂格式: ~50 μs/条 */
/* 简化格式: ~10 μs/条 */
```

#### 减少消息长度

```c
/* 设置合理的最大长度 */
log_set_max_msg_len(128);  /* 默认 */
log_set_max_msg_len(64);   /* 节省内存 */
log_set_max_msg_len(256);  /* 需要更长消息 */
```

### 9.5 后端优化

#### 禁用不需要的后端

```c
/* 生产环境：只使用 UART */
log_backend_enable("console", false);
log_backend_enable("memory", false);
log_backend_enable("uart", true);
```

#### 后端级别过滤

```c
/* Console 显示所有 */
console->min_level = LOG_LEVEL_TRACE;

/* UART 只显示重要信息 */
uart->min_level = LOG_LEVEL_WARN;

/* 减少 UART 传输量，提高性能 */
```

### 9.6 性能测量

#### 测量日志开销

```c
#include <time.h>

void measure_log_overhead(void) {
    clock_t start, end;
    const int iterations = 10000;
    
    /* 测量同步模式 */
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = false;
    log_init(&config);
    
    start = clock();
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test message %d", i);
    }
    end = clock();
    
    double sync_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("同步模式: %.3f 秒, %.1f msg/s\n",
           sync_time, iterations / sync_time);
    
    log_deinit();
    
    /* 测量异步模式 */
    config.async_mode = true;
    config.buffer_size = 8192;
    config.async_queue_size = 64;
    log_init(&config);
    
    start = clock();
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test message %d", i);
    }
    log_async_flush();
    end = clock();
    
    double async_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("异步模式: %.3f 秒, %.1f msg/s\n",
           async_time, iterations / async_time);
    printf("性能提升: %.1fx\n", sync_time / async_time);
    
    log_deinit();
}
```

#### 典型性能指标

| 模式 | 延迟 | 吞吐量 | CPU 占用 | 内存占用 |
|------|------|--------|----------|----------|
| 同步模式 | ~100 μs | ~10,000 msg/s | ~5% | ~1 KB |
| 异步模式 | ~10 μs | ~100,000 msg/s | ~2% | ~12 KB |

### 9.7 内存优化

#### 静态分配模式

```c
/* 启用静态分配（无动态内存）*/
#define LOG_USE_STATIC_ALLOC 1

/* 配置静态缓冲区大小 */
#define LOG_STATIC_BUFFER_SIZE 1024
```

#### 减少内存占用

```c
config_manager_config_t config = {
    .max_msg_len = 64,        /* 减少消息长度 */
    .async_mode = false,      /* 禁用异步（节省 ~10KB）*/
    .buffer_size = 0,
    .async_queue_size = 0
};

log_init(&config);
```

#### 内存占用估算

```
基础内存:
  - log_context_t: ~200 bytes
  - 后端指针数组: 4 × LOG_MAX_BACKENDS (16 bytes)
  - 模块过滤器: 40 × LOG_MAX_MODULE_FILTERS (640 bytes)
  
异步模式额外内存:
  - 异步缓冲区: buffer_size (默认 1024 bytes)
  - 异步队列: queue_size × sizeof(log_async_msg_t) (默认 32 × 256 = 8192 bytes)
  - 异步任务栈: stack_size (默认 2048 bytes)
  
总计（异步模式）: ~12 KB
总计（同步模式）: ~1 KB
```

## 10. 线程安全

### 10.1 线程安全保证

Log Framework 的所有公共 API 都是线程安全的，使用 OSAL Mutex 保护共享状态。

```c
/* 多个线程可以安全地同时记录日志 */
void thread1(void* arg) {
    while (1) {
        LOG_INFO("Thread 1 message");
        osal_task_delay(100);
    }
}

void thread2(void* arg) {
    while (1) {
        LOG_INFO("Thread 2 message");
        osal_task_delay(100);
    }
}

/* 主函数 */
int main(void) {
    log_init(NULL);
    
    /* 创建多个线程 */
    osal_task_create(&task1, "thread1", thread1, NULL, 1024, 10);
    osal_task_create(&task2, "thread2", thread2, NULL, 1024, 10);
    
    /* 线程安全，无需额外同步 */
    
    return 0;
}
```

### 10.2 锁的粒度

```c
/* 内部实现（简化版）*/
log_status_t log_write(log_level_t level, ...) {
    /* 获取锁 */
    osal_mutex_lock(&g_log_ctx.mutex);
    
    /* 临界区：最小化锁持有时间 */
    if (level < g_log_ctx.global_level) {
        osal_mutex_unlock(&g_log_ctx.mutex);
        return LOG_OK;  /* 早期返回 */
    }
    
    /* 格式化消息 */
    vsnprintf(msg_buf, fmt, args);
    
    /* 分发到后端 */
    for (size_t i = 0; i < g_log_ctx.backend_count; i++) {
        backend->write(backend->ctx, msg, len);
    }
    
    /* 释放锁 */
    osal_mutex_unlock(&g_log_ctx.mutex);
    
    return LOG_OK;
}
```

### 10.3 异步模式的线程安全

异步模式使用生产者-消费者模式，进一步减少锁竞争。

```c
/* 生产者（应用线程）*/
log_write() {
    osal_mutex_lock(&g_log_ctx.mutex);
    
    /* 快速操作：只是放入队列 */
    osal_queue_send(g_log_ctx.async_queue, &msg, 0);
    
    osal_mutex_unlock(&g_log_ctx.mutex);
    /* 立即返回，不等待输出 */
}

/* 消费者（异步任务）*/
async_task() {
    while (true) {
        /* 从队列接收（阻塞）*/
        osal_queue_receive(g_log_ctx.async_queue, &msg, TIMEOUT);
        
        /* 不需要锁，独立处理 */
        dispatch_to_backends(&msg);
    }
}
```

### 10.4 避免死锁

#### 规则 1: 不要在回调中调用日志 API

```c
/* ❌ 错误：可能导致死锁 */
void on_config_changed(const char* key, ...) {
    LOG_INFO("Config changed: %s", key);  /* 危险！ */
}

/* ✅ 正确：使用标志延迟处理 */
static volatile bool g_config_changed = false;

void on_config_changed(const char* key, ...) {
    g_config_changed = true;  /* 只设置标志 */
}

void main_loop(void) {
    while (1) {
        if (g_config_changed) {
            g_config_changed = false;
            LOG_INFO("Config changed");  /* 在主循环中记录 */
        }
    }
}
```

#### 规则 2: 后端写入函数不应持有其他锁

```c
/* ❌ 错误：后端持有其他锁 */
log_status_t bad_backend_write(void* ctx, const char* msg, size_t len) {
    osal_mutex_lock(&some_other_mutex);  /* 可能死锁 */
    /* ... */
    osal_mutex_unlock(&some_other_mutex);
    return LOG_OK;
}

/* ✅ 正确：后端不持有锁 */
log_status_t good_backend_write(void* ctx, const char* msg, size_t len) {
    /* 直接写入，不持有锁 */
    uart_write(uart, msg, len);
    return LOG_OK;
}
```

### 10.5 中断安全

日志系统不是中断安全的，不应在中断处理程序中直接调用。

```c
/* ❌ 错误：在中断中记录日志 */
void UART_IRQHandler(void) {
    LOG_INFO("UART interrupt");  /* 不安全！ */
}

/* ✅ 正确：使用标志，在任务中处理 */
static volatile bool g_uart_irq_flag = false;

void UART_IRQHandler(void) {
    g_uart_irq_flag = true;  /* 只设置标志 */
}

void uart_task(void* arg) {
    while (1) {
        if (g_uart_irq_flag) {
            g_uart_irq_flag = false;
            LOG_INFO("UART interrupt occurred");  /* 安全 */
        }
        osal_task_delay(10);
    }
}
```

## 11. 最佳实践

### 11.1 模块命名规范

```c
/* 推荐: 使用点号分隔的层次结构 */
#define LOG_MODULE "app.network.tcp"
#define LOG_MODULE "hal.uart.driver"
#define LOG_MODULE "middleware.fs.littlefs"

/* 避免: 使用下划线或无结构 */
#define LOG_MODULE "app_network_tcp"  /* 不推荐 */
#define LOG_MODULE "tcp"              /* 不推荐，太泛化 */
```

### 9.2 日志级别选择

```c
/* 正确的级别选择 */
LOG_TRACE("函数调用: %s(%d, %d)", __func__, a, b);  /* 函数跟踪 */
LOG_DEBUG("中间结果: result=%d", result);           /* 调试信息 */
LOG_INFO("服务启动成功，监听端口 %d", port);        /* 重要事件 */
LOG_WARN("连接超时，正在重试 (%d/%d)", retry, max); /* 警告 */
LOG_ERROR("文件打开失败: %s", strerror(errno));     /* 错误 */
LOG_FATAL("内存分配失败，系统无法继续");            /* 致命错误 */

/* 避免的做法 */
LOG_INFO("x=%d", x);  /* 不要用 INFO 记录调试信息 */
LOG_ERROR("用户登录成功");  /* 不要用 ERROR 记录正常事件 */
```

### 9.3 性能考虑

```c
/* 推荐: 使用编译时级别过滤 */
#define LOG_COMPILE_LEVEL LOG_LEVEL_INFO
/* TRACE 和 DEBUG 宏会被完全移除 */

/* 推荐: 避免在日志中进行复杂计算 */
/* 不好 */
LOG_DEBUG("结果: %d", expensive_calculation());

/* 好 */
int result = expensive_calculation();
LOG_DEBUG("结果: %d", result);

/* 推荐: 使用条件编译 */
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_DEBUG
    char debug_str[256];
    format_debug_info(debug_str, sizeof(debug_str));
    LOG_DEBUG("调试信息: %s", debug_str);
#endif
```

### 9.4 错误处理

```c
/* 推荐: 检查初始化状态 */
log_status_t status = log_init(NULL);
if (status != LOG_OK) {
    /* 降级处理：使用 printf */
    printf("日志系统初始化失败\n");
}

/* 推荐: 处理后端注册失败 */
log_backend_t* uart = log_backend_uart_create(uart_handle);
if (uart != NULL) {
    status = log_backend_register(uart);
    if (status != LOG_OK) {
        log_backend_uart_destroy(uart);
    }
}
```

### 9.5 资源管理

```c
/* 推荐: 在应用退出时清理 */
void app_cleanup(void) {
    /* 刷新待处理日志 */
    if (log_is_async_mode()) {
        log_async_flush();
    }
    
    /* 反初始化 */
    log_deinit();
}

/* 推荐: 使用 RAII 模式（C++）*/
class LogGuard {
public:
    LogGuard() { log_init(NULL); }
    ~LogGuard() { log_deinit(); }
};
```

### 9.6 线程安全

```c
/* Log Framework 的公共 API 是线程安全的 */
void thread1(void* arg) {
    LOG_INFO("线程 1 消息");
}

void thread2(void* arg) {
    LOG_INFO("线程 2 消息");
}

/* 注意: 后端实现需要是线程安全的 */
```

## 12. 完整示例

### 12.1 基础应用示例

```c
/**
 * \file            app_basic.c
 * \brief           基础日志应用示例
 */

#define LOG_MODULE "app"
#include "log/log.h"

/* 应用配置 */
typedef struct {
    int timeout;
    char name[32];
    bool debug_mode;
} app_config_t;

static app_config_t g_app_config;

/* 初始化应用 */
static int app_init(void) {
    LOG_INFO("应用初始化开始");
    
    /* 加载配置 */
    g_app_config.timeout = 5000;
    strncpy(g_app_config.name, "MyApp", sizeof(g_app_config.name));
    g_app_config.debug_mode = true;
    
    LOG_DEBUG("配置加载完成: timeout=%d, name=%s, debug=%d",
              g_app_config.timeout, g_app_config.name, g_app_config.debug_mode);
    
    LOG_INFO("应用初始化完成");
    return 0;
}

/* 应用主循环 */
static void app_run(void) {
    int counter = 0;
    
    LOG_INFO("应用开始运行");
    
    while (1) {
        counter++;
        
        if (counter % 10 == 0) {
            LOG_INFO("运行计数: %d", counter);
        }
        
        if (counter % 100 == 0) {
            LOG_WARN("已运行 %d 次迭代", counter);
        }
        
        /* 模拟工作 */
        osal_task_delay(100);
    }
}

int main(void) {
    /* 初始化日志系统 */
    log_config_t log_config = LOG_CONFIG_DEFAULT;
    log_config.level = LOG_LEVEL_DEBUG;
    log_config.format = "[%T] [%L] [%M] %m";
    log_init(&log_config);
    
    /* 注册 Console 后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    /* 初始化应用 */
    if (app_init() != 0) {
        LOG_FATAL("应用初始化失败");
        return -1;
    }
    
    /* 运行应用 */
    app_run();
    
    /* 清理 */
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
    
    return 0;
}
```

### 12.2 多模块应用示例

```c
/**
 * \file            app_multimodule.c
 * \brief           多模块日志应用示例
 */

/* ========== 网络模块 ========== */
#define LOG_MODULE "app.network"
#include "log/log.h"

static int network_init(void) {
    LOG_INFO("网络模块初始化");
    
    /* 配置网络 */
    LOG_DEBUG("配置 IP: 192.168.1.100");
    LOG_DEBUG("配置网关: 192.168.1.1");
    
    LOG_INFO("网络模块初始化完成");
    return 0;
}

static void network_connect(const char* host, int port) {
    LOG_INFO("连接到 %s:%d", host, port);
    
    /* 模拟连接 */
    LOG_DEBUG("正在解析主机名...");
    LOG_DEBUG("正在建立 TCP 连接...");
    
    LOG_INFO("连接成功");
}

/* ========== 存储模块 ========== */
#undef LOG_MODULE
#define LOG_MODULE "app.storage"

static int storage_init(void) {
    LOG_INFO("存储模块初始化");
    
    /* 挂载文件系统 */
    LOG_DEBUG("挂载文件系统");
    
    LOG_INFO("存储模块初始化完成");
    return 0;
}

static int storage_write(const char* path, const void* data, size_t len) {
    LOG_DEBUG("写入文件: %s, 大小: %zu bytes", path, len);
    
    /* 模拟写入 */
    if (len > 1024) {
        LOG_WARN("文件较大，写入可能需要时间");
    }
    
    LOG_DEBUG("写入完成");
    return 0;
}

/* ========== 主程序 ========== */
#undef LOG_MODULE
#define LOG_MODULE "app.main"

int main(void) {
    /* 初始化日志系统 */
    log_init(NULL);
    
    /* 注册后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    /* 设置模块级别 */
    log_set_level(LOG_LEVEL_INFO);  /* 全局 INFO */
    log_module_set_level("app.network", LOG_LEVEL_DEBUG);  /* 网络模块 DEBUG */
    log_module_set_level("app.storage", LOG_LEVEL_INFO);   /* 存储模块 INFO */
    
    LOG_INFO("应用启动");
    
    /* 初始化模块 */
    network_init();
    storage_init();
    
    /* 使用模块 */
    network_connect("example.com", 80);
    storage_write("/data/test.txt", "Hello", 5);
    
    LOG_INFO("应用运行中");
    
    /* 清理 */
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
    
    return 0;
}
```

### 12.3 异步模式示例

```c
/**
 * \file            app_async.c
 * \brief           异步日志应用示例
 */

#define LOG_MODULE "app.async"
#include "log/log.h"
#include "FreeRTOS.h"
#include "task.h"

/* 高频任务 */
static void high_frequency_task(void* param) {
    int counter = 0;
    
    while (1) {
        /* 高频日志记录 */
        LOG_DEBUG("高频任务: %d", counter++);
        
        /* 短延迟 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 低频任务 */
static void low_frequency_task(void* param) {
    int counter = 0;
    
    while (1) {
        /* 低频日志记录 */
        LOG_INFO("低频任务: %d", counter++);
        
        /* 长延迟 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* 监控任务 */
static void monitor_task(void* param) {
    while (1) {
        /* 检查待处理消息 */
        size_t pending = log_async_pending();
        
        if (pending > 16) {
            LOG_WARN("待处理日志较多: %zu", pending);
        }
        
        /* 定期刷新 */
        log_async_flush();
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

int main(void) {
    /* 配置异步模式 */
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.level = LOG_LEVEL_DEBUG;
    config.async_mode = true;
    config.buffer_size = 4096;
    config.async_queue_size = 64;
    config.async_policy = LOG_ASYNC_POLICY_DROP_OLDEST;
    
    log_init(&config);
    
    /* 注册后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    LOG_INFO("异步日志应用启动");
    
    /* 创建任务 */
    xTaskCreate(high_frequency_task, "high_freq", 1024, NULL, 2, NULL);
    xTaskCreate(low_frequency_task, "low_freq", 1024, NULL, 2, NULL);
    xTaskCreate(monitor_task, "monitor", 1024, NULL, 3, NULL);
    
    /* 启动调度器 */
    vTaskStartScheduler();
    
    /* 不应到达这里 */
    return 0;
}
```

### 12.4 多后端示例

```c
/**
 * \file            app_multibackend.c
 * \brief           多后端日志应用示例
 */

#define LOG_MODULE "app"
#include "log/log.h"
#include "hal/interface/nx_uart.h"

int main(void) {
    /* 初始化日志系统 */
    log_init(NULL);
    
    /* 后端 1: Console（开发调试）*/
    log_backend_t* console = log_backend_console_create();
    console->min_level = LOG_LEVEL_TRACE;  /* 显示所有 */
    log_backend_register(console);
    
    /* 后端 2: UART（生产环境）*/
    nx_uart_t* uart = nx_uart_open(0);
    log_backend_t* uart_backend = log_backend_uart_create(uart);
    uart_backend->min_level = LOG_LEVEL_WARN;  /* 只显示警告及以上 */
    log_backend_uart_set_timeout(uart_backend, 1000);
    log_backend_register(uart_backend);
    
    /* 后端 3: Memory（测试和分析）*/
    log_backend_t* memory = log_backend_memory_create(4096);
    memory->min_level = LOG_LEVEL_INFO;
    log_backend_register(memory);
    
    LOG_INFO("多后端日志系统已启动");
    
    /* 记录不同级别的日志 */
    LOG_TRACE("TRACE 消息 - 只输出到 Console");
    LOG_DEBUG("DEBUG 消息 - 只输出到 Console");
    LOG_INFO("INFO 消息 - 输出到 Console 和 Memory");
    LOG_WARN("WARN 消息 - 输出到所有后端");
    LOG_ERROR("ERROR 消息 - 输出到所有后端");
    
    /* 从 Memory 后端读取日志 */
    char buffer[1024];
    size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
    printf("\n=== Memory 后端内容 ===\n%.*s\n", (int)len, buffer);
    
    /* 清理 */
    log_backend_unregister("console");
    log_backend_unregister("uart");
    log_backend_unregister("memory");
    
    log_backend_console_destroy(console);
    log_backend_uart_destroy(uart_backend);
    log_backend_memory_destroy(memory);
    
    nx_uart_close(uart);
    log_deinit();
    
    return 0;
}
```

### 12.5 错误处理示例

```c
/**
 * \file            app_error_handling.c
 * \brief           错误处理示例
 */

#define LOG_MODULE "app"
#include "log/log.h"

/* 错误处理函数 */
static void handle_init_error(log_status_t status) {
    switch (status) {
        case LOG_ERROR_NO_MEMORY:
            printf("错误：内存不足\n");
            break;
        case LOG_ERROR_INVALID_PARAM:
            printf("错误：参数无效\n");
            break;
        case LOG_ERROR_ALREADY_INIT:
            printf("错误：已经初始化\n");
            break;
        default:
            printf("错误：未知错误 %d\n", status);
            break;
    }
}

/* 安全的初始化 */
static int safe_log_init(void) {
    log_status_t status;
    
    /* 检查是否已初始化 */
    if (log_is_initialized()) {
        LOG_WARN("日志系统已初始化");
        return 0;
    }
    
    /* 初始化 */
    status = log_init(NULL);
    if (status != LOG_OK) {
        handle_init_error(status);
        return -1;
    }
    
    /* 注册后端 */
    log_backend_t* console = log_backend_console_create();
    if (console == NULL) {
        printf("错误：创建 Console 后端失败\n");
        log_deinit();
        return -1;
    }
    
    status = log_backend_register(console);
    if (status != LOG_OK) {
        printf("错误：注册后端失败\n");
        log_backend_console_destroy(console);
        log_deinit();
        return -1;
    }
    
    return 0;
}

/* 安全的日志记录 */
static void safe_log_write(void) {
    /* 检查初始化状态 */
    if (!log_is_initialized()) {
        printf("警告：日志系统未初始化\n");
        return;
    }
    
    /* 记录日志 */
    LOG_INFO("安全的日志记录");
}

int main(void) {
    /* 安全初始化 */
    if (safe_log_init() != 0) {
        printf("日志系统初始化失败，使用降级模式\n");
        /* 使用 printf 作为降级方案 */
        printf("[INFO] 应用启动（降级模式）\n");
        return -1;
    }
    
    LOG_INFO("日志系统初始化成功");
    
    /* 安全记录日志 */
    safe_log_write();
    
    /* 清理 */
    log_backend_unregister("console");
    log_deinit();
    
    return 0;
}
```

### 12.6 性能测试示例

```c
/**
 * \file            app_performance.c
 * \brief           性能测试示例
 */

#define LOG_MODULE "perf"
#include "log/log.h"
#include <time.h>

/* 测试同步模式性能 */
static void test_sync_performance(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = false;
    log_init(&config);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    const int iterations = 10000;
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test message %d", i);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("同步模式性能测试:\n");
    printf("  消息数: %d\n", iterations);
    printf("  耗时: %.3f 秒\n", elapsed);
    printf("  吞吐量: %.1f msg/s\n", iterations / elapsed);
    printf("  平均延迟: %.1f μs\n", elapsed * 1000000 / iterations);
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}

/* 测试异步模式性能 */
static void test_async_performance(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = true;
    config.buffer_size = 65536;
    config.async_queue_size = 128;
    log_init(&config);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    const int iterations = 10000;
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test message %d", i);
    }
    
    log_async_flush();
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("异步模式性能测试:\n");
    printf("  消息数: %d\n", iterations);
    printf("  耗时: %.3f 秒\n", elapsed);
    printf("  吞吐量: %.1f msg/s\n", iterations / elapsed);
    printf("  平均延迟: %.1f μs\n", elapsed * 1000000 / iterations);
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}

/* 测试不同级别的性能 */
static void test_level_performance(void) {
    log_init(NULL);
    
    log_backend_t* memory = log_backend_memory_create(65536);
    log_backend_register(memory);
    
    const int iterations = 10000;
    
    /* 测试 1: 所有日志都通过过滤 */
    log_set_level(LOG_LEVEL_TRACE);
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test");
    }
    clock_t end = clock();
    double time_pass = (double)(end - start) / CLOCKS_PER_SEC;
    
    /* 测试 2: 所有日志都被过滤 */
    log_set_level(LOG_LEVEL_FATAL);
    start = clock();
    for (int i = 0; i < iterations; i++) {
        LOG_INFO("Test");
    }
    end = clock();
    double time_filter = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("级别过滤性能测试:\n");
    printf("  通过过滤: %.3f 秒\n", time_pass);
    printf("  被过滤: %.3f 秒\n", time_filter);
    printf("  过滤开销: %.1f%%\n", time_filter / time_pass * 100);
    
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
    log_deinit();
}

int main(void) {
    printf("=== Log Framework 性能测试 ===\n\n");
    
    test_sync_performance();
    printf("\n");
    
    test_async_performance();
    printf("\n");
    
    test_level_performance();
    
    return 0;
}
```

## 13. 常见问题

### 10.1 日志不输出

**问题**: 调用 LOG_INFO() 但没有输出

**可能原因**:
1. 日志系统未初始化
2. 日志级别过滤
3. 没有注册后端
4. 后端被禁用

**解决方案**:
```c
/* 检查初始化 */
if (!log_is_initialized()) {
    log_init(NULL);
}

/* 检查级别 */
log_level_t level = log_get_level();
printf("当前级别: %d\n", level);

/* 检查模块级别 */
log_level_t module_level = log_module_get_level(LOG_MODULE);
printf("模块级别: %d\n", module_level);

/* 检查后端 */
log_backend_t* backend = log_backend_get("console");
if (backend == NULL) {
    /* 注册后端 */
    log_backend_register(log_backend_console_create());
}
```

### 10.2 编译时日志被移除

**问题**: 某些日志宏不生成代码

**原因**: `LOG_COMPILE_LEVEL` 设置过高

**解决方案**:
```c
/* 检查编译选项 */
#if LOG_COMPILE_LEVEL > LOG_LEVEL_DEBUG
    #warning "DEBUG 日志在编译时被移除"
#endif

/* 调整编译级别 */
/* 在 CMakeLists.txt 或编译选项中 */
add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_TRACE)
```

### 10.3 异步模式消息丢失

**问题**: 异步模式下部分消息丢失

**原因**: 
1. 缓冲区满
2. 应用退出前未刷新

**解决方案**:
```c
/* 增加缓冲区大小 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.async_mode = true;
config.buffer_size = 8192;  /* 增加到 8KB */
config.async_queue_size = 64;  /* 增加队列深度 */

/* 退出前刷新 */
void app_exit(void) {
    log_async_flush();
    log_deinit();
}

/* 使用阻塞策略 */
log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);
```

### 10.4 性能问题

**问题**: 日志记录影响性能

**解决方案**:
```c
/* 1. 使用异步模式 */
config.async_mode = true;

/* 2. 提高日志级别 */
log_set_level(LOG_LEVEL_WARN);

/* 3. 使用编译时过滤 */
#define LOG_COMPILE_LEVEL LOG_LEVEL_INFO

/* 4. 禁用不需要的后端 */
log_backend_enable("uart", false);

/* 5. 减少格式化开销 */
log_set_format("[%l] %m");  /* 简化格式 */
```

### 10.5 内存不足

**问题**: 初始化失败，返回 `LOG_ERROR_NO_MEMORY`

**解决方案**:
```c
/* 减少缓冲区大小 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.max_msg_len = 64;  /* 减少消息长度 */
config.buffer_size = 1024;  /* 减少异步缓冲区 */

/* 使用同步模式 */
config.async_mode = false;

/* 减少后端数量 */
/* 只使用一个后端 */
```

### 10.6 UART 输出乱码

**问题**: UART 后端输出乱码

**可能原因**:
1. 波特率不匹配
2. UART 未正确初始化
3. 并发访问冲突

**解决方案**:
```c
/* 确保 UART 正确初始化 */
nx_uart_t* uart = nx_uart_open(0);
nx_uart_config_t uart_config = {
    .baudrate = 115200,
    .data_bits = 8,
    .stop_bits = 1,
    .parity = NX_UART_PARITY_NONE
};
nx_uart_configure(uart, &uart_config);

/* 设置超时 */
log_backend_t* uart_backend = log_backend_uart_create(uart);
log_backend_uart_set_timeout(uart_backend, 1000);

/* 确保线程安全（Log Framework 已处理）*/
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
