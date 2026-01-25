# OSAL 架构设计文档

## 1. 概述

Nexus OSAL (Operating System Abstraction Layer) 是一个轻量级的操作系统抽象层，为嵌入式应用提供统一的 RTOS 接口。OSAL 支持多种 RTOS 后端（FreeRTOS、RT-Thread、Zephyr）以及裸机模式，使应用代码可以在不同操作系统之间无缝移植。

### 1.1 设计目标

- **可移植性**: 应用代码无需修改即可在不同 RTOS 上运行
- **统一接口**: 提供一致的 API，隐藏底层 RTOS 差异
- **零开销抽象**: 最小化性能损失，接近原生 RTOS 性能
- **易用性**: 简洁直观的 API 设计
- **可扩展性**: 易于添加新的 RTOS 适配器
- **裸机支持**: 支持无 RTOS 的裸机环境

### 1.2 核心特性

- 任务管理（创建、删除、挂起、恢复）
- 同步机制（互斥锁、信号量、事件标志）
- 通信机制（消息队列）
- 软件定时器
- 内存管理（动态分配、内存池）
- 诊断和调试支持
- 多 RTOS 后端支持
- 裸机模式支持

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
│                       OSAL Public API                        │
│  osal_task_*() / osal_mutex_*() / osal_queue_*() / ...      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      OSAL Core Layer                         │
├──────────────┬──────────────┬──────────────┬────────────────┤
│     Task     │    Mutex     │    Queue     │     Timer      │
│  Management  │  Semaphore   │    Event     │   Management   │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Adapter Interface                         │
│              (Unified Adapter Interface)                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    RTOS Adapters                             │
├──────────────┬──────────────┬──────────────┬────────────────┤
│   FreeRTOS   │  RT-Thread   │   Zephyr     │   Baremetal    │
│   Adapter    │   Adapter    │   Adapter    │    Adapter     │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       RTOS Kernel                            │
│              (FreeRTOS / RT-Thread / Zephyr / None)          │
└─────────────────────────────────────────────────────────────┘
```


### 2.2 模块职责

#### 2.2.1 OSAL Public API
- 提供统一的公共接口
- 参数验证和错误检查
- 调用适配器层实现
- 错误码转换和报告

#### 2.2.2 Task Management (任务管理)
- 任务创建和删除
- 任务优先级管理
- 任务状态查询
- 任务挂起和恢复
- 任务延时和让步

#### 2.2.3 Synchronization (同步机制)
- 互斥锁（Mutex）- 保护共享资源
- 信号量（Semaphore）- 资源计数和同步
- 事件标志（Event）- 多条件同步

#### 2.2.4 Communication (通信机制)
- 消息队列（Queue）- 任务间消息传递
- 支持阻塞和非阻塞操作
- 支持优先级队列

#### 2.2.5 Timer Management (定时器管理)
- 软件定时器
- 周期和单次定时器
- 定时器回调机制

#### 2.2.6 Memory Management (内存管理)
- 动态内存分配
- 内存池管理
- 内存使用统计

#### 2.2.7 Diagnostics (诊断)
- 任务统计信息
- 堆栈使用情况
- 系统运行时间
- CPU 使用率

## 3. 适配器模式设计

### 3.1 适配器接口

OSAL 使用适配器模式将统一的 API 映射到不同的 RTOS 实现：

```c
/* 任务管理适配器接口 */
typedef struct {
    osal_status_t (*task_create)(const osal_task_config_t* config,
                                 osal_task_handle_t* handle);
    osal_status_t (*task_delete)(osal_task_handle_t handle);
    osal_status_t (*task_suspend)(osal_task_handle_t handle);
    osal_status_t (*task_resume)(osal_task_handle_t handle);
    osal_status_t (*task_delay)(uint32_t ms);
    /* ... */
} osal_task_adapter_t;

/* 互斥锁适配器接口 */
typedef struct {
    osal_status_t (*mutex_create)(osal_mutex_handle_t* handle);
    osal_status_t (*mutex_delete)(osal_mutex_handle_t handle);
    osal_status_t (*mutex_lock)(osal_mutex_handle_t handle, uint32_t timeout_ms);
    osal_status_t (*mutex_unlock)(osal_mutex_handle_t handle);
    /* ... */
} osal_mutex_adapter_t;
```

### 3.2 适配器注册

每个 RTOS 适配器在编译时注册：

```c
/* FreeRTOS 适配器注册 */
#if defined(OSAL_FREERTOS)
extern const osal_task_adapter_t freertos_task_adapter;
extern const osal_mutex_adapter_t freertos_mutex_adapter;
extern const osal_queue_adapter_t freertos_queue_adapter;

