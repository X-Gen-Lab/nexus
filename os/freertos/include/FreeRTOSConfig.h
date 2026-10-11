/**
 * \file            FreeRTOSConfig.h
 * \brief           Maintained static-only Cortex-M kernel profiles
 * \author          Nexus Team
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include "nexus/os/tick.h"
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
#define configENABLE_FPU                      (NEXUS_CPU_HAS_FPU || NEXUS_CPU_HAS_MVE)
#define configENABLE_MVE                      NEXUS_CPU_HAS_MVE
#define configENABLE_MPU                      NEXUS_OS_MEMORY_PROTECTION
#define configENABLE_TRUSTZONE                NEXUS_OS_TRUSTZONE
#define configRUN_FREERTOS_SECURE_ONLY        NEXUS_CPU_SECURE_ONLY
#define configCPU_CLOCK_HZ                    NEXUS_CORE_HZ
#define configTICK_RATE_HZ                    NEXUS_OS_TICK_HZ
#define configMAX_PRIORITIES                  NEXUS_OS_MAX_PRIORITIES
#define configMINIMAL_STACK_SIZE              NEXUS_OS_IDLE_STACK_WORDS
#define configMAX_TASK_NAME_LEN               NEXUS_OS_MAX_TASK_NAME_LEN
#define configUSE_16_BIT_TICKS                0
#define configIDLE_SHOULD_YIELD               1
#define configUSE_TASK_NOTIFICATIONS          NEXUS_OS_TASK_NOTIFICATIONS
#define configTASK_NOTIFICATION_ARRAY_ENTRIES NEXUS_OS_NOTIFICATION_SLOTS
#define configSUPPORT_STATIC_ALLOCATION       1
/* MPU storage and split-world Idle identities are composed by the consumer. */
#define configKERNEL_PROVIDED_STATIC_MEMORY                                    \
    (!NEXUS_OS_MEMORY_PROTECTION && !NEXUS_OS_TRUSTZONE)
#define configSUPPORT_DYNAMIC_ALLOCATION             0
#define configUSE_MUTEXES                            NEXUS_OS_MUTEXES
#define configUSE_RECURSIVE_MUTEXES                  0
#define configUSE_COUNTING_SEMAPHORES                NEXUS_OS_COUNTING_SEMAPHORES
#define configUSE_TIMERS                             0
#define configTIMER_TASK_STACK_DEPTH                 configMINIMAL_STACK_SIZE
#define configUSE_CO_ROUTINES                        0
#define configUSE_TRACE_FACILITY                     NEXUS_OS_TRACE
#define configUSE_STATS_FORMATTING_FUNCTIONS         0
#define configGENERATE_RUN_TIME_STATS                NEXUS_OS_RUNTIME_STATS
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

#if NEXUS_OS_MEMORY_PROTECTION
#define configUSE_MPU_WRAPPERS_V1                   0
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY 1
#define configTOTAL_MPU_REGIONS                     NEXUS_CPU_MPU_REGIONS
#define configSYSTEM_CALL_STACK_SIZE                NEXUS_OS_SYSTEM_CALL_STACK_WORDS
#define configENABLE_ACCESS_CONTROL_LIST            0
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS  0
#endif

#if NEXUS_OS_TRUSTZONE
/* Requested capacity only: the Secure image supplies its actual sealed stack
 * and trusted immutable mapping for the caller-owned Nonsecure Idle TCB. */
#define configMINIMAL_SECURE_STACK_SIZE NEXUS_OS_SECURE_IDLE_STACK_BYTES
#endif

#if NEXUS_OS_TICKLESS
/* Custom suppression owns the sleep clock and Tick phase explicitly. The
 * vendor's SysTick-only suppression path is never silently selected. */
#define configUSE_TICKLESS_IDLE 2
#define portSUPPRESS_TICKS_AND_SLEEP(ticks)                                    \
    nx_freertos_suppress_ticks_and_sleep((uint32_t)(ticks))
#else
#define configUSE_TICKLESS_IDLE 0
#endif

#if NEXUS_OS_RUNTIME_STATS
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()                               \
    nx_freertos_runtime_counter_start()
#define portGET_RUN_TIME_COUNTER_VALUE() nx_freertos_runtime_counter_now()
#endif

#if NEXUS_OS_TRACE
#include "nexus/os/freertos_diagnostics.h"
#define traceTASK_SWITCHED_IN()                                                \
    nx_freertos_trace_event(NX_FREERTOS_TRACE_SWITCH_IN,                       \
                            (uintptr_t)pxCurrentTCB, 0u)
#define traceTASK_CREATE(task)                                                 \
    nx_freertos_trace_event(NX_FREERTOS_TRACE_CREATE, (uintptr_t)(task), 0u)
#define traceTASK_DELETE(task)                                                 \
    nx_freertos_trace_event(NX_FREERTOS_TRACE_DELETE, (uintptr_t)(task), 0u)
#endif

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
