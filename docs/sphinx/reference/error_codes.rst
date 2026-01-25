Error Code Reference
====================

This page documents all error codes used throughout the Nexus platform.
All HAL and framework functions return ``nx_status_t`` to indicate success or failure.

Error Code Categories
---------------------

Error codes are organized into categories for easy identification:

- **0**: Success
- **1-19**: General errors
- **20-39**: State errors
- **40-59**: Resource errors
- **60-79**: Timeout errors
- **80-99**: IO errors
- **100-119**: DMA errors
- **120-139**: Data errors
- **140-159**: Permission errors

Success Code
------------

.. list-table::
   :header-rows: 1
   :widths: 20 10 70

   * - Code
     - Value
     - Description
   * - ``NX_OK``
     - 0
     - Operation completed successfully

General Errors (1-19)
---------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_GENERIC``
     - 1
     - Generic error (use more specific codes when possible)
   * - ``NX_ERR_INVALID_PARAM``
     - 2
     - Invalid parameter passed to function
   * - ``NX_ERR_NULL_PTR``
     - 3
     - Null pointer passed where valid pointer required
   * - ``NX_ERR_NOT_SUPPORTED``
     - 4
     - Operation not supported by this device/platform
   * - ``NX_ERR_NOT_FOUND``
     - 5
     - Requested item not found
   * - ``NX_ERR_INVALID_SIZE``
     - 6
     - Invalid size parameter

State Errors (20-39)
--------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_NOT_INIT``
     - 20
     - Device or module not initialized
   * - ``NX_ERR_ALREADY_INIT``
     - 21
     - Device or module already initialized
   * - ``NX_ERR_INVALID_STATE``
     - 22
     - Operation invalid in current state
   * - ``NX_ERR_BUSY``
     - 23
     - Device is busy with another operation
   * - ``NX_ERR_SUSPENDED``
     - 24
     - Device is in suspended state

Resource Errors (40-59)
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_NO_MEMORY``
     - 40
     - Out of memory
   * - ``NX_ERR_NO_RESOURCE``
     - 41
     - Required resource unavailable
   * - ``NX_ERR_RESOURCE_BUSY``
     - 42
     - Resource is busy
   * - ``NX_ERR_LOCKED``
     - 43
     - Resource is locked
   * - ``NX_ERR_FULL``
     - 44
     - Buffer or queue is full
   * - ``NX_ERR_EMPTY``
     - 45
     - Buffer or queue is empty

Timeout Errors (60-79)
----------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_TIMEOUT``
     - 60
     - Operation timed out
   * - ``NX_ERR_WOULD_BLOCK``
     - 61
     - Operation would block (non-blocking mode)

IO Errors (80-99)
-----------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_IO``
     - 80
     - Generic IO error
   * - ``NX_ERR_OVERRUN``
     - 81
     - Buffer overrun occurred
   * - ``NX_ERR_UNDERRUN``
     - 82
     - Buffer underrun occurred
   * - ``NX_ERR_PARITY``
     - 83
     - Parity error detected
   * - ``NX_ERR_FRAMING``
     - 84
     - Framing error detected
   * - ``NX_ERR_NOISE``
     - 85
     - Noise error detected
   * - ``NX_ERR_NACK``
     - 86
     - NACK received (I2C communication)
   * - ``NX_ERR_BUS``
     - 87
     - Bus error occurred
   * - ``NX_ERR_ARBITRATION``
     - 88
     - Arbitration lost (I2C/CAN)

DMA Errors (100-119)
--------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_DMA``
     - 100
     - Generic DMA error
   * - ``NX_ERR_DMA_TRANSFER``
     - 101
     - DMA transfer error
   * - ``NX_ERR_DMA_CONFIG``
     - 102
     - DMA configuration error