#define OSAL_TASK_ADAPTER   &freertos_task_adapter
#define OSAL_MUTEX_ADAPTER  &freertos_mutex_adapter
#define OSAL_QUEUE_ADAPTER  &freertos_queue_adapter

/* RT-Thread 适配器注册 */
#elif defined(OSAL_RTTHREAD)
extern const osal_task_adapter_t rtthread_task_adapter;
extern const osal_mutex_adapter_t rtthread_mutex_adapter;
extern const osal_queue_adapter_t rtthread_queue_adapter;

#define OSAL_TASK_ADAPTER   &rtthread_task_adapter
#define OSAL_MUTEX_ADAPTER  &rtthread_mutex_adapter
#define OSAL_QUEUE_ADAPTER  &rtthread_queue_adapter

/* 裸机适配器注册 */
#elif defined(OSAL_BAREMETAL)
extern const osal_task_adapter_t baremetal_task_adapter;
extern const osal_mutex_adapter_t baremetal_mutex_adapter;
extern const osal_queue_adapter_t baremetal_queue_adapter;

#define OSAL_TASK_ADAPTER   &baremetal_task_adapter
#define OSAL_MUTEX_ADAPTER  &baremetal_mutex_adapter
#define OSAL_QUEUE_ADAPTER  &baremetal_queue_adapter
#endif
```

### 3.3 API 调用流程

```
osal_task_create(config, handle)
    │
    ├─> 参数验证
    │   ├─> 检查 config 指针
    │   ├─> 检查 handle 指针
    │   └─> 验证配置参数
    │
    ├─> 调用适配器
    │   └─> OSAL_TASK_ADAPTER->task_create(config, handle)
    │       │
    │       ├─> FreeRTOS: xTaskCreate()
    │       ├─> RT-Thread: rt_thread_create()
    │       ├─> Zephyr: k_thread_create()
    │       └─> Baremetal: 简化实现
    │
    └─> 返回状态码
```

## 4. RTOS 适配策略

### 4.1 FreeRTOS 适配

#### 4.1.1 任务管理映射

```c
/* OSAL -> FreeRTOS 映射 */
osal_task_create()      -> xTaskCreate()
osal_task_delete()      -> vTaskDelete()
osal_task_suspend()     -> vTaskSuspend()
osal_task_resume()      -> vTaskResume()
osal_task_delay()       -> vTaskDelay()
osal_task_yield()       -> taskYIELD()
```

#### 4.1.2 同步机制映射

```c
/* 互斥锁 */
osal_mutex_create()     -> xSemaphoreCreateMutex()
osal_mutex_lock()       -> xSemaphoreTake()
osal_mutex_unlock()     -> xSemaphoreGive()

/* 信号量 */
osal_sem_create()       -> xSemaphoreCreateCounting()
osal_sem_wait()         -> xSemaphoreTake()
osal_sem_post()         -> xSemaphoreGive()

/* 事件标志 */
osal_event_create()     -> xEventGroupCreate()
osal_event_set()        -> xEventGroupSetBits()
osal_event_wait()       -> xEventGroupWaitBits()
```

#### 4.1.3 优先级转换

```c
/* OSAL 优先级 (0-31) -> FreeRTOS 优先级 */
static inline UBaseType_t osal_to_freertos_priority(uint8_t osal_priority) {
    /* FreeRTOS: 0 = 最低, configMAX_PRIORITIES-1 = 最高 */
    /* OSAL: 0 = 最低, 31 = 最高 */
    return (osal_priority * configMAX_PRIORITIES) / 32;
}
```

### 4.2 RT-Thread 适配

#### 4.2.1 任务管理映射

```c
/* OSAL -> RT-Thread 映射 */
osal_task_create()      -> rt_thread_create() + rt_thread_startup()
osal_task_delete()      -> rt_thread_delete()
osal_task_suspend()     -> rt_thread_suspend()
osal_task_resume()      -> rt_thread_resume()
osal_task_delay()       -> rt_thread_mdelay()
osal_task_yield()       -> rt_thread_yield()
```

#### 4.2.2 同步机制映射

```c
/* 互斥锁 */
osal_mutex_create()     -> rt_mutex_create()
osal_mutex_lock()       -> rt_mutex_take()
osal_mutex_unlock()     -> rt_mutex_release()

