/**
 * \file            timer.c
 * \brief           GD32 TIMER2 fixed-base shadow PWM and safe inactive output
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_pwm_port_t* s_pwm;

/** \brief           Reserve one timer base before configuring its safe output.
 */
nx_result_t nx_gd32_pwm_initialize(nx_pwm_port_t* port, uint32_t period,
                                   uint32_t tick_hz) {
    if (!port || !period || period > UINT16_MAX || !tick_hz ||
        100000000u % tick_hz != 0u || 100000000u / tick_hz > 65536u) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_pwm) {
        return NX_ERROR_BUSY;
    }
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_TIMER2);
    GPIO_BOP(GPIOA) = GPIO_PIN_6 << 16;
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6);
    timer_deinit(TIMER2);
    TIMER_PSC(TIMER2) = 100000000u / tick_hz - 1u;
    TIMER_CAR(TIMER2) = period - 1u;
    TIMER_CH0CV(TIMER2) = 0u;
    TIMER_CHCTL0(TIMER2) = TIMER_CHCTL0_CH0COMSEN | (6u << 4);
    TIMER_CTL0(TIMER2) = TIMER_CTL0_ARSE;
    TIMER_SWEVG(TIMER2) = TIMER_SWEVG_UPG;
    TIMER_INTF(TIMER2) = 0u;
    *port = (nx_pwm_port_t){.state = {period, 0u, tick_hz, false},
                            .initialized = true};
    s_pwm = port;
    return NX_SUCCESS;
}

/** \brief           Stage CCR, including exact zero and full-period duty. */
nx_result_t nx_pwm_port_set(nx_pwm_port_t* port, uint32_t period,
                            uint32_t duty) {
    if (!port || port != s_pwm || !port->initialized || !period ||
        duty > period || period > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (period != port->state.period_ticks) {
        return NX_ERROR_STATE;
    }
    TIMER_CH0CV(TIMER2) = duty;
    port->state.duty_ticks = duty;
    return NX_SUCCESS;
}

/** \brief           Load staged shadows before enabling the pin and counter. */
nx_result_t nx_pwm_port_start(nx_pwm_port_t* port) {
    if (!port || port != s_pwm || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!port->state.running) {
        TIMER_SWEVG(TIMER2) = TIMER_SWEVG_UPG;
        gpio_af_set(GPIOA, GPIO_AF_2, GPIO_PIN_6);
        TIMER_CHCTL2(TIMER2) = TIMER_CHCTL2_CH0EN;
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6);
        TIMER_CTL0(TIMER2) |= TIMER_CTL0_CEN;
        port->state.running = true;
    }
    return NX_SUCCESS;
}

/** \brief           Drive the selected inactive low before disabling timer. */
nx_result_t nx_pwm_port_stop(nx_pwm_port_t* port) {
    if (!port || port != s_pwm || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    GPIO_BOP(GPIOA) = GPIO_PIN_6 << 16;
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    TIMER_CHCTL2(TIMER2) = 0u;
    TIMER_CTL0(TIMER2) &= ~TIMER_CTL0_CEN;
    nx_gd32_peripheral_barrier();
    port->state.running = false;
    return NX_SUCCESS;
}

/** \brief           Return selected fixed-base and staged duty facts. */
nx_result_t nx_pwm_port_state(const nx_pwm_port_t* port,
                              nx_pwm_state_t* state) {
    if (!port || port != s_pwm || !state || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    *state = port->state;
    return NX_SUCCESS;
}

/** \brief           Release timer ownership while retaining safe pin output. */
nx_result_t nx_gd32_pwm_release(nx_pwm_port_t* port) {
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t status = nx_pwm_port_stop(port);
    if (status != NX_SUCCESS) {
        return status;
    }
    timer_deinit(TIMER2);
    rcu_periph_clock_disable(RCU_TIMER2);
    port->initialized = false;
    s_pwm = NULL;
    return NX_SUCCESS;
}
