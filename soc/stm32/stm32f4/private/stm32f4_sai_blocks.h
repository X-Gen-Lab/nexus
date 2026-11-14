/**
 * \file            stm32f4_sai_blocks.h
 * \brief           SAI double-buffer sessions without finite-mode RAM cost
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_SAI_BLOCKS_H
#define NEXUS_STM32F4_SAI_BLOCKS_H
#include "stm32f4_sai.h"

/** \brief Exact free-frame facts and a caller-upheld foreground service bound.
 * \note max_bit_clock_hz bounds external slave SCK and the selected master
 *       rate. max_service_gap_us includes preemption and service execution.
 */
typedef struct {
    nx_stm32_sai_config_t audio;
    uint32_t max_service_gap_us;
} nx_stm32_sai_blocks_config_t;

/** \brief Only a selected continuous personality retains these DMA loans. */
typedef struct {
    nx_stm32_sai_state_t cold;
    const nx_stm32_sai_blocks_config_t* config;
    nx_audio_blocks_request_t* active;
    nx_stream_producer_t producer;
    nx_stream_fill_t receive[2];
    nx_stream_fill_t replacement;
    nx_stream_block_t transmit[2];
    nx_stream_block_t next_transmit;
    nx_time_us_t observed_at;
    size_t transferred;
    volatile uint32_t tx_events;
    volatile uint32_t rx_events;
    uint8_t tx_bank;
    uint8_t rx_bank;
    uint8_t tx_loans;
    bool replacement_live;
    bool next_transmit_live;
    bool draining;
} nx_stm32_sai_blocks_state_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Shared readonly methods; finite submit/cancel are unselected. */
extern const nx_audio_ops_t nx_stm32_sai_blocks_ops;
/** \brief Admit the exact cold owner before shared resources are acquired.
 * \param[in] state: Zero independent owner retained through failed cleanup.
 * \param[in] config: Immutable format, routes and maximum service gap.
 * \return SUCCESS or rejection without effects or retained references.
 * \note Privileged serialized unmasked task; external slave clocks are idle.
 */
nx_result_t
nx_stm32_sai_blocks_admit(const nx_stm32_sai_blocks_state_t* state,
                          const nx_stm32_sai_blocks_config_t* config);
/** \brief Prepare the shared cold transport without starting a session.
 * \param[in,out] state: Zero owner; partial acquisition retains this storage.
 * \param[in] config: Same exact admitted format and scheduling facts.
 * \return SUCCESS or a failure with retained close responsibility.
 * \note Same privileged serialized task as the finite cold owner.
 */
nx_result_t
nx_stm32_sai_blocks_prepare(nx_stm32_sai_blocks_state_t* state,
                            const nx_stm32_sai_blocks_config_t* config);
/** \brief Drain the session before returning cold resources.
 * \param[in,out] state: Same retained owner as prepare and every IRQ entry.
 * \return SUCCESS after withdrawal, otherwise retained recovery duties.
 * \note Serialized task; failure never revokes a session or block borrow.
 */
nx_result_t nx_stm32_sai_blocks_close(nx_stm32_sai_blocks_state_t* state);
/** \brief Latch selected source faults without publishing any block.
 * \param[in,out] state: Static owner alive until its source vector joins.
 * \note Bounded IRQ entry; explicit service owns replacement and withdrawal.
 */
void nx_stm32_sai_blocks_irq(nx_stm32_sai_blocks_state_t* state);
/** \brief Latch selected DMA facts without returning a still-addressed loan.
 * \param[in,out] state: Static owner shared with the selected DMA vector.
 * \param[in] receive: True for RX, false for TX.
 * \note Bounded IRQ entry; late IRQs cannot restart a settled session.
 */
void nx_stm32_sai_blocks_dma_irq(nx_stm32_sai_blocks_state_t* state,
                                 bool receive);
#ifdef __cplusplus
}
#endif
#endif
