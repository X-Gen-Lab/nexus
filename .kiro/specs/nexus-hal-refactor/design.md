# Design Document: Nexus HAL 完整重写

## Overview

本设计文档描述了 Nexus 平台 HAL 完整重写的详细技术方案。采用现代化的 C 语言面向对象设计，通过接口指针实现多态，提供完整的设备抽象、生命周期管理、运行时配置、资源管理等高级特性。

## Architecture

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Application Layer                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Factory Layer                                      │
│     nx_factory_uart() / nx_factory_gpio() / nx_factory_spi() / ...          │
│     nx_factory_*_release() - Reference count management                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Interface Layer                                    │
│    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│    │ nx_uart_t   │  │ nx_gpio_t   │  │ nx_spi_t    │  │ nx_i2c_t    │      │
│    │  + ops      │  │  + ops      │  │  + ops      │  │  + ops      │      │
│    │  + lifecycle│  │  + lifecycle│  │  + lifecycle│  │  + lifecycle│      │
│    │  + power    │  │  + power    │  │  + power    │  │  + power    │      │
│    │  + diag     │  │  + diag     │  │  + diag     │  │  + diag     │      │
│    └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘      │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Base Interface Layer                               │
│    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│    │nx_lifecycle_t│ │ nx_power_t  │  │nx_configurable_t│ │nx_diagnostic_t│  │
│    └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘      │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Device Layer                                       │
│          nx_device_t + ref_count + state_machine + runtime_config           │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Resource Manager Layer                             │
│         ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                   │
│         │nx_dma_manager_t│ │nx_isr_manager_t│ │Clock Manager│                │
│         └─────────────┘  └─────────────┘  └─────────────┘                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Platform Driver Layer                              │
│    ┌─────────────────────────┐  ┌─────────────────────────┐                 │
│    │   platforms/stm32f4/    │  │   platforms/native/     │                 │
│    └─────────────────────────┘  └─────────────────────────┘                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                           Hardware / Simulation                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Interface Segregation**: Separate operations, configuration, lifecycle, and power management into independent interfaces
2. **Composition over Inheritance**: Devices gain capabilities by composing multiple interfaces
3. **Centralized Resource Management**: DMA, interrupts, and clocks are managed by dedicated managers
4. **Reference Counting**: Support multi-module sharing of device instances
5. **State Machine Management**: Device state transitions follow strict state machine rules

## Components and Interfaces

### 1. Unified Status Codes (hal/nx_status.h)

```c
/**
 * \file            nx_status.h
 * \brief           Nexus platform unified status/error codes
 * \author          Nexus Team
 *
 * Naming convention: NX_ERR_<CATEGORY>_<SPECIFIC_ERROR>
 * - NX_OK: Success
 * - NX_ERR_*: Error codes
 */
typedef enum nx_status_e {
    /* Success */
    NX_OK = 0,                      /**< Operation successful */

    /* General errors (1-19) */
    NX_ERR_GENERIC = 1,             /**< Generic error */
    NX_ERR_INVALID_PARAM = 2,       /**< Invalid parameter */
    NX_ERR_NULL_PTR = 3,            /**< Null pointer */
    NX_ERR_NOT_SUPPORTED = 4,       /**< Operation not supported */

    /* State errors (20-39) */
    NX_ERR_NOT_INIT = 20,           /**< Not initialized */
    NX_ERR_ALREADY_INIT = 21,       /**< Already initialized */
    NX_ERR_INVALID_STATE = 22,      /**< Invalid state */
    NX_ERR_BUSY = 23,               /**< Device busy */

    /* Resource errors (40-59) */
    NX_ERR_NO_MEMORY = 40,          /**< Out of memory */
    NX_ERR_NO_RESOURCE = 41,        /**< Resource unavailable */
    NX_ERR_RESOURCE_BUSY = 42,      /**< Resource busy */
    NX_ERR_LOCKED = 43,             /**< Resource locked */

    /* Timeout errors (60-79) */
    NX_ERR_TIMEOUT = 60,            /**< Operation timeout */

    /* IO errors (80-99) */
    NX_ERR_IO = 80,                 /**< IO error */
    NX_ERR_OVERRUN = 81,            /**< Buffer overrun */
    NX_ERR_UNDERRUN = 82,           /**< Buffer underrun */
    NX_ERR_PARITY = 83,             /**< Parity error */
    NX_ERR_FRAMING = 84,            /**< Framing error */
    NX_ERR_NOISE = 85,              /**< Noise error */

    /* DMA errors (100-119) */
    NX_ERR_DMA = 100,               /**< DMA error */
    NX_ERR_DMA_TRANSFER = 101,      /**< DMA transfer error */

    /* Data errors (120-139) */
    NX_ERR_NO_DATA = 120,           /**< No data available */
    NX_ERR_DATA_SIZE = 121,         /**< Data size error */
    NX_ERR_CRC = 122,               /**< CRC check error */

    /* Permission errors (140-159) */
    NX_ERR_PERMISSION = 140,        /**< Permission denied */

    NX_ERR_MAX                      /**< Maximum error code value */
} nx_status_t;

/**
 * \brief           Convert status code to string
 * \param[in]       status: Status code
 * \return          String description of the status code
 */
const char *nx_status_to_string(nx_status_t status);

/**
 * \brief           Check if status indicates success
 */
#define NX_IS_OK(status)    ((status) == NX_OK)
#define NX_IS_ERROR(status) ((status) != NX_OK)

/**
 * \brief           Return if error occurred
 */
#define NX_RETURN_IF_ERROR(status) \
    do { \
        nx_status_t __s = (status); \
        if (NX_IS_ERROR(__s)) return __s; \
    } while (0)

/**
 * \brief           Error callback function type
 * \param[in]       user_data: User data pointer
 * \param[in]       status: Status code
 * \param[in]       module: Module name
 * \param[in]       msg: Error message
 */
typedef void (*nx_error_callback_t)(void *user_data, nx_status_t status, 
                                     const char *module, const char *msg);

/**
 * \brief           Set global error callback
 * \param[in]       callback: Callback function
 * \param[in]       user_data: User data pointer
 */
void nx_set_error_callback(nx_error_callback_t callback, void *user_data);
```

