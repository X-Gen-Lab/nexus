Best Practices
==============

Comprehensive guide to best practices for developing robust, maintainable, and efficient Nexus applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Following best practices ensures code quality, reliability, and maintainability. This guide covers coding standards, design patterns, and development workflows for Nexus.

**Key Principles:**

* Write clean, readable code
* Design for testability
* Handle errors gracefully
* Document thoroughly
* Optimize wisely
* Test comprehensively

Code Organization
-----------------

Project Structure
~~~~~~~~~~~~~~~~~

**Recommended Structure:**

.. code-block:: text

   my_project/
   ├── src/                    # Source files
   │   ├── main.c
   │   ├── app/                # Application logic
   │   │   ├── app_init.c
   │   │   ├── app_task.c
   │   │   └── app_config.c
   │   ├── drivers/            # Custom drivers
   │   │   └── sensor_driver.c
   │   └── utils/              # Utility functions
   │       └── helpers.c
   ├── include/                # Header files
   │   ├── app/
   │   ├── drivers/
   │   └── utils/
   ├── tests/                  # Unit tests
   │   ├── test_app.cpp
   │   └── test_drivers.cpp
   ├── docs/                   # Documentation
   ├── CMakeLists.txt
   ├── .config                 # Kconfig configuration
   └── README.md

**Module Organization:**

.. code-block:: c

   /* sensor_driver.h - Public interface */
   #ifndef SENSOR_DRIVER_H
   #define SENSOR_DRIVER_H

   /**
    * \file            sensor_driver.h
    * \brief           Temperature sensor driver interface
    * \author          Nexus Team
    */

   #include <stdint.h>

   /**
    * \brief           Initialize sensor
    * \return          0 on success, negative on error
    */
   int sensor_init(void);

   /**
    * \brief           Read temperature
    * \param[out]      temp: Temperature in degrees Celsius
    * \return          0 on success, negative on error
    */
   int sensor_read_temperature(float* temp);

   #endif /* SENSOR_DRIVER_H */

.. code-block:: c

   /* sensor_driver.c - Implementation */
   /**
    * \file            sensor_driver.c
    * \brief           Temperature sensor driver implementation
    * \author          Nexus Team
    * \version         1.0.0
    * \date            2026-01-25
    */

   #include "sensor_driver.h"
   #include "hal/nx_i2c.h"
   #include "log/log.h"

   #define LOG_MODULE "sensor"

   /*---------------------------------------------------------------------------*/
   /* Private Definitions                                                       */
   /*---------------------------------------------------------------------------*/

   #define SENSOR_I2C_ADDR     0x48
   #define SENSOR_TEMP_REG     0x00

   /*---------------------------------------------------------------------------*/
   /* Private Variables                                                         */
   /*---------------------------------------------------------------------------*/

   static nx_i2c_t* i2c = NULL;

   /*---------------------------------------------------------------------------*/
   /* Private Functions                                                         */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Read sensor register
    */
   static int read_register(uint8_t reg, uint8_t* value)
   {
       /* Implementation */
       return 0;
   }

   /*---------------------------------------------------------------------------*/
   /* Public Functions                                                          */
   /*---------------------------------------------------------------------------*/

   /**
    * \brief           Initialize sensor
    * \details         Configures I2C and verifies sensor presence
    */
   int sensor_init(void)
   {
       /* Implementation */
       return 0;
   }

Naming Conventions
~~~~~~~~~~~~~~~~~~

**Follow Consistent Naming:**

.. code-block:: c

   /* Functions: snake_case */
   void initialize_system(void);
   int read_sensor_data(uint8_t* buffer, size_t len);

   /* Variables: snake_case */
   int sensor_count = 0;
   float temperature_celsius = 0.0f;

   /* Constants: UPPER_SNAKE_CASE */
   #define MAX_BUFFER_SIZE 256
   #define DEFAULT_TIMEOUT 1000

   /* Types: snake_case_t */
   typedef struct {
       uint32_t id;
       char name[32];
   } device_info_t;

   /* Enums: UPPER_SNAKE_CASE */
   typedef enum {
       STATE_IDLE = 0,
       STATE_ACTIVE,
       STATE_ERROR,
   } system_state_t;

   /* Private functions: static */
   static void internal_helper(void);

   /* Module prefix for public API */
   int sensor_init(void);
   int sensor_read(void);

