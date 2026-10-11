Memory Management
=================

Comprehensive guide to memory management in Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Effective memory management is critical for embedded systems with limited RAM. This guide covers memory allocation strategies, optimization techniques, and debugging tools.

**Memory Types:**

* **Stack** - Automatic variables, function calls
* **Heap** - Dynamic allocation
* **Static** - Global and static variables
* **Code** - Program instructions (Flash/ROM)

**Key Concepts:**

* Memory layout and sections
* Stack and heap management
* Memory pools
* Memory fragmentation
* Memory debugging

Memory Layout
-------------

Typical ARM Cortex-M Layout
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   ┌─────────────────────┐ 0xFFFFFFFF
   │   Device Registers  │
   ├─────────────────────┤ 0x40000000
   │   External RAM      │
   ├─────────────────────┤ 0x20000000
   │   SRAM              │
   │  ┌───────────────┐  │
   │  │  Heap (grows→)│  │
   │  ├───────────────┤  │
   │  │  Free Space   │  │
   │  ├───────────────┤  │
   │  │ (←grows Stack)│  │
   │  ├───────────────┤  │
   │  │  .bss (zero)  │  │
   │  ├───────────────┤  │
   │  │  .data (init) │  │
   │  └───────────────┘  │
   ├─────────────────────┤ 0x08000000
   │   Flash/ROM         │
   │  ┌───────────────┐  │
   │  │  .text (code) │  │
   │  ├───────────────┤  │
   │  │  .rodata      │  │
   │  └───────────────┘  │
   └─────────────────────┘ 0x00000000

**Memory Sections:**

* ``.text`` - Program code
* ``.rodata`` - Read-only data (const)
* ``.data`` - Initialized global/static variables
* ``.bss`` - Zero-initialized global/static variables
* ``.heap`` - Dynamic allocation
* ``.stack`` - Function call stack

Linker Script
~~~~~~~~~~~~~

**Example Linker Script (STM32F4):**

.. code-block:: text

   MEMORY
   {
       FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
       RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 192K
   }

   SECTIONS
   {
       .text :
       {
           *(.isr_vector)
           *(.text*)
           *(.rodata*)
       } > FLASH

       .data :
       {
           _sdata = .;
           *(.data*)
           _edata = .;
       } > RAM AT> FLASH

       .bss :
       {
           _sbss = .;
           *(.bss*)
           *(COMMON)
           _ebss = .;
       } > RAM

       ._user_heap_stack :
       {
           . = ALIGN(8);
           PROVIDE(end = .);
           PROVIDE(_heap_start = .);
           . = . + _Min_Heap_Size;
           . = . + _Min_Stack_Size;
           . = ALIGN(8);
       } > RAM
   }

Stack Management
----------------

Stack Allocation
~~~~~~~~~~~~~~~~

**FreeRTOS Task Stacks:**

.. code-block:: c

   /* Create task with specific stack size */
   osal_task_config_t config = {
       .name = "my_task",
       .func = my_task_function,
       .arg = NULL,
       .stack_size = 1024,  /* 1KB stack */
       .priority = OSAL_PRIORITY_NORMAL,
   };

   osal_task_handle_t task;
   osal_task_create(&config, &task);

**Stack Size Guidelines:**

+-------------------+------------------+
| Task Type         | Recommended Size |
+===================+==================+
| Minimal           | 256-512 bytes    |
+-------------------+------------------+
| Normal            | 512-1024 bytes   |
+-------------------+------------------+
| Complex           | 1024-2048 bytes  |
+-------------------+------------------+
| With printf       | +512 bytes       |
+-------------------+------------------+
| With FPU          | +128 bytes       |
+-------------------+------------------+

Stack Usage Monitoring
~~~~~~~~~~~~~~~~~~~~~~

**Check Stack High Water Mark:**

