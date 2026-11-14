/**
 * \file            stm32f4_cipher.h
 * \brief           Private static CRYP owner using the reviewed official HAL
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CIPHER_H
#define NEXUS_STM32F4_CIPHER_H
#include "nexus/io/cipher.h"
#include "stm32f4_crypto_dma.h"
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
/** \brief Separate optional input/output transports for the selected DMA mode.
 */
typedef struct {
    nx_stm32_crypto_dma_channel_t input;
    nx_stm32_crypto_dma_channel_t output;
} nx_stm32f4_cipher_dma_state_t;
/** \brief Static selected DMA state and actual readable/writable regions. */
typedef struct {
    nx_stm32f4_cipher_dma_state_t* state;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32f4_cipher_dma_config_t;
/** \brief Immutable IRQ facts; the Platform owns the vector lease. */
typedef struct {
    nx_irq_policy_t irq_policy;
    const nx_stm32f4_cipher_dma_config_t* dma;
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
#if NEXUS_STM32_CIPHER_DMA
/** \brief Admit selected immutable DMA facts without acquiring hardware.
 * \param[in] state: Cold owner storage.
 * \param[in] config: Validated outer configuration.
 * \return SUCCESS or rejection, with no effects or retained references.
 */
nx_result_t
nx_stm32f4_cipher_dma_admit(const nx_stm32f4_cipher_state_t* state,
                            const nx_stm32f4_cipher_config_t* config);
/** \brief Prepare selected DMA state after Platform has acquired DMA2 clock.
 * \param[in,out] state: Original owner with stable config.
 * \return SUCCESS or retained failure requiring stop.
 */
nx_result_t nx_stm32f4_cipher_dma_prepare(nx_stm32f4_cipher_state_t* state);
/** \brief Run the official complete basic encrypt/decrypt DMA transport.
 * \param[in,out] state: Admitted request, key/IV and exact memory spans.
 * \return SUCCESS or retained IO failure; no request is released.
 */
nx_result_t nx_stm32f4_cipher_dma_submit(nx_stm32f4_cipher_state_t* state);
/** \brief Poll both DMA channels without confusing memory and engine
 * completion. \param[in,out] state: Original serialized task owner and live
 * request. \return SUCCESS when output and engine are complete, BUSY or
 * retained IO.
 */
nx_result_t nx_stm32f4_cipher_dma_observe(nx_stm32f4_cipher_state_t* state);
/** \brief Join both channels before key/context reset or borrower release.
 * \param[in,out] state: Original owner, retained on any failure.
 * \return SUCCESS or retained failure; completed output blocks are recorded.
 */
nx_result_t nx_stm32f4_cipher_dma_withdraw(nx_stm32f4_cipher_state_t* state);
/** \brief Restore the original cold DMA channels without resetting siblings.
 * \param[in,out] state: Original owner; Platform still holds DMA2 clock.
 * \return SUCCESS or retained failure; no outer gate is changed.
 */
nx_result_t nx_stm32f4_cipher_dma_stop(nx_stm32f4_cipher_state_t* state);
#endif
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
 *       DMA mode never feeds this FIFO from the IRQ; one explicit task services
 *       its disabled DMA vectors and proves complete engine withdrawal.
 */
void nx_stm32f4_cipher_irq(nx_stm32f4_cipher_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