### 2. Base Interface Layer (hal/interface/)

```c
/* ===== hal/interface/nx_lifecycle.h ===== */

/**
 * \file            nx_lifecycle.h
 * \brief           Device lifecycle interface
 * \author          Nexus Team
 */

/**
 * \brief           Device state enumeration
 */
typedef enum nx_device_state_e {
    NX_DEV_STATE_UNINITIALIZED = 0,  /**< Device not initialized */
    NX_DEV_STATE_INITIALIZED,         /**< Device initialized */
    NX_DEV_STATE_RUNNING,             /**< Device running */
    NX_DEV_STATE_SUSPENDED,           /**< Device suspended */
    NX_DEV_STATE_ERROR,               /**< Device in error state */
} nx_device_state_t;

/**
 * \brief           Lifecycle interface - all devices must implement
 */
typedef struct nx_lifecycle_s nx_lifecycle_t;
struct nx_lifecycle_s {
    nx_status_t (*init)(nx_lifecycle_t *self);
    nx_status_t (*deinit)(nx_lifecycle_t *self);
    nx_status_t (*suspend)(nx_lifecycle_t *self);
    nx_status_t (*resume)(nx_lifecycle_t *self);
    nx_device_state_t (*get_state)(nx_lifecycle_t *self);
};

/* ===== hal/interface/nx_power.h ===== */

/**
 * \file            nx_power.h
 * \brief           Power management interface
 * \author          Nexus Team
 */

/**
 * \brief           Power management interface
 */
typedef struct nx_power_s nx_power_t;
struct nx_power_s {
    nx_status_t (*enable)(nx_power_t *self);
    nx_status_t (*disable)(nx_power_t *self);
    bool (*is_enabled)(nx_power_t *self);
};

/* ===== hal/interface/nx_configurable.h ===== */

/**
 * \file            nx_configurable.h
 * \brief           Generic configuration interface
 * \author          Nexus Team
 */

/**
 * \brief           Generic configuration interface
 */
typedef struct nx_configurable_s nx_configurable_t;
struct nx_configurable_s {
    nx_status_t (*get_config)(nx_configurable_t *self, void *cfg, size_t size);
    nx_status_t (*set_config)(nx_configurable_t *self, const void *cfg, size_t size);
    nx_status_t (*reset_config)(nx_configurable_t *self);
};

/* ===== hal/interface/nx_diagnostic.h ===== */

/**
 * \file            nx_diagnostic.h
 * \brief           Diagnostic interface
 * \author          Nexus Team
 */

/**
 * \brief           Diagnostic interface
 */
typedef struct nx_diagnostic_s nx_diagnostic_t;
struct nx_diagnostic_s {
    nx_status_t (*get_status)(nx_diagnostic_t *self, void *status, size_t size);
    nx_status_t (*get_statistics)(nx_diagnostic_t *self, void *stats, size_t size);
    nx_status_t (*clear_statistics)(nx_diagnostic_t *self);
};
```

### 3. Device Base Class (hal/base/nx_device.h)

```c
/**
 * \file            nx_device.h
 * \brief           Device base class definition
 * \author          Nexus Team
 */

/**
 * \brief           Device descriptor structure
 */
typedef struct nx_device_s nx_device_t;
struct nx_device_s {
    const char *name;                    /**< Device name */
    const void *default_config;          /**< Default configuration (ROM) */
    void *runtime_config;                /**< Runtime configuration (RAM) */
    size_t config_size;                  /**< Configuration structure size */

    struct {
        uint8_t init_res : 8;            /**< Initialization result */
        bool initialized : 1;            /**< Initialization flag */
        nx_device_state_t state : 3;     /**< Device state */
        uint8_t ref_count : 4;           /**< Reference count (max 15) */
    } state;

    /* Lifecycle function pointers */
    void *(*device_init)(const nx_device_t *dev);
    nx_status_t (*device_deinit)(const nx_device_t *dev);
    nx_status_t (*device_suspend)(const nx_device_t *dev);
    nx_status_t (*device_resume)(const nx_device_t *dev);
};

/**
 * \brief           Get device (increment reference count)
 * \param[in]       name: Device name
 * \return          Device interface pointer, NULL if not found
 */
void *nx_device_get(const char *name);

/**
 * \brief           Release device (decrement reference count)
 * \param[in]       dev_intf: Device interface pointer
 * \return          NX_OK on success
 */
nx_status_t nx_device_put(void *dev_intf);

/**
 * \brief           Find device (does not increment reference count)
 * \param[in]       name: Device name
 * \return          Device descriptor pointer, NULL if not found
 */
const nx_device_t *nx_device_find(const char *name);

/**
 * \brief           Reinitialize device with new configuration
 * \param[in]       dev: Device descriptor
 * \param[in]       new_config: New configuration
 * \return          NX_OK on success
 */
nx_status_t nx_device_reinit(const nx_device_t *dev, const void *new_config);
```

