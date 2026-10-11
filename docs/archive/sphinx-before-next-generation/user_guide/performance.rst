Performance Optimization
========================

This comprehensive guide covers performance analysis, optimization techniques, and best practices for Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Performance optimization is critical for embedded systems with limited resources. This guide provides strategies for analyzing and improving performance.

**Optimization Goals:**

* Minimize CPU usage
* Reduce memory footprint
* Decrease power consumption
* Improve response time
* Maximize throughput

**Optimization Process:**

1. **Measure** - Profile to find bottlenecks
2. **Analyze** - Understand performance issues
3. **Optimize** - Apply targeted improvements
4. **Verify** - Measure improvements
5. **Iterate** - Repeat until goals met

.. warning::

   Premature optimization is the root of all evil. Always measure before optimizing!

Performance Measurement
-----------------------

Timing Measurements
~~~~~~~~~~~~~~~~~~~

**Microsecond Timing:**

.. code-block:: c

   #include "hal/nx_timer.h"

   void measure_function_time(void)
   {
       /* Start high-resolution timer */
       uint32_t start = hal_timer_get_counter(TIMER_0);

       /* Function to measure */
       perform_operation();

       /* Calculate elapsed time */
       uint32_t end = hal_timer_get_counter(TIMER_0);
       uint32_t cycles = end - start;
       uint32_t us = cycles / (SystemCoreClock / 1000000);

       LOG_INFO("Operation took %lu us (%lu cycles)", us, cycles);
   }

**Millisecond Timing:**

.. code-block:: c

   #include "osal/osal.h"

   void measure_task_time(void)
   {
       uint32_t start = osal_get_time_ms();

       /* Task work */
       process_data();

       uint32_t elapsed = osal_get_time_ms() - start;
       LOG_INFO("Task took %lu ms", elapsed);
   }

**Profiling Macros:**

