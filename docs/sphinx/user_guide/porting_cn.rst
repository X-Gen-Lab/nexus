移植指南
========

本指南介绍如何将 Nexus 移植到新的 MCU 平台。

概述
----

将 Nexus 移植到新平台需要为目标 MCU 实现硬件抽象层（HAL）。
OSAL 和中间件层是平台无关的，无需修改。

前置条件
--------

开始之前，请确保你有：

- MCU 数据手册和参考手册
- 厂商 SDK 或寄存器定义
- ARM GCC 工具链（用于 ARM Cortex-M 目标）
- 对目标 MCU 架构的基本了解

移植步骤
--------

步骤 1：创建平台目录
~~~~~~~~~~~~~~~~~~~~

在 ``platforms/`` 下创建新目录：

::

    platforms/
    └── my_platform/
        ├── CMakeLists.txt
        ├── include/
        │   └── platform_config.h
        ├── src/
        │   ├── startup.c
        │   ├── system_init.c
        │   └── hal/
        │       ├── hal_gpio.c
        │       ├── hal_uart.c
        │       └── ...
        └── linker/
            └── my_platform.ld

步骤 2：实现 HAL 驱动
~~~~~~~~~~~~~~~~~~~~~

为你的平台实现每个 HAL 模块。从 GPIO 开始，因为它最简单：

**hal_gpio.c:**

.. code-block:: c

    /**
     * \file            hal_gpio.c
     * \brief           my_platform 的 GPIO HAL 实现
     */

    #include "hal/hal_gpio.h"
    #include "my_platform_registers.h"

    hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                               const hal_gpio_config_t* config)
    {
        if (config == NULL) {
            return HAL_ERROR_NULL_POINTER;
        }

        if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
            return HAL_ERROR_INVALID_PARAM;
        }

        /* 平台特定初始化 */
        GPIO_TypeDef* gpio = get_gpio_base(port);

        /* 配置方向 */
        if (config->direction == HAL_GPIO_DIR_OUTPUT) {
            gpio->MODER |= (1 << (pin * 2));
        } else {
            gpio->MODER &= ~(3 << (pin * 2));
        }

        /* 配置上拉/下拉 */
        /* ... */

        return HAL_OK;
    }

    hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                                hal_gpio_level_t level)
    {
        GPIO_TypeDef* gpio = get_gpio_base(port);

        if (level == HAL_GPIO_LEVEL_HIGH) {
            gpio->BSRR = (1 << pin);
        } else {
            gpio->BSRR = (1 << (pin + 16));
        }

        return HAL_OK;
    }

    /* 实现其余 GPIO 函数... */

步骤 3：创建启动代码
~~~~~~~~~~~~~~~~~~~~

为你的 MCU 实现启动代码：

**startup.c:**

.. code-block:: c

    /**
     * \file            startup.c
     * \brief           my_platform 的启动代码
     */

    #include <stdint.h>

    /* 栈指针（在链接脚本中定义） */
    extern uint32_t _estack;

    /* 入口点 */
    extern int main(void);

    /* 弱中断处理程序 */
    void Default_Handler(void) { while(1); }
    void Reset_Handler(void);
    void NMI_Handler(void)      __attribute__((weak, alias("Default_Handler")));
    void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
    /* ... 更多处理程序 ... */

    /* 向量表 */
    __attribute__((section(".isr_vector")))
    const void* vector_table[] = {
        &_estack,
        Reset_Handler,
        NMI_Handler,
        HardFault_Handler,
        /* ... 更多向量 ... */
    };

    void Reset_Handler(void)
    {
        /* 复制 .data 段 */
        /* 清零 .bss 段 */
        /* 调用系统初始化 */
        /* 调用 main */
        main();
        while(1);
    }

步骤 4：创建链接脚本
~~~~~~~~~~~~~~~~~~~~

为你的 MCU 创建链接脚本：

**my_platform.ld:**

