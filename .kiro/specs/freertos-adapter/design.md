# Design Document: FreeRTOS Adapter

## Overview

本设计文档描述 Nexus 平台 OSAL 层 FreeRTOS 适配器的技术设计。FreeRTOS 适配器将 OSAL 抽象接口映射到 FreeRTOS API，使应用程序能够在 FreeRTOS 实时操作系统上运行，同时保持与其他 OSAL 适配器的 API 兼容性。

### 设计目标

1. **完全兼容** - 实现所有 OSAL 接口，与 Baremetal/Native 适配器行为一致
2. **零开销抽象** - 直接映射到 FreeRTOS API，最小化运行时开销
3. **充分利用 FreeRTOS 特性** - 抢占式调度、优先级继承、ISR 安全 API
4. **可配置性** - 通过 FreeRTOSConfig.h 支持不同平台和应用需求

## Architecture

### 组件架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Application Layer                                │
│                    (使用 OSAL API 的应用代码)                             │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           OSAL Interface                                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │osal_task │ │osal_mutex│ │ osal_sem │ │osal_queue│ │osal_core │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      FreeRTOS Adapter Layer                              │
│                    (osal/adapters/freertos/)                             │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                    osal_freertos.c                                │  │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐    │  │
│  │  │Task Wrapper│ │Mutex Wrap  │ │ Sem Wrap   │ │Queue Wrap  │    │  │
│  │  │xTaskCreate │ │xSemaphore  │ │xSemaphore  │ │xQueueCreate│    │  │
│  │  │vTaskDelete │ │CreateMutex │ │CreateBinary│ │xQueueSend  │    │  │
│  │  └────────────┘ └────────────┘ └────────────┘ └────────────┘    │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         FreeRTOS Kernel                                  │
│                       (ext/freertos/)                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │  tasks.c │ │  queue.c │ │ timers.c │ │  list.c  │ │ portable │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Hardware / Platform                              │
│                    (ARM Cortex-M4, STM32F4, etc.)                       │
└─────────────────────────────────────────────────────────────────────────┘
```

### 文件结构

```
nexus/
├── ext/
│   └── freertos/                    # FreeRTOS Git submodule
│       ├── include/                 # FreeRTOS headers
│       │   ├── FreeRTOS.h
│       │   ├── task.h
│       │   ├── queue.h
│       │   ├── semphr.h
│       │   └── ...
│       ├── portable/
│       │   └── GCC/
│       │       └── ARM_CM4F/        # Cortex-M4F port
│       │           ├── port.c
│       │           └── portmacro.h
│       ├── tasks.c
│       ├── queue.c
│       ├── list.c
│       └── timers.c
│
├── osal/
│   ├── include/osal/
│   │   ├── osal.h                   # Main OSAL header
│   │   ├── osal_def.h               # Common definitions
│   │   ├── osal_task.h              # Task interface
│   │   ├── osal_mutex.h             # Mutex interface
│   │   ├── osal_sem.h               # Semaphore interface
│   │   └── osal_queue.h             # Queue interface
│   │
│   ├── adapters/
│   │   ├── baremetal/               # Existing baremetal adapter
│   │   ├── native/                  # Existing native adapter
│   │   └── freertos/                # NEW: FreeRTOS adapter
│   │       ├── osal_freertos.c      # FreeRTOS implementation
│   │       ├── FreeRTOSConfig.h     # Default config template
│   │       └── CMakeLists.txt       # Build configuration
│   │
│   └── CMakeLists.txt               # Updated to support FreeRTOS
│
└── platforms/
    └── stm32f4/
        └── FreeRTOSConfig.h         # Platform-specific config
