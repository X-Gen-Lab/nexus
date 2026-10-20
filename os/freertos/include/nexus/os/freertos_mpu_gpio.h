/**
 * \file            freertos_mpu_gpio.h
 * \brief           One protected-task scalar GPIO WRITE authority
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_MPU_GPIO_H
#define NEXUS_OS_FREERTOS_MPU_GPIO_H

#include "nexus/io/gpio.h"
#include "nexus/os/freertos_mpu.h"

#if !defined(NEXUS_OS_MPU_GPIO) || NEXUS_OS_MPU_GPIO != 1
#error "Scalar GPIO authority requires the selected U575 MPU-v2 profile"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Immutable WRITE grant in privileged Flash.
 * \note            port_slot addresses the readonly generated nx_binding_led
 *                  pointer. The initialized exact GPIOC PC7 face, operations
 *                  and provider state outlive an open authority through
 *                  successful join. The sole
 *                  lifecycle owner does not stop or reconfigure that GPIO.
 *                  No READ, toggle, pointer-copy or MMIO mapping is granted.
 */
typedef struct {
    const nx_gpio_port_t* const* port_slot;
    uint32_t device;
    uint32_t mask;
} nx_freertos_mpu_gpio_grant_t;

/**
 * \brief           Caller-owned protected authority; zero only at cold startup.
 * \note            Treat all fields as private. Join detaches the captured task
 *                  epoch, without resetting the bound task's epoch history.
 *                  Storage remains valid for the linked binding's
 *                  entire lifetime, including rejection and closed states.
 */
typedef struct {
    uint64_t task_epoch;
    TaskHandle_t handle;
    uint8_t phase;
} nx_freertos_mpu_gpio_t;

/**
 * \brief           One immutable assembly binding, in privileged Flash.
 * \note            Define exactly one nx_freertos_mpu_gpio_binding symbol.
 *                  Control and task are disjoint privileged RAM; grant is
 *                  privileged Flash. User arguments never select these objects.
 * \note            Keep the task epoch after deletion. Initialize only at cold
 *                  startup.
 */
typedef struct {
    nx_freertos_mpu_gpio_t* control;
    nx_freertos_mpu_task_t* task;
    const nx_freertos_mpu_gpio_grant_t* grant;
} nx_freertos_mpu_gpio_binding_t;

extern const nx_freertos_mpu_gpio_binding_t nx_freertos_mpu_gpio_binding;

/**
 * \brief           Open the fixed grant for its exact live protected task.
 * \return          SUCCESS or INVALID/CONTEXT/STATE/BUSY.
 *                  Rejection leaves no partial pin.
 * \note            Privileged Thread; scheduler running or not yet started.
 *                  BASEPRI/FAULTMASK must be zero, except the selected port's
 *                  bootstrap BASEPRI before scheduler start. PRIMASK is saved
 *                  and restored. This operation pins task deletion until join.
 */
nx_result_t nx_freertos_mpu_gpio_open(void) PRIVILEGED_FUNCTION;

/**
 * \brief           Revoke further calls without deleting or stopping the GPIO.
 * \return          SUCCESS, INVALID, CONTEXT or STATE; preserves all pins.
 * \note            The same privileged lifecycle owner serializes open/close/
 *                  join and task start/delete. NMI/HardFault never access this
 *                  authority. Incoming interrupt masks are preserved.
 */
nx_result_t nx_freertos_mpu_gpio_close(void) PRIVILEGED_FUNCTION;

/**
 * \brief           Join a closed authority before task deletion or GPIO stop.
 * \return          SUCCESS detaches the pin; INVALID/CONTEXT/STATE retain it.
 * \note            The single-core saved-mask boundary excludes every active
 *                  scalar effect. Join detaches its task pin before returning;
 *                  it does not delete the task or reset its lifetime epoch.
 */
nx_result_t nx_freertos_mpu_gpio_join(void) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_FREERTOS_MPU_GPIO_H */
