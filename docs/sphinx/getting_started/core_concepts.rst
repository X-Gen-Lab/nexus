Core Concepts
=============

This guide explains the fundamental concepts and design principles of the Nexus platform.

.. contents:: Table of Contents
   :local:
   :depth: 2

Architecture Overview
---------------------

Layered Design
~~~~~~~~~~~~~~

Nexus follows a strict layered architecture:

.. code-block:: text

   ┌─────────────────────────────────────┐
   │        Application Layer            │  Your code
   ├─────────────────────────────────────┤
   │        Framework Layer              │  Log, Shell, Config, Init
   ├─────────────────────────────────────┤
   │    OS Abstraction Layer (OSAL)      │  Tasks, Mutex, Queue, Timer
   ├─────────────────────────────────────┤
   │  Hardware Abstraction Layer (HAL)   │  GPIO, UART, SPI, I2C, ADC
   ├─────────────────────────────────────┤
   │        Platform Layer               │  STM32F4, STM32H7, Native
   └─────────────────────────────────────┘

**Key Principles**:

1. **Separation of Concerns**: Each layer has a specific responsibility
2. **Abstraction**: Hide implementation details behind clean interfaces
3. **Portability**: Minimize platform-specific code in applications
4. **Dependency Rule**: Upper layers depend on lower layers, never reverse

Design Patterns
~~~~~~~~~~~~~~~

**Factory Pattern**

HAL uses factory functions to create device instances:

.. code-block:: c

   /* Create GPIO device */
   nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);

   /* Use device */
   led->toggle(led);

   /* Release device */
   nx_factory_gpio_release((nx_gpio_t*)led);

**Interface Pattern**

Devices expose interfaces with function pointers:

.. code-block:: c

   typedef struct {
       void (*write)(nx_gpio_write_t* self, uint8_t value);
       void (*toggle)(nx_gpio_write_t* self);
       uint8_t (*read)(nx_gpio_write_t* self);
   } nx_gpio_write_t;

**Adapter Pattern**

OSAL adapts different RTOS backends to a common interface:

.. code-block:: c

   /* Same API works with FreeRTOS, bare-metal, etc. */
   osal_task_create(task_func, "task", 512, NULL, 1, NULL);

Hardware Abstraction Layer (HAL)
---------------------------------

Purpose
~~~~~~~

HAL provides unified APIs for hardware peripherals, hiding platform differences.

**Benefits**:

* Write once, run on multiple platforms
* Consistent API across different MCUs
* Easy to test with native simulation
* Simplified driver development

Key Concepts
~~~~~~~~~~~~

**Device Instances**

Each peripheral is represented as a device instance:

.. code-block:: c

   /* GPIO device */
   nx_gpio_write_t* led;

   /* UART device */
   nx_uart_t* uart;

   /* SPI device */
   nx_spi_t* spi;

**Configuration Structures**

Devices are configured using structures:

.. code-block:: c

   nx_gpio_config_t gpio_cfg = {
       .mode  = NX_GPIO_MODE_OUTPUT_PP,
       .pull  = NX_GPIO_PULL_NONE,
       .speed = NX_GPIO_SPEED_LOW,
   };

   nx_uart_config_t uart_cfg = {
       .baudrate    = 115200,
       .word_length = 8,
       .stop_bits   = 1,
       .parity      = 0,
   };

**Factory Functions**

Create devices using factory functions:

.. code-block:: c

   /* Simple creation */
   nx_gpio_write_t* led = nx_factory_gpio_write('A', 5);

   /* Creation with configuration */
   nx_gpio_t* gpio = nx_factory_gpio_with_config('A', 5, &gpio_cfg);

   /* Release when done */
   nx_factory_gpio_release(gpio);

**Interface Methods**

Access device functionality through interface methods:

.. code-block:: c

   /* GPIO operations */
   led->write(led, 1);
   led->toggle(led);
   uint8_t state = led->read(led);

   /* UART operations */
   nx_tx_sync_t* tx = uart->get_tx_sync(uart);
   tx->send(tx, data, len, timeout);

Peripheral Types
~~~~~~~~~~~~~~~~

**GPIO** - General Purpose I/O

* Digital input/output
* Interrupt support
* Pull-up/pull-down configuration

**UART** - Serial Communication

