/**
 * \file            u575_pwm_leaf_model.h
 * \brief           Real U575 PWM MMIO with external CPU and update facts
 * \author          Nexus Team
 */
#ifndef NX_U575_PWM_LEAF_MODEL_H
#define NX_U575_PWM_LEAF_MODEL_H
#define NX_U575_PWM_DEFAULT_PORT 1
#include "u575_devices_model.h"

#include "stm32u575_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_u575_pwm_leaf_poll(void* port);
void nx_u575_pwm_official_set(TIM_TypeDef* timer, uint32_t prescaler,
                              uint32_t reload, uint32_t compare);
extern TIM_TypeDef g_u575_pwm_leaf_decoy;
#ifdef __cplusplus
}
#endif
/* Use the production volatile defaults; only external update facts are modeled.
 */
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg)         (*(reg))
#define NX_U575_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#undef NX_U575_PWM_POLL
#define NX_U575_PWM_POLL(port) nx_u575_pwm_leaf_poll(port)
#undef TIM3
#define TIM3 (&g_u575_pwm_leaf_decoy)
#endif
