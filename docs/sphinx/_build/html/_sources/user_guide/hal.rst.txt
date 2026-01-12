Hardware Abstraction Layer (HAL)
================================

Overview
--------

The HAL provides a unified interface for hardware peripherals across different
MCU platforms. This allows application code to be portable without modification.

Supported Peripherals
---------------------

+----------+-------------+------------------+
| Module   | Header      | Description      |
+==========+=============+==================+
| GPIO     | hal_gpio.h  | General I/O      |
+----------+-------------+------------------+
| UART     | hal_uart.h  | Serial comm      |
+----------+-------------+------------------+
| SPI      | hal_spi.h   | SPI bus          |
+----------+-------------+------------------+
| I2C      | hal_i2c.h   | I2C bus          |
+----------+-------------+------------------+
| Timer    | hal_timer.h | Hardware timers  |
+----------+-------------+------------------+
| ADC      | hal_adc.h   | Analog input     |
+----------+-------------+------------------+

GPIO Module
-----------

**Configuration:**

.. code-block:: c

    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };

    hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

**Operations:**

.. code-block:: c

    // Write
    hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);

    // Read
    hal_gpio_level_t level;
    hal_gpio_read(HAL_GPIO_PORT_A, 5, &level);

    // Toggle
    hal_gpio_toggle(HAL_GPIO_PORT_A, 5);

UART Module
-----------

**Configuration:**

.. code-block:: c

    hal_uart_config_t config = {
        .baudrate = 115200,
        .wordlen  = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity   = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };

    hal_uart_init(HAL_UART_0, &config);

**Operations:**

.. code-block:: c

    // Transmit
    const char* msg = "Hello, Nexus!";
    hal_uart_transmit(HAL_UART_0, (uint8_t*)msg, strlen(msg), 1000);

    // Receive
    uint8_t buffer[64];
    hal_uart_receive(HAL_UART_0, buffer, sizeof(buffer), 1000);

Error Handling
--------------

All HAL functions return ``hal_status_t``:

.. code-block:: c

    hal_status_t status = hal_gpio_init(port, pin, &config);
    if (status != HAL_OK) {
        // Handle error
    }

Common status codes:

- ``HAL_OK`` - Success
- ``HAL_ERR_PARAM`` - Invalid parameter
- ``HAL_ERR_STATE`` - Invalid state
- ``HAL_ERROR_TIMEOUT`` - Operation timeout

API Reference
-------------

See :doc:`../api/hal` for complete API documentation.
