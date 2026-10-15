/**
 * \file            private.h
 * \brief           MPU linker and hardware boundary for privileged validation
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_MPU_PRIVATE_H
#define NEXUS_FREERTOS_MPU_PRIVATE_H

#include "nexus/os/freertos_mpu.h"

/** \brief Half-open linker-owned range, never widened to fit a user request. */
typedef struct {
    uintptr_t begin;
    uintptr_t end;
} nx_freertos_mpu_span_t;

/** \brief Real linker regions and independently observed MPU facts. */
typedef struct {
    nx_freertos_mpu_span_t privileged_flash;
    nx_freertos_mpu_span_t privileged_ram;
    nx_freertos_mpu_span_t user_flash;
    nx_freertos_mpu_span_t user_ram;
    nx_freertos_mpu_span_t syscall_flash;
    uint8_t version;
    uint8_t regions;
} nx_freertos_mpu_layout_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Read actual external linker bounds and MPU_TYPE without mutation. */
nx_result_t
nx_freertos_mpu_layout(nx_freertos_mpu_layout_t* layout) PRIVILEGED_FUNCTION;
#ifdef __cplusplus
}
#endif

#endif /* NEXUS_FREERTOS_MPU_PRIVATE_H */
