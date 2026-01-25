# Log Framework 架构设计文档

## 1. 概述

Log Framework 是 Nexus 嵌入式平台的统一日志框架，提供灵活、高效、线程安全的日志记录能力。

### 1.1 设计目标

- **灵活性**: 支持多种输出后端和格式化选项
- **高效性**: 最小化性能开销，支持异步模式
- **可靠性**: 线程安全，错误处理完善
- **易用性**: 简洁的 API 设计，便捷的宏接口
- **可移植性**: 跨平台设计，易于适配新平台
- **可扩展性**: 模块化架构，易于添加新功能

### 1.2 核心特性

- 6 个日志级别（TRACE, DEBUG, INFO, WARN, ERROR, FATAL）
- 多后端输出（Console, UART, Memory, 自定义）
- 模块级过滤（支持通配符）
- 可定制格式化（12 种格式化 Token）
- 同步/异步模式
- 线程安全保护
- 编译时级别过滤
- 后端级别过滤
- ANSI 颜色支持

## 2. 系统架构

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│                    (User Application Code)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       Macro Layer                            │
│  LOG_TRACE() / LOG_DEBUG() / LOG_INFO() / LOG_WARN() / ...  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       Public API Layer                       │
│  log_write() / log_set_level() / log_backend_register() ... │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Logic Layer                        │
├──────────────┬──────────────┬──────────────┬────────────────┤
│ Level Filter │   Module     │  Formatter   │  Dispatcher    │
│              │   Filter     │              │                │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Async Queue (Optional)                    │
│              Producer-Consumer Queue                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Backend Abstraction                       │
│              (Backend Interface + Registry)                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Output Backends                           │
├──────────────┬──────────────┬──────────────┬────────────────┤
│Console Backend│ UART Backend │Memory Backend│Custom Backend  │
└──────────────┴──────────────┴──────────────┴────────────────┘
```

### 2.2 模块职责

#### 2.2.1 Level Filter
- 实现全局日志级别过滤
- 提供级别比较和判断
- 支持运行时级别调整

#### 2.2.2 Module Filter
- 管理模块级别映射表
- 支持通配符模式匹配
- 提供模块级别查询和设置

#### 2.2.3 Formatter
- 解析格式化字符串
- 替换格式化 Token
- 生成最终输出字符串
- 处理消息截断

#### 2.2.4 Dispatcher
- 遍历所有注册的后端
- 检查后端级别和启用状态
- 调用后端写入接口
- 处理后端错误

#### 2.2.5 Async Queue
- 实现生产者-消费者队列
- 管理异步缓冲区
- 处理队列满策略
- 提供刷新和查询接口

#### 2.2.6 Backend Abstraction
- 定义统一的后端接口
- 管理后端注册表
- 提供后端查询和控制

## 3. 核心数据结构

### 3.1 日志上下文

```c
typedef struct {
    log_level_t global_level;              /* 全局日志级别 */
    char format[LOG_MAX_FORMAT_LEN];       /* 格式化字符串 */
    size_t max_msg_len;                    /* 最大消息长度 */
    bool color_enabled;                    /* 颜色启用标志 */
    bool async_mode;                       /* 异步模式标志 */
    log_async_policy_t async_policy;       /* 异步策略 */
    
    /* 后端管理 */
    log_backend_t* backends[LOG_MAX_BACKENDS];
    size_t backend_count;
    
    /* 模块过滤器 */
    log_module_filter_t filters[LOG_MAX_MODULE_FILTERS];
    size_t filter_count;
    
    /* 异步队列 */
    osal_queue_t async_queue;
    osal_task_t async_task;
    uint8_t* async_buffer;
    size_t async_buffer_size;
    
    /* 线程安全 */
    osal_mutex_t mutex;
    
    /* 状态标志 */
    bool initialized;
} log_context_t;
```

### 3.2 模块过滤器

```c
typedef struct {
    char pattern[LOG_MODULE_NAME_LEN];     /* 模块名模式 */
    log_level_t level;                     /* 模块级别 */
    bool is_wildcard;                      /* 是否为通配符 */
} log_module_filter_t;
```

### 3.3 后端接口

```c
struct log_backend {
    const char* name;                      /* 后端名称 */
    log_backend_init_fn init;              /* 初始化函数 */
    log_backend_write_fn write;            /* 写入函数 */
    log_backend_flush_fn flush;            /* 刷新函数 */
    log_backend_deinit_fn deinit;          /* 反初始化函数 */
    void* ctx;                             /* 后端上下文 */
    log_level_t min_level;                 /* 最小级别 */
    bool enabled;                          /* 启用标志 */
};
```

### 3.4 异步消息

```c
typedef struct {
    log_level_t level;                     /* 日志级别 */
    char module[LOG_MODULE_NAME_LEN];      /* 模块名 */
    char file[32];                         /* 文件名 */
    int line;                              /* 行号 */
    char func[32];                         /* 函数名 */
    uint32_t timestamp;                    /* 时间戳 */
    char message[LOG_MAX_MSG_LEN];         /* 消息内容 */
} log_async_msg_t;
```

## 4. 关键流程

### 4.1 初始化流程

```
log_init(config)
    │
    ├─> 验证配置参数
    │
    ├─> 初始化全局上下文
    │   ├─> 设置默认级别
    │   ├─> 设置默认格式
    │   └─> 清空后端列表
    │
    ├─> 初始化模块过滤器
    │   └─> 清空过滤器数组
    │
    ├─> 创建互斥锁
    │   └─> osal_mutex_create()
    │
    ├─> 初始化异步模式（如果启用）
    │   ├─> 分配异步缓冲区
    │   ├─> 创建异步队列
    │   └─> 创建异步任务
    │
    └─> 设置初始化标志
