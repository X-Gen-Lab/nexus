/**
 * \file            gpio.c
 * \brief           Fixed GPIO masks without runtime lookup or locks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_basic_cold.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_gpio.h"

_Static_assert(LL_GPIO_PULL_NO == 0U && LL_GPIO_PULL_UP == 1U &&
                   LL_GPIO_PULL_DOWN == 2U && LL_GPIO_OUTPUT_PUSHPULL == 0U &&
                   LL_GPIO_OUTPUT_OPENDRAIN == 1U,
               "Reviewed GPIO electrical facts differ from the selected SDK");
_Static_assert(LL_GPIO_SPEED_FREQ_LOW == 0U &&
                   LL_GPIO_SPEED_FREQ_MEDIUM == 1U &&
                   LL_GPIO_SPEED_FREQ_HIGH == 2U &&
                   LL_GPIO_SPEED_FREQ_VERY_HIGH == 3U,
               "GPIO slew selection must match the selected LL encodings");

#ifndef NX_STM32_GPIO_CLOCK_READ
#define NX_STM32_GPIO_CLOCK_READ(reg) (*(reg))
#endif
#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

static nx_result_t initialize_electrical(nx_stm32_gpio_state_t* port,
                                         uint32_t initial, uint8_t pull,
                                         uint8_t output_type, uint8_t speed);

/** \brief Match a GPIO bank without acquiring its bus clock. */
static unsigned gpio_bank_index(const GPIO_TypeDef* registers) {
    for (unsigned index = 0U; index < 9U; ++index) {
        const GPIO_TypeDef* bank =
            (const GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE + (uintptr_t)index *
                                                              UINT32_C(0x400));
        if (registers == bank)
            return index;
    }
#ifdef NEXUS_STM32_MODEL
    if (registers == GPIOA)
        return 0U;
#endif
    return 9U;
}

