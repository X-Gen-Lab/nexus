/**
 * \file            stm32f407_entropy.h
 * \brief           Explicit F407 RNG owner over the reviewed PLL48 profile
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ENTROPY_H
#define NEXUS_STM32F407_ENTROPY_H

#include "nexus/io/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Zero-initialized, single-executor controller owner.
 * \note Mutable implementation state, not an entropy estimate or certification.
 *       Retained gate/RNG ownership must be stopped before reclaiming this
 * state.
 */
typedef struct {
    uint64_t time_epoch;
    uint32_t previous_word;
    bool running;
    bool gate_owned;
    bool rng_owned;
    bool primed;
    bool faulted;
} nx_stm32f407_entropy_state_t;

/** \brief Shared immutable nonblocking word operations. */
extern const nx_entropy_ops_t nx_stm32f407_entropy_ops;

/** \brief Read-only cold admission before any System clock effects.
 * \param[in] state: Same serialized zero/fully stopped static owner.
 * \return SUCCESS, CONTEXT, INVALID or BUSY; no retained references/effects.
 * \note Requires privileged unmasked task context, not a live System domain.
 *       Checks RNGEN/IE only with the RNG gate already enabled. Foreign live
 *       hardware must be withdrawn by its owner before Platform.start/resume.
 */
nx_result_t
nx_stm32f407_entropy_admit(const nx_stm32f407_entropy_state_t* state);

/** \brief Start the exclusive RNG without changing the shared PLL profile.
 * \param[in,out] state: Zero/fully stopped state, serialized with all callers.
 * \return SUCCESS, CONTEXT, INVALID, BUSY, STATE, UNSUPPORTED or IO.
 * \note Privileged unmasked task only. Caller proves exclusive RNG ownership
 *       and maintains the existing PLL48 clock dependency through stop. Active
 *       RNG or IE rejects without stealing it. Only the RNG AHB2 gate is
 * acquired if initially disabled; no PLL, NVIC or other controller is changed.
 *       A post-write error may retain gate/RNG effects; retry stop, not rebind.
 */
nx_result_t nx_stm32f407_entropy_start(nx_stm32f407_entropy_state_t* state);

/** \brief Close admission, disable owned RNG and release only its acquired
 * gate. \param[in,out] state: Same live owner after all synchronous users
 * joined. \return SUCCESS for full withdrawal; IO retains failed cleanup
 * ownership; INVALID or CONTEXT rejects before hardware access. \note
 * Privileged unmasked task only. Shared PLL/source/HCLK remain untouched. No
 * sample, request or output buffer survives a synchronous call. Stop followed
 * by a new successful start resets the first-sample health test; no error is
 * cleared by a normal read or a timeout.
 */
nx_result_t nx_stm32f407_entropy_stop(nx_stm32f407_entropy_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_STM32F407_ENTROPY_H */
