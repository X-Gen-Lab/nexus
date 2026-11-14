/**
 * \file            pwm_cold.h
 * \brief           Static PWM configuration and retained cold withdrawal
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_TIMER_PWM_COLD_H
#define NEXUS_STM32F4_DRIVERS_TIMER_PWM_COLD_H

#include "drivers/gpio/pins.h"
#include "drivers/timer/instance.h"
#include "drivers/timer/stm32f407_timer_hardware.h"
#include "nexus/arch/arch.h"
#include "private/cold_context.h"
#include "system/stm32f407_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact general timer channel, counts and declared pad idle level. */
typedef struct {
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint16_t prescaler;
    bool inactive_high;
    TIM_TypeDef* registers;
    GPIO_TypeDef* gpio;
    uint8_t channel;
    uint8_t pin;
} nx_stm32_pwm_config_t;

/** \brief Reviewed outputs; package and Board authority remain separate. */
static inline bool
nx_stm32_pwm_route_valid(const nx_stm32_pwm_config_t* config) {
    if (config->channel < 1U || config->channel > 4U)
        return false;
    if (config->registers == TIM3) {
        return config->channel <= 2U ? config->gpio == GPIOA &&
                                           config->pin == config->channel + 5U
                                     : config->gpio == GPIOB &&
                                           config->pin == config->channel - 3U;
    }
    if (config->registers == TIM4)
        return config->gpio == GPIOB && config->pin == config->channel + 5U;
    if (config->registers == TIM5)
        return config->gpio == GPIOA && config->pin == config->channel - 1U;
    if (config->registers == TIM9)
        return config->channel <= 2U && config->gpio == GPIOA &&
               config->pin == config->channel + 1U;
    if (config->registers == TIM12)
        return config->channel <= 2U && config->gpio == GPIOB &&
               config->pin == config->channel + 13U;
    if (config->channel != 1U)
        return false;
    return (config->registers == TIM10 && config->gpio == GPIOB &&
            config->pin == 8U) ||
           (config->registers == TIM11 && config->gpio == GPIOB &&
            config->pin == 9U) ||
           (config->registers == TIM13 && config->gpio == GPIOA &&
            config->pin == 6U) ||
           (config->registers == TIM14 && config->gpio == GPIOA &&
            config->pin == 7U);
}

/** \brief Prepare selected PWM; cold_effects occupies existing private padding.
 * \note Platform serializes pad ownership. Failed clock proof requires close;
 *       the preserved effect bit is cleared only after observed gate removal.
 */
static inline nx_result_t
nx_stm32_pwm_prepare(nx_stm32_pwm_state_t* port,
                     nx_stm32_gpio_state_t* inactive,
                     const nx_stm32_pwm_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || inactive == NULL || config == NULL ||
        !nx_stm32_pwm_route_valid(config) || config->period_ticks == 0U ||
        (config->registers != TIM5 && config->period_ticks > 65535U) ||
        config->duty_ticks > config->period_ticks || config->tick_hz == 0U)
        return NX_ERROR_INVALID;
    const bool apb2 = nx_stm32_timer_apb2(config->registers);
    const uint32_t source_hz = apb2 ? NEXUS_APB2_TIMER_HZ : NEXUS_APB1_TIMER_HZ;
    if (source_hz / config->tick_hz != (uint32_t)config->prescaler + 1U ||
        source_hz % config->tick_hz != 0U)
        return NX_ERROR_INVALID;
    volatile uint32_t* clock = apb2 ? &RCC->APB2ENR : &RCC->APB1ENR;
    const uint32_t gate = nx_stm32_timer_gate(config->registers);
    if (port->initialized || inactive->initialized ||
        port->cold_effects != 0U || (*clock & gate) != 0U)
        return NX_ERROR_BUSY;
    /* Retain exact gate identity before the first fallible acquisition. */
    port->registers = config->registers;
    port->cold_effects = 1U;
    NX_PLATFORM_REGISTER_WRITE(*clock, *clock | gate);
    nx_arch_dsb();
    if ((*clock & gate) == 0U)
        return NX_ERROR_IO;
    TIM_TypeDef* timer = config->registers;
    const uint32_t channel = 1U << ((config->channel - 1U) * 4U);
    if ((timer->CCER & channel) != 0U ||
        ((timer->CR1 & TIM_CR1_CEN) != 0U &&
         (timer->PSC != config->prescaler ||
          timer->ARR != config->period_ticks - 1U)))
        return NX_ERROR_BUSY;
    inactive->registers = config->gpio;
    inactive->mask = 1U << config->pin;
    inactive->output = true;
    nx_result_t result = nx_stm32_gpio_initialize(
        inactive, config->inactive_high ? inactive->mask : 0U);
    if (result != NX_SUCCESS)
        return result;
    port->inactive_gpio = inactive;
    port->inactive_mask = inactive->mask;
    port->inactive_high = config->inactive_high;
    port->channel = config->channel;
    port->period_ticks = config->period_ticks;
    port->duty_ticks = config->duty_ticks;
    port->tick_hz = config->tick_hz;
    port->prescaler = config->prescaler;
    return nx_stm32_pwm_initialize(port);
}

/** \brief Observe inactive PWM before removing the selected timer gate.
 * \note A failed stop never permits Platform to release the pad lease.
 */
static inline nx_result_t nx_stm32_pwm_close(nx_stm32_pwm_state_t* port,
                                             nx_stm32_gpio_state_t* inactive) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || inactive == NULL)
        return NX_ERROR_INVALID;
    if (port->cold_effects == 0U)
        return port->initialized ? NX_ERROR_BUSY : NX_SUCCESS;
    const uint32_t gate = nx_stm32_timer_gate(port->registers);
    if (gate == 0U)
        return NX_ERROR_STATE;
    volatile uint32_t* clock =
        nx_stm32_timer_apb2(port->registers) ? &RCC->APB2ENR : &RCC->APB1ENR;
    if (port->initialized && (*clock & gate) == 0U)
        return NX_ERROR_IO;
    if (port->initialized) {
        nx_result_t result = nx_stm32_pwm_ops.stop(port);
        if (result != NX_SUCCESS)
            return result;
        port->initialized = false;
        inactive->initialized = false;
    }
    if ((port->cold_effects & 1U) != 0U) {
        if ((*clock & gate) != 0U && (port->registers->CCER & 0x1111U) != 0U)
            return NX_ERROR_BUSY;
        NX_PLATFORM_REGISTER_WRITE(*clock, *clock & ~gate);
        nx_arch_dsb();
        if ((*clock & gate) != 0U)
            return NX_ERROR_IO;
        port->cold_effects &= (uint8_t)~1U;
    }
    inactive->initialized = false;
    return NX_SUCCESS;
}

#ifdef __cplusplus
}
#endif
#endif