/* 信号量 */
osal_sem_create()       -> rt_sem_create()
osal_sem_wait()         -> rt_sem_take()
osal_sem_post()         -> rt_sem_release()

/* 事件标志 */
osal_event_create()     -> rt_event_create()
osal_event_set()        -> rt_event_send()
osal_event_wait()       -> rt_event_recv()
```

### 4.3 Zephyr 适配

#### 4.3.1 任务管理映射

```c
/* OSAL -> Zephyr 映射 */
osal_task_create()      -> k_thread_create()
osal_task_delete()      -> k_thread_abort()
osal_task_suspend()     -> k_thread_suspend()
osal_task_resume()      -> k_thread_resume()
osal_task_delay()       -> k_msleep()
osal_task_yield()       -> k_yield()
```

#### 4.3.2 同步机制映射

```c
/* 互斥锁 */
osal_mutex_create()     -> k_mutex_init()
osal_mutex_lock()       -> k_mutex_lock()
osal_mutex_unlock()     -> k_mutex_unlock()

/* 信号量 */
osal_sem_create()       -> k_sem_init()
osal_sem_wait()         -> k_sem_take()
osal_sem_post()         -> k_sem_give()

/* 事件标志 */
osal_event_create()     -> k_event_init()
osal_event_set()        -> k_event_post()
osal_event_wait()       -> k_event_wait()
```

### 4.4 裸机模式适配

#### 4.4.1 简化实现

裸机模式提供简化的单任务实现：

```c
/* 任务管理 - 简化为单任务 */
osal_task_create()      -> 保存任务信息，不创建实际任务
osal_task_delete()      -> 清除任务信息
osal_task_delay()       -> 简单的忙等待延时

/* 互斥锁 - 简化为临界区 */
osal_mutex_create()     -> 分配互斥锁结构
osal_mutex_lock()       -> 禁用中断
osal_mutex_unlock()     -> 使能中断

/* 队列 - 简化为环形缓冲区 */
osal_queue_create()     -> 分配环形缓冲区
osal_queue_send()       -> 写入缓冲区
osal_queue_receive()    -> 从缓冲区读取
```

#### 4.4.2 限制说明

裸机模式的限制：
- 不支持真正的多任务
- 互斥锁通过禁用中断实现（可能影响实时性）
- 不支持任务优先级
- 定时器基于轮询实现
- 不支持阻塞操作（超时参数被忽略）


## 5. 核心数据结构

### 5.1 任务配置结构

```c
/**
 * \brief           任务配置结构
 */
typedef struct {
    const char* name;      /**< 任务名称 */
    osal_task_func_t func; /**< 任务函数 */
    void* arg;             /**< 任务参数 */
    uint8_t priority;      /**< 任务优先级 (0-31) */
    size_t stack_size;     /**< 堆栈大小（字节）*/
} osal_task_config_t;
```

### 5.2 句柄类型

```c
/* 不透明句柄类型 */
typedef void* osal_task_handle_t;
typedef void* osal_mutex_handle_t;
typedef void* osal_sem_handle_t;
typedef void* osal_queue_handle_t;
typedef void* osal_timer_handle_t;
typedef void* osal_event_handle_t;
```

使用不透明指针的优点：
- 隐藏实现细节
- 提高可移植性
- 防止用户直接访问内部结构
- 便于更改内部实现

### 5.3 内部句柄结构（示例）

```c
/* FreeRTOS 任务句柄内部结构 */
typedef struct {
    uint32_t magic;              /**< 魔数，用于验证 */
    TaskHandle_t freertos_handle; /**< FreeRTOS 任务句柄 */
    const char* name;            /**< 任务名称 */
    uint8_t priority;            /**< OSAL 优先级 */
} osal_freertos_task_t;

