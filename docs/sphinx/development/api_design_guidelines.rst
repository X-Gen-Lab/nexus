API Design Guidelines
=====================

This comprehensive guide covers API design principles, patterns, and best practices for the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Well-designed APIs are crucial for a successful embedded platform. This guide provides detailed guidelines for:

* API design principles
* Naming conventions
* Function signatures
* Error handling
* Memory management
* Thread safety
* Documentation requirements
* Versioning and compatibility

Good API design makes the platform:

* **Easy to use**: Intuitive and consistent
* **Hard to misuse**: Type-safe and well-documented
* **Maintainable**: Clear and well-structured
* **Extensible**: Easy to add new features
* **Portable**: Works across platforms

Design Principles
-----------------

Fundamental Principles
~~~~~~~~~~~~~~~~~~~~~~

**1. Consistency**

Maintain consistency across all APIs:

* Naming conventions
* Parameter ordering
* Return value conventions
* Error handling patterns
* Documentation style

**Example - Consistent Naming**:

.. code-block:: c

   /* Good: Consistent naming pattern */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config);
   hal_status_t hal_spi_init(hal_spi_id_t id, const hal_spi_config_t* config);

   /* Bad: Inconsistent naming */
   hal_status_t gpio_initialize(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t InitUART(hal_uart_id_t id, const hal_uart_config_t* config);
   hal_status_t spi_setup(hal_spi_id_t id, const hal_spi_config_t* config);

**2. Simplicity**

Keep APIs simple and focused:

* One function, one purpose
* Minimal parameters
* Clear semantics
* No hidden behavior

**Example - Simple vs Complex**:

.. code-block:: c

   /* Good: Simple, focused function */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level);

   /* Bad: Too many responsibilities */
   hal_status_t hal_gpio_write_with_delay_and_toggle(
       hal_gpio_port_t port,
       uint8_t pin,
       hal_gpio_level_t level,
       uint32_t delay_ms,
       bool toggle_after,
       uint32_t toggle_count
   );

**3. Orthogonality**

Functions should be independent:

* No unexpected side effects
* No hidden dependencies
* Composable operations
* Predictable behavior

**Example - Orthogonal Design**:

.. code-block:: c

   /* Good: Independent operations */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level);
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t* level);

   /* Bad: Coupled operations */
   hal_status_t hal_gpio_init_and_write(hal_gpio_port_t port, uint8_t pin,
                                       const hal_gpio_config_t* config,
                                       hal_gpio_level_t initial_level);

**4. Discoverability**

Make APIs easy to discover:

* Logical grouping
* Clear naming
* Comprehensive documentation
* Examples and tutorials

**5. Safety**

Design for safety:

* Type safety
* Null pointer checks
* Bounds checking
* Resource management
* Error handling


Naming Conventions
------------------

Module Prefix
~~~~~~~~~~~~~

All public APIs must use module prefix:

**Format**: ``<module>_<component>_<action>``

**Examples**:

.. code-block:: c

   /* HAL module */
   hal_gpio_init()
   hal_uart_send()
   hal_spi_transfer()

   /* OSAL module */
   osal_task_create()
   osal_mutex_lock()
   osal_queue_send()

   /* Framework modules */
   log_init()
   shell_register_command()
   config_set_i32()

Function Names
~~~~~~~~~~~~~~

**Format**: ``<module>_<noun>_<verb>`` or ``<module>_<verb>_<noun>``

**Verbs**:

* ``init`` / ``deinit`` - Initialize / deinitialize
* ``create`` / ``destroy`` - Create / destroy object
* ``open`` / ``close`` - Open / close resource
* ``start`` / ``stop`` - Start / stop operation
* ``enable`` / ``disable`` - Enable / disable feature
* ``set`` / ``get`` - Set / get value
* ``read`` / ``write`` - Read / write data
* ``send`` / ``receive`` - Send / receive message
* ``lock`` / ``unlock`` - Lock / unlock mutex
* ``wait`` / ``signal`` - Wait / signal event

**Examples**:

.. code-block:: c

   /* Initialization */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t hal_gpio_deinit(hal_gpio_port_t port, uint8_t pin);

   /* Configuration */
   hal_status_t hal_gpio_set_mode(hal_gpio_port_t port, uint8_t pin, hal_gpio_mode_t mode);
   hal_status_t hal_gpio_get_mode(hal_gpio_port_t port, uint8_t pin, hal_gpio_mode_t* mode);

   /* Operations */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level);
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t* level);
   hal_status_t hal_gpio_toggle(hal_gpio_port_t port, uint8_t pin);

Type Names
~~~~~~~~~~

**Format**: ``<module>_<name>_t``

**Examples**:

.. code-block:: c

   /* Status types */
   typedef enum {
       HAL_OK = 0,
       HAL_ERROR,
       HAL_ERROR_PARAM,
   } hal_status_t;

   /* Configuration structures */
   typedef struct {
       hal_gpio_mode_t mode;
       hal_gpio_pull_t pull;
       hal_gpio_speed_t speed;
   } hal_gpio_config_t;

   /* Handle types */
   typedef void* osal_task_handle_t;
   typedef void* osal_mutex_handle_t;

Enumeration Values
~~~~~~~~~~~~~~~~~~

**Format**: ``<MODULE>_<TYPE>_<VALUE>``

**Examples**:

.. code-block:: c

   typedef enum {
       HAL_GPIO_MODE_INPUT = 0,
       HAL_GPIO_MODE_OUTPUT_PP,
       HAL_GPIO_MODE_OUTPUT_OD,
       HAL_GPIO_MODE_AF_PP,
       HAL_GPIO_MODE_AF_OD,
   } hal_gpio_mode_t;

   typedef enum {
       HAL_GPIO_PULL_NONE = 0,
       HAL_GPIO_PULL_UP,
       HAL_GPIO_PULL_DOWN,
   } hal_gpio_pull_t;

Macro Names
~~~~~~~~~~~

**Format**: ``<MODULE>_<NAME>``

**Examples**:

.. code-block:: c

   /* Constants */
   #define HAL_GPIO_PORT_MAX    16
   #define HAL_GPIO_PIN_MAX     16
   #define HAL_UART_BAUDRATE_MAX 921600

   /* Configuration macros */
   #define HAL_TIMEOUT_DEFAULT  1000
   #define HAL_TIMEOUT_INFINITE 0xFFFFFFFF


Function Signatures
-------------------

Parameter Order
~~~~~~~~~~~~~~~

**Standard Order**:

1. **Handle/Context**: Object or context pointer
2. **Input Parameters**: Data to be processed
3. **Output Parameters**: Results (pointers)
4. **Options/Flags**: Optional parameters

**Examples**:

.. code-block:: c

   /* Good: Consistent parameter order */
   hal_status_t hal_uart_send(
       hal_uart_id_t id,              /* 1. Handle */
       const uint8_t* data,            /* 2. Input */
       size_t length,                  /* 2. Input */
       uint32_t timeout                /* 4. Options */
   );

   hal_status_t hal_uart_receive(
       hal_uart_id_t id,              /* 1. Handle */
       uint8_t* buffer,                /* 3. Output */
       size_t buffer_size,             /* 2. Input */
       size_t* received_length,        /* 3. Output */
       uint32_t timeout                /* 4. Options */
   );

   /* Bad: Inconsistent parameter order */
   hal_status_t hal_uart_send(
       const uint8_t* data,
       hal_uart_id_t id,
       uint32_t timeout,
       size_t length
   );

Return Values
~~~~~~~~~~~~~

**Always Return Status**:

All functions that can fail must return a status code:

.. code-block:: c

   /* Good: Returns status */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);

   /* Bad: No error indication */
   void hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);

**Status Code Convention**:

.. code-block:: c

   typedef enum {
       HAL_OK = 0,                /* Success - always 0 */
       HAL_ERROR,                 /* General error */
       HAL_ERROR_PARAM,           /* Invalid parameter */
       HAL_ERROR_STATE,           /* Invalid state */
       HAL_ERROR_TIMEOUT,         /* Operation timeout */
       HAL_ERROR_NO_MEMORY,       /* Out of memory */
       HAL_ERROR_NOT_SUPPORTED,   /* Not supported */
       HAL_ERROR_BUSY,            /* Resource busy */
   } hal_status_t;

**Output Parameters**:

Use output parameters for returning data:

.. code-block:: c

   /* Good: Status return, data via output parameter */
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t* level);

   /* Bad: Data return, no error indication */
   hal_gpio_level_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin);

Input Parameters
~~~~~~~~~~~~~~~~

**Const Correctness**:

Mark input parameters as ``const``:

.. code-block:: c

   /* Good: Const input parameters */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data, size_t length, uint32_t timeout);
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);

   /* Bad: Missing const */
   hal_status_t hal_uart_send(hal_uart_id_t id, uint8_t* data, size_t length, uint32_t timeout);

**Pointer Parameters**:

Always validate pointer parameters:

.. code-block:: c

   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t* level) {
       /* Validate pointer */
       if (level == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Implementation */
       *level = /* read value */;
       return HAL_OK;
   }

**Array Parameters**:

Always include size parameter:

.. code-block:: c

   /* Good: Size parameter included */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data, size_t length, uint32_t timeout);

   /* Bad: No size parameter */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data, uint32_t timeout);

Output Parameters
~~~~~~~~~~~~~~~~~

**Pointer Convention**:

Output parameters must be pointers:

.. code-block:: c

   /* Good: Output via pointer */
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t* level);
   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer, size_t buffer_size,
                                 size_t* received_length, uint32_t timeout);

