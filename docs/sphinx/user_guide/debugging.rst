Debugging Guide
===============

This comprehensive guide covers debugging techniques, tools, and best practices for Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Debugging embedded systems presents unique challenges compared to desktop applications. This guide provides strategies and tools for effective debugging on Nexus.

**Key Topics:**

* Debug tools and setup
* Printf-style debugging
* Hardware debuggers (GDB, OpenOCD)
* Log framework for debugging
* Common debugging scenarios
* Platform-specific debugging
* Performance profiling
* Memory debugging

Debug Build Configuration
--------------------------

Enabling Debug Symbols
~~~~~~~~~~~~~~~~~~~~~~

**CMake Configuration:**

.. code-block:: bash

   # Debug build with symbols
   cmake -B build -DCMAKE_BUILD_TYPE=Debug

   # Release with debug info
   cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

**Compiler Flags:**

Debug builds automatically include:

* ``-g3`` - Maximum debug information
* ``-Og`` - Optimize for debugging
* ``DEBUG`` - Debug macro defined
* No optimization that hinders debugging

**Kconfig Options:**

.. code-block:: kconfig

   # Enable debug features
   CONFIG_DEBUG=y
   CONFIG_DEBUG_ASSERTIONS=y
   CONFIG_DEBUG_VERBOSE=y

   # Enable framework debug output
   CONFIG_FRAMEWORK_LOG=y
   CONFIG_LOG_LEVEL_DEBUG=y

Optimization Levels
~~~~~~~~~~~~~~~~~~~

Different optimization levels affect debuggability:

+-------------------+-------------+------------------+
| Build Type        | Optimization| Debuggability    |
+===================+=============+==================+
| Debug             | ``-Og``     | Excellent        |
+-------------------+-------------+------------------+
| RelWithDebInfo    | ``-O2 -g``  | Good             |
+-------------------+-------------+------------------+
| Release           | ``-O2``     | Limited          |
+-------------------+-------------+------------------+
| MinSizeRel        | ``-Os``     | Limited          |
+-------------------+-------------+------------------+

Printf-Style Debugging
----------------------

Using Log Framework
~~~~~~~~~~~~~~~~~~~

The Log framework is the primary debugging tool:

.. code-block:: c

   #define LOG_MODULE "mymodule"
   #include "log/log.h"

   void my_function(int value)
   {
       LOG_DEBUG("Entering my_function with value=%d", value);

       if (value < 0) {
           LOG_WARN("Negative value detected: %d", value);
       }

       /* Function logic */

       LOG_DEBUG("Exiting my_function");
   }

**Advantages:**

* Minimal performance impact
* Works on all platforms
* Supports multiple backends
* Module-level filtering
* Timestamp information

**Best Practices:**

1. Use appropriate log levels
2. Include context in messages
3. Log function entry/exit for complex flows
4. Log error conditions with details
5. Use module names for filtering

Debug Macros
~~~~~~~~~~~~

**Conditional Debug Code:**

.. code-block:: c

   #ifdef DEBUG
   static void dump_buffer(const uint8_t* buf, size_t len)
   {
       for (size_t i = 0; i < len; i++) {
           LOG_DEBUG("buf[%zu] = 0x%02X", i, buf[i]);
       }
   }
   #endif

   void process_data(const uint8_t* data, size_t len)
   {
   #ifdef DEBUG
       dump_buffer(data, len);
   #endif
       /* Process data */
   }

**Assertions:**

.. code-block:: c

   #include "hal/nx_assert.h"

   void set_speed(uint32_t speed)
   {
       /* Assert valid range */
       NX_ASSERT(speed >= MIN_SPEED && speed <= MAX_SPEED);

       /* Set speed */
   }

UART Debug Output
~~~~~~~~~~~~~~~~~

**Setup UART for Debug:**

.. code-block:: c

   /* Initialize UART for debug output */
   nx_uart_config_t uart_cfg = {
       .baudrate = 115200,
       .word_length = 8,
       .stop_bits = 1,
       .parity = 0,
   };

   nx_uart_t* debug_uart = nx_factory_uart_with_config(0, &uart_cfg);

   /* Register UART backend for logging */
   log_backend_t* uart_backend = log_backend_uart_create(debug_uart);
   log_backend_register(uart_backend);

**Connect Serial Terminal:**

.. code-block:: bash

   # Linux/macOS
   screen /dev/ttyUSB0 115200

   # Windows (PowerShell)
   # Use PuTTY, TeraTerm, or similar

Hardware Debugger Setup
-----------------------

