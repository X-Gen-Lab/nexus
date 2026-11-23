/**
 * \file            stm32f4_idle_power.h
 * \brief           Trusted masked Idle STOP transaction
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_IDLE_POWER_H
#define NX_STM32F4_IDLE_POWER_H
#include "nexus/soc/stm32f4_power.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Enter STOP after the sole tick owner handed off its paused source.
 * \note Private adapter entry, privileged Idle Thread with PRIMASK=1 and
 *       BASEPRI/FAULTMASK clear. Caller holds the scheduler suspended and the
 *       exact tick handoff receipt. Every other power_enter contract applies;
 *       Standby is forbidden. Incoming PRIMASK remains set on return. This
 *       does not pause, acknowledge or restore any kernel tick by itself.
 */
nx_result_t
nx_stm32_power_enter_idle_masked(nx_clock_barrier_t barrier,
                                 const nx_stm32_power_options_t* options,
                                 nx_stm32_stop_receipt_t* receipt);
#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_IDLE_POWER_H */