Error Handling
--------------

Return Status Codes
~~~~~~~~~~~~~~~~~~~

**Always Return Status:**

.. code-block:: c

   /* Good: Return status code */
   int initialize_device(device_t* device)
   {
       if (!device) {
           return -EINVAL;  /* Invalid parameter */
       }

       if (!device_is_present()) {
           return -ENODEV;  /* Device not found */
       }

       if (device_init_hardware() != 0) {
           return -EIO;  /* I/O error */
       }

       return 0;  /* Success */
   }

   /* Usage */
   int result = initialize_device(&my_device);
   if (result != 0) {
       LOG_ERROR("Device initialization failed: %d", result);
       return result;
   }

Check All Return Values
~~~~~~~~~~~~~~~~~~~~~~~~

**Never Ignore Errors:**

.. code-block:: c

   /* Bad: Ignoring return value */
   void bad_example(void)
   {
       osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
       /* What if lock failed? */
   }

   /* Good: Check return value */
   void good_example(void)
   {
       osal_status_t status = osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);
       if (status != OSAL_OK) {
           LOG_ERROR("Failed to acquire mutex: %d", status);
           return;
       }

       /* Critical section */

       osal_mutex_unlock(mutex);
   }

Use Assertions
~~~~~~~~~~~~~~

**Validate Preconditions:**

.. code-block:: c

   #include "hal/nx_assert.h"

   void process_buffer(const uint8_t* buffer, size_t len)
   {
       /* Assert preconditions */
       NX_ASSERT(buffer != NULL);
       NX_ASSERT(len > 0);
       NX_ASSERT(len <= MAX_BUFFER_SIZE);

       /* Process buffer */
       for (size_t i = 0; i < len; i++) {
           process_byte(buffer[i]);
       }
   }

Graceful Degradation
~~~~~~~~~~~~~~~~~~~~

**Handle Failures Gracefully:**

.. code-block:: c

   void sensor_task(void* arg)
   {
       int retry_count = 0;
       const int MAX_RETRIES = 3;

       while (1) {
           float temperature;
           int result = sensor_read_temperature(&temperature);

           if (result == 0) {
               /* Success */
               LOG_INFO("Temperature: %.1f°C", temperature);
               retry_count = 0;
           } else {
               /* Failure */
               LOG_WARN("Sensor read failed: %d", result);
               retry_count++;

               if (retry_count >= MAX_RETRIES) {
                   LOG_ERROR("Sensor failed after %d retries", MAX_RETRIES);
                   /* Enter safe mode or use default value */
                   temperature = DEFAULT_TEMPERATURE;
                   retry_count = 0;
               }
           }

           osal_task_delay(1000);
       }
   }

Resource Management
-------------------

RAII Pattern
~~~~~~~~~~~~

**Acquire Resources in Initialization:**

.. code-block:: c

   /* Good: RAII-style resource management */
   int process_data_file(const char* filename)
   {
       int result = -1;
       file_t* file = NULL;
       uint8_t* buffer = NULL;

       /* Acquire resources */
       file = file_open(filename, FILE_MODE_READ);
       if (!file) {
           LOG_ERROR("Failed to open file: %s", filename);
           goto cleanup;
       }

       buffer = osal_malloc(BUFFER_SIZE);
       if (!buffer) {
           LOG_ERROR("Failed to allocate buffer");
           goto cleanup;
       }

       /* Use resources */
       size_t bytes_read = file_read(file, buffer, BUFFER_SIZE);
       process_buffer(buffer, bytes_read);

       result = 0;  /* Success */

   cleanup:
       /* Release resources in reverse order */
       if (buffer) {
           osal_free(buffer);
       }
       if (file) {
           file_close(file);
       }

       return result;
   }

Avoid Resource Leaks
~~~~~~~~~~~~~~~~~~~~

**Always Release Resources:**

