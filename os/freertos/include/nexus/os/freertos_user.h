/**
 * \file            freertos_user.h
 * \brief           Narrow pointer-free services for MPU-isolated user tasks
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_USER_H
#define NEXUS_OS_FREERTOS_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicitly reviewed unprivileged executable code section. */
#define NX_FREERTOS_USER_CODE __attribute__((section(".nexus_user_text")))
/** \brief Explicit user RAM, separate from kernel and provider state. */
#define NX_FREERTOS_USER_DATA __attribute__((section(".nexus_user_data")))
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
 *                  No object, memory, I/O or privilege-changing API exists.
 */
uint32_t nx_freertos_user_ticks(void) NX_FREERTOS_USER_CODE;

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_FREERTOS_USER_H */
