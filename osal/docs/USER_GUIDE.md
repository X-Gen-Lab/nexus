# OSAL 使用指南

本文档提供 Nexus OSAL 的详细使用说明和最佳实践。

## 目录

1. [快速开始](#1-快速开始)
2. [OSAL 初始化](#2-osal-初始化)
3. [任务管理](#3-任务管理)
4. [互斥锁](#4-互斥锁)
5. [信号量](#5-信号量)
6. [事件标志](#6-事件标志)
7. [消息队列](#7-消息队列)
8. [软件定时器](#8-软件定时器)
9. [内存管理](#9-内存管理)
10. [诊断和调试](#10-诊断和调试)
11. [最佳实践](#11-最佳实践)
12. [常见问题](#12-常见问题)

## 1. 快速开始

### 1.1 最小示例

```c
#include "osal/osal.h"

/* 任务函数 */
void led_task(void* arg) {
    while (1) {
        /* 翻转 LED */
        gpio_toggle_led();
        
        /* 延时 500ms */
        osal_task_delay(500);
    }
}

int main(void) {
    /* 初始化 OSAL */
    osal_init();
    
    /* 创建任务 */
    osal_task_config_t config = {
        .name = "led_task",
        .func = led_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 512,
    };
    
    osal_task_handle_t task;
    osal_task_create(&config, &task);
    
    /* 启动调度器 */
    osal_start();  /* 不返回 */
    
    return 0;
}
```

### 1.2 包含头文件

```c
/* 方式 1: 包含主头文件（推荐）*/
#include "osal/osal.h"

/* 方式 2: 只包含需要的模块 */
#include "osal/osal_def.h"
#include "osal/osal_task.h"
#include "osal/osal_mutex.h"
```

## 2. OSAL 初始化

### 2.1 基本初始化

```c
#include "osal/osal.h"

int main(void) {
    /* 初始化 OSAL */
    osal_status_t status = osal_init();
    if (status != OSAL_OK) {
        printf("OSAL init failed\n");
        return -1;
    }
    
    /* 创建任务 */
    /* ... */
    
    /* 启动调度器 */
    osal_start();  /* 不返回 */
    
    return 0;
}
```

### 2.2 检查调度器状态

```c
/* 检查调度器是否运行 */
if (osal_is_running()) {
    printf("Scheduler is running\n");
} else {
    printf("Scheduler is not running\n");
}
```

### 2.3 临界区保护

```c
/* 保护临界代码段 */
void critical_operation(void) {
    osal_enter_critical();
    
    /* 临界代码 */
    shared_variable++;
    
    osal_exit_critical();
}
```

## 3. 任务管理

### 3.1 创建任务

```c
void my_task(void* arg) {
    int* count = (int*)arg;
    
    while (1) {
        printf("Task running, count = %d\n", (*count)++);
        osal_task_delay(1000);
    }
}

void create_task_example(void) {
    static int count = 0;
    
    /* 配置任务 */
    osal_task_config_t config = {
        .name = "my_task",
        .func = my_task,
        .arg = &count,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,  /* 1KB 堆栈 */
    };
    
    /* 创建任务 */
    osal_task_handle_t task;
    osal_status_t status = osal_task_create(&config, &task);
    
    if (status == OSAL_OK) {
        printf("Task created successfully\n");
    } else {
        printf("Task creation failed: %d\n", status);
    }
}
```

### 3.2 任务优先级

```c
/* 使用预定义优先级 */
config.priority = OSAL_TASK_PRIORITY_IDLE;     /* 0 - 最低 */
config.priority = OSAL_TASK_PRIORITY_LOW;      /* 8 */
config.priority = OSAL_TASK_PRIORITY_NORMAL;   /* 16 */
config.priority = OSAL_TASK_PRIORITY_HIGH;     /* 24 */
config.priority = OSAL_TASK_PRIORITY_REALTIME; /* 31 - 最高 */

/* 或使用自定义优先级 (0-31) */
config.priority = 20;
```

### 3.3 任务删除

```c
void task_delete_example(void) {
    osal_task_handle_t task;
    
    /* 创建任务 */
    osal_task_config_t config = {
        .name = "temp_task",
        .func = temp_task_func,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 512,
    };
    osal_task_create(&config, &task);
    
    /* 运行一段时间后删除 */
    osal_task_delay(5000);
    osal_task_delete(task);
    
    printf("Task deleted\n");
}

/* 任务自删除 */
void self_delete_task(void* arg) {
    printf("Task will delete itself\n");
    osal_task_delay(1000);
    
    /* 删除当前任务 */
    osal_task_delete(NULL);  /* NULL 表示当前任务 */
}
```

### 3.4 任务挂起和恢复

```c
void suspend_resume_example(void) {
    osal_task_handle_t task;
    
    /* 创建任务 */
    osal_task_config_t config = {
        .name = "worker",
        .func = worker_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    osal_task_create(&config, &task);
    
    /* 运行 2 秒 */
    osal_task_delay(2000);
    
    /* 挂起任务 */
    osal_task_suspend(task);
    printf("Task suspended\n");
    
    /* 暂停 2 秒 */
    osal_task_delay(2000);
    
    /* 恢复任务 */
    osal_task_resume(task);
    printf("Task resumed\n");
}
```

### 3.5 任务延时和让步

```c
void delay_yield_example(void) {
    /* 延时 1 秒 */
    osal_task_delay(1000);
    
    /* 主动让出 CPU */
    osal_task_yield();
}
```

### 3.6 任务信息查询

```c
void task_info_example(void) {
    osal_task_handle_t task = osal_task_get_current();
    
    /* 获取任务名称 */
    const char* name = osal_task_get_name(task);
    printf("Current task: %s\n", name);
    
    /* 获取任务优先级 */
    uint8_t priority = osal_task_get_priority(task);
    printf("Priority: %d\n", priority);
    
    /* 获取堆栈水位 */
    size_t watermark = osal_task_get_stack_watermark(task);
    printf("Stack watermark: %zu bytes\n", watermark);
    
    /* 获取任务状态 */
    osal_task_state_t state = osal_task_get_state(task);
    switch (state) {
        case OSAL_TASK_STATE_READY:
            printf("State: Ready\n");
            break;
        case OSAL_TASK_STATE_RUNNING:
            printf("State: Running\n");
            break;
        case OSAL_TASK_STATE_BLOCKED:
            printf("State: Blocked\n");
            break;
        case OSAL_TASK_STATE_SUSPENDED:
            printf("State: Suspended\n");
            break;
        default:
            printf("State: Unknown\n");
            break;
    }
}
```

## 4. 互斥锁

### 4.1 创建和使用互斥锁

```c
static osal_mutex_handle_t g_mutex;
static int g_shared_counter = 0;

void mutex_example(void) {
    /* 创建互斥锁 */
    osal_status_t status = osal_mutex_create(&g_mutex);
    if (status != OSAL_OK) {
        printf("Mutex creation failed\n");
        return;
    }
    
    /* 锁定互斥锁 */
    status = osal_mutex_lock(g_mutex, OSAL_WAIT_FOREVER);
    if (status == OSAL_OK) {
        /* 访问共享资源 */
        g_shared_counter++;
        printf("Counter: %d\n", g_shared_counter);
        
        /* 解锁互斥锁 */
        osal_mutex_unlock(g_mutex);
    }
}
```

### 4.2 超时锁定

```c
void mutex_timeout_example(void) {
    /* 尝试锁定，最多等待 100ms */
    osal_status_t status = osal_mutex_lock(g_mutex, 100);
    
    if (status == OSAL_OK) {
        /* 成功获取锁 */
        g_shared_counter++;
        osal_mutex_unlock(g_mutex);
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Mutex lock timeout\n");
    }
}
```

### 4.3 非阻塞锁定

```c
void mutex_trylock_example(void) {
    /* 尝试锁定，不等待 */
    osal_status_t status = osal_mutex_lock(g_mutex, OSAL_NO_WAIT);
    
    if (status == OSAL_OK) {
        /* 成功获取锁 */
        g_shared_counter++;
        osal_mutex_unlock(g_mutex);
    } else {
        printf("Mutex is busy\n");
    }
}
```

### 4.4 互斥锁状态查询

```c
void mutex_query_example(void) {
    /* 检查互斥锁是否被锁定 */
    if (osal_mutex_is_locked(g_mutex)) {
        printf("Mutex is locked\n");
        
        /* 获取锁的所有者 */
        osal_task_handle_t owner = osal_mutex_get_owner(g_mutex);
        if (owner) {
            const char* owner_name = osal_task_get_name(owner);
            printf("Owner: %s\n", owner_name);
        }
    } else {
        printf("Mutex is unlocked\n");
    }
}
```

### 4.5 删除互斥锁

```c
void mutex_delete_example(void) {
    /* 删除互斥锁 */
    osal_status_t status = osal_mutex_delete(g_mutex);
    if (status == OSAL_OK) {
        printf("Mutex deleted\n");
        g_mutex = NULL;
    } else if (status == OSAL_ERROR_BUSY) {
        printf("Mutex is still locked\n");
    }
}
```


## 5. 信号量

### 5.1 创建信号量

```c
static osal_sem_handle_t g_sem;

void semaphore_create_example(void) {
    /* 创建计数信号量，初始值为 0，最大值为 10 */
    osal_status_t status = osal_sem_create(&g_sem, 0, 10);
    if (status == OSAL_OK) {
        printf("Semaphore created\n");
    }
}
```

### 5.2 信号量等待（P 操作）

```c
void semaphore_wait_example(void) {
    /* 等待信号量，永久阻塞 */
    osal_status_t status = osal_sem_wait(g_sem, OSAL_WAIT_FOREVER);
    if (status == OSAL_OK) {
        printf("Semaphore acquired\n");
        /* 执行受保护的操作 */
    }
}

void semaphore_wait_timeout_example(void) {
    /* 等待信号量，最多等待 500ms */
    osal_status_t status = osal_sem_wait(g_sem, 500);
    if (status == OSAL_OK) {
        printf("Semaphore acquired\n");
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Semaphore wait timeout\n");
    }
}

void semaphore_trywait_example(void) {
    /* 尝试获取信号量，不等待 */
    osal_status_t status = osal_sem_wait(g_sem, OSAL_NO_WAIT);
    if (status == OSAL_OK) {
        printf("Semaphore acquired immediately\n");
    } else {
        printf("Semaphore not available\n");
    }
}
```

### 5.3 信号量释放（V 操作）

```c
void semaphore_post_example(void) {
    /* 释放信号量 */
    osal_status_t status = osal_sem_post(g_sem);
    if (status == OSAL_OK) {
        printf("Semaphore posted\n");
    }
}
```

### 5.4 信号量查询

```c
void semaphore_query_example(void) {
    /* 获取信号量当前计数 */
    uint32_t count = osal_sem_get_count(g_sem);
    printf("Semaphore count: %u\n", count);
}
```

### 5.5 生产者-消费者示例

```c
#define BUFFER_SIZE 10
static uint8_t g_buffer[BUFFER_SIZE];
static size_t g_write_idx = 0;
static size_t g_read_idx = 0;

static osal_sem_handle_t g_empty_sem;  /* 空槽位信号量 */
static osal_sem_handle_t g_full_sem;   /* 满槽位信号量 */
static osal_mutex_handle_t g_buffer_mutex;

void producer_consumer_init(void) {
    /* 创建信号量 */
    osal_sem_create(&g_empty_sem, BUFFER_SIZE, BUFFER_SIZE);  /* 初始全空 */
    osal_sem_create(&g_full_sem, 0, BUFFER_SIZE);             /* 初始无数据 */
    osal_mutex_create(&g_buffer_mutex);
}

void producer_task(void* arg) {
    uint8_t data = 0;
    
    while (1) {
        /* 等待空槽位 */
        osal_sem_wait(g_empty_sem, OSAL_WAIT_FOREVER);
        
        /* 写入数据 */
        osal_mutex_lock(g_buffer_mutex, OSAL_WAIT_FOREVER);
        g_buffer[g_write_idx] = data++;
        g_write_idx = (g_write_idx + 1) % BUFFER_SIZE;
        osal_mutex_unlock(g_buffer_mutex);
        
        /* 通知有新数据 */
        osal_sem_post(g_full_sem);
        
        printf("Produced: %d\n", data - 1);
        osal_task_delay(100);
    }
}

void consumer_task(void* arg) {
    while (1) {
        /* 等待数据 */
        osal_sem_wait(g_full_sem, OSAL_WAIT_FOREVER);
        
        /* 读取数据 */
        osal_mutex_lock(g_buffer_mutex, OSAL_WAIT_FOREVER);
        uint8_t data = g_buffer[g_read_idx];
        g_read_idx = (g_read_idx + 1) % BUFFER_SIZE;
        osal_mutex_unlock(g_buffer_mutex);
        
        /* 释放空槽位 */
        osal_sem_post(g_empty_sem);
        
        printf("Consumed: %d\n", data);
        osal_task_delay(150);
    }
}
```

### 5.6 删除信号量

```c
void semaphore_delete_example(void) {
    osal_status_t status = osal_sem_delete(g_sem);
    if (status == OSAL_OK) {
        printf("Semaphore deleted\n");
        g_sem = NULL;
    }
}
```

## 6. 事件标志

### 6.1 创建事件标志组

```c
static osal_event_handle_t g_event;

void event_create_example(void) {
    osal_status_t status = osal_event_create(&g_event);
    if (status == OSAL_OK) {
        printf("Event group created\n");
    }
}
```

### 6.2 设置事件标志

```c
/* 定义事件位 */
#define EVENT_BIT_0  (1 << 0)  /* 0x01 */
#define EVENT_BIT_1  (1 << 1)  /* 0x02 */
#define EVENT_BIT_2  (1 << 2)  /* 0x04 */
#define EVENT_BIT_3  (1 << 3)  /* 0x08 */

void event_set_example(void) {
    /* 设置单个事件位 */
    osal_event_set(g_event, EVENT_BIT_0);
    
    /* 设置多个事件位 */
    osal_event_set(g_event, EVENT_BIT_1 | EVENT_BIT_2);
    
    printf("Events set\n");
}
```

### 6.3 清除事件标志

```c
void event_clear_example(void) {
    /* 清除单个事件位 */
    osal_event_clear(g_event, EVENT_BIT_0);
    
    /* 清除多个事件位 */
    osal_event_clear(g_event, EVENT_BIT_1 | EVENT_BIT_2);
    
    printf("Events cleared\n");
}
```

### 6.4 等待事件标志

```c
void event_wait_any_example(void) {
    /* 等待任意一个事件位被设置 */
    uint32_t bits;
    osal_status_t status = osal_event_wait(g_event, 
                                           EVENT_BIT_0 | EVENT_BIT_1,
                                           false,  /* 等待任意 */
                                           true,   /* 自动清除 */
                                           &bits,
                                           OSAL_WAIT_FOREVER);
    
    if (status == OSAL_OK) {
        printf("Event received: 0x%02X\n", bits);
        
        if (bits & EVENT_BIT_0) {
            printf("Event 0 occurred\n");
        }
        if (bits & EVENT_BIT_1) {
            printf("Event 1 occurred\n");
        }
    }
}

void event_wait_all_example(void) {
    /* 等待所有事件位都被设置 */
    uint32_t bits;
    osal_status_t status = osal_event_wait(g_event,
                                           EVENT_BIT_0 | EVENT_BIT_1 | EVENT_BIT_2,
                                           true,   /* 等待全部 */
                                           true,   /* 自动清除 */
                                           &bits,
                                           5000);  /* 5 秒超时 */
    
    if (status == OSAL_OK) {
        printf("All events received: 0x%02X\n", bits);
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Event wait timeout\n");
    }
}
```

### 6.5 获取事件标志状态

```c
void event_get_example(void) {
    /* 获取当前事件标志状态 */
    uint32_t bits = osal_event_get(g_event);
    printf("Current events: 0x%02X\n", bits);
    
    /* 检查特定事件位 */
    if (bits & EVENT_BIT_0) {
        printf("Event 0 is set\n");
    }
    if (bits & EVENT_BIT_1) {
        printf("Event 1 is set\n");
    }
}
```

### 6.6 多任务同步示例

```c
/* 任务 1: 数据采集 */
void data_acquisition_task(void* arg) {
    while (1) {
        /* 采集数据 */
        acquire_sensor_data();
        
        /* 通知数据就绪 */
        osal_event_set(g_event, EVENT_BIT_0);
        
        osal_task_delay(1000);
    }
}

/* 任务 2: 数据处理 */
void data_processing_task(void* arg) {
    while (1) {
        /* 等待数据就绪 */
        uint32_t bits;
        osal_event_wait(g_event, EVENT_BIT_0, false, true, &bits, OSAL_WAIT_FOREVER);
        
        /* 处理数据 */
        process_sensor_data();
        
        /* 通知处理完成 */
        osal_event_set(g_event, EVENT_BIT_1);
    }
}

/* 任务 3: 数据上传 */
void data_upload_task(void* arg) {
    while (1) {
        /* 等待处理完成 */
        uint32_t bits;
        osal_event_wait(g_event, EVENT_BIT_1, false, true, &bits, OSAL_WAIT_FOREVER);
        
        /* 上传数据 */
        upload_to_cloud();
    }
}
```

### 6.7 删除事件标志组

```c
void event_delete_example(void) {
    osal_status_t status = osal_event_delete(g_event);
    if (status == OSAL_OK) {
        printf("Event group deleted\n");
        g_event = NULL;
    }
}
```

## 7. 消息队列

### 7.1 创建消息队列

```c
static osal_queue_handle_t g_queue;

void queue_create_example(void) {
    /* 创建队列：10 个元素，每个元素 4 字节 */
    osal_status_t status = osal_queue_create(&g_queue, 10, sizeof(uint32_t));
    if (status == OSAL_OK) {
        printf("Queue created\n");
    }
}
```

### 7.2 发送消息

```c
void queue_send_example(void) {
    uint32_t data = 12345;
    
    /* 发送到队列尾部，永久等待 */
    osal_status_t status = osal_queue_send(g_queue, &data, OSAL_WAIT_FOREVER);
    if (status == OSAL_OK) {
        printf("Message sent: %u\n", data);
    }
}

void queue_send_timeout_example(void) {
    uint32_t data = 67890;
    
    /* 发送到队列尾部，最多等待 100ms */
    osal_status_t status = osal_queue_send(g_queue, &data, 100);
    if (status == OSAL_OK) {
        printf("Message sent: %u\n", data);
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Queue send timeout (queue full)\n");
    }
}

void queue_send_front_example(void) {
    uint32_t urgent_data = 99999;
    
    /* 发送到队列头部（紧急消息）*/
    osal_status_t status = osal_queue_send_front(g_queue, &urgent_data, OSAL_NO_WAIT);
    if (status == OSAL_OK) {
        printf("Urgent message sent: %u\n", urgent_data);
    }
}
```

### 7.3 接收消息

```c
void queue_receive_example(void) {
    uint32_t data;
    
    /* 从队列接收，永久等待 */
    osal_status_t status = osal_queue_receive(g_queue, &data, OSAL_WAIT_FOREVER);
    if (status == OSAL_OK) {
        printf("Message received: %u\n", data);
    }
}

void queue_receive_timeout_example(void) {
    uint32_t data;
    
    /* 从队列接收，最多等待 500ms */
    osal_status_t status = osal_queue_receive(g_queue, &data, 500);
    if (status == OSAL_OK) {
        printf("Message received: %u\n", data);
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Queue receive timeout (queue empty)\n");
    }
}
```

### 7.4 查看消息（不移除）

```c
void queue_peek_example(void) {
    uint32_t data;
    
    /* 查看队列头部消息，不移除 */
    osal_status_t status = osal_queue_peek(g_queue, &data, OSAL_NO_WAIT);
    if (status == OSAL_OK) {
        printf("Peeked message: %u\n", data);
        /* 消息仍在队列中 */
    }
}
```

### 7.5 队列状态查询

```c
void queue_query_example(void) {
    /* 获取队列中的消息数量 */
    size_t count = osal_queue_get_count(g_queue);
    printf("Messages in queue: %zu\n", count);
    
    /* 获取队列可用空间 */
    size_t available = osal_queue_get_available(g_queue);
    printf("Available space: %zu\n", available);
    
    /* 检查队列是否为空 */
    if (osal_queue_is_empty(g_queue)) {
        printf("Queue is empty\n");
    }
    
    /* 检查队列是否已满 */
    if (osal_queue_is_full(g_queue)) {
        printf("Queue is full\n");
    }
}
```

### 7.6 清空队列

```c
void queue_reset_example(void) {
    osal_status_t status = osal_queue_reset(g_queue);
    if (status == OSAL_OK) {
        printf("Queue cleared\n");
    }
}
```

### 7.7 任务间通信示例

```c
/* 消息结构 */
typedef struct {
    uint8_t type;
    uint32_t value;
    char message[32];
} app_message_t;

static osal_queue_handle_t g_msg_queue;

void sender_task(void* arg) {
    app_message_t msg;
    uint32_t counter = 0;
    
    while (1) {
        /* 准备消息 */
        msg.type = 1;
        msg.value = counter++;
        snprintf(msg.message, sizeof(msg.message), "Message %u", counter);
        
        /* 发送消息 */
        osal_status_t status = osal_queue_send(g_msg_queue, &msg, 1000);
        if (status == OSAL_OK) {
            printf("Sent: %s\n", msg.message);
        } else {
            printf("Send failed\n");
        }
        
        osal_task_delay(500);
    }
}

void receiver_task(void* arg) {
    app_message_t msg;
    
    while (1) {
        /* 接收消息 */
        osal_status_t status = osal_queue_receive(g_msg_queue, &msg, OSAL_WAIT_FOREVER);
        if (status == OSAL_OK) {
            printf("Received: type=%d, value=%u, msg=%s\n",
                   msg.type, msg.value, msg.message);
            
            /* 处理消息 */
            process_message(&msg);
        }
    }
}

void message_queue_init(void) {
    /* 创建消息队列 */
    osal_queue_create(&g_msg_queue, 10, sizeof(app_message_t));
    
    /* 创建发送任务 */
    osal_task_config_t sender_config = {
        .name = "sender",
        .func = sender_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    osal_task_handle_t sender;
    osal_task_create(&sender_config, &sender);
    
    /* 创建接收任务 */
    osal_task_config_t receiver_config = {
        .name = "receiver",
        .func = receiver_task,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    osal_task_handle_t receiver;
    osal_task_create(&receiver_config, &receiver);
}
```

### 7.8 删除消息队列

```c
void queue_delete_example(void) {
    osal_status_t status = osal_queue_delete(g_queue);
    if (status == OSAL_OK) {
        printf("Queue deleted\n");
        g_queue = NULL;
    }
}
```


## 8. 软件定时器

### 8.1 创建定时器

```c
static osal_timer_handle_t g_timer;

/* 定时器回调函数 */
void timer_callback(osal_timer_handle_t timer, void* arg) {
    int* count = (int*)arg;
    (*count)++;
    printf("Timer fired, count = %d\n", *count);
}

void timer_create_example(void) {
    static int count = 0;
    
    /* 创建周期定时器，1000ms 周期 */
    osal_status_t status = osal_timer_create(&g_timer,
                                             "my_timer",
                                             1000,           /* 周期 1000ms */
                                             true,           /* 周期定时器 */
                                             timer_callback,
                                             &count);
    if (status == OSAL_OK) {
        printf("Timer created\n");
    }
}
```

### 8.2 启动和停止定时器

```c
void timer_start_stop_example(void) {
    /* 启动定时器 */
    osal_status_t status = osal_timer_start(g_timer);
    if (status == OSAL_OK) {
        printf("Timer started\n");
    }
    
    /* 运行 5 秒 */
    osal_task_delay(5000);
    
    /* 停止定时器 */
    status = osal_timer_stop(g_timer);
    if (status == OSAL_OK) {
        printf("Timer stopped\n");
    }
}
```

### 8.3 单次定时器

```c
void one_shot_timer_callback(osal_timer_handle_t timer, void* arg) {
    printf("One-shot timer fired\n");
    /* 定时器自动停止，不会再次触发 */
}

void one_shot_timer_example(void) {
    osal_timer_handle_t timer;
    
    /* 创建单次定时器，3000ms 后触发 */
    osal_timer_create(&timer,
                     "one_shot",
                     3000,              /* 3 秒后触发 */
                     false,             /* 单次定时器 */
                     one_shot_timer_callback,
                     NULL);
    
    /* 启动定时器 */
    osal_timer_start(timer);
    
    printf("One-shot timer started, will fire in 3 seconds\n");
}
```

### 8.4 修改定时器周期

```c
void timer_change_period_example(void) {
    /* 修改定时器周期为 2000ms */
    osal_status_t status = osal_timer_change_period(g_timer, 2000);
    if (status == OSAL_OK) {
        printf("Timer period changed to 2000ms\n");
    }
}
```

### 8.5 重置定时器

```c
void timer_reset_example(void) {
    /* 重置定时器（重新开始计时）*/
    osal_status_t status = osal_timer_reset(g_timer);
    if (status == OSAL_OK) {
        printf("Timer reset\n");
    }
}
```

### 8.6 定时器状态查询

```c
void timer_query_example(void) {
    /* 检查定时器是否激活 */
    if (osal_timer_is_active(g_timer)) {
        printf("Timer is active\n");
    } else {
        printf("Timer is inactive\n");
    }
    
    /* 获取定时器名称 */
    const char* name = osal_timer_get_name(g_timer);
    printf("Timer name: %s\n", name);
    
    /* 获取定时器周期 */
    uint32_t period = osal_timer_get_period(g_timer);
    printf("Timer period: %u ms\n", period);
}
```

### 8.7 看门狗定时器示例

```c
static osal_timer_handle_t g_watchdog_timer;
static volatile bool g_task_alive = false;

void watchdog_callback(osal_timer_handle_t timer, void* arg) {
    if (!g_task_alive) {
        printf("Watchdog timeout! Task not responding\n");
        /* 执行恢复操作 */
        system_reset();
    } else {
        /* 重置标志 */
        g_task_alive = false;
    }
}

void monitored_task(void* arg) {
    while (1) {
        /* 执行任务 */
        do_work();
        
        /* 喂狗 */
        g_task_alive = true;
        
        osal_task_delay(500);
    }
}

void watchdog_init(void) {
    /* 创建看门狗定时器，1 秒超时 */
    osal_timer_create(&g_watchdog_timer,
                     "watchdog",
                     1000,
                     true,  /* 周期定时器 */
                     watchdog_callback,
                     NULL);
    
    osal_timer_start(g_watchdog_timer);
}
```

### 8.8 删除定时器

```c
void timer_delete_example(void) {
    /* 停止定时器 */
    osal_timer_stop(g_timer);
    
    /* 删除定时器 */
    osal_status_t status = osal_timer_delete(g_timer);
    if (status == OSAL_OK) {
        printf("Timer deleted\n");
        g_timer = NULL;
    }
}
```

## 9. 内存管理

### 9.1 动态内存分配

```c
void memory_alloc_example(void) {
    /* 分配内存 */
    void* ptr = osal_mem_alloc(1024);
    if (ptr != NULL) {
        printf("Memory allocated: %p\n", ptr);
        
        /* 使用内存 */
        memset(ptr, 0, 1024);
        
        /* 释放内存 */
        osal_mem_free(ptr);
        printf("Memory freed\n");
    } else {
        printf("Memory allocation failed\n");
    }
}
```

### 9.2 对齐内存分配

```c
void memory_alloc_aligned_example(void) {
    /* 分配 16 字节对齐的内存 */
    void* ptr = osal_mem_alloc_aligned(1024, 16);
    if (ptr != NULL) {
        printf("Aligned memory allocated: %p\n", ptr);
        
        /* 检查对齐 */
        if (((uintptr_t)ptr % 16) == 0) {
            printf("Memory is properly aligned\n");
        }
        
        osal_mem_free(ptr);
    }
}
```

### 9.3 内存池

```c
/* 定义内存池 */
#define POOL_BLOCK_SIZE 64
#define POOL_BLOCK_COUNT 10

static osal_mem_pool_handle_t g_mem_pool;

void memory_pool_example(void) {
    /* 创建内存池 */
    osal_status_t status = osal_mem_pool_create(&g_mem_pool,
                                                POOL_BLOCK_SIZE,
                                                POOL_BLOCK_COUNT);
    if (status == OSAL_OK) {
        printf("Memory pool created\n");
    }
    
    /* 从内存池分配 */
    void* block1 = osal_mem_pool_alloc(g_mem_pool, OSAL_WAIT_FOREVER);
    void* block2 = osal_mem_pool_alloc(g_mem_pool, OSAL_WAIT_FOREVER);
    
    if (block1 && block2) {
        printf("Blocks allocated from pool\n");
        
        /* 使用内存块 */
        memset(block1, 0xAA, POOL_BLOCK_SIZE);
        memset(block2, 0xBB, POOL_BLOCK_SIZE);
        
        /* 释放回内存池 */
        osal_mem_pool_free(g_mem_pool, block1);
        osal_mem_pool_free(g_mem_pool, block2);
        printf("Blocks freed to pool\n");
    }
}
```

### 9.4 内存使用统计

```c
void memory_stats_example(void) {
    osal_mem_stats_t stats;
    
    /* 获取内存统计信息 */
    osal_mem_get_stats(&stats);
    
    printf("Memory Statistics:\n");
    printf("  Total heap size: %zu bytes\n", stats.total_size);
    printf("  Free heap size: %zu bytes\n", stats.free_size);
    printf("  Used heap size: %zu bytes\n", stats.used_size);
    printf("  Minimum free size: %zu bytes\n", stats.min_free_size);
    printf("  Allocation count: %u\n", stats.alloc_count);
    printf("  Free count: %u\n", stats.free_count);
}
```

### 9.5 内存泄漏检测

```c
void memory_leak_detection_example(void) {
    /* 记录初始状态 */
    osal_mem_stats_t stats_before;
    osal_mem_get_stats(&stats_before);
    
    /* 执行可能泄漏的操作 */
    void* ptr1 = osal_mem_alloc(100);
    void* ptr2 = osal_mem_alloc(200);
    osal_mem_free(ptr1);
    /* 忘记释放 ptr2 - 内存泄漏！ */
    
    /* 检查内存使用 */
    osal_mem_stats_t stats_after;
    osal_mem_get_stats(&stats_after);
    
    size_t leaked = stats_before.free_size - stats_after.free_size;
    if (leaked > 0) {
        printf("Memory leak detected: %zu bytes\n", leaked);
    }
    
    /* 修复泄漏 */
    osal_mem_free(ptr2);
}
```

## 10. 诊断和调试

### 10.1 任务统计

```c
void task_statistics_example(void) {
    osal_task_stats_t stats;
    osal_task_handle_t task = osal_task_get_current();
    
    /* 获取任务统计信息 */
    osal_status_t status = osal_task_get_stats(task, &stats);
    if (status == OSAL_OK) {
        printf("Task Statistics:\n");
        printf("  Name: %s\n", stats.name);
        printf("  Priority: %d\n", stats.priority);
        printf("  State: %d\n", stats.state);
        printf("  Stack size: %zu bytes\n", stats.stack_size);
        printf("  Stack watermark: %zu bytes\n", stats.stack_watermark);
        printf("  Runtime: %u ticks\n", stats.runtime);
        printf("  CPU usage: %u%%\n", stats.cpu_usage);
    }
}
```

### 10.2 系统统计

```c
void system_statistics_example(void) {
    osal_sys_stats_t stats;
    
    /* 获取系统统计信息 */
    osal_diag_get_sys_stats(&stats);
    
    printf("System Statistics:\n");
    printf("  Task count: %u\n", stats.task_count);
    printf("  Uptime: %u seconds\n", stats.uptime_seconds);
    printf("  Total context switches: %u\n", stats.context_switches);
    printf("  CPU usage: %u%%\n", stats.cpu_usage);
}
```

### 10.3 任务列表

```c
void task_list_example(void) {
    #define MAX_TASKS 16
    osal_task_info_t task_list[MAX_TASKS];
    size_t task_count;
    
    /* 获取任务列表 */
    osal_status_t status = osal_diag_get_task_list(task_list, MAX_TASKS, &task_count);
    if (status == OSAL_OK) {
        printf("Task List (%zu tasks):\n", task_count);
        printf("%-16s %-8s %-8s %-12s\n", "Name", "Priority", "State", "Stack Free");
        printf("--------------------------------------------------------\n");
        
        for (size_t i = 0; i < task_count; i++) {
            printf("%-16s %-8d %-8d %-12zu\n",
                   task_list[i].name,
                   task_list[i].priority,
                   task_list[i].state,
                   task_list[i].stack_watermark);
        }
    }
}
```

### 10.4 堆栈溢出检测

```c
void stack_overflow_check_example(void) {
    osal_task_handle_t task = osal_task_get_current();
    
    /* 获取堆栈水位 */
    size_t watermark = osal_task_get_stack_watermark(task);
    
    /* 检查是否接近溢出（剩余小于 10%）*/
    osal_task_stats_t stats;
    osal_task_get_stats(task, &stats);
    
    size_t threshold = stats.stack_size / 10;
    if (watermark < threshold) {
        printf("WARNING: Stack usage is high!\n");
        printf("  Stack size: %zu bytes\n", stats.stack_size);
        printf("  Free space: %zu bytes\n", watermark);
        printf("  Usage: %.1f%%\n", 
               (float)(stats.stack_size - watermark) * 100 / stats.stack_size);
    }
}
```

### 10.5 性能分析

```c
void performance_profiling_example(void) {
    /* 记录开始时间 */
    uint32_t start_time = osal_get_tick_count();
    
    /* 执行要分析的代码 */
    perform_complex_operation();
    
    /* 记录结束时间 */
    uint32_t end_time = osal_get_tick_count();
    
    /* 计算执行时间 */
    uint32_t elapsed_ticks = end_time - start_time;
    uint32_t elapsed_ms = osal_ticks_to_ms(elapsed_ticks);
    
    printf("Operation took %u ms (%u ticks)\n", elapsed_ms, elapsed_ticks);
}
```

### 10.6 断言和错误处理

```c
void assertion_example(void) {
    int value = get_sensor_value();
    
    /* 使用断言检查条件 */
    OSAL_ASSERT(value >= 0 && value <= 100);
    
    /* 使用断言并打印消息 */
    OSAL_ASSERT_MSG(value >= 0, "Sensor value is negative!");
}

void error_handling_example(void) {
    osal_status_t status = osal_task_create(&config, &task);
    
    /* 检查错误 */
    if (OSAL_IS_ERROR(status)) {
        printf("Error occurred: %d\n", status);
        
        /* 根据错误类型处理 */
        switch (status) {
            case OSAL_ERROR_NO_MEMORY:
                printf("Out of memory\n");
                break;
            case OSAL_ERROR_INVALID_PARAM:
                printf("Invalid parameter\n");
                break;
            default:
                printf("Unknown error\n");
                break;
        }
    }
}
```


## 11. 最佳实践

### 11.1 任务设计

#### 11.1.1 合理的任务优先级

```c
/* 推荐：根据任务重要性和实时性要求设置优先级 */

/* 高优先级：实时性要求高的任务 */
osal_task_config_t isr_handler_config = {
    .name = "isr_handler",
    .func = isr_handler_task,
    .priority = OSAL_TASK_PRIORITY_REALTIME,  /* 31 */
    .stack_size = 512,
};

/* 中等优先级：普通应用任务 */
osal_task_config_t app_config = {
    .name = "app_task",
    .func = app_task,
    .priority = OSAL_TASK_PRIORITY_NORMAL,  /* 16 */
    .stack_size = 1024,
};

/* 低优先级：后台任务 */
osal_task_config_t background_config = {
    .name = "background",
    .func = background_task,
    .priority = OSAL_TASK_PRIORITY_LOW,  /* 8 */
    .stack_size = 2048,
};

/* 避免：所有任务使用相同优先级 */
/* 这会导致任务调度不确定 */
```

#### 11.1.2 适当的堆栈大小

```c
/* 推荐：根据任务需求设置堆栈大小 */

/* 简单任务：512-1024 字节 */
config.stack_size = 512;

/* 中等复杂度任务：1024-2048 字节 */
config.stack_size = 1024;

/* 复杂任务或使用大量局部变量：2048-4096 字节 */
config.stack_size = 2048;

/* 避免：堆栈过小导致溢出 */
config.stack_size = 128;  /* 太小！ */

/* 避免：堆栈过大浪费内存 */
config.stack_size = 10240;  /* 太大！ */

/* 最佳实践：运行时检查堆栈使用情况 */
void check_stack_usage(void) {
    osal_task_handle_t task = osal_task_get_current();
    size_t watermark = osal_task_get_stack_watermark(task);
    
    if (watermark < 128) {
        printf("WARNING: Stack usage is very high!\n");
    }
}
```

#### 11.1.3 任务循环设计

```c
/* 推荐：任务应该包含无限循环 */
void good_task(void* arg) {
    /* 初始化 */
    init_resources();
    
    while (1) {
        /* 执行任务 */
        do_work();
        
        /* 让出 CPU 或等待事件 */
        osal_task_delay(100);
    }
    
    /* 清理（通常不会到达）*/
    cleanup_resources();
}

/* 避免：任务函数返回 */
void bad_task(void* arg) {
    do_work();
    return;  /* 错误！任务会被删除 */
}

/* 推荐：需要退出时显式删除 */
void exit_task(void* arg) {
    while (1) {
        if (should_exit()) {
            cleanup_resources();
            osal_task_delete(NULL);  /* 删除自己 */
        }
        
        do_work();
        osal_task_delay(100);
    }
}
```

### 11.2 同步机制选择

#### 11.2.1 互斥锁 vs 信号量

```c
/* 使用互斥锁：保护共享资源 */
static osal_mutex_handle_t g_resource_mutex;
static int g_shared_resource;

void access_shared_resource(void) {
    osal_mutex_lock(g_resource_mutex, OSAL_WAIT_FOREVER);
    
    /* 访问共享资源 */
    g_shared_resource++;
    
    osal_mutex_unlock(g_resource_mutex);
}

/* 使用信号量：任务同步和资源计数 */
static osal_sem_handle_t g_data_ready_sem;

void producer(void) {
    /* 生产数据 */
    produce_data();
    
    /* 通知消费者 */
    osal_sem_post(g_data_ready_sem);
}

void consumer(void) {
    /* 等待数据就绪 */
    osal_sem_wait(g_data_ready_sem, OSAL_WAIT_FOREVER);
    
    /* 消费数据 */
    consume_data();
}
```

#### 11.2.2 避免死锁

```c
/* 推荐：按固定顺序获取多个锁 */
static osal_mutex_handle_t g_mutex_a;
static osal_mutex_handle_t g_mutex_b;

void good_locking(void) {
    /* 总是按 A -> B 的顺序获取锁 */
    osal_mutex_lock(g_mutex_a, OSAL_WAIT_FOREVER);
    osal_mutex_lock(g_mutex_b, OSAL_WAIT_FOREVER);
    
    /* 访问资源 */
    
    /* 按相反顺序释放 */
    osal_mutex_unlock(g_mutex_b);
    osal_mutex_unlock(g_mutex_a);
}

/* 避免：不同顺序获取锁（可能死锁）*/
void bad_locking_task1(void) {
    osal_mutex_lock(g_mutex_a, OSAL_WAIT_FOREVER);
    osal_task_delay(10);  /* 模拟延时 */
    osal_mutex_lock(g_mutex_b, OSAL_WAIT_FOREVER);  /* 可能死锁 */
    /* ... */
}

void bad_locking_task2(void) {
    osal_mutex_lock(g_mutex_b, OSAL_WAIT_FOREVER);
    osal_task_delay(10);
    osal_mutex_lock(g_mutex_a, OSAL_WAIT_FOREVER);  /* 可能死锁 */
    /* ... */
}

/* 推荐：使用超时避免永久阻塞 */
void safe_locking(void) {
    osal_status_t status;
    
    status = osal_mutex_lock(g_mutex_a, 1000);
    if (status != OSAL_OK) {
        printf("Failed to acquire mutex A\n");
        return;
    }
    
    status = osal_mutex_lock(g_mutex_b, 1000);
    if (status != OSAL_OK) {
        printf("Failed to acquire mutex B\n");
        osal_mutex_unlock(g_mutex_a);
        return;
    }
    
    /* 访问资源 */
    
    osal_mutex_unlock(g_mutex_b);
    osal_mutex_unlock(g_mutex_a);
}
```

### 11.3 内存管理

#### 11.3.1 避免内存泄漏

```c
/* 推荐：总是释放分配的内存 */
void good_memory_usage(void) {
    void* buffer = osal_mem_alloc(1024);
    if (buffer == NULL) {
        return;
    }
    
    /* 使用内存 */
    process_data(buffer);
    
    /* 释放内存 */
    osal_mem_free(buffer);
}

/* 避免：忘记释放内存 */
void bad_memory_usage(void) {
    void* buffer = osal_mem_alloc(1024);
    if (buffer == NULL) {
        return;
    }
    
    process_data(buffer);
    /* 忘记释放 - 内存泄漏！ */
}

/* 推荐：使用内存池减少碎片 */
static osal_mem_pool_handle_t g_buffer_pool;

void init_buffer_pool(void) {
    osal_mem_pool_create(&g_buffer_pool, 1024, 10);
}

void use_buffer_pool(void) {
    void* buffer = osal_mem_pool_alloc(g_buffer_pool, 1000);
    if (buffer) {
        process_data(buffer);
        osal_mem_pool_free(g_buffer_pool, buffer);
    }
}
```

#### 11.3.2 静态分配 vs 动态分配

```c
/* 推荐：关键任务使用静态分配 */
static uint8_t critical_task_stack[2048];
static osal_task_handle_t critical_task;

void create_critical_task(void) {
    osal_task_config_t config = {
        .name = "critical",
        .func = critical_task_func,
        .priority = OSAL_TASK_PRIORITY_REALTIME,
        .stack_size = sizeof(critical_task_stack),
    };
    
    /* 使用静态堆栈创建任务 */
    osal_task_create_static(&config, &critical_task, critical_task_stack);
}

/* 动态分配：非关键任务 */
void create_normal_task(void) {
    osal_task_config_t config = {
        .name = "normal",
        .func = normal_task_func,
        .priority = OSAL_TASK_PRIORITY_NORMAL,
        .stack_size = 1024,
    };
    
    osal_task_handle_t task;
    osal_task_create(&config, &task);
}
```

### 11.4 错误处理

#### 11.4.1 检查返回值

```c
/* 推荐：总是检查返回值 */
void good_error_handling(void) {
    osal_mutex_handle_t mutex;
    osal_status_t status;
    
    status = osal_mutex_create(&mutex);
    if (status != OSAL_OK) {
        printf("Mutex creation failed: %d\n", status);
        return;
    }
    
    status = osal_mutex_lock(mutex, 1000);
    if (status == OSAL_OK) {
        /* 访问资源 */
        osal_mutex_unlock(mutex);
    } else if (status == OSAL_ERROR_TIMEOUT) {
        printf("Mutex lock timeout\n");
    }
    
    osal_mutex_delete(mutex);
}

/* 避免：忽略返回值 */
void bad_error_handling(void) {
    osal_mutex_handle_t mutex;
    osal_mutex_create(&mutex);  /* 未检查返回值 */
    osal_mutex_lock(mutex, 1000);  /* 未检查返回值 */
    /* ... */
}
```

#### 11.4.2 使用断言

```c
/* 推荐：使用断言检查前置条件 */
void process_data(const uint8_t* data, size_t size) {
    OSAL_ASSERT(data != NULL);
    OSAL_ASSERT(size > 0 && size <= MAX_SIZE);
    
    /* 处理数据 */
    for (size_t i = 0; i < size; i++) {
        process_byte(data[i]);
    }
}
```

### 11.5 性能优化

#### 11.5.1 减少上下文切换

```c
/* 推荐：批量处理减少切换 */
void efficient_task(void* arg) {
    while (1) {
        /* 批量处理多个消息 */
        for (int i = 0; i < 10; i++) {
            if (osal_queue_receive(queue, &msg, OSAL_NO_WAIT) == OSAL_OK) {
                process_message(&msg);
            } else {
                break;
            }
        }
        
        /* 延时让出 CPU */
        osal_task_delay(10);
    }
}

/* 避免：频繁切换 */
void inefficient_task(void* arg) {
    while (1) {
        /* 每次只处理一个消息就切换 */
        if (osal_queue_receive(queue, &msg, 0) == OSAL_OK) {
            process_message(&msg);
        }
        osal_task_yield();  /* 频繁切换 */
    }
}
```

#### 11.5.2 合理使用延时

```c
/* 推荐：使用适当的延时 */
void polling_task(void* arg) {
    while (1) {
        check_sensor();
        
        /* 根据需求设置延时 */
        osal_task_delay(100);  /* 100ms 轮询间隔 */
    }
}

/* 避免：过短的延时浪费 CPU */
void bad_polling_task(void* arg) {
    while (1) {
        check_sensor();
        osal_task_delay(1);  /* 1ms 太频繁 */
    }
}

/* 推荐：使用事件驱动代替轮询 */
void event_driven_task(void* arg) {
    while (1) {
        /* 等待事件，不浪费 CPU */
        uint32_t bits;
        osal_event_wait(event, EVENT_SENSOR_READY, false, true, 
                       &bits, OSAL_WAIT_FOREVER);
        
        /* 处理事件 */
        handle_sensor_data();
    }
}
```

## 12. 常见问题

### Q1: 任务创建失败？

**A**: 检查以下几点：
1. 堆栈大小是否足够
2. 系统内存是否充足
3. 任务优先级是否有效（0-31）
4. 任务函数指针是否有效

```c
/* 调试代码 */
osal_status_t status = osal_task_create(&config, &task);
if (status != OSAL_OK) {
    printf("Task creation failed: %d\n", status);
    
    /* 检查内存 */
    osal_mem_stats_t stats;
    osal_mem_get_stats(&stats);
    printf("Free memory: %zu bytes\n", stats.free_size);
}
```

### Q2: 如何调试死锁？

**A**: 使用以下方法：
1. 检查互斥锁获取顺序
2. 使用超时代替永久等待
3. 打印任务状态和锁的所有者

```c
/* 调试死锁 */
void debug_deadlock(void) {
    /* 打印所有任务状态 */
    osal_task_info_t task_list[16];
    size_t count;
    osal_diag_get_task_list(task_list, 16, &count);
    
    for (size_t i = 0; i < count; i++) {
        printf("Task: %s, State: %d\n", 
               task_list[i].name, task_list[i].state);
    }
    
    /* 检查互斥锁所有者 */
    if (osal_mutex_is_locked(mutex)) {
        osal_task_handle_t owner = osal_mutex_get_owner(mutex);
        printf("Mutex owner: %s\n", osal_task_get_name(owner));
    }
}
```

### Q3: 堆栈溢出如何处理？

**A**: 
1. 增加堆栈大小
2. 减少局部变量使用
3. 避免深度递归
4. 使用堆分配代替栈分配

```c
/* 检查堆栈使用 */
void check_stack(void) {
    osal_task_handle_t task = osal_task_get_current();
    size_t watermark = osal_task_get_stack_watermark(task);
    
    if (watermark < 256) {
        printf("WARNING: Stack almost full!\n");
        printf("Consider increasing stack size\n");
    }
}
```

### Q4: 如何选择合适的同步机制？

**A**: 
- **互斥锁**: 保护共享资源，防止竞态条件
- **信号量**: 任务同步、资源计数
- **事件标志**: 多条件同步、状态通知
- **消息队列**: 任务间数据传递

### Q5: OSAL 在裸机模式下的限制？

**A**: 裸机模式的限制：
- 不支持真正的多任务
- 互斥锁通过禁用中断实现
- 不支持阻塞操作
- 定时器基于轮询

```c
/* 裸机模式检查 */
#if defined(OSAL_BAREMETAL)
    #warning "Running in baremetal mode - limited functionality"
#endif
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**作者**: Nexus Team