.. code-block:: c

   void monitor_stack_usage(void)
   {
       osal_task_handle_t current = osal_task_get_current();
       uint32_t high_water = osal_task_get_stack_high_water(current);
       uint32_t stack_size = osal_task_get_stack_size(current);
       uint32_t used = stack_size - high_water;
       uint32_t percent = (used * 100) / stack_size;

       LOG_INFO("Task: %s", osal_task_get_name(current));
       LOG_INFO("Stack: %lu/%lu bytes (%lu%% used)",
                used, stack_size, percent);

       if (percent > 80) {
           LOG_WARN("Stack usage high - consider increasing size");
       }
   }

**Stack Overflow Detection:**

.. code-block:: c

   /* FreeRTOS stack overflow hook */
   void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
   {
       LOG_FATAL("Stack overflow in task: %s", pcTaskName);

       /* Dump task information */
       LOG_FATAL("Task handle: %p", xTask);

       /* Enter safe state */
       __disable_irq();
       while (1) {
           /* Halt system */
       }
   }

**Stack Canaries:**

.. code-block:: c

   /* Enable stack protection */
   #define STACK_CANARY 0xDEADBEEF

   typedef struct {
       uint32_t canary_start;
       uint8_t stack[1024];
       uint32_t canary_end;
   } protected_stack_t;

   void check_stack_canary(protected_stack_t* stack)
   {
       if (stack->canary_start != STACK_CANARY ||
           stack->canary_end != STACK_CANARY) {
           LOG_FATAL("Stack corruption detected!");
       }
   }

Heap Management
---------------

Dynamic Allocation
~~~~~~~~~~~~~~~~~~

**OSAL Memory Functions:**

.. code-block:: c

   /* Allocate memory */
   void* ptr = osal_malloc(1024);
   if (!ptr) {
       LOG_ERROR("Memory allocation failed");
       return;
   }

   /* Use memory */
   memset(ptr, 0, 1024);

   /* Free memory */
   osal_free(ptr);

**Aligned Allocation:**

.. code-block:: c

   /* Allocate aligned memory (for DMA) */
   void* aligned_ptr = osal_malloc_aligned(1024, 32);
   if (!aligned_ptr) {
       LOG_ERROR("Aligned allocation failed");
       return;
   }

   /* Use for DMA */
   dma_transfer(aligned_ptr, 1024);

   /* Free aligned memory */
   osal_free_aligned(aligned_ptr);

Heap Monitoring
~~~~~~~~~~~~~~~

**Check Heap Usage:**

.. code-block:: c

   void monitor_heap_usage(void)
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
       LOG_INFO("  Min Free Ever: %zu bytes", min_free);

       if (percent > 90) {
           LOG_WARN("Heap usage critical!");
       }

       if (min_free < 1024) {
           LOG_WARN("Heap has been nearly exhausted!");
       }
   }

**Periodic Monitoring:**

.. code-block:: c

   void heap_monitor_task(void* arg)
   {
       while (1) {
           monitor_heap_usage();
           osal_task_delay(10000);  /* Check every 10 seconds */
       }
   }

Memory Pools
------------

Fixed-Size Pools
~~~~~~~~~~~~~~~~

**Create Memory Pool:**

.. code-block:: c

   /* Define pool for message structures */
   typedef struct {
       uint32_t id;
       uint8_t data[64];
   } message_t;

   #define POOL_SIZE 10

   osal_pool_handle_t message_pool;

   void init_message_pool(void)
   {
       osal_pool_create(&message_pool, POOL_SIZE, sizeof(message_t));
   }

**Use Memory Pool:**

.. code-block:: c

   void send_message(uint32_t id, const uint8_t* data, size_t len)
   {
       /* Allocate from pool */
       message_t* msg = osal_pool_alloc(message_pool);
       if (!msg) {
           LOG_ERROR("Pool exhausted");
           return;
       }

       /* Fill message */
       msg->id = id;
       memcpy(msg->data, data, len);

       /* Send message */
       osal_queue_send(msg_queue, &msg, 0);
   }

   void receive_message(void)
   {
       message_t* msg;

       /* Receive message */
       if (osal_queue_receive(msg_queue, &msg, 1000) == OSAL_OK) {
           /* Process message */
           process_message(msg);

           /* Return to pool */
           osal_pool_free(message_pool, msg);
       }
   }