**Validation**:

Always validate output pointers:

.. code-block:: c

   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer, size_t buffer_size,
                                 size_t* received_length, uint32_t timeout) {
       /* Validate output pointers */
       if (buffer == NULL || received_length == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Validate buffer size */
       if (buffer_size == 0) {
           return HAL_ERROR_PARAM;
       }

       /* Implementation */
       *received_length = /* actual received */;
       return HAL_OK;
   }

Optional Parameters
~~~~~~~~~~~~~~~~~~~

**Use NULL for Optional Pointers**:

.. code-block:: c

   /**
    * \brief           Initialize UART with optional configuration
    * \param[in]       id: UART instance ID
    * \param[in]       config: Configuration (NULL for default)
    * \return          HAL_OK on success
    */
   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config) {
       hal_uart_config_t default_config;

       /* Use default if config is NULL */
       if (config == NULL) {
           default_config.baudrate = 115200;
           default_config.wordlen = HAL_UART_WORDLEN_8;
           default_config.stopbits = HAL_UART_STOPBITS_1;
           default_config.parity = HAL_UART_PARITY_NONE;
           config = &default_config;
       }

       /* Implementation */
       return HAL_OK;
   }

**Use Special Values for Optional Integers**:

.. code-block:: c

   /* Use 0 or MAX value for "default" or "infinite" */
   #define HAL_TIMEOUT_DEFAULT  0
   #define HAL_TIMEOUT_INFINITE 0xFFFFFFFF

   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data, size_t length, uint32_t timeout);


Data Structures
---------------

Structure Design
~~~~~~~~~~~~~~~~

**Configuration Structures**:

Use structures for configuration:

.. code-block:: c

   /**
    * \brief           GPIO configuration structure
    */
   typedef struct {
       hal_gpio_mode_t mode;          /**< GPIO mode */
       hal_gpio_pull_t pull;          /**< Pull-up/down configuration */
       hal_gpio_speed_t speed;        /**< Output speed */
       hal_gpio_level_t init_level;   /**< Initial output level */
   } hal_gpio_config_t;

   /* Usage */
   hal_gpio_config_t config = {
       .mode = HAL_GPIO_MODE_OUTPUT_PP,
       .pull = HAL_GPIO_PULL_NONE,
       .speed = HAL_GPIO_SPEED_LOW,
       .init_level = HAL_GPIO_LEVEL_LOW
   };
   hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

**Opaque Handles**:

Use opaque pointers for handles:

.. code-block:: c

   /* Public header - opaque type */
   typedef void* osal_task_handle_t;
   typedef void* osal_mutex_handle_t;

   /* Implementation file - actual structure */
   typedef struct {
       /* Internal fields */
       void* native_handle;
       uint32_t priority;
       char name[32];
   } osal_task_internal_t;

**Benefits**:

* Hide implementation details
* Allow implementation changes
* Prevent direct access to internals
* Type safety

Structure Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

**Designated Initializers**:

Always use designated initializers:

.. code-block:: c

   /* Good: Designated initializers */
   hal_gpio_config_t config = {
       .mode = HAL_GPIO_MODE_OUTPUT_PP,
       .pull = HAL_GPIO_PULL_NONE,
       .speed = HAL_GPIO_SPEED_LOW,
       .init_level = HAL_GPIO_LEVEL_LOW
   };

   /* Bad: Positional initializers */
   hal_gpio_config_t config = {
       HAL_GPIO_MODE_OUTPUT_PP,
       HAL_GPIO_PULL_NONE,
       HAL_GPIO_SPEED_LOW,
       HAL_GPIO_LEVEL_LOW
   };

**Default Values**:

Provide helper functions for default initialization:

.. code-block:: c

   /**
    * \brief           Get default GPIO configuration
    * \param[out]      config: Configuration structure to initialize
    */
   void hal_gpio_get_default_config(hal_gpio_config_t* config) {
       if (config == NULL) {
           return;
       }

       config->mode = HAL_GPIO_MODE_INPUT;
       config->pull = HAL_GPIO_PULL_NONE;
       config->speed = HAL_GPIO_SPEED_LOW;
       config->init_level = HAL_GPIO_LEVEL_LOW;
   }

   /* Usage */
   hal_gpio_config_t config;
   hal_gpio_get_default_config(&config);
   config.mode = HAL_GPIO_MODE_OUTPUT_PP;  /* Override specific fields */
   hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

Enumeration Design
~~~~~~~~~~~~~~~~~~

**Explicit Values**:

Always specify explicit values:

.. code-block:: c

   /* Good: Explicit values */
   typedef enum {
       HAL_GPIO_MODE_INPUT = 0,
       HAL_GPIO_MODE_OUTPUT_PP = 1,
       HAL_GPIO_MODE_OUTPUT_OD = 2,
       HAL_GPIO_MODE_AF_PP = 3,
       HAL_GPIO_MODE_AF_OD = 4,
   } hal_gpio_mode_t;

   /* Bad: Implicit values */
   typedef enum {
       HAL_GPIO_MODE_INPUT,
       HAL_GPIO_MODE_OUTPUT_PP,
       HAL_GPIO_MODE_OUTPUT_OD,
   } hal_gpio_mode_t;

**Size Specification**:

Specify enum size when needed:

.. code-block:: c

   /* Specify size for packed structures */
   typedef enum {
       HAL_GPIO_LEVEL_LOW = 0,
       HAL_GPIO_LEVEL_HIGH = 1,
   } hal_gpio_level_t;

   /* Ensure 8-bit size */
   _Static_assert(sizeof(hal_gpio_level_t) <= sizeof(uint8_t),
                  "hal_gpio_level_t must fit in uint8_t");


Error Handling
--------------

Status Codes
~~~~~~~~~~~~

**Consistent Status Codes**:

All modules use consistent status code pattern:

.. code-block:: c

   /* HAL status codes */
   typedef enum {
       HAL_OK = 0,                /* Success */
       HAL_ERROR,                 /* General error */
       HAL_ERROR_PARAM,           /* Invalid parameter */
       HAL_ERROR_STATE,           /* Invalid state */
       HAL_ERROR_TIMEOUT,         /* Operation timeout */
       HAL_ERROR_NO_MEMORY,       /* Out of memory */
       HAL_ERROR_NOT_SUPPORTED,   /* Not supported */
       HAL_ERROR_BUSY,            /* Resource busy */
   } hal_status_t;

   /* OSAL status codes */
   typedef enum {
       OSAL_OK = 0,               /* Success */
       OSAL_ERROR,                /* General error */
       OSAL_ERROR_PARAM,          /* Invalid parameter */
       OSAL_ERROR_TIMEOUT,        /* Operation timeout */
       OSAL_ERROR_NO_MEMORY,      /* Out of memory */
   } osal_status_t;

**Success is Always Zero**:

.. code-block:: c

   /* Good: Success is 0 */
   if (hal_gpio_init(port, pin, &config) == HAL_OK) {
       /* Success */
   }

   /* Also works */
   if (hal_gpio_init(port, pin, &config) != HAL_OK) {
       /* Error */
   }

Error Checking
~~~~~~~~~~~~~~

**Always Check Return Values**:

.. code-block:: c

   /* Good: Check return value */
   hal_status_t status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);
   if (status != HAL_OK) {
       LOG_ERROR("GPIO init failed: %d", status);
       return status;
   }

   /* Bad: Ignore return value */
   hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);

**Error Propagation**:

Propagate errors up the call stack:

.. code-block:: c

   hal_status_t initialize_peripherals(void) {
       hal_status_t status;

       /* Initialize GPIO */
       status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &gpio_config);
       if (status != HAL_OK) {
           LOG_ERROR("GPIO init failed: %d", status);
           return status;
       }

       /* Initialize UART */
       status = hal_uart_init(HAL_UART_1, &uart_config);
       if (status != HAL_OK) {
           LOG_ERROR("UART init failed: %d", status);
           /* Cleanup GPIO before returning */
           hal_gpio_deinit(HAL_GPIO_PORT_A, 5);
           return status;
       }

       return HAL_OK;
   }

Parameter Validation
~~~~~~~~~~~~~~~~~~~~

**Validate All Parameters**:

.. code-block:: c

   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config) {
       /* Validate port */
       if (port >= HAL_GPIO_PORT_MAX) {
           return HAL_ERROR_PARAM;
       }

       /* Validate pin */
       if (pin >= HAL_GPIO_PIN_MAX) {
           return HAL_ERROR_PARAM;
       }

       /* Validate config pointer */
       if (config == NULL) {
           return HAL_ERROR_PARAM;
       }

       /* Validate config values */
       if (config->mode >= HAL_GPIO_MODE_MAX) {
           return HAL_ERROR_PARAM;
       }

       /* Implementation */
       return HAL_OK;
   }

**Validation Order**:

1. Null pointer checks
2. Range checks
3. State checks
4. Resource availability checks

State Validation
~~~~~~~~~~~~~~~~

**Check State Before Operations**:

.. code-block:: c

   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                             size_t length, uint32_t timeout) {
       /* Validate parameters */
       if (id >= HAL_UART_MAX_INSTANCES) {
           return HAL_ERROR_PARAM;
       }
       if (data == NULL || length == 0) {
           return HAL_ERROR_PARAM;
       }

       /* Check if UART is initialized */
       if (!uart_is_initialized(id)) {
           return HAL_ERROR_STATE;
       }

       /* Check if UART is busy */
       if (uart_is_busy(id)) {
           return HAL_ERROR_BUSY;
       }

       /* Implementation */
       return HAL_OK;
   }

