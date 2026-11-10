/**
 * \file            stm32f407_wwdg_ewi.h
 * \brief           Caller-owned sticky WWDG early-warning observation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F407_WWDG_EWI_H
#define NEXUS_STM32F407_WWDG_EWI_H
#include "nexus/io/watchdog.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Zero-initialized optional storage, retained through hardware reset.
 * \note Fields are provider-owned after arm, never copied or reset by caller.
 */
typedef struct {
    const nx_watchdog_port_t* watchdog;
    volatile uint32_t occurrences;
    bool retained;
    bool overflow;
} nx_stm32f407_wwdg_ewi_t;

/**
 * \brief Enable irreversible EWI after explicit watchdog activation.
 * \param[in,out] warning: Distinct static zero storage, retained on effects.
 * \param[in] watchdog: Stable prepared/enabled F407 window-watchdog face.
 * \param[in] priority: Logical WWDG IRQ priority, 0..15.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY/IO; retained reports effects.
 * \note Serialized privileged Thread, masks clear. Caller has installed the
 *       sole WWDG vector calling ewi_irq(warning). No worker, callback or feed
 *       policy is installed. Sticky EWI and its storage cannot be reclaimed
 *       before reset, even after failed NVIC enable or a reset request.
 */
nx_result_t nx_stm32f407_wwdg_ewi_arm(nx_stm32f407_wwdg_ewi_t* warning,
                                      const nx_watchdog_port_t* watchdog,
                                      uint8_t priority);
/**
 * \brief Acknowledge one raw warning and latch its occurrence, without feed.
 * \param[in,out] warning: Exact retained object passed to arm.
 * \return SUCCESS, STATE when no event, IO on failed acknowledgement.
 * \note WWDG IRQ only, privileged single writer. Failure retains all owners;
 *       no watchdog reload, reset, cleanup or product recovery is attempted.
 */
nx_result_t nx_stm32f407_wwdg_ewi_irq(nx_stm32f407_wwdg_ewi_t* warning);
/**
 * \brief Read the saturating warning count and explicit lost-count fact.
 * \param[in] warning: Exact retained object, no concurrent arm/reclamation.
 * \param[out] occurrences: Distinct call-owned count, accessed only here.
 * \param[out] overflow: Distinct call-owned saturation observation.
 * \return SUCCESS or INVALID/CONTEXT/STATE without withdrawing any source.
 * \note Privileged Thread/configurable IRQ; saved mask protects the snapshot.
 */
nx_result_t nx_stm32f407_wwdg_ewi_read(const nx_stm32f407_wwdg_ewi_t* warning,
                                       uint32_t* occurrences, bool* overflow);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F407_WWDG_EWI_H */