GDB + OpenOCD (STM32)
~~~~~~~~~~~~~~~~~~~~~

**Install Tools:**

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install gdb-multiarch openocd

   # macOS
   brew install gdb openocd

   # Windows
   # Download from ARM website and OpenOCD releases

**OpenOCD Configuration:**

Create ``openocd.cfg``:

.. code-block:: tcl

   # STM32F4 Discovery
   source [find interface/stlink.cfg]
   source [find target/stm32f4x.cfg]

   # Optional: Enable semihosting
   $_TARGETNAME configure -event gdb-attach {
       echo "Debugger attached"
       reset init
   }

**Start OpenOCD:**

.. code-block:: bash

   # Terminal 1: Start OpenOCD
   openocd -f openocd.cfg

   # Output should show:
   # Info : Listening on port 3333 for gdb connections

**Connect GDB:**

.. code-block:: bash

   # Terminal 2: Start GDB
   arm-none-eabi-gdb build/applications/blinky/blinky.elf

   # In GDB:
   (gdb) target extended-remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) monitor reset init
   (gdb) continue

GDB Commands
~~~~~~~~~~~~

**Basic Commands:**

.. code-block:: text

   # Load program
   (gdb) load

   # Set breakpoint
   (gdb) break main
   (gdb) break hal_gpio.c:123

   # Run program
   (gdb) continue
   (gdb) run

   # Step execution
   (gdb) step          # Step into functions
   (gdb) next          # Step over functions
   (gdb) finish        # Step out of function

   # Examine variables
   (gdb) print variable_name
   (gdb) print *pointer
   (gdb) print array[5]

   # Examine memory
   (gdb) x/10x 0x20000000    # 10 hex words
   (gdb) x/10i $pc           # 10 instructions at PC

   # Backtrace
   (gdb) backtrace
   (gdb) bt full

   # Watchpoints
   (gdb) watch variable_name
   (gdb) rwatch variable_name  # Read watchpoint
   (gdb) awatch variable_name  # Access watchpoint

**Advanced Commands:**

.. code-block:: text

   # Conditional breakpoint
   (gdb) break main if argc > 1

   # Breakpoint commands
   (gdb) break hal_gpio_write
   (gdb) commands
   > print port
   > print pin
   > print value
   > continue
   > end

   # Display expressions
   (gdb) display variable_name
   (gdb) display/x register

   # Examine registers
   (gdb) info registers
   (gdb) info all-registers

   # Examine threads (RTOS)
   (gdb) info threads
   (gdb) thread 2

GDB Init Script
~~~~~~~~~~~~~~~

Create ``.gdbinit`` in project root:

.. code-block:: text

   # Connect to OpenOCD
   target extended-remote localhost:3333

   # Load symbols
   file build/applications/blinky/blinky.elf

   # Reset and halt
   monitor reset halt

   # Load program
   load

   # Set breakpoint at main
   break main

   # Start execution
   continue

   # Enable pretty printing
   set print pretty on
   set print array on

Then simply run:

.. code-block:: bash

   arm-none-eabi-gdb

J-Link Debugger
~~~~~~~~~~~~~~~

**J-Link GDB Server:**

.. code-block:: bash

   # Start J-Link GDB Server
   JLinkGDBServer -device STM32F407VG -if SWD -speed 4000

   # Connect GDB
   arm-none-eabi-gdb build/app.elf
   (gdb) target remote localhost:2331

**J-Link RTT (Real-Time Transfer):**

.. code-block:: c

   #include "SEGGER_RTT.h"

   void debug_output(void)
   {
       SEGGER_RTT_printf(0, "Debug message: %d\n", value);
   }

IDE Integration
---------------

VS Code
~~~~~~~

**Install Extensions:**

* C/C++ (Microsoft)
* Cortex-Debug
* CMake Tools

**launch.json Configuration:**

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug STM32F4",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "openocd",
               "cwd": "${workspaceRoot}",
               "executable": "${workspaceRoot}/build/applications/blinky/blinky.elf",
               "device": "STM32F407VG",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32f4x.cfg"
               ],
               "svdFile": "${workspaceRoot}/vendors/st/STM32F407.svd",
               "runToMain": true,
               "preLaunchTask": "build"
           }
       ]
   }

**tasks.json for Build:**

.. code-block:: json

   {
       "version": "2.0.0",
       "tasks": [
           {
               "label": "build",
               "type": "shell",
               "command": "cmake",
               "args": [
                   "--build",
                   "build",
                   "--config",
                   "Debug"
               ],
               "group": {
                   "kind": "build",
                   "isDefault": true
               }
           }
       ]
   }