Data Errors (120-139)
---------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_NO_DATA``
     - 120
     - No data available
   * - ``NX_ERR_DATA_SIZE``
     - 121
     - Data size error
   * - ``NX_ERR_CRC``
     - 122
     - CRC check failed
   * - ``NX_ERR_CHECKSUM``
     - 123
     - Checksum verification failed

Permission Errors (140-159)
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Code
     - Value
     - Description
   * - ``NX_ERR_PERMISSION``
     - 140
     - Permission denied
   * - ``NX_ERR_READ_ONLY``
     - 141
     - Attempted write to read-only resource

Error Handling Utilities
------------------------

Checking Status
~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/nx_status.h"

    nx_status_t status = some_function();

    /* Check if operation succeeded */
    if (NX_IS_OK(status)) {
        /* Success */
    }

    /* Check if operation failed */
    if (NX_IS_ERROR(status)) {
        /* Handle error */
    }

Error Propagation
~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/nx_status.h"

    nx_status_t my_function(void) {
        nx_status_t status;

        /* Return immediately if error occurs */
        status = some_operation();
        NX_RETURN_IF_ERROR(status);

        /* Check for NULL pointer */
        void* ptr = get_pointer();
        NX_RETURN_IF_NULL(ptr);

        /* Continue with normal flow */
        return NX_OK;
    }

Error Cleanup
~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/nx_status.h"

    nx_status_t my_function(void) {
        nx_status_t result = NX_OK;
        void* resource = NULL;

        /* Allocate resource */
        resource = allocate();
        if (!resource) {
            result = NX_ERR_NO_MEMORY;
            goto cleanup;
        }

        /* Use resource */
        NX_GOTO_IF_ERROR(operation(resource), cleanup, result);

    cleanup:
        if (resource) {
            free(resource);
        }
        return result;
    }

Error Callbacks
~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/nx_status.h"

    /* Error callback function */
    void error_handler(void* user_data, nx_status_t status,
                      const char* module, const char* msg) {
        /* Log error */
        LOG_ERROR(module, "Error %d: %s", status, msg);
    }

    /* Register error callback */
    nx_set_error_callback(error_handler, NULL);

    /* Errors will now be reported through callback */

Converting to String
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/nx_status.h"

    nx_status_t status = some_function();
    if (NX_IS_ERROR(status)) {
        const char* error_str = nx_status_to_string(status);
        printf("Error: %s\n", error_str);
    }

Best Practices
--------------

1. **Always check return values**: Never ignore status codes from HAL functions
2. **Use specific error codes**: Prefer specific codes over ``NX_ERR_GENERIC``
3. **Propagate errors**: Use ``NX_RETURN_IF_ERROR`` to propagate errors up the call stack
4. **Clean up resources**: Use ``NX_GOTO_IF_ERROR`` for proper cleanup on error
5. **Log errors**: Use error callbacks or logging to track errors in production
6. **Document error conditions**: Document which errors each function can return

Common Error Scenarios
----------------------

Initialization Errors
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* Device not initialized */
    status = device_operation();
    if (status == NX_ERR_NOT_INIT) {
        /* Initialize device first */
        device_init();
    }

Timeout Handling
~~~~~~~~~~~~~~~~

.. code-block:: c

    /* Operation with timeout */
    status = device_read(buffer, size, 1000);
    if (status == NX_ERR_TIMEOUT) {
        /* Retry or handle timeout */
    }

Resource Management
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* Check for resource availability */
    status = allocate_resource();
    if (status == NX_ERR_NO_MEMORY) {
        /* Free some memory and retry */
    }

Communication Errors
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* I2C communication */
    status = i2c_write(addr, data, len);
    if (status == NX_ERR_NACK) {
        /* Device not responding */
    } else if (status == NX_ERR_ARBITRATION) {
        /* Bus arbitration lost, retry */
    }

See Also
--------

- :doc:`../api/hal` - HAL API Reference
- :doc:`../user_guide/hal` - HAL User Guide
- :doc:`../development/coding_standards` - Coding Standards
