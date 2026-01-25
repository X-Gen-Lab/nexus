# Log Framework 移植指南

本文档说明如何将 Log Framework 移植到不同的平台和环境。

## 1. 移植概述

### 1.1 依赖项

Log Framework 依赖以下组件：

- **OSAL (操作系统抽象层)**: 提供互斥锁、队列、任务支持
- **HAL (硬件抽象层)**: UART 后端需要（可选）
- **标准 C 库**: stdio.h, stdarg.h, string.h, stdint.h

### 1.2 可移植性设计

- 使用标准 C99 语法
- 避免平台特定代码
- 通过抽象层隔离平台差异
- 支持静态内存分配

### 1.3 移植工作量

| 平台类型 | 工作量 | 说明 |
|---------|--------|------|
| 已支持 RTOS | 低 | 只需配置编译选项 |
| 新 RTOS | 中 | 需要实现 OSAL 接口 |
| 裸机 | 中 | 需要提供简单的互斥锁实现 |
| 新硬件平台 | 中-高 | 需要实现 UART HAL 接口 |

## 2. 平台适配

### 2.1 OSAL 互斥锁接口

Log Framework 需要以下 OSAL 互斥锁接口：

```c
/* osal/osal_mutex.h */

typedef struct osal_mutex osal_mutex_t;

/* 创建互斥锁 */
osal_status_t osal_mutex_create(osal_mutex_t* mutex);

/* 销毁互斥锁 */
osal_status_t osal_mutex_destroy(osal_mutex_t* mutex);

/* 锁定互斥锁 */
osal_status_t osal_mutex_lock(osal_mutex_t* mutex);

/* 解锁互斥锁 */
osal_status_t osal_mutex_unlock(osal_mutex_t* mutex);
```

#### 2.1.1 FreeRTOS 实现示例

```c
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct {
    SemaphoreHandle_t handle;
} osal_mutex_t;

osal_status_t osal_mutex_create(osal_mutex_t* mutex) {
    mutex->handle = xSemaphoreCreateMutex();
    return (mutex->handle != NULL) ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_destroy(osal_mutex_t* mutex) {
    vSemaphoreDelete(mutex->handle);
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_t* mutex) {
    return (xSemaphoreTake(mutex->handle, portMAX_DELAY) == pdTRUE) 
           ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_mutex_unlock(osal_mutex_t* mutex) {
    return (xSemaphoreGive(mutex->handle) == pdTRUE) 
           ? OSAL_OK : OSAL_ERROR;
}
```

### 2.2 OSAL 队列接口（异步模式）

异步模式需要队列支持：

```c
/* osal/osal_queue.h */

typedef struct osal_queue osal_queue_t;

/* 创建队列 */
osal_status_t osal_queue_create(osal_queue_t* queue, size_t item_size, 
                                size_t queue_length);

/* 销毁队列 */
osal_status_t osal_queue_destroy(osal_queue_t* queue);

/* 发送消息 */
osal_status_t osal_queue_send(osal_queue_t* queue, const void* item, 
                              uint32_t timeout_ms);

/* 接收消息 */
osal_status_t osal_queue_receive(osal_queue_t* queue, void* item, 
                                 uint32_t timeout_ms);
```

### 2.3 OSAL 任务接口（异步模式）

```c
/* osal/osal_task.h */

typedef struct osal_task osal_task_t;
typedef void (*osal_task_fn)(void* arg);

/* 创建任务 */
osal_status_t osal_task_create(osal_task_t* task, const char* name,
                               osal_task_fn func, void* arg,
                               size_t stack_size, int priority);

/* 删除任务 */
osal_status_t osal_task_delete(osal_task_t* task);
```

### 2.4 UART HAL 接口

UART 后端需要以下 HAL 接口：

```c
/* hal/interface/nx_uart.h */

typedef struct nx_uart nx_uart_t;

/* 打开 UART */
nx_uart_t* nx_uart_open(int id);

/* 关闭 UART */
void nx_uart_close(nx_uart_t* uart);

/* 配置 UART */
int nx_uart_configure(nx_uart_t* uart, const nx_uart_config_t* config);

/* 写入数据 */
int nx_uart_write(nx_uart_t* uart, const uint8_t* data, size_t len, 
                 uint32_t timeout_ms);
```

#### 2.4.1 STM32 HAL 实现示例

```c
#include "stm32f4xx_hal.h"

typedef struct {
    UART_HandleTypeDef huart;
    int id;
} nx_uart_stm32_t;

nx_uart_t* nx_uart_open(int id) {
    nx_uart_stm32_t* uart = malloc(sizeof(nx_uart_stm32_t));
    if (uart == NULL) {
        return NULL;
    }
    
    uart->id = id;
    
    /* 配置 UART 硬件 */
    if (id == 0) {
        uart->huart.Instance = USART1;
    } else if (id == 1) {
        uart->huart.Instance = USART2;
    }
    
    return (nx_uart_t*)uart;
}

int nx_uart_write(nx_uart_t* uart, const uint8_t* data, size_t len,
                 uint32_t timeout_ms) {
    nx_uart_stm32_t* stm32_uart = (nx_uart_stm32_t*)uart;
    
    HAL_StatusTypeDef status = HAL_UART_Transmit(&stm32_uart->huart,
                                                  (uint8_t*)data, len,
                                                  timeout_ms);
    
    return (status == HAL_OK) ? len : -1;
}
```

