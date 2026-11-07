/**
 * \file            stm32f407_sdio.h
 * \brief           Exact F407 polling SDIO infrastructure and media ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SDIO_H
#define NX_STM32F407_SDIO_H

#include "nexus/io/block.h"
#include "stm32f407_platform_cold.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Static six-pad socket and bounded CPU-polling profile. */
typedef struct {
    SDIO_TypeDef* registers;
    const nx_stm32_platform_pin_config_t* pins;
    uint32_t max_blocks;
    uint32_t poll_budget;
    uint32_t stop_timeout_us;
    uint8_t bus_width;
} nx_stm32_sdio_config_t;

/** \brief Acquired hardware effects stay live until cold close proves return.
 */
typedef struct {
    uint32_t clock_added;
    uint8_t pin_mask;
    bool owned;
} nx_stm32_sdio_receipt_t;

/** \brief Caller-owned session has no retained payload pointer or DMA state. */
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
} nx_stm32_sdio_state_t;

/** \brief One readonly table serves each statically assembled block face. */
extern const nx_block_ops_t nx_stm32_sdio_ops;

/**
 * \brief Acquire exact pads/gate without discovering or starting a card.
 * \param[in,out] state: Fresh zero storage, disjoint from static resources.
 * \param[in] config: Immutable reviewed pins, width and finite polling bounds.
 * \param[in,out] receipt: Zero entry owner, retained through failed cleanup.
 * \return Success or refusal with any partial effects in the same receipt.
 * \note Privileged unmasked Thread startup. Platform already owns GPIOC/D
 *       clocks and the HSE8/PLL168 time domain with live 48 MHz PLLQ output.
 *       This owner never enables USB, DMA, an SDIO IRQ or a hidden task.
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
 * access ends at each synchronous I/O return. Uncertain card programming is
 * separate: a deadline or clock-gate write cannot prove media completion. No
 * implicit card replacement or power cycle occurs.
 */
nx_result_t nx_stm32_sdio_close(nx_stm32_sdio_state_t* state,
                                const nx_stm32_sdio_config_t* config,
                                nx_stm32_sdio_receipt_t* receipt);

#ifdef __cplusplus
}
#endif
#endif
