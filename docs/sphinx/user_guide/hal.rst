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
        nx_status_t status = nx_hal_init();
        if (status != NX_OK) {
            return -1;
        }

        /* Get a GPIO device (port is character 'A', 'B', etc.) */
        nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
        if (led) {
            /* Initialize GPIO device */
            nx_lifecycle_t* lc = led->get_lifecycle(led);
            status = lc->init(lc);
            if (status == NX_OK) {
                led->write(led, 1);  /* Turn on */
                led->toggle(led);    /* Toggle */
            }
        }

        /* Get UART device */
        nx_uart_t* uart = nx_factory_uart(0);
        if (uart) {
            /* Initialize UART device */
            nx_lifecycle_t* lc = uart->get_lifecycle(uart);
            status = lc->init(lc);
            if (status == NX_OK) {
                nx_tx_sync_t* tx = uart->get_tx_sync(uart);
                const char* msg = "Hello!\n";
                tx->send(tx, (const uint8_t*)msg, strlen(msg), 1000);
            }
        }

        /* Cleanup */
        nx_hal_deinit();
        return 0;
    }

GPIO Module
-----------

**Get GPIO device:**

.. code-block:: c

    /* Get GPIO read-write interface (port is a character 'A', 'B', etc.) */
    nx_gpio_t* gpio = nx_factory_gpio('A', 5);  /* Port A, Pin 5 */

    /* Or get specific interfaces */
    nx_gpio_read_t* input = nx_factory_gpio_read('B', 0);
    nx_gpio_write_t* output = nx_factory_gpio_write('C', 13);

**Write operations:**

.. code-block:: c

    nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);
    if (led) {
        /* Initialize device */
        nx_lifecycle_t* lc = led->get_lifecycle(led);
        nx_status_t status = lc->init(lc);
        if (status == NX_OK) {
            led->write(led, 1);  /* High */
            led->write(led, 0);  /* Low */
            led->toggle(led);    /* Toggle state */
        }
    }

**Read operations:**

.. code-block:: c

    nx_gpio_read_t* button = nx_factory_gpio_read('B', 0);
    if (button) {
        /* Initialize device */
        nx_lifecycle_t* lc = button->get_lifecycle(button);
        nx_status_t status = lc->init(lc);
        if (status == NX_OK) {
            uint8_t state = button->read(button);
        }
    }

**External interrupt:**

.. code-block:: c

    void button_callback(void* user_data)
    {
        /* Handle button press */
    }

    nx_gpio_read_t* btn = nx_factory_gpio_read('B', 0);
    if (btn) {
        /* Initialize device */
        nx_lifecycle_t* lc = btn->get_lifecycle(btn);
        nx_status_t status = lc->init(lc);
        if (status == NX_OK) {
            btn->register_exti(btn, button_callback, NULL, NX_GPIO_TRIGGER_FALLING);
        }
    }

UART Module
-----------

**Get UART device:**

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);  /* UART0 */

.. note::
   UART configuration (baudrate, parity, stop bits, etc.) is done through
   Kconfig at compile-time. The factory function returns a configured device.

**Synchronous operations (blocking):**

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);
    if (uart) {
        /* Initialize device */
        nx_lifecycle_t* lc = uart->get_lifecycle(uart);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        nx_tx_sync_t* tx = uart->get_tx_sync(uart);
        nx_rx_sync_t* rx = uart->get_rx_sync(uart);

        /* Transmit with timeout */
        const char* msg = "Hello, Nexus!";
        status = tx->send(tx, (const uint8_t*)msg, strlen(msg), 1000);

        /* Receive with timeout */
        uint8_t buf[64];
        size_t len = sizeof(buf);
        status = rx->receive(rx, buf, &len, 1000);

        /* Receive exact length */
        len = 10;
        status = rx->receive_all(rx, buf, &len, 1000);
    }

**Asynchronous operations (non-blocking):**

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);
    if (uart) {
        /* Initialize device */
        nx_lifecycle_t* lc = uart->get_lifecycle(uart);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        nx_tx_async_t* tx = uart->get_tx_async(uart);
        nx_rx_async_t* rx = uart->get_rx_async(uart);

        /* Non-blocking send */
        status = tx->send(tx, data, len);
        if (status == NX_ERR_BUSY) {
            /* Device busy, try later */
        }

        /* Non-blocking receive */
        uint8_t buf[64];
        size_t len = sizeof(buf);
        status = rx->receive(rx, buf, &len);
        if (status == NX_OK) {
            /* Data received, len contains actual bytes read */
        }
    }

SPI Module
----------

**Get SPI bus:**

.. code-block:: c

    nx_spi_t* spi = nx_factory_spi(0);  /* SPI0 bus */

.. note::
   Bus-level configuration (clock frequency, pin mapping) is done through
   Kconfig at compile-time. Device-specific parameters (CS pin, speed, mode)
   are specified when acquiring communication handles.

**Synchronous operations (blocking):**

.. code-block:: c

    nx_spi_t* spi = nx_factory_spi(0);
    if (spi) {
        /* Initialize device */
        nx_lifecycle_t* lc = spi->get_lifecycle(spi);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        /* Configure device parameters */
        nx_spi_device_config_t dev_cfg = NX_SPI_DEVICE_CONFIG_DEFAULT(5, 1000000);
        dev_cfg.mode = NX_SPI_MODE_0;
        dev_cfg.bit_order = NX_SPI_BIT_ORDER_MSB;

        /* Get sync TX/RX handle for this device */
        nx_tx_rx_sync_t* txrx = spi->get_tx_rx_sync_handle(spi, dev_cfg);
        if (txrx) {
            uint8_t tx_data[] = {0x01, 0x02, 0x03};
            uint8_t rx_data[3];
            size_t rx_len = sizeof(rx_data);

            status = txrx->tx_rx(txrx, tx_data, sizeof(tx_data),
                                rx_data, &rx_len, 1000);
        }
    }

