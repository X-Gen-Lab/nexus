/**
 * \file            stm32f4_cipher.h
 * \brief           Private static CRYP owner using the reviewed official HAL
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CIPHER_H
#define NEXUS_STM32F4_CIPHER_H
#include "nexus/io/cipher.h"
/* Keep the callback ABI independent of another selected HAL module's conf. */
#if defined(USE_HAL_CRYP_REGISTER_CALLBACKS) &&                                \
    USE_HAL_CRYP_REGISTER_CALLBACKS != 0U
#error "The selected crypto transport requires the fixed callback ABI"
#endif
#ifndef USE_HAL_CRYP_REGISTER_CALLBACKS
#define USE_HAL_CRYP_REGISTER_CALLBACKS 0U
#endif
#include "stm32f4xx_hal_dma.h"
/* Crypto Handles declare DMA types even when only IRQ FIFO is selected. */
#include "stm32f4xx_hal_cryp.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable IRQ facts; the Platform owns the vector lease. */
typedef struct {
    nx_irq_policy_t irq_policy;
} nx_stm32f4_cipher_config_t;
/** \brief One static exclusive engine; zero initialize before cold admission.
 * \note Handle, active request and wake remain live through failed cleanup.
 *       A terminal IRQ only latches a cause. Task service withdraws hardware
 *       and clears secret context before publishing SETTLED.
 */
typedef struct {
    CRYP_HandleTypeDef handle;
    const nx_stm32f4_cipher_config_t* config;
    nx_cipher_request_t* active;
    const nx_irq_wake_t* wake;
    uint64_t epoch;
    nx_result_t terminal;
    bool running;
    bool gate_owned;
    bool terminal_pending;
    bool reset_owned;
    bool faulted;
} nx_stm32f4_cipher_state_t;
/** \brief Static shared operations; no runtime registry or allocation. */
extern const nx_cipher_ops_t nx_stm32f4_cipher_ops;
/** \brief Inspect an unowned engine before System has hardware effects.
 * \param[in] state: Zero or fully stopped private static storage.
 * \param[in] config: Independent immutable exact IRQ policy.
 * \return SUCCESS or INVALID/CONTEXT/BUSY, without effects or borrowing.
 * \note Serialized privileged unmasked task. Clock/reset ownership is unique.
 */
nx_result_t nx_stm32f4_cipher_admit(const nx_stm32f4_cipher_state_t* state,
                                    const nx_stm32f4_cipher_config_t* config);
/** \brief Acquire the engine gate after the selected System is live.
 * \param[in,out] state: Same admitted static owner.
 * \param[in] config: Borrowed unchanged through successful stop.
 * \return SUCCESS or error; a post-write error retains cleanup responsibility.
 * \note Platform separately installs/joins the dedicated IRQ vector. No PLL,
 *       task, allocator or product key policy is created here.
 */
nx_result_t nx_stm32f4_cipher_start(nx_stm32f4_cipher_state_t* state,
                                    const nx_stm32f4_cipher_config_t* config);
/** \brief Dispatch at most one official HAL FIFO observation.
 * \param[in,out] state: Stable static owner; valid even after stop.
 * \note Dedicated configurable IRQ, bounded. A late IRQ never consults a
 *       retired config/request. Completion remains borrowed until service.
 */
void nx_stm32f4_cipher_irq(nx_stm32f4_cipher_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