### 4. UART Interface (hal/interface/nx_uart.h)

```c
/**
 * \file            nx_uart.h
 * \brief           UART device interface definition
 * \author          Nexus Team
 */

/**
 * \brief           UART configuration structure
 */
typedef struct nx_uart_config_s {
    uint32_t baudrate;        /**< Baud rate */
    uint8_t word_length;      /**< Word length: 8 or 9 */
    uint8_t stop_bits;        /**< Stop bits: 1 or 2 */
    uint8_t parity;           /**< Parity: 0=none, 1=odd, 2=even */
    uint8_t flow_control;     /**< Flow control: 0=none, 1=rts, 2=cts, 3=rts_cts */
    bool dma_tx_enable;       /**< Enable DMA for TX */
    bool dma_rx_enable;       /**< Enable DMA for RX */
    size_t tx_buf_size;       /**< TX buffer size */
    size_t rx_buf_size;       /**< RX buffer size */
} nx_uart_config_t;

/**
 * \brief           UART statistics structure
 */
typedef struct nx_uart_stats_s {
    bool tx_busy;             /**< TX busy flag */
    bool rx_busy;             /**< RX busy flag */
    uint32_t tx_count;        /**< Total bytes transmitted */
    uint32_t rx_count;        /**< Total bytes received */
    uint32_t tx_errors;       /**< TX error count */
    uint32_t rx_errors;       /**< RX error count */
    uint32_t overrun_errors;  /**< Overrun error count */
    uint32_t framing_errors;  /**< Framing error count */
} nx_uart_stats_t;

/**
 * \brief           Asynchronous transmit interface
 */
typedef struct nx_tx_async_s nx_tx_async_t;
struct nx_tx_async_s {
    nx_status_t (*send)(nx_tx_async_t *self, const uint8_t *data, size_t len);
    size_t (*get_free_space)(nx_tx_async_t *self);
    bool (*is_busy)(nx_tx_async_t *self);
};

/**
 * \brief           Asynchronous receive interface
 */
typedef struct nx_rx_async_s nx_rx_async_t;
struct nx_rx_async_s {
    size_t (*read)(nx_rx_async_t *self, uint8_t *data, size_t max_len);
    size_t (*available)(nx_rx_async_t *self);
    nx_status_t (*set_callback)(nx_rx_async_t *self, void (*cb)(void *), void *ctx);
};

/**
 * \brief           Synchronous transmit interface
 */
typedef struct nx_tx_sync_s nx_tx_sync_t;
struct nx_tx_sync_s {
    nx_status_t (*send)(nx_tx_sync_t *self, const uint8_t *data, size_t len, 
                        uint32_t timeout_ms);
};

/**
 * \brief           Synchronous receive interface
 */
typedef struct nx_rx_sync_s nx_rx_sync_t;
struct nx_rx_sync_s {
    nx_status_t (*receive)(nx_rx_sync_t *self, uint8_t *data, size_t len,
                           uint32_t timeout_ms);
};

/**
 * \brief           UART device interface
 */
typedef struct nx_uart_s nx_uart_t;
struct nx_uart_s {
    /* Operation interfaces */
    nx_tx_async_t *(*get_tx_async)(nx_uart_t *self);
    nx_rx_async_t *(*get_rx_async)(nx_uart_t *self);
    nx_tx_sync_t *(*get_tx_sync)(nx_uart_t *self);
    nx_rx_sync_t *(*get_rx_sync)(nx_uart_t *self);

    /* Runtime configuration */
    nx_status_t (*set_baudrate)(nx_uart_t *self, uint32_t baudrate);
    nx_status_t (*get_config)(nx_uart_t *self, nx_uart_config_t *cfg);
    nx_status_t (*set_config)(nx_uart_t *self, const nx_uart_config_t *cfg);

    /* Base interfaces */
    nx_lifecycle_t *(*get_lifecycle)(nx_uart_t *self);
    nx_power_t *(*get_power)(nx_uart_t *self);
    nx_diagnostic_t *(*get_diagnostic)(nx_uart_t *self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_uart_t *self, nx_uart_stats_t *stats);
    nx_status_t (*clear_errors)(nx_uart_t *self);
};
```

### 5. GPIO Interface (hal/interface/nx_gpio.h)

