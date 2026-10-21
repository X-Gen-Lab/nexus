/**
 * \file            syscalls.c
 * \brief           Pointer-free MPU v2 services without kernel-object pools
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "mpu_syscall_numbers.h"
#include "nexus/arch/arch.h"
#include "nexus/os/freertos_user.h"
#include "task.h"
#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
#include "private.h"
#if NUM_SYSTEM_CALLS != 71 || SYSTEM_CALL_NexusGpioWrite != 70
#error "Selected GPIO service requires the exact derived syscall ABI"
#endif
nx_result_t MPU_NexusGpioWrite(uint32_t device, uint32_t set,
                               uint32_t reset) FREERTOS_SYSTEM_CALL;
nx_result_t MPU_NexusGpioWriteImpl(uint32_t device, uint32_t set,
                                   uint32_t reset) PRIVILEGED_FUNCTION;

nx_result_t MPU_NexusGpioWriteImpl(uint32_t device, uint32_t set,
                                   uint32_t reset) {
    return nx_freertos_mpu_gpio_write(device, set, reset);
}
#endif

#if configUSE_MPU_WRAPPERS_V1 != 0 || configENABLE_MPU != 1
#error "Narrow user services require the real MPU wrapper-v2 port"
#endif
#if INCLUDE_vTaskDelay != 1 ||                                                 \
    configTICK_TYPE_WIDTH_IN_BITS != TICK_TYPE_WIDTH_32_BITS
#error "Narrow user services require delay and 32-bit kernel ticks"
#endif

/* The linked veneers are the exact two ABI-compatible pinned port bodies.
 * Keep their declarations narrow; the vendor omnibus header exposes objects. */
void MPU_vTaskDelay(TickType_t ticks) FREERTOS_SYSTEM_CALL;
TickType_t MPU_xTaskGetTickCount(void) FREERTOS_SYSTEM_CALL;

void MPU_vTaskDelayImpl(TickType_t ticks) PRIVILEGED_FUNCTION;
TickType_t MPU_xTaskGetTickCountImpl(void) PRIVILEGED_FUNCTION;

void MPU_vTaskDelayImpl(TickType_t ticks) {
    configASSERT(!nx_arch_in_isr());
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    vTaskDelay(ticks);
}

TickType_t MPU_xTaskGetTickCountImpl(void) {
    configASSERT(!nx_arch_in_isr());
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    return xTaskGetTickCount();
}

/* The real port validates the SVC origin and enters its protected syscall
 * stack. Every other syscall slot is zero and retains no object translation
 * table, privileged handle, pointer-copy policy or global object capacity. */
PRIVILEGED_DATA UBaseType_t uxSystemCallImplementations[NUM_SYSTEM_CALLS] = {
    [SYSTEM_CALL_vTaskDelay] = (UBaseType_t)MPU_vTaskDelayImpl,
    [SYSTEM_CALL_xTaskGetTickCount] = (UBaseType_t)MPU_xTaskGetTickCountImpl,
#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
    [SYSTEM_CALL_NexusGpioWrite] = (UBaseType_t)MPU_NexusGpioWriteImpl,
#endif
};

void nx_freertos_user_delay(uint32_t ticks) {
    MPU_vTaskDelay((TickType_t)ticks);
}

uint32_t nx_freertos_user_ticks(void) {
    return (uint32_t)MPU_xTaskGetTickCount();
}

#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
nx_result_t nx_freertos_user_gpio_write(uint32_t device, uint32_t set_mask,
                                        uint32_t reset_mask) {
    return MPU_NexusGpioWrite(device, set_mask, reset_mask);
}
#endif
