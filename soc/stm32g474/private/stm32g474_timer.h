/**
 * \file            stm32g474_timer.h
 * \brief           Exact optional TIM5 capture and TIM3 quadrature sessions
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_TIMER_H
#define NX_STM32G474_TIMER_H
#include "nexus/io/encoder.h"
#include "stm32g474_system.h"
#include "timer_advanced_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Cold TIM5 CH1 PA0 AF2, full32-bit counter and exact170MHz base.
 * \param[in,out] state: Zeroed static state; acquired config marks cleanup.
 * \param[in] config: Exact tick/filter/edge and whole-timer authority.
 * \return SUCCESS or error; failed effects retain config until release.
 * \note Assembly owns clocks, analog/electrical route and TIM5 vector. This
 *       source does not claim a connector or start the capture ring.
 */
nx_result_t
nx_stm32g474_timer_capture_initialize(nx_timer_capture_controller_t* state,
                                      const nx_timer_capture_config_t* config);
/** \brief Release only a drained source after disabled/quiet vector proof. */
nx_result_t
nx_stm32g474_timer_capture_release(nx_timer_capture_controller_t* state);
/** \brief Publish at most one raw capture; CCR read clears its own event. */
void nx_stm32g474_timer_capture_irq(nx_timer_capture_controller_t* state);
extern const nx_timer_capture_ops_t nx_stm32g474_timer_capture_ops;
/** \brief Exact TIM3 CH1/CH2 PA6/PA7 AF2, no index or external-clock2 mode. */
typedef struct {
    nx_encoder_mode_t mode;
    uint8_t filter_a;
    uint8_t filter_b;
    bool invert_a;
    bool invert_b;
    bool exclusive;
} nx_stm32g474_encoder_config_t;
/** \brief Selected-only state; no growth to ordinary PWM or capture paths. */
typedef struct {
    const nx_stm32g474_encoder_config_t* config;
    bool initialized;
} nx_stm32g474_encoder_state_t;
/** \brief Configure the16-bit raw counter while all inputs remain disabled.
 * \param[in,out] state: Zeroed exclusive static storage, never caller-mutated.
 * \param[in] config: Immutable two-pin filter, polarity and counting facts.
 * \return SUCCESS or error; config retains partial cleanup ownership.
 * \note Enabled TIM3/GPIOA clocks, released reset, fixed170MHz and actual AF2
 *       pins are assembly facts. No interrupt or output is enabled here.
 */
nx_result_t
nx_stm32g474_encoder_initialize(nx_stm32g474_encoder_state_t* state,
                                const nx_stm32g474_encoder_config_t* config);
/** \brief End an observed stopped session with the unused vector quiet. */
nx_result_t nx_stm32g474_encoder_close(nx_stm32g474_encoder_state_t* state);
extern const nx_encoder_ops_t nx_stm32g474_encoder_ops;
#ifdef __cplusplus
}
#endif
#endif
