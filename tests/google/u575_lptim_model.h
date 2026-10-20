/**
 * \file            u575_lptim_model.h
 * \brief           External U575 LPTIM APB handshakes, counter and vector
 * \author          Nexus Team
 */
#ifndef NX_U575_LPTIM_MODEL_H
#define NX_U575_LPTIM_MODEL_H
#include "nexus/core/time.h"
#include "u575_model.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    LPTIM_TypeDef registers;
    nx_time_us_t now;
    uint32_t writes;
    uint32_t starts;
    uint32_t bad_access;
    uint32_t counter_reads;
    uint32_t exception;
    uint32_t before_enable_us;
    uint32_t before_disable_us;
    uint32_t before_start_us;
    uint8_t arr_delay;
    uint8_t cmp_delay;
    uint8_t dier_delay;
    uint8_t enable_delay;
    bool running;
    bool hold_arr;
    bool hold_cmp;
    bool hold_dier;
    bool hold_disable;
    bool hold_reset;
    bool hold_release;
    bool hold_pending;
    bool hold_gate;
    bool unstable_counter;
} nx_u575_lptim_model_t;
extern nx_u575_lptim_model_t g_u575_lptim_model;
void nx_u575_lptim_model_reset(void);
void nx_u575_lptim_model_step(uint32_t ticks);
uint32_t nx_u575_lptim_model_read(volatile uint32_t* reg);
void nx_u575_lptim_model_write(volatile uint32_t* reg, uint32_t value);
nx_time_us_t nx_u575_lptim_model_now(void);
void nx_u575_lptim_model_clear(void);
#ifdef __cplusplus
}
#endif
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(p)                   nx_u575_lptim_model_read(p)
#define NX_U575_WRITE(p, v)               nx_u575_lptim_model_write((p), (v))
#define NX_U575_LPTIM2                    (&g_u575_lptim_model.registers)
#define NX_U575_LPTIM_NOW()               nx_u575_lptim_model_now()
#define NX_U575_LPTIM_IRQ_ENABLED()       g_u575_model.irq_enabled[68]
#define NX_U575_LPTIM_IRQ_PENDING()       g_u575_model.irq_pending[68]
#define NX_U575_LPTIM_IRQ_ACTIVE()        g_u575_model.irq_active[68]
#define NX_U575_LPTIM_IRQ_PRIORITY()      g_u575_model.priority[68]
#define NX_U575_LPTIM_IRQ_SET_PRIORITY(p) (g_u575_model.priority[68] = (p))
#define NX_U575_LPTIM_IRQ_ENABLE()        (g_u575_model.irq_enabled[68] = true)
#define NX_U575_LPTIM_IRQ_DISABLE()       (g_u575_model.irq_enabled[68] = false)
#define NX_U575_LPTIM_IRQ_CLEAR()         nx_u575_lptim_model_clear()
#define NX_U575_LPTIM_EXCEPTION()         g_u575_lptim_model.exception
#endif
