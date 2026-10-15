/**
 * \file            features.h
 *
 * \brief           Reviewed CPU mechanisms and allocation-free result types.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_FEATURES_H
#define NEXUS_ARCH_FEATURES_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Result of a validated local CPU mechanism operation. */
typedef enum {
    NX_ARCH_OK = 0,
    NX_ARCH_INVALID,
    NX_ARCH_CONTEXT,
    NX_ARCH_UNSUPPORTED
} nx_arch_result_t;
/** \brief Security state compiled for this firmware image. */
typedef enum {
    NX_ARCH_SECURITY_SINGLE = 0,
    NX_ARCH_SECURITY_SECURE = 1,
    NX_ARCH_SECURITY_NONSECURE = 2
} nx_arch_security_state_t;
/**
 * \brief           Reviewed mechanisms in the exact compiled CPU profile.
 *
 * \note            Cache sizes are zero when absent, MPU version is zero,
 *                  seven or eight. These facts do not prove enabled hardware,
 *                  memory reachability, cross-domain ownership or HIL status.
 */
typedef struct {
    uint32_t dcache_line_bytes;
    uint32_t icache_line_bytes;
    uint8_t mpu_version;
    bool dwt_cycle_counter;
    bool sau;
    nx_arch_security_state_t security_state;
} nx_arch_features_t;
/**
 * \brief           Query immutable, compiled CPU mechanism capabilities.
 *
 * \return          Exact reviewed CPU facts; Native reports absent hardware.
 *
 * \note            Read-only Task/IRQ query, no hardware access or discovery.
 */
nx_arch_features_t nx_arch_features(void);
#ifdef __cplusplus
}
#endif
#endif
