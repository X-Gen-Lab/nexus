Error Handling
==============

Comprehensive guide to error handling strategies and best practices in Nexus applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Robust error handling is essential for reliable embedded systems. This guide covers error detection, reporting, recovery, and prevention strategies.

**Error Handling Goals:**

* Detect errors early
* Report errors clearly
* Recover gracefully
* Prevent system failures
* Maintain system integrity

Error Types
-----------

Hardware Errors
~~~~~~~~~~~~~~~

**Common Hardware Errors:**

* Peripheral initialization failure
* Communication timeouts
* Invalid sensor readings
* Memory access violations
* Power supply issues

**Example:**

.. code-block:: c

   nx_status_t init_sensor(void)
   {
       nx_i2c_t* i2c = nx_factory_i2c(0);
       if (!i2c) {
           LOG_ERROR("Failed to get I2C device");
           return NX_ERR_NO_DEVICE;
       }

       /* Check sensor presence */
       uint8_t device_id;
       nx_status_t status = i2c_read_register(i2c, SENSOR_ID_REG, &device_id);
       if (status != NX_OK) {
           LOG_ERROR("Failed to read sensor ID: %d", status);
           nx_factory_i2c_release(i2c);
           return NX_ERR_IO;
       }

       if (device_id != EXPECTED_SENSOR_ID) {
           LOG_ERROR("Invalid sensor ID: 0x%02X (expected 0x%02X)",
                     device_id, EXPECTED_SENSOR_ID);
           nx_factory_i2c_release(i2c);
           return NX_ERR_INVALID_DEVICE;
       }

       nx_factory_i2c_release(i2c);
       return NX_OK;
   }

Software Errors
~~~~~~~~~~~~~~~

**Common Software Errors:**

* Invalid parameters
* Buffer overflows
* Null pointer dereferences
* Resource exhaustion
* Logic errors

**Example:**

.. code-block:: c

   int process_buffer(const uint8_t* buffer, size_t len)
   {
       /* Validate parameters */
       if (!buffer) {
           LOG_ERROR("Null buffer pointer");
           return -EINVAL;
       }

       if (len == 0) {
           LOG_ERROR("Zero length buffer");
           return -EINVAL;
       }

       if (len > MAX_BUFFER_SIZE) {
           LOG_ERROR("Buffer too large: %zu (max %d)", len, MAX_BUFFER_SIZE);
           return -EINVAL;
       }

       /* Process buffer */
       for (size_t i = 0; i < len; i++) {
           if (process_byte(buffer[i]) != 0) {
               LOG_ERROR("Failed to process byte at index %zu", i);
               return -EIO;
           }
       }

       return 0;
   }

System Errors
~~~~~~~~~~~~~

**Common System Errors:**

* Out of memory
* Stack overflow
* Deadlock
* Task starvation
* Watchdog timeout

**Example:**

.. code-block:: c

   void* allocate_buffer(size_t size)
   {
       void* buffer = osal_malloc(size);
       if (!buffer) {
           LOG_ERROR("Out of memory: failed to allocate %zu bytes", size);

           /* Check heap status */
           size_t free_heap = osal_get_free_heap_size();
           LOG_ERROR("Free heap: %zu bytes", free_heap);

           /* Try to recover */
           cleanup_unused_resources();

           /* Retry allocation */
           buffer = osal_malloc(size);
           if (!buffer) {
               LOG_FATAL("Memory allocation failed after cleanup");
               /* Enter safe mode */
               enter_safe_mode();
           }
       }

       return buffer;
   }

Error Codes
-----------

Standard Error Codes
~~~~~~~~~~~~~~~~~~~~

**Nexus Error Codes:**

.. code-block:: c

   typedef enum {
       NX_OK = 0,                  /**< Success */
       NX_ERR_FAIL = -1,           /**< General failure */
       NX_ERR_PARAM = -2,          /**< Invalid parameter */
       NX_ERR_STATE = -3,          /**< Invalid state */
       NX_ERR_TIMEOUT = -4,        /**< Operation timeout */
       NX_ERR_NO_MEM = -5,         /**< Out of memory */
       NX_ERR_NO_DEVICE = -6,      /**< Device not found */
       NX_ERR_IO = -7,             /**< I/O error */
       NX_ERR_BUSY = -8,           /**< Resource busy */
       NX_ERR_NOT_SUPPORTED = -9,  /**< Not supported */
   } nx_status_t;

**POSIX-Style Error Codes:**

.. code-block:: c

   #include <errno.h>

   int my_function(void)
   {
       if (error_condition) {
           return -EINVAL;  /* Invalid argument */
       }

       if (timeout) {
           return -ETIMEDOUT;  /* Timeout */
       }

       if (no_memory) {
           return -ENOMEM;  /* Out of memory */
       }

       return 0;  /* Success */
   }

