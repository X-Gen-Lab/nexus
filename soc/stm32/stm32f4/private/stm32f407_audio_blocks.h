/**
 * \file            stm32f407_audio_blocks.h
 * \brief           I2S double-buffer sessions without finite-mode RAM cost
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_AUDIO_BLOCKS_H
#define NX_STM32F407_AUDIO_BLOCKS_H
#include "stm32f407_audio.h"

/** \brief Reviewed continuous scheduling bounds beside the shared cold facts.
 * \note Slave max_sample_rate_hz is an external clock upper bound, never a
 *       measured rate. The exact master rate must satisfy the same bound.
 */
typedef struct {
    nx_stm32_audio_config_t audio;
    uint32_t max_sample_rate_hz;
    uint32_t max_service_gap_us;
} nx_stm32_audio_blocks_config_t;

/** \brief Only a selected blocks personality retains these exact DMA loans. */
typedef struct {
    nx_stm32_audio_state_t cold;
    const nx_stm32_audio_blocks_config_t* config;
    nx_audio_blocks_request_t* active;
    nx_stream_producer_t producer;
    nx_stream_fill_t receive[2];
    nx_stream_fill_t replacement;
    nx_stream_block_t transmit[2];
    nx_stream_block_t next_transmit;
    nx_time_us_t observed_at;
    uint64_t time_epoch;
    size_t transferred;
    volatile uint32_t tx_events;
    volatile uint32_t rx_events;
    uint8_t tx_bank;
    uint8_t rx_bank;
    uint8_t tx_loans;
    bool replacement_live;
    bool next_transmit_live;
    bool draining;
    bool history_lost;
} nx_stm32_audio_blocks_state_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Shared static methods; finite submit/cancel are unselected. */
extern const nx_audio_ops_t nx_stm32_audio_blocks_ops;
/** \brief No-write cold admission before shared System resources are acquired.
 * \param[in] state Same zero owner, disjoint from immutable configuration.
 * \param[in] config Exact format, routes and reviewed scheduling bounds.
 * \return SUCCESS or a rejection without retained effects or references.
 * \note Serialized privileged unmasked task, as for the finite cold owner.
 */
nx_result_t
nx_stm32_audio_blocks_admit(const nx_stm32_audio_blocks_state_t* state,
                            const nx_stm32_audio_blocks_config_t* config);
/** \brief Validate continuous bounds and acquire the existing cold owner.
 * \param[in,out] state Zero caller storage, retained on partial acquisition.
 * \param[in] config Exact cold facts and externally upheld scheduling bounds.
 * \return SUCCESS or an error retaining every acquired cold resource.
 * \note Privileged serialized unmasked task; no DMA or serializer starts.
 */
nx_result_t
nx_stm32_audio_blocks_prepare(nx_stm32_audio_blocks_state_t* state,
                              const nx_stm32_audio_blocks_config_t* config);
/** \brief Seal admission, drain session and then release the shared cold owner.
 * \param[in,out] state Same owner as prepare, session and every IRQ entry.
 * \return SUCCESS after full withdrawal, otherwise retained recovery duties.
 * \note Same serialized task; failed drain never revokes blocks or session.
 */
nx_result_t nx_stm32_audio_blocks_close(nx_stm32_audio_blocks_state_t* state);
/** \brief Latch source faults without retaining caller callback pointers.
 * \param[in,out] state Same static owner, alive until source IRQ is joined.
 * \note Bounded IRQ entry; explicit task service replaces and publishes banks.
 */
void nx_stm32_audio_blocks_irq(nx_stm32_audio_blocks_state_t* state);
/** \brief Latch selected DMA events without publishing a still-addressed bank.
 * \param[in,out] state Same static owner as the selected stream vector.
 * \param[in] receive True for RX; false for TX.
 * \note IRQ only records facts. Missing service never causes automatic reuse.
 */
void nx_stm32_audio_blocks_dma_irq(nx_stm32_audio_blocks_state_t* state,
                                   bool receive);
#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F407_AUDIO_BLOCKS_H */
