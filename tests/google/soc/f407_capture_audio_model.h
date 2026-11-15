/**
 * \file            f407_capture_audio_model.h
 * \brief           MMIO-only model leaves for real F407 capture/audio C owners
 * \author          Nexus Team
 */
#ifndef NX_F407_CAPTURE_AUDIO_MODEL_H
#define NX_F407_CAPTURE_AUDIO_MODEL_H
#include "nexus/core/time.h"
#include "stm32_official_model.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_av_model_read(volatile uint32_t* reg);
void nx_av_model_write(volatile uint32_t* reg, uint32_t value);
void nx_av_model_poll(void);
nx_time_us_t nx_av_model_now(void);
uint32_t nx_av_model_cycles(void);
bool nx_av_model_privileged(void);
bool nx_av_model_in_isr(void);
bool nx_av_model_masked(void);
uint32_t nx_av_model_irq_active(IRQn_Type irq);
uint32_t nx_av_model_irq_pending(IRQn_Type irq);
uint32_t nx_av_model_irq_enabled(IRQn_Type irq);
void nx_av_model_irq_enable(IRQn_Type irq);
void nx_av_model_irq_disable(IRQn_Type irq);
void nx_av_model_irq_clear(IRQn_Type irq);
void nx_av_model_irq_priority(IRQn_Type irq, uint32_t priority);
uint32_t nx_av_model_irq_priority_read(IRQn_Type irq);
extern uint32_t g_av_trace;
extern uint32_t g_av_counter;
#ifdef __cplusplus
}
#endif
#define NX_AV_READ(reg)              nx_av_model_read(reg)
#define NX_AV_WRITE(reg, value)      nx_av_model_write((reg), (value))
#define NX_AV_POLL()                 nx_av_model_poll()
#define NX_AV_NOW()                  nx_av_model_now()
#define NX_AV_PRIVILEGED()           nx_av_model_privileged()
#define NX_AV_IN_ISR()               nx_av_model_in_isr()
#define NX_AV_MASKED()               nx_av_model_masked()
#define NX_AV_CYCLES()               nx_av_model_cycles()
#define NX_AV_TRACE                  (&g_av_trace)
#define NX_AV_COUNTER_CONTROL        (&g_av_counter)
#define NX_AV_IRQ_ACTIVE(irq)        nx_av_model_irq_active(irq)
#define NX_AV_IRQ_PENDING(irq)       nx_av_model_irq_pending(irq)
#define NX_AV_IRQ_ENABLED(irq)       nx_av_model_irq_enabled(irq)
#define NX_AV_IRQ_ENABLE(irq)        nx_av_model_irq_enable(irq)
#define NX_AV_IRQ_DISABLE(irq)       nx_av_model_irq_disable(irq)
#define NX_AV_IRQ_CLEAR(irq)         nx_av_model_irq_clear(irq)
#define NX_AV_IRQ_PRIORITY_READ(irq) nx_av_model_irq_priority_read(irq)
#define NX_AV_IRQ_PRIORITY(irq, priority)                                      \
    nx_av_model_irq_priority((irq), (priority))
#endif /* NX_F407_CAPTURE_AUDIO_MODEL_H */
