Debugging Guide
===============

This comprehensive guide covers debugging techniques, tools, and best practices for the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Debugging embedded systems requires specialized tools and techniques due to:

* **Limited Resources**: Constrained memory and CPU
* **Real-Time Constraints**: Timing-critical operations
* **Hardware Dependencies**: Direct hardware interaction
* **No Console**: Limited output capabilities
* **Concurrent Execution**: Multiple tasks and interrupts

This guide covers debugging strategies for both native (host) and embedded (target) platforms.

Debugging Tools
---------------

Native Platform Tools
~~~~~~~~~~~~~~~~~~~~~

**GDB (GNU Debugger)**

The primary debugger for native development:

.. code-block:: bash

   # Build with debug symbols
   python scripts/building/build.py --platform native --build-type debug

   # Run with GDB
   gdb build/native/debug/tests/hal_test

   # Common GDB commands
   (gdb) break hal_gpio_init     # Set breakpoint
   (gdb) run                      # Start program
   (gdb) next                     # Step over
   (gdb) step                     # Step into
   (gdb) continue                 # Continue execution
   (gdb) print variable           # Print variable
   (gdb) backtrace                # Show call stack
   (gdb) info locals              # Show local variables

**LLDB (LLVM Debugger)**

Alternative debugger with similar functionality:

.. code-block:: bash

   # Run with LLDB
   lldb build/native/debug/tests/hal_test

   # Common LLDB commands
   (lldb) breakpoint set --name hal_gpio_init
   (lldb) run
   (lldb) next
   (lldb) step
   (lldb) continue
   (lldb) print variable
   (lldb) bt
   (lldb) frame variable

**Valgrind**

Memory error detection:

.. code-block:: bash

   # Check for memory leaks
   valgrind --leak-check=full ./build/native/debug/tests/hal_test

   # Check for memory errors
   valgrind --tool=memcheck ./build/native/debug/tests/hal_test

   # Check for threading issues
   valgrind --tool=helgrind ./build/native/debug/tests/hal_test

**AddressSanitizer (ASan)**

Fast memory error detector:

.. code-block:: bash

   # Build with ASan
   python scripts/building/build.py --platform native --sanitizer address

   # Run tests (ASan automatically detects errors)
   ./build/native/debug/tests/hal_test

**ThreadSanitizer (TSan)**

Data race detector:

.. code-block:: bash

   # Build with TSan
   python scripts/building/build.py --platform native --sanitizer thread

   # Run tests
   ./build/native/debug/tests/hal_test



Embedded Platform Tools
~~~~~~~~~~~~~~~~~~~~~~~

**OpenOCD**

Open On-Chip Debugger for various targets:

.. code-block:: bash

   # Start OpenOCD for STM32F4
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

   # Connect GDB to OpenOCD
   arm-none-eabi-gdb build/stm32f4/debug/application.elf
   (gdb) target extended-remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) continue

**J-Link**

SEGGER J-Link debugger:

.. code-block:: bash

   # Start J-Link GDB server
   JLinkGDBServer -device STM32F407VG -if SWD -speed 4000

   # Connect GDB
   arm-none-eabi-gdb build/stm32f4/debug/application.elf
   (gdb) target remote localhost:2331
   (gdb) monitor reset
   (gdb) load
   (gdb) continue

**ST-Link**

STMicroelectronics debugger:

.. code-block:: bash

   # Start ST-Link GDB server
   st-util

   # Connect GDB
   arm-none-eabi-gdb build/stm32f4/debug/application.elf
   (gdb) target extended-remote localhost:4242
   (gdb) load
   (gdb) continue

**Ozone**

SEGGER Ozone debugger (GUI):

* Visual debugging interface
* Real-time variable watching
* Timeline view
* Instruction trace
* Performance analysis

IDE Integration
~~~~~~~~~~~~~~~

**VS Code**