```

### 4.2 日志写入流程（同步模式）

```
LOG_INFO("message", args...)
    │
    ├─> 编译时级别检查
    │   └─> #if LOG_COMPILE_LEVEL <= LOG_LEVEL_INFO
    │
    ├─> log_write(level, module, file, line, func, fmt, args)
    │   │
    │   ├─> 检查初始化状态
    │   │
    │   ├─> 获取互斥锁
    │   │
    │   ├─> 全局级别过滤
    │   │   └─> if (level < global_level) return;
    │   │
    │   ├─> 模块级别过滤
    │   │   ├─> 查找模块过滤器
    │   │   └─> if (level < module_level) return;
    │   │
    │   ├─> 格式化消息
    │   │   ├─> vsnprintf(msg_buf, fmt, args)
    │   │   └─> format_log_message(output_buf, format, ...)
    │   │
    │   ├─> 分发到后端
    │   │   └─> for each backend:
    │   │       ├─> 检查后端启用状态
    │   │       ├─> 检查后端级别
    │   │       └─> backend->write(msg, len)
    │   │
    │   └─> 释放互斥锁
    │
    └─> 返回
```

### 4.3 日志写入流程（异步模式）

```
LOG_INFO("message", args...)
    │
    ├─> log_write(level, module, file, line, func, fmt, args)
    │   │
    │   ├─> 检查初始化状态
    │   │
    │   ├─> 获取互斥锁
    │   │
    │   ├─> 级别过滤（同同步模式）
    │   │
    │   ├─> 格式化消息
    │   │
    │   ├─> 创建异步消息
    │   │   └─> 填充 log_async_msg_t
    │   │
    │   ├─> 放入队列
    │   │   ├─> osal_queue_send(async_queue, msg)
    │   │   └─> 处理队列满情况
    │   │       ├─> DROP_OLDEST: 丢弃最旧消息
    │   │       ├─> DROP_NEWEST: 丢弃当前消息
    │   │       └─> BLOCK: 阻塞等待
    │   │
    │   └─> 释放互斥锁
    │
    └─> 立即返回（不等待输出）

