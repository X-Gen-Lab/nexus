/**
 * \file            timer.c
 * \brief           General-purpose fixed-base TIM3 PWM without capture or DMA
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

/** \brief Locate the selected fixed channel compare register. */
static volatile uint32_t* compare(nx_pwm_port_t* port) {
    switch (port->channel) {
        case 1U:
            return &port->registers->CCR1;
        case 2U:
            return &port->registers->CCR2;
        case 3U:
            return &port->registers->CCR3;
        default:
            return &port->registers->CCR4;
    }
}

/** \brief Initialize one base only; another channel cannot silently reconfigure
 * it. */
nx_result_t nx_stm32_pwm_initialize(nx_pwm_port_t* port) {
    if (port == NULL || port->registers == NULL ||
        port->inactive_gpio == NULL || port->channel == 0U ||
        port->channel > 4U || port->period_ticks == 0U ||
        port->period_ticks > 65535U || port->duty_ticks > port->period_ticks ||
        port->tick_hz == 0U || port->inactive_mask == 0U) {
        return NX_ERROR_INVALID;
    }
    TIM_TypeDef* regs = port->registers;
    uint32_t enable = 1U << (((uint32_t)port->channel - 1U) * 4U);
    if ((regs->CCER & enable) != 0U) {
        return NX_ERROR_BUSY;
    }
    if ((regs->CR1 & TIM_CR1_CEN) != 0U &&
        (regs->PSC != port->prescaler ||
         regs->ARR != port->period_ticks - 1U)) {
        return NX_ERROR_BUSY;
    }
    if ((regs->CR1 & TIM_CR1_CEN) == 0U) {
        regs->CR1 = TIM_CR1_ARPE;
        regs->PSC = port->prescaler;
        regs->ARR = port->period_ticks - 1U;
        regs->EGR = TIM_EGR_UG;
        regs->SR = 0U;
    }
    volatile uint32_t* mode = port->channel <= 2U ? &regs->CCMR1 : &regs->CCMR2;
    uint32_t shift = ((uint32_t)port->channel - 1U) % 2U * 8U;
    *mode = (*mode & ~(0xFFU << shift)) | (0x68U << shift);
    *compare(port) = port->duty_ticks;
    regs->CCER &= ~(15U << (((uint32_t)port->channel - 1U) * 4U));
    port->initialized = true;
    port->running = false;
    return NX_SUCCESS;
}

/** \brief Stage only duty against the immutable shared period/PSC
 * configuration. */
nx_result_t nx_pwm_port_set(nx_pwm_port_t* port, uint32_t period_ticks,
                            uint32_t duty_ticks) {
    if (port == NULL || !port->initialized || period_ticks == 0U ||
        period_ticks > 65535U || duty_ticks > period_ticks) {
        return NX_ERROR_INVALID;
    }
    if (period_ticks != port->period_ticks ||
        port->registers->ARR != period_ticks - 1U ||
        port->registers->PSC != port->prescaler) {
        return NX_ERROR_STATE;
    }
    *compare(port) = duty_ticks;
    port->duty_ticks = duty_ticks;
    return NX_SUCCESS;
}

/** \brief Connect the initialized pin to AF2 before enabling its channel. */
nx_result_t nx_pwm_port_start(nx_pwm_port_t* port) {
    if (port == NULL || !port->initialized ||
        !port->inactive_gpio->initialized) {
        return NX_ERROR_STATE;
    }
    GPIO_TypeDef* gpio = port->inactive_gpio->registers;
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((port->inactive_mask & (1U << pin)) != 0U) {
            unsigned shift = (pin % 8U) * 4U;
            gpio->AFR[pin / 8U] =
                (gpio->AFR[pin / 8U] & ~(15U << shift)) | (2U << shift);
            gpio->MODER =
                (gpio->MODER & ~(3U << (pin * 2U))) | (2U << (pin * 2U));
        }
    }
    if ((port->registers->CR1 & TIM_CR1_CEN) == 0U) {
        port->registers->EGR = TIM_EGR_UG;
    }
    port->registers->CCER |= 1U << (((uint32_t)port->channel - 1U) * 4U);
    port->registers->CR1 |= TIM_CR1_CEN;
    port->running = true;
    return NX_SUCCESS;
}

/** \brief Disable only this channel and restore its declared GPIO idle level.
 */
nx_result_t nx_pwm_port_stop(nx_pwm_port_t* port) {
    if (port == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    port->registers->CCER &= ~(1U << (((uint32_t)port->channel - 1U) * 4U));
    nx_result_t result = nx_gpio_port_write(
        port->inactive_gpio, port->inactive_high ? port->inactive_mask : 0U,
        port->inactive_high ? 0U : port->inactive_mask);
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((port->inactive_mask & (1U << pin)) != 0U) {
            GPIO_TypeDef* gpio = port->inactive_gpio->registers;
            gpio->MODER =
                (gpio->MODER & ~(3U << (pin * 2U))) | (1U << (pin * 2U));
        }
    }
    if ((port->registers->CCER & 0x1111U) == 0U) {
        port->registers->CR1 &= ~(uint32_t)TIM_CR1_CEN;
    }
    port->running = false;
    return result;
}

/** \brief Report the staged values, without claiming measured waveform timing.
 */
nx_result_t nx_pwm_port_state(const nx_pwm_port_t* port,
                              nx_pwm_state_t* state) {
    if (port == NULL || !port->initialized || state == NULL) {
        return NX_ERROR_INVALID;
    }
    *state = (nx_pwm_state_t){.period_ticks = port->period_ticks,
                              .duty_ticks = port->duty_ticks,
                              .tick_hz = port->tick_hz,
                              .running = port->running};
    return NX_SUCCESS;
}
