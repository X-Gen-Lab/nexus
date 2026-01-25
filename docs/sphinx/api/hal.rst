HAL API Reference
=================

This section documents the Hardware Abstraction Layer (HAL) API.

Overview
--------

The Nexus HAL uses a factory pattern for device creation and interface-based
design for operations. Include ``hal/nx_hal.h`` to access all HAL functionality.

Core Functions
--------------

.. code-block:: c

    /* Initialize HAL subsystem */
    nx_status_t nx_hal_init(void);

    /* Deinitialize HAL subsystem */
    nx_status_t nx_hal_deinit(void);

    /* Check if HAL is initialized */
    bool nx_hal_is_initialized(void);

    /* Get HAL version string */
    const char* nx_hal_get_version(void);

Status Codes
------------

.. code-block:: c

    typedef enum {
        NX_OK = 0,           /* Success */
        NX_ERR_PARAM,        /* Invalid parameter */
        NX_ERR_STATE,        /* Invalid state */
        NX_ERR_TIMEOUT,      /* Operation timeout */
        NX_ERR_BUSY,         /* Resource busy */
        NX_ERR_NO_MEM,       /* Out of memory */
        NX_ERR_NOT_FOUND,    /* Resource not found */
        NX_ERR_NOT_SUPPORTED,/* Operation not supported */
        NX_ERR_IO,           /* I/O error */
    } nx_status_t;

GPIO API
--------

**Factory Functions:**

.. code-block:: c

    /* Get GPIO device */
    nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin);

    /* Get GPIO device with configuration */
    nx_gpio_t* nx_factory_gpio_with_config(uint8_t port, uint8_t pin,
                                           const nx_gpio_config_t* cfg);

    /* Release GPIO device */
    void nx_factory_gpio_release(nx_gpio_t* gpio);

**Configuration:**

.. code-block:: c

    typedef enum {
        NX_GPIO_MODE_INPUT = 0,
        NX_GPIO_MODE_OUTPUT_PP,
        NX_GPIO_MODE_OUTPUT_OD,
        NX_GPIO_MODE_AF_PP,
        NX_GPIO_MODE_AF_OD,
        NX_GPIO_MODE_ANALOG,
    } nx_gpio_mode_t;

    typedef enum {
        NX_GPIO_PULL_NONE = 0,
        NX_GPIO_PULL_UP,
        NX_GPIO_PULL_DOWN,
    } nx_gpio_pull_t;

    typedef enum {
        NX_GPIO_SPEED_LOW = 0,
        NX_GPIO_SPEED_MEDIUM,
        NX_GPIO_SPEED_HIGH,
        NX_GPIO_SPEED_VERY_HIGH,
    } nx_gpio_speed_t;

    typedef struct {
        nx_gpio_mode_t mode;
        nx_gpio_pull_t pull;
        nx_gpio_speed_t speed;
        uint8_t af_index;
    } nx_gpio_config_t;

**Interface:**

.. code-block:: c

    struct nx_gpio_s {
        /* Basic operations */
        uint8_t (*read)(nx_gpio_t* self);
        void (*write)(nx_gpio_t* self, uint8_t state);
        void (*toggle)(nx_gpio_t* self);

        /* Runtime configuration */
        nx_status_t (*set_mode)(nx_gpio_t* self, nx_gpio_mode_t mode);
        nx_status_t (*set_pull)(nx_gpio_t* self, nx_gpio_pull_t pull);
        nx_status_t (*get_config)(nx_gpio_t* self, nx_gpio_config_t* cfg);
        nx_status_t (*set_config)(nx_gpio_t* self, const nx_gpio_config_t* cfg);

        /* Interrupt configuration */
        nx_status_t (*set_exti)(nx_gpio_t* self, nx_gpio_exti_trig_t trig,
                                nx_gpio_exti_callback_t cb, void* ctx);
        nx_status_t (*clear_exti)(nx_gpio_t* self);

        /* Base interfaces */
        nx_lifecycle_t* (*get_lifecycle)(nx_gpio_t* self);
        nx_power_t* (*get_power)(nx_gpio_t* self);
    };

UART API
--------

**Factory Functions:**

.. code-block:: c

    /* Get UART device */
    nx_uart_t* nx_factory_uart(uint8_t index);

    /* Get UART device with configuration */
    nx_uart_t* nx_factory_uart_with_config(uint8_t index,
                                           const nx_uart_config_t* cfg);

    /* Release UART device */
    void nx_factory_uart_release(nx_uart_t* uart);

**Configuration:**

.. code-block:: c

    typedef struct {
        uint32_t baudrate;
        uint8_t word_length;   /* 8 or 9 */
        uint8_t stop_bits;     /* 1 or 2 */
        uint8_t parity;        /* 0=none, 1=odd, 2=even */
        uint8_t flow_control;  /* 0=none, 1=rts, 2=cts, 3=rts_cts */
        bool dma_tx_enable;
        bool dma_rx_enable;
        size_t tx_buf_size;
        size_t rx_buf_size;
    } nx_uart_config_t;

**Interface:**

