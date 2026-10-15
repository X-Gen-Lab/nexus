/**
 * \file            timebase_soc_model.h
 * \brief           Real SDK timer layout with external Cortex register models
 * \author          Nexus Team
 */
#ifndef NX_TIMEBASE_SOC_MODEL_H
#define NX_TIMEBASE_SOC_MODEL_H
#ifdef NX_TIMEBASE_GD_MODEL
#include "gd32f4xx.h"
#else
#include "stm32f407xx.h"
#endif
#include "timebase_lowpower_model.h"

#ifdef __cplusplus
extern "C" {
#endif
/** \brief SysTick physical register shape; not an alternate production type. */
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} nx_timebase_model_systick_t;
extern nx_timebase_model_systick_t g_timebase_model_systick;
extern NVIC_Type g_timebase_model_nvic;
extern SCB_Type g_timebase_model_scb;
#ifdef __cplusplus
}
#endif
#undef SysTick
#undef NVIC
#undef SCB
#define SysTick (&g_timebase_model_systick)
#define NVIC    (&g_timebase_model_nvic)
#define SCB     (&g_timebase_model_scb)
#ifdef NX_TIMEBASE_INTEGRATION
#ifdef __cplusplus
extern "C" {
#endif
void nx_timebase_model_ack(uint32_t value);
void nx_timebase_model_enable_irq(IRQn_Type irq);
void nx_timebase_model_disable_irq(IRQn_Type irq);
void nx_timebase_model_clear_irq(IRQn_Type irq);
void nx_timebase_model_priority(IRQn_Type irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#define NX_GD32_TIMEBASE_ACK(value)          nx_timebase_model_ack(value)
#define NX_STM32_TIMEBASE_ACK(system, value) nx_timebase_model_ack(value)
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_SetPriority
#define NVIC_EnableIRQ(irq)       nx_timebase_model_enable_irq(irq)
#define NVIC_DisableIRQ(irq)      nx_timebase_model_disable_irq(irq)
#define NVIC_ClearPendingIRQ(irq) nx_timebase_model_clear_irq(irq)
#define NVIC_SetPriority(irq, priority)                                        \
    nx_timebase_model_priority(irq, priority)
#endif
#endif /* NX_TIMEBASE_SOC_MODEL_H */