异步任务:
    while (true) {
        ├─> 从队列接收消息
        │   └─> osal_queue_receive(async_queue, &msg, timeout)
        │
        ├─> 分发到后端
        │   └─> for each backend:
        │       └─> backend->write(msg, len)
        │
        └─> 继续循环
    }
```

### 4.4 格式化流程

```
format_log_message(output, format, level, module, file, line, func, msg)
    │
    ├─> 遍历格式化字符串
    │   │
    │   └─> for each character:
    │       │
    │       ├─> if (char == '%'):
    │       │   │
    │       │   └─> 解析 Token:
    │       │       ├─> %T → 时间戳（毫秒）
    │       │       ├─> %t → 时间（HH:MM:SS）
    │       │       ├─> %L → 级别名称（完整）
    │       │       ├─> %l → 级别名称（短）
    │       │       ├─> %M → 模块名
    │       │       ├─> %F → 文件名
    │       │       ├─> %f → 函数名
    │       │       ├─> %n → 行号
    │       │       ├─> %m → 消息内容
    │       │       ├─> %c → 颜色代码
    │       │       ├─> %C → 颜色重置
    │       │       └─> %% → 字面 '%'
    │       │
    │       └─> else:
    │           └─> 直接复制字符
    │
    ├─> 检查输出长度
    │   └─> if (len > max_msg_len):
    │       └─> 截断并添加 "..."
    │
    └─> 返回格式化后的字符串
```

### 4.5 模块过滤匹配流程

```
log_module_get_level(module)
    │
    ├─> 遍历过滤器数组
    │   │
    │   └─> for each filter:
    │       │
    │       ├─> if (filter.is_wildcard):
    │       │   │
    │       │   └─> 通配符匹配:
    │       │       ├─> 提取前缀（"hal.*" → "hal."）
    │       │       └─> if (strncmp(module, prefix, len) == 0):
    │       │           └─> 返回 filter.level
    │       │
    │       └─> else:
    │           └─> 精确匹配:
    │               └─> if (strcmp(module, filter.pattern) == 0):
    │                   └─> 返回 filter.level
    │
    └─> 未找到匹配，返回全局级别
```

## 5. 后端实现

### 5.1 Console 后端

```c
typedef struct {
    FILE* stream;  /* stdout */
} console_backend_ctx_t;

log_status_t console_write(void* ctx, const char* msg, size_t len) {
    console_backend_ctx_t* console = (console_backend_ctx_t*)ctx;
    
    /* 写入 stdout */
    size_t written = fwrite(msg, 1, len, console->stream);
    
    /* 刷新输出 */
    fflush(console->stream);
    
    return (written == len) ? LOG_OK : LOG_ERROR_BACKEND;
}
```

### 5.2 UART 后端

```c
typedef struct {
    nx_uart_t* uart;       /* UART 接口 */
    uint32_t timeout_ms;   /* 超时时间 */
} uart_backend_ctx_t;

log_status_t uart_write(void* ctx, const char* msg, size_t len) {
    uart_backend_ctx_t* uart_ctx = (uart_backend_ctx_t*)ctx;
    
    /* 通过 UART 发送 */
    int ret = nx_uart_write(uart_ctx->uart, (const uint8_t*)msg, len,
                           uart_ctx->timeout_ms);
    
    return (ret == len) ? LOG_OK : LOG_ERROR_BACKEND;
}
```

### 5.3 Memory 后端

```c
typedef struct {
    uint8_t* buffer;       /* 环形缓冲区 */
    size_t size;           /* 缓冲区大小 */
    size_t head;           /* 写入位置 */
    size_t tail;           /* 读取位置 */
    size_t count;          /* 当前数据量 */
    osal_mutex_t mutex;    /* 互斥锁 */
} memory_backend_ctx_t;

