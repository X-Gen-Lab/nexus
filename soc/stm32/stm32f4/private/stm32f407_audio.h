/**
 * \file            stm32f407_audio.h
 * \brief           Private I2S2/3 finite DMA and shared PLLI2S ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_AUDIO_H
#define NX_STM32F407_AUDIO_H
#include "nexus/io/audio.h"
#include "stm32f407_av.h"

/** \brief Compatible shared clock lease; no registry or dynamically sized pool.
 * \note The two generated audio owners share this exact static object. The
 *       hardware's PLL source and M divider remain owned by the System clock.
 */
typedef struct {
    uint32_t saved_plli2s;
    uint32_t saved_source;
    uint32_t saved_trace;
    uint32_t saved_counter;
    uint16_t pll_n;
    uint8_t pll_r;
    uint8_t users;
    uint8_t cycle_users;
    bool acquired;
} nx_stm32_audio_clock_t;
/** \brief All wire settings are immutable resolved configuration. */
typedef struct {
    nx_stm32_av_config_t common;
    nx_stm32_av_dma_t tx;
    nx_stm32_av_dma_t rx;
    SPI_TypeDef* registers;
    SPI_TypeDef* extension;
    nx_stm32_audio_clock_t* clock;
    GPIO_TypeDef* ws_gpio;
    GPIO_TypeDef* ck_gpio;
    nx_audio_rate_t actual_rate;
    uint32_t hclk_hz;
    uint32_t pclk_hz;
    uint32_t pll_input_hz;
    uint16_t pll_n;
    uint16_t divider;
    uint8_t pll_m;
    uint8_t pll_r;
    uint8_t controller;
    uint8_t standard;
    uint8_t sample_bits;
    uint8_t slot_bits;
    uint8_t ws_pin;
    uint8_t ck_pin;
    bool odd;
    bool pll_hse;
    bool master;
    bool receive;
    bool duplex;
    bool mclk;
    bool cpol_high;
} nx_stm32_audio_config_t;
/** \brief Full retained operation; source loans are never reconstructed. */
typedef struct {
    const nx_stm32_audio_config_t* config;
    nx_audio_request_t* active;
    nx_stream_fill_t receive;
    nx_frame_dma_view_t transmit;
    nx_time_us_t observed_at;
    uint64_t time_epoch;
    size_t bytes;
    size_t tx_remaining;
    size_t rx_remaining;
    uint32_t events;
    uint32_t clock_added;
    uint32_t priorities[3];
    uint32_t terminal_cycle;
    nx_result_t result;
    bool initialized;
    bool owned;
    bool filling;
    bool tx_armed;
    bool rx_armed;
    bool tx_enable_attempted;
    bool rx_enable_attempted;
    bool wire_quiet;
    bool draining;
    bool history_lost;
    bool clock_user;
    bool cycle_user;
    bool receive_tail;
    bool terminal_stopped;
} nx_stm32_audio_state_t;
/** \brief One shared immutable operations table for I2S2 and I2S3. */
extern const nx_audio_ops_t nx_stm32_audio_ops;
/** \brief Cold no-write admission before shared Platform resource changes. */
nx_result_t nx_stm32_audio_admit(const nx_stm32_audio_state_t* state,
                                 const nx_stm32_audio_config_t* config);
/** \brief Prepare only; no I2SE, DMA or request is started.
 * \param[in,out] state Static zero owner, retained after partial failure.
 * \param[in] config Immutable selected I2S2/3 route, format and PLL tuple.
 * \return Success or failure retaining exact resource/clock responsibilities.
 * \note Serialized privileged startup, shared GPIO/DMA1 already acquired.
 */
nx_result_t nx_stm32_audio_prepare(nx_stm32_audio_state_t* state,
                                   const nx_stm32_audio_config_t* config);
/** \brief Withdraw only after stream/IRQ/loan joins; refusal retains System.
 * \param[in,out] state Same selected owner as prepare and physical IRQs.
 * \return Success after exact gate/PLLI2S lease release, otherwise retained IO.
 * \note Serialized privileged task. Shared DMA1 is never reset or gated here.
 */
nx_result_t nx_stm32_audio_close(nx_stm32_audio_state_t* state);
/** \brief Error-source entry; latches first read-clear SR faults. */
void nx_stm32_audio_irq(nx_stm32_audio_state_t* state);
/** \brief Selected finite DMA entry; master RX performs terminal clock stop. */
void nx_stm32_audio_dma_irq(nx_stm32_audio_state_t* state, bool receive);

#ifndef NX_AV_CYCLES
#define NX_AV_CYCLES()        (DWT->CYCCNT)
#define NX_AV_TRACE           (&DCB->DEMCR)
#define NX_AV_COUNTER_CONTROL (&DWT->CTRL)
#endif
#endif /* NX_STM32F407_AUDIO_H */
