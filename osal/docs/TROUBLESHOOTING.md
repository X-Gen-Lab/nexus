# OSAL 故障排查指南

本文档提供 Nexus OSAL 常见问题的诊断和解决方案。

## 目录

1. [初始化问题](#1-初始化问题)
2. [任务问题](#2-任务问题)
3. [同步问题](#3-同步问题)
4. [队列问题](#4-队列问题)
5. [定时器问题](#5-定时器问题)
6. [内存问题](#6-内存问题)
7. [性能问题](#7-性能问题)
8. [RTOS 适配问题](#8-rtos-适配问题)
9. [调试技巧](#9-调试技巧)
10. [常见错误码](#10-常见错误码)

## 1. 初始化问题

### 问题 1.1: OSAL 初始化失败

**症状**:
```c
osal_status_t status = osal_init();
/* status != OSAL_OK */
```

**可能原因**:
1. RTOS 未正确初始化
2. 内存不足
3. 配置错误

**诊断步骤**:

```c
/* 1. 检查返回的错误码 */
osal_status_t status = osal_init();
printf("OSAL init status: %d\n", status);

/* 2. 检查 RTOS 是否已初始化 */
#if defined(OSAL_FREERTOS)
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        printf("ERROR: FreeRTOS scheduler not started\n");
    }
#endif

/* 3. 检查内存 */
#if defined(OSAL_FREERTOS)
    size_t free_heap = xPortGetFreeHeapSize();
    printf("Free heap: %zu bytes\n", free_heap);
#endif
```

**解决方案**:

1. **确保正确的初始化顺序**:
```c
int main(void) {
    /* 1. 硬件初始化 */
    SystemInit();
    
    /* 2. OSAL 初始化 */
    osal_init();
    
    /* 3. 创建任务 */
    create_application_tasks();
    
    /* 4. 启动调度器 */
    osal_start();  /* 不返回 */
}
```

2. **增加堆大小**:
```c
/* FreeRTOS: 在 FreeRTOSConfig.h 中 */
#define configTOTAL_HEAP_SIZE ((size_t)(20 * 1024))  /* 20KB */
```

### 问题 1.2: 调度器启动失败

**症状**: `osal_start()` 返回或系统挂起

**解决方案**:

```c
/* 确保至少创建了一个任务 */
void idle_task(void* arg) {
    while (1) {
        osal_task_delay(1000);
    }
}

int main(void) {
    osal_init();
    
    /* 创建至少一个任务 */
    osal_task_config_t config = {
        .name = "idle",
        .func = idle_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_IDLE,
        .stack_size = 512,
    };
    osal_task_handle_t task;
    osal_task_create(&config, &task);
    
    osal_start();
}
```

## 2. 任务问题

### 问题 2.1: 任务创建失败

**症状**:
```c
osal_status_t status = osal_task_create(&config, &task);
/* status == OSAL_ERROR_NO_MEMORY */
```

**可能原因**:
1. 堆栈大小过大
2. 系统内存不足
3. 任务数量超过限制

**诊断步骤**:

```c
/* 检查可用内存 */
void check_memory(void) {
    #if defined(OSAL_FREERTOS)
        size_t free_heap = xPortGetFreeHeapSize();
        printf("Free heap: %zu bytes\n", free_heap);
        printf("Minimum free heap: %zu bytes\n", xPortGetMinimumEverFreeHeapSize());
    #endif
}

/* 尝试创建更小的任务 */
osal_task_config_t config = {
    .name = "test",
    .func = test_func,
    .arg = NULL,
    .priority = OSAL_TASK_PRIORITY_NORMAL,
    .stack_size = 256,  /* 减小堆栈 */
};
```

**解决方案**:

1. **减小堆栈大小**:
```c
/* 根据实际需求设置堆栈 */
config.stack_size = 512;   /* 简单任务 */
config.stack_size = 1024;  /* 中等任务 */
config.stack_size = 2048;  /* 复杂任务 */
```

2. **增加系统堆大小**:
```c
/* FreeRTOS */
#define configTOTAL_HEAP_SIZE ((size_t)(30 * 1024))
```

3. **使用静态分配**:
```c
static uint8_t task_stack[1024];
osal_task_create_static(&config, &task, task_stack);
```

### 问题 2.2: 任务堆栈溢出

**症状**: 系统崩溃、任务行为异常

**诊断步骤**:

```c
/* 检查堆栈使用情况 */
void check_stack_usage(void) {
    osal_task_handle_t task = osal_task_get_current();
    size_t watermark = osal_task_get_stack_watermark(task);
    
    osal_task_stats_t stats;
    osal_task_get_stats(task, &stats);
    
    printf("Task: %s\n", stats.name);
    printf("Stack size: %zu bytes\n", stats.stack_size);
    printf("Stack free: %zu bytes\n", watermark);
    printf("Stack usage: %.1f%%\n", 
           (float)(stats.stack_size - watermark) * 100 / stats.stack_size);
    
    if (watermark < 128) {
        printf("WARNING: Stack usage is very high!\n");
    }
}
```

**解决方案**:

1. **增加堆栈大小**:
```c
config.stack_size = 2048;  /* 增加到 2KB */
```

2. **减少局部变量**:
```c
/* 避免：大数组在栈上 */
void bad_function(void) {
    uint8_t buffer[2048];  /* 占用 2KB 堆栈 */
    /* ... */
}

/* 推荐：使用堆分配 */
void good_function(void) {
    uint8_t* buffer = osal_mem_alloc(2048);
    if (buffer) {
        /* ... */
        osal_mem_free(buffer);
    }
}
```

3. **启用堆栈溢出检测**:
```c
/* FreeRTOS */
#define configCHECK_FOR_STACK_OVERFLOW 2
```

### 问题 2.3: 任务优先级反转

**症状**: 高优先级任务被低优先级任务阻塞

**解决方案**:

```c
/* 使用互斥锁（支持优先级继承）*/
osal_mutex_handle_t mutex;
osal_mutex_create(&mutex);

/* 不要使用信号量保护共享资源 */
/* osal_sem_handle_t sem;  // 错误！ */
```

## 3. 同步问题

### 问题 3.1: 死锁

**症状**: 多个任务永久阻塞

**诊断步骤**:

```c
/* 打印所有任务状态 */
void debug_deadlock(void) {
    osal_task_info_t task_list[16];
    size_t count;
    
    osal_diag_get_task_list(task_list, 16, &count);
    
    printf("Task List:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  %s: state=%d, priority=%d\n",
               task_list[i].name,
               task_list[i].state,
               task_list[i].priority);
    }
    
    /* 检查互斥锁所有者 */
    if (osal_mutex_is_locked(mutex1)) {
        osal_task_handle_t owner = osal_mutex_get_owner(mutex1);
        printf("Mutex1 owner: %s\n", osal_task_get_name(owner));
    }
}
```

**解决方案**:

1. **按固定顺序获取锁**:
```c
/* 推荐：总是按 A -> B 的顺序 */
void safe_function(void) {
    osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);
    osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);
    
    /* 访问资源 */
    
    osal_mutex_unlock(mutex_b);
    osal_mutex_unlock(mutex_a);
}
```

2. **使用超时**:
```c
/* 使用超时避免永久阻塞 */
osal_status_t status = osal_mutex_lock(mutex, 1000);
if (status == OSAL_ERROR_TIMEOUT) {
    printf("Mutex lock timeout - possible deadlock\n");
    return;
}
```

3. **尝试锁定模式**:
```c
/* 尝试获取所有锁，失败则全部释放 */
bool try_lock_all(void) {
    if (osal_mutex_lock(mutex_a, OSAL_NO_WAIT) != OSAL_OK) {
        return false;
    }
    
    if (osal_mutex_lock(mutex_b, OSAL_NO_WAIT) != OSAL_OK) {
        osal_mutex_unlock(mutex_a);
        return false;
    }
    
    return true;
}

void safe_access(void) {
    while (!try_lock_all()) {
        osal_task_delay(10);  /* 等待后重试 */
    }
    
    /* 访问资源 */
    
    osal_mutex_unlock(mutex_b);
    osal_mutex_unlock(mutex_a);
}
```

### 问题 3.2: 信号量计数错误

**症状**: 信号量计数不符合预期

**诊断步骤**:

```c
/* 检查信号量状态 */
void check_semaphore(void) {
    uint32_t count = osal_sem_get_count(sem);
    printf("Semaphore count: %u\n", count);
    
    /* 检查是否有任务在等待 */
    /* 注意：不是所有 RTOS 都提供此功能 */
}
```

**解决方案**:

```c
/* 确保 post 和 wait 配对 */
void producer(void) {
    produce_data();
    osal_sem_post(sem);  /* 每次生产后 post */
}

void consumer(void) {
    osal_sem_wait(sem, OSAL_WAIT_FOREVER);  /* 每次消费前 wait */
    consume_data();
}
```

## 4. 队列问题

### 问题 4.1: 队列满

**症状**: `osal_queue_send()` 返回超时或失败

**诊断步骤**:

```c
/* 检查队列状态 */
void check_queue(void) {
    size_t count = osal_queue_get_count(queue);
    size_t available = osal_queue_get_available(queue);
    
    printf("Queue: %zu/%zu items\n", count, count + available);
    
    if (osal_queue_is_full(queue)) {
        printf("WARNING: Queue is full!\n");
    }
}
```

**解决方案**:

1. **增加队列大小**:
```c
/* 创建更大的队列 */
osal_queue_create(&queue, 20, sizeof(message_t));  /* 20 个元素 */
```

2. **加快消费速度**:
```c
/* 消费者任务优先级提高 */
consumer_config.priority = OSAL_TASK_PRIORITY_HIGH;
```

3. **使用非阻塞发送**:
```c
/* 队列满时丢弃消息 */
osal_status_t status = osal_queue_send(queue, &msg, OSAL_NO_WAIT);
if (status == OSAL_ERROR_TIMEOUT) {
    printf("Queue full, message dropped\n");
    dropped_count++;
}
```

### 问题 4.2: 队列数据损坏

**症状**: 接收到的数据不正确

**可能原因**:
1. 元素大小不匹配
2. 数据竞争
3. 堆栈变量生命周期问题

**解决方案**:

```c
/* 确保元素大小正确 */
typedef struct {
    uint8_t type;
    uint32_t value;
    char message[32];
} app_message_t;

/* 创建队列时使用正确的大小 */
osal_queue_create(&queue, 10, sizeof(app_message_t));

/* 发送时使用正确的类型 */
app_message_t msg = {
    .type = 1,
    .value = 123,
};
strcpy(msg.message, "Hello");
osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);

/* 避免：发送栈上临时变量的指针 */
void bad_send(void) {
    app_message_t msg;  /* 栈上变量 */
    /* ... */
    osal_queue_send(queue, &msg, OSAL_NO_WAIT);  /* 危险！ */
}  /* msg 被销毁 */
```

## 5. 定时器问题

### 问题 5.1: 定时器不触发

**症状**: 定时器回调函数未被调用

**诊断步骤**:

```c
/* 检查定时器状态 */
void check_timer(void) {
    if (osal_timer_is_active(timer)) {
        printf("Timer is active\n");
    } else {
        printf("Timer is NOT active\n");
    }
    
    uint32_t period = osal_timer_get_period(timer);
    printf("Timer period: %u ms\n", period);
}
```

**解决方案**:

```c
/* 确保定时器已启动 */
osal_timer_create(&timer, "my_timer", 1000, true, timer_callback, NULL);
osal_timer_start(timer);  /* 必须启动！ */

/* 检查回调函数 */
void timer_callback(osal_timer_handle_t timer, void* arg) {
    printf("Timer fired!\n");  /* 添加日志确认 */
}
```

### 问题 5.2: 定时器精度不足

**症状**: 定时器触发时间不准确

**解决方案**:

```c
/* 检查系统 tick 频率 */
#if defined(OSAL_FREERTOS)
    printf("Tick rate: %u Hz\n", configTICK_RATE_HZ);
    printf("Tick period: %u ms\n", 1000 / configTICK_RATE_HZ);
#endif

/* 定时器周期应该是 tick 周期的整数倍 */
/* 如果 tick = 1ms，则定时器周期应该是 1, 2, 3... ms */
```


## 6. 内存问题

### 问题 6.1: 内存分配失败

**症状**:
```c
void* ptr = osal_mem_alloc(1024);
/* ptr == NULL */
```

**可能原因**:
1. 堆内存不足
2. 内存碎片化
3. 内存泄漏

**诊断步骤**:

```c
/* 检查内存使用情况 */
void check_memory_usage(void) {
    osal_mem_stats_t stats;
    osal_mem_get_stats(&stats);
    
    printf("Memory Statistics:\n");
    printf("  Total heap: %zu bytes\n", stats.total_size);
    printf("  Used: %zu bytes (%.1f%%)\n", 
           stats.used_size,
           (float)stats.used_size * 100 / stats.total_size);
    printf("  Free: %zu bytes\n", stats.free_size);
    printf("  Peak usage: %zu bytes\n", stats.peak_used);
    printf("  Allocations: %u\n", stats.alloc_count);
    printf("  Frees: %u\n", stats.free_count);
    printf("  Failed allocs: %u\n", stats.failed_alloc_count);
    
    /* 检查是否有内存泄漏 */
    if (stats.alloc_count > stats.free_count) {
        uint32_t leaked = stats.alloc_count - stats.free_count;
        printf("WARNING: Possible memory leak (%u allocations not freed)\n", leaked);
    }
}

/* 尝试分配更小的内存 */
void* ptr = osal_mem_alloc(512);  /* 减小分配大小 */
if (ptr == NULL) {
    printf("Even small allocation failed - heap exhausted\n");
}
```

**解决方案**:

1. **增加堆大小**:
```c
/* FreeRTOS: 在 FreeRTOSConfig.h 中 */
#define configTOTAL_HEAP_SIZE ((size_t)(40 * 1024))  /* 增加到 40KB */
```

2. **使用内存池**:
```c
/* 为频繁分配的对象使用内存池 */
osal_mempool_handle_t pool;
osal_mempool_config_t config = {
    .block_size = 128,
    .block_count = 10,
};
osal_mempool_create(&config, &pool);

/* 从内存池分配 */
void* ptr = osal_mempool_alloc(pool);
/* 使用... */
osal_mempool_free(pool, ptr);
```

3. **及时释放内存**:
```c
/* 避免：忘记释放 */
void bad_function(void) {
    void* ptr = osal_mem_alloc(1024);
    /* ... */
    /* 忘记调用 osal_mem_free(ptr) */
}

/* 推荐：确保释放 */
void good_function(void) {
    void* ptr = osal_mem_alloc(1024);
    if (ptr) {
        /* ... */
        osal_mem_free(ptr);
    }
}
```

### 问题 6.2: 内存泄漏

**症状**: 可用内存逐渐减少，最终耗尽

**诊断步骤**:

```c
/* 启用内存泄漏检测 */
#define OSAL_MEM_DEBUG 1

/* 定期检查内存使用 */
void monitor_memory(void) {
    static size_t last_used = 0;
    
    osal_mem_stats_t stats;
    osal_mem_get_stats(&stats);
    
    if (stats.used_size > last_used) {
        printf("Memory usage increased: %zu -> %zu bytes\n",
               last_used, stats.used_size);
    }
    
    last_used = stats.used_size;
}

/* 打印内存分配跟踪 */
void dump_allocations(void) {
    #if OSAL_MEM_DEBUG
        osal_mem_dump_allocations();
    #endif
}
```

**解决方案**:

1. **使用 RAII 模式**:
```c
/* 使用结构化的资源管理 */
typedef struct {
    void* data;
    size_t size;
} buffer_t;

buffer_t* buffer_create(size_t size) {
    buffer_t* buf = osal_mem_alloc(sizeof(buffer_t));
    if (buf) {
        buf->data = osal_mem_alloc(size);
        if (!buf->data) {
            osal_mem_free(buf);
            return NULL;
        }
        buf->size = size;
    }
    return buf;
}

void buffer_destroy(buffer_t* buf) {
    if (buf) {
        if (buf->data) {
            osal_mem_free(buf->data);
        }
        osal_mem_free(buf);
    }
}
```

2. **检查所有错误路径**:
```c
/* 确保所有路径都释放资源 */
osal_status_t process_data(void) {
    void* buffer = osal_mem_alloc(1024);
    if (!buffer) {
        return OSAL_ERROR_NO_MEMORY;
    }
    
    osal_status_t status = do_something(buffer);
    if (status != OSAL_OK) {
        osal_mem_free(buffer);  /* 错误路径也要释放 */
        return status;
    }
    
    osal_mem_free(buffer);  /* 正常路径释放 */
    return OSAL_OK;
}
```

### 问题 6.3: 内存碎片化

**症状**: 有足够的总空闲内存，但无法分配大块内存

**诊断步骤**:

```c
/* 检查最大可分配块 */
void check_fragmentation(void) {
    osal_mem_stats_t stats;
    osal_mem_get_stats(&stats);
    
    printf("Free memory: %zu bytes\n", stats.free_size);
    
    /* 尝试分配大块内存 */
    size_t test_size = stats.free_size / 2;
    void* ptr = osal_mem_alloc(test_size);
    if (ptr) {
        printf("Can allocate %zu bytes\n", test_size);
        osal_mem_free(ptr);
    } else {
        printf("Cannot allocate %zu bytes - fragmentation?\n", test_size);
    }
}
```

**解决方案**:

1. **使用内存池**:
```c
/* 为固定大小的对象使用内存池 */
osal_mempool_handle_t small_pool;  /* 64 字节 */
osal_mempool_handle_t medium_pool; /* 256 字节 */
osal_mempool_handle_t large_pool;  /* 1024 字节 */

/* 根据需要从合适的池分配 */
void* alloc_by_size(size_t size) {
    if (size <= 64) {
        return osal_mempool_alloc(small_pool);
    } else if (size <= 256) {
        return osal_mempool_alloc(medium_pool);
    } else if (size <= 1024) {
        return osal_mempool_alloc(large_pool);
    } else {
        return osal_mem_alloc(size);
    }
}
```

2. **使用静态分配**:
```c
/* 对于长期存在的对象，使用静态分配 */
static uint8_t static_buffer[4096];

/* 而不是 */
/* uint8_t* buffer = osal_mem_alloc(4096); */
```

## 7. 性能问题

### 问题 7.1: CPU 使用率过高

**症状**: 系统响应慢，任务执行延迟

**诊断步骤**:

```c
/* 检查任务 CPU 使用率 */
void check_cpu_usage(void) {
    osal_task_info_t task_list[16];
    size_t count;
    
    osal_diag_get_task_list(task_list, 16, &count);
    
    printf("Task CPU Usage:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  %s: %.1f%%\n",
               task_list[i].name,
               task_list[i].cpu_usage);
    }
    
    /* 检查总 CPU 使用率 */
    float total_cpu = osal_diag_get_cpu_usage();
    printf("Total CPU: %.1f%%\n", total_cpu);
}

/* 检查任务运行时间 */
void check_task_runtime(void) {
    osal_task_handle_t task = osal_task_get_current();
    osal_task_stats_t stats;
    osal_task_get_stats(task, &stats);
    
    printf("Task: %s\n", stats.name);
    printf("Runtime: %u ms\n", stats.runtime_ms);
    printf("Context switches: %u\n", stats.context_switches);
}
```

**解决方案**:

1. **降低任务优先级**:
```c
/* 降低非关键任务的优先级 */
osal_task_set_priority(task, OSAL_TASK_PRIORITY_LOW);
```

2. **增加延时**:
```c
/* 避免：忙等待 */
void bad_task(void* arg) {
    while (1) {
        check_condition();  /* 持续占用 CPU */
    }
}

/* 推荐：使用延时 */
void good_task(void* arg) {
    while (1) {
        check_condition();
        osal_task_delay(10);  /* 释放 CPU */
    }
}
```

3. **使用事件驱动**:
```c
/* 使用信号量或事件等待 */
void event_driven_task(void* arg) {
    while (1) {
        /* 等待事件，不占用 CPU */
        osal_sem_wait(event_sem, OSAL_WAIT_FOREVER);
        
        /* 处理事件 */
        handle_event();
    }
}
```

### 问题 7.2: 任务响应慢

**症状**: 任务延迟高，实时性差

**诊断步骤**:

```c
/* 测量任务响应时间 */
void measure_response_time(void) {
    uint32_t start = osal_time_get_ms();
    
    /* 触发任务 */
    osal_sem_post(task_sem);
    
    /* 等待任务完成 */
    osal_sem_wait(done_sem, OSAL_WAIT_FOREVER);
    
    uint32_t end = osal_time_get_ms();
    printf("Response time: %u ms\n", end - start);
}
```

**解决方案**:

1. **提高任务优先级**:
```c
/* 提高关键任务的优先级 */
osal_task_set_priority(critical_task, OSAL_TASK_PRIORITY_REALTIME);
```

2. **减少临界区时间**:
```c
/* 避免：长时间持有锁 */
void bad_function(void) {
    osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
    
    /* 长时间操作 */
    process_large_data();
    
    osal_mutex_unlock(mutex);
}

/* 推荐：缩短临界区 */
void good_function(void) {
    /* 在锁外准备数据 */
    prepare_data();
    
    /* 只在必要时持有锁 */
    osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
    update_shared_data();
    osal_mutex_unlock(mutex);
}
```

3. **使用更高效的同步机制**:
```c
/* 使用事件标志代替多个信号量 */
osal_event_handle_t events;
osal_event_create(&events);

/* 等待多个事件 */
uint32_t flags = osal_event_wait(events, 
                                  EVENT_A | EVENT_B,
                                  true,  /* 等待所有 */
                                  1000);
```

### 问题 7.3: 上下文切换过多

**症状**: 系统开销大，吞吐量低

**诊断步骤**:

```c
/* 检查上下文切换次数 */
void check_context_switches(void) {
    osal_diag_stats_t stats;
    osal_diag_get_stats(&stats);
    
    printf("Context switches: %u\n", stats.context_switches);
    printf("Interrupts: %u\n", stats.interrupt_count);
}
```

**解决方案**:

1. **合并任务**:
```c
/* 避免：过多的小任务 */
/* task1, task2, task3... */

/* 推荐：合并相关任务 */
void combined_task(void* arg) {
    while (1) {
        do_task1_work();
        do_task2_work();
        do_task3_work();
        osal_task_delay(10);
    }
}
```

2. **增加时间片**:
```c
/* FreeRTOS */
#define configTICK_RATE_HZ 100  /* 降低 tick 频率 */
```

## 8. RTOS 适配问题

### 问题 8.1: 适配器编译错误

**症状**: 编译时出现未定义的符号

**解决方案**:

```c
/* 检查 RTOS 头文件是否正确包含 */
#if defined(OSAL_FREERTOS)
    #include "FreeRTOS.h"
    #include "task.h"
    #include "semphr.h"
    #include "queue.h"
#elif defined(OSAL_RTTHREAD)
    #include <rtthread.h>
#endif

/* 检查 Kconfig 配置 */
/* CONFIG_OSAL_FREERTOS=y */
```

### 问题 8.2: 运行时崩溃

**症状**: 系统在 RTOS 调用时崩溃

**诊断步骤**:

```c
/* 检查 RTOS 是否已初始化 */
#if defined(OSAL_FREERTOS)
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        printf("ERROR: Scheduler not started\n");
    }
#endif

/* 检查中断优先级配置 */
#if defined(OSAL_FREERTOS)
    /* 确保中断优先级正确 */
    NVIC_SetPriority(IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
```

**解决方案**:

```c
/* 确保正确的初始化顺序 */
int main(void) {
    /* 1. 硬件初始化 */
    SystemInit();
    
    /* 2. OSAL 初始化（会初始化 RTOS）*/
    osal_init();
    
    /* 3. 创建任务 */
    create_tasks();
    
    /* 4. 启动调度器 */
    osal_start();
}
```

### 问题 8.3: API 行为不一致

**症状**: 相同的代码在不同 RTOS 上行为不同

**解决方案**:

```c
/* 使用 OSAL 提供的抽象，避免直接调用 RTOS API */

/* 避免：直接使用 RTOS API */
#if defined(OSAL_FREERTOS)
    xSemaphoreTake(sem, portMAX_DELAY);
#elif defined(OSAL_RTTHREAD)
    rt_sem_take(sem, RT_WAITING_FOREVER);
#endif

/* 推荐：使用 OSAL API */
osal_sem_wait(sem, OSAL_WAIT_FOREVER);
```

## 9. 调试技巧

### 9.1 启用调试日志

```c
/* 在 Kconfig 中启用调试 */
CONFIG_OSAL_DEBUG=y
CONFIG_OSAL_LOG_LEVEL=4  /* DEBUG 级别 */

/* 在代码中使用日志 */
#include "osal/osal_log.h"

OSAL_LOG_DEBUG("Task created: %s", task_name);
OSAL_LOG_INFO("Mutex locked");
OSAL_LOG_WARN("Queue almost full: %zu/%zu", count, max);
OSAL_LOG_ERROR("Failed to allocate memory: %zu bytes", size);
```

### 9.2 使用断言

```c
/* 启用断言 */
#define OSAL_ASSERT_ENABLED 1

/* 在代码中使用断言 */
#include "osal/osal_assert.h"

void my_function(void* ptr) {
    OSAL_ASSERT(ptr != NULL);
    OSAL_ASSERT(size > 0 && size <= MAX_SIZE);
    
    /* ... */
}

/* 自定义断言处理 */
void osal_assert_handler(const char* file, int line, const char* expr) {
    printf("ASSERT FAILED: %s:%d: %s\n", file, line, expr);
    while (1) {
        /* 停止执行 */
    }
}
```

### 9.3 任务跟踪

```c
/* 启用任务跟踪 */
#if defined(OSAL_FREERTOS)
    #define configUSE_TRACE_FACILITY 1
    #define configGENERATE_RUN_TIME_STATS 1
#endif

/* 打印任务列表 */
void print_task_list(void) {
    osal_task_info_t tasks[16];
    size_t count;
    
    osal_diag_get_task_list(tasks, 16, &count);
    
    printf("\nTask List:\n");
    printf("%-16s %8s %8s %8s\n", "Name", "State", "Priority", "Stack");
    printf("------------------------------------------------\n");
    
    for (size_t i = 0; i < count; i++) {
        printf("%-16s %8d %8d %8zu\n",
               tasks[i].name,
               tasks[i].state,
               tasks[i].priority,
               tasks[i].stack_free);
    }
}
```

### 9.4 内存跟踪

```c
/* 启用内存跟踪 */
#define OSAL_MEM_DEBUG 1

/* 定期打印内存统计 */
void print_memory_stats(void) {
    osal_mem_stats_t stats;
    osal_mem_get_stats(&stats);
    
    printf("\nMemory Statistics:\n");
    printf("  Total:        %8zu bytes\n", stats.total_size);
    printf("  Used:         %8zu bytes (%.1f%%)\n",
           stats.used_size,
           (float)stats.used_size * 100 / stats.total_size);
    printf("  Free:         %8zu bytes\n", stats.free_size);
    printf("  Peak:         %8zu bytes\n", stats.peak_used);
    printf("  Allocations:  %8u\n", stats.alloc_count);
    printf("  Frees:        %8u\n", stats.free_count);
    printf("  Failed:       %8u\n", stats.failed_alloc_count);
}

/* 打印内存分配跟踪 */
void dump_memory_allocations(void) {
    #if OSAL_MEM_DEBUG
        printf("\nMemory Allocations:\n");
        osal_mem_dump_allocations();
    #endif
}
```

### 9.5 性能分析

```c
/* 测量函数执行时间 */
#define MEASURE_TIME(func) do { \
    uint32_t start = osal_time_get_us(); \
    func; \
    uint32_t end = osal_time_get_us(); \
    printf(#func " took %u us\n", end - start); \
} while(0)

/* 使用示例 */
MEASURE_TIME(process_data());

/* 测量任务切换时间 */
void measure_context_switch_time(void) {
    uint32_t start = osal_time_get_us();
    osal_task_yield();
    uint32_t end = osal_time_get_us();
    printf("Context switch: %u us\n", end - start);
}
```

## 10. 常见错误码

### 10.1 错误码速查表

| 错误码 | 值 | 含义 | 常见原因 |
|-------|---|------|---------|
| OSAL_OK | 0 | 成功 | - |
| OSAL_ERROR | -1 | 一般错误 | 未指定的错误 |
| OSAL_ERROR_NULL_POINTER | -2 | 空指针 | 传入 NULL 参数 |
| OSAL_ERROR_INVALID_PARAM | -3 | 无效参数 | 参数超出范围 |
| OSAL_ERROR_NO_MEMORY | -4 | 内存不足 | 堆内存耗尽 |
| OSAL_ERROR_TIMEOUT | -5 | 超时 | 等待超时 |
| OSAL_ERROR_BUSY | -6 | 资源忙 | 资源被占用 |
| OSAL_ERROR_NOT_SUPPORTED | -7 | 不支持 | 功能未实现 |
| OSAL_ERROR_NOT_INITIALIZED | -8 | 未初始化 | OSAL 未初始化 |
| OSAL_ERROR_ALREADY_EXISTS | -9 | 已存在 | 重复创建 |
| OSAL_ERROR_NOT_FOUND | -10 | 未找到 | 资源不存在 |
| OSAL_ERROR_FULL | -11 | 已满 | 队列或缓冲区满 |
| OSAL_ERROR_EMPTY | -12 | 为空 | 队列或缓冲区空 |

### 10.2 错误处理最佳实践

```c
/* 1. 总是检查返回值 */
osal_status_t status = osal_task_create(&config, &task);
if (status != OSAL_OK) {
    printf("Task create failed: %d\n", status);
    return status;
}

/* 2. 使用错误回调 */
void error_callback(void* user_data, osal_status_t status,
                    const char* module, const char* msg) {
    printf("[ERROR] %s: %s (code: %d)\n", module, msg, status);
    
    /* 记录到日志文件 */
    log_error(module, msg, status);
}

osal_set_error_callback(error_callback, NULL);

/* 3. 提供有意义的错误信息 */
if (size > MAX_SIZE) {
    osal_report_error(OSAL_ERROR_INVALID_PARAM,
                      "TASK",
                      "Stack size too large");
    return OSAL_ERROR_INVALID_PARAM;
}

/* 4. 清理资源 */
osal_status_t init_system(void) {
    osal_mutex_handle_t mutex;
    osal_sem_handle_t sem;
    
    if (osal_mutex_create(&mutex) != OSAL_OK) {
        return OSAL_ERROR_NO_MEMORY;
    }
    
    if (osal_sem_create(&sem, 0, 10) != OSAL_OK) {
        osal_mutex_delete(mutex);  /* 清理已创建的资源 */
        return OSAL_ERROR_NO_MEMORY;
    }
    
    return OSAL_OK;
}
```

### 10.3 调试检查清单

**初始化问题**:
- [ ] OSAL 是否已初始化？
- [ ] 初始化顺序是否正确？
- [ ] RTOS 配置是否正确？
- [ ] 内存是否足够？

**任务问题**:
- [ ] 任务堆栈是否足够？
- [ ] 任务优先级是否合理？
- [ ] 是否有堆栈溢出？
- [ ] 任务是否正确退出？

**同步问题**:
- [ ] 是否有死锁？
- [ ] 锁的顺序是否一致？
- [ ] 是否使用了超时？
- [ ] 是否有优先级反转？

**内存问题**:
- [ ] 是否有内存泄漏？
- [ ] 是否有内存碎片？
- [ ] 分配和释放是否配对？
- [ ] 是否使用了内存池？

**性能问题**:
- [ ] CPU 使用率是否过高？
- [ ] 任务优先级是否合理？
- [ ] 是否有忙等待？
- [ ] 临界区是否过长？

---

## 参考资源

- [OSAL 用户指南](USER_GUIDE.md)
- [OSAL 设计文档](DESIGN.md)
- [OSAL 测试指南](TEST_GUIDE.md)
- [OSAL 移植指南](PORTING_GUIDE.md)

如有问题，请联系 Nexus 团队或提交 Issue。
