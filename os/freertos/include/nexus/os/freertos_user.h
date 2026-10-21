/**
 * \file            freertos_user.h
 * \brief           Narrow pointer-free services for MPU-isolated user tasks
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_USER_H
#define NEXUS_OS_FREERTOS_USER_H

#include <stdint.h>
#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
#include "nexus/core/status.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicitly reviewed unprivileged executable code section. */
#define NX_FREERTOS_USER_CODE __attribute__((section(".nexus_user_text")))
/** \brief Explicit user RAM, separate from kernel and provider state. */
#define NX_FREERTOS_USER_DATA __attribute__((section(".nexus_user_data")))
/** \brief Explicit zero-initialized user storage in its reserved RAM span. */
#define NX_FREERTOS_USER_BSS __attribute__((section(".nexus_user_bss")))
/** \brief Trusted static task metadata placed in protected kernel RAM. */
#define NX_FREERTOS_PRIVILEGED_DATA                                            \
    __attribute__((section("privileged_data"), aligned(8)))

/**
 * \brief           Delay the calling user task through the real MPU SVC.
 * \param[in]       ticks: Relative kernel ticks; zero requests rescheduling.
 * \note            Thread only with a running scheduler. No handle or pointer
 *                  crosses the boundary. This is not a microsecond deadline.
 */
void nx_freertos_user_delay(uint32_t ticks) NX_FREERTOS_USER_CODE;

/**
 * \brief           Read the kernel's wrapping 32-bit tick counter through SVC.
 * \return          Current kernel tick count in the selected OS clock domain.
 * \note            Thread-only MPU profile service, distinct from Core time.
 *                  No object handle or pointer crosses this service.
 */
uint32_t nx_freertos_user_ticks(void) NX_FREERTOS_USER_CODE;

#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
/**
 * \brief           Write the current protected task's one static GPIO grant.
 * \param[in]       device: Exact assembly-local grant identifier, no pointer.
 * \param[in]       set_mask: Granted bits to set, disjoint from reset_mask.
 * \param[in]       reset_mask: Granted bits to reset.
 * \return          Scalar SUCCESS or INVALID/PERMISSION/STATE/CONTEXT.
 * \note            Thread only with a running scheduler. Unknown identifiers
 *                  and out-of-grant bits return PERMISSION; overlapping or
 *                  zero masks return INVALID. No user MMIO mapping, READ,
 *                  toggle, OS wait, heap or pointer borrow is introduced.
 *                  A closed or stale task incarnation performs no GPIO write.
 */
nx_result_t
nx_freertos_user_gpio_write(uint32_t device, uint32_t set_mask,
                            uint32_t reset_mask) NX_FREERTOS_USER_CODE;
#endif

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_FREERTOS_USER_H */
