/**
 * \file            stm32g474_pwm_group.h
 * \brief           Private TIM1 CH1..3 group, CH4 trigger and COMP1 break
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32G474_PWM_GROUP_H
#define NX_STM32G474_PWM_GROUP_H
#include "nexus/io/pwm_group.h"
#include "stm32g474_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Cold fixed TIM1 base and independently configured COMP1 identity.
 * \note The assembly exclusively owns TIM1, its shared break vector, clocks,
 *       pin routes and COMP1 for this entire session. It configures and proves
 *       Board-specific inactive pins and analog comparator inputs before this
 *       constructor; this timer driver neither invents nor configures wiring.
 *       comparator_csr is the exact enabled COMP1 CSR excluding VALUE. Its
 *       analog settling and threshold accuracy require separate qualification.
 *       Only CH1..3 main outputs are enabled; no complementary pin is implied.
 *       CH4 PWM2 exports internal TRGO2; a positive trigger below period emits
 *       one rising reference edge per complete period. No ADC starts here.
 */
typedef struct {
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t deadtime_ticks;
    uint32_t comparator_csr;
    bool exclusive;
} nx_stm32g474_pwm_group_config_t;
/** \brief Static selected-only state; no changes to ordinary PWM instances. */
typedef struct {
    const nx_stm32g474_pwm_group_config_t* config;
    uint32_t duty_ticks[3];
    uint32_t trigger_ticks;
    uint8_t deadtime_code;
    bool retained;
    bool initialized;
    volatile bool fault;
    bool quarantined;
} nx_stm32g474_pwm_group_state_t;
extern const nx_pwm_group_ops_t nx_stm32g474_pwm_group_ops;
/**
 * \brief Admit idle TIM1 and load a disabled coherent initial zero-duty group.
 * \param[in,out] state: Zero-initialized caller-owned static storage.
 * \param[in] config: Valid immutable cold facts, retained until successful
 * close. \return SUCCESS or error; retained marks cleanup responsibility after
 * effects. \note Requires the established170MHz system, owned enabled
 * TIM1/SYSCFG control clocks and released TIM1/SYSCFG resets. A retained
 * COMP.EN bit alone does not establish the comparator control-clock lease.
 * No GPIO, COMP threshold, NVIC vector, ADC or output is enabled here.
 */
nx_result_t nx_stm32g474_pwm_group_initialize(
    nx_stm32g474_pwm_group_state_t* state,
    const nx_stm32g474_pwm_group_config_t* config);
/**
 * \brief Latch observed COMP1/BIF fault and withdraw master output once.
 * \param[in,out] state: Exact static publisher context, privileged break IRQ.
 * \note Bounded, no loops/OS callbacks; acknowledgment cannot clear the latch.
 *       Cold assembly owns dispatch and joins this publisher before close.
 */
void nx_stm32g474_pwm_group_irq(nx_stm32g474_pwm_group_state_t* state);
/**
 * \brief Detach sources after publisher join, preserving failed cleanup leases.
 * \param[in,out] state: Same privileged task owner; pins/COMP remain external.
 * \return SUCCESS only with disabled/not-pending/not-active shared break
 * vector.
 */
nx_result_t nx_stm32g474_pwm_group_close(nx_stm32g474_pwm_group_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
