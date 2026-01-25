中断处理教程
==============

本教程教你如何使用 Nexus HAL 处理硬件中断。你将学习如何配置中断、编写中断处理程序以及实现事件驱动的应用程序。

学习目标
--------

完成本教程后，你将能够：

- 理解中断的工作原理和优势
- 配置 GPIO 外部中断
- 编写高效的中断处理程序
- 使用中断实现事件驱动编程
- 处理中断优先级和嵌套
- 避免常见的中断编程陷阱

前置条件
--------

- 完成 :doc:`first_application` 和 :doc:`gpio_control` 教程
- STM32F4 Discovery 开发板或兼容硬件
- 理解基本的中断概念

硬件设置
--------

本教程使用以下硬件：

**输入（按钮）：**

- PA0: 用户按钮（高电平有效）

**输出（LED）：**

- PA0: LED 0（绿色指示灯）
- PA1: LED 1（橙色指示灯）
- PA2: LED 2（红色指示灯）
- PB0: LED 3（蓝色指示灯）

STM32F4 Discovery 开发板无需额外接线。

第一部分：中断基础
------------------

什么是中断？
~~~~~~~~~~~~

中断是一种硬件机制，允许外部事件立即打断 CPU 的正常执行流程。与轮询相比，中断具有以下优势：

- **低延迟**：事件发生时立即响应
- **低功耗**：CPU 可以休眠等待事件
- **高效率**：不需要持续检查状态

中断工作流程
~~~~~~~~~~~~

以下流程图展示了中断处理的完整过程：

.. mermaid::
   :alt: 中断处理工作流程，展示从中断触发到处理完成的全过程

   flowchart TD
       START([程序正常运行]) --> EVENT[外部事件发生]
       EVENT --> TRIGGER[触发中断信号]
       TRIGGER --> SAVE[保存当前上下文]
       SAVE --> JUMP[跳转到中断处理程序]

       JUMP --> ISR_START[进入 ISR]
       ISR_START --> CLEAR[清除中断标志]
       CLEAR --> PROCESS[处理中断事件]
       PROCESS --> SET_FLAG[设置标志/信号量]
       SET_FLAG --> ISR_END[退出 ISR]

       ISR_END --> RESTORE[恢复上下文]
       RESTORE --> RESUME[恢复程序执行]
       RESUME --> MAIN_LOOP[主循环检查标志]
       MAIN_LOOP --> HANDLE[处理事件]
       HANDLE --> START

       style START fill:#e1f5ff
       style EVENT fill:#ffe1e1
       style ISR_START fill:#ffe1f5
       style PROCESS fill:#e1ffe1
       style MAIN_LOOP fill:#fff4e1

中断处理程序规则
~~~~~~~~~~~~~~~~

编写中断处理程序（ISR）时必须遵循以下规则：

1. **保持简短**：ISR 应该尽可能快地执行
2. **不要阻塞**：不要调用 ``osal_task_delay()`` 或其他阻塞函数
3. **最小化工作**：只做必要的工作，其余交给主循环
4. **使用 volatile**：中断和主代码共享的变量必须声明为 ``volatile``
5. **原子操作**：注意数据竞争和原子性问题

第二部分：GPIO 外部中断
-----------------------


基本按钮中断示例
~~~~~~~~~~~~~~~~

让我们从一个简单的按钮中断示例开始：

.. code-block:: c

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static volatile bool g_button_pressed = false;  /**< 按钮按下标志 */
    static nx_gpio_write_t* g_led0 = NULL;          /**< LED 设备 */

    /*-----------------------------------------------------------------------*/
    /* Interrupt Handler                                                     */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           GPIO 中断回调函数
     * \details         当按钮按下时由硬件触发
     * \param[in]       gpio: GPIO 设备指针
     * \param[in]       user_data: 用户数据（未使用）
     * \note            此函数在中断上下文中执行，必须保持简短
     */
    static void button_irq_handler(nx_gpio_t* gpio, void* user_data) {
        (void)gpio;
        (void)user_data;

        /* 设置标志通知主循环 */
        g_button_pressed = true;
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 OSAL 和 HAL */
        osal_init();
        nx_hal_init();

        /* 获取 LED 设备 */
        g_led0 = nx_factory_gpio_write('A', 0);
        if (!g_led0) {
            while (1) { /* 错误 */ }
        }

        /* 获取按钮 GPIO 设备（配置为中断输入） */
        nx_gpio_read_t* button = nx_factory_gpio_read('A', 0);
        if (!button) {
            while (1) { /* 错误 */ }
        }

        /* 注册中断回调 */
        nx_gpio_t* button_base = (nx_gpio_t*)button;
        if (button_base->register_irq_callback) {
            button_base->register_irq_callback(button_base,
                                              button_irq_handler,
                                              NULL);
        }

        /* 主循环 */
        while (1) {
            /* 检查按钮按下标志 */
            if (g_button_pressed) {
                g_button_pressed = false;  /* 清除标志 */

                /* 切换 LED 状态 */
                g_led0->toggle(g_led0);
            }

            /* 短暂延迟以降低 CPU 使用率 */
            osal_task_delay(10);
        }

        return 0;
    }

