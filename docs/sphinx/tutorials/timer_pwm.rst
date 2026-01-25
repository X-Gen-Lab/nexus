定时器和 PWM 教程
==================

本教程教你如何使用 Nexus HAL 的定时器和 PWM（脉冲宽度调制）功能。你将学习如何配置定时器、生成 PWM 信号、控制电机和 LED 亮度。

学习目标
--------

完成本教程后，你将能够：

- 理解定时器的工作原理和应用场景
- 配置和使用硬件定时器
- 生成精确的 PWM 信号
- 控制 LED 亮度（调光）
- 控制伺服电机和直流电机
- 实现多通道 PWM 输出
- 测量输入信号的频率和占空比

前置条件
--------

- 完成 :doc:`first_application`、:doc:`gpio_control` 和 :doc:`interrupt_handling` 教程
- STM32F4 Discovery 开发板或兼容硬件
- 理解基本的定时器和 PWM 概念
- 可选：示波器用于观察波形

硬件设置
--------

本教程使用以下硬件：

**PWM 输出引脚（STM32F4）：**

- PA0: Timer 2 Channel 1（LED 0 / PWM 输出 1）
- PA1: Timer 2 Channel 2（LED 1 / PWM 输出 2）
- PA2: Timer 2 Channel 3（LED 2 / PWM 输出 3）
- PB0: Timer 3 Channel 3（LED 3 / PWM 输出 4）

**可选外部设备：**

- LED（用于观察 PWM 调光效果）
- 伺服电机（SG90 或类似型号）
- 直流电机（带 H 桥驱动器）

.. warning::
   连接外部设备时，请确保电压和电流在安全范围内。使用适当的驱动电路保护 MCU。

第一部分：定时器基础
--------------------

定时器工作原理
~~~~~~~~~~~~~~

硬件定时器是一个计数器，以固定频率递增或递减。当计数器达到特定值时，可以触发中断或改变输出状态。

定时器的关键概念：

- **时钟源**：定时器的输入时钟频率
- **预分频器（Prescaler）**：降低时钟频率的分频器
- **自动重载值（ARR）**：计数器的最大值
- **计数模式**：向上计数、向下计数或中心对齐
- **PWM 模式**：根据比较值生成 PWM 信号

定时器配置流程
~~~~~~~~~~~~~~

以下流程图展示了定时器配置和使用的完整过程：

.. mermaid::
   :alt: 定时器配置流程，展示从初始化到 PWM 输出的全过程

   flowchart TD
       START([开始]) --> INIT_HAL[初始化 HAL]
       INIT_HAL --> GET_TIMER[获取定时器设备]
       GET_TIMER --> CHECK{设备可用?}

       CHECK -->|否| ERROR[错误处理]
       ERROR --> END([结束])

       CHECK -->|是| CONFIG[配置定时器参数]
       CONFIG --> SET_FREQ[设置频率/周期]
       SET_FREQ --> CONFIG_PWM[配置 PWM 通道]

       CONFIG_PWM --> SET_DUTY[设置占空比]
       SET_DUTY --> START_PWM[启动 PWM 输出]

       START_PWM --> RUNNING[PWM 运行中]
       RUNNING --> ADJUST{需要调整?}

       ADJUST -->|是| UPDATE[更新占空比]
       UPDATE --> RUNNING

       ADJUST -->|否| STOP{停止?}
       STOP -->|否| RUNNING
       STOP -->|是| STOP_PWM[停止 PWM]
       STOP_PWM --> RELEASE[释放资源]
       RELEASE --> END

       style START fill:#e1f5ff
       style INIT_HAL fill:#fff4e1
       style CONFIG fill:#ffe1f5
       style RUNNING fill:#e1ffe1
       style ERROR fill:#ffcccc

基本定时器示例
~~~~~~~~~~~~~~

让我们从一个简单的定时器示例开始：

.. code-block:: c

    /**
     * \file            timer_basic.c
     * \brief           基本定时器示例
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         演示如何使用定时器生成周期性中断
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define TIMER_INDEX     0       /**< 定时器索引 */
    #define TIMER_FREQ_HZ   1000    /**< 定时器频率 1kHz */

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static volatile uint32_t g_tick_count = 0;  /**< 定时器滴答计数 */
    static nx_gpio_write_t* g_led0 = NULL;      /**< LED 设备 */

    /*-----------------------------------------------------------------------*/
    /* Timer Callback                                                        */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           定时器中断回调函数
     * \details         每次定时器溢出时调用
     * \param[in]       timer: 定时器设备指针
     * \param[in]       user_data: 用户数据
     * \note            在中断上下文中执行，保持简短
     */
    static void timer_callback(nx_timer_base_t* timer, void* user_data) {
        (void)timer;
        (void)user_data;

        /* 增加计数器 */
        g_tick_count++;

        /* 每秒切换一次 LED（1000 次滴答 = 1 秒） */
        if ((g_tick_count % 1000) == 0) {
            if (g_led0) {
                g_led0->toggle(g_led0);
            }
        }
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

        /* 获取定时器设备 */
        nx_timer_base_t* timer = nx_factory_timer(TIMER_INDEX);
        if (!timer) {
            while (1) { /* 错误 */ }
        }

        /* 注册定时器回调 */
        if (timer->register_callback) {
            timer->register_callback(timer, timer_callback, NULL);
        }

        /* 配置定时器频率 */
        if (timer->set_frequency) {
            timer->set_frequency(timer, TIMER_FREQ_HZ);
        }

        /* 启动定时器 */
        if (timer->start) {
            timer->start(timer);
        }

        /* 主循环 */
        while (1) {
            /* 可以在这里执行其他任务 */
            osal_task_delay(100);
        }

        return 0;
    }

**关键点：**

- 定时器以 1kHz 频率运行（每毫秒触发一次）
- 回调函数在中断上下文中执行
- LED 每秒切换一次（1000 次滴答）
- 主循环可以执行其他任务

第二部分：PWM 基础
------------------

什么是 PWM？
~~~~~~~~~~~~

PWM（脉冲宽度调制）是一种通过改变脉冲宽度来控制平均功率的技术。

PWM 的关键参数：