.. code-block:: c

    struct nx_uart_s {
        /* Operation interfaces */
        nx_tx_async_t* (*get_tx_async)(nx_uart_t* self);
        nx_rx_async_t* (*get_rx_async)(nx_uart_t* self);
        nx_tx_sync_t* (*get_tx_sync)(nx_uart_t* self);
        nx_rx_sync_t* (*get_rx_sync)(nx_uart_t* self);

        /* Runtime configuration */
        nx_status_t (*set_baudrate)(nx_uart_t* self, uint32_t baudrate);
        nx_status_t (*get_config)(nx_uart_t* self, nx_uart_config_t* cfg);
        nx_status_t (*set_config)(nx_uart_t* self, const nx_uart_config_t* cfg);

        /* Base interfaces */
        nx_lifecycle_t* (*get_lifecycle)(nx_uart_t* self);
        nx_power_t* (*get_power)(nx_uart_t* self);
        nx_diagnostic_t* (*get_diagnostic)(nx_uart_t* self);

        /* Diagnostics */
        nx_status_t (*get_stats)(nx_uart_t* self, nx_uart_stats_t* stats);
        nx_status_t (*clear_errors)(nx_uart_t* self);
    };

**TX/RX Interfaces:**

.. code-block:: c

    /* Synchronous TX */
    struct nx_tx_sync_s {
        nx_status_t (*send)(nx_tx_sync_t* self, const uint8_t* data,
                            size_t len, uint32_t timeout_ms);
    };

    /* Synchronous RX */
    struct nx_rx_sync_s {
        nx_status_t (*receive)(nx_rx_sync_t* self, uint8_t* data,
                               size_t len, uint32_t timeout_ms);
    };

    /* Asynchronous TX */
    struct nx_tx_async_s {
        nx_status_t (*send)(nx_tx_async_t* self, const uint8_t* data, size_t len);
        size_t (*get_free_space)(nx_tx_async_t* self);
        bool (*is_busy)(nx_tx_async_t* self);
    };

    /* Asynchronous RX */
    struct nx_rx_async_s {
        size_t (*read)(nx_rx_async_t* self, uint8_t* data, size_t max_len);
        size_t (*available)(nx_rx_async_t* self);
        nx_status_t (*set_callback)(nx_rx_async_t* self, void (*cb)(void*), void* ctx);
    };

SPI API
-------

**Factory Functions:**

.. code-block:: c

    nx_spi_t* nx_factory_spi(uint8_t index);
    void nx_factory_spi_release(nx_spi_t* spi);

.. note::
   The ``nx_factory_spi_with_config()`` function has been removed.
   Use Kconfig for compile-time bus configuration.

I2C API
-------

**Factory Functions:**

.. code-block:: c

    nx_i2c_t* nx_factory_i2c(uint8_t index);
    void nx_factory_i2c_release(nx_i2c_t* i2c);

Timer API
---------

**Factory Functions:**

.. code-block:: c

    nx_timer_t* nx_factory_timer(uint8_t index);
    nx_timer_t* nx_factory_timer_with_config(uint8_t index, const nx_timer_config_t* cfg);
    void nx_factory_timer_release(nx_timer_t* timer);

ADC API
-------

**Factory Functions:**

.. code-block:: c

    nx_adc_t* nx_factory_adc(uint8_t index);
    nx_adc_t* nx_factory_adc_with_config(uint8_t index, const nx_adc_config_t* cfg);
    void nx_factory_adc_release(nx_adc_t* adc);

Device Enumeration
------------------

.. code-block:: c

    typedef struct {
        const char* name;
        const char* type;
        nx_device_state_t state;
        uint8_t ref_count;
    } nx_device_info_t;

    /* Enumerate all devices */
    size_t nx_factory_enumerate(nx_device_info_t* list, size_t max_count);

Base Interfaces
---------------

**Lifecycle Interface:**

.. code-block:: c

    struct nx_lifecycle_s {
        nx_status_t (*init)(nx_lifecycle_t* self);
        nx_status_t (*deinit)(nx_lifecycle_t* self);
        nx_status_t (*suspend)(nx_lifecycle_t* self);
        nx_status_t (*resume)(nx_lifecycle_t* self);
        nx_device_state_t (*get_state)(nx_lifecycle_t* self);
    };

**Power Interface:**

.. code-block:: c

    typedef enum {
        NX_POWER_MODE_NORMAL = 0,
        NX_POWER_MODE_SLEEP,
        NX_POWER_MODE_DEEP_SLEEP,
    } nx_power_mode_t;

    struct nx_power_s {
        nx_status_t (*set_mode)(nx_power_t* self, nx_power_mode_t mode);
        nx_power_mode_t (*get_mode)(nx_power_t* self);
    };

**Diagnostic Interface:**

.. code-block:: c

    struct nx_diagnostic_s {
        nx_status_t (*self_test)(nx_diagnostic_t* self);
        nx_status_t (*get_error_count)(nx_diagnostic_t* self, uint32_t* count);
        nx_status_t (*clear_errors)(nx_diagnostic_t* self);
    };


Related APIs
------------

- :doc:`osal` - OS abstraction for threading and synchronization
- :doc:`init` - Automatic initialization system
- :doc:`config` - Configuration management
- :doc:`log` - Logging framework

See Also
--------

- :doc:`../user_guide/hal` - HAL User Guide
- :doc:`../user_guide/porting` - Porting Guide
- :doc:`../reference/error_codes` - Error Code Reference
- :doc:`../platform_guides/index` - Platform-Specific Guides