**关键点：**

- 中断处理程序只设置标志，不执行复杂操作
- 使用 ``volatile`` 修饰共享变量
- 主循环检查标志并执行实际工作
- 中断处理程序保持简短快速

消抖处理
~~~~~~~~

机械按钮会产生抖动，导致多次中断触发。以下是软件消抖的实现：

.. code-block:: c

    #define DEBOUNCE_DELAY_MS  50  /**< 消抖延迟时间 */

    static volatile uint32_t g_last_interrupt_time = 0;
    static volatile bool g_button_pressed = false;

    /**
     * \brief           带消抖的 GPIO 中断处理程序
     */
    static void button_irq_handler_debounced(nx_gpio_t* gpio, void* user_data) {
        (void)gpio;
        (void)user_data;

        uint32_t now = osal_get_tick();

        /* 检查是否超过消抖时间 */
        if ((now - g_last_interrupt_time) >= DEBOUNCE_DELAY_MS) {
            g_last_interrupt_time = now;
            g_button_pressed = true;
        }
    }

边沿检测
~~~~~~~~

检测按钮的按下和释放事件：

.. code-block:: c

    typedef enum {
        BUTTON_EVENT_NONE,
        BUTTON_EVENT_PRESSED,
        BUTTON_EVENT_RELEASED
    } button_event_t;

    static volatile button_event_t g_button_event = BUTTON_EVENT_NONE;
    static volatile bool g_button_state = false;

    /**
     * \brief           边沿检测中断处理程序
     * \details         检测上升沿（按下）和下降沿（释放）
     */
    static void button_edge_handler(nx_gpio_t* gpio, void* user_data) {
        (void)user_data;

        /* 读取当前按钮状态 */
        nx_gpio_read_t* button = (nx_gpio_read_t*)gpio;
        bool current_state = button->read(button);

        /* 检测边沿 */
        if (current_state && !g_button_state) {
            /* 上升沿 - 按钮按下 */
            g_button_event = BUTTON_EVENT_PRESSED;
        } else if (!current_state && g_button_state) {
            /* 下降沿 - 按钮释放 */
            g_button_event = BUTTON_EVENT_RELEASED;
        }

        g_button_state = current_state;
    }

    /* 主循环中的使用 */
    while (1) {
        if (g_button_event == BUTTON_EVENT_PRESSED) {
            g_button_event = BUTTON_EVENT_NONE;
            /* 处理按下事件 */
            g_led0->write(g_led0, 1);
        } else if (g_button_event == BUTTON_EVENT_RELEASED) {
            g_button_event = BUTTON_EVENT_NONE;
            /* 处理释放事件 */
            g_led0->write(g_led0, 0);
        }

        osal_task_delay(10);
    }

第三部分：与 OSAL 集成
----------------------

使用信号量同步
~~~~~~~~~~~~~~

使用 OSAL 信号量在中断和任务之间同步：