Eclipse
~~~~~~~

**Setup:**

1. Install Eclipse IDE for Embedded C/C++ Developers
2. Install GNU MCU Eclipse plugins
3. Configure ARM toolchain path
4. Import CMake project

**Debug Configuration:**

1. Run → Debug Configurations
2. GDB Hardware Debugging → New
3. Configure:
   * C/C++ Application: path to .elf
   * GDB Command: arm-none-eabi-gdb
   * Remote Target: localhost:3333
   * Startup: load image, set breakpoint at main

CLion
~~~~~

**CMake Configuration:**

CLion automatically detects CMakeLists.txt.

**Embedded Development Plugin:**

Install "Embedded Development Support" plugin.

**Debug Configuration:**

1. Run → Edit Configurations
2. Add → Embedded GDB Server
3. Configure target and GDB server settings

Common Debugging Scenarios
---------------------------

Hard Fault Debugging
~~~~~~~~~~~~~~~~~~~~

**Symptoms:**

* Program crashes
* Enters HardFault_Handler
* System becomes unresponsive

**Debugging Steps:**

1. **Enable Fault Handlers:**

.. code-block:: c

   void HardFault_Handler(void)
   {
       /* Capture fault information */
       volatile uint32_t* cfsr = (uint32_t*)0xE000ED28;
       volatile uint32_t* hfsr = (uint32_t*)0xE000ED2C;
       volatile uint32_t* mmfar = (uint32_t*)0xE000ED34;
       volatile uint32_t* bfar = (uint32_t*)0xE000ED38;

       LOG_FATAL("HardFault!");
       LOG_FATAL("CFSR: 0x%08lX", *cfsr);
       LOG_FATAL("HFSR: 0x%08lX", *hfsr);
       LOG_FATAL("MMFAR: 0x%08lX", *mmfar);
       LOG_FATAL("BFAR: 0x%08lX", *bfar);

       /* Infinite loop for debugging */
       while (1) {
           __asm("NOP");
       }
   }

2. **Examine Stack:**

.. code-block:: text

   (gdb) backtrace
   (gdb) info registers
   (gdb) x/32x $sp

3. **Common Causes:**

* Null pointer dereference
* Stack overflow
* Invalid memory access
* Unaligned access
* Division by zero

Memory Corruption
~~~~~~~~~~~~~~~~~

**Symptoms:**

* Variables change unexpectedly
* Random crashes
* Data corruption

**Debugging Techniques:**

1. **Watchpoints:**

.. code-block:: text

   (gdb) watch my_variable
   (gdb) continue

2. **Memory Protection:**

.. code-block:: c

   /* Add guard bytes */
   #define GUARD_BYTE 0xDEADBEEF

   typedef struct {
       uint32_t guard_start;
       uint8_t data[256];
       uint32_t guard_end;
   } protected_buffer_t;

   void check_guards(protected_buffer_t* buf)
   {
       NX_ASSERT(buf->guard_start == GUARD_BYTE);
       NX_ASSERT(buf->guard_end == GUARD_BYTE);
   }

3. **Stack Canaries:**

Enable stack protection:

.. code-block:: cmake

   add_compile_options(-fstack-protector-strong)

Task Debugging (RTOS)
~~~~~~~~~~~~~~~~~~~~~

**FreeRTOS Thread Awareness:**

GDB with FreeRTOS plugin:

.. code-block:: text

   # List tasks
   (gdb) info threads

   # Switch to task
   (gdb) thread 3

   # Task backtrace
   (gdb) bt

**Task State Inspection:**

.. code-block:: c

   void debug_task_info(void)
   {
       osal_task_handle_t current = osal_task_get_current();

       LOG_DEBUG("Current task: %s", osal_task_get_name(current));
       LOG_DEBUG("Priority: %d", osal_task_get_priority(current));
       LOG_DEBUG("Stack high water: %lu",
                 osal_task_get_stack_high_water(current));
   }

**Deadlock Detection:**

.. code-block:: c

   /* Timeout on mutex locks */
   if (osal_mutex_lock(mutex, 5000) != OSAL_OK) {
       LOG_ERROR("Mutex lock timeout - possible deadlock");
       /* Dump task states */
       debug_all_tasks();
   }

Peripheral Debugging
~~~~~~~~~~~~~~~~~~~~

**GPIO:**

.. code-block:: c

   void debug_gpio_state(char port, uint8_t pin)
   {
       nx_gpio_t* gpio = nx_factory_gpio(port, pin);
       if (gpio) {
           uint8_t state = gpio->read(gpio);
           LOG_DEBUG("GPIO %c%d = %d", port, pin, state);
           nx_factory_gpio_release(gpio);
       }
   }

