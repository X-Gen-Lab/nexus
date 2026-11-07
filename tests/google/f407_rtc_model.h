/**
 * \file            f407_rtc_model.h
 * \brief           External F407 RTC register, oscillator and CPU model
 * \author          Nexus Team
 */
#ifndef NX_F407_RTC_MODEL_H
#define NX_F407_RTC_MODEL_H
#include "stm32_official_model.h"
#include "stm32f407_rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RTC_TypeDef rtc;
    RCC_TypeDef rcc;
    PWR_TypeDef power;
    EXTI_TypeDef exti;
    GPIO_TypeDef gpio_c;
    nx_time_us_t now;
    uint32_t writes;
    uint32_t clock_reads;
    uint32_t backup_resets;
    uint32_t illegal_writes;
    uint32_t calendar_reads;
    uint32_t prer_writes;
    uint32_t prer_values[4];
    bool privileged;
    bool masked;
    bool in_isr;
    bool irq_busy;
    uint32_t irq_enabled[3];
    uint32_t irq_pending[3];
    uint32_t irq_active[3];
    bool locked;
    uint8_t key;
    bool hold_lse;
    bool hold_init;
    bool hold_alarm;
    bool hold_clear;
    bool hold_write;
    bool hold_gpio_clock_enable;
    bool hold_gpio_clock_restore;
    bool hold_reload;
    bool reload_pending;
    uint32_t prior_time;
    uint32_t prior_date;
    bool incoherent;
} nx_f407_rtc_model_t;
extern nx_f407_rtc_model_t g_f407_rtc_model;
void nx_f407_rtc_model_reset(bool retained);
void nx_f407_rtc_model_calendar(const nx_rtc_calendar_t* calendar);
uint32_t nx_f407_rtc_model_read(volatile uint32_t* reg);
void nx_f407_rtc_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_f407_rtc_privileged(void);
bool nx_f407_rtc_masked(void);
bool nx_f407_rtc_in_isr(void);
void nx_f407_rtc_dsb(void);
uint32_t nx_f407_rtc_model_irq_enabled(IRQn_Type irq);
uint32_t nx_f407_rtc_model_irq_pending(IRQn_Type irq);
uint32_t nx_f407_rtc_model_irq_active(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_F407_RTC       (&g_f407_rtc_model.rtc)
#define NX_F407_RTC_RCC   (&g_f407_rtc_model.rcc)
#define NX_F407_RTC_PWR   (&g_f407_rtc_model.power)
#define NX_F407_RTC_EXTI  (&g_f407_rtc_model.exti)
#define NX_F407_RTC_GPIOC (&g_f407_rtc_model.gpio_c)
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#define NVIC_GetEnableIRQ(irq)  nx_f407_rtc_model_irq_enabled(irq)
#define NVIC_GetPendingIRQ(irq) nx_f407_rtc_model_irq_pending(irq)
#define NVIC_GetActive(irq)     nx_f407_rtc_model_irq_active(irq)
#define NX_F407_RTC_READ(p)     nx_f407_rtc_model_read(p)
#define NX_F407_RTC_WRITE(p, v) nx_f407_rtc_model_write((p), (v))
/* Replace external MMIO accesses, retaining the official LL inline bodies. */
#undef READ_REG
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define READ_REG(reg)         nx_f407_rtc_model_read(&(reg))
#define WRITE_REG(reg, value) nx_f407_rtc_model_write(&(reg), (value))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG((reg), (READ_REG(reg) & ~(uint32_t)(clear)) | (set))
#define SET_BIT(reg, bits)   MODIFY_REG((reg), 0U, (bits))
#define CLEAR_BIT(reg, bits) MODIFY_REG((reg), (bits), 0U)
#endif