```c
/**
 * \file            nx_gpio.h
 * \brief           GPIO device interface definition
 * \author          Nexus Team
 */

/**
 * \brief           GPIO mode enumeration
 */
typedef enum nx_gpio_mode_e {
    NX_GPIO_MODE_INPUT = 0,       /**< Input mode */
    NX_GPIO_MODE_OUTPUT_PP,       /**< Output push-pull */
    NX_GPIO_MODE_OUTPUT_OD,       /**< Output open-drain */
    NX_GPIO_MODE_AF_PP,           /**< Alternate function push-pull */
    NX_GPIO_MODE_AF_OD,           /**< Alternate function open-drain */
    NX_GPIO_MODE_ANALOG,          /**< Analog mode */
} nx_gpio_mode_t;

/**
 * \brief           GPIO pull-up/pull-down enumeration
 */
typedef enum nx_gpio_pull_e {
    NX_GPIO_PULL_NONE = 0,        /**< No pull-up/pull-down */
    NX_GPIO_PULL_UP,              /**< Pull-up */
    NX_GPIO_PULL_DOWN,            /**< Pull-down */
} nx_gpio_pull_t;

/**
 * \brief           GPIO speed enumeration
 */
typedef enum nx_gpio_speed_e {
    NX_GPIO_SPEED_LOW = 0,        /**< Low speed */
    NX_GPIO_SPEED_MEDIUM,         /**< Medium speed */
    NX_GPIO_SPEED_HIGH,           /**< High speed */
    NX_GPIO_SPEED_VERY_HIGH,      /**< Very high speed */
} nx_gpio_speed_t;

/**
 * \brief           GPIO external interrupt trigger enumeration
 */
typedef enum nx_gpio_exti_trig_e {
    NX_GPIO_EXTI_NONE = 0,        /**< No interrupt */
    NX_GPIO_EXTI_RISING,          /**< Rising edge trigger */
    NX_GPIO_EXTI_FALLING,         /**< Falling edge trigger */
    NX_GPIO_EXTI_BOTH,            /**< Both edges trigger */
} nx_gpio_exti_trig_t;

/**
 * \brief           GPIO configuration structure
 */
typedef struct nx_gpio_config_s {
    nx_gpio_mode_t mode;          /**< GPIO mode */
    nx_gpio_pull_t pull;          /**< Pull-up/pull-down configuration */
    nx_gpio_speed_t speed;        /**< GPIO speed */
    uint8_t af_index;             /**< Alternate function index */
} nx_gpio_config_t;

/**
 * \brief           GPIO external interrupt callback type
 * \param[in]       context: User context pointer
 */
typedef void (*nx_gpio_exti_callback_t)(void *context);

/**
 * \brief           GPIO device interface
 */
typedef struct nx_gpio_s nx_gpio_t;
struct nx_gpio_s {
    /* Basic operations */
    uint8_t (*read)(nx_gpio_t *self);
    void (*write)(nx_gpio_t *self, uint8_t state);
    void (*toggle)(nx_gpio_t *self);

    /* Runtime configuration */
    nx_status_t (*set_mode)(nx_gpio_t *self, nx_gpio_mode_t mode);
    nx_status_t (*set_pull)(nx_gpio_t *self, nx_gpio_pull_t pull);
    nx_status_t (*get_config)(nx_gpio_t *self, nx_gpio_config_t *cfg);
    nx_status_t (*set_config)(nx_gpio_t *self, const nx_gpio_config_t *cfg);

    /* Interrupt configuration */
    nx_status_t (*set_exti)(nx_gpio_t *self, nx_gpio_exti_trig_t trig, 
                            nx_gpio_exti_callback_t cb, void *ctx);
    nx_status_t (*clear_exti)(nx_gpio_t *self);

    /* Base interfaces */
    nx_lifecycle_t *(*get_lifecycle)(nx_gpio_t *self);
    nx_power_t *(*get_power)(nx_gpio_t *self);
};
```

### 6. SPI Interface (hal/interface/nx_spi.h)

