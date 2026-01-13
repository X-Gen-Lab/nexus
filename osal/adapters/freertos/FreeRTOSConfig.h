/**
 * \file            FreeRTOSConfig.h
 * \brief           FreeRTOS configuration for Nexus Platform
 *
 * \details         This is the default FreeRTOS configuration template for
 * STM32F4 platform. Platform-specific configurations can override this by
 * placing a FreeRTOSConfig.h in the platforms/[platform]/ directory.
 *
 * \note            This configuration is optimized for STM32F4 running at
 * 168MHz
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *----------------------------------------------------------*/

/*-----------------------------------------------------------
 * Scheduler Configuration
 *----------------------------------------------------------*/

/* Set to 1 to use preemptive scheduling, 0 for cooperative */
#define configUSE_PREEMPTION 1

/* Use port optimized task selection for Cortex-M */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

/* Tickless idle mode (0 = disabled, 1 = enabled) */
#define configUSE_TICKLESS_IDLE 0

/* CPU clock frequency in Hz (STM32F4 @ 168MHz) */
#define configCPU_CLOCK_HZ 168000000UL

/* Tick rate in Hz (1000 = 1ms tick) */
#define configTICK_RATE_HZ 1000

/* Maximum number of priorities (OSAL uses 0-31, so 32 priorities) */
#define configMAX_PRIORITIES 32

/* Minimum stack size in words (128 words = 512 bytes for Cortex-M) */
#define configMINIMAL_STACK_SIZE 128

/* Maximum task name length */
#define configMAX_TASK_NAME_LEN 16

/* Use 32-bit tick type for longer timeout support */
#define configUSE_16_BIT_TICKS 0

/* Idle task should yield when another task at idle priority is ready */
#define configIDLE_SHOULD_YIELD 1

/* Enable task notifications (efficient alternative to semaphores) */
#define configUSE_TASK_NOTIFICATIONS          1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 3

/*-----------------------------------------------------------
 * Memory Allocation Configuration
 *----------------------------------------------------------*/

/* Static allocation support (0 = disabled, 1 = enabled) */
#define configSUPPORT_STATIC_ALLOCATION 0

/* Dynamic allocation support (required for OSAL) */
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* Total heap size (32KB for STM32F4 with 128KB RAM) */
#define configTOTAL_HEAP_SIZE (32 * 1024)

/* Use application-provided heap (0 = use FreeRTOS heap) */
#define configAPPLICATION_ALLOCATED_HEAP 0

/* Stack overflow checking (0 = none, 1 = method 1, 2 = method 2) */
#define configCHECK_FOR_STACK_OVERFLOW 2

/*-----------------------------------------------------------
 * Hook Function Configuration
 *----------------------------------------------------------*/

/* Idle hook function */
#define configUSE_IDLE_HOOK 0

/* Tick hook function */
#define configUSE_TICK_HOOK 0

/* Malloc failed hook function */
#define configUSE_MALLOC_FAILED_HOOK 1

/* Daemon task startup hook */
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0

/*-----------------------------------------------------------
 * Synchronization Primitives Configuration
 *----------------------------------------------------------*/

/* Mutex support (required for OSAL) */
#define configUSE_MUTEXES 1

/* Recursive mutex support */
#define configUSE_RECURSIVE_MUTEXES 1

/* Counting semaphore support (required for OSAL) */
#define configUSE_COUNTING_SEMAPHORES 1

/* Queue sets (not required by OSAL) */
#define configUSE_QUEUE_SETS 0

/* Queue registry size for debugging */
#define configQUEUE_REGISTRY_SIZE 8

/*-----------------------------------------------------------
 * Software Timer Configuration
 *----------------------------------------------------------*/

/* Software timer support */
#define configUSE_TIMERS 1

/* Timer task priority (highest priority) */
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)

/* Timer command queue length */
#define configTIMER_QUEUE_LENGTH 10

/* Timer task stack depth */
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE

/*-----------------------------------------------------------
 * Co-routine Configuration (not used)
 *----------------------------------------------------------*/

#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 2

/*-----------------------------------------------------------
 * Interrupt Nesting Configuration (Cortex-M specific)
 *----------------------------------------------------------*/

/* Cortex-M specific: priority bits */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS __NVIC_PRIO_BITS
#else
#define configPRIO_BITS 4
#endif

/* Lowest interrupt priority (highest numerical value) */
#define configKERNEL_INTERRUPT_PRIORITY (15 << (8 - configPRIO_BITS))

/* Maximum syscall interrupt priority */
/* Interrupts with priority 0-4 cannot call FreeRTOS API */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))

/* Library versions for compatibility */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/*-----------------------------------------------------------
 * Debug and Trace Configuration
 *----------------------------------------------------------*/

/* Trace facility for debugging */
#define configUSE_TRACE_FACILITY 0

/* Stats formatting functions */
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

/* Generate run time stats */
#define configGENERATE_RUN_TIME_STATS 0

/*-----------------------------------------------------------
 * Optional Function Includes
 *----------------------------------------------------------*/

#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xResumeFromISR              1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle      0
#define INCLUDE_eTaskGetState               1
#define INCLUDE_xEventGroupSetBitFromISR    1
#define INCLUDE_xTimerPendFunctionCall      1
#define INCLUDE_xTaskAbortDelay             0
#define INCLUDE_xTaskGetHandle              0
#define INCLUDE_xTaskResumeFromISR          1
#define INCLUDE_pcTaskGetTaskName           1

/*-----------------------------------------------------------
 * Cortex-M Specific Definitions
 *----------------------------------------------------------*/

/* Definitions for Cortex-M interrupt handlers */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/*-----------------------------------------------------------
 * Assert Configuration
 *----------------------------------------------------------*/

/* Assert macro for debugging */
#ifdef DEBUG
extern void vAssertCalled(const char* file, int line);
#define configASSERT(x)                                                        \
    if ((x) == 0)                                                              \
    vAssertCalled(__FILE__, __LINE__)
#else
#define configASSERT(x)                                                        \
    if ((x) == 0) {                                                            \
        taskDISABLE_INTERRUPTS();                                              \
        for (;;)                                                               \
            ;                                                                  \
    }
#endif

/*-----------------------------------------------------------
 * FPU Configuration (Cortex-M4F)
 *----------------------------------------------------------*/

/* Enable FPU support */
#define configENABLE_FPU 1

/* MPU support (disabled by default) */
#define configENABLE_MPU 0

/* TrustZone support (disabled) */
#define configENABLE_TRUSTZONE 0

#endif /* FREERTOS_CONFIG_H */
