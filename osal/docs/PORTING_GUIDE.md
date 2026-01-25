# OSAL 移植指南

本文档指导如何为新的 RTOS 创建 OSAL 适配器。

## 目录

1. [移植概述](#1-移植概述)
2. [准备工作](#2-准备工作)
3. [创建适配器目录](#3-创建适配器目录)
4. [实现核心接口](#4-实现核心接口)
5. [配置构建系统](#5-配置构建系统)
6. [测试验证](#6-测试验证)
7. [优化调优](#7-优化调优)
8. [故障排查](#8-故障排查)

## 1. 移植概述

### 1.1 依赖项

OSAL 适配器需要以下依赖：

- **目标 RTOS**: 需要移植的 RTOS（如 ThreadX、embOS 等）
- **编译器**: 支持 C99 或更高标准
- **RTOS 文档**: 目标 RTOS 的 API 参考手册

### 1.2 工作量评估

| 任务 | 工作量 | 说明 |
|------|--------|------|
| 任务管理适配 | 2-3 天 | 创建、删除、优先级等 |
| 互斥锁适配 | 1-2 天 | 锁定、解锁、超时 |
| 信号量适配 | 1-2 天 | 等待、释放 |
| 事件适配 | 1-2 天 | 设置、等待事件 |
| 队列适配 | 2-3 天 | 发送、接收消息 |
| 定时器适配 | 2-3 天 | 软件定时器 |
| 内存管理适配 | 1-2 天 | 分配、释放 |
| 测试验证 | 3-5 天 | 单元测试和集成测试 |
| **总计** | **13-22 天** | 取决于 RTOS 复杂度 |

### 1.3 移植流程

```
1. 创建适配器目录结构
   ↓
2. 实现任务管理接口
   ↓
3. 实现互斥锁接口
   ↓
4. 实现信号量接口
   ↓
5. 实现事件接口
   ↓
6. 实现队列接口
   ↓
7. 实现定时器接口
   ↓
8. 实现内存管理接口
   ↓
9. 配置构建系统
   ↓
10. 编写测试用例
   ↓
11. 测试验证
   ↓
12. 性能优化
   ↓
13. 文档编写
```

## 2. 准备工作

### 2.1 收集 RTOS 信息

- RTOS 名称和版本
- API 参考文档
- 任务/线程模型
- 同步原语类型
- 内存管理方式
- 定时器实现

### 2.2 准备开发环境

```bash
# 克隆项目
git clone https://github.com/nexus-embedded/nexus.git
cd nexus

# 查看现有适配器
ls osal/adapters/
# baremetal/  freertos/  native/  rtthread/  zephyr/
```

### 2.3 阅读参考实现

```bash
# 参考 FreeRTOS 适配器实现
cd osal/adapters/freertos
ls
# osal_freertos_task.c
# osal_freertos_mutex.c
# osal_freertos_sem.c
# osal_freertos_queue.c
# osal_freertos_timer.c
# osal_freertos_mem.c
```

## 3. 创建适配器目录

### 3.1 目录结构

```
osal/adapters/
└── <rtos_name>/           # RTOS 名称（如 threadx、embos）
    ├── osal_<rtos>_task.c
    ├── osal_<rtos>_mutex.c
    ├── osal_<rtos>_sem.c
    ├── osal_<rtos>_event.c
    ├── osal_<rtos>_queue.c
    ├── osal_<rtos>_timer.c
    ├── osal_<rtos>_mem.c
    ├── osal_<rtos>_diag.c
    ├── osal_<rtos>_internal.h
    └── CMakeLists.txt
```

### 3.2 创建基础文件

```bash
# 创建适配器目录
mkdir -p osal/adapters/myrtos

# 创建源文件
cd osal/adapters/myrtos
touch osal_myrtos_task.c
touch osal_myrtos_mutex.c
touch osal_myrtos_sem.c
touch osal_myrtos_event.c
touch osal_myrtos_queue.c
touch osal_myrtos_timer.c
touch osal_myrtos_mem.c
touch osal_myrtos_diag.c
touch osal_myrtos_internal.h
touch CMakeLists.txt
```

## 4. 实现核心接口

### 4.1 任务管理接口

```c
/**
 * \file            osal_myrtos_task.c
 * \brief           MyRTOS task management adapter
 * \author          Nexus Team
 */

#include "osal/osal_task.h"
#include "osal_myrtos_internal.h"
#include <myrtos.h>  /* MyRTOS 头文件 */

/* 任务句柄内部结构 */
typedef struct {
    uint32_t magic;              /* 魔数验证 */
    myrtos_thread_t* thread;     /* MyRTOS 线程句柄 */
    const char* name;            /* 任务名称 */
    uint8_t priority;            /* OSAL 优先级 */
} osal_myrtos_task_t;

#define TASK_MAGIC 0x5441534B  /* "TASK" */

/* 优先级转换 */
static inline uint8_t osal_to_myrtos_priority(uint8_t osal_priority) {
    /* OSAL: 0 = 最低, 31 = 最高 */
    /* MyRTOS: 假设 0 = 最高, 255 = 最低 */
    return 255 - (osal_priority * 8);
}

/* 创建任务 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    /* 参数验证 */
    if (config == NULL || handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    if (config->func == NULL) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 分配任务结构 */
    osal_myrtos_task_t* task = malloc(sizeof(osal_myrtos_task_t));
    if (task == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }
    
    /* 转换优先级 */
    uint8_t myrtos_priority = osal_to_myrtos_priority(config->priority);
    
    /* 创建 MyRTOS 线程 */
    task->thread = myrtos_thread_create(
        config->name,
        config->func,
        config->arg,
        config->stack_size,
        myrtos_priority
    );
    
    if (task->thread == NULL) {
        free(task);
        return OSAL_ERROR_NO_MEMORY;
    }
    
    /* 初始化任务结构 */
    task->magic = TASK_MAGIC;
    task->name = config->name;
    task->priority = config->priority;
    
    *handle = (osal_task_handle_t)task;
    return OSAL_OK;
}

/* 删除任务 */
osal_status_t osal_task_delete(osal_task_handle_t handle) {
    if (handle == NULL) {
        /* 删除当前任务 */
        myrtos_thread_delete(NULL);
        return OSAL_OK;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    /* 验证魔数 */
    if (task->magic != TASK_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 删除 MyRTOS 线程 */
    myrtos_thread_delete(task->thread);
    
    /* 清除魔数 */
    task->magic = 0;
    
    /* 释放内存 */
    free(task);
    
    return OSAL_OK;
}

/* 挂起任务 */
osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 调用 MyRTOS API */
    int ret = myrtos_thread_suspend(task->thread);
    return (ret == 0) ? OSAL_OK : OSAL_ERROR;
}

/* 恢复任务 */
osal_status_t osal_task_resume(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 调用 MyRTOS API */
    int ret = myrtos_thread_resume(task->thread);
    return (ret == 0) ? OSAL_OK : OSAL_ERROR;
}

/* 任务延时 */
osal_status_t osal_task_delay(uint32_t ms) {
    /* 检查是否在 ISR 中 */
    if (osal_is_isr()) {
        return OSAL_ERROR_ISR;
    }
    
    /* 调用 MyRTOS 延时 */
    myrtos_thread_delay_ms(ms);
    
    return OSAL_OK;
}

/* 任务让步 */
osal_status_t osal_task_yield(void) {
    if (osal_is_isr()) {
        return OSAL_ERROR_ISR;
    }
    
    myrtos_thread_yield();
    return OSAL_OK;
}

/* 获取当前任务 */
osal_task_handle_t osal_task_get_current(void) {
    /* 注意：这需要维护一个任务列表来反向查找 */
    /* 简化实现：返回 MyRTOS 句柄 */
    return (osal_task_handle_t)myrtos_thread_self();
}

/* 获取任务名称 */
const char* osal_task_get_name(osal_task_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return NULL;
    }
    
    return task->name;
}

/* 获取任务优先级 */
uint8_t osal_task_get_priority(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return 0;
    }
    
    return task->priority;
}

/* 设置任务优先级 */
osal_status_t osal_task_set_priority(osal_task_handle_t handle,
                                     uint8_t priority) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    if (priority > 31) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 转换优先级 */
    uint8_t myrtos_priority = osal_to_myrtos_priority(priority);
    
    /* 设置 MyRTOS 优先级 */
    int ret = myrtos_thread_set_priority(task->thread, myrtos_priority);
    if (ret != 0) {
        return OSAL_ERROR;
    }
    
    /* 更新缓存的优先级 */
    task->priority = priority;
    
    return OSAL_OK;
}

/* 获取堆栈水位 */
size_t osal_task_get_stack_watermark(osal_task_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return 0;
    }
    
    /* 调用 MyRTOS API 获取堆栈使用情况 */
    return myrtos_thread_get_stack_free(task->thread);
}

/* 获取任务状态 */
osal_task_state_t osal_task_get_state(osal_task_handle_t handle) {
    if (handle == NULL) {
        return OSAL_TASK_STATE_DELETED;
    }
    
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    
    if (task->magic != TASK_MAGIC) {
        return OSAL_TASK_STATE_DELETED;
    }
    
    /* 获取 MyRTOS 状态并转换 */
    int state = myrtos_thread_get_state(task->thread);
    
    switch (state) {
        case MYRTOS_STATE_READY:
            return OSAL_TASK_STATE_READY;
        case MYRTOS_STATE_RUNNING:
            return OSAL_TASK_STATE_RUNNING;
        case MYRTOS_STATE_BLOCKED:
            return OSAL_TASK_STATE_BLOCKED;
        case MYRTOS_STATE_SUSPENDED:
            return OSAL_TASK_STATE_SUSPENDED;
        default:
            return OSAL_TASK_STATE_DELETED;
    }
}
```

### 4.2 互斥锁接口

```c
/**
 * \file            osal_myrtos_mutex.c
 * \brief           MyRTOS mutex adapter
 * \author          Nexus Team
 */

#include "osal/osal_mutex.h"
#include "osal_myrtos_internal.h"
#include <myrtos.h>

/* 互斥锁句柄内部结构 */
typedef struct {
    uint32_t magic;
    myrtos_mutex_t* mutex;
} osal_myrtos_mutex_t;

#define MUTEX_MAGIC 0x4D555458  /* "MUTX" */

/* 创建互斥锁 */
osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    /* 分配互斥锁结构 */
    osal_myrtos_mutex_t* mutex = malloc(sizeof(osal_myrtos_mutex_t));
    if (mutex == NULL) {
        return OSAL_ERROR_NO_MEMORY;
    }
    
    /* 创建 MyRTOS 互斥锁 */
    mutex->mutex = myrtos_mutex_create();
    if (mutex->mutex == NULL) {
        free(mutex);
        return OSAL_ERROR_NO_MEMORY;
    }
    
    mutex->magic = MUTEX_MAGIC;
    *handle = (osal_mutex_handle_t)mutex;
    
    return OSAL_OK;
}

/* 删除互斥锁 */
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    osal_myrtos_mutex_t* mutex = (osal_myrtos_mutex_t*)handle;
    
    if (mutex->magic != MUTEX_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 删除 MyRTOS 互斥锁 */
    int ret = myrtos_mutex_delete(mutex->mutex);
    if (ret != 0) {
        return OSAL_ERROR_BUSY;
    }
    
    mutex->magic = 0;
    free(mutex);
    
    return OSAL_OK;
}

/* 锁定互斥锁 */
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    if (osal_is_isr()) {
        return OSAL_ERROR_ISR;
    }
    
    osal_myrtos_mutex_t* mutex = (osal_myrtos_mutex_t*)handle;
    
    if (mutex->magic != MUTEX_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 转换超时值 */
    uint32_t myrtos_timeout;
    if (timeout_ms == OSAL_WAIT_FOREVER) {
        myrtos_timeout = MYRTOS_WAIT_FOREVER;
    } else if (timeout_ms == OSAL_NO_WAIT) {
        myrtos_timeout = 0;
    } else {
        myrtos_timeout = timeout_ms;
    }
    
    /* 锁定互斥锁 */
    int ret = myrtos_mutex_lock(mutex->mutex, myrtos_timeout);
    
    if (ret == 0) {
        return OSAL_OK;
    } else if (ret == MYRTOS_TIMEOUT) {
        return OSAL_ERROR_TIMEOUT;
    } else {
        return OSAL_ERROR;
    }
}

/* 解锁互斥锁 */
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }
    
    osal_myrtos_mutex_t* mutex = (osal_myrtos_mutex_t*)handle;
    
    if (mutex->magic != MUTEX_MAGIC) {
        return OSAL_ERROR_INVALID_PARAM;
    }
    
    /* 解锁互斥锁 */
    int ret = myrtos_mutex_unlock(mutex->mutex);
    return (ret == 0) ? OSAL_OK : OSAL_ERROR;
}

/* 获取互斥锁所有者 */
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return NULL;
    }
    
    osal_myrtos_mutex_t* mutex = (osal_myrtos_mutex_t*)handle;
    
    if (mutex->magic != MUTEX_MAGIC) {
        return NULL;
    }
    
    /* 获取所有者 */
    return (osal_task_handle_t)myrtos_mutex_get_owner(mutex->mutex);
}

/* 检查互斥锁是否被锁定 */
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    if (handle == NULL) {
        return false;
    }
    
    osal_myrtos_mutex_t* mutex = (osal_myrtos_mutex_t*)handle;
    
    if (mutex->magic != MUTEX_MAGIC) {
        return false;
    }
    
    return myrtos_mutex_is_locked(mutex->mutex);
}
```


## 5. 配置构建系统

### 5.1 CMakeLists.txt

```cmake
# osal/adapters/myrtos/CMakeLists.txt

# MyRTOS 适配器库
add_library(osal_myrtos STATIC)

target_sources(osal_myrtos
    PRIVATE
        osal_myrtos_task.c
        osal_myrtos_mutex.c
        osal_myrtos_sem.c
        osal_myrtos_event.c
        osal_myrtos_queue.c
        osal_myrtos_timer.c
        osal_myrtos_mem.c
        osal_myrtos_diag.c
)

target_include_directories(osal_myrtos
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${MYRTOS_INCLUDE_DIR}
)

target_link_libraries(osal_myrtos
    PUBLIC
        osal_interface
    PRIVATE
        myrtos
)

# 定义适配器宏
target_compile_definitions(osal_myrtos
    PUBLIC
        OSAL_MYRTOS=1
)
```

### 5.2 Kconfig 配置

```kconfig
# osal/Kconfig

choice OSAL_BACKEND
    prompt "OSAL Backend"
    default OSAL_FREERTOS

config OSAL_FREERTOS
    bool "FreeRTOS"
    help
      Use FreeRTOS as OSAL backend

config OSAL_RTTHREAD
    bool "RT-Thread"
    help
      Use RT-Thread as OSAL backend

config OSAL_MYRTOS
    bool "MyRTOS"
    help
      Use MyRTOS as OSAL backend

config OSAL_BAREMETAL
    bool "Baremetal"
    help
      Use baremetal (no RTOS) as OSAL backend

endchoice
```

### 5.3 主 CMakeLists.txt 集成

```cmake
# osal/CMakeLists.txt

# 根据配置选择适配器
if(CONFIG_OSAL_FREERTOS)
    add_subdirectory(adapters/freertos)
    target_link_libraries(osal PUBLIC osal_freertos)
elseif(CONFIG_OSAL_RTTHREAD)
    add_subdirectory(adapters/rtthread)
    target_link_libraries(osal PUBLIC osal_rtthread)
elseif(CONFIG_OSAL_MYRTOS)
    add_subdirectory(adapters/myrtos)
    target_link_libraries(osal PUBLIC osal_myrtos)
elseif(CONFIG_OSAL_BAREMETAL)
    add_subdirectory(adapters/baremetal)
    target_link_libraries(osal PUBLIC osal_baremetal)
endif()
```

## 6. 测试验证

### 6.1 单元测试

```c
/* tests/osal/test_myrtos_task.c */

#include "osal/osal.h"
#include <assert.h>

void test_task_func(void* arg) {
    int* count = (int*)arg;
    (*count)++;
}

void test_myrtos_task_create(void) {
    osal_init();
    
    int count = 0;
    osal_task_config_t config = {
        .name = "test_task",
        .func = test_task_func,
        .arg = &count,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    
    osal_task_handle_t task;
    osal_status_t status = osal_task_create(&config, &task);
    
    assert(status == OSAL_OK);
    assert(task != NULL);
    
    osal_task_delay(100);
    assert(count > 0);
    
    osal_task_delete(task);
}

int main(void) {
    test_myrtos_task_create();
    printf("All tests passed!\n");
    return 0;
}
```

### 6.2 集成测试

```c
/* 生产者-消费者测试 */
void test_producer_consumer(void) {
    osal_init();
    
    osal_queue_handle_t queue;
    osal_queue_create(&queue, 10, sizeof(uint32_t));
    
    /* 创建生产者任务 */
    osal_task_config_t producer_config = {
        .name = "producer",
        .func = producer_task,
        .arg = queue,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    osal_task_handle_t producer;
    osal_task_create(&producer_config, &producer);
    
    /* 创建消费者任务 */
    osal_task_config_t consumer_config = {
        .name = "consumer",
        .func = consumer_task,
        .arg = queue,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    osal_task_handle_t consumer;
    osal_task_create(&consumer_config, &consumer);
    
    /* 运行测试 */
    osal_start();
}
```

### 6.3 验证清单

- [ ] 任务创建和删除正常
- [ ] 任务优先级正确映射
- [ ] 任务挂起和恢复工作
- [ ] 互斥锁正确保护共享资源
- [ ] 信号量同步正常
- [ ] 事件标志工作正常
- [ ] 队列收发消息正确
- [ ] 定时器按时触发
- [ ] 内存分配和释放正常
- [ ] 无内存泄漏
- [ ] 性能满足要求

## 7. 优化调优

### 7.1 性能优化

#### 7.1.1 减少内存分配

```c
/* 避免：每次都分配内存 */
osal_status_t bad_mutex_create(osal_mutex_handle_t* handle) {
    osal_myrtos_mutex_t* mutex = malloc(sizeof(osal_myrtos_mutex_t));
    /* ... */
}

/* 推荐：直接返回 RTOS 句柄 */
osal_status_t good_mutex_create(osal_mutex_handle_t* handle) {
    myrtos_mutex_t* mutex = myrtos_mutex_create();
    *handle = (osal_mutex_handle_t)mutex;
    return OSAL_OK;
}
```

#### 7.1.2 优化优先级转换

```c
/* 使用查找表加速转换 */
static const uint8_t priority_map[32] = {
    255, 247, 239, 231, 223, 215, 207, 199,
    191, 183, 175, 167, 159, 151, 143, 135,
    127, 119, 111, 103, 95, 87, 79, 71,
    63, 55, 47, 39, 31, 23, 15, 7
};

static inline uint8_t osal_to_myrtos_priority(uint8_t osal_priority) {
    return priority_map[osal_priority];
}
```

### 7.2 内存优化

```c
/* 使用内存池减少碎片 */
#define TASK_POOL_SIZE 16
static osal_myrtos_task_t task_pool[TASK_POOL_SIZE];
static bool task_pool_used[TASK_POOL_SIZE];

static osal_myrtos_task_t* alloc_task(void) {
    for (int i = 0; i < TASK_POOL_SIZE; i++) {
        if (!task_pool_used[i]) {
            task_pool_used[i] = true;
            return &task_pool[i];
        }
    }
    return NULL;
}

static void free_task(osal_myrtos_task_t* task) {
    int index = task - task_pool;
    if (index >= 0 && index < TASK_POOL_SIZE) {
        task_pool_used[index] = false;
    }
}
```

## 8. 故障排查

### 8.1 常见问题

#### 问题 1: 任务创建失败

**症状**: `osal_task_create()` 返回错误

**可能原因**:
- 堆栈大小不足
- 内存不足
- 优先级转换错误

**解决方案**:
```c
/* 添加调试日志 */
osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    printf("Creating task: %s, priority: %d, stack: %zu\n",
           config->name, config->priority, config->stack_size);
    
    /* ... */
    
    if (task->thread == NULL) {
        printf("ERROR: myrtos_thread_create failed\n");
        free(task);
        return OSAL_ERROR_NO_MEMORY;
    }
    
    printf("Task created successfully\n");
    return OSAL_OK;
}
```

#### 问题 2: 优先级不正确

**症状**: 任务调度顺序不符合预期

**解决方案**:
```c
/* 验证优先级转换 */
void test_priority_mapping(void) {
    for (uint8_t i = 0; i <= 31; i++) {
        uint8_t myrtos_pri = osal_to_myrtos_priority(i);
        printf("OSAL %d -> MyRTOS %d\n", i, myrtos_pri);
    }
}
```

#### 问题 3: 互斥锁死锁

**症状**: 任务永久阻塞

**解决方案**:
```c
/* 添加超时和调试信息 */
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    printf("Task %s trying to lock mutex\n", osal_task_get_name(osal_task_get_current()));
    
    int ret = myrtos_mutex_lock(mutex->mutex, timeout_ms);
    
    if (ret == MYRTOS_TIMEOUT) {
        printf("WARNING: Mutex lock timeout!\n");
        printf("Owner: %s\n", osal_task_get_name(osal_mutex_get_owner(handle)));
        return OSAL_ERROR_TIMEOUT;
    }
    
    printf("Mutex locked successfully\n");
    return OSAL_OK;
}
```

### 8.2 调试技巧

#### 8.2.1 使用断言

```c
#define OSAL_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            printf("ASSERTION FAILED: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            while(1); \
        } \
    } while(0)

/* 在关键位置添加断言 */
osal_status_t osal_task_delete(osal_task_handle_t handle) {
    osal_myrtos_task_t* task = (osal_myrtos_task_t*)handle;
    OSAL_ASSERT(task->magic == TASK_MAGIC);
    /* ... */
}
```

#### 8.2.2 内存泄漏检测

```c
static int alloc_count = 0;
static int free_count = 0;

void* debug_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        alloc_count++;
        printf("malloc: %p, size: %zu, count: %d\n", ptr, size, alloc_count);
    }
    return ptr;
}

void debug_free(void* ptr) {
    if (ptr) {
        free_count++;
        printf("free: %p, count: %d\n", ptr, free_count);
        free(ptr);
    }
}

/* 检查泄漏 */
void check_memory_leak(void) {
    printf("Allocations: %d, Frees: %d\n", alloc_count, free_count);
    if (alloc_count != free_count) {
        printf("WARNING: Memory leak detected!\n");
    }
}
```

## 9. 文档编写

### 9.1 适配器文档

创建 `osal/adapters/myrtos/README.md`:

```markdown
# MyRTOS OSAL 适配器

## 概述

本适配器为 MyRTOS 提供 OSAL 接口支持。

## 支持的功能

- ✅ 任务管理
- ✅ 互斥锁
- ✅ 信号量
- ✅ 事件标志
- ✅ 消息队列
- ✅ 软件定时器
- ✅ 内存管理

## 限制

- 优先级映射可能存在精度损失
- 不支持静态内存分配

## 性能

- 任务创建: ~60 µs
- 上下文切换: ~2.5 µs
- 互斥锁获取: ~1.2 µs

## 测试状态

- 单元测试: ✅ 通过
- 集成测试: ✅ 通过
- 性能测试: ✅ 通过
```

### 9.2 更新主文档

在 `osal/docs/DESIGN.md` 中添加 MyRTOS 适配器说明。

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**作者**: Nexus Team
