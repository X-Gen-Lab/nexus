/**
 * \file            stm32g474_pwm_shared.h
 * \brief           Selected TIM3 owner and readonly PA6/PA7 PWM channel faces
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_PWM_SHARED_H
#define NX_STM32G474_PWM_SHARED_H

#include "stm32g474_provider.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable fixed170MHz TIM3 base and Board-declared safe levels.
 * \note Exactly CH1/PA6 and CH2/PA7 AF2 are selected. Both are normal PWM1,
 *       active-high, with one fixed up-counting base and no timer IRQ/DMA.
 *       inactive contains only Board pad bits6/7, not a channel bitmask.
 *       Config and owner outlive every channel caller and failed close retry.
 */
typedef struct {
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t duty_ticks[2];
    uint32_t inactive;
    uint32_t poll_limit;
} nx_stm32g474_pwm_shared_config_t;

/** \brief One physical owner; this storage exists only for the selected mode.
 * \note running_mask describes selected lanes, not two independently owned
 *       timers. One close owns final CEN/reset and both safe pad releases.
 */
typedef struct {
    const nx_stm32g474_pwm_shared_config_t* config;
    nx_stm32g474_gpio_state_t idle;
    uint32_t duty_ticks[2];
    uint8_t running_mask;
    bool initialized;
    bool retained;
    bool fault;
} nx_stm32g474_pwm_shared_state_t;

/** \brief Readonly private reference to a single lane in the physical owner.
 * \note channel is exactly1 or2. The immutable face keeps the existing public
 *       nx_pwm_port_t ABI and points directly to this reference as context.
 *       No additional vtable or forwarding-only runtime object is introduced.
 */
typedef struct {
    nx_stm32g474_pwm_shared_state_t* owner;
    uint8_t channel;
} nx_stm32g474_pwm_shared_channel_t;

extern const nx_pwm_ops_t nx_stm32g474_pwm_shared_ops;

/** \brief Acquire one cold TIM3 base and both Board-safe output pads.
 * \param[in,out] owner: Static zero-initialized exclusive physical owner.
 * \param[in] config: Immutable selected base and initial channel duties.
 * \return Success, INVALID/CONTEXT/STATE/BUSY/IO/TIMEOUT.
 * \note Privileged unmasked task only. Platform already owns actual TIM3 and
 *       GPIOA clocks/reset leases and the quiet TIM3 vector reservation.
 *       The caller holds the same exclusive physical TIM3 lease through every
 *       partial failure until complete close succeeds. It must not grant that
 *       lease to another state while this owner retains it; quiet registers do
 *       not prove the absence of another retained owner. No runtime ownership
 *       registry is provided by this private constructor.
 *       This mode never enables timer IRQ/DMA. Reject invalid/foreign cold
 *       state before effects; partial effects retain owner for close retry.
 *       Initialization leaves both outputs disabled and pins at safe GPIO.
 */
nx_result_t nx_stm32g474_pwm_shared_initialize(
    nx_stm32g474_pwm_shared_state_t* owner,
    const nx_stm32g474_pwm_shared_config_t* config);

/** \brief Withdraw both channels and the one base before releasing ownership.
 * \param[in,out] owner: Retained owner, including a partially failed start.
 * \return Success or CONTEXT/INVALID/STATE/IO.
 * \note No concurrent channel calls. Clock proof precedes accessible SFR
 *       reads. Refused disable/reset/idle retains cleanup for retry. A fully
 *       closed predecessor is a no-effect success and cannot stop a successor.
 *       Ordinary channel stop withdraws only its own lane and stops CEN only
 *       when no peer remains. A channel joining a running peer adopts the
 *       existing phase; no coherent group-start/first-full-pulse promise.
 */
nx_result_t
nx_stm32g474_pwm_shared_close(nx_stm32g474_pwm_shared_state_t* owner);

#ifndef NX_G474_PWM_SHARED_POLL
#define NX_G474_PWM_SHARED_POLL(owner) ((void)(owner))
#endif
#ifdef __cplusplus
}
#endif
#endif