.. code-block:: c

    static osal_sem_handle_t g_button_sem = NULL;

    /**
     * \brief           中断处理程序 - 释放信号量
     */
    static void button_irq_sem_handler(nx_gpio_t* gpio, void* user_data) {
        (void)gpio;
        (void)user_data;

        /* 释放信号量通知任务 */
        osal_sem_give_from_isr(g_button_sem);
    }

    /**
     * \brief           按钮处理任务
     */
    static void button_task(void* arg) {
        (void)arg;

        while (1) {
            /* 等待按钮按下信号 */
            if (osal_sem_take(g_button_sem, OSAL_WAIT_FOREVER) == OSAL_OK) {
                /* 处理按钮事件 */
                g_led0->toggle(g_led0);
            }
        }
    }

    int main(void) {
        osal_init();
        nx_hal_init();

        /* 获取设备 */
        g_led0 = nx_factory_gpio_write('A', 0);
        nx_gpio_read_t* button = nx_factory_gpio_read('A', 0);

        /* 创建二值信号量 */
        osal_sem_create_binary(&g_button_sem);

        /* 注册中断回调 */
        nx_gpio_t* button_base = (nx_gpio_t*)button;
        button_base->register_irq_callback(button_base,
                                          button_irq_sem_handler,
                                          NULL);

        /* 创建按钮处理任务 */
        osal_task_config_t task_config = {
            .name = "Button",
            .func = button_task,
            .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_HIGH,
            .stack_size = 1024
        };
        osal_task_handle_t task_handle;
        osal_task_create(&task_config, &task_handle);

        /* 启动调度器 */
        osal_start();

        return 0;
    }

使用消息队列
~~~~~~~~~~~~

通过消息队列传递中断数据：

.. code-block:: c

    /**
     * \brief           中断事件消息结构
     */
    typedef struct {
        uint32_t timestamp;     /**< 事件时间戳 */
        uint8_t event_type;     /**< 事件类型 */
        uint8_t gpio_port;      /**< GPIO 端口 */
        uint8_t gpio_pin;       /**< GPIO 引脚 */
    } irq_event_t;

    static osal_queue_handle_t g_irq_queue = NULL;

    /**
     * \brief           中断处理程序 - 发送消息到队列
     */
    static void gpio_irq_queue_handler(nx_gpio_t* gpio, void* user_data) {
        (void)user_data;

        /* 创建事件消息 */
        irq_event_t event = {
            .timestamp = osal_get_tick(),
            .event_type = 1,  /* 按钮按下 */
            .gpio_port = 'A',
            .gpio_pin = 0
        };

        /* 发送到队列（从 ISR） */
        osal_queue_send_from_isr(g_irq_queue, &event);
    }

    /**
     * \brief           事件处理任务
     */
    static void event_task(void* arg) {
        (void)arg;
        irq_event_t event;

        while (1) {
            /* 接收事件 */
            if (osal_queue_receive(g_irq_queue, &event,
                                  OSAL_WAIT_FOREVER) == OSAL_OK) {
                /* 处理事件 */
                if (event.event_type == 1) {
                    g_led0->toggle(g_led0);
                }
            }
        }
    }

    int main(void) {
        osal_init();
        nx_hal_init();

        /* 获取设备 */
        g_led0 = nx_factory_gpio_write('A', 0);
        nx_gpio_read_t* button = nx_factory_gpio_read('A', 0);

        /* 创建消息队列 */
        osal_queue_create(sizeof(irq_event_t), 10, &g_irq_queue);

        /* 注册中断回调 */
        nx_gpio_t* button_base = (nx_gpio_t*)button;
        button_base->register_irq_callback(button_base,
                                          gpio_irq_queue_handler,
                                          NULL);

        /* 创建事件处理任务 */
        osal_task_config_t task_config = {
            .name = "Event",
            .func = event_task,
            .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_HIGH,
            .stack_size = 1024
        };
        osal_task_handle_t task_handle;
        osal_task_create(&task_config, &task_handle);

        /* 启动调度器 */
        osal_start();

        return 0;
    }


第四部分：中断优先级
--------------------

理解中断优先级
~~~~~~~~~~~~~~

中断优先级决定了当多个中断同时发生时的处理顺序：

- **高优先级中断** 可以打断低优先级中断的执行
- **相同优先级** 的中断按照触发顺序排队
- **优先级反转** ：低优先级任务持有高优先级任务需要的资源

优先级配置示例
~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           配置不同优先级的中断
     */
    static void configure_interrupt_priorities(void) {
        /* 高优先级中断 - 紧急事件（如安全关断） */
        nx_gpio_read_t* emergency_button = nx_factory_gpio_read('A', 1);
        /* 配置为最高优先级 */

        /* 中等优先级中断 - 用户输入 */
        nx_gpio_read_t* user_button = nx_factory_gpio_read('A', 0);
        /* 配置为中等优先级 */

        /* 低优先级中断 - 非关键事件 */
        nx_gpio_read_t* status_input = nx_factory_gpio_read('B', 0);
        /* 配置为低优先级 */
    }

临界区保护
~~~~~~~~~~

在主代码中访问中断共享数据时，需要使用临界区保护：

