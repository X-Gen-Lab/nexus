/**
 * \file            platform_lifecycle.c
 * \brief           Shared clock acquisition and ordered system withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_platform_cold.h"

/** \brief Shared lifecycle calls require explicit cold Thread ownership. */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

nx_result_t nx_stm32_platform_resources_acquire(uint32_t dma_mask,
                                                uint32_t* dma_added,
                                                uint32_t* syscfg_added) {
    const uint32_t allowed = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if ((dma_mask & ~allowed) != 0U ||
        ((dma_mask == 0U) != (dma_added == NULL)) ||
        (dma_added != NULL && dma_added == syscfg_added))
        return NX_ERROR_INVALID;
    /* Check both slots before any effect; a failed prior owner is not reset. */
    if ((dma_added != NULL && *dma_added != 0U) ||
        (syscfg_added != NULL && *syscfg_added != 0U))
        return NX_ERROR_STATE;
    if (dma_added != NULL) {
        nx_result_t result = nx_stm32_platform_dma_acquire(dma_mask, dma_added);
        if (result != NX_SUCCESS)
            return result;
    }
    return syscfg_added != NULL ? nx_stm32_platform_syscfg_acquire(syscfg_added)
                                : NX_SUCCESS;
}

nx_result_t nx_stm32_platform_resources_release(uint32_t* dma_added,
                                                uint32_t* syscfg_added) {
    const uint32_t allowed = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if ((dma_added != NULL && dma_added == syscfg_added) ||
        (dma_added != NULL && (*dma_added & ~allowed) != 0U) ||
        (syscfg_added != NULL &&
         (*syscfg_added & ~(uint32_t)RCC_APB2ENR_SYSCFGEN) != 0U))
        return NX_ERROR_INVALID;
    if (syscfg_added != NULL) {
        nx_result_t result = nx_stm32_platform_syscfg_release(syscfg_added);
        if (result != NX_SUCCESS)
            return result;
    }
    return dma_added != NULL ? nx_stm32_platform_dma_release(dma_added)
                             : NX_SUCCESS;
}
