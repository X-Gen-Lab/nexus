/**
 * \file            stm32f4_platform_lifecycle.h
 * \brief           Fixed F4 system and shared-resource lifecycle connections.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_PLATFORM_LIFECYCLE_H
#define NEXUS_STM32F4_PLATFORM_LIFECYCLE_H

#include "nexus/core/platform_lifecycle.h"
#include "stm32f407_packet_cold.h"

/** \brief Optional Ethernet receipt; its concrete owner supplies the type. */
typedef struct nx_stm32_eth_resource_receipt nx_stm32_eth_resource_receipt_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Fixed shared-pin owner roles, selected only with pads.
 * \note            The immutable table connects the concrete C pad owner.
 *                  Omit the whole table for pinless selections so no GPIO
 *                  function relocation or mutable receipt is introduced.
 */
typedef struct {
    nx_result_t (*preflight)(const nx_stm32_platform_config_t* config);
    nx_result_t (*acquire)(const nx_stm32_platform_config_t* config,
                           nx_stm32_platform_pin_receipt_t* receipt);
    nx_result_t (*release_clocks)(nx_stm32_platform_pin_receipt_t* receipt);
    nx_result_t (*abort)(nx_stm32_platform_pin_receipt_t* receipt);
    nx_result_t (*release)(const nx_stm32_platform_config_t* config,
                           nx_stm32_platform_pin_receipt_t* receipt);
} nx_stm32f4_platform_pin_owner_t;

/**
 * \brief           Fixed DMA/SYSCFG owner roles, omitted when neither is used.
 * \note            Function addresses appear only for selected shared clocks.
 *                  The existing added-bit receipts remain their sole mutable
 *                  state; this table is readonly and never describes MMIO.
 */
typedef struct {
    nx_result_t (*acquire)(uint32_t dma_mask, uint32_t* dma_added,
                           uint32_t* syscfg_added);
    nx_result_t (*release)(uint32_t* dma_added, uint32_t* syscfg_added);
} nx_stm32f4_platform_clock_owner_t;

/**
 * \brief           Immutable connections to the one selected F4 owner set.
 * \note            Optional callbacks have fixed hardware-owner roles, never
 *                  opcodes or an action list. NULL hooks omit an owner and
 *                  its link dependency; Ethernet uses a paired state/hook set.
 *                  Existing concrete
 *                  receipts are referenced, never copied into another state.
 */
typedef struct {
    nx_stm32_system_t* system;
    const nx_stm32_platform_config_t* selected;
    nx_stm32_platform_pin_receipt_t* pins;
    const nx_stm32f4_platform_pin_owner_t* pin_owner;
    const nx_stm32f4_platform_clock_owner_t* clock_owner;
    uint32_t flash_bytes;
    uint32_t poll_limit;
    uint32_t dma_mask;
    uint32_t* dma_added;
    uint32_t* syscfg_added;
    nx_result_t (*permanent_stop)(void);
    nx_result_t (*can_close)(void);
    nx_stm32_eth_resource_receipt_t* ethernet;
    void (*ethernet_capture)(nx_stm32_eth_resource_receipt_t* receipt,
                             bool frames);
    nx_result_t (*ethernet_close)(nx_stm32_eth_resource_receipt_t* receipt);
    bool ethernet_frames;
} nx_stm32f4_platform_config_t;

/**
 * \brief           Admit identity/resources and establish the selected system.
 * \param[in]       config: Stable readonly connections with unique receipts.
 * \return          Primary, cleanup and remaining effects independently.
 * \note            Privileged unmasked Thread; caller serializes the complete
 *                  Platform. Base/selected admission precedes pin acquisition.
 *                  Failed system startup aborts untouched pin observations;
 *                  failed cleanup retains the same receipts for explicit stop.
 */
nx_platform_start_result_t
nx_stm32f4_platform_system_start(const nx_stm32f4_platform_config_t* config);

/**
 * \brief           Capture packet ownership and acquire shared DMA/SYSCFG.
 * \param[in]       config: Same immutable connections used for system start.
 * \return          Success or failure retaining all attempted clock effects.
 * \note            Privileged unmasked Thread, after system establishment.
 *                  Failure requires the Platform engine's reverse cleanup;
 *                  this function never hides the original acquisition error.
 */
nx_result_t nx_stm32f4_platform_resources_acquire(
    const nx_stm32f4_platform_config_t* config);

/**
 * \brief           Withdraw permanent guards, domains, pads and shared clocks.
 * \param[in]       config: Same connections; all selected controllers joined.
 * \return          First refusal, preserving later dependencies for retry.
 * \note            Privileged unmasked Thread. Startup SRAM remains image-owned
 *                  and its guard runs before CAN, Ethernet, pads and clocks.
 *                  No concrete receipt is reset to escape failed withdrawal.
 */
nx_result_t nx_stm32f4_platform_resources_release(
    const nx_stm32f4_platform_config_t* config);

/**
 * \brief           Stop the system only after shared-resource withdrawal.
 * \param[in]       config: Same static system owner and bounded poll count.
 * \param[in]       started: Actual system effects retained by this Platform.
 * \return          Success or failure retaining unresolved system ownership.
 * \note            Privileged unmasked Thread; never stops a foreign system.
 */
nx_result_t
nx_stm32f4_platform_system_release(const nx_stm32f4_platform_config_t* config,
                                   bool started);

#ifdef __cplusplus
}
#endif
#endif