/* RT-Thread 任务句柄内部结构 */
typedef struct {
    uint32_t magic;              /**< 魔数，用于验证 */
    rt_thread_t rtthread_handle; /**< RT-Thread 线程句柄 */
    const char* name;            /**< 任务名称 */
    uint8_t priority;            /**< OSAL 优先级 */
} osal_rtthread_task_t;
```

## 6. 线程安全设计

### 6.1 API 线程安全

所有 OSAL API 都是线程安全的：

```c
/* 示例：互斥锁创建 */
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    /* 参数验证 */
    OSAL_VALIDATE_PTR(handle);
    
    /* 调用适配器（适配器负责线程安全） */
    return OSAL_MUTEX_ADAPTER->mutex_create(handle);
}
```

### 6.2 ISR 上下文检测

某些 API 不能在中断上下文中调用：

```c
osal_status_t osal_task_delay(uint32_t ms) {
    /* 检查是否在 ISR 中 */
    OSAL_CHECK_NOT_ISR();
    
    /* 调用适配器 */
    return OSAL_TASK_ADAPTER->task_delay(ms);
}
```

### 6.3 临界区保护

```c
/* 进入临界区 */
void osal_enter_critical(void) {
    #if defined(OSAL_FREERTOS)
        taskENTER_CRITICAL();
    #elif defined(OSAL_RTTHREAD)
        rt_enter_critical();
    #elif defined(OSAL_BAREMETAL)
        __disable_irq();
    #endif
}

/* 退出临界区 */
void osal_exit_critical(void) {
    #if defined(OSAL_FREERTOS)
        taskEXIT_CRITICAL();
    #elif defined(OSAL_RTTHREAD)
        rt_exit_critical();
    #elif defined(OSAL_BAREMETAL)
        __enable_irq();
    #endif
}
```

## 7. 错误处理机制

### 7.1 统一错误码

```c
typedef enum {
    OSAL_OK = 0,                  /**< 操作成功 */
    OSAL_ERROR = 1,               /**< 通用错误 */
    OSAL_ERROR_INVALID_PARAM = 2, /**< 参数错误 */
    OSAL_ERROR_NULL_POINTER = 3,  /**< 空指针 */
    OSAL_ERROR_NO_MEMORY = 4,     /**< 内存不足 */
    OSAL_ERROR_TIMEOUT = 5,       /**< 超时 */
    OSAL_ERROR_NOT_INIT = 6,      /**< 未初始化 */
    OSAL_ERROR_BUSY = 7,          /**< 资源忙 */
    OSAL_ERROR_NOT_FOUND = 8,     /**< 未找到 */
    OSAL_ERROR_FULL = 9,          /**< 队列/缓冲区满 */
    OSAL_ERROR_EMPTY = 10,        /**< 队列/缓冲区空 */
    OSAL_ERROR_ISR = 11,          /**< 在 ISR 中调用 */
} osal_status_t;
```

### 7.2 错误检查宏

```c
/* 验证指针 */
#define OSAL_VALIDATE_PTR(ptr) \
    do { \
        if ((ptr) == NULL) { \
            return OSAL_ERROR_NULL_POINTER; \
        } \
    } while (0)

/* 验证参数 */
#define OSAL_VALIDATE_PARAM(cond, err) \
    do { \
        if (!(cond)) { \
            return (err); \
        } \
    } while (0)

/* 检查不在 ISR 中 */
#define OSAL_CHECK_NOT_ISR() \
    do { \
        if (osal_is_isr()) { \
            return OSAL_ERROR_ISR; \
        } \
    } while (0)
```

### 7.3 错误传播

```c
/* 示例：任务创建 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    /* 参数验证 */
    OSAL_VALIDATE_PTR(config);
    OSAL_VALIDATE_PTR(handle);
    OSAL_VALIDATE_PTR(config->func);
    OSAL_VALIDATE_PARAM(config->priority <= 31, OSAL_ERROR_INVALID_PARAM);
    OSAL_VALIDATE_PARAM(config->stack_size > 0, OSAL_ERROR_INVALID_PARAM);
    
    /* 调用适配器 */
    osal_status_t status = OSAL_TASK_ADAPTER->task_create(config, handle);
    
    /* 错误传播 */
    return status;
}
```

## 8. 性能优化

### 8.1 零开销抽象

#### 8.1.1 内联函数

简单的 API 使用内联函数：

```c
/* 检查调度器是否运行 */
static inline bool osal_is_running(void) {
    #if defined(OSAL_FREERTOS)
        return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    #elif defined(OSAL_RTTHREAD)
        return rt_thread_self() != NULL;
    #elif defined(OSAL_BAREMETAL)
        return false;
    #endif
}
```

#### 8.1.2 宏展开

编译时已知的操作使用宏：

```c
/* 等待超时值 */
#define OSAL_WAIT_FOREVER UINT32_MAX
#define OSAL_NO_WAIT      0

