操作系统抽象层 (OSAL)
=====================

概述
----

OSAL 为 RTOS 原语提供可移植接口。应用可以在不同 RTOS 后端
（FreeRTOS、RT-Thread、Zephyr）之间切换，或运行裸机模式，无需修改代码。

支持的后端
----------

+------------+-------------+
| 后端       | 状态        |
+============+=============+
| 裸机       | ✅ 已支持   |
+------------+-------------+
| FreeRTOS   | 🚧 计划中   |
+------------+-------------+
| RT-Thread  | 🚧 计划中   |
+------------+-------------+
| Zephyr     | 🚧 计划中   |
+------------+-------------+

任务管理
--------

**创建任务：**

.. code-block:: c

    void my_task(void* arg)
    {
        while (1) {
            // 任务代码
            osal_task_delay(100);
        }
    }

    osal_task_config_t config = {
        .name       = "my_task",
        .func       = my_task,
        .arg        = NULL,
        .stack_size = 1024,
        .priority   = OSAL_PRIORITY_NORMAL
    };

    osal_task_handle_t handle;
    osal_task_create(&config, &handle);

互斥锁
------

**使用方法：**

.. code-block:: c

    osal_mutex_handle_t mutex;
    osal_mutex_create(&mutex);

    // 加锁
    osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);

    // 临界区
    // ...

    // 解锁
    osal_mutex_unlock(mutex);

信号量
------

**二值信号量：**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_binary(0, &sem);

    // 等待
    osal_sem_take(sem, OSAL_WAIT_FOREVER);

    // 释放
    osal_sem_give(sem);

**计数信号量：**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_counting(10, 0, &sem);  // 最大=10, 初始=0

消息队列
--------

**使用方法：**

.. code-block:: c

    typedef struct {
        uint32_t id;
        uint32_t data;
    } message_t;

    osal_queue_handle_t queue;
    osal_queue_create(sizeof(message_t), 10, &queue);

    // 发送
    message_t msg = { .id = 1, .data = 42 };
    osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);

    // 接收
    message_t received;
    osal_queue_receive(queue, &received, OSAL_WAIT_FOREVER);

临界区
------

.. code-block:: c

    osal_enter_critical();
    // 中断已禁用
    // ...
    osal_exit_critical();

API 参考
--------

完整 API 文档请参见 :doc:`../api/osal`。