```c
/**
 * \file            nx_spi.h
 * \brief           SPI device interface definition
 * \author          Nexus Team
 */

/**
 * \brief           SPI mode enumeration
 */
typedef enum nx_spi_mode_e {
    NX_SPI_MODE_0 = 0,            /**< CPOL=0, CPHA=0 */
    NX_SPI_MODE_1,                /**< CPOL=0, CPHA=1 */
    NX_SPI_MODE_2,                /**< CPOL=1, CPHA=0 */
    NX_SPI_MODE_3,                /**< CPOL=1, CPHA=1 */
} nx_spi_mode_t;

/**
 * \brief           SPI configuration structure
 */
typedef struct nx_spi_config_s {
    uint32_t clock_hz;            /**< Clock frequency in Hz */
    nx_spi_mode_t mode;           /**< SPI mode */
    uint8_t bits;                 /**< Data bits: 8 or 16 */
    bool msb_first;               /**< MSB first flag */
    uint32_t cs_delay_us;         /**< CS delay in microseconds */
} nx_spi_config_t;

/**
 * \brief           SPI statistics structure
 */
typedef struct nx_spi_stats_s {
    bool busy;                    /**< Busy flag */
    uint32_t tx_count;            /**< Total bytes transmitted */
    uint32_t rx_count;            /**< Total bytes received */
    uint32_t error_count;         /**< Error count */
} nx_spi_stats_t;

/**
 * \brief           SPI bus interface
 */
typedef struct nx_spi_s nx_spi_t;
struct nx_spi_s {
    /* Synchronous transfer */
    nx_status_t (*transfer)(nx_spi_t *self, const uint8_t *tx, uint8_t *rx, 
                            size_t len, uint32_t timeout_ms);
    nx_status_t (*transmit)(nx_spi_t *self, const uint8_t *tx, size_t len,
                            uint32_t timeout_ms);
    nx_status_t (*receive)(nx_spi_t *self, uint8_t *rx, size_t len,
                           uint32_t timeout_ms);

    /* CS control */
    nx_status_t (*cs_select)(nx_spi_t *self);
    nx_status_t (*cs_deselect)(nx_spi_t *self);

    /* Bus lock */
    nx_status_t (*lock)(nx_spi_t *self, uint32_t timeout_ms);
    nx_status_t (*unlock)(nx_spi_t *self);

    /* Runtime configuration */
    nx_status_t (*set_clock)(nx_spi_t *self, uint32_t clock_hz);
    nx_status_t (*set_mode)(nx_spi_t *self, nx_spi_mode_t mode);
    nx_status_t (*get_config)(nx_spi_t *self, nx_spi_config_t *cfg);
    nx_status_t (*set_config)(nx_spi_t *self, const nx_spi_config_t *cfg);

    /* Base interfaces */
    nx_lifecycle_t *(*get_lifecycle)(nx_spi_t *self);
    nx_power_t *(*get_power)(nx_spi_t *self);
    nx_diagnostic_t *(*get_diagnostic)(nx_spi_t *self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_spi_t *self, nx_spi_stats_t *stats);
};
```

### 7. I2C Interface (hal/interface/nx_i2c.h)

```c
/**
 * \file            nx_i2c.h
 * \brief           I2C device interface definition
 * \author          Nexus Team
 */

/**
 * \brief           I2C speed enumeration
 */
typedef enum nx_i2c_speed_e {
    NX_I2C_SPEED_STANDARD = 0,    /**< Standard mode: 100 kHz */
    NX_I2C_SPEED_FAST,            /**< Fast mode: 400 kHz */
    NX_I2C_SPEED_FAST_PLUS,       /**< Fast mode plus: 1 MHz */
} nx_i2c_speed_t;

/**
 * \brief           I2C configuration structure
 */
typedef struct nx_i2c_config_s {
    nx_i2c_speed_t speed;         /**< I2C speed */
    uint16_t own_addr;            /**< Own address (slave mode) */
    bool addr_10bit;              /**< 10-bit address mode flag */
} nx_i2c_config_t;

/**
 * \brief           I2C statistics structure
 */
typedef struct nx_i2c_stats_s {
    bool busy;                    /**< Busy flag */
    uint32_t tx_count;            /**< Total bytes transmitted */
    uint32_t rx_count;            /**< Total bytes received */
    uint32_t nack_count;          /**< NACK count */
    uint32_t bus_error_count;     /**< Bus error count */
} nx_i2c_stats_t;

/**
 * \brief           I2C bus interface
 */
typedef struct nx_i2c_s nx_i2c_t;
struct nx_i2c_s {
    /* Master transfer */
    nx_status_t (*master_transmit)(nx_i2c_t *self, uint16_t addr, 
                                   const uint8_t *data, size_t len,
                                   uint32_t timeout_ms);
    nx_status_t (*master_receive)(nx_i2c_t *self, uint16_t addr,
                                  uint8_t *data, size_t len,
                                  uint32_t timeout_ms);
    
    /* Memory read/write */
    nx_status_t (*mem_write)(nx_i2c_t *self, uint16_t addr, uint16_t mem_addr,
                             uint8_t mem_addr_size, const uint8_t *data,
                             size_t len, uint32_t timeout_ms);
    nx_status_t (*mem_read)(nx_i2c_t *self, uint16_t addr, uint16_t mem_addr,
                            uint8_t mem_addr_size, uint8_t *data,
                            size_t len, uint32_t timeout_ms);

    /* Device detection */
    nx_status_t (*probe)(nx_i2c_t *self, uint16_t addr, uint32_t timeout_ms);
    nx_status_t (*scan)(nx_i2c_t *self, uint8_t *addr_list, size_t max, size_t *found);

    /* Runtime configuration */
    nx_status_t (*set_speed)(nx_i2c_t *self, nx_i2c_speed_t speed);
    nx_status_t (*get_config)(nx_i2c_t *self, nx_i2c_config_t *cfg);
    nx_status_t (*set_config)(nx_i2c_t *self, const nx_i2c_config_t *cfg);

    /* Base interfaces */
    nx_lifecycle_t *(*get_lifecycle)(nx_i2c_t *self);
    nx_power_t *(*get_power)(nx_i2c_t *self);
    nx_diagnostic_t *(*get_diagnostic)(nx_i2c_t *self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_i2c_t *self, nx_i2c_stats_t *stats);
};
```

