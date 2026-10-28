/**
 * \file            legacy_pwm.h
 * \brief           Reviewed F103RB/GD303RC legacy fixed PWM engine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_LEGACY_PWM_H
#define NX_LEGACY_PWM_H
#include "nexus/arch/arch.h"
#include "nexus/io/timer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Private fixed CH1 timer ABI reviewed against each exact SDK. */
typedef struct {
    volatile uint32_t control1, control2, slave, interrupt, status, event;
    volatile uint32_t channel_mode1, channel_mode2, enable, counter;
    volatile uint32_t prescaler, reload, repetition, compare1;
} nx_legacy_pwm_registers_t;
/** \brief Immutable reviewed timer and pad resources for the owner lifetime. */
typedef struct {
    nx_legacy_pwm_registers_t* registers;
    volatile uint32_t* gate;
    volatile uint32_t* pin_mode;
    volatile uint32_t* pin_lock;
    volatile uint32_t* pin_latch;
    volatile uint32_t* pin_set;
    const volatile bool* clock_started;
    uint32_t gate_mask, poll_limit;
} nx_legacy_pwm_config_t;
/** \brief One fixed PA6 channel with Board-declared inactive level.
 * \note Config cannot overlap state or API output. Period, duty and safe
 * level belong to this selected owner, never to the shared hardware facts.
 */
typedef struct {
    const nx_legacy_pwm_config_t* config;
    uint32_t tick_hz, period_ticks, duty_ticks, inactive;
    bool initialized, retained, running, fault;
} nx_legacy_pwm_state_t;
extern const nx_pwm_ops_t nx_legacy_pwm_ops;
/** \brief Configure stopped exclusive TIM3/TIMER2 CH1, reset-like source.
 * \param[in,out] port: Static cold owner after GPIO/timer gate acquisition.
 * \return SUCCESS or error; retained requires close before lease release.
 */
nx_result_t nx_legacy_pwm_initialize(nx_legacy_pwm_state_t* port);
/** \brief Stop timer and drive declared PA6 inactive GPIO level.
 * \param[in,out] port: Serialized owner with all consumers joined.
 * \return SUCCESS after readback; failure retains the concrete lease.
 */
nx_result_t nx_legacy_pwm_close(nx_legacy_pwm_state_t* port);
#ifndef NX_LEGACY_READ
#define NX_LEGACY_READ(reg) (*(reg))
#endif
#ifndef NX_LEGACY_WRITE
#define NX_LEGACY_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_LEGACY_POLL
#define NX_LEGACY_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifdef __cplusplus
}
#endif
#endif
