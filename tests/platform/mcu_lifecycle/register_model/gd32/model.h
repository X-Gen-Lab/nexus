#ifndef NEXUS_GD32_REGISTER_MODEL_H
#define NEXUS_GD32_REGISTER_MODEL_H
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include <stdbool.h>
enum {
    MODEL_GD_IRC_START = 1u << 0, MODEL_GD_SWITCH = 1u << 1,
    MODEL_GD_PLL_STOP = 1u << 2, MODEL_GD_HXTAL_STOP = 1u << 3,
    MODEL_GD_HXTAL_START = 1u << 4, MODEL_GD_PLL_START = 1u << 5,
    MODEL_GD_HIGH_DRIVE = 1u << 6, MODEL_GD_TIMER_STOP = 1u << 7,
    MODEL_GD_IRQ_DISABLE = 1u << 8, MODEL_GD_PENDING_CLEAR = 1u << 9,
    MODEL_GD_GATE_DISABLE = 1u << 10, MODEL_GD_TIMER_INIT = 1u << 11,
};
typedef struct {
    uint32_t registers[MODEL_GD_REG_COUNT];
    NVIC_Type nvic;
    SCB_Type scb;
    uint32_t dma[2][8];
    uint32_t timer_ctl0, timer_dmainten, timer_psc, timer_car, timer_count;
    uint32_t timer_pending, flash_latency, timer_clock_selector;
} model_gd_snapshot_t;
extern volatile uint32_t model_gd_registers[MODEL_GD_REG_COUNT];
extern uint32_t model_gd_faults, model_gd_timer_count, model_gd_mask;
extern bool model_gd_timer_pending, model_gd_unsafe_stop;
void model_gd_reset(void);
void model_gd_snapshot(model_gd_snapshot_t* out);
#endif
