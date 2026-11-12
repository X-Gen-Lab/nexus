/**
 * \file            stm32f4_hash.h
 * \brief           Private bounded HASH/HMAC FIFO owner with shared RNG IRQ
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_HASH_H
#define NEXUS_STM32F4_HASH_H
#include "nexus/io/hash.h"
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
/** \brief Immutable exact publisher facts; Platform owns HASH_RNG dispatch. */
typedef struct {
    nx_irq_policy_t irq_policy;
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
 * \note No implicit task/queue, DMA, RNG lease or shared NVIC mutation.
 */
nx_result_t nx_stm32f4_hash_start(nx_stm32f4_hash_state_t* state,
                                  const nx_stm32f4_hash_config_t* config);
/** \brief Observe at most seventeen FIFO words, without a BUSY wait.
 * \param[in,out] state: Static owner that outlives its fixed IRQ dispatcher.
 * \note Shared HASH_RNG IRQ; neither RNG registers nor whole vector are
 *       disabled here. HMAC phase waits progress via explicit task service.
 *       Late IRQs after successful stop do not access retired borrowers.
 */
void nx_stm32f4_hash_irq(nx_stm32f4_hash_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