Debug configuration (`.vscode/launch.json`):

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug Native",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/build/native/debug/tests/hal_test",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}",
               "environment": [],
               "externalConsole": false,
               "MIMode": "gdb"
           },
           {
               "name": "Debug STM32",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "openocd",
               "cwd": "${workspaceFolder}",
               "executable": "${workspaceFolder}/build/stm32f4/debug/application.elf",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32f4x.cfg"
               ]
           }
       ]
   }

**CLion**

* Built-in CMake support
* Integrated debugger
* Hardware debugging via OpenOCD
* Memory view
* Peripheral registers view

**Eclipse**

* GNU MCU Eclipse plugin
* OpenOCD integration
* J-Link integration
* Peripheral view
* SWV trace

Debugging Techniques
--------------------

Printf Debugging
~~~~~~~~~~~~~~~~

**Basic Logging**

.. code-block:: c

   #include "log.h"

   void hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                     const hal_gpio_config_t* config) {
       LOG_DEBUG("GPIO init: port=%d, pin=%d", port, pin);

       /* Implementation */

       LOG_DEBUG("GPIO init complete");
   }

**Conditional Logging**

.. code-block:: c

   #if defined(DEBUG_GPIO)
   #define GPIO_LOG(...) LOG_DEBUG(__VA_ARGS__)
   #else
   #define GPIO_LOG(...) do {} while(0)
   #endif

   void hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                      hal_gpio_level_t level) {
       GPIO_LOG("GPIO write: port=%d, pin=%d, level=%d", port, pin, level);
       /* Implementation */
   }

**Trace Points**

.. code-block:: c

   void complex_function(void) {
       LOG_TRACE("Enter: complex_function");

       if (condition1) {
           LOG_TRACE("Branch: condition1");
           /* Code */
       }

       if (condition2) {
           LOG_TRACE("Branch: condition2");
           /* Code */
       }

       LOG_TRACE("Exit: complex_function");
   }

Breakpoint Debugging
~~~~~~~~~~~~~~~~~~~~

**Conditional Breakpoints**

.. code-block:: bash

   # GDB: Break only when condition is true
   (gdb) break hal_gpio_write if port == 0 && pin == 5

   # GDB: Break after N hits
   (gdb) break hal_gpio_write
   (gdb) ignore 1 100  # Ignore first 100 hits

**Watchpoints**

.. code-block:: bash

   # GDB: Break when variable changes
   (gdb) watch gpio_state
   (gdb) continue

   # GDB: Break when memory location changes
   (gdb) watch *(int*)0x40020000

**Catchpoints**

.. code-block:: bash

   # GDB: Break on exception
   (gdb) catch throw

   # GDB: Break on system call
   (gdb) catch syscall

Assertion Debugging
~~~~~~~~~~~~~~~~~~~

**Runtime Assertions**

.. code-block:: c

   #include <assert.h>

   void hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                      hal_gpio_level_t level) {
       assert(port < HAL_GPIO_PORT_MAX);
       assert(pin < HAL_GPIO_PIN_MAX);

       /* Implementation */
   }

**Custom Assertions**