.. code-block:: c

   void use_gpio(void)
   {
       nx_gpio_write_t* led = nx_factory_gpio_write('D', 12);
       if (!led) {
           return;
       }

       /* Use GPIO */
       led->write(led, 1);

       /* Always release */
       nx_factory_gpio_release((nx_gpio_t*)led);
   }

   void use_uart(void)
   {
       nx_uart_t* uart = nx_factory_uart(0);
       if (!uart) {
           return;
       }

       /* Use UART */
       nx_tx_sync_t* tx = uart->get_tx_sync(uart);
       tx->send(tx, data, len, 1000);

       /* Always release */
       nx_factory_uart_release(uart);
   }

Concurrency
-----------

Protect Shared Resources
~~~~~~~~~~~~~~~~~~~~~~~~~

**Use Mutexes for Shared Data:**

.. code-block:: c

   /* Shared resource */
   static int shared_counter = 0;
   static osal_mutex_handle_t counter_mutex;

   void init_shared_resource(void)
   {
       osal_mutex_create(&counter_mutex);
   }

   void increment_counter(void)
   {
       osal_mutex_lock(counter_mutex, OSAL_WAIT_FOREVER);
       shared_counter++;
       osal_mutex_unlock(counter_mutex);
   }

   int get_counter(void)
   {
       int value;
       osal_mutex_lock(counter_mutex, OSAL_WAIT_FOREVER);
       value = shared_counter;
       osal_mutex_unlock(counter_mutex);
       return value;
   }

Minimize Critical Sections
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Hold Locks Briefly:**

.. code-block:: c

   /* Bad: Long critical section */
   void process_data_bad(void)
   {
       osal_mutex_lock(data_mutex, OSAL_WAIT_FOREVER);

       /* Long operation while holding lock */
       for (int i = 0; i < 1000; i++) {
           process_item(shared_data[i]);
       }

       osal_mutex_unlock(data_mutex);
   }

   /* Good: Short critical section */
   void process_data_good(void)
   {
       uint8_t local_copy[1000];

       /* Copy data while holding lock */
       osal_mutex_lock(data_mutex, OSAL_WAIT_FOREVER);
       memcpy(local_copy, shared_data, sizeof(local_copy));
       osal_mutex_unlock(data_mutex);

       /* Process local copy without lock */
       for (int i = 0; i < 1000; i++) {
           process_item(local_copy[i]);
       }
   }

Avoid Deadlocks
~~~~~~~~~~~~~~~

**Acquire Locks in Consistent Order:**

.. code-block:: c

   /* Define lock order */
   static osal_mutex_handle_t mutex_a;
   static osal_mutex_handle_t mutex_b;

   /* Always acquire in same order: A then B */
   void function_1(void)
   {
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);

       /* Critical section */

       osal_mutex_unlock(mutex_b);
       osal_mutex_unlock(mutex_a);
   }

   void function_2(void)
   {
       /* Same order: A then B */
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);

       /* Critical section */

       osal_mutex_unlock(mutex_b);
       osal_mutex_unlock(mutex_a);
   }

Memory Management
-----------------

Prefer Static Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~

**Use Static Allocation When Possible:**

.. code-block:: c

   /* Good: Static allocation */
   #define MAX_DEVICES 10
   static device_t devices[MAX_DEVICES];
   static size_t device_count = 0;

   device_t* allocate_device(void)
   {
       if (device_count >= MAX_DEVICES) {
           return NULL;
       }
       return &devices[device_count++];
   }

Check Allocation Failures
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Always Check malloc/new:**

.. code-block:: c

   void process_large_data(void)
   {
       uint8_t* buffer = osal_malloc(LARGE_SIZE);
       if (!buffer) {
           LOG_ERROR("Failed to allocate %d bytes", LARGE_SIZE);
           /* Handle error - use smaller buffer or fail gracefully */
           return;
       }

       /* Use buffer */
       process_buffer(buffer, LARGE_SIZE);

       /* Free buffer */
       osal_free(buffer);
   }

Initialize Variables
~~~~~~~~~~~~~~~~~~~~

**Always Initialize:**