.. code-block:: c

   #ifdef PROFILE
   #define PROFILE_START(name) \
       uint32_t __profile_##name##_start = hal_timer_get_counter(TIMER_0)

   #define PROFILE_END(name) \
       do { \
           uint32_t __end = hal_timer_get_counter(TIMER_0); \
           uint32_t __cycles = __end - __profile_##name##_start; \
           uint32_t __us = __cycles / (SystemCoreClock / 1000000); \
           LOG_INFO("PROFILE %s: %lu us (%lu cycles)", \
                    #name, __us, __cycles); \
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

   void print_cpu_usage(void)
   {
       char stats_buffer[512];
       vTaskGetRunTimeStats(stats_buffer);

       LOG_INFO("Task Statistics:");
       LOG_INFO("%s", stats_buffer);
   }

   uint32_t get_cpu_usage_percent(void)
   {
       TaskStatus_t* task_array;
       uint32_t total_runtime;
       uint32_t num_tasks;

       /* Get task count */
       num_tasks = uxTaskGetNumberOfTasks();

       /* Allocate array */
       task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       if (!task_array) {
           return 0;
       }

       /* Get task stats */
       num_tasks = uxTaskGetSystemState(task_array, num_tasks, &total_runtime);

       /* Calculate CPU usage */
       uint32_t idle_runtime = 0;
       for (uint32_t i = 0; i < num_tasks; i++) {
           if (strcmp(task_array[i].pcTaskName, "IDLE") == 0) {
               idle_runtime = task_array[i].ulRunTimeCounter;
               break;
           }
       }

       vPortFree(task_array);

       if (total_runtime == 0) {
           return 0;
       }

       uint32_t cpu_usage = 100 - ((idle_runtime * 100) / total_runtime);
       return cpu_usage;
   }

   #endif

**Idle Task Hook:**

.. code-block:: c

   static uint32_t idle_count = 0;
   static uint32_t last_check_time = 0;

   void vApplicationIdleHook(void)
   {
       idle_count++;

       /* Check CPU usage every second */
       uint32_t now = osal_get_time_ms();
       if (now - last_check_time >= 1000) {
           uint32_t cpu_usage = get_cpu_usage_percent();
           LOG_DEBUG("CPU usage: %lu%%", cpu_usage);

           last_check_time = now;
           idle_count = 0;
       }
   }

Memory Profiling
~~~~~~~~~~~~~~~~

**Stack Usage:**

.. code-block:: c

   void check_stack_usage(void)
   {
       osal_task_handle_t current = osal_task_get_current();
       uint32_t high_water = osal_task_get_stack_high_water(current);
       uint32_t stack_size = osal_task_get_stack_size(current);
       uint32_t used = stack_size - high_water;
       uint32_t percent = (used * 100) / stack_size;

       LOG_INFO("Task: %s", osal_task_get_name(current));
       LOG_INFO("Stack: %lu/%lu bytes (%lu%%)", used, stack_size, percent);

       if (percent > 80) {
           LOG_WARN("Stack usage high!");
       }
   }

   void check_all_tasks_stack(void)
   {
       TaskStatus_t* task_array;
       uint32_t num_tasks = uxTaskGetNumberOfTasks();

       task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       if (!task_array) {
           return;
       }

       num_tasks = uxTaskGetSystemState(task_array, num_tasks, NULL);

       LOG_INFO("Task Stack Usage:");
       for (uint32_t i = 0; i < num_tasks; i++) {
           uint32_t high_water = task_array[i].usStackHighWaterMark;
           LOG_INFO("  %s: %lu bytes free",
                    task_array[i].pcTaskName, high_water);
       }

       vPortFree(task_array);
   }

**Heap Usage:**

.. code-block:: c

   void check_heap_usage(void)
   {
       size_t free_heap = osal_get_free_heap_size();
       size_t min_free = osal_get_minimum_ever_free_heap_size();
       size_t total_heap = configTOTAL_HEAP_SIZE;
       size_t used = total_heap - free_heap;
       uint32_t percent = (used * 100) / total_heap;

       LOG_INFO("Heap Usage:");
       LOG_INFO("  Total: %zu bytes", total_heap);
       LOG_INFO("  Used: %zu bytes (%lu%%)", used, percent);
       LOG_INFO("  Free: %zu bytes", free_heap);
       LOG_INFO("  Min Free: %zu bytes", min_free);

       if (percent > 90) {
           LOG_WARN("Heap usage critical!");
       }
   }

Interrupt Latency
~~~~~~~~~~~~~~~~~

**Measure Interrupt Response:**

.. code-block:: c

   static volatile uint32_t irq_entry_time = 0;
   static volatile uint32_t irq_exit_time = 0;

   void EXTI0_IRQHandler(void)
   {
       /* Record entry time */
       irq_entry_time = hal_timer_get_counter(TIMER_0);

       /* Handle interrupt */
       handle_button_press();

       /* Record exit time */
       irq_exit_time = hal_timer_get_counter(TIMER_0);

       /* Clear interrupt flag */
       EXTI->PR = EXTI_PR_PR0;
   }

   void check_irq_latency(void)
   {
       if (irq_exit_time > irq_entry_time) {
           uint32_t cycles = irq_exit_time - irq_entry_time;
           uint32_t us = cycles / (SystemCoreClock / 1000000);
           LOG_INFO("IRQ latency: %lu us", us);
       }
   }

Compiler Optimizations
----------------------

Optimization Levels
~~~~~~~~~~~~~~~~~~~

**GCC/Clang Optimization Flags:**

+-------------+------------------+---------------------------+
| Level       | Flags            | Description               |
+=============+==================+===========================+
| ``-O0``     | No optimization  | Debug builds              |
+-------------+------------------+---------------------------+
| ``-Og``     | Debug optimize   | Debuggable optimization   |
+-------------+------------------+---------------------------+
| ``-O1``     | Basic optimize   | Moderate optimization     |
+-------------+------------------+---------------------------+
| ``-O2``     | Full optimize    | Recommended for release   |
+-------------+------------------+---------------------------+
| ``-O3``     | Aggressive       | Maximum speed             |
+-------------+------------------+---------------------------+
| ``-Os``     | Size optimize    | Minimum code size         |
+-------------+------------------+---------------------------+
| ``-Ofast``  | Fast math        | Non-standard compliant    |
+-------------+------------------+---------------------------+

**CMake Configuration:**

.. code-block:: cmake

   # Release build with -O2
   set(CMAKE_BUILD_TYPE Release)

   # Size optimization
   set(CMAKE_BUILD_TYPE MinSizeRel)

   # Custom optimization
   add_compile_options(-O3 -flto)

Link-Time Optimization (LTO)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Enable LTO:**

.. code-block:: cmake

   # CMakeLists.txt
   if(CMAKE_BUILD_TYPE STREQUAL "Release")
       set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
       add_compile_options(-flto)
       add_link_options(-flto)
   endif()

**Benefits:**

* Cross-module inlining
* Dead code elimination
* Better optimization opportunities
* Smaller binary size

**Trade-offs:**

* Longer build times
* Higher memory usage during linking
* May complicate debugging

Function Inlining
~~~~~~~~~~~~~~~~~

**Inline Functions:**

.. code-block:: c

   /* Force inline */
   static inline __attribute__((always_inline))
   uint32_t fast_multiply(uint32_t a, uint32_t b)
   {
       return a * b;
   }

   /* Suggest inline */
   static inline uint32_t calculate_checksum(const uint8_t* data, size_t len)
   {
       uint32_t sum = 0;
       for (size_t i = 0; i < len; i++) {
           sum += data[i];
       }
       return sum;
   }

   /* Never inline (for debugging) */
   __attribute__((noinline))
   void debug_function(void)
   {
       /* ... */
   }

**When to Inline:**

* Small functions (<10 lines)
* Functions called frequently
* Functions in hot paths
* Simple calculations

**When NOT to Inline:**

* Large functions
* Rarely called functions
* Functions with loops
* Recursive functions

Code Optimization Techniques
-----------------------------

Algorithm Optimization
~~~~~~~~~~~~~~~~~~~~~~

**Choose Efficient Algorithms:**

.. code-block:: c

   /* Bad: O(n²) bubble sort */
   void bubble_sort(int* arr, size_t n)
   {
       for (size_t i = 0; i < n - 1; i++) {
           for (size_t j = 0; j < n - i - 1; j++) {
               if (arr[j] > arr[j + 1]) {
                   int temp = arr[j];
                   arr[j] = arr[j + 1];
                   arr[j + 1] = temp;
               }
           }
       }
   }

   /* Better: O(n log n) quicksort */
   void quicksort(int* arr, int low, int high)
   {
       if (low < high) {
           int pivot = partition(arr, low, high);
           quicksort(arr, low, pivot - 1);
           quicksort(arr, pivot + 1, high);
       }
   }

**Use Lookup Tables:**

.. code-block:: c

   /* Bad: Calculate every time */
   uint8_t calculate_crc(uint8_t data)
   {
       uint8_t crc = 0;
       for (int i = 0; i < 8; i++) {
           if ((crc ^ data) & 0x01) {
               crc = (crc >> 1) ^ 0x8C;
           } else {
               crc >>= 1;
           }
           data >>= 1;
       }
       return crc;
   }

   /* Good: Use lookup table */
   static const uint8_t crc_table[256] = {
       0x00, 0x07, 0x0E, 0x09, /* ... */
   };

   uint8_t calculate_crc_fast(uint8_t data)
   {
       return crc_table[data];
   }

Loop Optimization
~~~~~~~~~~~~~~~~~

**Loop Unrolling:**

.. code-block:: c

   /* Original loop */
   void copy_data(uint8_t* dst, const uint8_t* src, size_t len)
   {
       for (size_t i = 0; i < len; i++) {
           dst[i] = src[i];
       }
   }

   /* Unrolled loop (4x) */
   void copy_data_fast(uint8_t* dst, const uint8_t* src, size_t len)
   {
       size_t i = 0;

       /* Process 4 bytes at a time */
       for (; i + 4 <= len; i += 4) {
           dst[i + 0] = src[i + 0];
           dst[i + 1] = src[i + 1];
           dst[i + 2] = src[i + 2];
           dst[i + 3] = src[i + 3];
       }

       /* Handle remaining bytes */
       for (; i < len; i++) {
           dst[i] = src[i];
       }
   }

**Loop Invariant Code Motion:**

.. code-block:: c

   /* Bad: Recalculate every iteration */
   void process_array(int* arr, size_t len, int factor)
   {
       for (size_t i = 0; i < len; i++) {
           arr[i] = arr[i] * (factor + 10);  /* factor + 10 is invariant */
       }
   }

   /* Good: Calculate once */
   void process_array_fast(int* arr, size_t len, int factor)
   {
       int multiplier = factor + 10;  /* Move out of loop */
       for (size_t i = 0; i < len; i++) {
           arr[i] = arr[i] * multiplier;
       }
   }

**Strength Reduction:**

.. code-block:: c

   /* Bad: Use expensive operations */
   void calculate_powers(int* result, int base, size_t n)
   {
       for (size_t i = 0; i < n; i++) {
           result[i] = pow(base, i);  /* Expensive */
       }
   }

   /* Good: Use cheaper operations */
   void calculate_powers_fast(int* result, int base, size_t n)
   {
       int power = 1;
       for (size_t i = 0; i < n; i++) {
           result[i] = power;
           power *= base;  /* Cheaper than pow() */
       }
   }

Data Structure Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Use Appropriate Data Structures:**

.. code-block:: c

   /* Bad: Linear search in array */
   typedef struct {
       int id;
       char name[32];
   } device_t;

   device_t devices[100];

   device_t* find_device(int id)
   {
       for (int i = 0; i < 100; i++) {
           if (devices[i].id == id) {
               return &devices[i];
           }
       }
       return NULL;
   }

   /* Good: Use hash table */
   #define HASH_SIZE 16

   typedef struct device_node {
       device_t device;
       struct device_node* next;
   } device_node_t;

   device_node_t* hash_table[HASH_SIZE];

   uint32_t hash(int id)
   {
       return id % HASH_SIZE;
   }

   device_t* find_device_fast(int id)
   {
       uint32_t index = hash(id);
       device_node_t* node = hash_table[index];

       while (node) {
           if (node->device.id == id) {
               return &node->device;
           }
           node = node->next;
       }
       return NULL;
   }

**Pack Structures:**

.. code-block:: c

   /* Bad: Unpacked structure (12 bytes on 32-bit) */
   typedef struct {
       uint8_t flag;      /* 1 byte + 3 padding */
       uint32_t value;    /* 4 bytes */
       uint8_t status;    /* 1 byte + 3 padding */
   } unpacked_t;

   /* Good: Packed structure (6 bytes) */
   typedef struct __attribute__((packed)) {
       uint8_t flag;      /* 1 byte */
       uint8_t status;    /* 1 byte */
       uint32_t value;    /* 4 bytes */
   } packed_t;

   /* Better: Aligned and packed (8 bytes, but faster access) */
   typedef struct {
       uint32_t value;    /* 4 bytes */
       uint8_t flag;      /* 1 byte */
       uint8_t status;    /* 1 byte */
       uint16_t padding;  /* 2 bytes explicit padding */
   } aligned_t;

Memory Access Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Cache-Friendly Access:**

.. code-block:: c

   /* Bad: Column-major access (cache unfriendly) */
   void process_matrix_bad(int matrix[100][100])
   {
       for (int col = 0; col < 100; col++) {
           for (int row = 0; row < 100; row++) {
               matrix[row][col] *= 2;
           }
       }
   }

   /* Good: Row-major access (cache friendly) */
   void process_matrix_good(int matrix[100][100])
   {
       for (int row = 0; row < 100; row++) {
           for (int col = 0; col < 100; col++) {
               matrix[row][col] *= 2;
           }
       }
   }

**Alignment:**

.. code-block:: c

   /* Ensure proper alignment for DMA */
   __attribute__((aligned(32)))
   uint8_t dma_buffer[1024];

   /* Align structure to cache line */
   typedef struct __attribute__((aligned(64))) {
       uint32_t data[16];
   } cache_aligned_t;

Hardware Acceleration
---------------------

DMA Usage
~~~~~~~~~

**Use DMA for Large Transfers:**

.. code-block:: c

   /* Bad: CPU copy */
   void copy_large_buffer(uint8_t* dst, const uint8_t* src, size_t len)
   {
       for (size_t i = 0; i < len; i++) {
           dst[i] = src[i];
       }
   }

   /* Good: DMA copy */
   void copy_large_buffer_dma(uint8_t* dst, const uint8_t* src, size_t len)
   {
       nx_dma_config_t config = {
           .direction = DMA_MEMORY_TO_MEMORY,
           .src_inc = DMA_INC_ENABLE,
           .dst_inc = DMA_INC_ENABLE,
           .data_width = DMA_WIDTH_BYTE,
       };

       nx_dma_t* dma = nx_factory_dma(0);
       dma->configure(dma, &config);
       dma->start(dma, src, dst, len);
       dma->wait(dma, 1000);
       nx_factory_dma_release(dma);
   }

**DMA for Peripheral I/O:**

.. code-block:: c

   /* Use DMA for UART transmission */
   void uart_send_dma(nx_uart_t* uart, const uint8_t* data, size_t len)
   {
       nx_tx_dma_t* tx_dma = uart->get_tx_dma(uart);
       if (tx_dma) {
           tx_dma->send(tx_dma, data, len);
           /* CPU is free to do other work */
       }
   }

Hardware Crypto
~~~~~~~~~~~~~~~

**Use Hardware Acceleration:**

.. code-block:: c

   /* Software AES (slow) */
   void aes_encrypt_sw(const uint8_t* key, const uint8_t* input,
                       uint8_t* output)
   {
       /* Software AES implementation */
       sw_aes_encrypt(key, input, output);
   }

   /* Hardware AES (fast) */
   void aes_encrypt_hw(const uint8_t* key, const uint8_t* input,
                       uint8_t* output)
   {
       nx_crypto_t* crypto = nx_factory_crypto(0);
       crypto->aes_encrypt(crypto, key, input, output);
       nx_factory_crypto_release(crypto);
   }

RTOS Optimization
-----------------

Task Priority
~~~~~~~~~~~~~

**Set Appropriate Priorities:**

.. code-block:: c

   /* High priority for time-critical tasks */
   osal_task_create(isr_handler_task, "isr", 512, NULL,
                    OSAL_PRIORITY_REALTIME, &isr_task);

   /* Normal priority for regular tasks */
   osal_task_create(processing_task, "proc", 1024, NULL,
                    OSAL_PRIORITY_NORMAL, &proc_task);

   /* Low priority for background tasks */
   osal_task_create(logging_task, "log", 512, NULL,
                    OSAL_PRIORITY_LOW, &log_task);

**Priority Inversion:**

.. code-block:: c

   /* Use priority inheritance mutexes */
   osal_mutex_config_t config = {
       .type = OSAL_MUTEX_RECURSIVE,
       .priority_inherit = true,  /* Enable priority inheritance */
   };

   osal_mutex_handle_t mutex;
   osal_mutex_create_ex(&config, &mutex);

Task Stack Size
~~~~~~~~~~~~~~~

**Optimize Stack Sizes:**

.. code-block:: c

   /* Measure actual stack usage */
   void optimize_stack_sizes(void)
   {
       TaskStatus_t* tasks;
       uint32_t num_tasks = uxTaskGetNumberOfTasks();

       tasks = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       num_tasks = uxTaskGetSystemState(tasks, num_tasks, NULL);

       for (uint32_t i = 0; i < num_tasks; i++) {
           uint32_t high_water = tasks[i].usStackHighWaterMark;
           uint32_t stack_size = tasks[i].usStackHighWaterMark * 4;  /* Approx */

           LOG_INFO("Task %s: %lu bytes free (reduce stack?)",
                    tasks[i].pcTaskName, high_water);
       }

       vPortFree(tasks);
   }

Synchronization Overhead
~~~~~~~~~~~~~~~~~~~~~~~~~

**Minimize Lock Contention:**

.. code-block:: c

   /* Bad: Hold lock during slow operation */
   void process_data_bad(void)
   {
       osal_mutex_lock(data_mutex, OSAL_WAIT_FOREVER);

       /* Long operation while holding lock */
       for (int i = 0; i < 1000; i++) {
           process_item(i);
       }

       osal_mutex_unlock(data_mutex);
   }

   /* Good: Minimize critical section */
   void process_data_good(void)
   {
       /* Copy data while holding lock */
       osal_mutex_lock(data_mutex, OSAL_WAIT_FOREVER);
       memcpy(local_buffer, shared_buffer, sizeof(local_buffer));
       osal_mutex_unlock(data_mutex);

       /* Process local copy without lock */
       for (int i = 0; i < 1000; i++) {
           process_item(local_buffer[i]);
       }
   }

**Use Lock-Free Algorithms:**

.. code-block:: c

   /* Lock-free ring buffer */
   typedef struct {
       volatile uint32_t head;
       volatile uint32_t tail;
       uint8_t buffer[256];
   } lockfree_ringbuf_t;

   bool ringbuf_push(lockfree_ringbuf_t* rb, uint8_t data)
   {
       uint32_t next_head = (rb->head + 1) % 256;
       if (next_head == rb->tail) {
           return false;  /* Full */
       }

       rb->buffer[rb->head] = data;
       rb->head = next_head;  /* Atomic on Cortex-M */
       return true;
   }

Power Optimization
------------------

See :doc:`power_management` for detailed power optimization techniques.

**Quick Tips:**

* Use sleep modes when idle
* Reduce clock frequency when possible
* Disable unused peripherals
* Use DMA to allow CPU sleep
* Optimize interrupt handlers

Code Size Optimization
----------------------

Compiler Flags
~~~~~~~~~~~~~~

**Size Optimization:**

.. code-block:: cmake

   # Optimize for size
   set(CMAKE_BUILD_TYPE MinSizeRel)

   # Additional size flags
   add_compile_options(
       -Os                    # Optimize for size
       -ffunction-sections    # Each function in own section
       -fdata-sections        # Each data in own section
   )

   add_link_options(
       -Wl,--gc-sections      # Remove unused sections
       -Wl,--print-gc-sections # Print removed sections
   )

Remove Unused Code
~~~~~~~~~~~~~~~~~~

**Conditional Compilation:**

.. code-block:: c

   /* Remove debug code in release builds */
   #ifdef DEBUG
   void debug_print_state(void)
   {
       /* Debug code */
   }
   #endif

   /* Use Kconfig to remove features */
   #ifdef CONFIG_FEATURE_ADVANCED
   void advanced_feature(void)
   {
       /* Advanced feature code */
   }
   #endif

**Link-Time Garbage Collection:**

.. code-block:: cmake

   # Remove unused functions at link time
   add_compile_options(-ffunction-sections -fdata-sections)
   add_link_options(-Wl,--gc-sections)

Reduce Library Size
~~~~~~~~~~~~~~~~~~~

**Use Minimal Libraries:**

.. code-block:: cmake

   # Use newlib-nano for smaller C library
   add_link_options(--specs=nano.specs)

   # Remove floating point printf support
   add_compile_definitions(PRINTF_DISABLE_SUPPORT_FLOAT)

Best Practices
--------------

1. **Measure First**
   * Profile before optimizing
   * Identify real bottlenecks
   * Set performance goals
   * Measure improvements

2. **Optimize Hot Paths**
   * Focus on frequently executed code
   * Optimize inner loops
   * Optimize interrupt handlers
   * Optimize critical sections

3. **Choose Right Algorithms**
   * Use appropriate data structures
   * Consider time/space trade-offs
   * Use standard library when possible
   * Benchmark alternatives

4. **Minimize Memory Access**
   * Use registers when possible
   * Reduce cache misses
   * Align data properly
   * Use DMA for large transfers

5. **Reduce Overhead**
   * Minimize function calls
   * Reduce context switches
   * Minimize lock contention
   * Use efficient synchronization

6. **Balance Optimization**
   * Don't sacrifice readability
   * Don't sacrifice maintainability
   * Don't sacrifice correctness
   * Document optimizations

7. **Test Thoroughly**
   * Verify correctness after optimization
   * Test edge cases
   * Test on target hardware
   * Measure actual improvements

Performance Checklist
---------------------

**Before Optimization:**

- [ ] Profile application
- [ ] Identify bottlenecks
- [ ] Set performance goals
- [ ] Establish baseline measurements

**During Optimization:**

- [ ] Focus on hot paths
- [ ] One optimization at a time
- [ ] Measure each change
- [ ] Document optimizations

**After Optimization:**

- [ ] Verify correctness
- [ ] Measure improvements
- [ ] Update documentation
- [ ] Review code quality

See Also
--------

* :doc:`profiling` - Performance Profiling
* :doc:`memory_management` - Memory Management
* :doc:`power_management` - Power Management
* :doc:`../development/performance_optimization` - Development Performance Guide

