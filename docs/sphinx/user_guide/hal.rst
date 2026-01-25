Hardware Abstraction Layer (HAL)
================================

Overview
--------

The Nexus HAL provides a unified, object-oriented interface for hardware peripherals
across different MCU platforms. It uses a factory pattern to create device instances
and interface-based design for operations.

Key features:

* Factory pattern for device creation and lifecycle management
* Interface-based design with consistent API across peripherals
* Reference counting for safe resource management
* Support for both synchronous and asynchronous operations
* Power management and diagnostic interfaces

.. seealso::

   * :doc:`osal` - OS Abstraction Layer for RTOS integration
   * :doc:`kconfig_peripherals` - Peripheral configuration guide
   * :doc:`../platform_guides/index` - Platform-specific guides
   * :doc:`../tutorials/gpio_control` - GPIO tutorial
   * :doc:`../tutorials/uart_communication` - UART tutorial
   * :doc:`../api/hal` - Complete HAL API reference

Architecture
------------

The HAL is organized into several layers:

.. mermaid::
   :alt: HAL architecture showing factory pattern and device lifecycle

   graph TB
       APP[Application Code] --> FACTORY[nx_factory]
       FACTORY --> CREATE[Create Device]
       CREATE --> INTERFACE[Device Interface]
       INTERFACE --> GPIO[nx_gpio_t]
       INTERFACE --> UART[nx_uart_t]
       INTERFACE --> SPI[nx_spi_t]
       INTERFACE --> I2C[nx_i2c_t]

       GPIO --> PLATFORM[Platform Implementation]
       UART --> PLATFORM
       SPI --> PLATFORM
       I2C --> PLATFORM

       PLATFORM --> STM32F4[STM32F4 HAL]
       PLATFORM --> STM32H7[STM32H7 HAL]
       PLATFORM --> NATIVE[Native HAL]

       STM32F4 --> HW[Hardware]
       STM32H7 --> HW

       style APP fill:#e1f5ff
       style FACTORY fill:#fff4e1
       style INTERFACE fill:#ffe1f5
       style PLATFORM fill:#e1ffe1
       style HW fill:#cccccc

Device Lifecycle
~~~~~~~~~~~~~~~~

The following diagram shows the complete device lifecycle from creation to release:

.. mermaid::
   :alt: Device lifecycle workflow showing creation, configuration, usage, and release

   sequenceDiagram
       participant App as Application
       participant Factory as nx_factory
       participant Device as Device Instance
       participant Platform as Platform HAL
       participant HW as Hardware

       App->>Factory: nx_factory_gpio(port, pin)
       Factory->>Platform: Allocate device
       Platform->>HW: Initialize hardware
       HW-->>Platform: Hardware ready
       Platform-->>Factory: Device created
       Factory-->>App: Return device pointer

       App->>Device: write(device, value)
       Device->>Platform: Platform-specific write
       Platform->>HW: Set pin state
       HW-->>Platform: State set
       Platform-->>Device: Write complete
       Device-->>App: Operation complete

       App->>Factory: nx_factory_gpio_release(device)
       Factory->>Platform: Decrement ref count
       Platform->>HW: Deinitialize hardware
       HW-->>Platform: Hardware released
       Platform-->>Factory: Device released
       Factory-->>App: Release complete

Supported Peripherals
---------------------

+----------+---------------+------------------+
| Module   | Interface     | Description      |
+==========+===============+==================+
| GPIO     | nx_gpio_t     | General I/O      |
+----------+---------------+------------------+
| UART     | nx_uart_t     | Serial comm      |
+----------+---------------+------------------+
| SPI      | nx_spi_t      | SPI bus          |
+----------+---------------+------------------+
| I2C      | nx_i2c_t      | I2C bus          |
+----------+---------------+------------------+
| Timer    | nx_timer_t    | Hardware timers  |
+----------+---------------+------------------+
| ADC      | nx_adc_t      | Analog input     |
+----------+---------------+------------------+

Getting Started
---------------

**Include the main header:**

.. code-block:: c

    #include "hal/nx_hal.h"

**Initialize and use:**

.. code-block:: c

    int main(void)
    {
        /* Initialize HAL */
        nx_hal_init();

        /* Get a GPIO device */
        nx_gpio_t* led = nx_factory_gpio(0, 5);  /* Port A, Pin 5 */
        if (led) {
            led->write(led, 1);  /* Turn on */
            nx_factory_gpio_release(led);
        }

        /* Cleanup */
        nx_hal_deinit();
        return 0;
    }

GPIO Module
-----------

**Get GPIO with default configuration:**

.. code-block:: c

    nx_gpio_t* gpio = nx_factory_gpio(port, pin);

**Get GPIO with custom configuration:**

