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
/** \brief Persistent TIM5 vector receipt, no duplicate controller state. */
typedef struct {
    uint8_t saved_priority;
    bool vector_owned;
} nx_stm32g474_capture_cold_t;
/** \brief Prepare PA0, capture controller and its quiet vector in that order.
 * \param[in,out] state: Static unbound capture storage.
 * \param[in] config: Static reviewed capture clock/filter/edge geometry.
 * \param[in,out] cold: Zeroed vector receipt, retained through failure/retry.
 * \param[in] priority: Reviewed logical priority admitted by the resolver.
 * \return Status; close must settle any partially acquired source/vector.
 */
nx_result_t
nx_stm32g474_timer_capture_prepare(nx_timer_capture_controller_t* state,
                                   const nx_timer_capture_config_t* config,
                                   nx_stm32g474_capture_cold_t* cold,
                                   uint8_t priority);
/** \brief Drain the controller, disable/restore vector, then release state.
 * \param[in,out] state: Same static capture owner, borrowers already joined.
 * \param[in,out] cold: Same receipt; failure retains unproved ownership.
 * \return Success only after every source and vector is quiet/read back.
 */
nx_result_t
nx_stm32g474_timer_capture_close(nx_timer_capture_controller_t* state,
                                 nx_stm32g474_capture_cold_t* cold);
/** \brief Prepare exact PA6/PA7 AF2 before binding the TIM3 encoder.
 * \param[in,out] state: Static unretained exact encoder state.
 * \param[in] config: Static reviewed filter, polarity and counting facts.
 * \return Status; existing close and Platform restore own partial effects.
 */
nx_result_t
nx_stm32g474_encoder_prepare(nx_stm32g474_encoder_state_t* state,
                             const nx_stm32g474_encoder_config_t* config);
extern const nx_encoder_ops_t nx_stm32g474_encoder_ops;
#ifdef __cplusplus
}
#endif
#endif