### 8. DMA Manager (hal/resource/nx_dma_manager.h)

```c
/**
 * \file            nx_dma_manager.h
 * \brief           DMA resource manager interface
 * \author          Nexus Team
 */

/**
 * \brief           DMA channel handle (opaque)
 */
typedef struct nx_dma_channel_s nx_dma_channel_t;

/**
 * \brief           DMA transfer direction enumeration
 */
typedef enum nx_dma_direction_e {
    NX_DMA_DIR_PERIPH_TO_MEM = 0, /**< Peripheral to memory */
    NX_DMA_DIR_MEM_TO_PERIPH,     /**< Memory to peripheral */
    NX_DMA_DIR_MEM_TO_MEM,        /**< Memory to memory */
} nx_dma_direction_t;

/**
 * \brief           DMA transfer complete callback type
 * \param[in]       user_data: User data pointer
 * \param[in]       result: Transfer result status
 */
typedef void (*nx_dma_callback_t)(void *user_data, nx_status_t result);

/**
 * \brief           DMA request configuration structure
 */
typedef struct nx_dma_request_s {
    uint32_t periph_addr;         /**< Peripheral address */
    uint32_t memory_addr;         /**< Memory address */
    uint32_t transfer_count;      /**< Transfer count */
    uint8_t periph_width;         /**< Peripheral data width: 8/16/32 */
    uint8_t memory_width;         /**< Memory data width: 8/16/32 */
    bool periph_inc;              /**< Peripheral address increment */
    bool memory_inc;              /**< Memory address increment */
    bool circular;                /**< Circular mode flag */
    uint8_t priority;             /**< Priority: 0-3 */
    nx_dma_callback_t callback;   /**< Completion callback */
    void *user_data;              /**< User data for callback */
} nx_dma_request_t;

/**
 * \brief           DMA manager interface
 */
typedef struct nx_dma_manager_s nx_dma_manager_t;
struct nx_dma_manager_s {
    nx_dma_channel_t *(*alloc)(nx_dma_manager_t *self, uint32_t periph);
    nx_status_t (*free)(nx_dma_manager_t *self, nx_dma_channel_t *ch);
    nx_status_t (*start)(nx_dma_channel_t *ch, const nx_dma_request_t *req);
    nx_status_t (*stop)(nx_dma_channel_t *ch);
    uint32_t (*get_remaining)(nx_dma_channel_t *ch);
};

/**
 * \brief           Get DMA manager singleton instance
 * \return          DMA manager pointer
 */
nx_dma_manager_t *nx_dma_manager_get(void);
```

### 9. ISR Manager (hal/resource/nx_isr_manager.h)

```c
/**
 * \file            nx_isr_manager.h
 * \brief           Interrupt service routine manager interface
 * \author          Nexus Team
 */

/**
 * \brief           ISR callback priority enumeration
 */
typedef enum nx_isr_priority_e {
    NX_ISR_PRIORITY_HIGHEST = 0,  /**< Highest priority */
    NX_ISR_PRIORITY_HIGH = 1,     /**< High priority */
    NX_ISR_PRIORITY_NORMAL = 2,   /**< Normal priority */
    NX_ISR_PRIORITY_LOW = 3,      /**< Low priority */
} nx_isr_priority_t;

/**
 * \brief           ISR callback handle (opaque)
 */
typedef struct nx_isr_handle_s nx_isr_handle_t;

/**
 * \brief           ISR callback function type
 * \param[in]       data: User data pointer
 */
typedef void (*nx_isr_func_t)(void *data);

/**
 * \brief           ISR manager interface
 */
typedef struct nx_isr_manager_s nx_isr_manager_t;
struct nx_isr_manager_s {
    nx_isr_handle_t *(*connect)(nx_isr_manager_t *self, uint32_t irq,
                                 nx_isr_func_t func, void *data, 
                                 nx_isr_priority_t priority);
    nx_status_t (*disconnect)(nx_isr_manager_t *self, nx_isr_handle_t *handle);
    nx_status_t (*set_hw_priority)(nx_isr_manager_t *self, uint32_t irq, uint8_t hw_prio);
    nx_status_t (*enable)(nx_isr_manager_t *self, uint32_t irq);
    nx_status_t (*disable)(nx_isr_manager_t *self, uint32_t irq);
};

/**
 * \brief           Get ISR manager singleton instance
 * \return          ISR manager pointer
 */
nx_isr_manager_t *nx_isr_manager_get(void);
```

### 10. Factory Layer (hal/nx_factory.h)