```

## Components and Interfaces

### 1. FreeRTOS 源码集成

FreeRTOS 源码通过 Git submodule 集成到 `ext/freertos/` 目录。

```bash
# 添加 FreeRTOS submodule
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git ext/freertos
cd ext/freertos
git checkout V11.1.0
```

### 2. 优先级映射

OSAL 使用 0-31 优先级范围（0 最低，31 最高），需要映射到 FreeRTOS 优先级。

```c
/**
 * \brief           Map OSAL priority to FreeRTOS priority
 * \param[in]       osal_prio: OSAL priority (0-31)
 * \return          FreeRTOS priority (0 to configMAX_PRIORITIES-1)
 */
static inline UBaseType_t osal_to_freertos_priority(uint8_t osal_prio)
{
    /* OSAL: 0 (lowest) to 31 (highest) */
    /* FreeRTOS: 0 (lowest) to configMAX_PRIORITIES-1 (highest) */
    if (osal_prio > 31) {
        osal_prio = 31;
    }
    return (UBaseType_t)((osal_prio * (configMAX_PRIORITIES - 1)) / 31);
}
```

### 3. 超时转换

OSAL 使用毫秒超时，需要转换为 FreeRTOS ticks。

```c
/**
 * \brief           Convert OSAL timeout to FreeRTOS ticks
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          FreeRTOS ticks
 */
static inline TickType_t osal_to_freertos_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        return portMAX_DELAY;
    }
    if (timeout_ms == OSAL_NO_WAIT) {
        return 0;
    }
    return pdMS_TO_TICKS(timeout_ms);
}
```

### 4. ISR 上下文检测

```c
/**
 * \brief           Check if currently in ISR context
 * \return          true if in ISR, false otherwise
 */
static inline bool is_in_isr(void)
{
#if defined(__ARM_ARCH)
    /* Read IPSR register on ARM Cortex-M */
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return (ipsr != 0);
#else
    /* Use FreeRTOS port function if available */
    return xPortIsInsideInterrupt();
#endif
}
```

### 5. 核心 API 实现

#### 5.1 OSAL 初始化

```c
static bool s_osal_initialized = false;
static bool s_osal_running = false;

osal_status_t osal_init(void)
{
    if (s_osal_initialized) {
        return OSAL_OK;
    }
    /* FreeRTOS doesn't require explicit initialization */
    s_osal_initialized = true;
    return OSAL_OK;
}

void osal_start(void)
{
    if (!s_osal_initialized) {
        osal_init();
    }
    s_osal_running = true;
    vTaskStartScheduler();
    /* Should never reach here */
}

bool osal_is_running(void)
{
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}
```

#### 5.2 任务管理

```c
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle)
{
    if (config == NULL || handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    if (config->func == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    TaskHandle_t task_handle;
    UBaseType_t priority = osal_to_freertos_priority(config->priority);
    
    BaseType_t result = xTaskCreate(
        (TaskFunction_t)config->func,
        config->name ? config->name : "task",
        config->stack_size / sizeof(StackType_t),  /* Convert bytes to words */
        config->arg,
        priority,
        &task_handle
    );

    if (result != pdPASS) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_task_handle_t)task_handle;
    return OSAL_OK;
}

osal_status_t osal_task_delete(osal_task_handle_t handle)
{
    vTaskDelete((TaskHandle_t)handle);
    return OSAL_OK;
}

osal_status_t osal_task_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
    return OSAL_OK;
}
```

#### 5.3 互斥锁

```c
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_mutex_handle_t)mutex;
    return OSAL_OK;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    if (is_in_isr()) {
        return OSAL_ERROR_ISR;
    }

    TickType_t ticks = osal_to_freertos_ticks(timeout_ms);
    if (xSemaphoreTake((SemaphoreHandle_t)handle, ticks) != pdTRUE) {
        return OSAL_ERROR_TIMEOUT;
    }
    return OSAL_OK;
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    xSemaphoreGive((SemaphoreHandle_t)handle);
    return OSAL_OK;
}
```

#### 5.4 信号量

```c
osal_status_t osal_sem_create_binary(uint32_t initial,
                                     osal_sem_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    if (initial > 0) {
        xSemaphoreGive(sem);
    }

    *handle = (osal_sem_handle_t)sem;
    return OSAL_OK;
}

osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return OSAL_OK;
}
```

#### 5.5 消息队列

```c
osal_status_t osal_queue_create(size_t item_size,
                                size_t item_count,
                                osal_queue_handle_t* handle)
{
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    if (item_size == 0 || item_count == 0) {
        return OSAL_ERROR_INVALID_PARAM;
    }

    QueueHandle_t queue = xQueueCreate(item_count, item_size);
    if (queue == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }

    *handle = (osal_queue_handle_t)queue;
    return OSAL_OK;
}

osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle,
                                       const void* item)
{
    if (handle == NULL || item == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR((QueueHandle_t)handle, item, 
                          &xHigherPriorityTaskWoken) != pdTRUE) {
        return OSAL_ERROR_FULL;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return OSAL_OK;
}
```

## Data Models

### FreeRTOSConfig.h 模板

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *----------------------------------------------------------*/

/* Scheduler */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      168000000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    32
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (32 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Hook functions */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* Synchronization primitives */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0

/* Software timers */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* Co-routines (not used) */
#define configUSE_CO_ROUTINES                   0

/* Interrupt nesting (Cortex-M specific) */
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Debug and trace */
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* API includes */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_pcTaskGetTaskName               1

/* Cortex-M specific definitions */
#define configPRIO_BITS                         4
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* FreeRTOS MPU specific definitions (if using MPU) */
#define configENABLE_MPU                        0
#define configENABLE_FPU                        1
#define configENABLE_TRUSTZONE                  0

#endif /* FREERTOS_CONFIG_H */
```



## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: OSAL 初始化幂等性

*For any* sequence of osal_init() calls, all calls SHALL return OSAL_OK and the system SHALL remain in a valid initialized state.

**Validates: Requirements 3.4**

### Property 2: 任务生命周期一致性

*For any* valid task configuration (non-null function, priority 0-31, positive stack size), creating a task SHALL succeed with OSAL_OK and return a valid handle; subsequently suspending, resuming, and deleting that task SHALL each succeed with OSAL_OK.

**Validates: Requirements 4.1, 4.2, 4.3, 4.4**

### Property 3: 优先级映射正确性

*For any* OSAL priority value in range [0, 31], the priority mapping function SHALL produce a FreeRTOS priority in range [0, configMAX_PRIORITIES-1], where OSAL priority 0 maps to FreeRTOS priority 0 and OSAL priority 31 maps to the highest FreeRTOS priority.

**Validates: Requirements 4.7**

### Property 4: 任务名称保持

*For any* task created with a non-null name, osal_task_get_name() SHALL return a string equal to the original name.

**Validates: Requirements 4.9**

### Property 5: 互斥锁生命周期一致性

*For any* mutex created via osal_mutex_create(), the mutex SHALL be lockable and unlockable; after unlock, the mutex SHALL be deletable with OSAL_OK.

**Validates: Requirements 5.1, 5.2, 5.3, 5.4**

### Property 6: 互斥锁锁定/解锁往返

*For any* unlocked mutex, locking then unlocking SHALL return the mutex to unlocked state, allowing subsequent lock operations to succeed.

**Validates: Requirements 5.3, 5.4**

### Property 7: 信号量生命周期一致性

*For any* semaphore (binary or counting) created with valid parameters, take and give operations SHALL succeed when the semaphore state permits, and deletion SHALL succeed with OSAL_OK.

**Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

### Property 8: 计数信号量计数正确性

*For any* counting semaphore with max_count N and initial count I, after K give operations (where I+K ≤ N) and M take operations (where M ≤ I+K), the effective count SHALL be I+K-M.

**Validates: Requirements 6.2, 6.4, 6.5**

### Property 9: 消息队列往返一致性

*For any* queue with item_size S and item_count N, sending an item then receiving SHALL return an item with identical content to what was sent.

**Validates: Requirements 7.1, 7.3, 7.5**

### Property 10: 消息队列计数准确性

*For any* queue, after sending K items (K ≤ capacity) and receiving M items (M ≤ K), osal_queue_get_count() SHALL return K-M.

**Validates: Requirements 7.7**

### Property 11: 队列 Peek 不移除

*For any* non-empty queue, calling osal_queue_peek() SHALL return the front item without changing the queue count.

**Validates: Requirements 7.6**

### Property 12: 超时转换正确性

*For any* timeout value, OSAL_WAIT_FOREVER SHALL convert to portMAX_DELAY, OSAL_NO_WAIT SHALL convert to 0, and positive millisecond values SHALL convert to the equivalent tick count using pdMS_TO_TICKS().

**Validates: Requirements 9.1, 9.2, 9.3**

### Property 13: 空指针错误处理

*For any* OSAL API that accepts pointer parameters, passing NULL for required pointers SHALL return OSAL_ERROR_NULL_POINTER.

**Validates: Requirements 10.1**

### Property 14: 无效参数错误处理

*For any* OSAL API with parameter constraints (e.g., priority > 31, item_size = 0), passing invalid values SHALL return OSAL_ERROR_INVALID_PARAM.

**Validates: Requirements 10.2**

### Property 15: 临界区嵌套支持

*For any* sequence of N nested osal_enter_critical() calls followed by N osal_exit_critical() calls, the system SHALL correctly track nesting depth and only restore interrupts after the final exit.

**Validates: Requirements 8.3**

## Error Handling

### 错误码映射

| OSAL Error | FreeRTOS Condition | Description |
|------------|-------------------|-------------|
| OSAL_OK | pdPASS / pdTRUE | 操作成功 |
| OSAL_ERROR_NULL_POINTER | N/A (pre-check) | 空指针参数 |
| OSAL_ERROR_INVALID_PARAM | N/A (pre-check) | 无效参数 |
| OSAL_ERROR_NO_MEMORY | NULL handle returned | 内存分配失败 |
| OSAL_ERROR_TIMEOUT | pdFALSE from blocking API | 操作超时 |
| OSAL_ERROR_FULL | errQUEUE_FULL | 队列已满 |
| OSAL_ERROR_EMPTY | Queue empty | 队列为空 |
| OSAL_ERROR_ISR | ISR context detected | 从 ISR 调用非 ISR 安全 API |

### 错误处理策略

1. **参数验证优先** - 在调用 FreeRTOS API 前验证所有参数
2. **ISR 上下文检测** - 对不支持 ISR 调用的 API 检测并返回错误
3. **资源清理** - 创建失败时确保不泄漏资源
4. **断言用于内部错误** - 使用 configASSERT 捕获不应发生的内部错误

## Testing Strategy

### 单元测试

单元测试使用 Google Test 框架，通过 Mock FreeRTOS API 进行隔离测试。

**测试文件**: `tests/unit/osal/test_osal_freertos.cpp`

**测试覆盖**:
- 所有 OSAL API 的正常路径
- 空指针和无效参数错误处理
- 边界条件（优先级 0/31，超时 0/MAX）

### 属性测试

属性测试使用 RapidCheck 或类似的 C++ 属性测试库，验证正确性属性。

**配置**:
- 每个属性测试最少运行 100 次迭代
- 使用随机生成的有效输入

**测试标注格式**:
```cpp
// Feature: freertos-adapter, Property 9: 消息队列往返一致性
// Validates: Requirements 7.1, 7.3, 7.5
RC_GTEST_PROP(QueueRoundTrip, ...) { ... }
```

### 集成测试

在真实 FreeRTOS 环境中运行的集成测试，验证多任务场景。

**测试场景**:
- 多任务并发访问共享资源
- 优先级反转和优先级继承
- ISR 与任务间通信
- 长时间运行稳定性

### 测试平台

| 平台 | 用途 |
|------|------|
| Native (Mock) | 单元测试，CI 快速验证 |
| QEMU ARM | 集成测试，无需硬件 |
| STM32F4 Discovery | 硬件验证，性能测试 |
