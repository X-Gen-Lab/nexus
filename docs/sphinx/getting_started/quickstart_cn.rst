快速入门
========

本指南将帮助你创建第一个 Nexus 应用 - 一个简单的 LED 闪烁程序。

创建新项目
----------

1. 创建项目目录：

.. code-block:: bash

    mkdir my_blinky
    cd my_blinky

2. 创建 ``CMakeLists.txt``：

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.16)
    project(my_blinky C)

    # 添加 Nexus 作为子目录
    add_subdirectory(nexus)

    # 创建应用
    add_executable(blinky main.c)
    target_link_libraries(blinky PRIVATE platform_stm32f4 hal_interface)

3. 创建 ``main.c``：

.. code-block:: c

    #include "hal/hal.h"

    #define LED_PORT    HAL_GPIO_PORT_D
    #define LED_PIN     12

    int main(void)
    {
        hal_gpio_config_t config = {
            .direction   = HAL_GPIO_DIR_OUTPUT,
            .pull        = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed       = HAL_GPIO_SPEED_LOW,
            .init_level  = HAL_GPIO_LEVEL_LOW
        };

        hal_system_init();
        hal_gpio_init(LED_PORT, LED_PIN, &config);

        while (1) {
            hal_gpio_toggle(LED_PORT, LED_PIN);
            hal_delay_ms(500);
        }

        return 0;
    }

构建和烧录
----------

.. code-block:: bash

    # 配置
    cmake -B build \
        -DCMAKE_TOOLCHAIN_FILE=nexus/cmake/toolchains/arm-none-eabi.cmake \
        -DNEXUS_PLATFORM=stm32f4

    # 构建
    cmake --build build

    # 烧录 (使用 OpenOCD)
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/blinky.elf verify reset exit"

代码解析
--------

**HAL 初始化：**

.. code-block:: c

    hal_system_init();  // 初始化系统 (SysTick, 时钟)

**GPIO 配置：**

.. code-block:: c

    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,  // 输出模式
        .pull        = HAL_GPIO_PULL_NONE,   // 无上下拉
        .output_mode = HAL_GPIO_OUTPUT_PP,   // 推挽输出
        .speed       = HAL_GPIO_SPEED_LOW,   // 低速
        .init_level  = HAL_GPIO_LEVEL_LOW    // 初始低电平
    };
    hal_gpio_init(LED_PORT, LED_PIN, &config);

**主循环：**

.. code-block:: c

    while (1) {
        hal_gpio_toggle(LED_PORT, LED_PIN);  // 翻转 LED
        hal_delay_ms(500);                   // 等待 500ms
    }

下一步
------

- 添加 UART 串口输出
- 使用 OSAL 实现多任务
- 探索中间件组件