```c
/**
 * \file            nx_factory.h
 * \brief           Device factory interface
 * \author          Nexus Team
 */

/**
 * \brief           Device information structure
 */
typedef struct nx_device_info_s {
    const char *name;             /**< Device name */
    const char *type;             /**< Device type */
    nx_device_state_t state;      /**< Device state */
    uint8_t ref_count;            /**< Reference count */
} nx_device_info_t;

/**
 * \brief           Get GPIO device
 * \param[in]       port: GPIO port number
 * \param[in]       pin: GPIO pin number
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t *nx_factory_gpio(uint8_t port, uint8_t pin);

/**
 * \brief           Get GPIO device with configuration
 * \param[in]       port: GPIO port number
 * \param[in]       pin: GPIO pin number
 * \param[in]       cfg: GPIO configuration
 * \return          GPIO interface pointer, NULL on failure
 */
nx_gpio_t *nx_factory_gpio_with_config(uint8_t port, uint8_t pin, 
                                        const nx_gpio_config_t *cfg);

/**
 * \brief           Release GPIO device
 * \param[in]       gpio: GPIO interface pointer
 */
void nx_factory_gpio_release(nx_gpio_t *gpio);

/**
 * \brief           Get UART device
 * \param[in]       index: UART index
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t *nx_factory_uart(uint8_t index);

/**
 * \brief           Get UART device with configuration
 * \param[in]       index: UART index
 * \param[in]       cfg: UART configuration
 * \return          UART interface pointer, NULL on failure
 */
nx_uart_t *nx_factory_uart_with_config(uint8_t index, const nx_uart_config_t *cfg);

/**
 * \brief           Release UART device
 * \param[in]       uart: UART interface pointer
 */
void nx_factory_uart_release(nx_uart_t *uart);

/**
 * \brief           Get SPI device
 * \param[in]       index: SPI index
 * \return          SPI interface pointer, NULL on failure
 */
nx_spi_t *nx_factory_spi(uint8_t index);

/**
 * \brief           Get SPI device with configuration
 * \param[in]       index: SPI index
 * \param[in]       cfg: SPI configuration
 * \return          SPI interface pointer, NULL on failure
 */
nx_spi_t *nx_factory_spi_with_config(uint8_t index, const nx_spi_config_t *cfg);

/**
 * \brief           Release SPI device
 * \param[in]       spi: SPI interface pointer
 */
void nx_factory_spi_release(nx_spi_t *spi);

/**
 * \brief           Get I2C device
 * \param[in]       index: I2C index
 * \return          I2C interface pointer, NULL on failure
 */
nx_i2c_t *nx_factory_i2c(uint8_t index);

/**
 * \brief           Get I2C device with configuration
 * \param[in]       index: I2C index
 * \param[in]       cfg: I2C configuration
 * \return          I2C interface pointer, NULL on failure
 */
nx_i2c_t *nx_factory_i2c_with_config(uint8_t index, const nx_i2c_config_t *cfg);

/**
 * \brief           Release I2C device
 * \param[in]       i2c: I2C interface pointer
 */
void nx_factory_i2c_release(nx_i2c_t *i2c);

/**
 * \brief           Enumerate all registered devices
 * \param[out]      list: Output device info list
 * \param[in]       max_count: Maximum number of devices to enumerate
 * \return          Number of devices enumerated
 */
size_t nx_factory_enumerate(nx_device_info_t *list, size_t max_count);
```

## Data Models

### Device State Machine

```
                    ┌─────────────────┐
                    │  UNINITIALIZED  │
                    └────────┬────────┘
                             │ init()
                             ▼
                    ┌─────────────────┐
         ┌─────────│   INITIALIZED   │─────────┐
         │         └────────┬────────┘         │
         │                  │ start()          │
         │                  ▼                  │
         │         ┌─────────────────┐         │
         │    ┌────│    RUNNING      │────┐    │
         │    │    └────────┬────────┘    │    │
         │    │             │             │    │
         │ suspend()        │ error    resume()│
         │    │             ▼             │    │
         │    │    ┌─────────────────┐    │    │
         │    └───▶│   SUSPENDED     │────┘    │
         │         └─────────────────┘         │
         │                                     │
         │ deinit()                   deinit() │
         │                                     │
         └──────────────────┬──────────────────┘
                            ▼
                   ┌─────────────────┐
                   │  UNINITIALIZED  │
                   └─────────────────┘
```

### Reference Count Model

```c
/**
 * \brief           Device get flow (increment reference count)
 * \param[in]       name: Device name
 * \return          Device interface pointer, NULL if not found
 */
void *nx_device_get(const char *name) {
    const nx_device_t *dev = nx_device_find(name);
    if (!dev) return NULL;

    nx_enter_critical();

    if (dev->state.ref_count == 0) {
        /* First acquisition, perform initialization */
        void *intf = dev->device_init(dev);
        if (!intf) {
            nx_exit_critical();
            return NULL;
        }
        dev->state.initialized = true;
    }

    if (dev->state.ref_count < 15) {
        dev->state.ref_count++;
    }

    nx_exit_critical();
    return /* interface pointer */;
}

/**
 * \brief           Device put flow (decrement reference count)
 * \param[in]       dev_intf: Device interface pointer
 * \return          NX_OK on success
 */
nx_status_t nx_device_put(void *dev_intf) {
    nx_device_t *dev = /* get device from interface */;

    nx_enter_critical();

    if (dev->state.ref_count > 0) {
        dev->state.ref_count--;

        if (dev->state.ref_count == 0) {
            /* Last reference, perform deinitialization */
            if (dev->device_deinit) {
                dev->device_deinit(dev);
            }
            dev->state.initialized = false;
        }
    }

    nx_exit_critical();
    return NX_OK;
}
```

