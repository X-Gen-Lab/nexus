/**
 * \file            gpio.c
 * \brief           Fixed GPIO masks without runtime lookup or locks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_GPIO_CLOCK_READ
#define NX_STM32_GPIO_CLOCK_READ(reg) (*(reg))
#endif

/** \brief Preload output levels before exposing the configured output mode. */
nx_result_t nx_stm32_gpio_initialize(nx_gpio_port_t* port, uint32_t initial) {
    if (port == NULL || port->registers == NULL || port->mask == 0U ||
        (port->mask & ~0xFFFFU) != 0U || (initial & ~port->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    GPIO_TypeDef* regs = port->registers;
    regs->BSRR = initial | ((port->mask & ~initial) << 16U);
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((port->mask & (1U << pin)) != 0U) {
            regs->MODER = (regs->MODER & ~(3U << (pin * 2U))) |
                          ((port->output ? 1U : 0U) << (pin * 2U));
            regs->OTYPER &= ~(1U << pin);
            regs->PUPDR &= ~(3U << (pin * 2U));
        }
    }
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Validate dynamic masks before the one atomic BSRR write. */
nx_result_t nx_gpio_port_write(nx_gpio_port_t* port, uint32_t set_mask,
                               uint32_t reset_mask) {
    if (port == NULL || !port->initialized || !port->output ||
        (set_mask & reset_mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (((set_mask | reset_mask) & ~port->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    port->registers->BSRR = set_mask | (reset_mask << 16U);
    return NX_SUCCESS;
}

/** \brief Return a masked single-port input snapshot. */
nx_result_t nx_gpio_port_read(const nx_gpio_port_t* port, uint32_t* value) {
    if (port == NULL || !port->initialized || value == NULL) {
        return NX_ERROR_INVALID;
    }
    *value = port->registers->IDR & port->mask;
    return NX_SUCCESS;
}

/** \brief Toggle using one ODR snapshot under the serialized writer contract.
 */
nx_result_t nx_gpio_port_toggle(nx_gpio_port_t* port, uint32_t mask) {
    if (port == NULL || !port->initialized || !port->output) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~port->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t high = port->registers->ODR & mask;
    return nx_gpio_port_write(port, mask & ~high, high);
}

/** \brief Establish GPIO clock access before the first mode/BSRR operation. */
nx_result_t nx_stm32_gpio_clock_enable(unsigned port_index) {
    if (port_index > 8U || g_nx_stm32_system.rcc == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t bit = 1U << port_index;
    g_nx_stm32_system.rcc->AHB1ENR |= bit;
    uint32_t observed =
        NX_STM32_GPIO_CLOCK_READ(&g_nx_stm32_system.rcc->AHB1ENR);
    nx_arch_dsb();
    return (observed & bit) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Bind only the reviewed USART1 PA9/PA10 route, preserving SWD pins. */
nx_result_t nx_stm32_uart1_pins_prepare(void) {
    nx_result_t result = nx_stm32_gpio_clock_enable(0U);
    if (result != NX_SUCCESS) {
        return result;
    }
    g_nx_stm32_system.rcc->APB2ENR |= RCC_APB2ENR_USART1EN;
    volatile uint32_t observed = g_nx_stm32_system.rcc->APB2ENR;
    nx_arch_dsb();
    if ((observed & RCC_APB2ENR_USART1EN) == 0U) {
        return NX_ERROR_IO;
    }
    uint32_t mode_mask = (3U << 18U) | (3U << 20U);
    GPIOA->MODER = (GPIOA->MODER & ~mode_mask) | (2U << 18U) | (2U << 20U);
    GPIOA->OTYPER &= ~((1U << 9U) | (1U << 10U));
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~mode_mask) | (2U << 18U) | (2U << 20U);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~mode_mask) | (1U << 20U);
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((15U << 4U) | (15U << 8U))) |
                    (7U << 4U) | (7U << 8U);
    return NX_SUCCESS;
}

/** \brief Preserve declared idle output while withdrawing software access. */
nx_result_t nx_stm32_gpio_stop(nx_gpio_port_t* port, uint32_t inactive) {
    if (port == NULL || (inactive & ~port->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (!port->initialized) {
        return NX_SUCCESS;
    }
    if (!port->output) {
        port->initialized = false;
        return NX_SUCCESS;
    }
    nx_result_t result =
        nx_gpio_port_write(port, inactive, port->mask & ~inactive);
    if (result == NX_SUCCESS) {
        port->initialized = false;
    }
    return result;
}

/** \brief Configure only explicit reviewed electrical fields, preserving peers.
 */
nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                   uint8_t mode, uint8_t af, uint8_t pull,
                                   bool open_drain) {
    if (gpio == NULL || pin > 15U || mode > 3U || af > 15U || pull > 2U) {
        return NX_ERROR_INVALID;
    }
    uint32_t mode_shift = (uint32_t)pin * 2U;
    uint32_t af_shift = ((uint32_t)pin % 8U) * 4U;
    gpio->AFR[pin / 8U] =
        (gpio->AFR[pin / 8U] & ~(15U << af_shift)) | ((uint32_t)af << af_shift);
    gpio->PUPDR =
        (gpio->PUPDR & ~(3U << mode_shift)) | ((uint32_t)pull << mode_shift);
    gpio->OTYPER =
        (gpio->OTYPER & ~(1U << pin)) | (open_drain ? 1U << pin : 0U);
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3U << mode_shift)) | (2U << mode_shift);
    gpio->MODER =
        (gpio->MODER & ~(3U << mode_shift)) | ((uint32_t)mode << mode_shift);
    return NX_SUCCESS;
}

/** \brief Pack one pin's mode/type/speed/pull/AF/output state in twelve bits.
 */
uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio, uint8_t pin) {
    if (gpio == NULL || pin > 15U) {
        return 0U;
    }
    uint32_t shift = (uint32_t)pin * 2U;
    uint32_t state =
        ((gpio->MODER >> shift) & 3U) | (((gpio->OTYPER >> pin) & 1U) << 2U) |
        (((gpio->OSPEEDR >> shift) & 3U) << 3U) |
        (((gpio->PUPDR >> shift) & 3U) << 5U) |
        (((gpio->AFR[pin / 8U] >> ((uint32_t)pin % 8U * 4U)) & 15U) << 7U) |
        (((gpio->ODR >> pin) & 1U) << 11U);
    return (uint16_t)state;
}

/** \brief Preload the captured level before restoring its electrical mode. */
void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin, uint16_t state) {
    if (gpio == NULL || pin > 15U) {
        return;
    }
    uint32_t bit = 1U << pin;
    uint32_t shift = (uint32_t)pin * 2U;
    uint32_t af_shift = (uint32_t)pin % 8U * 4U;
    gpio->BSRR = (state & (1U << 11U)) != 0U ? bit : bit << 16U;
    gpio->AFR[pin / 8U] = (gpio->AFR[pin / 8U] & ~(15U << af_shift)) |
                          (((uint32_t)state >> 7U & 15U) << af_shift);
    gpio->OTYPER =
        (gpio->OTYPER & ~bit) | (((uint32_t)state >> 2U & 1U) << pin);
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3U << shift)) |
                    (((uint32_t)state >> 3U & 3U) << shift);
    gpio->PUPDR = (gpio->PUPDR & ~(3U << shift)) |
                  (((uint32_t)state >> 5U & 3U) << shift);
    gpio->MODER =
        (gpio->MODER & ~(3U << shift)) | (((uint32_t)state & 3U) << shift);
    nx_arch_dsb();
}