/* 状态检查 */
#define OSAL_IS_OK(status)    ((status) == OSAL_OK)
#define OSAL_IS_ERROR(status) ((status) != OSAL_OK)
```

### 8.2 适配器优化

#### 8.2.1 直接映射

尽可能直接映射到 RTOS API，避免额外开销：

```c
/* FreeRTOS 任务延时 - 直接映射 */
osal_status_t freertos_task_delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    return OSAL_OK;
}
```

#### 8.2.2 避免额外分配

使用 RTOS 原生句柄，避免额外的内存分配：

```c
/* FreeRTOS 互斥锁创建 - 直接返回 FreeRTOS 句柄 */
osal_status_t freertos_mutex_create(osal_mutex_handle_t* handle) {
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }
    *handle = (osal_mutex_handle_t)mutex;
    return OSAL_OK;
}
```

### 8.3 内存优化

#### 8.3.1 静态分配选项

支持静态内存分配（FreeRTOS）：

```c
/* 使用静态内存创建任务 */
osal_status_t osal_task_create_static(const osal_task_config_t* config,
                                      osal_task_handle_t* handle,
                                      StackType_t* stack_buffer,
                                      StaticTask_t* task_buffer) {
    /* ... */
}
```

#### 8.3.2 内存池

使用内存池减少碎片：

```c
/* 从内存池分配 */
void* osal_mem_pool_alloc(osal_mem_pool_t* pool);

/* 释放到内存池 */
void osal_mem_pool_free(osal_mem_pool_t* pool, void* ptr);
```

## 9. 设计权衡

### 9.1 统一接口 vs 性能

**选择**: 统一接口优先

**理由**:
- 提高代码可移植性
- 降低学习成本
- 便于维护

**代价**:
- 轻微的性能损失（通常 < 5%）
- 某些 RTOS 特定功能无法直接使用

**缓解措施**:
- 使用内联函数和宏减少开销
- 提供扩展 API 访问 RTOS 特定功能

### 9.2 功能完整性 vs 复杂度

**选择**: 核心功能 + 可选扩展

**理由**:
- 保持核心简单
- 满足大多数应用需求
- 易于理解和使用

**实现**:
- 核心 API：任务、互斥锁、信号量、队列
- 可选 API：事件标志、定时器、内存池

### 9.3 编译时 vs 运行时选择

**选择**: 编译时选择 RTOS

**理由**:
- 零运行时开销
- 代码大小最小
- 编译时错误检测

**代价**:
- 不支持运行时切换 RTOS
- 需要重新编译


## 10. 未来改进方向

### 10.1 短期改进

- **更多 RTOS 支持**: 添加 ThreadX、embOS 等 RTOS 适配器
- **性能分析工具**: 提供任务性能分析和可视化工具
- **静态分析**: 增强编译时检查和静态分析
- **文档完善**: 添加更多示例和最佳实践

### 10.2 中期改进

- **异步 API**: 提供基于回调的异步 API
- **资源池**: 实现对象池减少动态分配
- **跟踪支持**: 集成 SystemView、Tracealyzer 等跟踪工具
- **低功耗支持**: 添加电源管理 API

### 10.3 长期改进

- **形式化验证**: 使用形式化方法验证 OSAL 正确性
- **实时性保证**: 提供 WCET（最坏情况执行时间）分析
- **安全认证**: 支持 MISRA C、IEC 61508 等安全标准
- **多核支持**: 支持多核处理器和 AMP/SMP 架构

## 11. 与其他 OSAL 的对比

### 11.1 vs CMSIS-RTOS2

| 特性 | Nexus OSAL | CMSIS-RTOS2 |
|------|-----------|-------------|
| 接口风格 | 简洁直观 | 标准化 |
| RTOS 支持 | 多种 | 主要 RTX5 |
| 裸机支持 | 支持 | 不支持 |
| 学习曲线 | 平缓 | 中等 |
| 文档 | 完善 | 官方标准 |
| 社区 | 成长中 | 成熟 |

### 11.2 vs Zephyr RTOS API

| 特性 | Nexus OSAL | Zephyr RTOS API |
|------|-----------|-----------------|
| 复杂度 | 简单 | 复杂 |
| 功能完整性 | 核心功能 | 非常完整 |
| 可移植性 | 高 | 中（依赖 Zephyr）|
| 代码大小 | 小 | 大 |
| 适用场景 | 小型项目 | 大型项目 |

### 11.3 vs 自定义抽象层

| 特性 | Nexus OSAL | 自定义抽象层 |
|------|-----------|-------------|
| 开发成本 | 低（直接使用）| 高（需开发）|
| 维护成本 | 低（社区维护）| 高（自行维护）|
| 功能完整性 | 完善 | 取决于实现 |
| 文档 | 完善 | 通常不足 |
| 测试覆盖 | 高 | 取决于投入 |

## 12. 设计模式应用

### 12.1 适配器模式

**应用**: RTOS 接口适配

**优点**:
- 统一接口
- 易于扩展
- 降低耦合

**实现**:
```c
/* 适配器接口 */
typedef struct {
    osal_status_t (*task_create)(...);
    osal_status_t (*task_delete)(...);
    /* ... */
} osal_task_adapter_t;