.. code-block:: c

   #define ASSERT(expr) \
       do { \
           if (!(expr)) { \
               LOG_ERROR("Assertion failed: %s, file %s, line %d", \
                        #expr, __FILE__, __LINE__); \
               while(1);  /* Halt */ \
           } \
       } while(0)

   void process_data(uint8_t* buffer, size_t length) {
       ASSERT(buffer != NULL);
       ASSERT(length > 0);
       ASSERT(length <= MAX_BUFFER_SIZE);

       /* Implementation */
   }

**Static Assertions**

.. code-block:: c

   /* Compile-time checks */
   _Static_assert(sizeof(hal_gpio_config_t) == 16,
                  "hal_gpio_config_t size mismatch");

   _Static_assert(HAL_GPIO_PORT_MAX <= 16,
                  "Too many GPIO ports");

Memory Debugging
~~~~~~~~~~~~~~~~

**Stack Overflow Detection**

.. code-block:: c

   /* Enable stack canaries */
   #define STACK_CANARY 0xDEADBEEF

   void task_function(void* param) {
       uint32_t stack_canary = STACK_CANARY;

       /* Task code */

       /* Check canary at end */
       if (stack_canary != STACK_CANARY) {
           LOG_ERROR("Stack overflow detected!");
       }
   }

**Heap Debugging**

.. code-block:: c

   /* Wrapper for malloc with tracking */
   void* debug_malloc(size_t size, const char* file, int line) {
       void* ptr = malloc(size);
       if (ptr) {
           LOG_DEBUG("malloc: %p, size=%zu, %s:%d", ptr, size, file, line);
           track_allocation(ptr, size, file, line);
       }
       return ptr;
   }

   #define malloc(size) debug_malloc(size, __FILE__, __LINE__)

**Buffer Overrun Detection**

.. code-block:: c

   /* Add guard bytes around buffers */
   #define GUARD_BYTE 0xAA

   typedef struct {
       uint8_t guard_before[4];
       uint8_t data[256];
       uint8_t guard_after[4];
   } guarded_buffer_t;

   void init_guarded_buffer(guarded_buffer_t* buf) {
       memset(buf->guard_before, GUARD_BYTE, sizeof(buf->guard_before));
       memset(buf->guard_after, GUARD_BYTE, sizeof(buf->guard_after));
   }

   bool check_guarded_buffer(guarded_buffer_t* buf) {
       for (size_t i = 0; i < sizeof(buf->guard_before); i++) {
           if (buf->guard_before[i] != GUARD_BYTE) {
               LOG_ERROR("Buffer underrun detected!");
               return false;
           }
       }
       for (size_t i = 0; i < sizeof(buf->guard_after); i++) {
           if (buf->guard_after[i] != GUARD_BYTE) {
               LOG_ERROR("Buffer overrun detected!");
               return false;
           }
       }
       return true;
   }



Hardware Debugging
------------------

SWD/JTAG Debugging
~~~~~~~~~~~~~~~~~~

**SWD (Serial Wire Debug)**

* 2-wire interface (SWDIO, SWCLK)
* Lower pin count than JTAG
* Supported by most ARM Cortex-M devices

**JTAG (Joint Test Action Group)**

* 4-wire interface (TDI, TDO, TMS, TCK)
* Standard debugging interface
* Supports boundary scan

**Connection Setup**

.. code-block:: text

   Debugger          Target
   --------          ------
   SWDIO    <--->    SWDIO
   SWCLK    <--->    SWCLK
   GND      <--->    GND
   VCC      <--->    VCC (optional)

SWV Trace
~~~~~~~~~

**Serial Wire Viewer**

Real-time trace output without halting CPU:

.. code-block:: c

   /* Enable SWV output */
   void swv_init(void) {
       /* Enable TPIU and ITM */
       CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
       ITM->LAR = 0xC5ACCE55;
       ITM->TCR = ITM_TCR_ITMENA_Msk;
       ITM->TER = 0xFFFFFFFF;
   }

   /* Send character via SWV */
   void swv_putchar(char c) {
       while (ITM->PORT[0].u32 == 0);
       ITM->PORT[0].u8 = c;
   }

   /* Printf via SWV */
   int _write(int file, char *ptr, int len) {
       for (int i = 0; i < len; i++) {
           swv_putchar(ptr[i]);
       }
       return len;
   }

RTT (Real-Time Transfer)
~~~~~~~~~~~~~~~~~~~~~~~~

**SEGGER RTT**

Bidirectional communication without halting:

.. code-block:: c

   #include "SEGGER_RTT.h"

   /* Initialize RTT */
   SEGGER_RTT_Init();

   /* Print via RTT */
   SEGGER_RTT_printf(0, "Debug message: %d\n", value);

   /* Read from RTT */
   char buffer[64];
   int bytes = SEGGER_RTT_Read(0, buffer, sizeof(buffer));

Common Issues
-------------

Timing Issues
~~~~~~~~~~~~~

**Race Conditions**

.. code-block:: c

   /* Problem: Race condition */
   static volatile int counter = 0;

   void task1(void) {
       counter++;  /* Not atomic! */
   }

   void task2(void) {
       counter++;  /* Not atomic! */
   }

   /* Solution: Use mutex or atomic operations */
   static osal_mutex_handle_t counter_mutex;
   static int counter = 0;

   void task1(void) {
       osal_mutex_lock(counter_mutex, OSAL_WAIT_FOREVER);
       counter++;
       osal_mutex_unlock(counter_mutex);
   }

**Deadlocks**

.. code-block:: c

   /* Problem: Deadlock */
   void task1(void) {
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);  /* Deadlock! */
       /* Work */
       osal_mutex_unlock(mutex_b);
       osal_mutex_unlock(mutex_a);
   }

   void task2(void) {
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);  /* Deadlock! */
       /* Work */
       osal_mutex_unlock(mutex_a);
       osal_mutex_unlock(mutex_b);
   }

   /* Solution: Always acquire locks in same order */
   void task1(void) {
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);
       /* Work */
       osal_mutex_unlock(mutex_b);
       osal_mutex_unlock(mutex_a);
   }

   void task2(void) {
       osal_mutex_lock(mutex_a, OSAL_WAIT_FOREVER);  /* Same order */
       osal_mutex_lock(mutex_b, OSAL_WAIT_FOREVER);
       /* Work */
       osal_mutex_unlock(mutex_b);
       osal_mutex_unlock(mutex_a);
   }