log_status_t memory_write(void* ctx, const char* msg, size_t len) {
    memory_backend_ctx_t* mem = (memory_backend_ctx_t*)ctx;
    
    osal_mutex_lock(&mem->mutex);
    
    /* 写入环形缓冲区 */
    for (size_t i = 0; i < len; i++) {
        mem->buffer[mem->head] = msg[i];
        mem->head = (mem->head + 1) % mem->size;
        
        if (mem->count < mem->size) {
            mem->count++;
        } else {
            /* 缓冲区满，覆盖最旧数据 */
            mem->tail = (mem->tail + 1) % mem->size;
        }
    }
    
    osal_mutex_unlock(&mem->mutex);
    
    return LOG_OK;
}
```

## 6. 线程安全设计

### 6.1 保护策略

使用全局互斥锁保护所有公共 API：

```c
static log_context_t g_log_ctx;

log_status_t log_write(log_level_t level, const char* module,
                       const char* file, int line, const char* func,
                       const char* fmt, ...) {
    /* 获取锁 */
    osal_mutex_lock(&g_log_ctx.mutex);
    
    /* 执行日志操作 */
    /* ... */
    
    /* 释放锁 */
    osal_mutex_unlock(&g_log_ctx.mutex);
    
    return LOG_OK;
}
```

### 6.2 异步模式线程安全

```c
/* 生产者（应用线程）*/
log_write() {
    osal_mutex_lock(&g_log_ctx.mutex);
    
    /* 格式化消息 */
    /* 放入队列（队列本身是线程安全的）*/
    osal_queue_send(g_log_ctx.async_queue, &msg, 0);
    
    osal_mutex_unlock(&g_log_ctx.mutex);
}

/* 消费者（异步任务）*/
async_task() {
    while (true) {
        /* 从队列接收（阻塞）*/
        osal_queue_receive(g_log_ctx.async_queue, &msg, TIMEOUT);
        
        /* 不需要锁，因为后端写入是独立的 */
        dispatch_to_backends(&msg);
    }
}
```

### 6.3 死锁避免

- 后端写入函数不应持有其他锁
- 避免在日志回调中调用日志 API
- 使用超时机制防止永久阻塞

## 7. 内存管理

### 7.1 静态分配

```c
/* 全局上下文（静态分配）*/
static log_context_t g_log_ctx;

/* 后端数组（静态分配）*/
static log_backend_t* g_backends[LOG_MAX_BACKENDS];

/* 模块过滤器（静态分配）*/
static log_module_filter_t g_filters[LOG_MAX_MODULE_FILTERS];
```

### 7.2 动态分配（异步缓冲区）

```c
/* 异步缓冲区（动态分配）*/
g_log_ctx.async_buffer = malloc(config->buffer_size);
if (g_log_ctx.async_buffer == NULL) {
    return LOG_ERROR_NO_MEMORY;
}
```

### 7.3 内存占用估算

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

## 8. 性能优化

### 8.1 编译时优化

```c
/* 编译时级别过滤 */
#if LOG_COMPILE_LEVEL <= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...) LOG_WRITE(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)  /* 完全移除 */
#endif
```

### 8.2 运行时优化

```c
/* 快速路径：级别过滤 */
if (level < g_log_ctx.global_level) {
    return LOG_OK;  /* 早期返回，避免格式化 */
}

/* 缓存模块级别查询结果 */
static __thread log_level_t cached_level = LOG_LEVEL_NONE;
static __thread char cached_module[LOG_MODULE_NAME_LEN] = "";

