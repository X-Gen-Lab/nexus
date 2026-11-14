/**
 * \file            stm32f407_sdio.h
 * \brief           Exact F407 SD/MMC infrastructure and finite media ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SDIO_H
#define NX_STM32F407_SDIO_H

#include "nexus/io/block.h"
#include "nexus/io/block_async.h"
#include "nexus/io/dma.h"
#include "nexus/io/sdio_io.h"
#include "stm32f407_platform_cold.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Selected finite engine; polling remains the zero default. */
typedef enum {
    NX_STM32_SDIO_POLL,
    NX_STM32_SDIO_IRQ,
    NX_STM32_SDIO_DMA
} nx_stm32_sdio_engine_t;

/** \brief Explicit card protocol; no automatic SD-to-MMC fallback probe. */
typedef enum {
    NX_STM32_SDIO_SD_MEMORY,
    NX_STM32_SDIO_MMC,
    NX_STM32_SDIO_IO
} nx_stm32_sdio_protocol_t;

/** \brief Selected I/O-card register facts, not a device or object pool. */
typedef struct {
    uint16_t block_bytes[7];
    uint8_t function_count;
    uint8_t enabled;
    uint8_t interrupts;
    uint8_t inflight;
    bool interrupt_latched;
    bool interrupt_armed;
    bool memory_present;
} nx_stm32_sdio_io_state_t;

/** \brief Only asynchronous selections allocate this persistent extension. */
typedef struct {
    nx_request_t* active;
    nx_result_t terminal;
    uint32_t words;
    uint32_t completed_blocks;
    uint32_t flags;
    uint32_t function_address;
    uint16_t unit_bytes;
    uint8_t function;
    bool block_mode;
    bool increment;
    bool draining;
    bool dma_complete; /**< Observed terminal receipt, not media completion. */
    bool dma_started;
} nx_stm32_sdio_async_state_t;

/** \brief Exact six/ten-pad socket, selected engine and finite bounds. */
typedef struct {
    SDIO_TypeDef* registers;
    const nx_stm32_platform_pin_config_t* pins;
    uint32_t max_blocks;
    uint32_t poll_budget;
    uint32_t stop_timeout_us;
    uint8_t bus_width;
    bool high_speed; /**< Explicit negotiated SDR timing, never fallback. */
    nx_stm32_sdio_engine_t engine;
    nx_stm32_sdio_async_state_t* async;
    const nx_dma_memory_region_t* dma_regions;
    size_t dma_region_count;
    uint8_t irq_priority;
    nx_stm32_sdio_protocol_t protocol;
    nx_stm32_sdio_io_state_t* io;
} nx_stm32_sdio_config_t;

/** \brief Acquired hardware effects stay live until cold close proves return.
 */
typedef struct {
    uint32_t clock_added;
    uint16_t pin_mask;
    bool owned;
    uint8_t sdio_priority;
    uint8_t dma_priority;
    bool vectors;
} nx_stm32_sdio_receipt_t;

/** \brief Session owns media; only a selected async extension borrows data. */
typedef struct {
    const nx_stm32_sdio_config_t* config;
    nx_block_geometry_t geometry;
    uint32_t cid[4];
    uint16_t rca;
    nx_block_state_t state;
    bool infrastructure;
    bool geometry_valid;
    bool high_capacity;
    bool selected;
    bool protocol_started;
    bool programming_pending;
    bool transfer_pending; /**< Issued command requires media repair. */
    bool removed;
} nx_stm32_sdio_state_t;

/** \brief One readonly table serves each statically assembled block face. */
extern const nx_block_ops_t nx_stm32_sdio_ops;
/** \brief Optional shared asynchronous face on the same controller. */
extern const nx_block_async_ops_t nx_stm32_sdio_async_ops;
/** \brief Optional SDIO function face shares this exact physical host. */
extern const nx_sdio_io_ops_t nx_stm32_sdio_io_ops;

/**
 * \brief Advance at most eight FIFO words and latch terminal data flags.
 * \param[in,out] state: Exact statically bound owner, stable through close.
 * \note Fixed IRQ only, no protocol polling, callbacks or request settlement.
 */
void nx_stm32_sdio_irq(nx_stm32_sdio_state_t* state);
/**
 * \brief Latch only DMA2 stream3 channel4 terminal observations.
 * \param[in,out] state: Exact statically bound owner, not a request pointer.
 * \note A DMA TC flag does not prove SDIO CRC, card readiness or settlement.
 */
void nx_stm32_sdio_dma_irq(nx_stm32_sdio_state_t* state);

/**
 * \brief Acquire exact pads/gate without discovering or starting a card.
 * \param[in,out] state: Fresh zero storage, disjoint from static resources.
 * \param[in] config: Immutable reviewed pins, width and finite polling bounds.
 * \param[in,out] receipt: Zero entry owner, retained through failed cleanup.
 * \return Success or refusal with any partial effects in the same receipt.
 * \note Privileged unmasked Thread startup. Platform already owns GPIOC/D (and
 * GPIOB for MMC8) clocks and the selected static ClockPlan: the normal kernel
 * is 48 MHz; only the exact ES0182 HS375 profile permits 75 MHz with a
 * non-bypass 37.5 MHz wire clock. PCLK2 supports an even divider satisfying
 * RM0090
 * section 31.2 and ES0182 section 2.4.6 for the selected bus width.
 * The finite engine leases fixed IRQ vectors. Platform owns
 *       the DMA2 gate before DMA preparation; only submit enables a stream.
 *       This owner does not enable USB, discover media or create a task.
 */
nx_result_t nx_stm32_sdio_prepare(nx_stm32_sdio_state_t* state,
                                  const nx_stm32_sdio_config_t* config,
                                  nx_stm32_sdio_receipt_t* receipt);
/**
 * \brief Stop the card/session before restoring acquired pads and gate.
 * \param[in,out] state: Same storage, kept live through refused stop/retry.
 * \param[in] config: Same immutable resources supplied to preparation.
 * \param[in,out] receipt: Exact retained effects; empty receipt has no
 * authority over an independently active state or controller. \return Success
 * after observation and restoration, or a retained failure. \note Caller data
 * access ends at each synchronous return or acquire-observed asynchronous
 * SETTLED. Uncertain card programming is
 * separate: a deadline or clock-gate write cannot prove media completion. No
 * implicit card replacement or power cycle occurs. A TX FIFO copy can outlive
 * SETTLED without retaining caller RAM: it keeps the cold owner and makes
 * DMA submission and close return BUSY. No isolated FIFO reset is provided;
 * the caller must arrange system recovery without disrupting peer DMA owners.
 */
nx_result_t nx_stm32_sdio_close(nx_stm32_sdio_state_t* state,
                                const nx_stm32_sdio_config_t* config,
                                nx_stm32_sdio_receipt_t* receipt);

#ifdef __cplusplus
}
#endif
#endif