Error Messages
~~~~~~~~~~~~~~

**Provide Helpful Error Messages**:

.. code-block:: c

   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config) {
       if (port >= HAL_GPIO_PORT_MAX) {
           LOG_ERROR("Invalid GPIO port: %d (max: %d)", port, HAL_GPIO_PORT_MAX - 1);
           return HAL_ERROR_PARAM;
       }

       if (pin >= HAL_GPIO_PIN_MAX) {
           LOG_ERROR("Invalid GPIO pin: %d (max: %d)", pin, HAL_GPIO_PIN_MAX - 1);
           return HAL_ERROR_PARAM;
       }

       if (config == NULL) {
           LOG_ERROR("GPIO config is NULL");
           return HAL_ERROR_PARAM;
       }

       return HAL_OK;
   }


Memory Management
-----------------

Ownership Rules
~~~~~~~~~~~~~~~

**Clear Ownership**:

Always document who owns memory:

.. code-block:: c

   /**
    * \brief           Create GPIO device (caller owns returned pointer)
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin
    * \return          GPIO device pointer (must be freed with nx_factory_gpio_release)
    */
   nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin);

   /**
    * \brief           Release GPIO device (frees memory)
    * \param[in]       gpio: GPIO device pointer
    */
   void nx_factory_gpio_release(nx_gpio_t* gpio);

**Allocation Patterns**:

1. **Caller Allocates**: Caller provides buffer

   .. code-block:: c

      /* Caller allocates buffer */
      hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                    size_t buffer_size, size_t* received_length,
                                    uint32_t timeout);

      /* Usage */
      uint8_t buffer[256];
      size_t received;
      hal_uart_receive(HAL_UART_1, buffer, sizeof(buffer), &received, 1000);

2. **Callee Allocates**: Function allocates and returns

   .. code-block:: c

      /* Function allocates memory */
      nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin);

      /* Usage */
      nx_gpio_t* gpio = nx_factory_gpio(0, 5);
      /* Use gpio */
      nx_factory_gpio_release(gpio);  /* Caller must free */

3. **Static Allocation**: Function returns static data

   .. code-block:: c

      /* Returns pointer to static data */
      const char* hal_get_version(void);

      /* Usage */
      const char* version = hal_get_version();
      /* No need to free */

Buffer Management
~~~~~~~~~~~~~~~~~

**Size Parameters**:

Always include buffer size:

.. code-block:: c

   /* Good: Buffer size included */
   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                 size_t buffer_size, size_t* received_length,
                                 uint32_t timeout);

   /* Bad: No size parameter */
   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                 size_t* received_length, uint32_t timeout);

**Bounds Checking**:

Always check buffer bounds:

.. code-block:: c

   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                 size_t buffer_size, size_t* received_length,
                                 uint32_t timeout) {
       /* Validate buffer */
       if (buffer == NULL || buffer_size == 0) {
           return HAL_ERROR_PARAM;
       }

       /* Check available data */
       size_t available = uart_get_available_data(id);
       if (available > buffer_size) {
           /* Buffer too small */
           return HAL_ERROR_NO_MEMORY;
       }

       /* Copy data with bounds check */
       for (size_t i = 0; i < available && i < buffer_size; i++) {
           buffer[i] = uart_read_byte(id);
       }

       *received_length = available;
       return HAL_OK;
   }

**String Handling**:

Use safe string functions:

.. code-block:: c

   /* Good: Size-limited string copy */
   hal_status_t config_get_str(const char* key, char* buffer, size_t buffer_size) {
       if (buffer == NULL || buffer_size == 0) {
           return CONFIG_ERROR_PARAM;
       }

       const char* value = config_find_value(key);
       if (value == NULL) {
           return CONFIG_ERROR_NOT_FOUND;
       }

       /* Safe string copy with null termination */
       strncpy(buffer, value, buffer_size - 1);
       buffer[buffer_size - 1] = '\0';

       return CONFIG_OK;
   }

   /* Bad: Unsafe string copy */
   hal_status_t config_get_str(const char* key, char* buffer) {
       const char* value = config_find_value(key);
       strcpy(buffer, value);  /* Buffer overflow risk! */
       return CONFIG_OK;
   }

Resource Cleanup
~~~~~~~~~~~~~~~~

**RAII Pattern**:

Use init/deinit pairs:

.. code-block:: c

   /* Initialize resource */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);

   /* Cleanup resource */
   hal_status_t hal_gpio_deinit(hal_gpio_port_t port, uint8_t pin);

   /* Usage */
   hal_status_t status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);
   if (status != HAL_OK) {
       return status;
   }

   /* Use GPIO */

   /* Always cleanup */
   hal_gpio_deinit(HAL_GPIO_PORT_A, 5);

**Error Cleanup**:

Clean up on error:

.. code-block:: c

   hal_status_t initialize_system(void) {
       hal_status_t status;

       /* Initialize GPIO */
       status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &gpio_config);
       if (status != HAL_OK) {
           return status;
       }

       /* Initialize UART */
       status = hal_uart_init(HAL_UART_1, &uart_config);
       if (status != HAL_OK) {
           /* Cleanup GPIO before returning */
           hal_gpio_deinit(HAL_GPIO_PORT_A, 5);
           return status;
       }

       /* Initialize SPI */
       status = hal_spi_init(HAL_SPI_1, &spi_config);
       if (status != HAL_OK) {
           /* Cleanup UART and GPIO */
           hal_uart_deinit(HAL_UART_1);
           hal_gpio_deinit(HAL_GPIO_PORT_A, 5);
           return status;
       }

       return HAL_OK;
   }


Thread Safety
-------------

Reentrancy
~~~~~~~~~~

**Document Thread Safety**:

Always document thread safety guarantees:

.. code-block:: c

   /**
    * \brief           Write to GPIO pin (thread-safe)
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin
    * \param[in]       level: Output level
    * \return          HAL_OK on success
    * \note            This function is thread-safe
    */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level);

   /**
    * \brief           Initialize GPIO pin (NOT thread-safe)
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin
    * \param[in]       config: Configuration
    * \return          HAL_OK on success
    * \note            This function is NOT thread-safe. Call only during initialization.
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);

**Thread-Safe Implementation**:

Use mutexes for shared resources:

.. code-block:: c

   /* Global mutex for GPIO access */
   static osal_mutex_handle_t gpio_mutex;

   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level) {
       hal_status_t status;

       /* Lock mutex */
       if (osal_mutex_lock(gpio_mutex, OSAL_WAIT_FOREVER) != OSAL_OK) {
           return HAL_ERROR;
       }

       /* Critical section */
       status = platform_gpio_write(port, pin, level);

       /* Unlock mutex */
       osal_mutex_unlock(gpio_mutex);

       return status;
   }

Atomic Operations
~~~~~~~~~~~~~~~~~

**Use Atomic Operations**:

For simple operations, use atomic operations:

.. code-block:: c

   #include <stdatomic.h>

   /* Atomic counter */
   static atomic_uint_fast32_t request_counter = ATOMIC_VAR_INIT(0);

   uint32_t get_next_request_id(void) {
       return atomic_fetch_add(&request_counter, 1);
   }

**Lock-Free Queues**:

For high-performance scenarios, consider lock-free data structures:

.. code-block:: c

   /* Lock-free ring buffer for interrupt-to-task communication */
   typedef struct {
       uint8_t buffer[256];
       atomic_uint_fast32_t head;
       atomic_uint_fast32_t tail;
   } lockfree_ringbuf_t;

   bool ringbuf_push(lockfree_ringbuf_t* rb, uint8_t data) {
       uint32_t head = atomic_load(&rb->head);
       uint32_t next_head = (head + 1) % 256;

       if (next_head == atomic_load(&rb->tail)) {
           return false;  /* Buffer full */
       }

       rb->buffer[head] = data;
       atomic_store(&rb->head, next_head);
       return true;
   }

Interrupt Safety
~~~~~~~~~~~~~~~~

**Document Interrupt Safety**:

.. code-block:: c

   /**
    * \brief           Write to GPIO pin (interrupt-safe)
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin
    * \param[in]       level: Output level
    * \return          HAL_OK on success
    * \note            This function is interrupt-safe and can be called from ISR
    */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level);

   /**
    * \brief           Send data via UART (NOT interrupt-safe)
    * \param[in]       id: UART instance
    * \param[in]       data: Data buffer
    * \param[in]       length: Data length
    * \param[in]       timeout: Timeout in milliseconds
    * \return          HAL_OK on success
    * \note            This function is NOT interrupt-safe. Do not call from ISR.
    */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data, size_t length, uint32_t timeout);

**ISR-Safe Implementation**:

.. code-block:: c

   /* ISR-safe function - no blocking, no allocation */
   hal_status_t hal_gpio_write_isr(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level) {
       /* Direct hardware access, no locks */
       platform_gpio_write_direct(port, pin, level);
       return HAL_OK;
   }

   /* Regular function - may block */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin, hal_gpio_level_t level) {
       /* Use mutex for thread safety */
       osal_mutex_lock(gpio_mutex, OSAL_WAIT_FOREVER);
       platform_gpio_write(port, pin, level);
       osal_mutex_unlock(gpio_mutex);
       return HAL_OK;
   }


Documentation Requirements
--------------------------

Doxygen Comments
~~~~~~~~~~~~~~~~

**File Headers**:

Every file must have a header comment:

.. code-block:: c

   /**
    * \file            hal_gpio.h
    * \brief           GPIO Hardware Abstraction Layer
    * \author          Nexus Team
    */

**Function Documentation**:

All public functions must be documented:

.. code-block:: c

   /**
    * \brief           Initialize GPIO pin
    * \param[in]       port: GPIO port (HAL_GPIO_PORT_A to HAL_GPIO_PORT_K)
    * \param[in]       pin: GPIO pin number (0-15)
    * \param[in]       config: Pointer to configuration structure
    * \return          HAL_OK on success, error code otherwise
    * \retval          HAL_OK: Success
    * \retval          HAL_ERROR_PARAM: Invalid parameter
    * \retval          HAL_ERROR_STATE: GPIO already initialized
    * \note            Pin must be deinitialized before re-initialization
    * \warning         This function is NOT thread-safe
    * \see             hal_gpio_deinit
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config);

**Structure Documentation**:

Document all structures and fields:

.. code-block:: c

   /**
    * \brief           GPIO configuration structure
    */
   typedef struct {
       hal_gpio_mode_t mode;          /**< GPIO mode (input/output/alternate) */
       hal_gpio_pull_t pull;          /**< Pull-up/down configuration */
       hal_gpio_speed_t speed;        /**< Output speed (low/medium/high/very high) */
       hal_gpio_level_t init_level;   /**< Initial output level (low/high) */
   } hal_gpio_config_t;

**Enumeration Documentation**:

Document all enumerations and values:

.. code-block:: c

   /**
    * \brief           GPIO mode enumeration
    */
   typedef enum {
       HAL_GPIO_MODE_INPUT = 0,       /**< Input mode */
       HAL_GPIO_MODE_OUTPUT_PP,       /**< Output push-pull mode */
       HAL_GPIO_MODE_OUTPUT_OD,       /**< Output open-drain mode */
       HAL_GPIO_MODE_AF_PP,           /**< Alternate function push-pull */
       HAL_GPIO_MODE_AF_OD,           /**< Alternate function open-drain */
       HAL_GPIO_MODE_ANALOG,          /**< Analog mode */
   } hal_gpio_mode_t;

Usage Examples
~~~~~~~~~~~~~~

**Provide Examples**:

Include usage examples in documentation:

.. code-block:: c

   /**
    * \brief           Initialize GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin number
    * \param[in]       config: Configuration structure
    * \return          HAL_OK on success
    *
    * \par Example:
    * \code
    * // Configure PA5 as output
    * hal_gpio_config_t config = {
    *     .mode = HAL_GPIO_MODE_OUTPUT_PP,
    *     .pull = HAL_GPIO_PULL_NONE,
    *     .speed = HAL_GPIO_SPEED_LOW,
    *     .init_level = HAL_GPIO_LEVEL_LOW
    * };
    *
    * hal_status_t status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);
    * if (status != HAL_OK) {
    *     // Handle error
    * }
    *
    * // Write to pin
    * hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);
    * \endcode
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config);

**Complex Examples**:

For complex APIs, provide detailed examples:

.. code-block:: c

   /**
    * \brief           Send data via UART with DMA
    *
    * \par Example: Basic Usage
    * \code
    * uint8_t data[] = "Hello, World!";
    * size_t sent;
    * hal_status_t status = hal_uart_send_dma(HAL_UART_1, data, sizeof(data), &sent, 1000);
    * if (status == HAL_OK) {
    *     printf("Sent %zu bytes\n", sent);
    * }
    * \endcode
    *
    * \par Example: Error Handling
    * \code
    * hal_status_t status = hal_uart_send_dma(HAL_UART_1, data, sizeof(data), &sent, 1000);
    * switch (status) {
    *     case HAL_OK:
    *         printf("Success\n");
    *         break;
    *     case HAL_ERROR_TIMEOUT:
    *         printf("Timeout\n");
    *         break;
    *     case HAL_ERROR_BUSY:
    *         printf("UART busy\n");
    *         break;
    *     default:
    *         printf("Error: %d\n", status);
    *         break;
    * }
    * \endcode
    */
   hal_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                  size_t length, size_t* sent, uint32_t timeout);



Versioning and Compatibility
-----------------------------

API Versioning
~~~~~~~~~~~~~~

**Semantic Versioning**:

Use semantic versioning for APIs:

* **Major version**: Breaking changes
* **Minor version**: New features (backward compatible)
* **Patch version**: Bug fixes (backward compatible)

**Version Macros**:

.. code-block:: c

   /* Version information */
   #define HAL_VERSION_MAJOR    1
   #define HAL_VERSION_MINOR    2
   #define HAL_VERSION_PATCH    3

   /* Version as single number */
   #define HAL_VERSION          ((HAL_VERSION_MAJOR << 16) | \
                                 (HAL_VERSION_MINOR << 8) | \
                                 HAL_VERSION_PATCH)

   /* Version string */
   #define HAL_VERSION_STRING   "1.2.3"

   /**
    * \brief           Get HAL version
    * \return          Version as 32-bit number (major.minor.patch)
    */
   uint32_t hal_get_version(void);

   /**
    * \brief           Get HAL version string
    * \return          Version string (e.g., "1.2.3")
    */
   const char* hal_get_version_string(void);

Backward Compatibility
~~~~~~~~~~~~~~~~~~~~~~

**Deprecation Process**:

1. Mark as deprecated in documentation
2. Add deprecation warning
3. Provide migration path
4. Remove in next major version

**Deprecation Example**:

.. code-block:: c

   /**
    * \brief           Old function (DEPRECATED)
    * \deprecated      Use hal_gpio_init() instead
    * \see             hal_gpio_init
    */
   __attribute__((deprecated("Use hal_gpio_init() instead")))
   hal_status_t hal_gpio_configure(hal_gpio_port_t port, uint8_t pin,
                                   const hal_gpio_config_t* config);

   /**
    * \brief           New function
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config);

**API Evolution**:

When adding new features, maintain backward compatibility:

.. code-block:: c

   /* Version 1.0 - Original API */
   typedef struct {
       hal_gpio_mode_t mode;
       hal_gpio_pull_t pull;
   } hal_gpio_config_t;

   /* Version 1.1 - Extended API (backward compatible) */
   typedef struct {
       hal_gpio_mode_t mode;
       hal_gpio_pull_t pull;
       hal_gpio_speed_t speed;        /* New field with default */
       hal_gpio_level_t init_level;   /* New field with default */
   } hal_gpio_config_t;

   /* Helper for backward compatibility */
   void hal_gpio_get_default_config(hal_gpio_config_t* config) {
       config->mode = HAL_GPIO_MODE_INPUT;
       config->pull = HAL_GPIO_PULL_NONE;
       config->speed = HAL_GPIO_SPEED_LOW;      /* Default for new field */
       config->init_level = HAL_GPIO_LEVEL_LOW; /* Default for new field */
   }

ABI Stability
~~~~~~~~~~~~~

**Maintain ABI Stability**:

* Don't change structure layout
* Don't change enum values
* Don't change function signatures
* Don't remove public symbols

**Breaking Changes**:

If breaking changes are necessary:

1. Increment major version
2. Document all breaking changes
3. Provide migration guide
4. Consider compatibility layer

**Example Migration Guide**:

.. code-block:: rst

   Migration from v1.x to v2.0
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   **Breaking Changes**:

   1. ``hal_gpio_configure()`` renamed to ``hal_gpio_init()``
   2. ``hal_gpio_config_t`` structure changed
   3. Return type changed from ``int`` to ``hal_status_t``

   **Migration Steps**:

   Old code (v1.x):

   .. code-block:: c

      hal_gpio_config_t config = {
          .mode = GPIO_MODE_OUTPUT,
          .pull = GPIO_PULL_NONE
      };
      int result = hal_gpio_configure(GPIO_PORT_A, 5, &config);

   New code (v2.0):

   .. code-block:: c

      hal_gpio_config_t config = {
          .mode = HAL_GPIO_MODE_OUTPUT_PP,
          .pull = HAL_GPIO_PULL_NONE,
          .speed = HAL_GPIO_SPEED_LOW,
          .init_level = HAL_GPIO_LEVEL_LOW
      };
      hal_status_t status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);


Performance Considerations
--------------------------

Efficiency Guidelines
~~~~~~~~~~~~~~~~~~~~~

**Minimize Overhead**:

Keep API overhead minimal:

.. code-block:: c

   /* Good: Minimal overhead */
   static inline hal_status_t hal_gpio_write_fast(hal_gpio_port_t port,
                                                   uint8_t pin,
                                                   hal_gpio_level_t level) {
       /* Direct register access */
       if (level == HAL_GPIO_LEVEL_HIGH) {
           GPIO_PORTS[port]->BSRR = (1U << pin);
       } else {
           GPIO_PORTS[port]->BSRR = (1U << (pin + 16));
       }
       return HAL_OK;
   }

   /* Bad: Excessive overhead */
   hal_status_t hal_gpio_write_slow(hal_gpio_port_t port,
                                    uint8_t pin,
                                    hal_gpio_level_t level) {
       /* Validate parameters */
       if (port >= HAL_GPIO_PORT_MAX) return HAL_ERROR_PARAM;
       if (pin >= HAL_GPIO_PIN_MAX) return HAL_ERROR_PARAM;

       /* Lock mutex */
       osal_mutex_lock(gpio_mutex, OSAL_WAIT_FOREVER);

       /* Log operation */
       LOG_DEBUG("GPIO write: port=%d, pin=%d, level=%d", port, pin, level);

       /* Write value */
       if (level == HAL_GPIO_LEVEL_HIGH) {
           GPIO_PORTS[port]->BSRR = (1U << pin);
       } else {
           GPIO_PORTS[port]->BSRR = (1U << (pin + 16));
       }

       /* Unlock mutex */
       osal_mutex_unlock(gpio_mutex);

       return HAL_OK;
   }