- **频率（Frequency）**：PWM 信号的重复频率（Hz）
- **周期（Period）**：一个完整周期的时间（秒）
- **占空比（Duty Cycle）**：高电平时间占周期的百分比（0-100%）
- **分辨率（Resolution）**：占空比的最小调整步长

PWM 应用场景：

- **LED 调光**：通过改变占空比控制亮度
- **电机控制**：控制电机速度和方向
- **伺服控制**：通过脉冲宽度控制伺服角度
- **音频生成**：生成简单的音调
- **电源转换**：DC-DC 转换器

PWM 波形示意图
~~~~~~~~~~~~~~

.. code-block:: text

    占空比 25%:
    ___     ___     ___     ___
       |___|   |___|   |___|   |___

    占空比 50%:
    _____   _____   _____   _____
         |_|     |_|     |_|     |_|

    占空比 75%:
    _______   _______   _______
           |_|       |_|       |_|

基本 PWM 示例
~~~~~~~~~~~~~

让我们创建一个简单的 PWM 输出：

.. code-block:: c

    /**
     * \file            pwm_basic.c
     * \brief           基本 PWM 示例
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         演示如何生成 PWM 信号控制 LED 亮度
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define PWM_TIMER_INDEX  0          /**< PWM 定时器索引 */
    #define PWM_CHANNEL      0          /**< PWM 通道 */
    #define PWM_FREQ_HZ      1000       /**< PWM 频率 1kHz */
    #define PWM_DUTY_PERCENT 50         /**< 初始占空比 50% */

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 OSAL 和 HAL */
        osal_init();
        nx_hal_init();

        /* 获取 PWM 定时器设备 */
        nx_timer_pwm_t* pwm = nx_factory_timer_pwm(PWM_TIMER_INDEX);
        if (!pwm) {
            while (1) { /* 错误 */ }
        }

        /* 配置 PWM 频率 */
        if (pwm->set_frequency) {
            pwm->set_frequency(pwm, PWM_FREQ_HZ);
        }

        /* 配置 PWM 通道 */
        if (pwm->configure_channel) {
            pwm->configure_channel(pwm, PWM_CHANNEL);
        }

        /* 设置占空比 */
        if (pwm->set_duty_cycle) {
            pwm->set_duty_cycle(pwm, PWM_CHANNEL, PWM_DUTY_PERCENT);
        }

        /* 启动 PWM 输出 */
        if (pwm->start_channel) {
            pwm->start_channel(pwm, PWM_CHANNEL);
        }

        /* 主循环 */
        while (1) {
            osal_task_delay(1000);
        }

        return 0;
    }

**关键点：**

- PWM 频率设置为 1kHz（适合 LED 控制）
- 占空比设置为 50%（LED 半亮）
- PWM 信号持续输出，无需 CPU 干预


第三部分：LED 调光
------------------

呼吸灯效果
~~~~~~~~~~

创建平滑的呼吸灯效果：

.. code-block:: c

    /**
     * \file            pwm_breathing.c
     * \brief           PWM 呼吸灯效果
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         使用 PWM 实现平滑的呼吸灯效果
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define PWM_TIMER_INDEX     0       /**< PWM 定时器索引 */
    #define PWM_CHANNEL         0       /**< PWM 通道 */
    #define PWM_FREQ_HZ         1000    /**< PWM 频率 */
    #define BREATH_STEP         1       /**< 呼吸步长 */
    #define BREATH_DELAY_MS     20      /**< 呼吸延迟 */

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 */
        osal_init();
        nx_hal_init();

        /* 获取 PWM 设备 */
        nx_timer_pwm_t* pwm = nx_factory_timer_pwm(PWM_TIMER_INDEX);
        if (!pwm) {
            while (1) { /* 错误 */ }
        }

        /* 配置 PWM */
        pwm->set_frequency(pwm, PWM_FREQ_HZ);
        pwm->configure_channel(pwm, PWM_CHANNEL);
        pwm->start_channel(pwm, PWM_CHANNEL);

        /* 呼吸灯主循环 */
        uint8_t duty = 0;
        bool increasing = true;

        while (1) {
            /* 设置当前占空比 */
            pwm->set_duty_cycle(pwm, PWM_CHANNEL, duty);

            /* 更新占空比 */
            if (increasing) {
                duty += BREATH_STEP;
                if (duty >= 100) {
                    duty = 100;
                    increasing = false;
                }
            } else {
                if (duty <= BREATH_STEP) {
                    duty = 0;
                    increasing = true;
                } else {
                    duty -= BREATH_STEP;
                }
            }

            /* 延迟 */
            osal_task_delay(BREATH_DELAY_MS);
        }

        return 0;
    }

**效果说明：**

- LED 从暗到亮平滑过渡（2 秒）
- 然后从亮到暗平滑过渡（2 秒）
- 循环往复，形成呼吸效果

多通道 PWM 控制
~~~~~~~~~~~~~~~

同时控制多个 LED 的亮度：