**Pool Statistics:**

.. code-block:: c

   void print_pool_stats(void)
   {
       uint32_t available = osal_pool_get_available(message_pool);
       uint32_t total = POOL_SIZE;
       uint32_t used = total - available;

       LOG_INFO("Message Pool:");
       LOG_INFO("  Total: %lu", total);
       LOG_INFO("  Used: %lu", used);
       LOG_INFO("  Available: %lu", available);
   }

Static Allocation
-----------------

Compile-Time Allocation
~~~~~~~~~~~~~~~~~~~~~~~

**Static Buffers:**

.. code-block:: c

   /* Static allocation - no runtime overhead */
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

**Static RTOS Objects:**

.. code-block:: c

   /* FreeRTOS static allocation */
   #define TASK_STACK_SIZE 1024

   static StackType_t task_stack[TASK_STACK_SIZE];
   static StaticTask_t task_tcb;

   void create_static_task(void)
   {
       TaskHandle_t task = xTaskCreateStatic(
           task_function,
           "static_task",
           TASK_STACK_SIZE,
           NULL,
           tskIDLE_PRIORITY + 1,
           task_stack,
           &task_tcb
       );
   }

Memory Fragmentation
--------------------

Causes and Prevention
~~~~~~~~~~~~~~~~~~~~~

**Fragmentation Example:**

.. code-block:: text

   Initial:  [--------FREE--------]

   Alloc A:  [AAA-----FREE--------]
   Alloc B:  [AAABBB--FREE--------]
   Alloc C:  [AAABBBCC-FREE-------]

   Free B:   [AAA---CC-FREE-------]  ← Fragmentation!

   Alloc D:  Can't allocate 4 bytes even though 6 bytes free

**Prevention Strategies:**

1. **Use Memory Pools** - Fixed-size allocations
2. **Allocate Once** - Allocate at startup, never free
3. **Use Static Allocation** - No fragmentation
4. **Allocate in Order** - Allocate large blocks first
5. **Avoid Frequent Alloc/Free** - Reuse buffers

**Example: Buffer Reuse:**

.. code-block:: c

   /* Bad: Allocate/free repeatedly */
   void process_data_bad(void)
   {
       for (int i = 0; i < 100; i++) {
           uint8_t* buffer = malloc(1024);
           process_buffer(buffer);
           free(buffer);  /* Causes fragmentation */
       }
   }

   /* Good: Reuse buffer */
   void process_data_good(void)
   {
       uint8_t* buffer = malloc(1024);
       if (!buffer) return;

       for (int i = 0; i < 100; i++) {
           process_buffer(buffer);
       }

       free(buffer);
   }

Memory Debugging
----------------

Leak Detection
~~~~~~~~~~~~~~

**Track Allocations:**

.. code-block:: c

   #ifdef DEBUG_MEMORY

   typedef struct {
       void* ptr;
       size_t size;
       const char* file;
       int line;
   } alloc_info_t;

   #define MAX_ALLOCS 100
   static alloc_info_t alloc_table[MAX_ALLOCS];
   static size_t alloc_count = 0;

   void* debug_malloc(size_t size, const char* file, int line)
   {
       void* ptr = malloc(size);
       if (ptr && alloc_count < MAX_ALLOCS) {
           alloc_table[alloc_count].ptr = ptr;
           alloc_table[alloc_count].size = size;
           alloc_table[alloc_count].file = file;
           alloc_table[alloc_count].line = line;
           alloc_count++;
       }
       return ptr;
   }

   void debug_free(void* ptr)
   {
       for (size_t i = 0; i < alloc_count; i++) {
           if (alloc_table[i].ptr == ptr) {
               /* Remove from table */
               alloc_table[i] = alloc_table[--alloc_count];
               break;
           }
       }
       free(ptr);
   }

   void check_memory_leaks(void)
   {
       if (alloc_count > 0) {
           LOG_WARN("Memory leaks detected: %zu allocations",
                    alloc_count);
           for (size_t i = 0; i < alloc_count; i++) {
               LOG_WARN("  %p (%zu bytes) at %s:%d",
                        alloc_table[i].ptr,
                        alloc_table[i].size,
                        alloc_table[i].file,
                        alloc_table[i].line);
           }
       }
   }

   #define malloc(size) debug_malloc(size, __FILE__, __LINE__)
   #define free(ptr) debug_free(ptr)

   #endif

