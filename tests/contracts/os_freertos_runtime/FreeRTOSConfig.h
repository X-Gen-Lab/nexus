/**
 * \file            FreeRTOSConfig.h
 * \brief           Test-only real FreeRTOS POSIX scheduler configuration
 * \author          Nexus Team
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION                         1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION      0
#define configCPU_CLOCK_HZ                           168000000u
#define configTICK_RATE_HZ                           1000u
#define configMAX_PRIORITIES                         8
#define configMINIMAL_STACK_SIZE                     4096u
#define configMAX_TASK_NAME_LEN                      16
#define configUSE_16_BIT_TICKS                       0
#define configIDLE_SHOULD_YIELD                      1
#define configUSE_TASK_NOTIFICATIONS                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES        1
#define configSUPPORT_STATIC_ALLOCATION              1
#define configKERNEL_PROVIDED_STATIC_MEMORY          1
#define configSUPPORT_DYNAMIC_ALLOCATION             0
#define configUSE_MUTEXES                            1
#define configUSE_RECURSIVE_MUTEXES                  0
#define configUSE_COUNTING_SEMAPHORES                1
#define configUSE_TIMERS                             0
#define configTIMER_TASK_STACK_DEPTH                 configMINIMAL_STACK_SIZE
#define configUSE_CO_ROUTINES                        0
#define configUSE_TRACE_FACILITY                     0
#define configUSE_STATS_FORMATTING_FUNCTIONS         0
#define configGENERATE_RUN_TIME_STATS                0
#define configCHECK_FOR_STACK_OVERFLOW               0
#define configUSE_IDLE_HOOK                          0
#define configUSE_TICK_HOOK                          0
#define configUSE_MALLOC_FAILED_HOOK                 0
#define configPRIO_BITS                              4u
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      ((1u << configPRIO_BITS) - 1u)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u
#define configKERNEL_INTERRUPT_PRIORITY                                        \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                   \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define vPortSVCHandler                     SVC_Handler
#define xPortPendSVHandler                  PendSV_Handler
#define xPortSysTickHandler                 SysTick_Handler

/**
 * \brief           Stop an invariant violation before continuing corrupted
 *                  state
 * \param[in]       file: Source file diagnostic, not transmitted automatically
 * \param[in]       line: Source line diagnostic
 * \note            Application may override weak fail-stop to record/reset.
 */
void nx_freertos_assert_failed(const char* file, unsigned line);
#define configASSERT(expression)                                               \
    do {                                                                       \
        if (!(expression)) {                                                   \
            nx_freertos_assert_failed(__FILE__, __LINE__);                     \
        }                                                                      \
    } while (0)
#endif /* FREERTOS_CONFIG_H */