**Inline Functions**:

Use inline for performance-critical functions:

.. code-block:: c

   /* Inline for performance */
   static inline uint32_t hal_get_tick(void) {
       return SysTick->VAL;
   }

   /* Regular function for complex operations */
   hal_status_t hal_delay_ms(uint32_t ms);

**Avoid Allocations**:

Minimize dynamic memory allocation:

.. code-block:: c

   /* Good: Caller provides buffer */
   hal_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                 size_t buffer_size, size_t* received_length,
                                 uint32_t timeout);

   /* Bad: Function allocates memory */
   uint8_t* hal_uart_receive_alloc(hal_uart_id_t id, size_t* length,
                                   uint32_t timeout);


Batch Operations
~~~~~~~~~~~~~~~~

**Provide Batch APIs**:

For operations on multiple items:

.. code-block:: c

   /* Single operation */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level);

   /* Batch operation - more efficient */
   hal_status_t hal_gpio_write_multiple(hal_gpio_port_t port,
                                        uint16_t pin_mask,
                                        uint16_t value_mask);

   /* Usage */
   /* Set pins 0, 2, 4 high; pins 1, 3, 5 low */
   hal_gpio_write_multiple(HAL_GPIO_PORT_A,
                          0x003F,  /* Pins 0-5 */
                          0x0015); /* Pins 0, 2, 4 high */

**DMA Support**:

Provide DMA APIs for bulk transfers:

.. code-block:: c

   /* Blocking transfer */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                             size_t length, uint32_t timeout);

   /* DMA transfer - more efficient for large data */
   hal_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                 size_t length, hal_uart_callback_t callback,
                                 void* user_data);

Zero-Copy APIs
~~~~~~~~~~~~~~

**Avoid Unnecessary Copies**:

.. code-block:: c

   /* Good: Zero-copy API */
   hal_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                 size_t length, hal_uart_callback_t callback,
                                 void* user_data);

   /* Bad: Copies data internally */
   hal_status_t hal_uart_send_copy(hal_uart_id_t id, const uint8_t* data,
                                   size_t length);

**Buffer Ownership**:

Document buffer ownership clearly:

.. code-block:: c

   /**
    * \brief           Send data via UART using DMA
    * \param[in]       id: UART instance
    * \param[in]       data: Data buffer (must remain valid until callback)
    * \param[in]       length: Data length
    * \param[in]       callback: Completion callback
    * \param[in]       user_data: User data for callback
    * \return          HAL_OK on success
    * \note            Buffer must remain valid until callback is invoked
    * \warning         Do not modify or free buffer until transfer completes
    */
   hal_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                 size_t length, hal_uart_callback_t callback,
                                 void* user_data);

Caching Considerations
~~~~~~~~~~~~~~~~~~~~~~

**Cache-Aligned Buffers**:

For DMA operations, ensure proper alignment:

.. code-block:: c

   /* Cache line size */
   #define CACHE_LINE_SIZE 32

   /* Aligned buffer for DMA */
   __attribute__((aligned(CACHE_LINE_SIZE)))
   static uint8_t dma_buffer[256];

   /**
    * \brief           Send data via UART using DMA
    * \param[in]       data: Data buffer (must be cache-aligned for DMA)
    * \note            Buffer must be aligned to CACHE_LINE_SIZE
    */
   hal_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                 size_t length, hal_uart_callback_t callback,
                                 void* user_data);

**Cache Operations**:

Provide cache management APIs:

.. code-block:: c

   /**
    * \brief           Clean data cache for DMA
    * \param[in]       addr: Buffer address
    * \param[in]       size: Buffer size
    */
   void hal_cache_clean(void* addr, size_t size);

   /**
    * \brief           Invalidate data cache after DMA
    * \param[in]       addr: Buffer address
    * \param[in]       size: Buffer size
    */
   void hal_cache_invalidate(void* addr, size_t size);


Testing Requirements
--------------------

Unit Testing
~~~~~~~~~~~~

**Testable Design**:

Design APIs for testability:

.. code-block:: c

   /* Good: Dependency injection for testing */
   typedef struct {
       hal_status_t (*read)(uint32_t addr, uint8_t* data);
       hal_status_t (*write)(uint32_t addr, uint8_t data);
   } hal_flash_ops_t;

   hal_status_t hal_flash_init(const hal_flash_ops_t* ops);

   /* Test with mock operations */
   hal_status_t mock_flash_read(uint32_t addr, uint8_t* data) {
       *data = test_data[addr];
       return HAL_OK;
   }

   hal_flash_ops_t mock_ops = {
       .read = mock_flash_read,
       .write = mock_flash_write
   };

   hal_flash_init(&mock_ops);

**Test Coverage**:

Ensure comprehensive test coverage:

* Normal operation
* Error conditions
* Boundary conditions
* Invalid parameters
* State transitions
* Concurrent access

**Example Test Cases**:

.. code-block:: c

   /* Test normal operation */
   void test_gpio_write_normal(void) {
       hal_gpio_config_t config = {
           .mode = HAL_GPIO_MODE_OUTPUT_PP,
           .pull = HAL_GPIO_PULL_NONE,
           .speed = HAL_GPIO_SPEED_LOW,
           .init_level = HAL_GPIO_LEVEL_LOW
       };

       assert(hal_gpio_init(HAL_GPIO_PORT_A, 5, &config) == HAL_OK);
       assert(hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH) == HAL_OK);
       assert(hal_gpio_deinit(HAL_GPIO_PORT_A, 5) == HAL_OK);
   }

   /* Test invalid parameters */
   void test_gpio_write_invalid_port(void) {
       assert(hal_gpio_write(HAL_GPIO_PORT_MAX, 5, HAL_GPIO_LEVEL_HIGH) == HAL_ERROR_PARAM);
   }

   /* Test invalid state */
   void test_gpio_write_not_initialized(void) {
       assert(hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH) == HAL_ERROR_STATE);
   }

Integration Testing
~~~~~~~~~~~~~~~~~~~

**Hardware Testing**:

Test on actual hardware:

.. code-block:: c

   /* Hardware integration test */
   void test_uart_loopback(void) {
       uint8_t tx_data[] = "Hello";
       uint8_t rx_data[16];
       size_t received;

       /* Configure UART in loopback mode */
       hal_uart_config_t config = {
           .baudrate = 115200,
           .wordlen = HAL_UART_WORDLEN_8,
           .stopbits = HAL_UART_STOPBITS_1,
           .parity = HAL_UART_PARITY_NONE,
           .mode = HAL_UART_MODE_LOOPBACK
       };

       assert(hal_uart_init(HAL_UART_1, &config) == HAL_OK);

       /* Send data */
       assert(hal_uart_send(HAL_UART_1, tx_data, sizeof(tx_data), 1000) == HAL_OK);

       /* Receive data */
       assert(hal_uart_receive(HAL_UART_1, rx_data, sizeof(rx_data), &received, 1000) == HAL_OK);

       /* Verify data */
       assert(received == sizeof(tx_data));
       assert(memcmp(tx_data, rx_data, sizeof(tx_data)) == 0);

       hal_uart_deinit(HAL_UART_1);
   }

**Stress Testing**:

Test under stress conditions:

.. code-block:: c

   /* Stress test - rapid operations */
   void test_gpio_stress(void) {
       hal_gpio_config_t config = {
           .mode = HAL_GPIO_MODE_OUTPUT_PP,
           .pull = HAL_GPIO_PULL_NONE,
           .speed = HAL_GPIO_SPEED_HIGH,
           .init_level = HAL_GPIO_LEVEL_LOW
       };

       assert(hal_gpio_init(HAL_GPIO_PORT_A, 5, &config) == HAL_OK);

       /* Toggle 10000 times */
       for (int i = 0; i < 10000; i++) {
           assert(hal_gpio_toggle(HAL_GPIO_PORT_A, 5) == HAL_OK);
       }

       hal_gpio_deinit(HAL_GPIO_PORT_A, 5);
   }



Common Patterns
---------------

Initialization Pattern
~~~~~~~~~~~~~~~~~~~~~~

**Standard Init/Deinit**:

.. code-block:: c

   /* Initialize resource */
   hal_status_t hal_xxx_init(hal_xxx_id_t id, const hal_xxx_config_t* config);

   /* Deinitialize resource */
   hal_status_t hal_xxx_deinit(hal_xxx_id_t id);

   /* Usage */
   hal_xxx_config_t config;
   hal_xxx_get_default_config(&config);
   config.param1 = value1;

   if (hal_xxx_init(HAL_XXX_1, &config) == HAL_OK) {
       /* Use resource */
       hal_xxx_deinit(HAL_XXX_1);
   }

**Global Init**:

For modules requiring global initialization:

.. code-block:: c

   /**
    * \brief           Initialize HAL module
    * \return          HAL_OK on success
    * \note            Must be called before any other HAL functions
    */
   hal_status_t hal_init(void);

   /**
    * \brief           Deinitialize HAL module
    * \return          HAL_OK on success
    */
   hal_status_t hal_deinit(void);

Configuration Pattern
~~~~~~~~~~~~~~~~~~~~~

**Configuration Structure**:

