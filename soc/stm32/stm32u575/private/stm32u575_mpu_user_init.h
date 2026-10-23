/**
 * \file            stm32u575_mpu_user_init.h
 * \brief           Exact protected-image user RAM cold initialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32U575_MPU_USER_INIT_H
#define NEXUS_STM32U575_MPU_USER_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One exact half-open linker reservation, never a runtime mapping. */
typedef struct {
    uintptr_t begin;
    uintptr_t end;
} nx_stm32u575_mpu_user_span_t;

/**
 * \brief           Trusted reset-stack descriptor; no mutable global inputs.
 * \note            The complete descriptor lies in privileged RAM. All five
 *                  reservations are nonempty, disjoint 32-byte granules.
 *                  User data source is privileged Flash; destination data and
 *                  BSS are separate user RAM spans, including their padding.
 *                  Empty data/BSS spans are valid at reservation boundaries.
 */
typedef struct {
    nx_stm32u575_mpu_user_span_t privileged_flash;
    nx_stm32u575_mpu_user_span_t syscall_flash;
    nx_stm32u575_mpu_user_span_t user_flash;
    nx_stm32u575_mpu_user_span_t privileged_ram;
    nx_stm32u575_mpu_user_span_t user_ram;
    uintptr_t data_load;
    uintptr_t data_start;
    uintptr_t data_end;
    uintptr_t bss_start;
    uintptr_t bss_end;
} nx_stm32u575_mpu_user_plan_t;

/**
 * \brief           Validate all bounds before any user RAM copy or zero write.
 * \note            Only the trusted cold reset path calls this helper, before
 *                  user execution, MPU setup, primary RAM or constructors.
 *                  Rejection changes no memory. The descriptor itself must be
 *                  a valid readable aligned object before entry.
 */
bool nx_stm32u575_mpu_user_initialize(const nx_stm32u575_mpu_user_plan_t* plan);

/** \brief Construct the trusted plan from strong linker bounds on reset MSP. */
bool nx_stm32u575_mpu_user_boot(void);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32U575_MPU_USER_INIT_H */