if (strcmp(module, cached_module) == 0) {
    /* 使用缓存的级别 */
} else {
    /* 查询并更新缓存 */
}
```

### 8.3 异步模式优化

```c
/* 使用无锁队列（高级优化）*/
/* 使用环形缓冲区减少内存分配 */
/* 批量写入后端（减少系统调用）*/
```

## 9. 错误处理

### 9.1 错误码设计

```c
typedef enum {
    LOG_OK = 0,                    /* 成功 */
    LOG_ERROR = 1,                 /* 通用错误 */
    LOG_ERROR_INVALID_PARAM = 2,   /* 参数错误 */
    LOG_ERROR_NOT_INIT = 3,        /* 未初始化 */
    LOG_ERROR_NO_MEMORY = 4,       /* 内存不足 */
    LOG_ERROR_FULL = 5,            /* 缓冲区满 */
    LOG_ERROR_BACKEND = 6,         /* 后端错误 */
    LOG_ERROR_ALREADY_INIT = 7,    /* 已初始化 */
} log_status_t;
```

### 9.2 错误传播

```c
/* 后端错误不影响其他后端 */
for (size_t i = 0; i < g_log_ctx.backend_count; i++) {
    log_backend_t* backend = g_log_ctx.backends[i];
    
    if (!backend->enabled) {
        continue;
    }
    
    log_status_t status = backend->write(backend->ctx, msg, len);
    if (status != LOG_OK) {
        /* 记录错误但继续 */
        g_log_ctx.backend_errors[i]++;
    }
}
```

## 10. 设计权衡

### 10.1 同步 vs 异步

**选择**: 支持两种模式

**理由**:
- 同步模式：简单、可靠、适合低频日志
- 异步模式：高性能、非阻塞、适合高频日志

**实现**: 通过配置选择

### 10.2 静态 vs 动态内存

**选择**: 混合方式

**理由**:
- 核心结构静态分配（可预测）
- 异步缓冲区动态分配（灵活）

### 10.3 功能完整性 vs 代码大小

**选择**: 模块化设计

**理由**:
- 通过宏开关控制功能
- 编译时优化减少代码

### 10.4 性能 vs 功能

**选择**: 分层过滤

**理由**:
- 编译时过滤（零开销）
- 运行时过滤（灵活性）
- 后端过滤（细粒度控制）

## 11. 扩展性设计

### 11.1 自定义后端

```c
/* 用户可以实现自定义后端 */
log_status_t custom_write(void* ctx, const char* msg, size_t len) {
    /* 自定义实现 */
    return LOG_OK;
}

log_backend_t custom_backend = {
    .name = "custom",
    .write = custom_write,
    .ctx = &custom_ctx,
    .min_level = LOG_LEVEL_TRACE,
    .enabled = true
};

log_backend_register(&custom_backend);
```

### 11.2 自定义格式化

```c
/* 未来可以支持自定义格式化函数 */
typedef log_status_t (*log_formatter_fn)(char* output, size_t size,
                                         const log_format_args_t* args);

log_set_custom_formatter(my_formatter);
```

## 12. 未来改进方向

### 12.1 短期改进

- 添加日志轮转功能
- 支持日志文件输出
- 实现日志压缩
- 添加日志统计信息

### 12.2 中期改进

- 支持结构化日志（JSON）
- 实现日志采样
- 添加日志过滤规则引擎
- 支持远程日志传输

### 12.3 长期改进

- 分布式日志聚合
- 日志分析和可视化
- 实时日志监控
- 日志安全和审计

## 13. 代码示例

### 13.1 完整的驱动初始化示例

```c
/**
 * \file            uart_driver.c
 * \brief           UART 驱动示例（带日志）
 */

#define LOG_MODULE "hal.uart"
#include "log/log.h"
#include "uart_hw.h"

/* UART 设备结构 */
typedef struct {
    int id;
    uart_hw_t* hw;
    bool initialized;
} uart_device_t;

static uart_device_t g_uart_devices[UART_MAX_DEVICES];