.. code-block:: c

   /* Configuration structure */
   typedef struct {
       uint32_t param1;
       uint32_t param2;
       bool enable_feature;
   } hal_xxx_config_t;

   /* Get default configuration */
   void hal_xxx_get_default_config(hal_xxx_config_t* config);

   /* Initialize with configuration */
   hal_status_t hal_xxx_init(hal_xxx_id_t id, const hal_xxx_config_t* config);

   /* Runtime configuration */
   hal_status_t hal_xxx_set_config(hal_xxx_id_t id, const hal_xxx_config_t* config);
   hal_status_t hal_xxx_get_config(hal_xxx_id_t id, hal_xxx_config_t* config);

Callback Pattern
~~~~~~~~~~~~~~~~

**Callback Registration**:

.. code-block:: c

   /* Callback function type */
   typedef void (*hal_xxx_callback_t)(hal_xxx_id_t id, hal_xxx_event_t event,
                                      void* user_data);

   /**
    * \brief           Register callback
    * \param[in]       id: Instance ID
    * \param[in]       callback: Callback function (NULL to unregister)
    * \param[in]       user_data: User data passed to callback
    * \return          HAL_OK on success
    */
   hal_status_t hal_xxx_register_callback(hal_xxx_id_t id,
                                          hal_xxx_callback_t callback,
                                          void* user_data);

   /* Usage */
   void my_callback(hal_xxx_id_t id, hal_xxx_event_t event, void* user_data) {
       /* Handle event */
   }

   hal_xxx_register_callback(HAL_XXX_1, my_callback, &my_context);

**Multiple Callbacks**:

For multiple event types:

.. code-block:: c

   /* Callback structure */
   typedef struct {
       void (*on_complete)(hal_xxx_id_t id, void* user_data);
       void (*on_error)(hal_xxx_id_t id, hal_status_t error, void* user_data);
       void (*on_timeout)(hal_xxx_id_t id, void* user_data);
   } hal_xxx_callbacks_t;

   /**
    * \brief           Register callbacks
    * \param[in]       id: Instance ID
    * \param[in]       callbacks: Callback structure (NULL to unregister all)
    * \param[in]       user_data: User data passed to callbacks
    * \return          HAL_OK on success
    */
   hal_status_t hal_xxx_register_callbacks(hal_xxx_id_t id,
                                           const hal_xxx_callbacks_t* callbacks,
                                           void* user_data);

Iterator Pattern
~~~~~~~~~~~~~~~~

**Iteration API**:

.. code-block:: c

   /* Iterator handle */
   typedef void* hal_xxx_iterator_t;

   /**
    * \brief           Get first item
    * \param[out]      iterator: Iterator handle
    * \param[out]      item: First item
    * \return          HAL_OK if item found, HAL_ERROR_NOT_FOUND if empty
    */
   hal_status_t hal_xxx_get_first(hal_xxx_iterator_t* iterator,
                                  hal_xxx_item_t* item);

   /**
    * \brief           Get next item
    * \param[in,out]   iterator: Iterator handle
    * \param[out]      item: Next item
    * \return          HAL_OK if item found, HAL_ERROR_NOT_FOUND if end
    */
   hal_status_t hal_xxx_get_next(hal_xxx_iterator_t* iterator,
                                 hal_xxx_item_t* item);

   /* Usage */
   hal_xxx_iterator_t iter;
   hal_xxx_item_t item;

   if (hal_xxx_get_first(&iter, &item) == HAL_OK) {
       do {
           /* Process item */
       } while (hal_xxx_get_next(&iter, &item) == HAL_OK);
   }

Factory Pattern
~~~~~~~~~~~~~~~

**Object Creation**:

.. code-block:: c

   /**
    * \brief           Create GPIO device
    * \param[in]       port: GPIO port
    * \param[in]       pin: GPIO pin
    * \return          GPIO device pointer (NULL on error)
    * \note            Caller must free with nx_factory_gpio_release()
    */
   nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin);

   /**
    * \brief           Release GPIO device
    * \param[in]       gpio: GPIO device pointer
    */
   void nx_factory_gpio_release(nx_gpio_t* gpio);

   /* Usage */
   nx_gpio_t* gpio = nx_factory_gpio(0, 5);
   if (gpio != NULL) {
       nx_gpio_write(gpio, NX_GPIO_LEVEL_HIGH);
       nx_factory_gpio_release(gpio);
   }

Builder Pattern
~~~~~~~~~~~~~~~

**Complex Object Construction**:

.. code-block:: c

   /* Builder handle */
   typedef struct hal_xxx_builder* hal_xxx_builder_t;

   /**
    * \brief           Create builder
    * \return          Builder handle (NULL on error)
    */
   hal_xxx_builder_t hal_xxx_builder_create(void);

   /**
    * \brief           Set parameter
    * \param[in]       builder: Builder handle
    * \param[in]       value: Parameter value
    * \return          HAL_OK on success
    */
   hal_status_t hal_xxx_builder_set_param1(hal_xxx_builder_t builder, uint32_t value);
   hal_status_t hal_xxx_builder_set_param2(hal_xxx_builder_t builder, uint32_t value);

   /**
    * \brief           Build object
    * \param[in]       builder: Builder handle
    * \param[out]      obj: Created object
    * \return          HAL_OK on success
    */
   hal_status_t hal_xxx_builder_build(hal_xxx_builder_t builder, hal_xxx_t* obj);

   /**
    * \brief           Destroy builder
    * \param[in]       builder: Builder handle
    */
   void hal_xxx_builder_destroy(hal_xxx_builder_t builder);

   /* Usage */
   hal_xxx_builder_t builder = hal_xxx_builder_create();
   hal_xxx_builder_set_param1(builder, 100);
   hal_xxx_builder_set_param2(builder, 200);

   hal_xxx_t obj;
   if (hal_xxx_builder_build(builder, &obj) == HAL_OK) {
       /* Use object */
   }
   hal_xxx_builder_destroy(builder);


Anti-Patterns to Avoid
----------------------

Boolean Parameters
~~~~~~~~~~~~~~~~~~

**Avoid Boolean Parameters**:

.. code-block:: c

   /* Bad: Boolean parameter is unclear */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, bool output);

   /* Usage - unclear what true means */
   hal_gpio_init(HAL_GPIO_PORT_A, 5, true);

   /* Good: Use enum for clarity */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             hal_gpio_mode_t mode);

   /* Usage - clear and explicit */
   hal_gpio_init(HAL_GPIO_PORT_A, 5, HAL_GPIO_MODE_OUTPUT_PP);

Output Parameters First
~~~~~~~~~~~~~~~~~~~~~~~

**Don't Put Output Parameters First**:

.. code-block:: c

   /* Bad: Output parameter first */
   hal_status_t hal_gpio_read(hal_gpio_level_t* level, hal_gpio_port_t port,
                             uint8_t pin);

   /* Good: Input parameters first, output last */
   hal_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin,
                             hal_gpio_level_t* level);

Magic Numbers
~~~~~~~~~~~~~

**Avoid Magic Numbers**:

.. code-block:: c

   /* Bad: Magic numbers */
   hal_status_t hal_uart_init(hal_uart_id_t id, uint32_t config);
   hal_uart_init(HAL_UART_1, 0x00001C00);  /* What does this mean? */

   /* Good: Named constants */
   typedef struct {
       uint32_t baudrate;
       hal_uart_wordlen_t wordlen;
       hal_uart_stopbits_t stopbits;
       hal_uart_parity_t parity;
   } hal_uart_config_t;

   hal_uart_config_t config = {
       .baudrate = 115200,
       .wordlen = HAL_UART_WORDLEN_8,
       .stopbits = HAL_UART_STOPBITS_1,
       .parity = HAL_UART_PARITY_NONE
   };
   hal_uart_init(HAL_UART_1, &config);

Inconsistent Naming
~~~~~~~~~~~~~~~~~~~

**Maintain Consistent Naming**:

.. code-block:: c

   /* Bad: Inconsistent naming */
   hal_status_t gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t hal_uart_initialize(hal_uart_id_t id, const hal_uart_config_t* config);
   hal_status_t SPI_Init(hal_spi_id_t id, const hal_spi_config_t* config);

   /* Good: Consistent naming */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin, const hal_gpio_config_t* config);
   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config);
   hal_status_t hal_spi_init(hal_spi_id_t id, const hal_spi_config_t* config);

Global State
~~~~~~~~~~~~

**Avoid Hidden Global State**:

.. code-block:: c

   /* Bad: Hidden global state */
   static hal_gpio_port_t current_port;

   void hal_gpio_set_port(hal_gpio_port_t port) {
       current_port = port;
   }

   hal_status_t hal_gpio_write(uint8_t pin, hal_gpio_level_t level) {
       /* Uses hidden global state */
       return platform_gpio_write(current_port, pin, level);
   }

   /* Good: Explicit parameters */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level) {
       return platform_gpio_write(port, pin, level);
   }


String Return Values
~~~~~~~~~~~~~~~~~~~~

**Don't Return Strings Directly**:

.. code-block:: c

   /* Bad: Returns pointer to internal buffer */
   char* hal_get_error_string(hal_status_t status) {
       static char buffer[64];
       sprintf(buffer, "Error: %d", status);
       return buffer;  /* Not thread-safe! */
   }

   /* Good: Caller provides buffer */
   hal_status_t hal_get_error_string(hal_status_t status, char* buffer,
                                     size_t buffer_size) {
       if (buffer == NULL || buffer_size == 0) {
           return HAL_ERROR_PARAM;
       }
       snprintf(buffer, buffer_size, "Error: %d", status);
       return HAL_OK;
   }

   /* Also good: Return const string literal */
   const char* hal_get_error_string(hal_status_t status) {
       switch (status) {
           case HAL_OK: return "Success";
           case HAL_ERROR: return "Error";
           case HAL_ERROR_PARAM: return "Invalid parameter";
           default: return "Unknown error";
       }
   }

