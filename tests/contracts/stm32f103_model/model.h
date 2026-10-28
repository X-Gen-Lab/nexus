/**
 * \file            model.h
 * \brief           F103 external register, W0C and NVIC model boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F103_MODEL_H
#define NX_STM32F103_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32f103xb.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RCC_TypeDef rcc;
    FLASH_TypeDef flash;
    TIM_TypeDef timer;
    TIM_TypeDef pwm;
    ADC_TypeDef adc;
    GPIO_TypeDef gpio;
    AFIO_TypeDef afio;
    EXTI_TypeDef exti;
    SPI_TypeDef spi;
    uint8_t spi_response;
    bool spi_pending;
    bool hold_spi_rx;
    bool hold_spi_reset;
    bool hold_cs_high;
    bool spi_cs_violation;
    unsigned cs_assertions;
    unsigned cs_releases;
    uint32_t advance_on_cs_release;
    bool refuse_exti_pending;
    bool reassert_exti_pending;
    uint32_t refused_falling;
    unsigned irq_enables[43];
    IWDG_TypeDef watchdog;
    DBGMCU_TypeDef debug;
    uint32_t debug_control_reads;
    uint32_t debug_control_writes;
    bool hold_lsi;
    bool hold_lsi_off;
    bool hold_watchdog_update;
    bool watchdog_enabled;
    uint32_t feeds;
    USART_TypeDef* uart;
    uint16_t flash_kib;
    uint16_t device_id;
    uint32_t device_id_reads;
    uint32_t writes;
    uint32_t ungated_timer_writes;
    uint32_t timer_writes;
    uint32_t timer_ug_pulses;
    bool hide_timer_when_gated;
    bool pend_timer_on_dier_write;
    bool privileged;
    uint32_t reads;
    uint32_t transmitted;
    uint8_t last_tx;
    bool hold_irq_on;
    bool hold_irq_off;
    bool hold_timer_ack;
    bool hold_timer_source_off;
    bool hold_timer_gate_off;
    bool hold_update;
    bool hold_counter;
    bool irq_enabled[43];
    bool irq_pending[43];
    bool irq_active[43];
    uint8_t priority[43];
    uint8_t group;
    volatile uint32_t* ignore_write;
} nx_f103_model_t;
extern nx_f103_model_t g_f103_model;
void nx_f103_model_reset(void);
void nx_f103_model_spi_poll(void* port);
void nx_f103_model_spi_write(SPI_TypeDef* spi, uint8_t byte);
uint8_t nx_f103_model_spi_read(SPI_TypeDef* spi);
void nx_f103_model_watchdog_poll(void* port);
void nx_f103_model_device_poll(unsigned kind, void* port);
bool nx_f103_model_privileged(void);
uint32_t nx_f103_model_device_id(void);
uint32_t nx_f103_model_read(volatile uint32_t* reg);
void nx_f103_model_write(volatile uint32_t* reg, uint32_t value);
nx_result_t nx_f103_model_irq_control(int irq, bool enabled, uint8_t priority);
#define NX_F103_WATCHDOG_POLL(port)        nx_f103_model_watchdog_poll(port)
#define NX_F103_SPI_POLL(port)             nx_f103_model_spi_poll(port)
#define NX_F103_SPI_WRITE_DATA(regs, byte) nx_f103_model_spi_write(regs, byte)
#define NX_F103_SPI_READ_DATA(regs)        nx_f103_model_spi_read(regs)
#define NX_F103_MODEL                      1
#define NX_LEGACY_READ(reg)                nx_f103_model_read(reg)
#define NX_LEGACY_WRITE(reg, value)        nx_f103_model_write(reg, value)
#define NX_LEGACY_POLL(kind, port)         nx_f103_model_device_poll(kind, port)
#define nx_arch_is_privileged()            nx_f103_model_privileged()
#define NX_F103_READ(reg)                  nx_f103_model_read(reg)
#define NX_F103_WRITE(reg, value)          nx_f103_model_write((reg), (value))
#define NX_F103_IRQ_AVAILABLE(irq)                                             \
    (!g_f103_model.irq_enabled[(irq)] && !g_f103_model.irq_pending[(irq)] &&   \
     !g_f103_model.irq_active[(irq)])
#define NX_F103_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_f103_model_irq_control((irq), (enabled), (priority))
#define NX_F103_IRQ_PRIORITY(irq)         g_f103_model.priority[(irq)]
#define NX_F103_IRQ_GROUP()               g_f103_model.group
#define NX_F103_FLASH_KIB()               g_f103_model.flash_kib
#define NX_F103_DEVICE_ID()               nx_f103_model_device_id()
#define NX_F103_UART_TRANSMIT(regs, byte) nx_f103_model_write(&(regs)->DR, byte)
#define NX_F103_UART_RECEIVE(regs)        ((uint8_t)nx_f103_model_read(&(regs)->DR))
#define NX_F103_GPIOA                     (&g_f103_model.gpio)
#define NX_F103_AFIO                      (&g_f103_model.afio)
#undef GPIOA
#undef AFIO
#undef RCC
#undef EXTI
#undef ADC1
#undef TIM3
#undef FLASH
#undef SPI1
#define GPIOA NX_F103_GPIOA
#define AFIO  NX_F103_AFIO
#define RCC   (&g_f103_model.rcc)
#define EXTI  (&g_f103_model.exti)
#define ADC1  (&g_f103_model.adc)
#define TIM3  (&g_f103_model.pwm)
#define FLASH (&g_f103_model.flash)
#define SPI1  (&g_f103_model.spi)
#ifdef __cplusplus
}
#endif
#endif