**UART:**

.. code-block:: c

   void debug_uart_status(nx_uart_t* uart)
   {
       nx_status_t status = uart->get_status(uart);
       LOG_DEBUG("UART status: 0x%08X", status);

       /* Check for errors */
       if (status & UART_ERROR_OVERRUN) {
           LOG_WARN("UART overrun error");
       }
       if (status & UART_ERROR_FRAMING) {
           LOG_WARN("UART framing error");
       }
   }

**SPI:**

.. code-block:: c

   void debug_spi_transfer(nx_spi_t* spi,
                           const uint8_t* tx_data,
                           uint8_t* rx_data,
                           size_t len)
   {
       LOG_DEBUG("SPI transfer: %zu bytes", len);

   #ifdef DEBUG
       LOG_DEBUG("TX:");
       for (size_t i = 0; i < len; i++) {
           LOG_DEBUG("  [%zu] = 0x%02X", i, tx_data[i]);
       }
   #endif

       nx_status_t status = spi->transfer(spi, tx_data, rx_data, len, 1000);

   #ifdef DEBUG
       if (status == NX_OK) {
           LOG_DEBUG("RX:");
           for (size_t i = 0; i < len; i++) {
               LOG_DEBUG("  [%zu] = 0x%02X", i, rx_data[i]);
           }
       }
   #endif
   }

Performance Profiling
---------------------

Timing Measurements
~~~~~~~~~~~~~~~~~~~

**Simple Timing:**

.. code-block:: c

   uint32_t start = osal_get_time_ms();

   /* Code to measure */
   perform_operation();

   uint32_t elapsed = osal_get_time_ms() - start;
   LOG_INFO("Operation took %lu ms", elapsed);

**High-Resolution Timing:**

.. code-block:: c

   /* Use hardware timer for microsecond precision */
   uint32_t start = hal_timer_get_counter(TIMER_0);

   /* Code to measure */
   perform_operation();

   uint32_t end = hal_timer_get_counter(TIMER_0);
   uint32_t cycles = end - start;
   uint32_t us = cycles / (SystemCoreClock / 1000000);

   LOG_INFO("Operation took %lu us (%lu cycles)", us, cycles);

**Function Profiling:**

