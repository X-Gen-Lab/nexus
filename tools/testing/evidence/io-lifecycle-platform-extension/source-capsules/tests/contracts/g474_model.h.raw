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
    USART_TypeDef* uart;
    bool hold_hse;
    bool hold_hsi;
    bool hold_pll;
    bool hold_switch;
    bool hold_vos;
    bool hold_uart_on;
    bool hold_uart_off;
    bool hold_irq_on;
    bool hold_irq_off;
    bool boost_delayed;
    bool unsafe_order;
    volatile uint32_t* ignore_write;
    bool irq_enabled[102];
    uint8_t priority[102];
    uint8_t group;
    uint16_t flash_kib;
    uint16_t device_id;
    bool privileged;
    uint32_t reads;
    uint32_t writes;
    uint32_t transmitted;
} nx_g474_model_t;
extern nx_g474_model_t g_g474_model;
void nx_g474_model_reset(void);
bool nx_g474_model_privileged(void);
uint32_t nx_g474_model_read(volatile uint32_t* reg);
void nx_g474_model_write(volatile uint32_t* reg, uint32_t value);
void nx_g474_model_system_poll(void* system);
void nx_g474_model_uart_poll(void* port);
void nx_g474_model_boost_delay(void* system);
nx_result_t nx_g474_model_irq_control(int irq, bool enabled, uint8_t priority);
#define nx_arch_is_privileged()     nx_g474_model_privileged()
#define NX_G474_MODEL               1
#define NX_G474_IRQ_AVAILABLE(irq)  (!g_g474_model.irq_enabled[(irq)])
#define NX_G474_GPIOA               (&g_g474_model.gpio)
#define NX_G474_READ(reg)           nx_g474_model_read(reg)
#define NX_G474_WRITE(reg, value)   nx_g474_model_write((reg), (value))
#define NX_G474_SYSTEM_POLL(system) nx_g474_model_system_poll(system)
#define NX_G474_UART_POLL(port)     nx_g474_model_uart_poll(port)
#define NX_G474_BOOST_DELAY(system) nx_g474_model_boost_delay(system)
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
