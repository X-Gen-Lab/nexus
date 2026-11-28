/**
 * \file            stm32f103_standby.h
 * \brief           Selected F103RB PA0 Standby and reset evidence.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F103_STANDBY_H
#define NEXUS_STM32F103_STANDBY_H

#include "nexus/components/clock_domain.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NX_STM32F103_STANDBY_WAKE_LATCHED = 1U,
    NX_STM32F103_STANDBY_ENTERED = 2U
};

/** \brief Independent RCC reset causes and sticky PWR evidence. */
typedef struct {
    uint32_t reset_causes;
    uint32_t standby_flags;
} nx_stm32f103_standby_boot_t;

/** \brief Independent result, cleanup, and evidence for a returned attempt. */
typedef struct {
    nx_result_t status;
    nx_result_t cleanup;
    uint32_t before_flags;
    uint32_t after_flags;
    bool wait_issued;
    bool platform_stopped;
} nx_stm32f1_standby_receipt_t;

/**
 * \brief Read boot flags without acknowledging them.
 * \param[out] output: Stable storage written only on complete success.
 * \return Success, or BUSY when PWR is clocked or held in interface reset.
 * \note Call after successful Platform start and before any wake-flag discard.
 *       SBF records Standby entry. WUF can clear on reset. Capture does not
 *       establish which wake source caused this reset.
 */
nx_result_t nx_stm32f103_standby_boot(nx_stm32f103_standby_boot_t* output);

/**
 * \brief Fully stop Platform and attempt terminal Standby via PA0.
 * \param[in] barrier: Closed and drained caller ClockDomain.
 * \param[in] discard_wuf: Permit clearing a previously observed WUF.
 * \note Privileged baremetal task with interrupt masks clear. Successful
 *       entry exits through reset and does not return. Any return after WFI
 *       is an error; if platform_stopped, reopen resources explicitly only
 *       after successful cleanup. Previous requests and time epochs do not
 *       carry across reset. SBF is preserved.
 */
nx_stm32f1_standby_receipt_t
nx_stm32f103_standby_enter(nx_clock_barrier_t barrier, bool discard_wuf);

#ifdef __cplusplus
}
#endif
#endif