Excessive Parameters
~~~~~~~~~~~~~~~~~~~~

**Avoid Too Many Parameters**:

.. code-block:: c

   /* Bad: Too many parameters */
   hal_status_t hal_uart_init(hal_uart_id_t id, uint32_t baudrate,
                             hal_uart_wordlen_t wordlen,
                             hal_uart_stopbits_t stopbits,
                             hal_uart_parity_t parity,
                             hal_uart_flow_t flow,
                             bool enable_dma,
                             uint32_t timeout);

   /* Good: Use configuration structure */
   typedef struct {
       uint32_t baudrate;
       hal_uart_wordlen_t wordlen;
       hal_uart_stopbits_t stopbits;
       hal_uart_parity_t parity;
       hal_uart_flow_t flow;
       bool enable_dma;
       uint32_t timeout;
   } hal_uart_config_t;

   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config);


Platform-Specific Considerations
---------------------------------

Endianness
~~~~~~~~~~

**Handle Endianness**:

.. code-block:: c

   /* Endianness conversion macros */
   #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
   #define CPU_TO_BE16(x) __builtin_bswap16(x)
   #define CPU_TO_BE32(x) __builtin_bswap32(x)
   #define BE16_TO_CPU(x) __builtin_bswap16(x)
   #define BE32_TO_CPU(x) __builtin_bswap32(x)
   #else
   #define CPU_TO_BE16(x) (x)
   #define CPU_TO_BE32(x) (x)
   #define BE16_TO_CPU(x) (x)
   #define BE32_TO_CPU(x) (x)
   #endif

   /* Usage */
   uint32_t network_value = CPU_TO_BE32(local_value);
   uint32_t local_value = BE32_TO_CPU(network_value);

Alignment
~~~~~~~~~

**Handle Alignment Requirements**:

.. code-block:: c

   /* Aligned structure */
   typedef struct {
       uint32_t field1;
       uint16_t field2;
       uint8_t field3;
       uint8_t padding;  /* Explicit padding */
   } __attribute__((aligned(4))) hal_xxx_data_t;

   /* Check alignment */
   _Static_assert(sizeof(hal_xxx_data_t) % 4 == 0,
                  "hal_xxx_data_t must be 4-byte aligned");

   /* Unaligned access helpers */
   static inline uint32_t read_unaligned_u32(const uint8_t* ptr) {
       uint32_t value;
       memcpy(&value, ptr, sizeof(value));
       return value;
   }

   static inline void write_unaligned_u32(uint8_t* ptr, uint32_t value) {
       memcpy(ptr, &value, sizeof(value));
   }

Bit Fields
~~~~~~~~~~

**Avoid Bit Fields for Portability**:

.. code-block:: c

   /* Bad: Bit fields are not portable */
   typedef struct {
       uint32_t field1 : 8;
       uint32_t field2 : 8;
       uint32_t field3 : 16;
   } hal_xxx_reg_t;

   /* Good: Use bit masks and shifts */
   #define HAL_XXX_FIELD1_POS   0
   #define HAL_XXX_FIELD1_MASK  (0xFF << HAL_XXX_FIELD1_POS)
   #define HAL_XXX_FIELD2_POS   8
   #define HAL_XXX_FIELD2_MASK  (0xFF << HAL_XXX_FIELD2_POS)
   #define HAL_XXX_FIELD3_POS   16
   #define HAL_XXX_FIELD3_MASK  (0xFFFF << HAL_XXX_FIELD3_POS)

   /* Helper macros */
   #define HAL_XXX_SET_FIELD1(reg, val) \
       ((reg) = ((reg) & ~HAL_XXX_FIELD1_MASK) | \
                (((val) << HAL_XXX_FIELD1_POS) & HAL_XXX_FIELD1_MASK))

   #define HAL_XXX_GET_FIELD1(reg) \
       (((reg) & HAL_XXX_FIELD1_MASK) >> HAL_XXX_FIELD1_POS)

Integer Sizes
~~~~~~~~~~~~~

**Use Fixed-Width Types**:

.. code-block:: c

   /* Good: Fixed-width types */
   #include <stdint.h>

   typedef struct {
       uint8_t byte_field;
       uint16_t word_field;
       uint32_t dword_field;
       uint64_t qword_field;
   } hal_xxx_data_t;

   /* Bad: Variable-width types */
   typedef struct {
       char byte_field;      /* Size varies */
       short word_field;     /* Size varies */
       int dword_field;      /* Size varies */
       long qword_field;     /* Size varies */
   } hal_xxx_data_t;

Platform Abstraction
~~~~~~~~~~~~~~~~~~~~

**Abstract Platform Differences**:

.. code-block:: c

   /* Platform-specific implementation */
   #if defined(PLATFORM_STM32)
   #include "stm32_gpio.h"
   #define PLATFORM_GPIO_WRITE(port, pin, level) stm32_gpio_write(port, pin, level)
   #elif defined(PLATFORM_NRF52)
   #include "nrf52_gpio.h"
   #define PLATFORM_GPIO_WRITE(port, pin, level) nrf52_gpio_write(port, pin, level)
   #else
   #error "Unsupported platform"
   #endif

   /* Platform-independent API */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level) {
       PLATFORM_GPIO_WRITE(port, pin, level);
       return HAL_OK;
   }


Examples and Case Studies
--------------------------

Example 1: GPIO API
~~~~~~~~~~~~~~~~~~~

**Complete GPIO API Design**:

.. code-block:: c

   /* hal_gpio.h */

   /**
    * \file            hal_gpio.h
    * \brief           GPIO Hardware Abstraction Layer
    * \author          Nexus Team
    */

   #ifndef HAL_GPIO_H
   #define HAL_GPIO_H

   #include <stdint.h>
   #include <stdbool.h>

   /*---------------------------------------------------------------------------*/
   /* Type Definitions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           GPIO status codes
    */
   typedef enum {
       HAL_GPIO_OK = 0,           /**< Success */
       HAL_GPIO_ERROR,            /**< General error */
       HAL_GPIO_ERROR_PARAM,      /**< Invalid parameter */
       HAL_GPIO_ERROR_STATE,      /**< Invalid state */
   } hal_gpio_status_t;

   /**
    * \brief           GPIO port enumeration
    */
   typedef enum {
       HAL_GPIO_PORT_A = 0,       /**< Port A */
       HAL_GPIO_PORT_B,           /**< Port B */
       HAL_GPIO_PORT_C,           /**< Port C */
       HAL_GPIO_PORT_MAX          /**< Maximum port number */
   } hal_gpio_port_t;

   /**
    * \brief           GPIO mode enumeration
    */
   typedef enum {
       HAL_GPIO_MODE_INPUT = 0,   /**< Input mode */
       HAL_GPIO_MODE_OUTPUT_PP,   /**< Output push-pull */
       HAL_GPIO_MODE_OUTPUT_OD,   /**< Output open-drain */
   } hal_gpio_mode_t;

   /**
    * \brief           GPIO pull configuration
    */
   typedef enum {
       HAL_GPIO_PULL_NONE = 0,    /**< No pull */
       HAL_GPIO_PULL_UP,          /**< Pull-up */
       HAL_GPIO_PULL_DOWN,        /**< Pull-down */
   } hal_gpio_pull_t;

   /**
    * \brief           GPIO level enumeration
    */
   typedef enum {
       HAL_GPIO_LEVEL_LOW = 0,    /**< Low level */
       HAL_GPIO_LEVEL_HIGH,       /**< High level */
   } hal_gpio_level_t;

   /**
    * \brief           GPIO configuration structure
    */
   typedef struct {
       hal_gpio_mode_t mode;      /**< GPIO mode */
       hal_gpio_pull_t pull;      /**< Pull configuration */
       hal_gpio_level_t init_level; /**< Initial level */
   } hal_gpio_config_t;

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Get default GPIO configuration
    * \param[out]      config: Configuration structure
    */
   void hal_gpio_get_default_config(hal_gpio_config_t* config);

   /**
    * \brief           Initialize GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: Pin number (0-15)
    * \param[in]       config: Configuration structure
    * \return          HAL_GPIO_OK on success
    */
   hal_gpio_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                                   const hal_gpio_config_t* config);

   /**
    * \brief           Deinitialize GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: Pin number
    * \return          HAL_GPIO_OK on success
    */
   hal_gpio_status_t hal_gpio_deinit(hal_gpio_port_t port, uint8_t pin);

   /**
    * \brief           Write to GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: Pin number
    * \param[in]       level: Output level
    * \return          HAL_GPIO_OK on success
    */
   hal_gpio_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                                    hal_gpio_level_t level);

   /**
    * \brief           Read from GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: Pin number
    * \param[out]      level: Read level
    * \return          HAL_GPIO_OK on success
    */
   hal_gpio_status_t hal_gpio_read(hal_gpio_port_t port, uint8_t pin,
                                   hal_gpio_level_t* level);

   /**
    * \brief           Toggle GPIO pin
    * \param[in]       port: GPIO port
    * \param[in]       pin: Pin number
    * \return          HAL_GPIO_OK on success
    */
   hal_gpio_status_t hal_gpio_toggle(hal_gpio_port_t port, uint8_t pin);

   #endif /* HAL_GPIO_H */


Example 2: UART API with DMA
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**UART API with Async Operations**:

