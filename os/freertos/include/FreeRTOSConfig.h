/**
 * \file            FreeRTOSConfig.h
 * \brief           Maintained static-only Cortex-M kernel profiles
 * \author          Nexus Team
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include "nexus_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION NEXUS_CPU_HAS_BASEPRI
/* MVE and scalar FP share the coprocessor register bank and extended exception
 * frame. This kernel switch enables its context/lazy-frame initialization; CPU
 * facts and +nofp compiler flags still distinguish integer-only MVE from FP. */
#define configENABLE_FPU                             (NEXUS_CPU_HAS_FPU || NEXUS_CPU_HAS_MVE)
#define configENABLE_MVE                             NEXUS_CPU_HAS_MVE
#define configENABLE_MPU                             0
#define configENABLE_TRUSTZONE                       0
#define configRUN_FREERTOS_SECURE_ONLY               NEXUS_CPU_SECURE_ONLY
#define configCPU_CLOCK_HZ                           NEXUS_CORE_HZ
#define configTICK_RATE_HZ                           1000u
#define configMAX_PRIORITIES                         8
#define configMINIMAL_STACK_SIZE                     128u
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
#define configCHECK_FOR_STACK_OVERFLOW               2
#define configUSE_IDLE_HOOK                          0
#define configUSE_TICK_HOOK                          0
#define configUSE_MALLOC_FAILED_HOOK                 0
#define configPRIO_BITS                              NEXUS_IRQ_PRIORITY_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      ((1u << configPRIO_BITS) - 1u)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY NEXUS_IRQ_SYSCALL_PRIORITY
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

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
