Performance Optimization
========================

Guide to optimizing performance in the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Performance optimization in embedded systems requires balancing:

* **Speed**: Execution time and throughput
* **Memory**: RAM and flash usage
* **Power**: Energy consumption
* **Code Size**: Flash footprint

This guide covers optimization techniques for the Nexus platform.

Profiling and Measurement
--------------------------

Timing Measurement
~~~~~~~~~~~~~~~~~~

**Cycle Counter**

.. code-block:: c

   /* Enable DWT cycle counter */
   CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
   DWT->CYCCNT = 0;
   DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

   /* Measure execution time */
   uint32_t start = DWT->CYCCNT;
   function_to_measure();
   uint32_t cycles = DWT->CYCCNT - start;
   uint32_t microseconds = cycles / (SystemCoreClock / 1000000);

**Timing Macros**

.. code-block:: c

   #define TIMING_START() \
       uint32_t _timing_start = DWT->CYCCNT

   #define TIMING_END(name) \
       do { \
           uint32_t _cycles = DWT->CYCCNT - _timing_start; \
           LOG_INFO("%s: %u cycles", name, _cycles); \
       } while(0)

   /* Usage */
   TIMING_START();
   process_data();
   TIMING_END("process_data");

Memory Profiling
~~~~~~~~~~~~~~~~

**Stack Usage**

.. code-block:: c

   /* Fill stack with pattern */
   extern uint32_t _sstack;
   extern uint32_t _estack;

   void stack_fill(void) {
       uint32_t* p = &_sstack;
       while (p < &_estack) {
           *p++ = 0xDEADBEEF;
       }
   }

   /* Check stack usage */
   uint32_t stack_get_usage(void) {
       uint32_t* p = &_sstack;
       uint32_t count = 0;
       while (p < &_estack && *p == 0xDEADBEEF) {
           p++;
           count++;
       }
       return (&_estack - &_sstack - count) * sizeof(uint32_t);
   }

**Heap Usage**

.. code-block:: c

   /* Track heap allocations */
   static size_t heap_allocated = 0;
   static size_t heap_peak = 0;

   void* tracked_malloc(size_t size) {
       void* ptr = malloc(size);
       if (ptr) {
           heap_allocated += size;
           if (heap_allocated > heap_peak) {
               heap_peak = heap_allocated;
           }
       }
       return ptr;
   }

   void tracked_free(void* ptr, size_t size) {
       free(ptr);
       heap_allocated -= size;
   }

Code Optimization
-----------------

Compiler Optimization
~~~~~~~~~~~~~~~~~~~~~

**Optimization Levels**

.. code-block:: cmake

   # Debug build - no optimization
   set(CMAKE_C_FLAGS_DEBUG "-O0 -g")

   # Release build - optimize for speed
   set(CMAKE_C_FLAGS_RELEASE "-O2")

   # Size-optimized build
   set(CMAKE_C_FLAGS_MINSIZEREL "-Os")

   # Maximum optimization
   set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g")

**Function Inlining**

.. code-block:: c

   /* Force inline for small, frequently called functions */
   static inline __attribute__((always_inline))
   uint32_t fast_multiply(uint32_t a, uint32_t b) {
       return a * b;
   }

   /* Prevent inlining for large functions */
   __attribute__((noinline))
   void large_function(void) {
       /* Large function body */
   }

Algorithm Optimization
~~~~~~~~~~~~~~~~~~~~~~

**Use Efficient Algorithms**

.. code-block:: c

   /* Bad: O(n²) search */
   bool find_duplicate_slow(int* array, size_t length) {
       for (size_t i = 0; i < length; i++) {
           for (size_t j = i + 1; j < length; j++) {
               if (array[i] == array[j]) {
                   return true;
               }
           }
       }
       return false;
   }

   /* Good: O(n) with hash set */
   bool find_duplicate_fast(int* array, size_t length) {
       hash_set_t* seen = hash_set_create();
       for (size_t i = 0; i < length; i++) {
           if (hash_set_contains(seen, array[i])) {
               hash_set_destroy(seen);
               return true;
           }
           hash_set_add(seen, array[i]);
       }
       hash_set_destroy(seen);
       return false;
   }

**Loop Optimization**

.. code-block:: c

   /* Bad: Function call in loop condition */
   for (size_t i = 0; i < strlen(str); i++) {
       process(str[i]);
   }

   /* Good: Cache length */
   size_t len = strlen(str);
   for (size_t i = 0; i < len; i++) {
       process(str[i]);
   }

   /* Better: Loop unrolling for small fixed sizes */
   #define PROCESS_4(base) \
       process(str[base]); \
       process(str[base+1]); \
       process(str[base+2]); \
       process(str[base+3])