* Asynchronous serial
* Configurable baud rate
* TX/RX with timeouts

**SPI** - Serial Peripheral Interface

* Master/slave modes
* Full-duplex communication
* Configurable clock polarity/phase

**I2C** - Inter-Integrated Circuit

* Master/slave modes
* 7-bit/10-bit addressing
* Clock stretching support

**ADC** - Analog-to-Digital Converter

* Single/continuous conversion
* Multiple channels
* DMA support

**Timer** - Hardware Timers

* PWM generation
* Input capture
* Output compare

See :doc:`../user_guide/hal` for detailed HAL documentation.

OS Abstraction Layer (OSAL)
----------------------------

Purpose
~~~~~~~

OSAL provides portable RTOS primitives, allowing applications to work with different operating systems.

**Supported Backends**:

* Bare-metal (cooperative scheduling)
* FreeRTOS
* RT-Thread
* Zephyr

Key Concepts
~~~~~~~~~~~~

**Tasks/Threads**

Create and manage concurrent execution:

.. code-block:: c

   /* Task function */
   void my_task(void* arg) {
       while (1) {
           /* Task work */
           osal_task_delay(1000);
       }
   }

   /* Create task */
   osal_task_handle_t task;
   osal_task_create(my_task, "my_task", 512, NULL, 1, &task);

**Mutexes**

Mutual exclusion for shared resources:

.. code-block:: c

   /* Create mutex */
   osal_mutex_handle_t mutex;
   osal_mutex_create(&mutex);

   /* Lock */
   osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);

   /* Critical section */
   shared_resource++;

   /* Unlock */
   osal_mutex_unlock(mutex);

**Semaphores**

Signaling and resource counting:

.. code-block:: c

   /* Create binary semaphore */
   osal_sem_handle_t sem;
   osal_sem_create(&sem, 0, 1);

   /* Wait for signal */
   osal_sem_wait(sem, OSAL_WAIT_FOREVER);

   /* Signal */
   osal_sem_give(sem);

**Message Queues**

Inter-task communication:

.. code-block:: c

   /* Create queue */
   osal_queue_handle_t queue;
   osal_queue_create(&queue, 10, sizeof(message_t));

   /* Send message */
   message_t msg = {.id = 1, .data = 42};
   osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);

   /* Receive message */
   message_t received;
   osal_queue_receive(queue, &received, OSAL_WAIT_FOREVER);

**Software Timers**

Periodic or one-shot timers:

.. code-block:: c

   /* Timer callback */
   void timer_callback(void* arg) {
       /* Timer expired */
   }

   /* Create timer */
   osal_timer_handle_t timer;
   osal_timer_create(&timer, "timer", 1000, true, NULL, timer_callback);

   /* Start timer */
   osal_timer_start(timer);

Synchronization Patterns
~~~~~~~~~~~~~~~~~~~~~~~~~

**Producer-Consumer**

.. code-block:: c

   /* Producer task */
   void producer_task(void* arg) {
       while (1) {
           data_t data = produce_data();
           osal_queue_send(queue, &data, OSAL_WAIT_FOREVER);
       }
   }

   /* Consumer task */
   void consumer_task(void* arg) {
       while (1) {
           data_t data;
           osal_queue_receive(queue, &data, OSAL_WAIT_FOREVER);
           consume_data(&data);
       }
   }

**Resource Protection**

.. code-block:: c

   /* Shared resource with mutex */
   static int shared_counter = 0;
   static osal_mutex_handle_t counter_mutex;

   void increment_counter(void) {
       osal_mutex_lock(counter_mutex, OSAL_WAIT_FOREVER);
       shared_counter++;
       osal_mutex_unlock(counter_mutex);
   }

See :doc:`../user_guide/osal` for detailed OSAL documentation.

Framework Layer
---------------

Logging Framework
~~~~~~~~~~~~~~~~~

Flexible logging with multiple backends and log levels.

**Log Levels**:

.. code-block:: c

   LOG_ERROR("Critical error: %d", error_code);
   LOG_WARN("Warning: %s", warning_msg);
   LOG_INFO("Information: %d", value);
   LOG_DEBUG("Debug: %s", debug_info);

**Module Filtering**:

.. code-block:: c

   /* Define module */
   #define LOG_MODULE "MyModule"

   /* Log with module tag */
   LOG_INFO("Module-specific log");

