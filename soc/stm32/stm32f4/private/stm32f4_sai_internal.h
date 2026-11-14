/**
 * \file            stm32f4_sai_internal.h
 * \brief           Shared SAI cold-owner facts for finite and block transports
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_SAI_INTERNAL_H
#define NEXUS_STM32F4_SAI_INTERNAL_H
#include "stm32f4_sai.h"

static inline size_t nx_stm32_sai_unit(const nx_stm32_sai_config_t* c) {
    return c->sample_bits <= 8U ? 1U : c->sample_bits <= 16U ? 2U : 4U;
}
/** \brief Only enabled wire slots exchange FIFO samples (RM0390, SLOTR). */
static inline uint8_t
nx_stm32_sai_stored_slots(const nx_stm32_sai_config_t* c) {
    uint8_t count = 0U;
    for (uint16_t mask = c->active_slots; mask != 0U; mask >>= 1U)
        count += (uint8_t)(mask & 1U);
    return count;
}
/** \brief Address identity selects one nx_stm32_sai_controller without adding
 * instance RAM. */
static inline SAI_TypeDef* nx_stm32_sai_controller(const SAI_Block_TypeDef* b) {
    return b == SAI1_Block_A || b == SAI1_Block_B ? SAI1
#if defined(STM32F446xx)
                                                  : SAI2
#else
                                                  : NULL
#endif
        ;
}
static inline SAI_Block_TypeDef*
nx_stm32_sai_first(const nx_stm32_sai_config_t* c) {
    return (SAI_Block_TypeDef*)((uintptr_t)nx_stm32_sai_controller(c->source) +
                                4U);
}
static inline SAI_Block_TypeDef*
nx_stm32_sai_second(const nx_stm32_sai_config_t* c) {
    return (SAI_Block_TypeDef*)((uintptr_t)nx_stm32_sai_controller(c->source) +
                                0x24U);
}
static inline IRQn_Type
nx_stm32_sai_source_irq(const nx_stm32_sai_config_t* c) {
#if defined(STM32F446xx)
    if (nx_stm32_sai_controller(c->source) == SAI2)
        return SAI2_IRQn;
#else
    (void)c;
#endif
    return SAI1_IRQn;
}
static inline uint32_t nx_stm32_sai_gate(const nx_stm32_sai_config_t* c) {
#if defined(STM32F446xx)
    if (nx_stm32_sai_controller(c->source) == SAI2)
        return RCC_APB2ENR_SAI2EN;
#else
    (void)c;
#endif
    return RCC_APB2ENR_SAI1EN;
}
static inline uint32_t nx_stm32_sai_reset(const nx_stm32_sai_config_t* c) {
#if defined(STM32F446xx)
    if (nx_stm32_sai_controller(c->source) == SAI2)
        return RCC_APB2RSTR_SAI2RST;
#else
    (void)c;
#endif
    return RCC_APB2RSTR_SAI1RST;
}

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe the original clock, reset and monotonic history.
 * \param[in,out] state: Prepared owner; failure latches lost history.
 * \return True only while all selected hardware resources remain live.
 * \note Serialized task; no register state can repair lost ownership history.
 */
bool nx_stm32_sai_live(nx_stm32_sai_state_t* state);
/** \brief Exclude all shared cold configuration and hardware control storage.
 * \param[in] state: Original owner and its exact immutable facts.
 * \param[in] config: Original immutable configuration.
 * \param[in] data: Candidate typed control or DMA payload.
 * \param[in] bytes: Complete object or payload capacity.
 * \return True for a disjoint nonwrapping span.
 * \note Pure validation; caller also excludes its personality-specific state.
 */
bool nx_stm32_sai_disjoint(const nx_stm32_sai_state_t* state,
                           const nx_stm32_sai_config_t* config,
                           const void* data, size_t bytes);
/** \brief Prime the FIFO and enable the synchronous sink before its source.
 * \param[in,out] state: Prepared owner with the original time_epoch.
 * \param[in,out] request: Accepted request whose deadline remains
 * authoritative. \return SUCCESS after ACTIVE, BUSY while priming, otherwise a
 * drain cause. \note Serialized task; checks the same epoch and deadline
 * between effects.
 */
nx_result_t nx_stm32_sai_start(nx_stm32_sai_state_t* state,
                               nx_request_t* request);
/** \brief Request source and sink disable and observe both frame tails.
 * \param[in,out] state: Same live owner; wire_quiet is a proved latch.
 * \return True after serializer withdrawal, false while clocks or FIFO remain.
 * \note Serialized task; DMA and IRQ withdrawal are separate obligations.
 */
bool nx_stm32_sai_wire_stop(nx_stm32_sai_state_t* state);
/** \brief Latch the first task or IRQ cause under the incoming IRQ mask.
 * \param[in,out] state: Same stable owner used by every selected source.
 * \param[in] cause: Failure to latch; SUCCESS leaves the latch unchanged.
 * \note Bounded task or IRQ call; retains no additional reference.
 */
void nx_stm32_sai_latch(nx_stm32_sai_state_t* state, nx_result_t cause);
#ifdef __cplusplus
}
#endif
#endif