.. code-block:: c

    /**
     * \file            pwm_multi_channel.c
     * \brief           多通道 PWM 控制
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         使用多通道 PWM 创建彩色灯光效果
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"
    #include <math.h>

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define PWM_TIMER_INDEX  0          /**< PWM 定时器索引 */
    #define PWM_FREQ_HZ      1000       /**< PWM 频率 */
    #define NUM_CHANNELS     4          /**< PWM 通道数量 */

    /*-----------------------------------------------------------------------*/
    /* Data Structures                                                       */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           LED 通道配置
     */
    typedef struct {
        uint8_t channel;        /**< PWM 通道号 */
        uint8_t duty;           /**< 当前占空比 */
        uint8_t target_duty;    /**< 目标占空比 */
        int8_t step;            /**< 变化步长 */
    } led_channel_t;

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static nx_timer_pwm_t* g_pwm = NULL;
    static led_channel_t g_leds[NUM_CHANNELS];

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           初始化 LED 通道
     */
    static void init_led_channels(void) {
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            g_leds[i].channel = i;
            g_leds[i].duty = 0;
            g_leds[i].target_duty = 0;
            g_leds[i].step = 1;

            /* 配置并启动通道 */
            g_pwm->configure_channel(g_pwm, i);
            g_pwm->set_duty_cycle(g_pwm, i, 0);
            g_pwm->start_channel(g_pwm, i);
        }
    }

    /**
     * \brief           更新 LED 亮度
     */
    static void update_leds(void) {
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            led_channel_t* led = &g_leds[i];

            /* 检查是否需要更新 */
            if (led->duty != led->target_duty) {
                /* 向目标值移动 */
                if (led->duty < led->target_duty) {
                    led->duty += led->step;
                    if (led->duty > led->target_duty) {
                        led->duty = led->target_duty;
                    }
                } else {
                    if (led->duty < led->step) {
                        led->duty = 0;
                    } else {
                        led->duty -= led->step;
                    }
                    if (led->duty < led->target_duty) {
                        led->duty = led->target_duty;
                    }
                }

                /* 更新 PWM 输出 */
                g_pwm->set_duty_cycle(g_pwm, led->channel, led->duty);
            }
        }
    }

    /**
     * \brief           设置 LED 目标亮度
     */
    static void set_led_brightness(uint8_t channel, uint8_t brightness) {
        if (channel < NUM_CHANNELS) {
            g_leds[channel].target_duty = brightness;
        }
    }

    /**
     * \brief           流水灯效果
     */
    static void pattern_chase(void) {
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            /* 点亮当前 LED */
            set_led_brightness(i, 100);
            osal_task_delay(200);

            /* 熄灭当前 LED */
            set_led_brightness(i, 0);
        }
    }

    /**
     * \brief           渐变效果
     */
    static void pattern_fade_all(void) {
        /* 全部渐亮 */
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            set_led_brightness(i, 100);
        }
        osal_task_delay(2000);

        /* 全部渐暗 */
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            set_led_brightness(i, 0);
        }
        osal_task_delay(2000);
    }

    /**
     * \brief           波浪效果
     */
    static void pattern_wave(void) {
        static uint32_t phase = 0;

        for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            /* 使用正弦波计算亮度 */
            float angle = (phase + i * 90) * 3.14159f / 180.0f;
            uint8_t brightness = (uint8_t)((sin(angle) + 1.0f) * 50.0f);
            set_led_brightness(i, brightness);
        }

        phase = (phase + 10) % 360;
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 */
        osal_init();
        nx_hal_init();

        /* 获取 PWM 设备 */
        g_pwm = nx_factory_timer_pwm(PWM_TIMER_INDEX);
        if (!g_pwm) {
            while (1) { /* 错误 */ }
        }

        /* 配置 PWM 频率 */
        g_pwm->set_frequency(g_pwm, PWM_FREQ_HZ);

        /* 初始化 LED 通道 */
        init_led_channels();

        /* 主循环 */
        uint8_t pattern = 0;
        uint32_t pattern_counter = 0;

        while (1) {
            /* 更新 LED 状态 */
            update_leds();

            /* 每 5 秒切换一次模式 */
            if ((pattern_counter % 250) == 0) {
                pattern = (pattern + 1) % 3;
            }

            /* 执行当前模式 */
            switch (pattern) {
                case 0:
                    pattern_chase();
                    break;
                case 1:
                    pattern_fade_all();
                    break;
                case 2:
                    pattern_wave();
                    break;
            }

            pattern_counter++;
            osal_task_delay(20);
        }

        return 0;
    }

**效果说明：**

- **流水灯**：LED 依次点亮和熄灭
- **渐变**：所有 LED 同时渐亮和渐暗
- **波浪**：LED 形成波浪式的亮度变化
- 每 5 秒自动切换效果

第四部分：伺服电机控制
----------------------

伺服电机原理
~~~~~~~~~~~~

标准伺服电机（如 SG90）使用 PWM 信号控制角度：

- **频率**：50Hz（20ms 周期）
- **脉冲宽度**：
  - 1.0ms = 0°（最小角度）
  - 1.5ms = 90°（中间位置）
  - 2.0ms = 180°（最大角度）

伺服控制示例
~~~~~~~~~~~~

