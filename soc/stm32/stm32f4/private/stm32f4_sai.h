/**
 * \file            stm32f4_sai.h
 * \brief           Statically owned SAI1 finite audio and complete DMA loans
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_SAI_H
#define NEXUS_STM32F4_SAI_H
#include "nexus/io/audio.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f407_av.h"
/** \brief Complete selected free-protocol geometry; no live format changes.
 * \note SAI1 on exact F446/F479 uses external slave clocks or the selected
 *       System-owned PLLSAI Q kernel in master mode. active_slots is a nonzero
 *       subset of the configured wire slots. Only enabled slots exchange
 *       samples with the FIFO, in increasing wire-slot order; their compact
 *       storage contains no inactive-slot padding. One logical owner reserves
 *       global configuration and both SAI1
 *       blocks, selecting A or B simplex or a synchronous A/B duplex pair.
 *       Shared GPIO and DMA2 gates/snapshots are acquired by Platform first.
 *       max_bit_clock_hz is the electrical SCK ceiling, never a measurement.
 *       Slave mode requires no_divider=false and mckdiv=0. Master mckdiv=0
 *       divides by one, otherwise by2*mckdiv; no_divider bypasses this stage,
 *       suppresses MCLK and requires mckdiv=0. Every mode reserves APB2
 *       strictly above twice SCK. A selected PE2/PF7 MCLK pad requires the
 *       shared 2.048..45 MHz characterized output range; an unselected
 *       MCLK pad does not impose a minimum frame rate. GPIO slew is10.
 *       No audio method changes RCC.
 */
typedef struct {
    nx_stm32_av_config_t common;
    nx_stm32_av_dma_t tx;
    nx_stm32_av_dma_t rx;
    SAI_Block_TypeDef* transmit;
    SAI_Block_TypeDef* receive;
    SAI_Block_TypeDef* source;
    uint32_t hclk_hz;
    uint32_t pclk_hz;
    uint32_t max_bit_clock_hz;
    uint16_t frame_bits;
    uint16_t active_slots;
    uint8_t frame_sync_bits;
    uint8_t first_bit_offset;
    uint8_t sample_bits;
    uint8_t slot_bits;
    uint8_t slots;
    bool frame_sync_high;
    bool frame_sync_before;
    bool frame_sync_channels;
    bool first_lsb;
    bool sample_rising;
    bool master;
    bool no_divider;
    uint8_t mckdiv;
} nx_stm32_sai_config_t;
/** \brief Only a selected finite personality retains these exact loan fields.
 * \note Zero before initial prepare; retain state/config through partial
 *       preparation or drain failure. No caller pointer survives SETTLED.
 */
typedef struct {
    const nx_stm32_sai_config_t* config;
    nx_audio_request_t* active;
    nx_stream_fill_t filling;
    nx_frame_dma_view_t transmit;
    uint64_t time_epoch;
    nx_time_us_t drain_until;
    size_t bytes;
    /** \brief Equal-priority IRQs only OR aligned flags; the task resets
     *       them before enabling any publisher and observes them after join.
     */
    volatile uint32_t events;
    uint32_t clock_added;
    uint32_t priorities[3];
    /** \brief First failure is latched under saved PRIMASK; IRQ/task readers
     *       use an actual load, never a cached foreground-only assumption.
     */
    volatile nx_result_t result;
    bool owned;
    bool initialized;
    bool receive_loan;
    bool tx_armed;
    bool rx_armed;
    bool draining;
    bool wire_quiet;
    bool history_lost;
} nx_stm32_sai_state_t;
/** \brief No-write admission before shared Platform resource acquisition.
 * \param[in] state: Zero independent owner, not an existing live instance.
 * \param[in] config: Exact geometry and reviewed pad/DMA/software clock plan.
 * \return SUCCESS or rejection with no retained reference or hardware effect.
 * \note One privileged unmasked serialized task; external slave SCK is idle.
 */
nx_result_t nx_stm32_sai_admit(const nx_stm32_sai_state_t* state,
                               const nx_stm32_sai_config_t* config);
/** \brief Jointly prepare both blocks with the serializer and DMA disabled.
 * \param[in,out] state: Zero static owner, retained on partial failure.
 * \param[in] config: Same immutable selected route and free-protocol geometry.
 * \return SUCCESS, otherwise retained effects require this owner's close.
 * \note Shared GPIO/DMA2 gates and snapshots already belong to Platform.
 *       External slave clocks begin only after acquire-observed ACTIVE.
 */
nx_result_t nx_stm32_sai_prepare(nx_stm32_sai_state_t* state,
                                 const nx_stm32_sai_config_t* config);
/** \brief Seal admission and return resources only after complete withdrawal.
 * \param[in,out] state: Exact owner selected by prepare and every IRQ entry.
 * \return SUCCESS or retained BUSY/IO; failed close never revokes a loan.
 * \note Serialized task. Stop source before synchronous sink, then observe
 *       both frame tails; never reset or gate shared DMA2.
 */
nx_result_t nx_stm32_sai_close(nx_stm32_sai_state_t* state);
/** \brief Bounded shared-vector source observation; never publishes loans.
 * \param[in,out] state: Same static SAI1 owner, alive until the vector joins.
 * \note Only selected A/B sources are acknowledged. Explicit service performs
 *       complete serializer/DMA/IRQ/address withdrawal and final publication.
 */
void nx_stm32_sai_irq(nx_stm32_sai_state_t* state);
/** \brief Latch exact selected DMA flags without transferring stream rights.
 * \param[in,out] state: Same retained owner as submit and source IRQ.
 * \param[in] receive: True for selected RX; false for selected TX.
 * \note IRQ entry is bounded; sticky W1C flags do not establish completion.
 */
void nx_stm32_sai_dma_irq(nx_stm32_sai_state_t* state, bool receive);
extern const nx_audio_ops_t nx_stm32_sai_ops;
#ifdef __cplusplus
}
#endif
#endif