**Backends**:

* Console (stdout)
* UART
* File
* Custom backends

See :doc:`../user_guide/log` for detailed logging documentation.

Shell Framework
~~~~~~~~~~~~~~~

Interactive command-line interface for debugging and control.

**Command Registration**:

.. code-block:: c

   static int cmd_handler(int argc, char* argv[]) {
       shell_printf("Command executed\n");
       return 0;
   }

   static const shell_command_t cmd_def = {
       .name = "mycommand",
       .handler = cmd_handler,
       .help = "My custom command",
       .usage = "mycommand [args]",
       .completion = NULL
   };

   shell_register_command(&cmd_def);

**Features**:

* Command history
* Tab completion
* Built-in commands (help, clear, reboot)
* Custom command registration

See :doc:`../user_guide/shell` for detailed shell documentation.

Configuration Framework
~~~~~~~~~~~~~~~~~~~~~~~

Key-value configuration storage with persistence.

**Basic Usage**:

.. code-block:: c

   /* Store values */
   config_set_i32("app.timeout", 5000);
   config_set_str("device.name", "MyDevice");
   config_set_bool("feature.enabled", true);

   /* Retrieve values */
   int32_t timeout;
   config_get_i32("app.timeout", &timeout, 0);

**Namespaces**:

.. code-block:: c

   /* Open namespace */
   config_ns_handle_t wifi_ns;
   config_open_namespace("wifi", &wifi_ns);

   /* Store in namespace */
   config_ns_set_str(wifi_ns, "ssid", "MyNetwork");

   /* Close namespace */
   config_close_namespace(wifi_ns);

**Import/Export**:

.. code-block:: c

   /* Export to JSON */
   char buffer[1024];
   size_t size;
   config_export(CONFIG_FORMAT_JSON, 0, buffer, sizeof(buffer), &size);

   /* Import from JSON */
   config_import(CONFIG_FORMAT_JSON, 0, buffer, size);

See :doc:`../user_guide/config` for detailed configuration documentation.

Configuration System (Kconfig)
-------------------------------

Purpose
~~~~~~~

Kconfig provides compile-time configuration for:

* Platform selection
* Peripheral enablement
* Feature selection
* Resource allocation

Key Concepts
~~~~~~~~~~~~

**Configuration Options**

Define what features to include:

.. code-block:: kconfig

   config HAL_GPIO
       bool "Enable GPIO support"
       default y
       help
         Enable GPIO peripheral support

**Dependencies**

Express relationships between options:

.. code-block:: kconfig

   config HAL_SPI
       bool "Enable SPI support"
       depends on HAL_GPIO
       help
         Enable SPI peripheral support

**Defaults**

Provide sensible defaults:

.. code-block:: kconfig

   config HAL_UART_BAUDRATE
       int "Default UART baud rate"
       default 115200
       range 9600 921600

**Generated Header**

Kconfig generates ``nexus_config.h``:

.. code-block:: c

   #define CONFIG_HAL_GPIO 1
   #define CONFIG_HAL_UART 1
   #define CONFIG_HAL_UART_BAUDRATE 115200

Usage in Code
~~~~~~~~~~~~~

.. code-block:: c

   #include "nexus_config.h"

   #ifdef CONFIG_HAL_GPIO
   /* GPIO code */
   #endif

   #ifdef CONFIG_HAL_UART
   /* UART code */
   #endif

See :doc:`configuration` for detailed Kconfig usage.

Memory Management
-----------------

Static Allocation
~~~~~~~~~~~~~~~~~

All memory allocated at compile time:

**Advantages**:

* Deterministic behavior
* No fragmentation
* Suitable for safety-critical systems

**Disadvantages**:

* Less flexible
* May waste memory

**Example**:

.. code-block:: c

   #define MAX_DEVICES 10
   static device_t devices[MAX_DEVICES];

Dynamic Allocation
~~~~~~~~~~~~~~~~~~

Runtime memory allocation:

**Advantages**:

* Flexible
* Efficient memory usage

**Disadvantages**:

* Fragmentation risk
* Non-deterministic

**Example**:

.. code-block:: c

   device_t* device = malloc(sizeof(device_t));
   /* Use device */
   free(device);

Pool Allocation
~~~~~~~~~~~~~~~

Fixed-size memory pools:

**Advantages**:

* Fast allocation
* No fragmentation
* Predictable behavior

**Disadvantages**:

* Fixed block size
* May waste memory

**Example**:

.. code-block:: c

   /* Create pool */
   osal_pool_handle_t pool;
   osal_pool_create(&pool, 10, sizeof(device_t));

   /* Allocate from pool */
   device_t* device = osal_pool_alloc(pool);

   /* Return to pool */
   osal_pool_free(pool, device);

Error Handling
--------------

Status Codes
~~~~~~~~~~~~

All APIs return status codes:

.. code-block:: c

   typedef enum {
       HAL_OK = 0,
       HAL_ERR_FAIL,
       HAL_ERR_PARAM,
       HAL_ERR_STATE,
       HAL_ERR_TIMEOUT,
       HAL_ERR_NO_MEMORY,
   } hal_status_t;

Error Checking
~~~~~~~~~~~~~~

Always check return values:

.. code-block:: c

   hal_status_t status = hal_gpio_init(port, pin, &config);
   if (status != HAL_OK) {
       LOG_ERROR("GPIO init failed: %d", status);
       return status;
   }

Error Recovery
~~~~~~~~~~~~~~

Implement appropriate recovery strategies:

.. code-block:: c

   /* Retry on timeout */
   int retries = 3;
   while (retries--) {
       status = hal_uart_send(uart, data, len, timeout);
       if (status == HAL_OK) {
           break;
       }
       if (status == HAL_ERR_TIMEOUT) {
           continue;  /* Retry */
       }
       return status;  /* Other errors */
   }

Resource Management
-------------------

Initialization
~~~~~~~~~~~~~~

Proper initialization order:

.. code-block:: c

   int main(void) {
       /* 1. Initialize HAL */
       nx_hal_init();

       /* 2. Initialize OSAL */
       osal_init();

       /* 3. Initialize frameworks */
       log_init(NULL);
       shell_init(&shell_config);

       /* 4. Initialize application */
       app_init();

       /* 5. Start scheduler (if using RTOS) */
       osal_start_scheduler();

       return 0;
   }

Cleanup
~~~~~~~

Proper resource cleanup:

.. code-block:: c

   void app_cleanup(void) {
       /* Release devices */
       nx_factory_gpio_release(led);
       nx_factory_uart_release(uart);

       /* Deinitialize frameworks */
       shell_deinit();
       log_deinit();

       /* Deinitialize HAL */
       nx_hal_deinit();
   }

Best Practices
--------------

Code Organization
~~~~~~~~~~~~~~~~~

1. **Separate concerns**: One file per module
2. **Use headers**: Public API in headers, implementation in source
3. **Minimize dependencies**: Reduce coupling between modules
4. **Document interfaces**: Use Doxygen comments

Error Handling
~~~~~~~~~~~~~~

1. **Check all return values**: Never ignore errors
2. **Log errors**: Use logging framework
3. **Fail gracefully**: Implement recovery strategies
4. **Use assertions**: For programming errors

Resource Management
~~~~~~~~~~~~~~~~~~~

1. **Initialize before use**: Always initialize resources
2. **Clean up**: Release resources when done
3. **Avoid leaks**: Match allocations with deallocations
4. **Use RAII pattern**: Acquire resources in initialization

Concurrency
~~~~~~~~~~~

1. **Protect shared data**: Use mutexes
2. **Avoid deadlocks**: Acquire locks in consistent order
3. **Minimize critical sections**: Hold locks briefly
4. **Use queues**: For inter-task communication

Testing
~~~~~~~

1. **Write unit tests**: Test individual components
2. **Test error paths**: Don't just test happy path
3. **Use mocks**: Isolate dependencies
4. **Automate tests**: Run tests in CI/CD

Next Steps
----------

Now that you understand core concepts:

1. :doc:`configuration` - Master Kconfig configuration
2. :doc:`examples_tour` - Explore complex examples
3. :doc:`../user_guide/architecture` - Deep dive into architecture
4. :doc:`../tutorials/index` - Follow step-by-step tutorials

See Also
--------

* :doc:`../user_guide/hal` - HAL API reference
* :doc:`../user_guide/osal` - OSAL API reference
* :doc:`../user_guide/log` - Logging framework
* :doc:`../development/coding_standards` - Coding standards