Error Code Conversion
~~~~~~~~~~~~~~~~~~~~~

**Convert Between Error Systems:**

.. code-block:: c

   nx_status_t errno_to_nx_status(int err)
   {
       switch (err) {
       case 0:
           return NX_OK;
       case EINVAL:
           return NX_ERR_PARAM;
       case ETIMEDOUT:
           return NX_ERR_TIMEOUT;
       case ENOMEM:
           return NX_ERR_NO_MEM;
       case EIO:
           return NX_ERR_IO;
       case EBUSY:
           return NX_ERR_BUSY;
       default:
           return NX_ERR_FAIL;
       }
   }

   const char* nx_status_to_string(nx_status_t status)
   {
       switch (status) {
       case NX_OK:
           return "Success";
       case NX_ERR_FAIL:
           return "General failure";
       case NX_ERR_PARAM:
           return "Invalid parameter";
       case NX_ERR_STATE:
           return "Invalid state";
       case NX_ERR_TIMEOUT:
           return "Timeout";
       case NX_ERR_NO_MEM:
           return "Out of memory";
       case NX_ERR_NO_DEVICE:
           return "Device not found";
       case NX_ERR_IO:
           return "I/O error";
       case NX_ERR_BUSY:
           return "Resource busy";
       case NX_ERR_NOT_SUPPORTED:
           return "Not supported";
       default:
           return "Unknown error";
       }
   }

Error Detection
---------------

Parameter Validation
~~~~~~~~~~~~~~~~~~~~

**Validate All Inputs:**

.. code-block:: c

   int configure_device(device_t* device, const config_t* config)
   {
       /* Validate pointers */
       if (!device) {
           LOG_ERROR("Null device pointer");
           return -EINVAL;
       }

       if (!config) {
           LOG_ERROR("Null config pointer");
           return -EINVAL;
       }

       /* Validate ranges */
       if (config->speed < MIN_SPEED || config->speed > MAX_SPEED) {
           LOG_ERROR("Invalid speed: %d (range: %d-%d)",
                     config->speed, MIN_SPEED, MAX_SPEED);
           return -EINVAL;
       }

       /* Validate state */
       if (device->state != DEVICE_STATE_IDLE) {
           LOG_ERROR("Device not in idle state: %d", device->state);
           return -EBUSY;
       }

       /* Configure device */
       return 0;
   }

Assertions
~~~~~~~~~~

**Use Assertions for Programming Errors:**

.. code-block:: c

   #include "hal/nx_assert.h"

   void process_data(const uint8_t* data, size_t len)
   {
       /* Assert preconditions (programming errors) */
       NX_ASSERT(data != NULL);
       NX_ASSERT(len > 0);
       NX_ASSERT(len <= MAX_SIZE);

       /* Process data */
       for (size_t i = 0; i < len; i++) {
           NX_ASSERT(i < len);  /* Bounds check */
           process_byte(data[i]);
       }
   }

**Custom Assert Handler:**

.. code-block:: c

   void nx_assert_failed(const char* file, int line, const char* expr)
   {
       /* Log assertion failure */
       LOG_FATAL("Assertion failed: %s at %s:%d", expr, file, line);

       /* Dump system state */
       dump_system_state();

       /* Enter safe mode or reset */
       __disable_irq();
       while (1) {
           /* Halt */
       }
   }

Runtime Checks
~~~~~~~~~~~~~~

**Check Return Values:**

.. code-block:: c

   void send_data(const uint8_t* data, size_t len)
   {
       nx_uart_t* uart = nx_factory_uart(0);
       if (!uart) {
           LOG_ERROR("Failed to get UART device");
           return;
       }

       nx_tx_sync_t* tx = uart->get_tx_sync(uart);
       if (!tx) {
           LOG_ERROR("Failed to get TX interface");
           nx_factory_uart_release(uart);
           return;
       }

       nx_status_t status = tx->send(tx, data, len, 1000);
       if (status != NX_OK) {
           LOG_ERROR("Failed to send data: %d", status);
           /* Handle error */
       }

       nx_factory_uart_release(uart);
   }

Error Reporting
---------------

Logging Errors
~~~~~~~~~~~~~~

**Use Appropriate Log Levels:**

.. code-block:: c

   #define LOG_MODULE "sensor"
   #include "log/log.h"

   int read_sensor(float* value)
   {
       /* Recoverable error - WARN */
       if (sensor_not_ready()) {
           LOG_WARN("Sensor not ready, retrying...");
           osal_task_delay(100);
       }

       /* Failure - ERROR */
       int result = sensor_read_raw(value);
       if (result != 0) {
           LOG_ERROR("Sensor read failed: %d", result);
           return result;
       }

       /* Invalid data - ERROR */
       if (*value < MIN_VALUE || *value > MAX_VALUE) {
           LOG_ERROR("Invalid sensor value: %.2f (range: %.2f-%.2f)",
                     *value, MIN_VALUE, MAX_VALUE);
           return -EINVAL;
       }

       /* Critical failure - FATAL */
       if (sensor_hardware_fault()) {
           LOG_FATAL("Sensor hardware fault detected");
           return -EIO;
       }

       return 0;
   }

