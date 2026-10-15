Profiling Guide
===============

Comprehensive guide to profiling and performance analysis for Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Profiling helps identify performance bottlenecks and optimize resource usage. This guide covers profiling tools, techniques, and analysis methods.

**Profiling Goals:**

* Identify CPU hotspots
* Measure execution time
* Analyze memory usage
* Monitor task behavior
* Optimize critical paths

**Profiling Types:**

* Time profiling
* Memory profiling
* Task profiling
* Interrupt profiling
* Power profiling

Time Profiling
--------------

Hardware Timer Profiling
~~~~~~~~~~~~~~~~~~~~~~~~~

**High-Resolution Timing:**

.. code-block:: c

   #include "hal/nx_timer.h"

   /* Initialize profiling timer */
   static nx_timer_t* profile_timer = NULL;

   void profiling_init(void)
   {
       profile_timer = nx_factory_timer(TIMER_PROFILE);
       if (profile_timer) {
           /* Configure for maximum resolution */
           nx_timer_config_t config = {
               .prescaler = 0,  /* No prescaling */
               .period = 0xFFFFFFFF,  /* Maximum period */
               .mode = TIMER_MODE_UP,
           };
           profile_timer->configure(profile_timer, &config);
           profile_timer->start(profile_timer);
       }
   }

   uint32_t profiling_get_cycles(void)
   {
       return profile_timer->get_counter(profile_timer);
   }

   uint32_t profiling_cycles_to_us(uint32_t cycles)
   {
       return cycles / (SystemCoreClock / 1000000);
   }

**Profiling Macros:**

