# Log Framework 故障排查指南

本文档提供 Log Framework 常见问题的诊断和解决方案。

## 目录

1. [初始化问题](#1-初始化问题)
2. [日志输出问题](#2-日志输出问题)
3. [性能问题](#3-性能问题)
4. [内存问题](#4-内存问题)
5. [后端问题](#5-后端问题)
6. [异步模式问题](#6-异步模式问题)
7. [编译问题](#7-编译问题)
8. [调试技巧](#8-调试技巧)

## 1. 初始化问题

### 1.1 初始化失败

**症状**: `log_init()` 返回错误码

**可能原因**:
1. 内存不足
2. OSAL 未初始化
3. 配置参数无效

**诊断步骤**:
```c
log_status_t status = log_init(NULL);
printf("Init status: %d\n", status);

switch (status) {
    case LOG_ERROR_NO_MEMORY:
        printf("内存不足\n");
        break;
    case LOG_ERROR_INVALID_PARAM:
        printf("配置参数无效\n");
        break;
    case LOG_ERROR_ALREADY_INIT:
        printf("已经初始化\n");
        break;
}
```

**解决方案**:
```c
/* 方案 1: 减少内存使用 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.max_msg_len = 64;      /* 减少消息长度 */
config.async_mode = false;    /* 禁用异步模式 */
log_init(&config);

/* 方案 2: 确保 OSAL 已初始化 */
osal_init();
log_init(NULL);

/* 方案 3: 检查是否已初始化 */
if (!log_is_initialized()) {
    log_init(NULL);
}
```

### 1.2 重复初始化

**症状**: 第二次调用 `log_init()` 返回 `LOG_ERROR_ALREADY_INIT`

**原因**: 日志系统已经初始化

**解决方案**:
```c
/* 检查初始化状态 */
if (!log_is_initialized()) {
    log_init(NULL);
} else {
    printf("日志系统已初始化\n");
}

/* 或者先反初始化 */
log_deinit();
log_init(NULL);
```

## 2. 日志输出问题

### 2.1 日志不输出

**症状**: 调用 `LOG_INFO()` 但没有任何输出

**可能原因**:
1. 日志系统未初始化
2. 日志级别过滤
3. 没有注册后端
4. 后端被禁用
5. 编译时级别过滤

**诊断步骤**:
```c
/* 步骤 1: 检查初始化 */
if (!log_is_initialized()) {
    printf("日志系统未初始化\n");
    log_init(NULL);
}

/* 步骤 2: 检查全局级别 */
log_level_t level = log_get_level();
printf("全局级别: %d\n", level);
if (level > LOG_LEVEL_INFO) {
    printf("级别过滤了 INFO 消息\n");
    log_set_level(LOG_LEVEL_INFO);
}

/* 步骤 3: 检查模块级别 */
log_level_t module_level = log_module_get_level("mymodule");
printf("模块级别: %d\n", module_level);

/* 步骤 4: 检查后端 */
log_backend_t* backend = log_backend_get("console");
if (backend == NULL) {
    printf("Console 后端未注册\n");
    log_backend_register(log_backend_console_create());
} else if (!backend->enabled) {
    printf("Console 后端已禁用\n");
    log_backend_enable("console", true);
}

/* 步骤 5: 检查编译时级别 */
#if LOG_COMPILE_LEVEL > LOG_LEVEL_INFO
    printf("INFO 日志在编译时被过滤\n");
#endif
```

**解决方案**:
```c
/* 完整的初始化和配置 */
void setup_logging(void) {
    /* 初始化 */
    log_init(NULL);
    
    /* 设置级别 */
    log_set_level(LOG_LEVEL_TRACE);
    
    /* 注册后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    /* 测试输出 */
    LOG_INFO("日志系统已配置");
}
```

### 2.2 部分日志不输出

**症状**: 某些模块的日志不输出

**原因**: 模块级别过滤

**诊断**:
```c
/* 检查模块级别 */
log_level_t level = log_module_get_level("mymodule");
printf("模块 'mymodule' 级别: %d\n", level);

/* 列出所有模块过滤器 */
void list_module_filters(void) {
    /* 需要访问内部状态，或添加 API */
}
```

**解决方案**:
```c
/* 清除模块级别 */
log_module_clear_level("mymodule");

/* 或设置为更低级别 */
log_module_set_level("mymodule", LOG_LEVEL_TRACE);

/* 清除所有模块级别 */
log_module_clear_all();
```

### 2.3 日志输出乱码

**症状**: 日志输出包含乱码字符

**可能原因**:
1. UART 波特率不匹配
2. 字符编码问题
3. 缓冲区溢出
4. 并发访问冲突

**诊断**:
```c
/* 检查 UART 配置 */
nx_uart_config_t config;
nx_uart_get_config(uart, &config);
printf("波特率: %d\n", config.baudrate);

/* 检查消息长度 */
size_t max_len = log_get_max_msg_len();
printf("最大消息长度: %zu\n", max_len);
```

**解决方案**:
```c
/* 方案 1: 确保 UART 配置正确 */
nx_uart_config_t uart_config = {
    .baudrate = 115200,
    .data_bits = 8,
    .stop_bits = 1,
    .parity = NX_UART_PARITY_NONE
};
nx_uart_configure(uart, &uart_config);

/* 方案 2: 增加消息长度限制 */
log_set_max_msg_len(256);

/* 方案 3: 使用同步模式避免并发 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.async_mode = false;
log_init(&config);
```

## 3. 性能问题

### 3.1 日志记录太慢

**症状**: 日志记录导致明显的性能下降

**可能原因**:
1. 使用同步模式
2. 日志级别太低
3. 格式化字符串太复杂
4. UART 传输慢

**诊断**:
```c
#include <time.h>

void measure_log_performance(void) {
    clock_t start = clock();
    
    for (int i = 0; i < 1000; i++) {
        LOG_INFO("Test message %d", i);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("1000 条日志耗时: %.3f 秒\n", elapsed);
    printf("平均每条: %.3f 毫秒\n", elapsed * 1000 / 1000);
}
```

**解决方案**:
```c
/* 方案 1: 启用异步模式 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.async_mode = true;
config.buffer_size = 4096;
log_init(&config);

/* 方案 2: 提高日志级别 */
log_set_level(LOG_LEVEL_WARN);  /* 只记录警告及以上 */

/* 方案 3: 简化格式 */
log_set_format("[%l] %m");  /* 最简格式 */

/* 方案 4: 使用编译时过滤 */
#define LOG_COMPILE_LEVEL LOG_LEVEL_INFO

/* 方案 5: 禁用不需要的后端 */
log_backend_enable("uart", false);
```

### 3.2 异步模式延迟高

**症状**: 异步模式下日志输出延迟明显

**原因**: 
1. 队列深度不足
2. 异步任务优先级低
3. 后端写入慢

**解决方案**:
```c
/* 增加队列深度 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.async_mode = true;
config.async_queue_size = 64;  /* 增加到 64 */
log_init(&config);

/* 提高异步任务优先级（需要修改源码或配置）*/
#define LOG_ASYNC_TASK_PRIORITY 16  /* 更高优先级 */

/* 定期刷新 */
void periodic_flush(void) {
    log_async_flush();
}
```

## 4. 内存问题

### 4.1 内存不足

**症状**: 初始化返回 `LOG_ERROR_NO_MEMORY`

**原因**: 
1. 异步缓冲区太大
2. 消息长度太大
3. 系统内存不足

**诊断**:
```c
/* 计算内存需求 */
void calculate_memory_usage(void) {
    size_t base = sizeof(log_context_t);  /* 约 200 bytes */
    size_t async_buffer = 4096;           /* 异步缓冲区 */
    size_t async_queue = 32 * 256;        /* 队列 */
    size_t total = base + async_buffer + async_queue;
    
    printf("预计内存使用: %zu bytes\n", total);
}
```

**解决方案**:
```c
/* 减少内存使用 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.max_msg_len = 64;       /* 减少到 64 */
config.async_mode = false;     /* 禁用异步 */
log_init(&config);

/* 或减少异步缓冲区 */
config.async_mode = true;
config.buffer_size = 1024;     /* 减少到 1KB */
config.async_queue_size = 16;  /* 减少队列 */
log_init(&config);
```

### 4.2 内存泄漏

**症状**: 长时间运行后内存持续增长

**诊断**:
```c
/* 使用 Valgrind 检测 */
valgrind --leak-check=full ./my_app

/* 或使用 AddressSanitizer */
/* 编译时添加: -fsanitize=address */
```

**可能原因**:
1. 后端未正确销毁
2. 异步缓冲区未释放

**解决方案**:
```c
/* 确保正确清理 */
void cleanup_logging(void) {
    /* 刷新异步消息 */
    if (log_is_async_mode()) {
        log_async_flush();
    }
    
    /* 注销所有后端 */
    log_backend_unregister("console");
    log_backend_unregister("uart");
    log_backend_unregister("memory");
    
    /* 反初始化 */
    log_deinit();
}
```

## 5. 后端问题

### 5.1 Console 后端不工作

**症状**: Console 后端注册成功但无输出

**诊断**:
```c
/* 检查 stdout */
printf("Testing stdout\n");
fflush(stdout);

/* 检查后端状态 */
log_backend_t* backend = log_backend_get("console");
if (backend != NULL) {
    printf("后端启用: %d\n", backend->enabled);
    printf("最小级别: %d\n", backend->min_level);
}
```

**解决方案**:
```c
/* 确保 stdout 可用 */
setvbuf(stdout, NULL, _IONBF, 0);  /* 禁用缓冲 */

/* 检查后端级别 */
log_backend_t* console = log_backend_get("console");
if (console != NULL) {
    console->min_level = LOG_LEVEL_TRACE;
}
```

### 5.2 UART 后端输出失败

**症状**: UART 后端注册成功但无输出或输出不完整

**诊断**:
```c
/* 测试 UART 直接写入 */
nx_uart_t* uart = nx_uart_open(0);
const char* test = "UART test\n";
int ret = nx_uart_write(uart, (const uint8_t*)test, strlen(test), 1000);
printf("UART 写入返回: %d\n", ret);

/* 检查超时设置 */
log_backend_t* uart_backend = log_backend_get("uart");
if (uart_backend != NULL) {
    /* 获取超时（需要 API 支持）*/
}
```

**解决方案**:
```c
/* 增加超时 */
log_backend_t* uart_backend = log_backend_uart_create(uart);
log_backend_uart_set_timeout(uart_backend, 5000);  /* 5 秒 */

/* 确保 UART 正确初始化 */
nx_uart_config_t config = {
    .baudrate = 115200,
    .data_bits = 8,
    .stop_bits = 1,
    .parity = NX_UART_PARITY_NONE
};
nx_uart_configure(uart, &config);

/* 使用 DMA 模式（如果支持）*/
/* 需要平台特定实现 */
```

### 5.3 Memory 后端溢出

**症状**: Memory 后端数据丢失

**原因**: 环形缓冲区溢出

**解决方案**:
```c
/* 增加缓冲区大小 */
log_backend_t* memory = log_backend_memory_create(8192);  /* 8KB */

/* 定期读取 */
void periodic_read_memory_backend(void) {
    log_backend_t* memory = log_backend_get("memory");
    if (memory != NULL) {
        char buffer[1024];
        size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
        
        /* 处理数据 */
        process_log_data(buffer, len);
        
        /* 清空缓冲区 */
        log_backend_memory_clear(memory);
    }
}
```

## 6. 异步模式问题

### 6.1 消息丢失

**症状**: 异步模式下部分消息丢失

**原因**: 
1. 队列满
2. 应用退出前未刷新

**诊断**:
```c
/* 检查待处理消息 */
size_t pending = log_async_pending();
printf("待处理消息: %zu\n", pending);

/* 检查策略 */
log_async_policy_t policy = log_async_get_policy();
printf("队列满策略: %d\n", policy);
```

**解决方案**:
```c
/* 方案 1: 增加队列大小 */
log_config_t config = LOG_CONFIG_DEFAULT;
config.async_mode = true;
config.async_queue_size = 64;
log_init(&config);

/* 方案 2: 使用阻塞策略 */
log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);

/* 方案 3: 退出前刷新 */
void app_exit(void) {
    log_async_flush();
    log_deinit();
}

/* 方案 4: 定期刷新 */
void periodic_task(void) {
    log_async_flush();
}
```

### 6.2 异步任务崩溃

**症状**: 系统在异步模式下崩溃

**可能原因**:
1. 栈溢出
2. 后端写入失败
3. 队列损坏

**诊断**:
```c
/* 检查栈使用 */
#define LOG_ASYNC_TASK_STACK_SIZE 4096  /* 增加栈大小 */

/* 添加错误处理 */
void async_task_error_handler(void) {
    printf("异步任务错误\n");
    /* 记录错误信息 */
}
```

**解决方案**:
```c
/* 增加栈大小 */
#define LOG_ASYNC_TASK_STACK_SIZE 4096

/* 添加异常处理 */
/* 在异步任务中添加 try-catch 或错误检查 */
```

## 7. 编译问题

### 7.1 编译时日志被移除

**症状**: 某些日志宏不生成代码

**原因**: `LOG_COMPILE_LEVEL` 设置过高

**诊断**:
```c
/* 检查编译级别 */
#if LOG_COMPILE_LEVEL > LOG_LEVEL_DEBUG
    #warning "DEBUG 日志在编译时被移除"
#endif

printf("编译级别: %d\n", LOG_COMPILE_LEVEL);
```

**解决方案**:
```c
/* 在编译选项中设置 */
/* CMakeLists.txt */
add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_TRACE)

/* 或在代码中定义 */
#define LOG_COMPILE_LEVEL LOG_LEVEL_TRACE
#include "log/log.h"
```

### 7.2 链接错误

**症状**: 链接时出现未定义引用

**可能原因**:
1. 未链接 OSAL 库
2. 未链接 HAL 库
3. 后端实现缺失

**解决方案**:
```cmake
# CMakeLists.txt
target_link_libraries(my_app PRIVATE
    log_framework
    osal
    hal
)
```

## 8. 调试技巧

### 8.1 启用调试输出

```c
/* 在日志系统内部添加调试输出 */
#define LOG_DEBUG_ENABLED 1

#if LOG_DEBUG_ENABLED
    #define LOG_INTERNAL_DEBUG(fmt, ...) \
        printf("[LOG_DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_INTERNAL_DEBUG(fmt, ...) ((void)0)
#endif
```

### 8.2 使用 Memory 后端调试

```c
/* 使用 Memory 后端捕获日志 */
void debug_with_memory_backend(void) {
    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);
    
    /* 执行测试 */
    test_function();
    
    /* 读取并分析日志 */
    char buffer[4096];
    size_t len = log_backend_memory_read(memory, buffer, sizeof(buffer));
    
    printf("捕获的日志:\n%.*s\n", (int)len, buffer);
    
    /* 清理 */
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);
}
```

### 8.3 性能分析

```c
/* 测量各个阶段的时间 */
void profile_logging(void) {
    clock_t t1, t2, t3, t4;
    
    t1 = clock();
    log_init(NULL);
    t2 = clock();
    
    LOG_INFO("Test message");
    t3 = clock();
    
    log_deinit();
    t4 = clock();
    
    printf("初始化: %.3f ms\n", (double)(t2 - t1) / CLOCKS_PER_SEC * 1000);
    printf("日志记录: %.3f ms\n", (double)(t3 - t2) / CLOCKS_PER_SEC * 1000);
    printf("反初始化: %.3f ms\n", (double)(t4 - t3) / CLOCKS_PER_SEC * 1000);
}
```

### 8.4 使用断言

```c
/* 在关键路径添加断言 */
void log_write_with_asserts(log_level_t level, const char* module,
                            const char* file, int line, const char* func,
                            const char* fmt, ...) {
    assert(log_is_initialized());
    assert(module != NULL);
    assert(fmt != NULL);
    assert(level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_FATAL);
    
    /* 正常处理 */
}
```

### 8.5 日志统计

```c
/* 添加统计信息 */
typedef struct {
    size_t total_messages;
    size_t dropped_messages;
    size_t backend_errors;
    size_t format_errors;
} log_stats_t;

void print_log_stats(void) {
    log_stats_t stats;
    /* 获取统计信息（需要 API 支持）*/
    
    printf("日志统计:\n");
    printf("  总消息数: %zu\n", stats.total_messages);
    printf("  丢弃消息: %zu\n", stats.dropped_messages);
    printf("  后端错误: %zu\n", stats.backend_errors);
    printf("  格式错误: %zu\n", stats.format_errors);
}
```

## 9. 常见错误码

| 错误码 | 名称 | 描述 | 解决方案 |
|--------|------|------|----------|
| 0 | LOG_OK | 成功 | - |
| 1 | LOG_ERROR | 通用错误 | 检查具体错误信息 |
| 2 | LOG_ERROR_INVALID_PARAM | 参数错误 | 检查参数有效性 |
| 3 | LOG_ERROR_NOT_INIT | 未初始化 | 调用 log_init() |
| 4 | LOG_ERROR_NO_MEMORY | 内存不足 | 减少内存使用 |
| 5 | LOG_ERROR_FULL | 缓冲区满 | 增加缓冲区或刷新 |
| 6 | LOG_ERROR_BACKEND | 后端错误 | 检查后端配置 |
| 7 | LOG_ERROR_ALREADY_INIT | 已初始化 | 先调用 log_deinit() |

## 10. 获取帮助

如果以上方法无法解决问题：

1. 查看 [用户指南](USER_GUIDE.md)
2. 查看 [设计文档](DESIGN.md)
3. 查看 [测试指南](TEST_GUIDE.md)
4. 提交 Issue 到项目仓库
5. 联系技术支持

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