Error Context
~~~~~~~~~~~~~

**Include Relevant Information:**

.. code-block:: c

   int process_packet(const packet_t* packet)
   {
       if (!packet) {
           LOG_ERROR("Null packet pointer");
           return -EINVAL;
       }

       /* Include context in error messages */
       if (packet->length > MAX_PACKET_SIZE) {
           LOG_ERROR("Packet too large: type=0x%02X, len=%d, max=%d, seq=%lu",
                     packet->type, packet->length, MAX_PACKET_SIZE,
                     packet->sequence);
           return -EINVAL;
       }

       if (!validate_checksum(packet)) {
           LOG_ERROR("Checksum mismatch: type=0x%02X, len=%d, seq=%lu, "
                     "expected=0x%04X, actual=0x%04X",
                     packet->type, packet->length, packet->sequence,
                     packet->checksum, calculate_checksum(packet));
           return -EINVAL;
       }

       return 0;
   }

Error Callbacks
~~~~~~~~~~~~~~~

**Register Error Handlers:**

.. code-block:: c

   typedef void (*error_callback_t)(int error_code, const char* message);

   static error_callback_t error_callback = NULL;

   void register_error_callback(error_callback_t callback)
   {
       error_callback = callback;
   }

   void report_error(int error_code, const char* format, ...)
   {
       char message[256];
       va_list args;

       /* Format message */
       va_start(args, format);
       vsnprintf(message, sizeof(message), format, args);
       va_end(args);

       /* Log error */
       LOG_ERROR("%s", message);

       /* Call callback if registered */
       if (error_callback) {
           error_callback(error_code, message);
       }
   }

Error Recovery
--------------

Retry Strategies
~~~~~~~~~~~~~~~~

**Implement Retry Logic:**

.. code-block:: c

   int send_with_retry(const uint8_t* data, size_t len)
   {
       const int MAX_RETRIES = 3;
       const uint32_t RETRY_DELAY_MS = 100;

       for (int retry = 0; retry < MAX_RETRIES; retry++) {
           int result = send_data(data, len);

           if (result == 0) {
               /* Success */
               if (retry > 0) {
                   LOG_INFO("Send succeeded after %d retries", retry);
               }
               return 0;
           }

           /* Log retry */
           LOG_WARN("Send failed (attempt %d/%d): %d",
                    retry + 1, MAX_RETRIES, result);

           /* Delay before retry */
           if (retry < MAX_RETRIES - 1) {
               osal_task_delay(RETRY_DELAY_MS);
           }
       }

       LOG_ERROR("Send failed after %d retries", MAX_RETRIES);
       return -EIO;
   }

**Exponential Backoff:**

.. code-block:: c

   int connect_with_backoff(void)
   {
       const int MAX_RETRIES = 5;
       uint32_t delay_ms = 100;  /* Start with 100ms */

       for (int retry = 0; retry < MAX_RETRIES; retry++) {
           int result = connect();

           if (result == 0) {
               return 0;
           }

           LOG_WARN("Connection failed (attempt %d/%d), retrying in %lu ms",
                    retry + 1, MAX_RETRIES, delay_ms);

           osal_task_delay(delay_ms);

           /* Exponential backoff: double delay each time */
           delay_ms *= 2;
           if (delay_ms > 5000) {
               delay_ms = 5000;  /* Cap at 5 seconds */
           }
       }

       return -ETIMEDOUT;
   }

Fallback Mechanisms
~~~~~~~~~~~~~~~~~~~

**Provide Alternatives:**

.. code-block:: c

   float read_temperature(void)
   {
       float temp;

       /* Try primary sensor */
       if (read_primary_sensor(&temp) == 0) {
           return temp;
       }

       LOG_WARN("Primary sensor failed, trying backup");

       /* Try backup sensor */
       if (read_backup_sensor(&temp) == 0) {
           return temp;
       }

       LOG_ERROR("Both sensors failed, using default value");

       /* Return default value */
       return DEFAULT_TEMPERATURE;
   }

Graceful Degradation
~~~~~~~~~~~~~~~~~~~~

**Reduce Functionality:**