.. code-block:: text

    /* 内存布局 */
    MEMORY
    {
        FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
        RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
    }

    /* 入口点 */
    ENTRY(Reset_Handler)

    /* 段 */
    SECTIONS
    {
        .isr_vector :
        {
            . = ALIGN(4);
            KEEP(*(.isr_vector))
            . = ALIGN(4);
        } > FLASH

        .text :
        {
            . = ALIGN(4);
            *(.text)
            *(.text*)
            . = ALIGN(4);
        } > FLASH

        /* ... 更多段 ... */
    }

步骤 5：添加 CMake 配置
~~~~~~~~~~~~~~~~~~~~~~~

为你的平台创建 CMakeLists.txt：

**CMakeLists.txt:**

.. code-block:: cmake

    # 平台: my_platform

    set(PLATFORM_SOURCES
        src/startup.c
        src/system_init.c
        src/hal/hal_gpio.c
        src/hal/hal_uart.c
        src/hal/hal_spi.c
        src/hal/hal_i2c.c
        src/hal/hal_timer.c
    )

    add_library(platform_my_platform STATIC ${PLATFORM_SOURCES})

    target_include_directories(platform_my_platform PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/hal/include
    )

    target_link_libraries(platform_my_platform PUBLIC
        hal_interface
    )

    # 链接脚本
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/my_platform.ld)
    target_link_options(platform_my_platform PUBLIC
        -T${LINKER_SCRIPT}
    )

步骤 6：添加工具链文件
~~~~~~~~~~~~~~~~~~~~~~

如果需要，创建工具链文件：

**cmake/toolchains/my_platform.cmake:**

.. code-block:: cmake

    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR arm)

    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
    set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

    set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard")
    set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard")

HAL 实现清单
------------

按推荐顺序实现这些 HAL 模块：

+----------+----------+------------------------------------------+
| 模块     | 优先级   | 备注                                     |
+==========+==========+==========================================+
| GPIO     | 必需     | 从这里开始，最简单的模块                 |
+----------+----------+------------------------------------------+
| System   | 必需     | 时钟初始化，延时函数                     |
+----------+----------+------------------------------------------+
| UART     | 必需     | 调试必需                                 |
+----------+----------+------------------------------------------+
| Timer    | 必需     | OSAL 计时需要                            |
+----------+----------+------------------------------------------+
| SPI      | 可选     | 如果使用 SPI 外设                        |
+----------+----------+------------------------------------------+
| I2C      | 可选     | 如果使用 I2C 外设                        |
+----------+----------+------------------------------------------+
| ADC      | 可选     | 如果使用模拟输入                         |
+----------+----------+------------------------------------------+

测试你的移植
------------

1. **构建 blinky 示例：**

   .. code-block:: bash

       cmake -B build \
           -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/my_platform.cmake \
           -DNEXUS_PLATFORM=my_platform
       cmake --build build

2. **烧录并验证 LED 闪烁**

3. **在 native 平台运行单元测试：**

   .. code-block:: bash

       cmake -B build-test -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
       cmake --build build-test
       ctest --test-dir build-test

4. **单独测试每个 HAL 模块**

参考实现
--------

参见 ``platforms/stm32f4/`` 获取完整的参考实现。

需要学习的关键文件：

- ``platforms/stm32f4/src/hal/hal_gpio.c`` - GPIO 实现
- ``platforms/stm32f4/src/startup.c`` - 启动代码
- ``platforms/stm32f4/linker/stm32f407.ld`` - 链接脚本
- ``platforms/stm32f4/CMakeLists.txt`` - CMake 配置

常见问题
--------

**链接器报未定义符号错误：**
    确保所有 HAL 函数都已实现，即使是作为桩函数。

**启动时硬件故障：**
    检查向量表对齐和栈指针初始化。

**中断不工作：**
    验证 NVIC 配置和中断处理程序名称。

**计时问题：**
    确保系统时钟配置正确。

获取帮助
--------

如果遇到问题：

1. 查看参考实现
2. 查阅 MCU 文档
3. 在 GitHub 上提交 issue，附上你的平台详情