## 3. 编译配置

### 3.1 CMake 配置

```cmake
# 平台选择
set(LOG_PLATFORM "stm32f4" CACHE STRING "Target platform")

# 功能开关
option(LOG_ENABLE_ASYNC "Enable async mode" ON)
option(LOG_ENABLE_COLOR "Enable ANSI color" ON)
option(LOG_ENABLE_UART_BACKEND "Enable UART backend" ON)

# 平台特定源文件
if(LOG_PLATFORM STREQUAL "stm32f4")
    target_sources(log_framework PRIVATE
        src/backend/log_backend_uart_stm32.c
    )
    target_compile_definitions(log_framework PRIVATE
        LOG_PLATFORM_STM32F4
    )
elseif(LOG_PLATFORM STREQUAL "esp32")
    target_sources(log_framework PRIVATE
        src/backend/log_backend_uart_esp32.c
    )
    target_compile_definitions(log_framework PRIVATE
        LOG_PLATFORM_ESP32
    )
endif()

# 异步模式
if(LOG_ENABLE_ASYNC)
    target_compile_definitions(log_framework PRIVATE
        LOG_ENABLE_ASYNC=1
    )
endif()
```

### 3.2 Kconfig 配置

```kconfig
menu "Log Framework"

config LOG_FRAMEWORK
    bool "Enable Log Framework"
    default y
    help
      Enable the logging framework.

if LOG_FRAMEWORK

config LOG_DEFAULT_LEVEL
    int "Default log level"
    default 2
    range 0 6
    help
      Default log level (0=TRACE, 2=INFO, 6=NONE).

config LOG_MAX_MSG_LEN
    int "Maximum message length"
    default 128
    range 64 512
    help
      Maximum length of log messages.

config LOG_ENABLE_ASYNC
    bool "Enable async mode"
    default n
    depends on OSAL_QUEUE && OSAL_TASK
    help
      Enable asynchronous logging mode.

config LOG_ASYNC_BUFFER_SIZE
    int "Async buffer size"
    default 1024
    depends on LOG_ENABLE_ASYNC
    help
      Size of async buffer in bytes.

config LOG_ENABLE_COLOR
    bool "Enable ANSI color"
    default y
    help
      Enable ANSI color codes in log output.

config LOG_ENABLE_UART_BACKEND
    bool "Enable UART backend"
    default y
    depends on HAL_UART
    help
      Enable UART output backend.

endif # LOG_FRAMEWORK

endmenu
```

### 3.3 编译宏定义

```c
/* log_port.h - 平台配置头文件 */

#ifndef LOG_PORT_H
#define LOG_PORT_H

/* 平台检测 */
#if defined(STM32F4)
    #define LOG_PLATFORM_STM32F4
#elif defined(ESP32)
    #define LOG_PLATFORM_ESP32
#elif defined(__linux__)
    #define LOG_PLATFORM_LINUX
#endif

/* 功能开关 */
#ifndef LOG_ENABLE_ASYNC
    #define LOG_ENABLE_ASYNC 0
#endif

#ifndef LOG_ENABLE_COLOR
    #define LOG_ENABLE_COLOR 1
#endif

/* 默认配置值 */
#ifndef LOG_DEFAULT_LEVEL
    #define LOG_DEFAULT_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_MAX_MSG_LEN
    #define LOG_MAX_MSG_LEN 128
#endif

#ifndef LOG_MAX_BACKENDS
    #define LOG_MAX_BACKENDS 4
#endif

/* 平台特定配置 */
#ifdef LOG_PLATFORM_STM32F4
    #define LOG_UART_DEFAULT_TIMEOUT 1000
#elif defined(LOG_PLATFORM_ESP32)
    #define LOG_UART_DEFAULT_TIMEOUT 500
#endif

#endif /* LOG_PORT_H */
```

## 4. 移植步骤

### 4.1 准备工作

1. **评估依赖**
   - 检查 OSAL 是否已实现
   - 确认 HAL UART 接口是否可用
   - 确定是否需要异步模式

2. **规划内存**
   - 计算所需 RAM 大小
   - 评估栈使用情况
   - 确定是否使用异步模式

3. **选择功能**
   - 确定需要启用的功能
   - 评估代码大小影响

### 4.2 实现步骤

#### 步骤 1: 实现 OSAL 接口

```c
/* 1. 创建 osal_mutex.c */
/* 2. 实现互斥锁接口 */
/* 3. 测试互斥锁功能 */

void test_osal_mutex(void) {
    osal_mutex_t mutex;
    
    assert(osal_mutex_create(&mutex) == OSAL_OK);
    assert(osal_mutex_lock(&mutex) == OSAL_OK);
    assert(osal_mutex_unlock(&mutex) == OSAL_OK);
    assert(osal_mutex_destroy(&mutex) == OSAL_OK);
    
    printf("OSAL mutex test passed\n");
}
```

