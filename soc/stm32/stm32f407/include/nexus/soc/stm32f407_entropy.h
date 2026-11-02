/**
 * \file            stm32f407_entropy.h
 * \brief           Explicit optional construction of the unique F407 RNG source
 * \author          Nexus Team
 */
#ifndef NEXUS_SOC_STM32F407_ENTROPY_H
#define NEXUS_SOC_STM32F407_ENTROPY_H

#include "nexus/io/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact Board/profile facts admitted against the live RCC
 * \note            First scope uses an 8 MHz reviewed HSE Board or the fixed
 *                  16 MHz HSI. These values come from validated assembly facts,
 *                  not guessed hardware. Start checks exact PLL48 and HCLK;
 *                  caller maintains the source/PLLQ/HCLK dependency until stop.
 */
typedef struct {
    uint32_t hse_hz;
    uint32_t hclk_hz;
} nx_stm32f407_entropy_config_t;

/**
 * \brief           Return the existing static source face without starting it
 * \return          Stable readonly face, including while the source is stopped
 * \note            No allocation, register access, clock acquisition, locking
 *                  or registration. The face's private state belongs to the
 *                  unique explicit constructor target. Data calls use its
 *                  shared provider table directly; a cold face reports STATE.
 */
const nx_entropy_port_t* nx_stm32f407_entropy_instance(void);

/**
 * \brief           Start the explicit source after successful Platform startup
 * \param[in]       config: Exact validated Board/profile facts, copied during
 *                  this call; source/PLL48 dependency stays live until stop
 * \return          SUCCESS, CONTEXT, INVALID, BUSY, STATE, UNSUPPORTED or IO
 * \note            Privileged unmasked serialized task. One unique hardware
 *                  RNG owner; a repeated live start returns BUSY. No scheduler,
 *                  Platform startup or clock-profile change is implicit. A
 *                  post-write IO may retain owned effects; stop must succeed
 *                  before releasing Platform clock resources or recovery state.
 */
nx_result_t nx_stm32f407_entropy_instance_start(
    const nx_stm32f407_entropy_config_t* config);

/**
 * \brief           Stop the source before withdrawing Platform clock resources
 * \return          SUCCESS when stopped; IO retains failed cleanup ownership
 * \note            Privileged unmasked serialized task after source consumers
 *                  have joined. Retry the same stop on failed withdrawal; do
 *                  not reset or rebind private state. The existing static face
 *                  remains valid and reports STATE while admission is closed.
 */
nx_result_t nx_stm32f407_entropy_instance_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_SOC_STM32F407_ENTROPY_H */