.. code-block:: c

   /* hal_uart.h */

   /**
    * \file            hal_uart.h
    * \brief           UART Hardware Abstraction Layer
    * \author          Nexus Team
    */

   #ifndef HAL_UART_H
   #define HAL_UART_H

   #include <stdint.h>
   #include <stddef.h>

   /*---------------------------------------------------------------------------*/
   /* Type Definitions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           UART instance ID
    */
   typedef enum {
       HAL_UART_1 = 0,            /**< UART instance 1 */
       HAL_UART_2,                /**< UART instance 2 */
       HAL_UART_MAX               /**< Maximum UART instances */
   } hal_uart_id_t;

   /**
    * \brief           UART status codes
    */
   typedef enum {
       HAL_UART_OK = 0,           /**< Success */
       HAL_UART_ERROR,            /**< General error */
       HAL_UART_ERROR_PARAM,      /**< Invalid parameter */
       HAL_UART_ERROR_TIMEOUT,    /**< Operation timeout */
       HAL_UART_ERROR_BUSY,       /**< UART busy */
   } hal_uart_status_t;

   /**
    * \brief           UART event types
    */
   typedef enum {
       HAL_UART_EVENT_TX_COMPLETE = 0, /**< Transmission complete */
       HAL_UART_EVENT_RX_COMPLETE,     /**< Reception complete */
       HAL_UART_EVENT_ERROR,           /**< Error occurred */
   } hal_uart_event_t;

   /**
    * \brief           UART callback function type
    * \param[in]       id: UART instance ID
    * \param[in]       event: Event type
    * \param[in]       user_data: User data
    */
   typedef void (*hal_uart_callback_t)(hal_uart_id_t id,
                                       hal_uart_event_t event,
                                       void* user_data);

   /**
    * \brief           UART configuration structure
    */
   typedef struct {
       uint32_t baudrate;         /**< Baudrate (e.g., 115200) */
       uint8_t wordlen;           /**< Word length (7, 8, 9 bits) */
       uint8_t stopbits;          /**< Stop bits (1, 2) */
       uint8_t parity;            /**< Parity (none, even, odd) */
   } hal_uart_config_t;

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Get default UART configuration
    * \param[out]      config: Configuration structure
    */
   void hal_uart_get_default_config(hal_uart_config_t* config);

   /**
    * \brief           Initialize UART
    * \param[in]       id: UART instance ID
    * \param[in]       config: Configuration structure
    * \return          HAL_UART_OK on success
    */
   hal_uart_status_t hal_uart_init(hal_uart_id_t id,
                                   const hal_uart_config_t* config);

   /**
    * \brief           Deinitialize UART
    * \param[in]       id: UART instance ID
    * \return          HAL_UART_OK on success
    */
   hal_uart_status_t hal_uart_deinit(hal_uart_id_t id);

   /**
    * \brief           Send data (blocking)
    * \param[in]       id: UART instance ID
    * \param[in]       data: Data buffer
    * \param[in]       length: Data length
    * \param[in]       timeout: Timeout in milliseconds
    * \return          HAL_UART_OK on success
    */
   hal_uart_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                                   size_t length, uint32_t timeout);

   /**
    * \brief           Receive data (blocking)
    * \param[in]       id: UART instance ID
    * \param[out]      buffer: Receive buffer
    * \param[in]       buffer_size: Buffer size
    * \param[out]      received: Bytes received
    * \param[in]       timeout: Timeout in milliseconds
    * \return          HAL_UART_OK on success
    */
   hal_uart_status_t hal_uart_receive(hal_uart_id_t id, uint8_t* buffer,
                                      size_t buffer_size, size_t* received,
                                      uint32_t timeout);

   /**
    * \brief           Send data using DMA (non-blocking)
    * \param[in]       id: UART instance ID
    * \param[in]       data: Data buffer (must remain valid until callback)
    * \param[in]       length: Data length
    * \param[in]       callback: Completion callback
    * \param[in]       user_data: User data for callback
    * \return          HAL_UART_OK on success
    * \note            Buffer must remain valid until callback is invoked
    */
   hal_uart_status_t hal_uart_send_dma(hal_uart_id_t id, const uint8_t* data,
                                       size_t length,
                                       hal_uart_callback_t callback,
                                       void* user_data);

   /**
    * \brief           Receive data using DMA (non-blocking)
    * \param[in]       id: UART instance ID
    * \param[out]      buffer: Receive buffer (must remain valid until callback)
    * \param[in]       buffer_size: Buffer size
    * \param[in]       callback: Completion callback
    * \param[in]       user_data: User data for callback
    * \return          HAL_UART_OK on success
    * \note            Buffer must remain valid until callback is invoked
    */
   hal_uart_status_t hal_uart_receive_dma(hal_uart_id_t id, uint8_t* buffer,
                                          size_t buffer_size,
                                          hal_uart_callback_t callback,
                                          void* user_data);

   #endif /* HAL_UART_H */

Example 3: Configuration API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Type-Safe Configuration API**:

.. code-block:: c

   /* config.h */

   /**
    * \file            config.h
    * \brief           Configuration System API
    * \author          Nexus Team
    */

   #ifndef CONFIG_H
   #define CONFIG_H

   #include <stdint.h>
   #include <stdbool.h>
   #include <stddef.h>

   /*---------------------------------------------------------------------------*/
   /* Type Definitions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Configuration status codes
    */
   typedef enum {
       CONFIG_OK = 0,             /**< Success */
       CONFIG_ERROR,              /**< General error */
       CONFIG_ERROR_NOT_FOUND,    /**< Key not found */
       CONFIG_ERROR_TYPE,         /**< Type mismatch */
       CONFIG_ERROR_RANGE,        /**< Value out of range */
   } config_status_t;

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize configuration system
    * \return          CONFIG_OK on success
    */
   config_status_t config_init(void);

   /**
    * \brief           Set integer value
    * \param[in]       key: Configuration key
    * \param[in]       value: Integer value
    * \return          CONFIG_OK on success
    */
   config_status_t config_set_i32(const char* key, int32_t value);

   /**
    * \brief           Get integer value
    * \param[in]       key: Configuration key
    * \param[out]      value: Integer value
    * \return          CONFIG_OK on success
    */
   config_status_t config_get_i32(const char* key, int32_t* value);

   /**
    * \brief           Set string value
    * \param[in]       key: Configuration key
    * \param[in]       value: String value
    * \return          CONFIG_OK on success
    */
   config_status_t config_set_str(const char* key, const char* value);

   /**
    * \brief           Get string value
    * \param[in]       key: Configuration key
    * \param[out]      buffer: Output buffer
    * \param[in]       buffer_size: Buffer size
    * \return          CONFIG_OK on success
    */
   config_status_t config_get_str(const char* key, char* buffer,
                                  size_t buffer_size);

   /**
    * \brief           Set boolean value
    * \param[in]       key: Configuration key
    * \param[in]       value: Boolean value
    * \return          CONFIG_OK on success
    */
   config_status_t config_set_bool(const char* key, bool value);

   /**
    * \brief           Get boolean value
    * \param[in]       key: Configuration key
    * \param[out]      value: Boolean value
    * \return          CONFIG_OK on success
    */
   config_status_t config_get_bool(const char* key, bool* value);

   /**
    * \brief           Check if key exists
    * \param[in]       key: Configuration key
    * \return          true if key exists
    */
   bool config_has_key(const char* key);

   /**
    * \brief           Delete key
    * \param[in]       key: Configuration key
    * \return          CONFIG_OK on success
    */
   config_status_t config_delete(const char* key);

   /**
    * \brief           Save configuration to storage
    * \return          CONFIG_OK on success
    */
   config_status_t config_save(void);

   /**
    * \brief           Load configuration from storage
    * \return          CONFIG_OK on success
    */
   config_status_t config_load(void);

   #endif /* CONFIG_H */


See Also
--------

Related Documentation
~~~~~~~~~~~~~~~~~~~~~

* :doc:`coding_standards` - Code style and formatting guidelines
* :doc:`architecture_design` - System architecture and design patterns
* :doc:`testing` - Testing strategies and best practices
* :doc:`documentation_contributing` - Documentation guidelines

External Resources
~~~~~~~~~~~~~~~~~~

* `C Coding Standard <https://www.kernel.org/doc/html/latest/process/coding-style.html>`_
* `API Design Guidelines <https://www.gnu.org/prep/standards/>`_
* `Embedded C Coding Standard <https://barrgroup.com/embedded-systems/books/embedded-c-coding-standard>`_
* `MISRA C Guidelines <https://www.misra.org.uk/>`_

Summary
-------

This guide provides comprehensive API design guidelines for the Nexus Embedded Platform:

* **Design Principles**: Consistency, simplicity, orthogonality, discoverability, safety
* **Naming Conventions**: Module prefixes, function names, type names, enumerations
* **Function Signatures**: Parameter order, return values, input/output parameters
* **Data Structures**: Configuration structures, opaque handles, enumerations
* **Error Handling**: Status codes, parameter validation, error propagation
* **Memory Management**: Ownership rules, buffer management, resource cleanup
* **Thread Safety**: Reentrancy, atomic operations, interrupt safety
* **Documentation**: Doxygen comments, usage examples, API documentation
* **Versioning**: Semantic versioning, backward compatibility, deprecation
* **Performance**: Efficiency, batch operations, zero-copy APIs
* **Testing**: Unit testing, integration testing, testable design
* **Common Patterns**: Initialization, configuration, callbacks, iterators
* **Anti-Patterns**: Boolean parameters, magic numbers, global state
* **Platform Considerations**: Endianness, alignment, portability
* **Examples**: Complete API designs for GPIO, UART, configuration

Following these guidelines ensures consistent, maintainable, and high-quality APIs across the Nexus platform.