#### 步骤 2: 实现 UART HAL 接口（可选）

```c
/* 1. 创建 nx_uart_platform.c */
/* 2. 实现 UART 接口 */
/* 3. 测试 UART 读写 */

void test_uart(void) {
    nx_uart_t* uart = nx_uart_open(0);
    assert(uart != NULL);
    
    const char* msg = "Test message\n";
    int ret = nx_uart_write(uart, (const uint8_t*)msg, strlen(msg), 1000);
    assert(ret == strlen(msg));
    
    nx_uart_close(uart);
    printf("UART test passed\n");
}
```

#### 步骤 3: 配置编译系统

```cmake
# 添加到项目 CMakeLists.txt

add_subdirectory(framework/log)

target_link_libraries(my_app PRIVATE
    log_framework
    osal
    hal
)
```

#### 步骤 4: 测试基本功能

```c
void test_log_basic(void) {
    /* 初始化 */
    log_status_t status = log_init(NULL);
    assert(status == LOG_OK);
    
    /* 注册 Console 后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);
    
    /* 记录日志 */
    LOG_INFO("Log framework test");
    LOG_DEBUG("Debug value: %d", 42);
    
    /* 清理 */
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
    
    printf("Log basic test passed\n");
}
```

#### 步骤 5: 测试 UART 后端（如果使用）

```c
void test_log_uart(void) {
    log_init(NULL);
    
    /* 初始化 UART */
    nx_uart_t* uart = nx_uart_open(0);
    assert(uart != NULL);
    
    /* 创建 UART 后端 */
    log_backend_t* uart_backend = log_backend_uart_create(uart);
    assert(uart_backend != NULL);
    
    log_backend_register(uart_backend);
    
    /* 记录日志 */
    LOG_INFO("UART backend test");
    
    /* 清理 */
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart_backend);
    nx_uart_close(uart);
    log_deinit();
    
    printf("UART backend test passed\n");
}
```

### 4.3 验证清单

- [ ] OSAL 互斥锁正常工作
- [ ] UART 读写功能正常（如果使用）
- [ ] 基本日志记录成功
- [ ] 多后端输出正常
- [ ] 异步模式正常（如果使用）
- [ ] 内存使用在预期范围内
- [ ] 性能满足要求
- [ ] 所有测试用例通过

## 5. 平台特定优化

### 5.1 内存优化

```c
/* 针对资源受限平台 */
log_config_t config = {
    .level = LOG_LEVEL_INFO,
    .format = "[%l] %m",      /* 简化格式 */
    .async_mode = false,      /* 禁用异步 */
    .max_msg_len = 64,        /* 减少消息长度 */
    .color_enabled = false    /* 禁用颜色 */
};
```

### 5.2 性能优化

```c
/* 使用编译时级别过滤 */
#define LOG_COMPILE_LEVEL LOG_LEVEL_INFO

/* 禁用不需要的功能 */
#define LOG_ENABLE_COLOR 0
#define LOG_ENABLE_ASYNC 0
```

### 5.3 UART 优化

```c
/* 使用 DMA 传输 */
int nx_uart_write_dma(nx_uart_t* uart, const uint8_t* data, size_t len) {
    /* 启动 DMA 传输 */
    HAL_UART_Transmit_DMA(&huart, (uint8_t*)data, len);
    
    /* 等待完成 */
    while (HAL_UART_GetState(&huart) != HAL_UART_STATE_READY) {
        /* 等待 */
    }
    
    return len;
}
```

## 6. 故障排查

### 6.1 编译错误

**问题**: 找不到 OSAL 头文件

**解决**:
```cmake
target_include_directories(log_framework PRIVATE
    ${OSAL_INCLUDE_DIR}
)
```

**问题**: 链接错误，未定义的引用

**解决**:
```cmake
target_link_libraries(log_framework PRIVATE
    osal
    hal
)
```

### 6.2 运行时错误

**问题**: 初始化失败

**检查**:
- OSAL 是否正确初始化
- 内存是否足够
- 配置参数是否有效

**问题**: UART 输出失败

**检查**:
- UART 是否已初始化
- 波特率是否正确
- 超时设置是否合理

### 6.3 性能问题

**问题**: 日志记录太慢

**优化**:
- 使用异步模式
- 提高日志级别
- 简化格式化字符串
- 使用 DMA 传输

## 7. 示例项目

完整的移植示例可以在以下目录找到：

- `examples/stm32f4_log/` - STM32F4 平台示例
- `examples/esp32_log/` - ESP32 平台示例
- `examples/linux_log/` - Linux 平台示例
- `examples/baremetal_log/` - 裸机平台示例

## 8. 技术支持

如果在移植过程中遇到问题，可以：

1. 查看 [故障排查指南](TROUBLESHOOTING.md)
2. 参考 [API 文档](USER_GUIDE.md)
3. 查看示例项目
4. 提交 Issue 到项目仓库

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