.. code-block:: c

   typedef enum {
       MODE_FULL = 0,
       MODE_REDUCED,
       MODE_MINIMAL,
       MODE_SAFE,
   } operation_mode_t;

   static operation_mode_t current_mode = MODE_FULL;

   void handle_error(int error_severity)
   {
       switch (error_severity) {
       case ERROR_MINOR:
           /* Continue normal operation */
           break;

       case ERROR_MODERATE:
           if (current_mode == MODE_FULL) {
               LOG_WARN("Entering reduced mode");
               current_mode = MODE_REDUCED;
               disable_non_essential_features();
           }
           break;

       case ERROR_SEVERE:
           if (current_mode != MODE_MINIMAL) {
               LOG_ERROR("Entering minimal mode");
               current_mode = MODE_MINIMAL;
               disable_all_optional_features();
           }
           break;

       case ERROR_CRITICAL:
           LOG_FATAL("Entering safe mode");
           current_mode = MODE_SAFE;
           enter_safe_mode();
           break;
       }
   }

Safe Mode
~~~~~~~~~

**Implement Safe Mode:**

.. code-block:: c

   void enter_safe_mode(void)
   {
       LOG_FATAL("Entering safe mode");

       /* Disable all non-essential peripherals */
       disable_all_peripherals();

       /* Stop all non-critical tasks */
       stop_non_critical_tasks();

       /* Enable only essential functions */
       enable_essential_functions();

       /* Indicate safe mode (LED pattern, etc.) */
       indicate_safe_mode();

       /* Wait for manual intervention or watchdog reset */
       while (1) {
           /* Minimal operation */
           osal_task_delay(1000);
       }
   }

Error Prevention
----------------

Defensive Programming
~~~~~~~~~~~~~~~~~~~~~

**Check Assumptions:**

.. code-block:: c

   void process_queue(queue_t* queue)
   {
       /* Defensive checks */
       if (!queue) {
           LOG_ERROR("Null queue pointer");
           return;
       }

       if (!queue->initialized) {
           LOG_ERROR("Queue not initialized");
           return;
       }

       while (!queue_is_empty(queue)) {
           item_t* item = queue_dequeue(queue);

           /* Check for unexpected null */
           if (!item) {
               LOG_ERROR("Unexpected null item from queue");
               continue;
           }

           process_item(item);
       }
   }

Input Sanitization
~~~~~~~~~~~~~~~~~~

**Validate and Sanitize:**

.. code-block:: c

   int set_device_name(device_t* device, const char* name)
   {
       if (!device || !name) {
           return -EINVAL;
       }

       /* Check length */
       size_t len = strlen(name);
       if (len == 0 || len >= MAX_NAME_LENGTH) {
           LOG_ERROR("Invalid name length: %zu", len);
           return -EINVAL;
       }

       /* Sanitize input - remove invalid characters */
       char sanitized[MAX_NAME_LENGTH];
       size_t j = 0;
       for (size_t i = 0; i < len && j < MAX_NAME_LENGTH - 1; i++) {
           if (isalnum(name[i]) || name[i] == '_' || name[i] == '-') {
               sanitized[j++] = name[i];
           }
       }
       sanitized[j] = '\0';

       /* Copy sanitized name */
       strncpy(device->name, sanitized, MAX_NAME_LENGTH - 1);
       device->name[MAX_NAME_LENGTH - 1] = '\0';

       return 0;
   }

Resource Limits
~~~~~~~~~~~~~~~

**Enforce Limits:**

.. code-block:: c

   #define MAX_CONNECTIONS 10

   static connection_t connections[MAX_CONNECTIONS];
   static size_t connection_count = 0;

   int create_connection(void)
   {
       /* Check limit */
       if (connection_count >= MAX_CONNECTIONS) {
           LOG_ERROR("Connection limit reached: %zu/%d",
                     connection_count, MAX_CONNECTIONS);
           return -ENOMEM;
       }

       /* Create connection */
       connection_t* conn = &connections[connection_count++];
       initialize_connection(conn);

       return 0;
   }

Best Practices
--------------

1. **Return Status Codes**
   * Always return error status
   * Use consistent error codes
   * Document error conditions
   * Check all return values

2. **Log Errors Appropriately**
   * Use correct log levels
   * Include context
   * Log at error source
   * Don't spam logs

3. **Handle Errors Gracefully**
   * Implement retry logic
   * Provide fallbacks
   * Degrade gracefully
   * Maintain system integrity

4. **Validate Inputs**
   * Check all parameters
   * Validate ranges
   * Sanitize inputs
   * Use assertions

5. **Prevent Errors**
   * Defensive programming
   * Resource limits
   * Input validation
   * Code reviews

6. **Test Error Paths**
   * Test failure scenarios
   * Test recovery mechanisms
   * Test edge cases
   * Use fault injection

7. **Document Errors**
   * Document error codes
   * Document recovery procedures
   * Document limitations
   * Provide examples

See Also
--------

* :doc:`debugging` - Debugging Guide
* :doc:`testing` - Testing Guide
* :doc:`best_practices` - Best Practices
* :doc:`../development/troubleshooting` - Troubleshooting Guide

