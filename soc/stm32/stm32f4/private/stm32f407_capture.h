/**
 * \file            stm32f407_capture.h
 * \brief           Exact DCMI snapshots and opaque storage block owner
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_CAPTURE_H
#define NX_STM32F407_CAPTURE_H
#include "nexus/io/capture.h"
#include "stm32f407_av.h"

/** \brief Static crop counts are input clocks/lines, never output pixel sizes.
 */
typedef struct {
    uint16_t x_clocks;
    uint16_t y_lines;
    uint16_t width_clocks;
    uint16_t height_lines;
    bool enabled;
} nx_stm32_capture_crop_t;
/** \brief Selected chip route and electrical upper bound; no sensor policy. */
typedef struct {
    nx_stm32_av_config_t common;
    nx_stm32_av_dma_t memory;
    DCMI_TypeDef* registers;
    nx_stm32_capture_crop_t crop;
    uint32_t hclk_hz;
    uint32_t pixel_clock_hz;
    uint32_t max_service_gap_us;
    uint8_t bits;
    uint8_t frame_divisor;
    uint8_t codes[4];
    bool embedded;
    bool jpeg;
    bool continuous;
    bool pclk_falling;
    bool hsync_high;
    bool vsync_high;
} nx_stm32_capture_config_t;
/** \brief All request and posted producer references survive failed stop. */
typedef struct {
    const nx_stm32_capture_config_t* config;
    nx_capture_request_t* active;
    nx_stream_t* stream;
    nx_stream_producer_t posted;
    nx_stream_fill_t targets[2];
    nx_stream_fill_t replacement;
    nx_time_us_t observed_at;
    uint64_t time_epoch;
    size_t block_bytes;
    size_t remaining;
    uint32_t events;
    uint32_t dma_events;
    uint32_t clock_added;
    uint32_t priorities[2];
    nx_result_t result;
    uint8_t expected_bank;
    bool initialized;
    bool owned;
    bool running;
    bool draining;
    bool replacement_live;
    bool history_lost;
} nx_stm32_capture_state_t;
/** \brief One readonly table for both reviewed DCMI DMA lanes. */
extern const nx_capture_ops_t nx_stm32_capture_ops;
/** \brief No-write cold preflight; rejected state has no resource authority. */
nx_result_t nx_stm32_capture_admit(const nx_stm32_capture_state_t* state,
                                   const nx_stm32_capture_config_t* config);
/** \brief Platform-only preparation; never starts CAPTURE or borrows a buffer.
 * \param[in,out] state Fresh static owner, retained after partial failure.
 * \param[in] config Immutable selected route, clocks, DMA and polarity.
 * \return Success or failure retaining exact peripheral/pad responsibility.
 * \note Privileged unmasked startup, with shared GPIO/DMA leases already held.
 */
nx_result_t nx_stm32_capture_prepare(nx_stm32_capture_state_t* state,
                                     const nx_stm32_capture_config_t* config);
/** \brief Stop and join before withdrawing this DCMI gate; failure retains it.
 * \param[in,out] state Same owner used by Platform prepare and IRQ entries.
 * \return Success after full source/IRQ/bus withdrawal, otherwise retained IO.
 * \note Serialized privileged task; no shared DMA2 reset is ever authorized.
 */
nx_result_t nx_stm32_capture_close(nx_stm32_capture_state_t* state);
/** \brief Exact generated DCMI entry; latches only while this owner is live. */
void nx_stm32_capture_irq(nx_stm32_capture_state_t* state);
/** \brief Exact generated DMA entry; no borrowed request access after detach.
 */
void nx_stm32_capture_dma_irq(nx_stm32_capture_state_t* state);
#endif /* NX_STM32F407_CAPTURE_H */
