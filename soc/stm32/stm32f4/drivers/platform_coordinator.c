/**
 * \file            platform_coordinator.c
 * \brief           F4 system startup and fixed shared-owner withdrawal order.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_platform_lifecycle.h"

/** \brief Validate all optional connections before invoking any owner. */
static bool valid_config(const nx_stm32f4_platform_config_t* config) {
    if (config == NULL || config->system == NULL || config->flash_bytes == 0U ||
        config->poll_limit == 0U ||
        ((config->selected == NULL) != (config->pins == NULL)) ||
        ((config->selected == NULL) != (config->pin_owner == NULL)) ||
        ((config->dma_mask == 0U) != (config->dma_added == NULL)) ||
        (config->dma_added != NULL &&
         config->dma_added == config->syscfg_added) ||
        ((config->ethernet == NULL) != (config->ethernet_capture == NULL)) ||
        ((config->ethernet == NULL) != (config->ethernet_close == NULL)))
        return false;
    const nx_stm32f4_platform_pin_owner_t* pins = config->pin_owner;
    if (pins != NULL && (pins->preflight == NULL || pins->acquire == NULL ||
                         pins->release_clocks == NULL || pins->abort == NULL ||
                         pins->release == NULL))
        return false;
    const nx_stm32f4_platform_clock_owner_t* clocks = config->clock_owner;
    bool selected_clocks =
        config->dma_added != NULL || config->syscfg_added != NULL;
    if (selected_clocks != (clocks != NULL) ||
        (clocks != NULL &&
         (clocks->acquire == NULL || clocks->release == NULL)))
        return false;
    const uint32_t allowed = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    return (config->dma_mask & ~allowed) == 0U;
}

/** \brief Never change execution context while admitting a cold transaction. */
static nx_result_t admit(const nx_stm32f4_platform_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    return valid_config(config) ? NX_SUCCESS : NX_ERROR_INVALID;
}

nx_platform_start_result_t
nx_stm32f4_platform_system_start(const nx_stm32f4_platform_config_t* config) {
    nx_platform_start_result_t result = {admit(config), NX_SUCCESS, 0U};
    if (result.primary != NX_SUCCESS)
        return result;
    result.primary = nx_stm32_system_preflight(config->system);
    if (result.primary == NX_SUCCESS && config->pin_owner != NULL)
        result.primary = config->pin_owner->preflight(config->selected);
    if (result.primary == NX_SUCCESS)
        result.primary = nx_stm32_validate_identity(config->flash_bytes);
    if (result.primary != NX_SUCCESS)
        return result;
    if (config->pins != NULL) {
        result.primary =
            config->pin_owner->acquire(config->selected, config->pins);
        if (result.primary != NX_SUCCESS) {
            result.cleanup = config->pin_owner->release_clocks(config->pins);
            result.remaining_effects = result.cleanup == NX_SUCCESS ? 0U : 1U;
            return result;
        }
    }
    nx_stm32_start_result_t system =
        nx_stm32_system_start(config->system, config->poll_limit);
    if (system.primary == 0)
        return result;
    result.primary = NX_ERROR_IO;
    result.cleanup = system.cleanup == 0 ? NX_SUCCESS : NX_ERROR_IO;
    result.remaining_effects = system.remaining_effects;
    if (config->pins != NULL) {
        nx_result_t pins = config->pin_owner->abort(config->pins);
        if (pins != NX_SUCCESS) {
            result.cleanup = pins;
            result.remaining_effects |= 1U;
        }
    }
    return result;
}

nx_result_t nx_stm32f4_platform_resources_acquire(
    const nx_stm32f4_platform_config_t* config) {
    nx_result_t result = admit(config);
    if (result != NX_SUCCESS)
        return result;
    if (config->ethernet != NULL)
        config->ethernet_capture(config->ethernet, config->ethernet_frames);
    return config->clock_owner == NULL
               ? NX_SUCCESS
               : config->clock_owner->acquire(
                     config->dma_mask, config->dma_added, config->syscfg_added);
}

nx_result_t nx_stm32f4_platform_resources_release(
    const nx_stm32f4_platform_config_t* config) {
    nx_result_t result = admit(config);
    if (result != NX_SUCCESS)
        return result;
    /* The permanent guard proves no image-owned SRAM will be withdrawn. */
    if (config->permanent_stop != NULL) {
        result = config->permanent_stop();
        if (result != NX_SUCCESS)
            return result;
    }
    if (config->can_close != NULL) {
        result = config->can_close();
        if (result != NX_SUCCESS)
            return result;
    }
    if (config->ethernet != NULL) {
        result = config->ethernet_close(config->ethernet);
        if (result != NX_SUCCESS)
            return result;
    }
    if (config->pins != NULL) {
        result = config->pin_owner->release(config->selected, config->pins);
        if (result != NX_SUCCESS)
            return result;
    }
    return config->clock_owner == NULL
               ? NX_SUCCESS
               : config->clock_owner->release(config->dma_added,
                                              config->syscfg_added);
}

nx_result_t
nx_stm32f4_platform_system_release(const nx_stm32f4_platform_config_t* config,
                                   bool started) {
    nx_result_t result = admit(config);
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32_platform_system_release(config->system, started,
                                            config->poll_limit);
}