Memory Corruption Detection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Guard Bytes:**

.. code-block:: c

   #define GUARD_PATTERN 0xDEADBEEF

   typedef struct {
       uint32_t guard_start;
       uint8_t data[256];
       uint32_t guard_end;
   } guarded_buffer_t;

   void init_guarded_buffer(guarded_buffer_t* buf)
   {
       buf->guard_start = GUARD_PATTERN;
       buf->guard_end = GUARD_PATTERN;
   }

   bool check_guarded_buffer(guarded_buffer_t* buf)
   {
       if (buf->guard_start != GUARD_PATTERN) {
           LOG_ERROR("Buffer underflow detected!");
           return false;
       }
       if (buf->guard_end != GUARD_PATTERN) {
           LOG_ERROR("Buffer overflow detected!");
           return false;
       }
       return true;
   }

**Memory Fill Patterns:**

.. code-block:: c

   /* Fill freed memory with pattern */
   void debug_free_with_pattern(void* ptr, size_t size)
   {
       memset(ptr, 0xDD, size);  /* 0xDD = "Dead" */
       free(ptr);
   }

   /* Detect use-after-free */
   void check_for_use_after_free(void* ptr, size_t size)
   {
       uint8_t* bytes = (uint8_t*)ptr;
       for (size_t i = 0; i < size; i++) {
           if (bytes[i] == 0xDD) {
               LOG_ERROR("Use-after-free detected at offset %zu", i);
               return;
           }
       }
   }

Best Practices
--------------

1. **Prefer Static Allocation**
   * Deterministic behavior
   * No fragmentation
   * Faster allocation
   * Suitable for safety-critical systems

2. **Use Memory Pools**
   * Fixed-size allocations
   * Fast allocation/deallocation
   * No fragmentation
   * Predictable behavior

3. **Minimize Dynamic Allocation**
   * Allocate at startup
   * Reuse buffers
   * Avoid frequent alloc/free
   * Use stack when possible

4. **Monitor Memory Usage**
   * Track stack high water marks
   * Monitor heap usage
   * Check for leaks
   * Set up alerts

5. **Handle Allocation Failures**
   * Always check return values
   * Have fallback strategies
   * Log failures
   * Graceful degradation

6. **Align Data Properly**
   * Use aligned allocations for DMA
   * Pack structures carefully
   * Consider cache line alignment
   * Use compiler attributes

7. **Test Memory Limits**
   * Test with low memory
   * Test allocation failures
   * Test fragmentation scenarios
   * Use memory debugging tools

Memory Optimization Checklist
------------------------------

**Design Phase:**

- [ ] Estimate memory requirements
- [ ] Choose allocation strategy
- [ ] Plan for worst-case usage
- [ ] Design for determinism

**Implementation Phase:**

- [ ] Use appropriate allocation method
- [ ] Check all allocation failures
- [ ] Free all allocated memory
- [ ] Avoid memory leaks

**Testing Phase:**

- [ ] Test with memory limits
- [ ] Monitor memory usage
- [ ] Check for leaks
- [ ] Verify stack sizes

**Optimization Phase:**

- [ ] Reduce dynamic allocation
- [ ] Optimize data structures
- [ ] Pack structures
- [ ] Use memory pools

See Also
--------

* :doc:`performance` - Performance Optimization
* :doc:`debugging` - Debugging Guide
* :doc:`best_practices` - Best Practices
* :doc:`../development/debugging_guide` - Development Debugging

