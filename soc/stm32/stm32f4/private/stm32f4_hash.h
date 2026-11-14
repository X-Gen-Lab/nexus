/**
 * \file            stm32f4_hash.h
 * \brief           Private bounded HASH/HMAC FIFO owner with shared RNG IRQ
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_HASH_H
#define NEXUS_STM32F4_HASH_H
#include "nexus/io/hash.h"
#include "stm32f4_crypto_dma.h"
/* Keep the callback ABI independent of another selected HAL module's conf. */
#if defined(USE_HAL_HASH_REGISTER_CALLBACKS) &&                                \
    USE_HAL_HASH_REGISTER_CALLBACKS != 0U
#error "The selected crypto transport requires the fixed callback ABI"
#endif
#ifndef USE_HAL_HASH_REGISTER_CALLBACKS
#define USE_HAL_HASH_REGISTER_CALLBACKS 0U
#endif
#include "stm32f4xx_hal_dma.h"
/* Crypto Handles declare DMA types even when only IRQ FIFO is selected. */
#include "stm32f4xx_hal_hash.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One optional official HASH transport and its exact input channel. */
typedef struct {
    HASH_HandleTypeDef handle;
    nx_stm32_crypto_dma_channel_t input;
} nx_stm32f4_hash_dma_state_t;
/** \brief Selected DMA input/key state; digest is read through the CPU. */
typedef struct {
    nx_stm32f4_hash_dma_state_t* state;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32f4_hash_dma_config_t;
/** \brief Immutable exact publisher facts; Platform owns HASH_RNG dispatch. */
typedef struct {
    nx_irq_policy_t irq_policy;
    const nx_stm32f4_hash_dma_config_t* dma;
} nx_stm32f4_hash_config_t;
/** \brief One controller, one request, no heap, full-message copy or worker.
 * \note Phase/offset are private hardware protocol state. Key bytes remain
 *       caller storage. A failed reset retains every borrow until recovery.
 */
typedef struct {
    const nx_stm32f4_hash_config_t* config;
    nx_hash_request_t* active;
    const nx_irq_wake_t* wake;
    uint64_t epoch;
    size_t offset;
    nx_result_t terminal;
    uint8_t phase;
    bool first_block;
    bool waiting;
    bool running;
    bool gate_owned;
    bool reset_owned;
    bool terminal_pending;
    bool faulted;
} nx_stm32f4_hash_state_t;
#if NEXUS_STM32_HASH_DMA
/** \brief Admit selected immutable DMA facts without acquiring hardware.
 * \param[in] state: Cold owner storage.
 * \param[in] config: Validated outer configuration.
 * \return SUCCESS or rejection, with no effects or retained references.
 */
nx_result_t nx_stm32f4_hash_dma_admit(const nx_stm32f4_hash_state_t* state,
                                      const nx_stm32f4_hash_config_t* config);
/** \brief Prepare the selected input stream after DMA2 clock acquisition.
 * \param[in,out] state: Original owner and stable configuration.
 * \return SUCCESS or retained failure requiring stop.
 */
nx_result_t nx_stm32f4_hash_dma_prepare(nx_stm32f4_hash_state_t* state);
/** \brief Start the official whole-word SHA1/MD5/HMAC DMA transport.
 * \param[in,out] state: Accepted positive aligned message/key spans.
 * \return SUCCESS or retained failure; digest and borrowers remain live.
 */
nx_result_t nx_stm32f4_hash_dma_submit(nx_stm32f4_hash_state_t* state);
/** \brief Advance official HMAC phases and retrieve only a ready digest.
 * \param[in,out] state: Original serialized task owner with all spans live.
 * \return SUCCESS on proven digest readiness, BUSY or retained IO; no release.
 */
nx_result_t nx_stm32f4_hash_dma_observe(nx_stm32f4_hash_state_t* state);
/** \brief Join DMA and erase HAL key/message references before HASH reset.
 * \param[in,out] state: Original owner retained on failed withdrawal.
 * \return SUCCESS or retained failure; RNG sources are never changed.
 */
nx_result_t nx_stm32f4_hash_dma_withdraw(nx_stm32f4_hash_state_t* state);
/** \brief Restore the selected cold stream without a shared DMA reset.
 * \param[in,out] state: Original owner; Platform still holds DMA2 clock.
 * \return SUCCESS or retained failure, without changing outer clock gates.
 */
nx_result_t nx_stm32f4_hash_dma_stop(nx_stm32f4_hash_state_t* state);
#endif
/** \brief Shared static operations for the reviewed one-shot FIFO mode. */
extern const nx_hash_ops_t nx_stm32f4_hash_ops;
/** \brief Inspect cold storage/engine without writes or retained references.
 * \param[in] state: Zero or fully stopped static owner.
 * \param[in] config: Independent readonly publisher policy.
 * \return SUCCESS or INVALID/CONTEXT/BUSY before effects.
 * \note Privileged serialized task; the RNG gate and IRQ source are unrelated.
 */
nx_result_t nx_stm32f4_hash_admit(const nx_stm32f4_hash_state_t* state,
                                  const nx_stm32f4_hash_config_t* config);
/** \brief Acquire only HASH after the selected System has a live time epoch.
 * \param[in,out] state: Previously admitted stable private owner.
 * \param[in] config: Borrowed immutable facts through successful stop.
 * \return SUCCESS or error; post-write errors preserve cleanup responsibility.
 * \note No implicit task/queue, RNG lease or shared NVIC mutation. Selected
 *       DMA alone allocates its extension; Platform explicitly holds DMA2
 * clock.
 */
nx_result_t nx_stm32f4_hash_start(nx_stm32f4_hash_state_t* state,
                                  const nx_stm32f4_hash_config_t* config);
/** \brief Observe at most seventeen FIFO words, without a BUSY wait.
 * \param[in,out] state: Static owner that outlives its fixed IRQ dispatcher.
 * \note Shared HASH_RNG IRQ; neither RNG registers nor whole vector are
 *       disabled here. DMA mode only withdraws the digest-level hint; callbacks
 *       and HMAC phases progress through explicit unmasked task service.
 *       Late IRQs after successful stop do not access retired borrowers.
 */
void nx_stm32f4_hash_irq(nx_stm32f4_hash_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
