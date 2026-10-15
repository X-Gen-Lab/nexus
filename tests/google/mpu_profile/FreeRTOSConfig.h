/**
 * \file            FreeRTOSConfig.h
 * \brief           Host contract types from the real MPU wrapper-v2 port
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_MPU_CONFIG_H
#define NEXUS_TEST_MPU_CONFIG_H

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configCPU_CLOCK_HZ                      168000000u
#define configTICK_RATE_HZ                      1000u
#define configMAX_PRIORITIES                    8u
#define configMINIMAL_STACK_SIZE                128u
#define configMAX_TASK_NAME_LEN                 16u
#define configUSE_16_BIT_TICKS                  0
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configUSE_TASK_NOTIFICATIONS            0
#define configUSE_MUTEXES                       0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configENABLE_FPU                        0
#define configENABLE_TRUSTZONE                  0
#ifndef configRUN_FREERTOS_SECURE_ONLY
#define configRUN_FREERTOS_SECURE_ONLY 0
#endif
#define configENABLE_MPU                            1
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY 1
#define configUSE_MPU_WRAPPERS_V1                   0
#define configENABLE_ACCESS_CONTROL_LIST            0
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS  0
#define configTOTAL_MPU_REGIONS                     8
#define configSYSTEM_CALL_STACK_SIZE                128
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        0x50u
#define configKERNEL_INTERRUPT_PRIORITY             0xF0u
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_xTaskGetCurrentTaskHandle           1
#define INCLUDE_xTaskGetSchedulerState              1

#endif /* NEXUS_TEST_MPU_CONFIG_H */
