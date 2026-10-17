/**
 * \file            h743_clock_model.h
 * \brief           External SysTick MMIO for exact H743 clock ownership tests
 * \author          Nexus Team
 */
#ifndef NX_H743_CLOCK_MODEL_H
#define NX_H743_CLOCK_MODEL_H
#include "h743_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern SysTick_Type g_h743_clock_systick;
extern uint32_t g_h743_clock_ctrl_reads;
typedef struct {
    bool hold_voltage_raise;
    bool hold_voltage_restore;
    bool hold_pll_ready;
    bool hold_pll_release;
    bool hold_source_switch;
    bool hold_source_restore;
    bool delayed_hpre;
    uint32_t hpre_pending_hclk;
    uint32_t effective_d1cfgr;
    uint32_t prescaler_pending_at_pll_admission;
    uint32_t unsafe_transitions;
    uint32_t running_pll_config_writes;
} nx_h743_clock_behavior_t;
extern nx_h743_clock_behavior_t g_h743_clock_behavior;
uint32_t nx_h743_clock_read(volatile uint32_t* reg);
void nx_h743_clock_write(volatile uint32_t* reg, uint32_t value);
void nx_h743_clock_poll(void);
#ifdef __cplusplus
}
#endif
#undef SysTick
#define SysTick (&g_h743_clock_systick)
#undef SYSCFG
#define SYSCFG (&g_h743_model.mux)
#undef NX_H743_READ
#define NX_H743_READ(reg) nx_h743_clock_read(reg)
#undef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) nx_h743_clock_write((reg), (value))
#define NX_H743_CLOCK_POLL()      nx_h743_clock_poll()
#endif