/** \brief Validate every GPIO argument before clock or state changes. */
nx_result_t nx_stm32_gpio_admit(const nx_stm32_gpio_state_t* port,
                                const nx_stm32_gpio_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->mask == 0U ||
        (config->mask & ~UINT32_C(0xFFFF)) != 0U ||
        (config->initial & ~config->mask) != 0U || config->pull > 2U ||
        config->output_type > 1U ||
        config->speed > NX_STM32_GPIO_SPEED_VERY_HIGH ||
        (!config->output && (config->output_type != 0U ||
                             config->speed != NX_STM32_GPIO_SPEED_PRESERVE)) ||
        (config->output && config->registers == GPIOC &&
         (config->mask & UINT32_C(0xE000)) != 0U &&
         config->speed != NX_STM32_GPIO_SPEED_PRESERVE &&
         config->speed != NX_STM32_GPIO_SPEED_LOW))
        return NX_ERROR_INVALID;
    if (port->initialized)
        return NX_ERROR_BUSY;
    if (gpio_bank_index(config->registers) == 9U ||
        g_nx_stm32_system.rcc == NULL)
        return NX_ERROR_INVALID;
    if (config->registers == GPIOC && (config->mask & (1U << 13U)) != 0U &&
        (g_nx_stm32_system.rcc->BDCR & RCC_BDCR_LSEBYP) == 0U &&
        (g_nx_stm32_system.rcc->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) !=
            0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief Acquire the admitted bank before applying declared electrical facts.
 */
nx_result_t nx_stm32_gpio_prepare(nx_stm32_gpio_state_t* port,
                                  const nx_stm32_gpio_config_t* config) {
    nx_result_t result = nx_stm32_gpio_admit(port, config);
    if (result != NX_SUCCESS)
        return result;
    unsigned index = gpio_bank_index(config->registers);
    uint32_t bit = 1U << index;
    uint32_t saved_clock = g_nx_stm32_system.rcc->AHB1ENR & bit;
    result = nx_stm32_gpio_clock_enable(index);
    if (result != NX_SUCCESS) {
        g_nx_stm32_system.rcc->AHB1ENR =
            (g_nx_stm32_system.rcc->AHB1ENR & ~bit) | saved_clock;
        nx_arch_dsb();
        return result;
    }
    port->registers = config->registers;
    port->mask = config->mask;
    port->output = config->output;
    /* Argument rejection precedes pin writes. Hardware refusal retains the
     * Platform pin receipt/clock for whole-resource rollback. */
    return initialize_electrical(port, config->initial, config->pull,
                                 config->output_type, config->speed);
}

/** \brief Preload output levels before exposing the configured output mode. */
static nx_result_t initialize_electrical(nx_stm32_gpio_state_t* port,
                                         uint32_t initial, uint8_t pull,
                                         uint8_t output_type, uint8_t speed) {
    if (port == NULL || port->registers == NULL || port->mask == 0U ||
        (port->mask & ~0xFFFFU) != 0U || (initial & ~port->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (port->initialized) {
        return NX_ERROR_BUSY;
    }
    GPIO_TypeDef* regs = port->registers;
    NX_PLATFORM_REGISTER_WRITE(regs->BSRR,
                               initial | ((port->mask & ~initial) << 16U));
    if (port->output) {
        nx_arch_dsb();
        if ((LL_GPIO_ReadOutputPort(regs) & port->mask) != initial)
            return NX_ERROR_IO;
    }
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((port->mask & (1U << pin)) != 0U) {
            uint32_t selected = 1U << pin;
            LL_GPIO_SetPinOutputType(regs, selected, output_type);
            LL_GPIO_SetPinPull(regs, selected, pull);
            nx_arch_dsb();
            if (LL_GPIO_GetPinOutputType(regs, selected) != output_type ||
                LL_GPIO_GetPinPull(regs, selected) != pull)
                return NX_ERROR_IO;
            bool restricted = regs == GPIOC && pin >= 13U;
            if (port->output &&
                (speed != NX_STM32_GPIO_SPEED_PRESERVE || restricted)) {
                /* DS8626 limits PC13/14/15 to LOW output speed. Explicit
                 * choices are admitted before effects; zero keeps old intent.
                 */
                uint32_t selected_speed =
                    restricted ? LL_GPIO_SPEED_FREQ_LOW
                               : (uint32_t)speed - NX_STM32_GPIO_SPEED_LOW;
                LL_GPIO_SetPinSpeed(regs, selected, selected_speed);
                nx_arch_dsb();
                if (LL_GPIO_GetPinSpeed(regs, selected) != selected_speed)
                    return NX_ERROR_IO;
            }
            uint32_t mode =
                port->output ? LL_GPIO_MODE_OUTPUT : LL_GPIO_MODE_INPUT;
            LL_GPIO_SetPinMode(regs, selected, mode);
            nx_arch_dsb();
            if (LL_GPIO_GetPinMode(regs, selected) != mode)
                return NX_ERROR_IO;
        }
    }
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Keep existing internal users on explicit push-pull/no-pull facts. */
nx_result_t nx_stm32_gpio_initialize(nx_stm32_gpio_state_t* port,
                                     uint32_t initial) {
    return initialize_electrical(port, initial, 0U, 0U,
                                 NX_STM32_GPIO_SPEED_PRESERVE);
}

/** \brief Validate dynamic masks before the one atomic BSRR write. */
nx_result_t nx_stm32_gpio_write(void* context, uint32_t set_mask,
                                uint32_t reset_mask) {
    nx_stm32_gpio_state_t* port = context;
    if (port == NULL || !port->initialized || !port->output ||
        (set_mask & reset_mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (((set_mask | reset_mask) & ~port->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    NX_PLATFORM_REGISTER_WRITE(port->registers->BSRR,
                               set_mask | (reset_mask << 16U));
    return NX_SUCCESS;
}

/** \brief Return a masked single-port input snapshot. */
nx_result_t nx_stm32_gpio_read(const void* context, uint32_t* value) {
    const nx_stm32_gpio_state_t* port = context;
    if (port == NULL || !port->initialized || value == NULL ||
        port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    /* The complete SDK register struct is owned by this binding.
     * Reject output aliases before any input observation or output store. */
    const uintptr_t output = (uintptr_t)value;
    const uintptr_t state = (uintptr_t)port;
    const uintptr_t bank = (uintptr_t)port->registers;
    if (output % _Alignof(uint32_t) != 0U ||
        output > UINTPTR_MAX - sizeof(*value) ||
        state > UINTPTR_MAX - sizeof(*port) ||
        bank > UINTPTR_MAX - sizeof(*port->registers) ||
        (output < state + sizeof(*port) && state < output + sizeof(*value)) ||
        (output < bank + sizeof(*port->registers) &&
         bank < output + sizeof(*value))) {
        return NX_ERROR_INVALID;
    }
    *value = LL_GPIO_ReadInputPort(port->registers) & port->mask;
    return NX_SUCCESS;
}

/** \brief Toggle using one ODR snapshot under the serialized writer contract.
 */
nx_result_t nx_stm32_gpio_toggle(void* context, uint32_t mask) {
    nx_stm32_gpio_state_t* port = context;
    if (port == NULL || !port->initialized || !port->output) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~port->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t high = LL_GPIO_ReadOutputPort(port->registers) & mask;
    return nx_stm32_gpio_write(port, mask & ~high, high);
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
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_9, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_9 | LL_GPIO_PIN_10,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_9, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_10, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_9, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_10, LL_GPIO_PULL_UP);
    LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_9, LL_GPIO_AF_7);
    LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_10, LL_GPIO_AF_7);
    return NX_SUCCESS;
}

/** \brief Preserve declared idle output while withdrawing software access. */
nx_result_t nx_stm32_gpio_stop(nx_stm32_gpio_state_t* port, uint32_t inactive) {
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
        nx_stm32_gpio_write(port, inactive, port->mask & ~inactive);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_dsb();
    if ((LL_GPIO_ReadOutputPort(port->registers) & port->mask) != inactive)
        return NX_ERROR_IO;
    port->initialized = false;
    return NX_SUCCESS;
}

/** \brief Configure only explicit reviewed electrical fields, preserving peers.
 */
nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                   uint8_t mode, uint8_t af, uint8_t pull,
                                   bool open_drain) {
    if (gpio == NULL || pin > 15U || mode > 3U || af > 15U || pull > 2U) {
        return NX_ERROR_INVALID;
    }
    uint32_t selected = 1U << pin;
    if (pin < 8U) {
        LL_GPIO_SetAFPin_0_7(gpio, selected, af);
    } else {
        LL_GPIO_SetAFPin_8_15(gpio, selected, af);
    }
    LL_GPIO_SetPinPull(gpio, selected, pull);
    LL_GPIO_SetPinOutputType(gpio, selected,
                             open_drain ? LL_GPIO_OUTPUT_OPENDRAIN
                                        : LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(gpio, selected, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinMode(gpio, selected, mode);
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
    NX_PLATFORM_REGISTER_WRITE(gpio->BSRR,
                               (state & (1U << 11U)) != 0U ? bit : bit << 16U);
    uint32_t af = (uint32_t)state >> 7U & 15U;
    if (pin < 8U) {
        LL_GPIO_SetAFPin_0_7(gpio, bit, af);
    } else {
        LL_GPIO_SetAFPin_8_15(gpio, bit, af);
    }
    LL_GPIO_SetPinOutputType(gpio, bit, (uint32_t)state >> 2U & 1U);
    LL_GPIO_SetPinSpeed(gpio, bit, (uint32_t)state >> 3U & 3U);
    LL_GPIO_SetPinPull(gpio, bit, (uint32_t)state >> 5U & 3U);
    LL_GPIO_SetPinMode(gpio, bit, (uint32_t)state & 3U);
    nx_arch_dsb();
}

/** \brief One shared immutable method table for this execution mode. */
const nx_gpio_ops_t nx_stm32_gpio_ops = {
    .write = nx_stm32_gpio_write,
    .read = nx_stm32_gpio_read,
    .toggle = nx_stm32_gpio_toggle,
};