.. code-block:: c

    static volatile uint32_t g_counter = 0;

    /**
     * \brief           中断处理程序 - 增加计数器
     */
    static void timer_irq_handler(nx_gpio_t* gpio, void* user_data) {
        (void)gpio;
        (void)user_data;
        g_counter++;
    }

    /**
     * \brief           安全读取计数器
     */
    static uint32_t read_counter_safe(void) {
        uint32_t value;

        /* 进入临界区 */
        osal_enter_critical();
        value = g_counter;
        osal_exit_critical();

        return value;
    }

    /**
     * \brief           安全重置计数器
     */
    static void reset_counter_safe(void) {
        /* 进入临界区 */
        osal_enter_critical();
        g_counter = 0;
        osal_exit_critical();
    }

第五部分：完整示例
------------------

多按钮中断管理器
~~~~~~~~~~~~~~~~

以下是一个完整的多按钮中断管理示例：

.. code-block:: c

    /**
     * \file            interrupt_demo.c
     * \brief           中断处理完整示例
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define DEBOUNCE_DELAY_MS  50
    #define MAX_BUTTONS        4

    /*-----------------------------------------------------------------------*/
    /* Data Structures                                                       */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           按钮事件类型
     */
    typedef enum {
        BUTTON_EVENT_NONE,
        BUTTON_EVENT_PRESSED,
        BUTTON_EVENT_RELEASED,
        BUTTON_EVENT_LONG_PRESS
    } button_event_type_t;

    /**
     * \brief           按钮事件结构
     */
    typedef struct {
        uint32_t timestamp;
        uint8_t button_id;
        button_event_type_t event_type;
    } button_event_t;

    /**
     * \brief           按钮状态结构
     */
    typedef struct {
        uint8_t id;
        bool current_state;
        bool last_state;
        uint32_t press_time;
        uint32_t last_event_time;
    } button_state_t;

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static osal_queue_handle_t g_event_queue = NULL;
    static button_state_t g_buttons[MAX_BUTTONS];
    static nx_gpio_write_t* g_leds[MAX_BUTTONS];

    /*-----------------------------------------------------------------------*/
    /* Interrupt Handlers                                                    */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           通用按钮中断处理程序
     */
    static void button_irq_handler(nx_gpio_t* gpio, void* user_data) {
        uint8_t button_id = (uint8_t)(uintptr_t)user_data;
        uint32_t now = osal_get_tick();

        /* 检查消抖 */
        if ((now - g_buttons[button_id].last_event_time) < DEBOUNCE_DELAY_MS) {
            return;
        }

        /* 读取当前状态 */
        nx_gpio_read_t* button = (nx_gpio_read_t*)gpio;
        bool current_state = button->read(button);

        /* 检测边沿 */
        button_event_t event;
        event.timestamp = now;
        event.button_id = button_id;

        if (current_state && !g_buttons[button_id].last_state) {
            /* 按下事件 */
            event.event_type = BUTTON_EVENT_PRESSED;
            g_buttons[button_id].press_time = now;
            osal_queue_send_from_isr(g_event_queue, &event);
        } else if (!current_state && g_buttons[button_id].last_state) {
            /* 释放事件 */
            uint32_t press_duration = now - g_buttons[button_id].press_time;

            if (press_duration > 1000) {
                event.event_type = BUTTON_EVENT_LONG_PRESS;
            } else {
                event.event_type = BUTTON_EVENT_RELEASED;
            }
            osal_queue_send_from_isr(g_event_queue, &event);
        }

        g_buttons[button_id].last_state = current_state;
        g_buttons[button_id].last_event_time = now;
    }

    /*-----------------------------------------------------------------------*/
    /* Tasks                                                                 */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           事件处理任务
     */
    static void event_handler_task(void* arg) {
        (void)arg;
        button_event_t event;

        while (1) {
            /* 等待事件 */
            if (osal_queue_receive(g_event_queue, &event,
                                  OSAL_WAIT_FOREVER) == OSAL_OK) {

                /* 处理事件 */
                switch (event.event_type) {
                    case BUTTON_EVENT_PRESSED:
                        /* 按钮按下 - 点亮 LED */
                        if (g_leds[event.button_id]) {
                            g_leds[event.button_id]->write(
                                g_leds[event.button_id], 1);
                        }
                        break;

                    case BUTTON_EVENT_RELEASED:
                        /* 按钮释放 - 熄灭 LED */
                        if (g_leds[event.button_id]) {
                            g_leds[event.button_id]->write(
                                g_leds[event.button_id], 0);
                        }
                        break;

                    case BUTTON_EVENT_LONG_PRESS:
                        /* 长按 - 切换 LED */
                        if (g_leds[event.button_id]) {
                            g_leds[event.button_id]->toggle(
                                g_leds[event.button_id]);
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Initialization                                                        */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           初始化按钮和中断
     */
    static void init_buttons(void) {
        /* 按钮配置 */
        const char button_ports[] = {'A', 'A', 'A', 'B'};
        const uint8_t button_pins[] = {0, 1, 2, 0};

        for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
            /* 初始化按钮状态 */
            g_buttons[i].id = i;
            g_buttons[i].current_state = false;
            g_buttons[i].last_state = false;
            g_buttons[i].press_time = 0;
            g_buttons[i].last_event_time = 0;

            /* 获取按钮 GPIO 设备 */
            nx_gpio_read_t* button = nx_factory_gpio_read(
                button_ports[i], button_pins[i]);

            if (button) {
                /* 注册中断回调 */
                nx_gpio_t* button_base = (nx_gpio_t*)button;
                if (button_base->register_irq_callback) {
                    button_base->register_irq_callback(
                        button_base,
                        button_irq_handler,
                        (void*)(uintptr_t)i);
                }
            }
        }
    }

    /**
     * \brief           初始化 LED
     */
    static void init_leds(void) {
        g_leds[0] = nx_factory_gpio_write('A', 0);
        g_leds[1] = nx_factory_gpio_write('A', 1);
        g_leds[2] = nx_factory_gpio_write('A', 2);
        g_leds[3] = nx_factory_gpio_write('B', 0);
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 OSAL 和 HAL */
        osal_init();
        nx_hal_init();

        /* 初始化硬件 */
        init_leds();
        init_buttons();

        /* 创建事件队列 */
        osal_queue_create(sizeof(button_event_t), 20, &g_event_queue);

        /* 创建事件处理任务 */
        osal_task_config_t task_config = {
            .name = "EventHandler",
            .func = event_handler_task,
            .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_HIGH,
            .stack_size = 2048
        };
        osal_task_handle_t task_handle;
        osal_task_create(&task_config, &task_handle);

        /* 启动调度器 */
        osal_start();

        return 0;
    }

**工作原理：**

1. 按钮按下触发中断
2. 中断处理程序检测边沿并发送事件到队列
3. 事件处理任务接收事件并控制 LED
4. 支持短按、长按和释放事件
5. 自动消抖处理

最佳实践
--------

1. **保持 ISR 简短**：中断处理程序应该尽可能快

2. **使用标志或信号量**：在 ISR 和主代码之间通信

3. **正确使用 volatile**：中断共享的变量必须声明为 ``volatile``

4. **临界区保护**：访问共享数据时使用临界区

5. **消抖处理**：始终对机械按钮实现消抖

6. **优先级设置**：根据紧急程度设置中断优先级

7. **避免阻塞**：不要在 ISR 中调用阻塞函数

8. **错误处理**：检查中断注册的返回值

常见陷阱
--------

**忘记 volatile 关键字：**

.. code-block:: c

    /* 错误 - 编译器可能优化掉检查 */
    static bool flag = false;

    /* 正确 - 强制每次从内存读取 */
    static volatile bool flag = false;

**在 ISR 中调用阻塞函数：**

.. code-block:: c

    /* 错误 - 不要在 ISR 中延迟 */
    static void bad_isr(nx_gpio_t* gpio, void* user_data) {
        osal_task_delay(100);  /* 错误！ */
    }

    /* 正确 - 只设置标志 */
    static void good_isr(nx_gpio_t* gpio, void* user_data) {
        g_flag = true;  /* 正确 */
    }

**没有消抖：**

机械按钮会抖动，导致多次中断。始终实现消抖。

**数据竞争：**

.. code-block:: c

    /* 错误 - 没有保护 */
    g_counter++;  /* 主代码 */

    /* 正确 - 使用临界区 */
    osal_enter_critical();
    g_counter++;
    osal_exit_critical();

下一步
------

- :doc:`timer_pwm` - 学习定时器和 PWM
- :doc:`../user_guide/hal` - 探索更多 HAL 功能
- :doc:`task_creation` - 多任务编程
- :doc:`../platform_guides/stm32f4` - 平台特定的中断详情