.. code-block:: c

   #ifdef ENABLE_PROFILING

   #define PROFILE_START(name) \
       uint32_t __profile_##name##_start = profiling_get_cycles()

   #define PROFILE_END(name) \
       do { \
           uint32_t __cycles = profiling_get_cycles() - __profile_##name##_start; \
           uint32_t __us = profiling_cycles_to_us(__cycles); \
           LOG_INFO("PROFILE %s: %lu us (%lu cycles)", #name, __us, __cycles); \
       } while (0)

   #define PROFILE_FUNCTION() \
       PROFILE_START(__func__); \
       /* Function body */ \
       PROFILE_END(__func__)

   #else

   #define PROFILE_START(name)
   #define PROFILE_END(name)
   #define PROFILE_FUNCTION()

   #endif

**Usage Example:**

.. code-block:: c

   void process_data(const uint8_t* data, size_t len)
   {
       PROFILE_START(process_data);

       /* Data processing */
       for (size_t i = 0; i < len; i++) {
           process_byte(data[i]);
       }

       PROFILE_END(process_data);
   }

Statistical Profiling
~~~~~~~~~~~~~~~~~~~~~

**Collect Multiple Samples:**

.. code-block:: c

   typedef struct {
       const char* name;
       uint32_t min_cycles;
       uint32_t max_cycles;
       uint32_t total_cycles;
       uint32_t count;
   } profile_stats_t;

   #define MAX_PROFILES 32
   static profile_stats_t profiles[MAX_PROFILES];
   static size_t profile_count = 0;

   void profile_record(const char* name, uint32_t cycles)
   {
       /* Find or create profile entry */
       profile_stats_t* stats = NULL;
       for (size_t i = 0; i < profile_count; i++) {
           if (strcmp(profiles[i].name, name) == 0) {
               stats = &profiles[i];
               break;
           }
       }

       if (!stats && profile_count < MAX_PROFILES) {
           stats = &profiles[profile_count++];
           stats->name = name;
           stats->min_cycles = UINT32_MAX;
           stats->max_cycles = 0;
           stats->total_cycles = 0;
           stats->count = 0;
       }

       if (stats) {
           /* Update statistics */
           if (cycles < stats->min_cycles) {
               stats->min_cycles = cycles;
           }
           if (cycles > stats->max_cycles) {
               stats->max_cycles = cycles;
           }
           stats->total_cycles += cycles;
           stats->count++;
       }
   }

   void profile_print_stats(void)
   {
       LOG_INFO("Profile Statistics:");
       for (size_t i = 0; i < profile_count; i++) {
           profile_stats_t* s = &profiles[i];
           uint32_t avg = s->total_cycles / s->count;

           LOG_INFO("  %s:", s->name);
           LOG_INFO("    Count: %lu", s->count);
           LOG_INFO("    Min:   %lu us", profiling_cycles_to_us(s->min_cycles));
           LOG_INFO("    Max:   %lu us", profiling_cycles_to_us(s->max_cycles));
           LOG_INFO("    Avg:   %lu us", profiling_cycles_to_us(avg));
       }
   }

Task Profiling
--------------

FreeRTOS Runtime Stats
~~~~~~~~~~~~~~~~~~~~~~

**Enable Runtime Stats:**

.. code-block:: c

   /* FreeRTOSConfig.h */
   #define configGENERATE_RUN_TIME_STATS 1
   #define configUSE_TRACE_FACILITY 1
   #define configUSE_STATS_FORMATTING_FUNCTIONS 1

   /* Provide runtime counter */
   extern uint32_t profiling_get_runtime_counter(void);
   #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() profiling_init()
   #define portGET_RUN_TIME_COUNTER_VALUE() profiling_get_runtime_counter()

**Print Task Statistics:**

.. code-block:: c

   void print_task_stats(void)
   {
       char stats_buffer[512];

       /* Get task statistics */
       vTaskGetRunTimeStats(stats_buffer);

       LOG_INFO("Task Statistics:");
       LOG_INFO("%s", stats_buffer);

       /* Output format:
        * Task          Abs Time      % Time
        * ----------------------------------------
        * IDLE          12345678      50%
        * sensor_task   6172839       25%
        * network_task  6172839       25%
        */
   }

**Task State Information:**

.. code-block:: c

   void print_task_info(void)
   {
       TaskStatus_t* task_array;
       uint32_t total_runtime;
       uint32_t num_tasks;

       /* Get task count */
       num_tasks = uxTaskGetNumberOfTasks();

       /* Allocate array */
       task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       if (!task_array) {
           return;
       }

       /* Get task information */
       num_tasks = uxTaskGetSystemState(task_array, num_tasks, &total_runtime);

       LOG_INFO("Task Information:");
       for (uint32_t i = 0; i < num_tasks; i++) {
           TaskStatus_t* task = &task_array[i];

           LOG_INFO("  %s:", task->pcTaskName);
           LOG_INFO("    State: %d", task->eCurrentState);
           LOG_INFO("    Priority: %lu", task->uxCurrentPriority);
           LOG_INFO("    Stack HWM: %u", task->usStackHighWaterMark);
           LOG_INFO("    Runtime: %lu", task->ulRunTimeCounter);

           if (total_runtime > 0) {
               uint32_t percent = (task->ulRunTimeCounter * 100) / total_runtime;
               LOG_INFO("    CPU: %lu%%", percent);
           }
       }

       vPortFree(task_array);
   }

CPU Usage Monitoring
~~~~~~~~~~~~~~~~~~~~

**Calculate CPU Usage:**

.. code-block:: c

   static uint32_t last_idle_time = 0;
   static uint32_t last_total_time = 0;

   uint32_t get_cpu_usage_percent(void)
   {
       TaskStatus_t* tasks;
       uint32_t total_runtime;
       uint32_t num_tasks;
       uint32_t idle_time = 0;

       num_tasks = uxTaskGetNumberOfTasks();
       tasks = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       if (!tasks) {
           return 0;
       }

       num_tasks = uxTaskGetSystemState(tasks, num_tasks, &total_runtime);

       /* Find idle task runtime */
       for (uint32_t i = 0; i < num_tasks; i++) {
           if (strcmp(tasks[i].pcTaskName, "IDLE") == 0) {
               idle_time = tasks[i].ulRunTimeCounter;
               break;
           }
       }

       vPortFree(tasks);

       /* Calculate CPU usage since last call */
       uint32_t idle_delta = idle_time - last_idle_time;
       uint32_t total_delta = total_runtime - last_total_time;

       last_idle_time = idle_time;
       last_total_time = total_runtime;

       if (total_delta == 0) {
           return 0;
       }

       uint32_t cpu_usage = 100 - ((idle_delta * 100) / total_delta);
       return cpu_usage;
   }

Memory Profiling
----------------

Heap Usage Tracking
~~~~~~~~~~~~~~~~~~~

**Monitor Heap:**

.. code-block:: c

   typedef struct {
       size_t total_size;
       size_t free_size;
       size_t min_free_size;
       size_t alloc_count;
       size_t free_count;
   } heap_stats_t;

   void get_heap_stats(heap_stats_t* stats)
   {
       stats->total_size = configTOTAL_HEAP_SIZE;
       stats->free_size = xPortGetFreeHeapSize();
       stats->min_free_size = xPortGetMinimumEverFreeHeapSize();

       /* Track allocations if enabled */
   #ifdef TRACK_HEAP_ALLOCATIONS
       stats->alloc_count = heap_alloc_count;
       stats->free_count = heap_free_count;
   #endif
   }

   void print_heap_stats(void)
   {
       heap_stats_t stats;
       get_heap_stats(&stats);

       size_t used = stats.total_size - stats.free_size;
       uint32_t percent = (used * 100) / stats.total_size;

       LOG_INFO("Heap Statistics:");
       LOG_INFO("  Total: %zu bytes", stats.total_size);
       LOG_INFO("  Used: %zu bytes (%lu%%)", used, percent);
       LOG_INFO("  Free: %zu bytes", stats.free_size);
       LOG_INFO("  Min Free: %zu bytes", stats.min_free_size);

   #ifdef TRACK_HEAP_ALLOCATIONS
       LOG_INFO("  Allocations: %zu", stats.alloc_count);
       LOG_INFO("  Frees: %zu", stats.free_count);
       LOG_INFO("  Outstanding: %zu", stats.alloc_count - stats.free_count);
   #endif
   }

Stack Usage Analysis
~~~~~~~~~~~~~~~~~~~~

**Check All Task Stacks:**

.. code-block:: c

   void analyze_stack_usage(void)
   {
       TaskStatus_t* tasks;
       uint32_t num_tasks = uxTaskGetNumberOfTasks();

       tasks = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
       if (!tasks) {
           return;
       }

       num_tasks = uxTaskGetSystemState(tasks, num_tasks, NULL);

       LOG_INFO("Stack Usage Analysis:");
       for (uint32_t i = 0; i < num_tasks; i++) {
           TaskStatus_t* task = &tasks[i];
           uint32_t high_water = task->usStackHighWaterMark * sizeof(StackType_t);

           /* Estimate stack size (not directly available) */
           uint32_t stack_size = high_water * 2;  /* Rough estimate */
           uint32_t used = stack_size - high_water;
           uint32_t percent = (used * 100) / stack_size;

           LOG_INFO("  %s:", task->pcTaskName);
           LOG_INFO("    High Water: %lu bytes", high_water);
           LOG_INFO("    Usage: ~%lu%%", percent);

           if (percent > 80) {
               LOG_WARN("    WARNING: High stack usage!");
           }
       }

       vPortFree(tasks);
   }

Interrupt Profiling
-------------------

Interrupt Latency
~~~~~~~~~~~~~~~~~

**Measure Interrupt Response:**

.. code-block:: c

   static volatile uint32_t irq_trigger_time = 0;
   static volatile uint32_t irq_entry_time = 0;
   static volatile uint32_t irq_exit_time = 0;

   /* Set trigger time when event occurs */
   void trigger_interrupt(void)
   {
       irq_trigger_time = profiling_get_cycles();
       /* Trigger interrupt */
   }

   /* Measure in ISR */
   void EXTI0_IRQHandler(void)
   {
       irq_entry_time = profiling_get_cycles();

       /* Handle interrupt */
       handle_interrupt();

       irq_exit_time = profiling_get_cycles();

       /* Clear flag */
       EXTI->PR = EXTI_PR_PR0;
   }

   void print_irq_latency(void)
   {
       if (irq_exit_time > irq_trigger_time) {
           uint32_t latency = irq_entry_time - irq_trigger_time;
           uint32_t duration = irq_exit_time - irq_entry_time;

           LOG_INFO("Interrupt Latency: %lu us",
                    profiling_cycles_to_us(latency));
           LOG_INFO("Interrupt Duration: %lu us",
                    profiling_cycles_to_us(duration));
       }
   }

Interrupt Load
~~~~~~~~~~~~~~

**Track Interrupt Frequency:**

.. code-block:: c

   typedef struct {
       const char* name;
       uint32_t count;
       uint32_t total_cycles;
       uint32_t max_cycles;
   } irq_stats_t;

   #define MAX_IRQS 16
   static irq_stats_t irq_stats[MAX_IRQS];

   void irq_profile_enter(uint8_t irq_num)
   {
       if (irq_num < MAX_IRQS) {
           irq_stats[irq_num].count++;
       }
   }

   void irq_profile_exit(uint8_t irq_num, uint32_t cycles)
   {
       if (irq_num < MAX_IRQS) {
           irq_stats[irq_num].total_cycles += cycles;
           if (cycles > irq_stats[irq_num].max_cycles) {
               irq_stats[irq_num].max_cycles = cycles;
           }
       }
   }

   void print_irq_stats(void)
   {
       LOG_INFO("Interrupt Statistics:");
       for (uint8_t i = 0; i < MAX_IRQS; i++) {
           if (irq_stats[i].count > 0) {
               uint32_t avg = irq_stats[i].total_cycles / irq_stats[i].count;

               LOG_INFO("  IRQ %d:", i);
               LOG_INFO("    Count: %lu", irq_stats[i].count);
               LOG_INFO("    Avg: %lu us", profiling_cycles_to_us(avg));
               LOG_INFO("    Max: %lu us",
                        profiling_cycles_to_us(irq_stats[i].max_cycles));
           }
       }
   }

Profiling Tools
---------------

GDB Profiling
~~~~~~~~~~~~~

**Sample-Based Profiling:**

.. code-block:: text

   # Start program
   (gdb) run

   # Interrupt periodically and record PC
   (gdb) while 1
   > interrupt
   > x/i $pc
   > continue
   > end

   # Analyze which functions appear most often

**Automated Sampling:**

.. code-block:: python

   # profile_gdb.py
   import gdb
   import time
   from collections import Counter

   samples = Counter()

   for i in range(1000):
       gdb.execute("interrupt", to_string=True)
       frame = gdb.selected_frame()
       func = frame.name()
       if func:
           samples[func] += 1
       gdb.execute("continue", to_string=True)
       time.sleep(0.01)

   print("Profile Results:")
   for func, count in samples.most_common(10):
       print(f"  {func}: {count} samples")

SystemView (SEGGER)
~~~~~~~~~~~~~~~~~~~

**Enable SystemView:**

.. code-block:: c

   #include "SEGGER_SYSVIEW.h"

   void app_init(void)
   {
       /* Initialize SystemView */
       SEGGER_SYSVIEW_Conf();

       /* Application code */
   }

   /* Mark events */
   void process_data(void)
   {
       SEGGER_SYSVIEW_RecordEnterISR();
       /* Processing */
       SEGGER_SYSVIEW_RecordExitISR();
   }

Profiling Best Practices
-------------------------

1. **Profile Representative Workloads**
   * Use realistic data
   * Test typical scenarios
   * Include edge cases
   * Measure under load

2. **Minimize Profiling Overhead**
   * Use hardware timers
   * Limit logging
   * Sample strategically
   * Disable when not needed

3. **Focus on Hotspots**
   * Identify bottlenecks first
   * Optimize critical paths
   * Measure improvements
   * Iterate

4. **Consider Context**
   * Account for interrupts
   * Consider cache effects
   * Note system state
   * Document conditions

5. **Automate Profiling**
   * Script profiling runs
   * Collect statistics
   * Generate reports
   * Track over time

See Also
--------

* :doc:`performance` - Performance Optimization
* :doc:`debugging` - Debugging Guide
* :doc:`testing` - Testing Guide
* :doc:`memory_management` - Memory Management