/* 初始化 UART 设备 */
int uart_init(int id, const uart_config_t* config) {
    LOG_TRACE("进入 uart_init, id=%d", id);
    
    if (id < 0 || id >= UART_MAX_DEVICES) {
        LOG_ERROR("无效的 UART ID: %d", id);
        return -1;
    }
    
    uart_device_t* dev = &g_uart_devices[id];
    
    if (dev->initialized) {
        LOG_WARN("UART%d 已经初始化", id);
        return 0;
    }
    
    LOG_DEBUG("初始化 UART%d, 波特率=%d", id, config->baudrate);
    
    /* 初始化硬件 */
    dev->hw = uart_hw_init(id, config);
    if (dev->hw == NULL) {
        LOG_ERROR("UART%d 硬件初始化失败", id);
        return -1;
    }
    
    dev->id = id;
    dev->initialized = true;
    
    LOG_INFO("UART%d 初始化成功", id);
    LOG_TRACE("退出 uart_init");
    
    return 0;
}

/* 写入数据 */
int uart_write(int id, const void* data, size_t len) {
    LOG_TRACE("uart_write: id=%d, len=%zu", id, len);
    
    if (id < 0 || id >= UART_MAX_DEVICES) {
        LOG_ERROR("无效的 UART ID: %d", id);
        return -1;
    }
    
    uart_device_t* dev = &g_uart_devices[id];
    
    if (!dev->initialized) {
        LOG_ERROR("UART%d 未初始化", id);
        return -1;
    }
    
    /* 写入硬件 */
    int ret = uart_hw_write(dev->hw, data, len);
    if (ret < 0) {
        LOG_ERROR("UART%d 写入失败: %d", id, ret);
        return ret;
    }
    
    LOG_DEBUG("UART%d 写入 %d 字节", id, ret);
    return ret;
}
```

### 13.2 应用状态机示例

```c
/**
 * \file            app_state_machine.c
 * \brief           应用状态机示例（带日志）
 */

#define LOG_MODULE "app.fsm"
#include "log/log.h"

/* 状态定义 */
typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_SHUTDOWN
} app_state_t;

static app_state_t g_current_state = STATE_INIT;

