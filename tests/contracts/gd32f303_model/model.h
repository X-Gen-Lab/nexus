/**
 * \file            model.h
 * \brief           External GD32F303 MMIO, W0C and NVIC hardware model boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32F303_MODEL_H
#define NX_GD32F303_MODEL_H
#include "nexus/core/status.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern bool g_gd303_irq[60];
extern bool g_gd303_pending[60];
extern uint8_t g_gd303_priority[60];
extern unsigned g_gd303_writes;
extern volatile uint32_t* g_gd303_ignore;
extern bool g_gd303_hold_update;
extern uint32_t g_gd303_density;
extern bool g_gd303_update_trgo;
extern bool g_gd303_spi_hold_rx;
extern bool g_gd303_spi_bad_cs;
extern unsigned g_gd303_spi_bytes;
void nx_gd303_model_spi_poll(void);
#define NX_GD303_SPI_POLL() nx_gd303_model_spi_poll()
extern bool g_gd303_hold_irq_off;
extern bool g_gd303_pend_on_timer_withdraw;
extern bool g_gd303_hold_late_irq_off;
extern bool g_gd303_hold_cs_release;
extern bool g_gd303_hold_spi_reset_release;
void nx_gd303_model_write(volatile uint32_t* reg, uint32_t value);
uint32_t nx_gd303_model_read(volatile uint32_t* reg);
nx_result_t nx_gd303_model_irq(int irq, bool enable, uint8_t priority);
#define NX_GD303_READ(reg)          nx_gd303_model_read(reg)
#define NX_GD303_WRITE(reg, value)  nx_gd303_model_write((reg), (value))
#define NX_GD303_IRQ_AVAILABLE(irq) (!g_gd303_irq[irq] && !g_gd303_pending[irq])
#define NX_GD303_IRQ_CONTROL(irq, enable, priority)                            \
    nx_gd303_model_irq((irq), (enable), (priority))
#define NX_GD303_IRQ_PRIORITY(irq) g_gd303_priority[irq]
#define NX_GD303_IRQ_GROUP()       3U
#define NX_GD303_DENSITY()         g_gd303_density
#define NX_GD303_CPU_INIT()        ((void)0)
#ifdef __cplusplus
}
#endif
#endif
