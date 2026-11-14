/**
 * \file            stm32f4_crypto_irq.h
 * \brief           Cold vector receipts for statically selected crypto owners
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CRYPTO_IRQ_H
#define NEXUS_STM32F4_CRYPTO_IRQ_H
#include "nexus/core/status.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Only the selected cold owner holds this vector until full release. */
typedef struct {
    uint8_t saved_priority;
    bool owned;
} nx_stm32f4_crypto_irq_t;
/** \brief Reject foreign active/enabled/pending vectors without effects.
 * \param[in] receipt: Independent zero or fully released static storage.
 * \param[in] irq: Exact CRYP or HASH_RNG vector from selected device facts.
 * \param[in] priority: Unshifted priority within the physical NVIC width.
 * \return SUCCESS or INVALID/CONTEXT/BUSY before any hardware write.
 * \note Privileged unmasked task; HASH_RNG requires one static shared owner.
 */
nx_result_t nx_stm32f4_crypto_irq_admit(const nx_stm32f4_crypto_irq_t* receipt,
                                        uint16_t irq, uint8_t priority);
/** \brief Save priority and enable only the already admitted vector.
 * \param[in,out] receipt: Static cleanup responsibility, retained on failure.
 * \param[in] irq: Same exact vector used during admission and release.
 * \param[in] priority: Same reviewed unshifted priority.
 * \return SUCCESS or error; failed writes still require release.
 * \note Caller owns the engine gate separately. No engine IRQ source is set.
 */
nx_result_t nx_stm32f4_crypto_irq_acquire(nx_stm32f4_crypto_irq_t* receipt,
                                          uint16_t irq, uint8_t priority);
/** \brief Join vector references after every engine source has been quieted.
 * \param[in,out] receipt: Same static receipt, cleared only after readback.
 * \param[in] irq: Same statically selected vector.
 * \return SUCCESS or CONTEXT/INVALID/BUSY; refusal retains responsibility.
 * \note Task-only. Restore only the saved priority, never another IRQ's state.
 *       The HASH_RNG owner must join every selected publisher before this call.
 */
nx_result_t nx_stm32f4_crypto_irq_release(nx_stm32f4_crypto_irq_t* receipt,
                                          uint16_t irq);
#ifdef __cplusplus
}
#endif
#endif