Memory Optimization
-------------------

Reduce Memory Usage
~~~~~~~~~~~~~~~~~~~

**Use Bit Fields**

.. code-block:: c

   /* Bad: Wastes memory */
   typedef struct {
       bool flag1;     /* 1 byte */
       bool flag2;     /* 1 byte */
       bool flag3;     /* 1 byte */
       bool flag4;     /* 1 byte */
   } flags_wasteful_t;  /* 4 bytes total */

   /* Good: Pack into single byte */
   typedef struct {
       uint8_t flag1 : 1;
       uint8_t flag2 : 1;
       uint8_t flag3 : 1;
       uint8_t flag4 : 1;
       uint8_t reserved : 4;
   } flags_packed_t;  /* 1 byte total */

**Use Const for Read-Only Data**

.. code-block:: c

   /* Bad: Uses RAM */
   static uint8_t lookup_table[256] = { /* ... */ };

   /* Good: Uses flash */
   static const uint8_t lookup_table[256] = { /* ... */ };

**Memory Pools**

.. code-block:: c

   /* Fixed-size memory pool */
   #define POOL_SIZE 10
   #define BLOCK_SIZE 64

   static uint8_t memory_pool[POOL_SIZE][BLOCK_SIZE];
   static bool pool_used[POOL_SIZE];

   void* pool_alloc(void) {
       for (size_t i = 0; i < POOL_SIZE; i++) {
           if (!pool_used[i]) {
               pool_used[i] = true;
               return memory_pool[i];
           }
       }
       return NULL;
   }

   void pool_free(void* ptr) {
       for (size_t i = 0; i < POOL_SIZE; i++) {
           if (memory_pool[i] == ptr) {
               pool_used[i] = false;
               return;
           }
       }
   }

DMA Optimization
----------------

Use DMA for Bulk Transfers
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /* Bad: CPU-based transfer */
   void uart_send_slow(const uint8_t* data, size_t length) {
       for (size_t i = 0; i < length; i++) {
           while (!(UART->SR & UART_SR_TXE));
           UART->DR = data[i];
       }
   }

   /* Good: DMA transfer */
   void uart_send_fast(const uint8_t* data, size_t length) {
       /* Configure DMA */
       DMA_Stream->PAR = (uint32_t)&UART->DR;
       DMA_Stream->M0AR = (uint32_t)data;
       DMA_Stream->NDTR = length;
       DMA_Stream->CR |= DMA_SxCR_EN;

       /* CPU is free to do other work */
   }

Cache Optimization
------------------

Data Cache
~~~~~~~~~~

.. code-block:: c

   /* Align DMA buffers to cache line */
   #define CACHE_LINE_SIZE 32

   __attribute__((aligned(CACHE_LINE_SIZE)))
   static uint8_t dma_buffer[256];

   /* Clean cache before DMA TX */
   void dma_tx_prepare(void* buffer, size_t size) {
       SCB_CleanDCache_by_Addr(buffer, size);
   }

   /* Invalidate cache after DMA RX */
   void dma_rx_complete(void* buffer, size_t size) {
       SCB_InvalidateDCache_by_Addr(buffer, size);
   }

Instruction Cache
~~~~~~~~~~~~~~~~~

.. code-block:: c

   /* Enable instruction cache */
   void enable_icache(void) {
       SCB_EnableICache();
   }

   /* Place frequently called functions in fast memory */
   __attribute__((section(".fast_code")))
   void critical_function(void) {
       /* Time-critical code */
   }

Power Optimization
------------------

Sleep Modes
~~~~~~~~~~~

.. code-block:: c

   /* Enter sleep mode when idle */
   void idle_task(void) {
       while (1) {
           __WFI();  /* Wait for interrupt */
       }
   }

   /* Deep sleep mode */
   void enter_deep_sleep(void) {
       SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
       __WFI();
   }

Clock Gating
~~~~~~~~~~~~

.. code-block:: c

   /* Disable unused peripheral clocks */
   void optimize_clocks(void) {
       /* Disable unused peripherals */
       RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOCEN |
                         RCC_AHB1ENR_GPIODEN);

       /* Enable only when needed */
       if (need_spi) {
           RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
       }
   }

See Also
--------

* :doc:`debugging_guide` - Profiling and debugging
* :doc:`testing` - Performance testing
* :doc:`architecture_design` - System architecture

Summary
-------

Key optimization techniques:

* **Profiling**: Measure before optimizing
* **Algorithms**: Use efficient algorithms
* **Memory**: Minimize RAM/flash usage
* **DMA**: Offload bulk transfers
* **Cache**: Optimize cache usage
* **Power**: Use sleep modes and clock gating

Always measure the impact of optimizations to ensure they provide real benefits.
