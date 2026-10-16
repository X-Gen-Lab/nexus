/**
 * \file            g474_model.h
 * \brief           External register and NVIC model boundaries for G474
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_G474_MODEL_H
#define NX_G474_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nx_stm32g474_system;
struct nx_stm32g474_uart_state;
typedef struct {
    RCC_TypeDef rcc;
    FLASH_TypeDef flash;
    PWR_TypeDef power;
    TIM_TypeDef timer;
    GPIO_TypeDef gpio;
    GPIO_TypeDef gpio_b;
    GPIO_TypeDef gpio_c;
    GPIO_TypeDef gpio_d;
    USART_TypeDef* uart;
    USART_TypeDef* other_uarts[5];
    SPI_TypeDef* spi[2];
    TIM_TypeDef* pwm[2];
    uint32_t pwm_compare_live[2];
    uint32_t pwm_updates[2];
    bool pwm_trigger_observed;
    bool hold_pwm_update;
    bool hold_pwm_reset;
    bool hold_pwm_reset_release;
    IWDG_TypeDef* watchdog;
    bool hold_watchdog_update;
    bool hold_spi_progress;
    bool hold_spi_reset;
    bool hold_spi_reset_release;
    bool hold_spi_cs_release;
    uint32_t spi_transmitted;
    bool hold_hse;
    bool hold_lsi_off;
    bool hold_hsi;
    bool hold_pll;
    bool hold_switch;
    bool hold_vos;
    bool hold_uart_on;
    bool hold_uart_off;
    bool hold_irq_on;
    bool hold_irq_off;
    bool hold_timer_count;
    bool hold_timer_reset_release;
    bool hold_timer_trigger_restore;
    bool timer_repend_on_dier_off;
    bool hold_timer_pending_clear;
    bool timer_off_reads_zero;
    bool hold_timer_gate_off;
    bool timer_update_observed;
    bool timer_trigger_observed;
    bool hold_timer_ack;
    bool hold_timer_source_off;
    bool boost_delayed;
    bool unsafe_order;
    volatile uint32_t* ignore_write;
    bool irq_enabled[102];
    bool irq_pending[102];
    bool irq_active[102];
    uint8_t priority[102];
    uint8_t group;
    uint16_t flash_kib;
    uint16_t device_id;
    bool privileged;
    uint32_t reads;
    uint32_t writes;
    uint32_t transmitted;
    void (*critical_observer)(void* context, bool entering);
    void* critical_context;
    bool observing_critical;
    void (*watchdog_start_observer)(void* context);
    void* watchdog_start_context;
    volatile uint32_t* preempt_register;
    volatile uint32_t* pending_register;
    uint32_t preempt_mask;
    uint32_t preempt_value;
    void (*before_irq_save)(void* context);
    void* before_irq_context;
} nx_g474_model_t;
extern nx_g474_model_t g_g474_model;
void nx_g474_model_reset(void);
bool nx_g474_model_privileged(void);
uint32_t nx_g474_model_read(volatile uint32_t* reg);
void nx_g474_model_write(volatile uint32_t* reg, uint32_t value);
void nx_g474_model_system_poll(void* system);
void nx_g474_model_uart_poll(void* port);
void nx_g474_model_spi_poll(void* port);
void nx_g474_model_pwm_poll(void* port);
void nx_g474_model_pwm_update(unsigned index);
void nx_g474_model_spi_write(volatile uint32_t* reg, uint8_t value);
uint8_t nx_g474_model_spi_read(volatile uint32_t* reg);
void nx_g474_model_boost_delay(void* system);
nx_result_t nx_g474_model_irq_control(int irq, bool enabled, uint8_t priority);
nx_arch_irq_state_t nx_g474_model_irq_save(void);
void nx_g474_model_irq_restore(nx_arch_irq_state_t previous);
#define nx_arch_irq_save()         nx_g474_model_irq_save()
#define nx_arch_irq_restore(saved) nx_g474_model_irq_restore(saved)
#define nx_arch_is_privileged()    nx_g474_model_privileged()
#define NX_G474_MODEL              1
#define NX_G474_IRQ_AVAILABLE(irq)                                             \
    (!g_g474_model.irq_enabled[(irq)] && !g_g474_model.irq_pending[(irq)] &&   \
     !g_g474_model.irq_active[(irq)])
#define NX_G474_GPIOA                  (&g_g474_model.gpio)
#define NX_G474_READ(reg)              nx_g474_model_read(reg)
#define NX_G474_WRITE(reg, value)      nx_g474_model_write((reg), (value))
#define NX_G474_SYSTEM_POLL(system)    nx_g474_model_system_poll(system)
#define NX_G474_UART_POLL(port)        nx_g474_model_uart_poll(port)
#define NX_G474_SPI_POLL(port)         nx_g474_model_spi_poll(port)
#define NX_G474_SPI_WRITE8(reg, value) nx_g474_model_spi_write((reg), (value))
#define NX_G474_SPI_READ8(reg)         nx_g474_model_spi_read(reg)
#define NX_G474_PWM_POLL(port)         nx_g474_model_pwm_poll(port)
#define NX_G474_BOOST_DELAY(system)    nx_g474_model_boost_delay(system)
#define NX_G474_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_g474_model_irq_control((irq), (enabled), (priority))
#define NX_G474_IRQ_PRIORITY(irq) g_g474_model.priority[(irq)]
#define NX_G474_IRQ_GROUP()       g_g474_model.group
#define NX_G474_FLASH_KIB()       g_g474_model.flash_kib
#define NX_G474_DEVICE_ID()       g_g474_model.device_id
#define NX_G474_FLASH_OPTIONS()   g_g474_model.flash.OPTR
#ifdef __cplusplus
}
#endif
#endif
