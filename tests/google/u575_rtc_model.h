/**
 * \file            u575_rtc_model.h
 * \brief           External RTC clock crossing, calendar and register model
 * \author          Nexus Team
 */
#ifndef NX_U575_RTC_MODEL_H
#define NX_U575_RTC_MODEL_H
#include "stm32u575_rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RTC_TypeDef rtc;
    FLASH_TypeDef flash;
    PWR_TypeDef power;
    RCC_TypeDef rcc;
    nx_time_us_t now;
    nx_time_us_t disable_at;
    uint32_t writes;
    uint32_t forbidden_writes;
    uint32_t reads;
    uint32_t calendar_reads;
    uint32_t rtc_reads;
    uint32_t calendar_race_reads;
    uint32_t protected_write_refusals;
    uint32_t wpr_unlocks;
    uint32_t wpr_locks;
    uint8_t wpr_key;
    bool wpr_locked;
    bool hold_unlock;
    bool source_running;
    bool hold_disable;
    bool hold_clear;
    bool hold_restore;
    bool unstable_calendar;
    bool privileged;
    bool irq_enabled;
    bool irq_pending;
    bool irq_active;
} nx_u575_rtc_model_t;
extern nx_u575_rtc_model_t g_u575_rtc_model;
void nx_u575_rtc_model_reset(void);
void nx_u575_rtc_model_step(uint64_t us);
void nx_u575_rtc_model_calendar(const nx_rtc_calendar_t* calendar);
uint32_t nx_u575_rtc_model_read(volatile uint32_t* reg);
void nx_u575_rtc_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_U575_RETAINED_RCC   (&g_u575_rtc_model.rcc)
#define NX_U575_RETAINED_FLASH (&g_u575_rtc_model.flash)
#define NX_U575_RTC            (&g_u575_rtc_model.rtc)
#define NX_U575_RTC_PWR        (&g_u575_rtc_model.power)
#define NX_U575_RTC_IRQ_QUIET()                                                \
    (!g_u575_rtc_model.irq_enabled && !g_u575_rtc_model.irq_pending &&         \
     !g_u575_rtc_model.irq_active)
#define nx_arch_is_privileged() (g_u575_rtc_model.privileged)
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(p)     nx_u575_rtc_model_read(p)
#define NX_U575_WRITE(p, v) nx_u575_rtc_model_write((p), (v))
#endif