.. code-block:: c

   /* Good: Initialize variables */
   void good_example(void)
   {
       int count = 0;
       float temperature = 0.0f;
       char buffer[64] = {0};
       device_t* device = NULL;

       /* Use variables */
   }

   /* Bad: Uninitialized variables */
   void bad_example(void)
   {
       int count;  /* Undefined value */
       float temperature;  /* Undefined value */
       /* Using these causes undefined behavior */
   }

Logging and Debugging
---------------------

Use Appropriate Log Levels
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Choose Correct Level:**

.. code-block:: c

   #define LOG_MODULE "app"
   #include "log/log.h"

   void application_function(void)
   {
       /* TRACE: Detailed flow information */
       LOG_TRACE("Entering application_function");

       /* DEBUG: Development information */
       LOG_DEBUG("Processing %d items", item_count);

       /* INFO: Normal operation */
       LOG_INFO("Application started successfully");

       /* WARN: Recoverable issues */
       if (retry_count > 0) {
           LOG_WARN("Operation required %d retries", retry_count);
       }

       /* ERROR: Failures */
       if (result != 0) {
           LOG_ERROR("Operation failed with error %d", result);
       }

       /* FATAL: Critical errors */
       if (critical_failure) {
           LOG_FATAL("Critical system failure - halting");
       }
   }

Add Context to Logs
~~~~~~~~~~~~~~~~~~~

**Include Relevant Information:**

.. code-block:: c

   /* Bad: Insufficient context */
   LOG_ERROR("Failed");

   /* Good: Detailed context */
   LOG_ERROR("Failed to initialize sensor %d: error code %d, retry %d/%d",
             sensor_id, error_code, retry_count, MAX_RETRIES);

   /* Good: Include state information */
   LOG_DEBUG("Processing packet: type=%d, len=%d, seq=%lu, crc=0x%04X",
             packet->type, packet->len, packet->sequence, packet->crc);

Use Module Tags
~~~~~~~~~~~~~~~

**Organize Logs by Module:**

.. code-block:: c

   /* sensor_driver.c */
   #define LOG_MODULE "sensor"
   #include "log/log.h"

   void sensor_init(void)
   {
       LOG_INFO("Initializing sensor");  /* [sensor] Initializing sensor */
   }

   /* network.c */
   #define LOG_MODULE "network"
   #include "log/log.h"

   void network_connect(void)
   {
       LOG_INFO("Connecting");  /* [network] Connecting */
   }

Testing
-------

Write Testable Code
~~~~~~~~~~~~~~~~~~~

**Design for Testability:**

.. code-block:: c

   /* Bad: Hard to test (direct hardware access) */
   void bad_example(void)
   {
       GPIOD->ODR |= (1 << 12);  /* Direct register access */
   }

   /* Good: Testable (uses abstraction) */
   void good_example(nx_gpio_write_t* led)
   {
       led->write(led, 1);  /* Can mock in tests */
   }

   /* Test */
   TEST(AppTest, GoodExample) {
       MockGPIO mock_led;
       good_example(&mock_led);
       EXPECT_EQ(mock_led.last_value, 1);
   }

Test Edge Cases
~~~~~~~~~~~~~~~

**Test Boundary Conditions:**

.. code-block:: cpp

   TEST(BufferTest, EdgeCases) {
       /* Test empty buffer */
       EXPECT_EQ(process_buffer(NULL, 0), -EINVAL);

       /* Test maximum size */
       uint8_t large_buffer[MAX_SIZE];
       EXPECT_EQ(process_buffer(large_buffer, MAX_SIZE), 0);

       /* Test overflow */
       EXPECT_EQ(process_buffer(large_buffer, MAX_SIZE + 1), -EINVAL);

       /* Test minimum size */
       uint8_t small_buffer[1];
       EXPECT_EQ(process_buffer(small_buffer, 1), 0);
   }

Test Error Paths
~~~~~~~~~~~~~~~~

**Test Failure Scenarios:**

.. code-block:: cpp

   TEST(SensorTest, ErrorHandling) {
       /* Test sensor not present */
       mock_sensor_present(false);
       EXPECT_EQ(sensor_init(), -ENODEV);

       /* Test I2C failure */
       mock_i2c_fail(true);
       EXPECT_EQ(sensor_read(), -EIO);

       /* Test timeout */
       mock_sensor_timeout(true);
       EXPECT_EQ(sensor_read(), -ETIMEDOUT);
   }