.. code-block:: c

    nx_gpio_config_t cfg = {
        .mode  = NX_GPIO_MODE_OUTPUT_PP,
        .pull  = NX_GPIO_PULL_NONE,
        .speed = NX_GPIO_SPEED_LOW,
    };

    nx_gpio_t* led = nx_factory_gpio_with_config(0, 5, &cfg);

**Operations:**

.. code-block:: c

    /* Write */
    led->write(led, 1);  /* High */
    led->write(led, 0);  /* Low */

    /* Read */
    uint8_t state = led->read(led);

    /* Toggle */
    led->toggle(led);

    /* Runtime configuration */
    led->set_mode(led, NX_GPIO_MODE_INPUT);
    led->set_pull(led, NX_GPIO_PULL_UP);

    /* Release when done */
    nx_factory_gpio_release(led);

**External interrupt:**

.. code-block:: c

    void button_callback(void* ctx)
    {
        /* Handle button press */
    }

    nx_gpio_t* btn = nx_factory_gpio(0, 0);
    btn->set_mode(btn, NX_GPIO_MODE_INPUT);
    btn->set_exti(btn, NX_GPIO_EXTI_FALLING, button_callback, NULL);

UART Module
-----------

**Get UART with default configuration:**

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);

**Get UART with custom configuration:**

.. code-block:: c

    nx_uart_config_t cfg = {
        .baudrate     = 115200,
        .word_length  = 8,
        .stop_bits    = 1,
        .parity       = 0,  /* None */
        .flow_control = 0,  /* None */
    };

    nx_uart_t* uart = nx_factory_uart_with_config(0, &cfg);

**Synchronous operations:**

.. code-block:: c

    nx_tx_sync_t* tx = uart->get_tx_sync(uart);
    nx_rx_sync_t* rx = uart->get_rx_sync(uart);

    /* Transmit with timeout */
    const char* msg = "Hello, Nexus!";
    tx->send(tx, (uint8_t*)msg, strlen(msg), 1000);

    /* Receive with timeout */
    uint8_t buf[64];
    rx->receive(rx, buf, sizeof(buf), 1000);

**Asynchronous operations:**

.. code-block:: c

    nx_tx_async_t* tx = uart->get_tx_async(uart);
    nx_rx_async_t* rx = uart->get_rx_async(uart);

    /* Non-blocking send */
    tx->send(tx, data, len);

    /* Check available data */
    size_t avail = rx->available(rx);
    if (avail > 0) {
        size_t read = rx->read(rx, buf, avail);
    }

    /* Set receive callback */
    rx->set_callback(rx, my_rx_callback, NULL);

**Release when done:**

.. code-block:: c

    nx_factory_uart_release(uart);

SPI Module
----------

**Get SPI device:**

.. code-block:: c

    nx_spi_t* spi = nx_factory_spi(0);

    /* Use SPI... */

    nx_factory_spi_release(spi);

.. note::
   Bus-level configuration (clock frequency, pin mapping) should be done
   through Kconfig at compile-time. Device-specific parameters (CS pin,
   speed, mode) are specified when acquiring communication handles.

I2C Module
----------

**Get I2C device:**

.. code-block:: c

    nx_i2c_t* i2c = nx_factory_i2c(0);

    /* Use I2C... */

    nx_factory_i2c_release(i2c);

Error Handling
--------------

All factory functions return ``NULL`` on failure. Interface methods return
``nx_status_t``:

.. code-block:: c

    nx_gpio_t* gpio = nx_factory_gpio(port, pin);
    if (!gpio) {
        /* Handle error: device not available */
        return -1;
    }

    nx_status_t status = gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP);
    if (status != NX_OK) {
        /* Handle error */
    }

Common status codes:

- ``NX_OK`` - Success
- ``NX_ERR_PARAM`` - Invalid parameter
- ``NX_ERR_STATE`` - Invalid state
- ``NX_ERR_TIMEOUT`` - Operation timeout
- ``NX_ERR_BUSY`` - Resource busy
- ``NX_ERR_NO_MEM`` - Out of memory

Lifecycle Management
--------------------

Devices support lifecycle operations through the ``nx_lifecycle_t`` interface:

.. code-block:: c

    nx_gpio_t* gpio = nx_factory_gpio(0, 5);
    nx_lifecycle_t* lc = gpio->get_lifecycle(gpio);

    /* Suspend device */
    lc->suspend(lc);

    /* Resume device */
    lc->resume(lc);

Power Management
----------------

Devices support power management through the ``nx_power_t`` interface:

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);
    nx_power_t* pwr = uart->get_power(uart);

    /* Enter low power mode */
    pwr->set_mode(pwr, NX_POWER_MODE_SLEEP);

    /* Wake up */
    pwr->set_mode(pwr, NX_POWER_MODE_NORMAL);

API Reference
-------------

See :doc:`../api/hal` for complete API documentation.