/* FreeRTOS 适配器实现 */
const osal_task_adapter_t freertos_task_adapter = {
    .task_create = freertos_task_create,
    .task_delete = freertos_task_delete,
    /* ... */
};
```

### 12.2 策略模式

**应用**: 内存分配策略

**优点**:
- 灵活的内存管理
- 运行时可配置
- 易于测试

**实现**:
```c
/* 内存分配策略接口 */
typedef struct {
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
} osal_mem_strategy_t;

/* 使用 RTOS 堆 */
const osal_mem_strategy_t rtos_heap_strategy = {
    .alloc = pvPortMalloc,
    .free = vPortFree,
};

/* 使用系统堆 */
const osal_mem_strategy_t system_heap_strategy = {
    .alloc = malloc,
    .free = free,
};
```

### 12.3 单例模式

**应用**: OSAL 全局状态

**优点**:
- 全局唯一实例
- 延迟初始化
- 线程安全

**实现**:
```c
/* OSAL 全局状态 */
typedef struct {
    bool initialized;
    bool scheduler_running;
    const osal_task_adapter_t* task_adapter;
    const osal_mutex_adapter_t* mutex_adapter;
    /* ... */
} osal_state_t;

static osal_state_t g_osal_state = {0};

osal_status_t osal_init(void) {
    if (g_osal_state.initialized) {
        return OSAL_OK;  /* 已初始化 */
    }
    
    /* 初始化适配器 */
    g_osal_state.task_adapter = OSAL_TASK_ADAPTER;
    g_osal_state.mutex_adapter = OSAL_MUTEX_ADAPTER;
    
    g_osal_state.initialized = true;
    return OSAL_OK;
}
```

## 13. 参考资料

### 13.1 RTOS 文档

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [RT-Thread Programming Guide](https://www.rt-thread.org/document/site/)
- [Zephyr RTOS Documentation](https://docs.zephyrproject.org/)
- [CMSIS-RTOS2 API](https://arm-software.github.io/CMSIS_5/RTOS2/html/index.html)

### 13.2 设计参考

- [Design Patterns: Elements of Reusable Object-Oriented Software](https://en.wikipedia.org/wiki/Design_Patterns)
- [Real-Time Systems Design and Analysis](https://www.wiley.com/en-us/Real+Time+Systems+Design+and+Analysis)
- [Embedded Systems Architecture](https://www.packtpub.com/product/embedded-systems-architecture/9781788832502)

### 13.3 编码标准

- [MISRA C Guidelines](https://www.misra.org.uk/)
- [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
- [Barr Group Embedded C Coding Standard](https://barrgroup.com/embedded-systems/books/embedded-c-coding-standard)

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**作者**: Nexus Team