Documentation
-------------

Document Public APIs
~~~~~~~~~~~~~~~~~~~~

**Use Doxygen Comments:**

.. code-block:: c

   /**
    * \brief           Initialize the sensor subsystem
    * \param[in]       config: Configuration structure
    * \return          0 on success, negative error code on failure
    * \note            Must be called before any other sensor functions
    * \warning         Not thread-safe during initialization
    */
   int sensor_subsystem_init(const sensor_config_t* config);

Document Complex Logic
~~~~~~~~~~~~~~~~~~~~~~

**Explain Non-Obvious Code:**

.. code-block:: c

   /**
    * \brief           Calculate CRC-16 using polynomial 0x1021
    * \details         Uses table-driven algorithm for performance.
    *                  The lookup table is generated at compile time.
    */
   uint16_t calculate_crc16(const uint8_t* data, size_t len)
   {
       uint16_t crc = 0xFFFF;

       for (size_t i = 0; i < len; i++) {
           /* XOR byte into CRC */
           uint8_t index = (crc >> 8) ^ data[i];
           crc = (crc << 8) ^ crc_table[index];
       }

       return crc;
   }

Maintain README
~~~~~~~~~~~~~~~

**Keep README Updated:**

.. code-block:: markdown

   # My Project

   ## Description
   Brief description of the project.

   ## Features
   - Feature 1
   - Feature 2

   ## Requirements
   - CMake 3.16+
   - ARM GCC toolchain
   - Python 3.8+

   ## Building
   ```bash
   cmake -B build
   cmake --build build
   ```

   ## Configuration
   See `.config` for configuration options.

   ## Testing
   ```bash
   python scripts/test/test.py
   ```

   ## License
   MIT License

Performance
-----------

Profile Before Optimizing
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Measure First:**

.. code-block:: c

   /* Identify bottlenecks before optimizing */
   void profile_application(void)
   {
       PROFILE_START(data_processing);
       process_data();
       PROFILE_END(data_processing);

       PROFILE_START(network_send);
       send_network_data();
       PROFILE_END(network_send);

       /* Optimize the slowest part first */
   }

Optimize Hot Paths
~~~~~~~~~~~~~~~~~~

**Focus on Frequently Executed Code:**

.. code-block:: c

   /* Optimize inner loops */
   void process_samples(const int16_t* samples, size_t count)
   {
       /* This loop runs millions of times - optimize it */
       for (size_t i = 0; i < count; i++) {
           /* Use efficient operations */
           int32_t value = samples[i];
           value = (value * gain) >> 8;  /* Shift instead of divide */
           output[i] = (int16_t)value;
       }
   }

Don't Sacrifice Readability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Balance Optimization and Clarity:**

.. code-block:: c

   /* Bad: Overly optimized, hard to understand */
   void bad_example(uint8_t* d, uint8_t* s, size_t n)
   {
       while (n--) *d++ = *s++;
   }

   /* Good: Clear and efficient */
   void good_example(uint8_t* dest, const uint8_t* src, size_t len)
   {
       memcpy(dest, src, len);  /* Clear intent, compiler optimizes */
   }

Code Review Checklist
---------------------

**Before Submitting Code:**

- [ ] Code follows style guidelines
- [ ] All functions documented
- [ ] Error handling implemented
- [ ] Resources properly managed
- [ ] No memory leaks
- [ ] Thread-safe where needed
- [ ] Tests written and passing
- [ ] No compiler warnings
- [ ] Code reviewed by peer
- [ ] Documentation updated

**Review Checklist:**

- [ ] Code is readable and maintainable
- [ ] Logic is correct
- [ ] Edge cases handled
- [ ] Error paths tested
- [ ] Performance acceptable
- [ ] Security considerations addressed
- [ ] No code duplication
- [ ] Follows project conventions

See Also
--------

* :doc:`../development/coding_standards` - Coding Standards
* :doc:`../development/code_review_guidelines` - Code Review Guidelines
* :doc:`testing` - Testing Guide
* :doc:`debugging` - Debugging Guide
* :doc:`performance` - Performance Optimization

