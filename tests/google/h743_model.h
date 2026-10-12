/**
 * \file            h743_model.h
 * \brief           External H743 MMIO ready/refusal and IRQ boundaries
 * \author          Nexus Team
 */
#ifndef NX_H743_MODEL_H
#define NX_H743_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
/* Native C++ cannot parse unused CMSIS cache helpers that cast its 64-bit
 * pointers to the 32-bit processor address type. Keep the real CM7/device
 * register definitions; only this host-only translation excludes those Arm
 * helper bodies. Production C and actual ARM consumers use the full header. */
#ifdef __cplusplus
#define ARM_CACHEL1_ARMV7_H
#endif
#include "stm32h743xx.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RCC_TypeDef rcc;
    FLASH_TypeDef flash;
    PWR_TypeDef power;
    TIM_TypeDef timer;
    SCB_Type* core;
    GPIO_TypeDef gpio_b;
    GPIO_TypeDef gpio_d;
    USART_TypeDef* uart;
    bool irq_enabled[150];
    bool irq_pending[150];
    bool irq_active[150];
    uint8_t priority[150];
    uint8_t group;
    uint16_t flash_kib;
    uint16_t device_id;
    bool privileged;
    bool hold_uart_on;
    bool hold_uart_off;
    bool hold_irq_on;
    bool hold_irq_off;
    volatile uint32_t* ignore_write;
    uint32_t reads;
    uint32_t writes;
    uint32_t gpio_writes;
    uint32_t transmitted;
    void (*critical_observer)(void* context, bool entering);
    void* critical_context;
    bool observing_critical;
    void (*before_irq_save)(void* context);
} nx_h743_model_t;
extern nx_h743_model_t g_h743_model;
void nx_h743_model_reset(void);
uint32_t nx_h743_model_read(volatile uint32_t* reg);
void nx_h743_model_write(volatile uint32_t* reg, uint32_t value);
nx_result_t nx_h743_model_irq_control(int irq, bool enabled, uint8_t priority);
void nx_h743_model_uart_poll(void* port);
bool nx_h743_model_privileged(void);
nx_arch_irq_state_t nx_h743_model_irq_save(void);
void nx_h743_model_irq_restore(nx_arch_irq_state_t previous);
#define nx_arch_irq_save()         nx_h743_model_irq_save()
#define nx_arch_irq_restore(saved) nx_h743_model_irq_restore(saved)
#define NX_H743_MODEL              1
#define NX_H743_READ(reg)          nx_h743_model_read(reg)
#define NX_H743_WRITE(reg, value)  nx_h743_model_write((reg), (value))
#define NX_H743_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_h743_model_irq_control((irq), (enabled), (priority))
#define NX_H743_IRQ_AVAILABLE(irq)                                             \
    (!g_h743_model.irq_enabled[(irq)] && !g_h743_model.irq_pending[(irq)] &&   \
     !g_h743_model.irq_active[(irq)])
#define NX_H743_IRQ_PRIORITY(irq) g_h743_model.priority[(irq)]
#define NX_H743_IRQ_GROUP()       g_h743_model.group
#define NX_H743_FLASH_KIB()       g_h743_model.flash_kib
#define NX_H743_DEVICE_ID()       g_h743_model.device_id
#define NX_H743_GPIOB             (&g_h743_model.gpio_b)
#define NX_H743_GPIOD             (&g_h743_model.gpio_d)
#define NX_H743_UART_POLL(port)   nx_h743_model_uart_poll(port)
#define nx_arch_is_privileged()   nx_h743_model_privileged()
#ifdef __cplusplus
}
#endif
#endif