## Directory Structure

```
hal/
├── include/
│   └── hal/
│       ├── nx_hal.h                 # Main header file
│       ├── nx_status.h              # Status/error codes (avoid conflict with Linux error.h)
│       ├── nx_types.h               # Basic type definitions
│       ├── interface/
│       │   ├── nx_lifecycle.h       # Lifecycle interface
│       │   ├── nx_power.h           # Power management interface
│       │   ├── nx_configurable.h    # Configuration interface
│       │   ├── nx_diagnostic.h      # Diagnostic interface
│       │   ├── nx_uart.h            # UART interface
│       │   ├── nx_gpio.h            # GPIO interface
│       │   ├── nx_spi.h             # SPI interface
│       │   ├── nx_i2c.h             # I2C interface
│       │   ├── nx_timer.h           # Timer interface
│       │   └── nx_adc.h             # ADC interface
│       ├── base/
│       │   └── nx_device.h          # Device base class
│       ├── resource/
│       │   ├── nx_dma_manager.h     # DMA manager
│       │   └── nx_isr_manager.h     # ISR manager
│       └── nx_factory.h             # Factory interface
├── src/
│   ├── nx_status.c                  # Status code implementation
│   ├── nx_device.c                  # Device base class implementation
│   └── nx_factory.c                 # Factory common implementation
└── CMakeLists.txt

platforms/
├── stm32f4/
│   ├── include/
│   │   └── platform/
│   │       └── stm32f4_hal.h        # Platform specific header
│   ├── src/
│   │   ├── nx_uart_stm32f4.c        # UART driver
│   │   ├── nx_gpio_stm32f4.c        # GPIO driver
│   │   ├── nx_spi_stm32f4.c         # SPI driver
│   │   ├── nx_i2c_stm32f4.c         # I2C driver
│   │   ├── nx_dma_stm32f4.c         # DMA manager
│   │   ├── nx_isr_stm32f4.c         # ISR manager
│   │   └── nx_factory_stm32f4.c     # Factory implementation
│   └── CMakeLists.txt
└── native/
    ├── src/
    │   ├── nx_uart_native.c
    │   ├── nx_gpio_native.c
    │   └── ...
    └── CMakeLists.txt
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system.*

### Property 1: 引用计数一致性

*For any* 设备实例，获取次数减去释放次数应等于当前引用计数值。

**Validates: Requirements 3.1, 3.4**

### Property 2: 配置往返一致性

*For any* 有效的配置结构体，调用 `set_config()` 后立即调用 `get_config()` 应返回等价的配置值。

**Validates: Requirements 4.5, 4.6**

### Property 3: 状态机转换合法性

*For any* 设备状态转换，只能按照状态机定义的合法路径进行转换。

**Validates: Requirements 2.1, 2.4, 2.5**

### Property 4: 资源释放完整性

*For any* 被反初始化的设备，其占用的所有资源（DMA通道、中断、内存）都应被正确释放。

**Validates: Requirements 2.1, 7.2**

### Property 5: 错误码一致性

*For any* HAL 接口调用，返回值类型应为 `nx_status_t` 且值在定义范围内。

**Validates: Requirements 1.1, 1.2**

### Property 6: DMA 通道唯一性

*For any* 时刻，每个 DMA 通道最多被一个设备占用。

**Validates: Requirements 7.1, 7.3**

### Property 7: 中断回调链完整性

*For any* 中断事件，所有已注册的回调函数都应按优先级顺序被调用。

**Validates: Requirements 6.1, 6.3, 6.4**

## Error Handling

### Error Handling Strategy

| Error Type | Handling | Return Value |
|------------|----------|--------------|
| Invalid parameter | Return immediately | NX_ERR_INVALID_PARAM |
| Null pointer | Return immediately | NX_ERR_NULL_PTR |
| Device busy | Return immediately | NX_ERR_BUSY |
| Timeout | Return processed data count | NX_ERR_TIMEOUT |
| Resource unavailable | Return immediately | NX_ERR_NO_RESOURCE |
| Hardware error | Log error count, return | NX_ERR_IO |
| Fatal error | Trigger error callback, optional reset | NX_ERR_GENERIC |

## Testing Strategy

### Unit Tests

1. **Interface Tests**: Verify basic functionality of each interface method
2. **Boundary Tests**: Test parameter boundary values and exceptional inputs
3. **State Tests**: Verify correctness of state machine transitions

### Property Tests

1. **Reference Count Tests**: Random get/put sequences, verify count consistency
2. **Configuration Round-trip Tests**: Random config values, verify set/get consistency
3. **Concurrency Tests**: Multi-threaded access, verify thread safety

### Integration Tests

1. **Device Lifecycle Tests**: Complete init/deinit/reinit flow
2. **Resource Management Tests**: DMA/ISR allocation and release
3. **Low Power Tests**: suspend/resume flow

### Test Framework

- Use Unity as unit test framework
- Property tests use Hypothesis (Python) to generate test cases
- Native platform for rapid iteration testing
