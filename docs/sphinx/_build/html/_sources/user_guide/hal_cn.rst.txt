硬件抽象层 (HAL)
================

概述
----

HAL 为不同 MCU 平台的硬件外设提供统一接口。这使得应用代码无需修改即可移植。

支持的外设
----------

+----------+-------------+------------------+
| 模块     | 头文件      | 描述             |
+==========+=============+==================+
| GPIO     | hal_gpio.h  | 通用输入输出     |
+----------+-------------+------------------+
| UART     | hal_uart.h  | 串口通信         |
+----------+-------------+------------------+
| SPI      | hal_spi.h   | SPI 总线         |
+----------+-------------+------------------+
| I2C      | hal_i2c.h   | I2C 总线         |
+----------+-------------+------------------+
| Timer    | hal_timer.h | 硬件定时器       |
+----------+-------------+------------------+
| ADC      | hal_adc.h   | 模拟输入         |
+----------+-------------+------------------+

GPIO 模块
---------

**配置：**

.. code-block:: c

    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,  // 输出模式
        .pull        = HAL_GPIO_PULL_NONE,   // 无上下拉
        .output_mode = HAL_GPIO_OUTPUT_PP,   // 推挽输出
        .speed       = HAL_GPIO_SPEED_LOW,   // 低速
        .init_level  = HAL_GPIO_LEVEL_LOW    // 初始低电平
    };

    hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

**操作：**

.. code-block:: c

    // 写入
    hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);

    // 读取
    hal_gpio_level_t level;
    hal_gpio_read(HAL_GPIO_PORT_A, 5, &level);

    // 翻转
    hal_gpio_toggle(HAL_GPIO_PORT_A, 5);

UART 模块
---------

**配置：**

.. code-block:: c

    hal_uart_config_t config = {
        .baudrate = 115200,                   // 波特率
        .wordlen  = HAL_UART_WORDLEN_8,       // 8 位数据
        .stopbits = HAL_UART_STOPBITS_1,      // 1 位停止位
        .parity   = HAL_UART_PARITY_NONE,     // 无校验
        .flowctrl = HAL_UART_FLOWCTRL_NONE    // 无流控
    };

    hal_uart_init(HAL_UART_0, &config);

**操作：**

.. code-block:: c

    // 发送
    const char* msg = "Hello, Nexus!";
    hal_uart_transmit(HAL_UART_0, (uint8_t*)msg, strlen(msg), 1000);

    // 接收
    uint8_t buffer[64];
    hal_uart_receive(HAL_UART_0, buffer, sizeof(buffer), 1000);

错误处理
--------

所有 HAL 函数返回 ``hal_status_t``：

.. code-block:: c

    hal_status_t status = hal_gpio_init(port, pin, &config);
    if (status != HAL_OK) {
        // 处理错误
    }

常见状态码：

- ``HAL_OK`` - 成功
- ``HAL_ERR_PARAM`` - 参数无效
- ``HAL_ERR_STATE`` - 状态无效
- ``HAL_ERROR_TIMEOUT`` - 操作超时

API 参考
--------

完整 API 文档请参见 :doc:`../api/hal`。
