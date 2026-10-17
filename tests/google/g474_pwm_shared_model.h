/**
 * \file            g474_pwm_shared_model.h
 * \brief           External TIM3 preload/update and peer-lane control leaves
 * \author          Nexus Team
 */
#ifndef NX_G474_PWM_SHARED_MODEL_H
#define NX_G474_PWM_SHARED_MODEL_H
#include "g474_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern TIM_TypeDef g_g474_shared_timer;
uint32_t nx_g474_shared_read(volatile uint32_t* reg);
void nx_g474_shared_write(volatile uint32_t* reg, uint32_t value);
void nx_g474_shared_poll(void* owner);
#ifdef __cplusplus
}
struct nx_g474_shared_model_t {
    uint32_t active_psc;
    uint32_t active_arr;
    uint32_t active_ccr[2];
    unsigned updates;
    unsigned software_updates;
    unsigned reset_asserts;
    unsigned timer_reads;
    unsigned timer_writes;
    unsigned trigger_edges;
    unsigned illegal_controls;
    bool refuse_update;
    bool refuse_ack;
    bool refuse_reset_assert;
    bool refuse_reset_release;
    volatile uint32_t* refuse_register;
    uint32_t hold_ccer_set;
    void (*write_observer)(volatile uint32_t*, uint32_t);
};
extern nx_g474_shared_model_t g_shared_pwm;
void nx_g474_shared_reset();
void nx_g474_shared_update();
void nx_g474_shared_advance(unsigned ticks);
#endif
#undef TIM3
#undef GPIOA
#undef RCC
#define TIM3  (&g_g474_shared_timer)
#define GPIOA (&g_g474_model.gpio)
#define RCC   (&g_g474_model.rcc)
#undef NX_G474_READ
#undef NX_G474_WRITE
#define NX_G474_READ(reg)              nx_g474_shared_read(reg)
#define NX_G474_WRITE(reg, value)      nx_g474_shared_write((reg), (uint32_t)(value))
#define NX_G474_PWM_SHARED_POLL(owner) nx_g474_shared_poll(owner)
#endif
