/**
 * \file            platform_cold.c
 * \brief           Concrete selected admission and shared cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_platform_cold.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "nexus/arch/arch.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool config_valid(const nx_gd32_platform_config_t* config) {
    if (config == NULL || (config->pin_count != 0U && config->pins == NULL) ||
        config->pin_count > 144U ||
        (config->dma_lane_count != 0U && config->dma_lanes == NULL) ||
        config->dma_lane_count > 16U)
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_gd32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->snapshot == NULL || pin->port_index > 8U || pin->pin > 15U ||
            pin->registers != GPIOA + (GPIOB - GPIOA) * pin->port_index)
            return false;
    }
    for (size_t i = 0U; i < config->dma_lane_count; ++i) {
        if ((config->dma_lanes[i].registers != DMA0 &&
             config->dma_lanes[i].registers != DMA1) ||
            config->dma_lanes[i].channel > 7U)
            return false;
    }
    return true;
}

static uint32_t gpio_mask(const nx_gd32_platform_config_t* config) {
    uint32_t mask = 0U;
    for (size_t i = 0U; i < config->pin_count; ++i)
        mask |= 1U << config->pins[i].port_index;
    return mask;
}

nx_result_t nx_gd32_platform_pins_admit(const nx_gd32_platform_config_t* config,
                                        bool powered_only) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_gd32_platform_pin_config_t* pin = &config->pins[i];
        if (powered_only && (RCU_AHB1EN & (1U << pin->port_index)) == 0U)
            continue;
        uint32_t mode = (GPIO_CTL(pin->registers) >> (pin->pin * 2U)) & 3U;
        if ((mode != 0U && mode != 3U) ||
            (GPIO_LOCK(pin->registers) & (1U << pin->pin)) != 0U)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_gd32_platform_selected_preflight(const nx_gd32_platform_config_t* config) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    if ((RCU_APB1EN & config->apb1_clocks) != 0U ||
        (RCU_APB2EN & config->apb2_clocks) != 0U ||
        (RCU_AHB1EN & config->ahb1_clocks) != 0U ||
        (RCU_AHB2EN & config->ahb2_clocks) != 0U)
        return NX_ERROR_BUSY;
    if (config->exti_lines != 0U &&
        ((EXTI_INTEN | EXTI_EVEN | EXTI_PD) & config->exti_lines) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < 3U; ++i) {
        if (config->irq_masks[i] != 0U &&
            ((NVIC->ISER[i] | NVIC->ISPR[i] | NVIC->IABR[i]) &
             config->irq_masks[i]) != 0U)
            return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < config->dma_lane_count; ++i) {
        if ((DMA_CHCTL(config->dma_lanes[i].registers,
                       config->dma_lanes[i].channel) &
             DMA_CHXCTL_CHEN) != 0U)
            return NX_ERROR_BUSY;
    }
    return nx_gd32_platform_pins_admit(config, true);
}

nx_result_t
nx_gd32_platform_gpio_clocks_release(nx_gd32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL || (receipt->clocks_added & ~UINT32_C(0x1FF)) != 0U)
        return NX_ERROR_INVALID;
    if (receipt->captured)
        return NX_ERROR_BUSY;
    if (receipt->clocks_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN,
                                   RCU_AHB1EN & ~receipt->clocks_added);
        nx_arch_dsb();
        if ((RCU_AHB1EN & receipt->clocks_added) != 0U)
            return NX_ERROR_IO;
        receipt->clocks_added = 0U;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_gd32_platform_pins_acquire(const nx_gd32_platform_config_t* config,
                              nx_gd32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured || receipt->clocks_added != 0U)
        return NX_ERROR_STATE;
    uint32_t mask = gpio_mask(config);
    receipt->clocks_added = ~RCU_AHB1EN & mask;
    if (receipt->clocks_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN | mask);
        nx_arch_dsb();
    }
    if ((RCU_AHB1EN & mask) != mask)
        return NX_ERROR_IO;
    nx_result_t status = nx_gd32_platform_pins_admit(config, false);
    if (status != NX_SUCCESS)
        return status;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_gd32_platform_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_gd32_pin_capture(pin->registers, pin->pin);
    }
    receipt->captured = true;
    return NX_SUCCESS;
}

nx_result_t
nx_gd32_platform_pins_abort(nx_gd32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    /* Only the failed system path may discard untouched pad observations. */
    receipt->captured = false;
    return nx_gd32_platform_gpio_clocks_release(receipt);
}

nx_result_t
nx_gd32_platform_pins_release(const nx_gd32_platform_config_t* config,
                              nx_gd32_platform_pin_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured) {
        /* Withdraw every remaining selected driver before restoring any latch.
         * A refused withdrawal leaves all snapshots available for retry. */
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_gd32_platform_pin_config_t* pin = &config->pins[i - 1U];
            uint32_t mask = 3U << (pin->pin * 2U);
            uint32_t mode = GPIO_CTL(pin->registers) & mask;
            if (mode != 0U && mode != mask) {
                NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(pin->registers),
                                           GPIO_CTL(pin->registers) | mask);
                nx_arch_dsb();
                if ((GPIO_CTL(pin->registers) & mask) != mask)
                    return NX_ERROR_IO;
            }
        }
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_gd32_platform_pin_config_t* pin = &config->pins[i - 1U];
            if (nx_gd32_pin_capture(pin->registers, pin->pin) !=
                *pin->snapshot) {
                nx_gd32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
                nx_arch_dsb();
                if (nx_gd32_pin_capture(pin->registers, pin->pin) !=
                    *pin->snapshot)
                    return NX_ERROR_IO;
            }
        }
        receipt->captured = false;
    }
    return nx_gd32_platform_gpio_clocks_release(receipt);
}

nx_result_t nx_gd32_platform_dma_acquire(uint32_t mask, uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    uint32_t allowed = RCU_AHB1EN_DMA0EN | RCU_AHB1EN_DMA1EN;
    if (added == NULL || mask == 0U || (mask & ~allowed) != 0U)
        return NX_ERROR_INVALID;
    if (*added != 0U)
        return NX_ERROR_STATE;
    *added = ~RCU_AHB1EN & mask;
    NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN | mask);
    nx_arch_dsb();
    return (RCU_AHB1EN & mask) == mask ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_gd32_platform_dma_release(uint32_t* added) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    uint32_t allowed = RCU_AHB1EN_DMA0EN | RCU_AHB1EN_DMA1EN;
    if (added == NULL || (*added & ~allowed) != 0U)
        return NX_ERROR_INVALID;
    if (*added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN & ~*added);
        nx_arch_dsb();
        if ((RCU_AHB1EN & *added) != 0U)
            return NX_ERROR_IO;
        *added = 0U;
    }
    return NX_SUCCESS;
}