/* 状态名称 */
static const char* state_name(app_state_t state) {
    switch (state) {
        case STATE_INIT: return "INIT";
        case STATE_IDLE: return "IDLE";
        case STATE_RUNNING: return "RUNNING";
        case STATE_ERROR: return "ERROR";
        case STATE_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

/* 状态转换 */
static void change_state(app_state_t new_state) {
    if (g_current_state == new_state) {
        LOG_DEBUG("状态未变化: %s", state_name(new_state));
        return;
    }
    
    LOG_INFO("状态转换: %s -> %s", 
             state_name(g_current_state), state_name(new_state));
    
    /* 退出当前状态 */
    switch (g_current_state) {
        case STATE_RUNNING:
            LOG_DEBUG("退出 RUNNING 状态");
            /* 清理运行资源 */
            break;
        default:
            break;
    }
    
    /* 进入新状态 */
    g_current_state = new_state;
    
    switch (new_state) {
        case STATE_INIT:
            LOG_DEBUG("进入 INIT 状态");
            /* 初始化资源 */
            break;
        case STATE_RUNNING:
            LOG_DEBUG("进入 RUNNING 状态");
            /* 启动运行 */
            break;
        case STATE_ERROR:
            LOG_ERROR("进入 ERROR 状态");
            /* 错误处理 */
            break;
        default:
            break;
    }
}

/* 状态机主循环 */
void app_state_machine_run(void) {
    LOG_INFO("状态机启动");
    
    change_state(STATE_IDLE);
    
    while (g_current_state != STATE_SHUTDOWN) {
        switch (g_current_state) {
            case STATE_IDLE:
                /* 空闲状态处理 */
                if (should_start()) {
                    change_state(STATE_RUNNING);
                }
                break;
                
            case STATE_RUNNING:
                /* 运行状态处理 */
                if (error_occurred()) {
                    change_state(STATE_ERROR);
                } else if (should_stop()) {
                    change_state(STATE_IDLE);
                }
                break;
                
            case STATE_ERROR:
                /* 错误状态处理 */
                if (error_recovered()) {
                    change_state(STATE_IDLE);
                }
                break;
                
            default:
                LOG_FATAL("未知状态: %d", g_current_state);
                change_state(STATE_ERROR);
                break;
        }
        
        osal_task_delay(100);
    }
    
    LOG_INFO("状态机停止");
}
```

### 13.3 网络协议示例

```c
/**
 * \file            protocol.c
 * \brief           网络协议示例（带日志）
 */

#define LOG_MODULE "net.protocol"
#include "log/log.h"

/* 协议头 */
typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t seq;
} protocol_header_t;

/* 十六进制转储 */
static void log_hex_dump(const char* prefix, const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    char hex_str[256];
    size_t offset = 0;
    
    for (size_t i = 0; i < len && offset < sizeof(hex_str) - 3; i++) {
        offset += snprintf(hex_str + offset, sizeof(hex_str) - offset,
                          "%02X ", bytes[i]);
    }
    
    LOG_DEBUG("%s: %s", prefix, hex_str);
}

/* 发送数据包 */
int protocol_send(const void* data, size_t len) {
    LOG_TRACE("protocol_send: len=%zu", len);
    
    if (len > 1024) {
        LOG_WARN("数据包较大: %zu bytes", len);
    }
    
    /* 构造协议头 */
    protocol_header_t header = {
        .version = 1,
        .type = 0x01,
        .length = len,
        .seq = get_next_seq()
    };
    
    LOG_DEBUG("发送数据包: version=%d, type=0x%02X, len=%d, seq=%u",
              header.version, header.type, header.length, header.seq);
    
    /* 转储协议头 */
    log_hex_dump("Header", &header, sizeof(header));
    
    /* 发送 */
    int ret = network_send(&header, sizeof(header));
    if (ret < 0) {
        LOG_ERROR("发送协议头失败: %d", ret);
        return ret;
    }
    
    ret = network_send(data, len);
    if (ret < 0) {
        LOG_ERROR("发送数据失败: %d", ret);
        return ret;
    }
    
    LOG_INFO("数据包发送成功: seq=%u, len=%zu", header.seq, len);
    return 0;
}

/* 接收数据包 */
int protocol_receive(void* buffer, size_t size) {
    LOG_TRACE("protocol_receive: size=%zu", size);
    
    /* 接收协议头 */
    protocol_header_t header;
    int ret = network_receive(&header, sizeof(header));
    if (ret < 0) {
        LOG_ERROR("接收协议头失败: %d", ret);
        return ret;
    }
    
    LOG_DEBUG("接收数据包: version=%d, type=0x%02X, len=%d, seq=%u",
              header.version, header.type, header.length, header.seq);
    
    /* 验证协议头 */
    if (header.version != 1) {
        LOG_ERROR("不支持的协议版本: %d", header.version);
        return -1;
    }
    
    if (header.length > size) {
        LOG_ERROR("缓冲区太小: 需要 %d, 提供 %zu", header.length, size);
        return -1;
    }
    
    /* 接收数据 */
    ret = network_receive(buffer, header.length);
    if (ret < 0) {
        LOG_ERROR("接收数据失败: %d", ret);
        return ret;
    }
    
    LOG_INFO("数据包接收成功: seq=%u, len=%d", header.seq, header.length);
    return header.length;
}
```

## 14. 参考资料

### 14.1 相关文档

- [用户使用指南](USER_GUIDE.md) - 详细的使用说明
- [移植指南](PORTING_GUIDE.md) - 平台移植说明
- [测试指南](TEST_GUIDE.md) - 测试方法和用例
- [故障排查指南](TROUBLESHOOTING.md) - 常见问题解决
- [变更日志](CHANGELOG.md) - 版本变更记录

### 14.2 API 参考

完整的 API 参考请查看头文件：
- `framework/log/include/log/log.h` - 核心 API
- `framework/log/include/log/log_backend.h` - 后端接口
- `framework/log/include/log/log_def.h` - 类型定义

### 14.3 示例代码

示例代码位于：
- `examples/log/` - 完整示例
- `tests/log/` - 测试用例

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
