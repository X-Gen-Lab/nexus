Init Framework API Reference
============================

This section documents the Initialization Framework API.

Overview
--------

The Nexus Init Framework provides a compile-time automatic initialization system
similar to Linux initcall. Initialization functions are registered at compile time
using linker sections and executed in level order at system startup.

Initialization Levels
---------------------

The framework defines six initialization levels that execute in order:

1. **BOARD** - Board-level initialization (clock, power)
2. **PREV** - Pre-initialization (memory, debug)
3. **BSP** - BSP initialization (peripheral configuration)
4. **DRIVER** - Driver initialization
5. **COMPONENT** - Component initialization (middleware)
6. **APP** - Application initialization

Core Functions
--------------

.. doxygenfunction:: nx_init_run
   :project: nexus

.. doxygenfunction:: nx_init_get_stats
   :project: nexus

.. doxygenfunction:: nx_init_is_complete
   :project: nexus

Type Definitions
----------------

.. doxygentypedef:: nx_init_fn_t
   :project: nexus

.. doxygenenum:: nx_init_level_t
   :project: nexus

.. doxygenstruct:: nx_init_stats_s
   :project: nexus
   :members:

Export Macros
-------------

Use these macros to register initialization functions at compile time:

.. doxygendefine:: NX_INIT_BOARD_EXPORT
   :project: nexus

.. doxygendefine:: NX_INIT_PREV_EXPORT
   :project: nexus

.. doxygendefine:: NX_INIT_BSP_EXPORT
   :project: nexus

.. doxygendefine:: NX_INIT_DRIVER_EXPORT
   :project: nexus

.. doxygendefine:: NX_INIT_COMPONENT_EXPORT
   :project: nexus

.. doxygendefine:: NX_INIT_APP_EXPORT
   :project: nexus

Usage Example
-------------

.. code-block:: c

    #include "nx_init.h"

    /* Board-level initialization function */
    static int board_clock_init(void) {
        /* Configure system clock */
        return 0;  /* Success */
    }

    /* Register the function to run at BOARD level */
    NX_INIT_BOARD_EXPORT(board_clock_init);

    /* Driver initialization function */
    static int uart_driver_init(void) {
        /* Initialize UART driver */
        return 0;  /* Success */
    }

    /* Register the function to run at DRIVER level */
    NX_INIT_DRIVER_EXPORT(uart_driver_init);

    /* In main() */
    int main(void) {
        nx_status_t status;

        /* Run all registered initialization functions */
        status = nx_init_run();
        if (status != NX_OK) {
            /* Handle initialization failure */
        }

        /* Check if all initializations succeeded */
        if (!nx_init_is_complete()) {
            /* Some initializations failed */
            nx_init_stats_t stats;
            nx_init_get_stats(&stats);
            /* Log or handle the failure */
        }

        /* Application code */
        while (1) {
            /* ... */
        }
    }

Thread Safety
-------------

The Init Framework is **not thread-safe**. All initialization functions should be
called during system startup before any threads are created. The ``nx_init_run()``
function should only be called once from the main thread.

Notes
-----

- Initialization functions execute in level order (BOARD → PREV → BSP → DRIVER → COMPONENT → APP)
- Within the same level, execution order is undefined
- If an initialization function fails (returns non-zero), execution continues with remaining functions
- Use ``nx_init_get_stats()`` to check for failures after ``nx_init_run()``
- Initialization functions should be lightweight and avoid blocking operations

Related APIs
------------

- :doc:`hal` - HAL initialization typically uses DRIVER level
- :doc:`osal` - OSAL initialization typically uses PREV level
- :doc:`log` - Log framework initialization
- :doc:`config` - Config manager initialization

See Also
--------

- :doc:`../user_guide/architecture` - System Architecture
- :doc:`../reference/error_codes` - Error Code Reference
- :doc:`hal` - HAL API Reference
