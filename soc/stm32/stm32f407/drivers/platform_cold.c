/**
 * \file            platform_cold.c
 * \brief           Concrete selected admission and shared cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_platform_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool config_valid(const nx_stm32_platform_config_t* config) {
    if (config == NULL || (config->pin_count != 0U && config->pins == NULL) ||
        config->pin_count > 144U ||
        (config->dma_stream_count != 0U && config->dma_streams == NULL) ||
        config->dma_stream_count > 16U)
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->registers == NULL || pin->snapshot == NULL ||
            pin->port_index > 8U || pin->pin > 15U)
            return false;
    }
    for (size_t i = 0U; i < config->dma_stream_count; ++i) {
        if (config->dma_streams[i] == NULL)
            return false;
    }
    return true;
}

static uint32_t gpio_mask(const nx_stm32_platform_config_t* config) {
    uint32_t mask = 0U;
    for (size_t i = 0U; i < config->pin_count; ++i)
        mask |= 1U << config->pins[i].port_index;
    return mask;
}

nx_result_t
nx_stm32_platform_pins_admit(const nx_stm32_platform_config_t* config,
                             bool powered_only) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (powered_only && (RCC->AHB1ENR & (1U << pin->port_index)) == 0U)
            continue;
        uint32_t mode = (pin->registers->MODER >> (pin->pin * 2U)) & 3U;
        if ((mode != 0U && mode != 3U) ||
            (pin->registers->LCKR & (1U << pin->pin)) != 0U)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_platform_selected_preflight(const nx_stm32_platform_config_t* config) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    if ((RCC->APB1ENR & config->apb1_clocks) != 0U ||
        (RCC->APB2ENR & config->apb2_clocks) != 0U ||
        (RCC->AHB1ENR & config->ahb1_clocks) != 0U ||
        (RCC->AHB2ENR & config->ahb2_clocks) != 0U)
        return NX_ERROR_BUSY;
    if (config->exti_lines != 0U &&
        ((EXTI->IMR | EXTI->EMR | EXTI->PR) & config->exti_lines) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < 3U; ++i) {
        if (config->irq_masks[i] != 0U &&
            ((NVIC->ISER[i] | NVIC->ISPR[i] | NVIC->IABR[i]) &
             config->irq_masks[i]) != 0U)
            return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < config->dma_stream_count; ++i) {
        if ((config->dma_streams[i]->CR & DMA_SxCR_EN) != 0U)
            return NX_ERROR_BUSY;
    }
    return nx_stm32_platform_pins_admit(config, true);
}

nx_result_t nx_stm32_platform_gpio_clocks_release(
    nx_stm32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL || (receipt->clocks_added & ~UINT32_C(0x1FF)) != 0U)
        return NX_ERROR_INVALID;
    if (receipt->captured)
        return NX_ERROR_BUSY;
    if (receipt->clocks_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,
                                   RCC->AHB1ENR & ~receipt->clocks_added);
        nx_arch_dsb();
        if ((RCC->AHB1ENR & receipt->clocks_added) != 0U)
            return NX_ERROR_IO;
        receipt->clocks_added = 0U;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_platform_pins_acquire(const nx_stm32_platform_config_t* config,
                               nx_stm32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured || receipt->clocks_added != 0U)
        return NX_ERROR_STATE;
    uint32_t mask = gpio_mask(config);
    receipt->clocks_added = ~RCC->AHB1ENR & mask;
    if (receipt->clocks_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR, RCC->AHB1ENR | mask);
        nx_arch_dsb();
    }
    if ((RCC->AHB1ENR & mask) != mask)
        return NX_ERROR_IO;
    nx_result_t status = nx_stm32_platform_pins_admit(config, false);
    if (status != NX_SUCCESS)
        return status;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
    }
    receipt->captured = true;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_platform_pins_abort(nx_stm32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    /* Only the failed system path may discard untouched pad observations. */
    receipt->captured = false;
    return nx_stm32_platform_gpio_clocks_release(receipt);
}

nx_result_t
nx_stm32_platform_pins_release(const nx_stm32_platform_config_t* config,
                               nx_stm32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured) {
        /* Withdraw every remaining selected driver before restoring any latch.
         * A refused withdrawal leaves all snapshots available for retry. */
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
            uint32_t mask = 3U << (pin->pin * 2U);
            uint32_t mode = pin->registers->MODER & mask;
            if (mode != 0U && mode != mask) {
                NX_PLATFORM_REGISTER_WRITE(pin->registers->MODER,
                                           pin->registers->MODER | mask);
                nx_arch_dsb();
                if ((pin->registers->MODER & mask) != mask)
                    return NX_ERROR_IO;
            }
        }
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
            if (nx_stm32_pin_capture(pin->registers, pin->pin) !=
                *pin->snapshot) {
                nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
                nx_arch_dsb();
                if (nx_stm32_pin_capture(pin->registers, pin->pin) !=
                    *pin->snapshot)
                    return NX_ERROR_IO;
            }
        }
        receipt->captured = false;
    }
    return nx_stm32_platform_gpio_clocks_release(receipt);
}

nx_result_t nx_stm32_platform_dma_acquire(uint32_t mask, uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    uint32_t allowed = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    if (added == NULL || mask == 0U || (mask & ~allowed) != 0U)
        return NX_ERROR_INVALID;
    if (*added != 0U)
        return NX_ERROR_STATE;
    *added = ~RCC->AHB1ENR & mask;
    NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR, RCC->AHB1ENR | mask);
    nx_arch_dsb();
    return (RCC->AHB1ENR & mask) == mask ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32_platform_dma_release(uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    uint32_t allowed = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
    if (added == NULL || (*added & ~allowed) != 0U)
        return NX_ERROR_INVALID;
    if (*added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR, RCC->AHB1ENR & ~*added);
        nx_arch_dsb();
        if ((RCC->AHB1ENR & *added) != 0U)
            return NX_ERROR_IO;
        *added = 0U;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_platform_syscfg_acquire(uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (added == NULL)
        return NX_ERROR_INVALID;
    if (*added != 0U)
        return NX_ERROR_STATE;
    *added = ~RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR,
                               RCC->APB2ENR | RCC_APB2ENR_SYSCFGEN);
    nx_arch_dsb();
    return (RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) != 0U ? NX_SUCCESS
                                                       : NX_ERROR_IO;
}

nx_result_t nx_stm32_platform_syscfg_release(uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (added == NULL || (*added & ~(uint32_t)RCC_APB2ENR_SYSCFGEN) != 0U)
        return NX_ERROR_INVALID;
    if (*added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR & ~*added);
        nx_arch_dsb();
        if ((RCC->APB2ENR & *added) != 0U)
            return NX_ERROR_IO;
        *added = 0U;
    }
    return NX_SUCCESS;
}