.. code-block:: c

   #ifdef PROFILE
   #define PROFILE_START(name) \
       uint32_t __profile_##name##_start = osal_get_time_ms()

   #define PROFILE_END(name) \
       do { \
           uint32_t __elapsed = osal_get_time_ms() - __profile_##name##_start; \
           LOG_INFO("PROFILE: %s took %lu ms", #name, __elapsed); \
       } while (0)
   #else
   #define PROFILE_START(name)
   #define PROFILE_END(name)
   #endif

   void my_function(void)
   {
       PROFILE_START(my_function);

       /* Function code */

       PROFILE_END(my_function);
   }

CPU Usage Monitoring
~~~~~~~~~~~~~~~~~~~~

**FreeRTOS Runtime Stats:**

.. code-block:: c

   #if (configGENERATE_RUN_TIME_STATS == 1)
   void print_task_stats(void)
   {
       char stats_buffer[512];
       vTaskGetRunTimeStats(stats_buffer);
       LOG_INFO("Task Statistics:\n%s", stats_buffer);
   }
   #endif

**Idle Task Hook:**

.. code-block:: c

   void vApplicationIdleHook(void)
   {
       static uint32_t idle_count = 0;
       idle_count++;

       /* Log CPU usage periodically */
       if (idle_count % 10000 == 0) {
           uint32_t cpu_usage = calculate_cpu_usage();
           LOG_DEBUG("CPU usage: %lu%%", cpu_usage);
       }
   }

Memory Debugging
----------------

Stack Usage
~~~~~~~~~~~

**Check Stack High Water Mark:**

.. code-block:: c

   void check_stack_usage(void)
   {
       osal_task_handle_t current = osal_task_get_current();
       uint32_t high_water = osal_task_get_stack_high_water(current);
       uint32_t stack_size = osal_task_get_stack_size(current);
       uint32_t used = stack_size - high_water;
       uint32_t percent = (used * 100) / stack_size;

       LOG_INFO("Stack usage: %lu/%lu bytes (%lu%%)",
                used, stack_size, percent);

       if (percent > 80) {
           LOG_WARN("Stack usage high!");
       }
   }

**Stack Overflow Detection:**

.. code-block:: c

   /* FreeRTOS stack overflow hook */
   void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
   {
       LOG_FATAL("Stack overflow in task: %s", pcTaskName);
       /* Handle error */
       while (1);
   }

Heap Usage
~~~~~~~~~~

**Monitor Heap:**

.. code-block:: c

   void check_heap_usage(void)
   {
       size_t free_heap = osal_get_free_heap_size();
       size_t min_free = osal_get_minimum_ever_free_heap_size();

       LOG_INFO("Free heap: %zu bytes", free_heap);
       LOG_INFO("Minimum free heap: %zu bytes", min_free);

       if (free_heap < 1024) {
           LOG_WARN("Low memory!");
       }
   }

**Memory Leak Detection:**

.. code-block:: c

   #ifdef DEBUG
   static size_t alloc_count = 0;
   static size_t free_count = 0;

   void* debug_malloc(size_t size)
   {
       void* ptr = malloc(size);
       if (ptr) {
           alloc_count++;
           LOG_DEBUG("malloc(%zu) = %p [count=%zu]",
                     size, ptr, alloc_count);
       }
       return ptr;
   }

   void debug_free(void* ptr)
   {
       if (ptr) {
           free_count++;
           LOG_DEBUG("free(%p) [count=%zu]", ptr, free_count);
           free(ptr);
       }
   }

   void check_memory_leaks(void)
   {
       if (alloc_count != free_count) {
           LOG_WARN("Memory leak detected: %zu allocs, %zu frees",
                    alloc_count, free_count);
       }
   }
   #endif

Platform-Specific Debugging
----------------------------

STM32 Debugging
~~~~~~~~~~~~~~~

**SWO (Serial Wire Output):**

.. code-block:: c

   /* Enable SWO output */
   void swo_init(void)
   {
       /* Configure SWO pin */
       /* Enable ITM */
       ITM->LAR = 0xC5ACCE55;
       ITM->TER = 0x1;
       ITM->TCR = 0x00010005;
   }

   /* Printf to SWO */
   int _write(int file, char* ptr, int len)
   {
       for (int i = 0; i < len; i++) {
           ITM_SendChar(ptr[i]);
       }
       return len;
   }

**ETM Trace:**

Configure ETM for instruction trace in OpenOCD.

Native Platform Debugging
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Valgrind:**

.. code-block:: bash

   # Memory leak detection
   valgrind --leak-check=full ./build/app

   # Memory error detection
   valgrind --tool=memcheck ./build/app

**AddressSanitizer:**

.. code-block:: cmake

   # Enable ASan
   add_compile_options(-fsanitize=address)
   add_link_options(-fsanitize=address)

**GDB on Native:**

.. code-block:: bash

   gdb ./build/app
   (gdb) run
   (gdb) backtrace

Best Practices
--------------

1. **Use Appropriate Log Levels**
   * TRACE for detailed flow
   * DEBUG for development
   * INFO for normal operation
   * WARN for recoverable issues
   * ERROR for failures
   * FATAL for critical errors

2. **Add Context to Logs**
   * Include relevant variable values
   * Log function parameters
   * Log return values
   * Include error codes

3. **Use Assertions**
   * Check preconditions
   * Validate parameters
   * Verify invariants
   * Catch programming errors early

4. **Minimize Debug Impact**
   * Use conditional compilation
   * Avoid blocking operations in debug code
   * Be mindful of timing changes
   * Use async logging when possible

5. **Document Debug Features**
   * Document debug commands
   * Explain debug output format
   * Provide troubleshooting guides
   * Include debug build instructions

6. **Version Control Debug Code**
   * Keep debug code in version control
   * Use feature flags for debug features
   * Don't commit temporary debug code
   * Review debug code in code reviews

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**GDB Connection Fails**

* Check OpenOCD is running
* Verify correct port (3333)
* Check firewall settings
* Try different debug adapter

**Breakpoints Don't Work**

* Verify debug symbols (-g flag)
* Check optimization level
* Ensure code is not in ROM
* Try hardware breakpoints

**Variables Optimized Out**

* Use ``-Og`` optimization
* Mark variables as ``volatile``
* Use ``-fno-omit-frame-pointer``
* Disable specific optimizations

**Slow Debugging**

* Reduce log verbosity
* Use hardware breakpoints
* Increase GDB timeout
* Check USB connection quality

See Also
--------

* :doc:`testing` - Testing and Validation
* :doc:`profiling` - Performance Profiling
* :doc:`../development/debugging_guide` - Development Debugging Guide
* :doc:`../platform_guides/index` - Platform-Specific Guides