Memory Issues
~~~~~~~~~~~~~

**Stack Overflow**

Symptoms:
* Random crashes
* Corrupted variables
* Hard faults

Detection:
* Enable stack overflow detection in RTOS
* Use stack canaries
* Monitor stack usage

**Heap Fragmentation**

Symptoms:
* Allocation failures despite available memory
* Increasing allocation time

Solutions:
* Use fixed-size memory pools
* Minimize dynamic allocation
* Use memory allocator with defragmentation

**Memory Leaks**

Detection:
* Track allocations and frees
* Use Valgrind on native platform
* Monitor heap usage over time

Interrupt Issues
~~~~~~~~~~~~~~~~

**Priority Inversion**

Problem: Low-priority task holds resource needed by high-priority task

Solution: Use priority inheritance mutexes

**Interrupt Latency**

Problem: Interrupts disabled too long

Solutions:
* Minimize critical sections
* Use nested interrupts
* Defer work to tasks

Best Practices
--------------

Defensive Programming
~~~~~~~~~~~~~~~~~~~~~

* Validate all inputs
* Check all return values
* Use assertions liberally
* Initialize all variables
* Handle all error cases

Reproducible Bugs
~~~~~~~~~~~~~~~~~

* Create minimal test case
* Document steps to reproduce
* Identify conditions that trigger bug
* Automate reproduction if possible

Debugging Checklist
~~~~~~~~~~~~~~~~~~~

☐ Can you reproduce the bug consistently?
☐ What are the exact steps to reproduce?
☐ What is the expected behavior?
☐ What is the actual behavior?
☐ When did the bug first appear?
☐ What changed recently?
☐ Does it happen on all platforms?
☐ Are there any error messages?
☐ What is the call stack?
☐ What are the variable values?

See Also
--------

* :doc:`testing` - Testing strategies
* :doc:`development_environment` - Development setup
* :doc:`troubleshooting` - Common issues

Summary
-------

Effective debugging requires:

* **Tools**: GDB, Valgrind, ASan, OpenOCD, J-Link
* **Techniques**: Printf, breakpoints, assertions, memory debugging
* **Hardware**: SWD/JTAG, SWV trace, RTT
* **Best Practices**: Defensive programming, reproducible bugs, systematic approach

Master these debugging techniques to efficiently identify and fix issues in the Nexus platform.