.. code-block:: c

    /**
     * \file            servo_control.c
     * \brief           伺服电机控制示例
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         使用 PWM 控制标准伺服电机（SG90）
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define SERVO_TIMER_INDEX   0       /**< 伺服定时器索引 */
    #define SERVO_CHANNEL       0       /**< 伺服 PWM 通道 */
    #define SERVO_FREQ_HZ       50      /**< 伺服频率 50Hz */

    /* 伺服脉冲宽度（微秒） */
    #define SERVO_PULSE_MIN_US  1000    /**< 最小脉冲宽度 (0°) */
    #define SERVO_PULSE_MID_US  1500    /**< 中间脉冲宽度 (90°) */
    #define SERVO_PULSE_MAX_US  2000    /**< 最大脉冲宽度 (180°) */
    #define SERVO_PERIOD_US     20000   /**< 周期 (20ms) */

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           将角度转换为占空比
     * \param[in]       angle: 角度 (0-180)
     * \return          占空比百分比 (0-100)
     */
    static uint8_t angle_to_duty_cycle(uint8_t angle) {
        /* 限制角度范围 */
        if (angle > 180) {
            angle = 180;
        }

        /* 计算脉冲宽度 */
        uint32_t pulse_us = SERVO_PULSE_MIN_US +
                           ((uint32_t)angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / 180;

        /* 转换为占空比百分比 */
        uint8_t duty = (uint8_t)((pulse_us * 100) / SERVO_PERIOD_US);

        return duty;
    }

    /**
     * \brief           设置伺服角度
     * \param[in]       pwm: PWM 设备指针
     * \param[in]       channel: PWM 通道
     * \param[in]       angle: 目标角度 (0-180)
     */
    static void servo_set_angle(nx_timer_pwm_t* pwm, uint8_t channel,
                               uint8_t angle) {
        uint8_t duty = angle_to_duty_cycle(angle);
        pwm->set_duty_cycle(pwm, channel, duty);
    }

    /**
     * \brief           平滑移动伺服到目标角度
     * \param[in]       pwm: PWM 设备指针
     * \param[in]       channel: PWM 通道
     * \param[in]       current: 当前角度
     * \param[in]       target: 目标角度
     * \param[in]       speed: 移动速度（度/步）
     */
    static void servo_move_smooth(nx_timer_pwm_t* pwm, uint8_t channel,
                                 uint8_t current, uint8_t target,
                                 uint8_t speed) {
        if (current < target) {
            /* 向目标角度移动 */
            for (uint8_t angle = current; angle <= target; angle += speed) {
                if (angle > target) {
                    angle = target;
                }
                servo_set_angle(pwm, channel, angle);
                osal_task_delay(20);
            }
        } else {
            /* 向目标角度移动 */
            for (uint8_t angle = current; angle >= target; angle -= speed) {
                if (angle < target) {
                    angle = target;
                }
                servo_set_angle(pwm, channel, angle);
                osal_task_delay(20);

                if (angle == 0) break;  /* 防止下溢 */
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 */
        osal_init();
        nx_hal_init();

        /* 获取 PWM 设备 */
        nx_timer_pwm_t* pwm = nx_factory_timer_pwm(SERVO_TIMER_INDEX);
        if (!pwm) {
            while (1) { /* 错误 */ }
        }

        /* 配置 PWM */
        pwm->set_frequency(pwm, SERVO_FREQ_HZ);
        pwm->configure_channel(pwm, SERVO_CHANNEL);
        pwm->start_channel(pwm, SERVO_CHANNEL);

        /* 初始化到中间位置 */
        servo_set_angle(pwm, SERVO_CHANNEL, 90);
        osal_task_delay(1000);

        /* 主循环 - 伺服扫描 */
        uint8_t current_angle = 90;

        while (1) {
            /* 移动到 0° */
            servo_move_smooth(pwm, SERVO_CHANNEL, current_angle, 0, 2);
            current_angle = 0;
            osal_task_delay(500);

            /* 移动到 180° */
            servo_move_smooth(pwm, SERVO_CHANNEL, current_angle, 180, 2);
            current_angle = 180;
            osal_task_delay(500);

            /* 移动到 90° */
            servo_move_smooth(pwm, SERVO_CHANNEL, current_angle, 90, 2);
            current_angle = 90;
            osal_task_delay(500);
        }

        return 0;
    }

**关键点：**

- 伺服频率必须是 50Hz
- 脉冲宽度决定角度位置
- 平滑移动避免伺服抖动
- 延迟给伺服足够的响应时间


第五部分：直流电机控制
----------------------

直流电机驱动原理
~~~~~~~~~~~~~~~~

直流电机通常需要 H 桥驱动器（如 L298N、TB6612）来控制：

- **速度控制**：通过 PWM 占空比控制速度
- **方向控制**：通过 GPIO 控制 H 桥方向引脚
- **制动**：同时拉低或拉高两个方向引脚

H 桥连接示意：

.. code-block:: text

    MCU          H-Bridge (L298N)      Motor
    ----         ----------------      -----
    PWM  ------> ENA (Enable)
    GPIO ------> IN1 (Direction)  --> Motor+
    GPIO ------> IN2 (Direction)  --> Motor-

直流电机控制示例
~~~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \file            dc_motor_control.c
     * \brief           直流电机控制示例
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         使用 PWM 和 GPIO 控制直流电机速度和方向
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define MOTOR_PWM_TIMER     0       /**< 电机 PWM 定时器 */
    #define MOTOR_PWM_CHANNEL   0       /**< 电机 PWM 通道 */
    #define MOTOR_PWM_FREQ      20000   /**< PWM 频率 20kHz */

    /*-----------------------------------------------------------------------*/
    /* Data Structures                                                       */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           电机方向枚举
     */
    typedef enum {
        MOTOR_DIR_STOP,         /**< 停止 */
        MOTOR_DIR_FORWARD,      /**< 正转 */
        MOTOR_DIR_BACKWARD,     /**< 反转 */
        MOTOR_DIR_BRAKE         /**< 制动 */
    } motor_direction_t;

    /**
     * \brief           电机控制结构
     */
    typedef struct {
        nx_timer_pwm_t* pwm;            /**< PWM 设备 */
        uint8_t pwm_channel;            /**< PWM 通道 */
        nx_gpio_write_t* dir_pin1;      /**< 方向引脚 1 */
        nx_gpio_write_t* dir_pin2;      /**< 方向引脚 2 */
        motor_direction_t direction;    /**< 当前方向 */
        uint8_t speed;                  /**< 当前速度 (0-100) */
    } motor_t;

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           初始化电机
     * \param[out]      motor: 电机结构指针
     * \param[in]       pwm_index: PWM 定时器索引
     * \param[in]       pwm_channel: PWM 通道
     * \param[in]       dir1_port: 方向引脚 1 端口
     * \param[in]       dir1_pin: 方向引脚 1 引脚号
     * \param[in]       dir2_port: 方向引脚 2 端口
     * \param[in]       dir2_pin: 方向引脚 2 引脚号
     * \return          true 成功，false 失败
     */
    static bool motor_init(motor_t* motor, uint8_t pwm_index,
                          uint8_t pwm_channel,
                          char dir1_port, uint8_t dir1_pin,
                          char dir2_port, uint8_t dir2_pin) {
        /* 获取 PWM 设备 */
        motor->pwm = nx_factory_timer_pwm(pwm_index);
        if (!motor->pwm) {
            return false;
        }

        motor->pwm_channel = pwm_channel;

        /* 配置 PWM */
        motor->pwm->set_frequency(motor->pwm, MOTOR_PWM_FREQ);
        motor->pwm->configure_channel(motor->pwm, pwm_channel);
        motor->pwm->set_duty_cycle(motor->pwm, pwm_channel, 0);
        motor->pwm->start_channel(motor->pwm, pwm_channel);

        /* 获取方向控制引脚 */
        motor->dir_pin1 = nx_factory_gpio_write(dir1_port, dir1_pin);
        motor->dir_pin2 = nx_factory_gpio_write(dir2_port, dir2_pin);

        if (!motor->dir_pin1 || !motor->dir_pin2) {
            return false;
        }

        /* 初始化状态 */
        motor->direction = MOTOR_DIR_STOP;
        motor->speed = 0;

        /* 停止电机 */
        motor->dir_pin1->write(motor->dir_pin1, 0);
        motor->dir_pin2->write(motor->dir_pin2, 0);

        return true;
    }

    /**
     * \brief           设置电机方向
     * \param[in]       motor: 电机结构指针
     * \param[in]       direction: 目标方向
     */
    static void motor_set_direction(motor_t* motor,
                                   motor_direction_t direction) {
        motor->direction = direction;

        switch (direction) {
            case MOTOR_DIR_STOP:
                /* 停止 - 两个引脚都为低 */
                motor->dir_pin1->write(motor->dir_pin1, 0);
                motor->dir_pin2->write(motor->dir_pin2, 0);
                motor->pwm->set_duty_cycle(motor->pwm, motor->pwm_channel, 0);
                break;

            case MOTOR_DIR_FORWARD:
                /* 正转 - IN1=1, IN2=0 */
                motor->dir_pin1->write(motor->dir_pin1, 1);
                motor->dir_pin2->write(motor->dir_pin2, 0);
                break;

            case MOTOR_DIR_BACKWARD:
                /* 反转 - IN1=0, IN2=1 */
                motor->dir_pin1->write(motor->dir_pin1, 0);
                motor->dir_pin2->write(motor->dir_pin2, 1);
                break;

            case MOTOR_DIR_BRAKE:
                /* 制动 - 两个引脚都为高 */
                motor->dir_pin1->write(motor->dir_pin1, 1);
                motor->dir_pin2->write(motor->dir_pin2, 1);
                motor->pwm->set_duty_cycle(motor->pwm, motor->pwm_channel, 0);
                break;
        }
    }

    /**
     * \brief           设置电机速度
     * \param[in]       motor: 电机结构指针
     * \param[in]       speed: 速度 (0-100)
     */
    static void motor_set_speed(motor_t* motor, uint8_t speed) {
        /* 限制速度范围 */
        if (speed > 100) {
            speed = 100;
        }

        motor->speed = speed;

        /* 更新 PWM 占空比 */
        if (motor->direction != MOTOR_DIR_STOP &&
            motor->direction != MOTOR_DIR_BRAKE) {
            motor->pwm->set_duty_cycle(motor->pwm, motor->pwm_channel, speed);
        }
    }

    /**
     * \brief           平滑加速到目标速度
     * \param[in]       motor: 电机结构指针
     * \param[in]       target_speed: 目标速度 (0-100)
     * \param[in]       accel_step: 加速步长
     */
    static void motor_accelerate(motor_t* motor, uint8_t target_speed,
                                uint8_t accel_step) {
        if (motor->speed < target_speed) {
            /* 加速 */
            for (uint8_t speed = motor->speed; speed <= target_speed;
                 speed += accel_step) {
                if (speed > target_speed) {
                    speed = target_speed;
                }
                motor_set_speed(motor, speed);
                osal_task_delay(50);
            }
        } else {
            /* 减速 */
            for (uint8_t speed = motor->speed; speed >= target_speed;
                 speed -= accel_step) {
                if (speed < target_speed) {
                    speed = target_speed;
                }
                motor_set_speed(motor, speed);
                osal_task_delay(50);

                if (speed == 0) break;  /* 防止下溢 */
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 */
        osal_init();
        nx_hal_init();

        /* 初始化电机 */
        motor_t motor;
        if (!motor_init(&motor, 0, 0, 'A', 1, 'A', 2)) {
            while (1) { /* 错误 */ }
        }

        /* 主循环 - 电机测试序列 */
        while (1) {
            /* 正转加速 */
            motor_set_direction(&motor, MOTOR_DIR_FORWARD);
            motor_accelerate(&motor, 100, 5);
            osal_task_delay(2000);

            /* 正转减速 */
            motor_accelerate(&motor, 0, 5);
            osal_task_delay(500);

            /* 反转加速 */
            motor_set_direction(&motor, MOTOR_DIR_BACKWARD);
            motor_accelerate(&motor, 100, 5);
            osal_task_delay(2000);

            /* 反转减速 */
            motor_accelerate(&motor, 0, 5);
            osal_task_delay(500);

            /* 制动测试 */
            motor_set_direction(&motor, MOTOR_DIR_FORWARD);
            motor_set_speed(&motor, 80);
            osal_task_delay(1000);
            motor_set_direction(&motor, MOTOR_DIR_BRAKE);
            osal_task_delay(1000);

            /* 停止 */
            motor_set_direction(&motor, MOTOR_DIR_STOP);
            osal_task_delay(2000);
        }

        return 0;
    }

**关键点：**

- PWM 频率设置为 20kHz（超声波频率，电机运行更安静）
- 使用平滑加速避免电流冲击
- 制动模式可以快速停止电机
- 方向切换前应先停止电机

第六部分：完整示例
------------------

多功能 PWM 控制器
~~~~~~~~~~~~~~~~~

以下是一个完整的多功能 PWM 控制示例：

.. code-block:: c

    /**
     * \file            pwm_controller.c
     * \brief           多功能 PWM 控制器
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         集成 LED 调光、伺服控制和电机控制的完整示例
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"
    #include "framework/shell/shell.h"
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define LED_PWM_TIMER       0       /**< LED PWM 定时器 */
    #define LED_PWM_CHANNEL     0       /**< LED PWM 通道 */
    #define LED_PWM_FREQ        1000    /**< LED PWM 频率 */

    #define SERVO_PWM_TIMER     1       /**< 伺服 PWM 定时器 */
    #define SERVO_PWM_CHANNEL   0       /**< 伺服 PWM 通道 */
    #define SERVO_PWM_FREQ      50      /**< 伺服 PWM 频率 */

    #define MOTOR_PWM_TIMER     2       /**< 电机 PWM 定时器 */
    #define MOTOR_PWM_CHANNEL   0       /**< 电机 PWM 通道 */
    #define MOTOR_PWM_FREQ      20000   /**< 电机 PWM 频率 */

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static nx_timer_pwm_t* g_led_pwm = NULL;
    static nx_timer_pwm_t* g_servo_pwm = NULL;
    static nx_timer_pwm_t* g_motor_pwm = NULL;
    static nx_uart_t* g_uart = NULL;

    /*-----------------------------------------------------------------------*/
    /* UART Helper Functions                                                 */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           打印字符串到 UART
     */
    static void uart_print(const char* str) {
        if (g_uart) {
            nx_tx_sync_t* tx = g_uart->get_tx_sync(g_uart);
            if (tx) {
                tx->send(tx, (const uint8_t*)str, strlen(str), 1000);
            }
        }
    }

    /**
     * \brief           打印格式化字符串到 UART
     */
    static void uart_printf(const char* fmt, ...) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        uart_print(buf);
    }

    /*-----------------------------------------------------------------------*/
    /* Command Handlers                                                      */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           LED 控制命令
     * \details         用法: led <brightness>
     */
    static int cmd_led(int argc, char* argv[]) {
        if (argc < 2) {
            uart_print("Usage: led <brightness 0-100>\r\n");
            return 1;
        }

        uint8_t brightness = (uint8_t)atoi(argv[1]);
        if (brightness > 100) {
            brightness = 100;
        }

        g_led_pwm->set_duty_cycle(g_led_pwm, LED_PWM_CHANNEL, brightness);
        uart_printf("LED brightness set to %d%%\r\n", brightness);

        return 0;
    }

    /**
     * \brief           伺服控制命令
     * \details         用法: servo <angle>
     */
    static int cmd_servo(int argc, char* argv[]) {
        if (argc < 2) {
            uart_print("Usage: servo <angle 0-180>\r\n");
            return 1;
        }

        uint8_t angle = (uint8_t)atoi(argv[1]);
        if (angle > 180) {
            angle = 180;
        }

        /* 计算占空比 */
        uint32_t pulse_us = 1000 + ((uint32_t)angle * 1000) / 180;
        uint8_t duty = (uint8_t)((pulse_us * 100) / 20000);

        g_servo_pwm->set_duty_cycle(g_servo_pwm, SERVO_PWM_CHANNEL, duty);
        uart_printf("Servo angle set to %d degrees\r\n", angle);

        return 0;
    }

    /**
     * \brief           电机控制命令
     * \details         用法: motor <speed>
     */
    static int cmd_motor(int argc, char* argv[]) {
        if (argc < 2) {
            uart_print("Usage: motor <speed 0-100>\r\n");
            return 1;
        }

        uint8_t speed = (uint8_t)atoi(argv[1]);
        if (speed > 100) {
            speed = 100;
        }

        g_motor_pwm->set_duty_cycle(g_motor_pwm, MOTOR_PWM_CHANNEL, speed);
        uart_printf("Motor speed set to %d%%\r\n", speed);

        return 0;
    }

    /**
     * \brief           PWM 信息命令
     */
    static int cmd_pwm_info(int argc, char* argv[]) {
        (void)argc;
        (void)argv;

        uart_print("\r\n=== PWM Controller Status ===\r\n");
        uart_printf("LED PWM:   Timer %d, Channel %d, Freq %d Hz\r\n",
                   LED_PWM_TIMER, LED_PWM_CHANNEL, LED_PWM_FREQ);
        uart_printf("Servo PWM: Timer %d, Channel %d, Freq %d Hz\r\n",
                   SERVO_PWM_TIMER, SERVO_PWM_CHANNEL, SERVO_PWM_FREQ);
        uart_printf("Motor PWM: Timer %d, Channel %d, Freq %d Hz\r\n",
                   MOTOR_PWM_TIMER, MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ);
        uart_print("=============================\r\n");

        return 0;
    }

    /*-----------------------------------------------------------------------*/
    /* Command Definitions                                                   */
    /*-----------------------------------------------------------------------*/

    static const shell_command_t cmd_led_def = {
        .name = "led",
        .handler = cmd_led,
        .help = "Control LED brightness",
        .usage = "led <brightness 0-100>",
        .completion = NULL
    };

    static const shell_command_t cmd_servo_def = {
        .name = "servo",
        .handler = cmd_servo,
        .help = "Control servo angle",
        .usage = "servo <angle 0-180>",
        .completion = NULL
    };

    static const shell_command_t cmd_motor_def = {
        .name = "motor",
        .handler = cmd_motor,
        .help = "Control motor speed",
        .usage = "motor <speed 0-100>",
        .completion = NULL
    };

    static const shell_command_t cmd_pwm_info_def = {
        .name = "pwm",
        .handler = cmd_pwm_info,
        .help = "Show PWM controller status",
        .usage = "pwm",
        .completion = NULL
    };

    /*-----------------------------------------------------------------------*/
    /* Initialization                                                        */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           初始化 PWM 设备
     */
    static bool init_pwm_devices(void) {
        /* 初始化 LED PWM */
        g_led_pwm = nx_factory_timer_pwm(LED_PWM_TIMER);
        if (!g_led_pwm) {
            return false;
        }
        g_led_pwm->set_frequency(g_led_pwm, LED_PWM_FREQ);
        g_led_pwm->configure_channel(g_led_pwm, LED_PWM_CHANNEL);
        g_led_pwm->set_duty_cycle(g_led_pwm, LED_PWM_CHANNEL, 0);
        g_led_pwm->start_channel(g_led_pwm, LED_PWM_CHANNEL);

        /* 初始化伺服 PWM */
        g_servo_pwm = nx_factory_timer_pwm(SERVO_PWM_TIMER);
        if (!g_servo_pwm) {
            return false;
        }
        g_servo_pwm->set_frequency(g_servo_pwm, SERVO_PWM_FREQ);
        g_servo_pwm->configure_channel(g_servo_pwm, SERVO_PWM_CHANNEL);
        g_servo_pwm->set_duty_cycle(g_servo_pwm, SERVO_PWM_CHANNEL, 7);  /* 90° */
        g_servo_pwm->start_channel(g_servo_pwm, SERVO_PWM_CHANNEL);

        /* 初始化电机 PWM */
        g_motor_pwm = nx_factory_timer_pwm(MOTOR_PWM_TIMER);
        if (!g_motor_pwm) {
            return false;
        }
        g_motor_pwm->set_frequency(g_motor_pwm, MOTOR_PWM_FREQ);
        g_motor_pwm->configure_channel(g_motor_pwm, MOTOR_PWM_CHANNEL);
        g_motor_pwm->set_duty_cycle(g_motor_pwm, MOTOR_PWM_CHANNEL, 0);
        g_motor_pwm->start_channel(g_motor_pwm, MOTOR_PWM_CHANNEL);

        return true;
    }

    /**
     * \brief           初始化 Shell
     */
    static bool init_shell(void) {
        shell_config_t config = {
            .prompt = "pwm> ",
            .cmd_buffer_size = 128,
            .history_depth = 16,
            .max_commands = 32
        };

        if (shell_init(&config) != SHELL_OK) {
            return false;
        }

        shell_register_builtin_commands();
        shell_register_command(&cmd_led_def);
        shell_register_command(&cmd_servo_def);
        shell_register_command(&cmd_motor_def);
        shell_register_command(&cmd_pwm_info_def);

        return true;
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 */
        osal_init();
        nx_hal_init();

        /* 获取 UART */
        g_uart = nx_factory_uart(0);
        if (!g_uart) {
            while (1) { /* 错误 */ }
        }

        /* 初始化 PWM 设备 */
        if (!init_pwm_devices()) {
            uart_print("Failed to initialize PWM devices\r\n");
            while (1) { /* 错误 */ }
        }

        /* 初始化 Shell */
        if (!init_shell()) {
            uart_print("Failed to initialize shell\r\n");
            while (1) { /* 错误 */ }
        }

        /* 打印欢迎信息 */
        uart_print("\r\n");
        uart_print("========================================\r\n");
        uart_print("  Nexus PWM Controller Demo\r\n");
        uart_printf("  HAL Version: %s\r\n", nx_hal_get_version());
        uart_print("  Type 'help' for available commands\r\n");
        uart_print("========================================\r\n");
        uart_print("pwm> ");

        /* 主循环 */
        while (1) {
            shell_process();
            osal_task_delay(10);
        }

        return 0;
    }

**功能说明：**

- 通过串口命令控制 LED、伺服和电机
- 支持实时调整 PWM 参数
- 提供状态查询功能
- 易于扩展和定制


最佳实践
--------

1. **选择合适的 PWM 频率**

   - LED 调光：1-10kHz（避免闪烁）
   - 伺服电机：50Hz（标准）
   - 直流电机：10-20kHz（超声波频率，更安静）
   - 音频：根据音调选择

2. **避免频率冲突**

   - 不同应用使用不同的定时器
   - 检查定时器资源分配
   - 注意定时器共享通道

3. **平滑过渡**

   - 使用渐变避免突变
   - 电机加速时使用斜坡函数
   - 伺服移动时使用小步长

4. **功耗优化**

   - 不使用时停止 PWM
   - 降低不必要的 PWM 频率
   - 使用低功耗定时器模式

5. **精确计时**

   - 使用硬件定时器而非软件延迟
   - 考虑时钟源的精度
   - 校准定时器频率

6. **错误处理**

   - 检查设备初始化返回值
   - 验证参数范围
   - 实现超时保护

7. **资源管理**

   - 正确释放定时器资源
   - 避免资源泄漏
   - 使用工厂模式管理设备

8. **调试技巧**

   - 使用示波器观察波形
   - 验证频率和占空比
   - 检查信号质量

常见问题
--------

**PWM 输出无信号：**

- 检查定时器是否正确初始化
- 验证 GPIO 引脚配置
- 确认 PWM 通道已启动
- 检查占空比是否为 0

**LED 闪烁：**

- PWM 频率太低（< 100Hz）
- 增加 PWM 频率到 1kHz 以上
- 检查电源稳定性

**伺服抖动：**

- PWM 频率不是 50Hz
- 脉冲宽度不稳定
- 电源电流不足
- 使用电容滤波

**电机运行不平滑：**

- PWM 频率太低
- 增加到 10-20kHz
- 检查 H 桥驱动器
- 添加加速斜坡

**定时器冲突：**

- 多个功能使用同一定时器
- 使用不同的定时器
- 检查 Kconfig 配置

**占空比不准确：**

- 定时器分辨率不足
- 增加定时器时钟频率
- 使用更高分辨率的定时器

性能优化
--------

提高 PWM 分辨率
~~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           配置高分辨率 PWM
     * \details         使用更高的定时器时钟获得更好的分辨率
     */
    static void configure_high_resolution_pwm(void) {
        nx_timer_pwm_t* pwm = nx_factory_timer_pwm(0);

        /* 设置较高的 PWM 频率 */
        pwm->set_frequency(pwm, 10000);  /* 10kHz */

        /* 现在可以使用更精细的占空比控制 */
        /* 例如：50.5% 而不是只能 50% 或 51% */
    }

减少 CPU 负载
~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           使用 DMA 更新 PWM（如果支持）
     * \details         减少 CPU 干预，提高效率
     */
    static void pwm_with_dma(void) {
        /* 某些平台支持 DMA 自动更新 PWM */
        /* 查看平台文档了解详情 */
    }

多通道同步
~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           同步多个 PWM 通道
     * \details         确保多个通道同时更新
     */
    static void sync_pwm_channels(nx_timer_pwm_t* pwm) {
        /* 停止所有通道 */
        for (uint8_t ch = 0; ch < 4; ch++) {
            pwm->stop_channel(pwm, ch);
        }

        /* 更新所有占空比 */
        pwm->set_duty_cycle(pwm, 0, 25);
        pwm->set_duty_cycle(pwm, 1, 50);
        pwm->set_duty_cycle(pwm, 2, 75);
        pwm->set_duty_cycle(pwm, 3, 100);

        /* 同时启动所有通道 */
        for (uint8_t ch = 0; ch < 4; ch++) {
            pwm->start_channel(pwm, ch);
        }
    }

高级应用
--------

音频生成
~~~~~~~~

使用 PWM 生成简单的音调：

.. code-block:: c

    /**
     * \brief           使用 PWM 生成音调
     * \param[in]       pwm: PWM 设备
     * \param[in]       channel: PWM 通道
     * \param[in]       frequency: 音调频率（Hz）
     * \param[in]       duration_ms: 持续时间（毫秒）
     */
    static void play_tone(nx_timer_pwm_t* pwm, uint8_t channel,
                         uint32_t frequency, uint32_t duration_ms) {
        /* 设置 PWM 频率为音调频率 */
        pwm->set_frequency(pwm, frequency);

        /* 设置 50% 占空比 */
        pwm->set_duty_cycle(pwm, channel, 50);

        /* 启动 PWM */
        pwm->start_channel(pwm, channel);

        /* 播放指定时间 */
        osal_task_delay(duration_ms);

        /* 停止 PWM */
        pwm->stop_channel(pwm, channel);
    }

    /* 播放音阶 */
    static void play_scale(nx_timer_pwm_t* pwm, uint8_t channel) {
        const uint32_t notes[] = {
            262,  /* C4 */
            294,  /* D4 */
            330,  /* E4 */
            349,  /* F4 */
            392,  /* G4 */
            440,  /* A4 */
            494,  /* B4 */
            523   /* C5 */
        };

        for (uint8_t i = 0; i < 8; i++) {
            play_tone(pwm, channel, notes[i], 500);
            osal_task_delay(100);  /* 音符间隔 */
        }
    }

RGB LED 控制
~~~~~~~~~~~~

使用三个 PWM 通道控制 RGB LED：

.. code-block:: c

    /**
     * \brief           RGB LED 控制结构
     */
    typedef struct {
        nx_timer_pwm_t* pwm;
        uint8_t red_channel;
        uint8_t green_channel;
        uint8_t blue_channel;
    } rgb_led_t;

    /**
     * \brief           设置 RGB 颜色
     * \param[in]       led: RGB LED 结构
     * \param[in]       red: 红色亮度 (0-100)
     * \param[in]       green: 绿色亮度 (0-100)
     * \param[in]       blue: 蓝色亮度 (0-100)
     */
    static void rgb_set_color(rgb_led_t* led, uint8_t red,
                             uint8_t green, uint8_t blue) {
        led->pwm->set_duty_cycle(led->pwm, led->red_channel, red);
        led->pwm->set_duty_cycle(led->pwm, led->green_channel, green);
        led->pwm->set_duty_cycle(led->pwm, led->blue_channel, blue);
    }

    /**
     * \brief           彩虹效果
     */
    static void rgb_rainbow_effect(rgb_led_t* led) {
        /* 红 -> 黄 */
        for (uint8_t g = 0; g <= 100; g += 2) {
            rgb_set_color(led, 100, g, 0);
            osal_task_delay(20);
        }

        /* 黄 -> 绿 */
        for (uint8_t r = 100; r > 0; r -= 2) {
            rgb_set_color(led, r, 100, 0);
            osal_task_delay(20);
        }

        /* 绿 -> 青 */
        for (uint8_t b = 0; b <= 100; b += 2) {
            rgb_set_color(led, 0, 100, b);
            osal_task_delay(20);
        }

        /* 青 -> 蓝 */
        for (uint8_t g = 100; g > 0; g -= 2) {
            rgb_set_color(led, 0, g, 100);
            osal_task_delay(20);
        }

        /* 蓝 -> 品红 */
        for (uint8_t r = 0; r <= 100; r += 2) {
            rgb_set_color(led, r, 0, 100);
            osal_task_delay(20);
        }

        /* 品红 -> 红 */
        for (uint8_t b = 100; b > 0; b -= 2) {
            rgb_set_color(led, 100, 0, b);
            osal_task_delay(20);
        }
    }

编码器输入
~~~~~~~~~~

使用定时器编码器模式读取旋转编码器：

.. code-block:: c

    /**
     * \brief           初始化编码器
     * \param[in]       timer_index: 定时器索引
     * \return          编码器设备指针
     */
    static nx_timer_encoder_t* encoder_init(uint8_t timer_index) {
        nx_timer_encoder_t* encoder = nx_factory_timer_encoder(timer_index);
        if (!encoder) {
            return NULL;
        }

        /* 配置编码器模式 */
        if (encoder->configure) {
            encoder->configure(encoder);
        }

        /* 启动编码器 */
        if (encoder->start) {
            encoder->start(encoder);
        }

        return encoder;
    }

    /**
     * \brief           读取编码器位置
     * \param[in]       encoder: 编码器设备
     * \return          当前位置
     */
    static int32_t encoder_get_position(nx_timer_encoder_t* encoder) {
        int32_t position = 0;

        if (encoder->get_count) {
            position = encoder->get_count(encoder);
        }

        return position;
    }

    /**
     * \brief           重置编码器位置
     * \param[in]       encoder: 编码器设备
     */
    static void encoder_reset(nx_timer_encoder_t* encoder) {
        if (encoder->reset_count) {
            encoder->reset_count(encoder);
        }
    }

故障排除
--------

使用示波器调试
~~~~~~~~~~~~~~

1. **连接示波器**：将探头连接到 PWM 输出引脚
2. **设置触发**：使用边沿触发捕获波形
3. **测量参数**：
   - 频率：使用频率测量功能
   - 占空比：使用占空比测量功能
   - 上升/下降时间：检查信号质量
4. **验证波形**：确保波形符合预期

软件调试
~~~~~~~~

.. code-block:: c

    /**
     * \brief           PWM 调试信息输出
     */
    static void pwm_debug_info(nx_timer_pwm_t* pwm, uint8_t channel) {
        uart_printf("PWM Debug Info:\r\n");
        uart_printf("  Timer: %p\r\n", (void*)pwm);
        uart_printf("  Channel: %d\r\n", channel);

        /* 如果 HAL 提供了查询接口 */
        /* uart_printf("  Frequency: %lu Hz\r\n", pwm->get_frequency(pwm)); */
        /* uart_printf("  Duty Cycle: %d%%\r\n", pwm->get_duty_cycle(pwm, channel)); */
    }

下一步
------

- :doc:`spi_communication` - 学习 SPI 通信
- :doc:`i2c_sensors` - 使用 I2C 传感器
- :doc:`adc_sampling` - ADC 采样和信号处理
- :doc:`../user_guide/hal` - 探索更多 HAL 功能

参考资料
--------

- :doc:`../user_guide/hal` - HAL API 完整文档
- :doc:`../platform_guides/stm32f4` - STM32F4 平台特定信息
- `PWM 原理 <https://en.wikipedia.org/wiki/Pulse-width_modulation>`_
- `伺服电机控制 <https://www.servocity.com/how-does-a-servo-work>`_

