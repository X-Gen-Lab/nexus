/**
 * \file            model.h
 * \brief           Host F103 USART LIN flags, GPIO BSRR and NVIC observations.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32F1_UART_LIN_MODEL_H
#define NEXUS_TEST_STM32F1_UART_LIN_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LL declarations are parsed on Native. No modeled code executes these
 * exclusive-access helpers, so accidental use fails at link time. */
uint32_t __LDREXW(volatile uint32_t* address);
uint32_t __STREXW(uint32_t value, volatile uint32_t* address);

uint32_t nx_f1_model_status(USART_TypeDef* registers);
uint8_t nx_f1_model_receive(USART_TypeDef* registers);
void nx_f1_model_transmit(USART_TypeDef* registers, uint8_t value);
void nx_f1_model_gpio_bsrr(GPIO_TypeDef* registers, uint32_t value);
void nx_f1_model_write(volatile uint32_t* registers, uint32_t value);
bool nx_f1_model_irq_enabled(IRQn_Type irq);
bool nx_f1_model_irq_pending(IRQn_Type irq);
bool nx_f1_model_irq_active(IRQn_Type irq);
uint32_t nx_f1_model_irq_priority(IRQn_Type irq);
void nx_f1_model_irq_set_priority(IRQn_Type irq, uint32_t value);
void nx_f1_model_irq_enable(IRQn_Type irq);
void nx_f1_model_irq_disable(IRQn_Type irq);
void nx_f1_model_irq_clear(IRQn_Type irq);

#ifdef __cplusplus
}
#endif

#define NX_STM32F1_UART_STATUS(reg)  nx_f1_model_status(reg)
#define NX_STM32F1_UART_RECEIVE(reg) nx_f1_model_receive(reg)
#define NX_STM32F1_UART_TRANSMIT(reg, value)                                   \
    nx_f1_model_transmit((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_f1_model_gpio_bsrr((reg), (value))
#define NX_STM32F1_WRITE(reg, value)     nx_f1_model_write((reg), (value))
#define NX_STM32F1_IRQ_ENABLED(irq)      nx_f1_model_irq_enabled(irq)
#define NX_STM32F1_IRQ_PENDING(irq)      nx_f1_model_irq_pending(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)       nx_f1_model_irq_active(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)     nx_f1_model_irq_priority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value)                                \
    nx_f1_model_irq_set_priority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)  nx_f1_model_irq_enable(irq)
#define NX_STM32F1_IRQ_DISABLE(irq) nx_f1_model_irq_disable(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)   nx_f1_model_irq_clear(irq)
#endif
