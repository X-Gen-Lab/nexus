/**
 * \file            guard.h
 * \brief           Single-use privileged scheduler-start SVC admission
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_MPU_GUARD_H
#define NEXUS_FREERTOS_MPU_GUARD_H
#include "FreeRTOS.h"
#include <stdint.h>

extern uint8_t __nexus_user_flash_start__[];
extern uint8_t __nexus_user_flash_end__[];
extern uint8_t __nexus_syscall_flash_start__[];
extern uint8_t __nexus_syscall_flash_end__[];

#if configENABLE_MPU != 1 || configUSE_MPU_WRAPPERS_V1 != 0
#error "Scheduler-start guard requires the maintained MPU wrapper-v2 port"
#endif

/** \brief One kernel bootstrap lease; zero is cold, two is permanently used. */
typedef struct {
    uint32_t phase;
} nx_freertos_mpu_start_t;

/** \brief Actual CPU registers and exact external privileged reservations. */
typedef struct {
    uint32_t control;
    uint32_t exception;
    uintptr_t startup_pc;
    uintptr_t flash_begin;
    uintptr_t flash_end;
    uintptr_t ram_begin;
    uintptr_t ram_end;
} nx_freertos_mpu_start_facts_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Read strong linker labels and raw CONTROL/IPSR without mutation. */
nx_freertos_mpu_start_facts_t
nx_freertos_mpu_start_facts(void) PRIVILEGED_FUNCTION;
/** \brief Arm once before the real port performs any scheduler-start writes. */
BaseType_t
nx_freertos_mpu_start_arm(nx_freertos_mpu_start_t* start) PRIVILEGED_FUNCTION;
/** \brief Consume only a real privileged Thread/MSP bootstrap SVC frame. */
BaseType_t
nx_freertos_mpu_start_consume(nx_freertos_mpu_start_t* start,
                              const uint32_t* frame,
                              uint32_t exception_return) PRIVILEGED_FUNCTION;
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_FREERTOS_MPU_GUARD_H */