**Asynchronous operations (non-blocking):**

.. code-block:: c

    nx_spi_t* spi = nx_factory_spi(0);
    if (spi) {
        /* Initialize device */
        nx_lifecycle_t* lc = spi->get_lifecycle(spi);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        nx_spi_device_config_t dev_cfg = NX_SPI_DEVICE_CONFIG_DEFAULT(5, 1000000);

        /* Get async TX/RX handle */
        nx_tx_rx_async_t* txrx = spi->get_tx_rx_async_handle(spi, dev_cfg,
                                                             my_callback, NULL);
        if (txrx) {
            uint8_t tx_data[] = {0x01, 0x02, 0x03};
            status = txrx->tx_rx(txrx, tx_data, sizeof(tx_data), 1000);
            /* Received data will be delivered via callback */
        }
    }

I2C Module
----------

**Get I2C bus:**

.. code-block:: c

    nx_i2c_t* i2c = nx_factory_i2c(0);  /* I2C0 bus */

.. note::
   Bus-level configuration (speed, pin mapping) is done through Kconfig at
   compile-time. Device address is specified when acquiring communication handles.

**Synchronous operations (blocking):**

.. code-block:: c

    nx_i2c_t* i2c = nx_factory_i2c(0);
    if (i2c) {
        /* Initialize device */
        nx_lifecycle_t* lc = i2c->get_lifecycle(i2c);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        uint8_t dev_addr = 0x50;  /* Device address (7-bit) */

        /* Get sync TX/RX handle for this device */
        nx_tx_rx_sync_t* txrx = i2c->get_tx_rx_sync_handle(i2c, dev_addr);
        if (txrx) {
            uint8_t tx_data[] = {0x00, 0x10};  /* Register address */
            uint8_t rx_data[16];
            size_t rx_len = sizeof(rx_data);

            status = txrx->tx_rx(txrx, tx_data, sizeof(tx_data),
                                rx_data, &rx_len, 1000);
        }

        /* Or use TX-only handle */
        nx_tx_sync_t* tx = i2c->get_tx_sync_handle(i2c, dev_addr);
        if (tx) {
            uint8_t data[] = {0x00, 0x10, 0xAA};
            status = tx->send(tx, data, sizeof(data), 1000);
        }
    }

**Asynchronous operations (non-blocking):**

.. code-block:: c

    nx_i2c_t* i2c = nx_factory_i2c(0);
    if (i2c) {
        /* Initialize device */
        nx_lifecycle_t* lc = i2c->get_lifecycle(i2c);
        nx_status_t status = lc->init(lc);
        if (status != NX_OK) {
            return;
        }

        uint8_t dev_addr = 0x50;

        /* Get async TX/RX handle */
        nx_tx_rx_async_t* txrx = i2c->get_tx_rx_async_handle(i2c, dev_addr,
                                                             my_callback, NULL);
        if (txrx) {
            uint8_t tx_data[] = {0x00, 0x10};
            status = txrx->tx_rx(txrx, tx_data, sizeof(tx_data), 1000);
            /* Received data will be delivered via callback */
        }
    }

Error Handling
--------------

All factory functions return ``NULL`` on failure. Interface methods return
``nx_status_t``:

.. code-block:: c

    nx_gpio_t* gpio = nx_factory_gpio('A', 5);
    if (!gpio) {
        /* Handle error: device not available */
        return -1;
    }

    /* Check operation status */
    nx_lifecycle_t* lc = gpio->read.get_lifecycle(&gpio->read);
    nx_status_t status = lc->init(lc);
    if (status != NX_OK) {
        /* Handle error */
        const char* err_str = nx_status_to_string(status);
    }

Common status codes:

- ``NX_OK`` - Success
- ``NX_ERR_INVALID_PARAM`` - Invalid parameter
- ``NX_ERR_INVALID_STATE`` - Invalid state
- ``NX_ERR_TIMEOUT`` - Operation timeout
- ``NX_ERR_BUSY`` - Resource busy
- ``NX_ERR_NO_MEMORY`` - Out of memory
- ``NX_ERR_NO_DATA`` - No data available
- ``NX_ERR_NACK`` - NACK received (I2C)
- ``NX_ERR_BUS`` - Bus error

Lifecycle Management
--------------------

Devices support lifecycle operations through the ``nx_lifecycle_t`` interface:

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);
    if (uart) {
        nx_lifecycle_t* lc = uart->get_lifecycle(uart);

        /* Initialize device */
        nx_status_t status = lc->init(lc);

        /* Check device state */
        nx_device_state_t state = lc->get_state(lc);

        /* Suspend device */
        status = lc->suspend(lc);

        /* Resume device */
        status = lc->resume(lc);

        /* Deinitialize device */
        status = lc->deinit(lc);
    }

Power Management
----------------

Devices support power management through the ``nx_power_t`` interface:

.. code-block:: c

    nx_uart_t* uart = nx_factory_uart(0);
    if (uart) {
        nx_power_t* pwr = uart->get_power(uart);

        /* Disable peripheral clock to save power */
        nx_status_t status = pwr->disable(pwr);

        /* Enable peripheral clock */
        status = pwr->enable(pwr);

        /* Check power state */
        bool enabled = pwr->is_enabled(pwr);
    }

API Reference
-------------

See :doc:`../api/hal` for complete API documentation.
