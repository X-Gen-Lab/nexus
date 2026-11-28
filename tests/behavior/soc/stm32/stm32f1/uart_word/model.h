/**
 * \file            model.h
 * \brief           Register side effects of the exact F103 USART2 word path.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_UART_WORD_MODEL_H
#define NEXUS_TEST_F1_UART_WORD_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Parsed by the official LL header on Native; selected code never calls them.
 */
uint32_t __LDREXW(volatile uint32_t* address);
uint32_t __STREXW(uint32_t value, volatile uint32_t* address);

uint32_t nx_uart_word_model_status(void);
uint16_t nx_uart_word_model_receive(void);
void nx_uart_word_model_transmit(uint16_t word);
void nx_uart_word_model_write(volatile uint32_t* reg, uint32_t value);
void nx_uart_word_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
bool nx_uart_word_model_irq_enabled(void);
bool nx_uart_word_model_irq_pending(void);
bool nx_uart_word_model_irq_active(void);
uint32_t nx_uart_word_model_irq_priority(void);
void nx_uart_word_model_irq_set_priority(uint32_t value);
void nx_uart_word_model_irq_enable(void);
void nx_uart_word_model_irq_disable(void);
void nx_uart_word_model_irq_clear(void);
#ifdef __cplusplus
}
#endif

#define NX_STM32F1_UART_WORD_STATUS()       nx_uart_word_model_status()
#define NX_STM32F1_UART_WORD_RECEIVE()      nx_uart_word_model_receive()
#define NX_STM32F1_UART_WORD_TRANSMIT(word) nx_uart_word_model_transmit(word)
#define NX_STM32F1_WRITE(reg, value)        nx_uart_word_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value)    nx_uart_word_model_bsrr((reg), (value))
#define NX_STM32F1_WORD_IRQ_ENABLED()       nx_uart_word_model_irq_enabled()
#define NX_STM32F1_WORD_IRQ_PENDING()       nx_uart_word_model_irq_pending()
#define NX_STM32F1_WORD_IRQ_ACTIVE()        nx_uart_word_model_irq_active()
#define NX_STM32F1_WORD_IRQ_PRIORITY()      nx_uart_word_model_irq_priority()
#define NX_STM32F1_WORD_IRQ_SET_PRIORITY(value)                                \
    nx_uart_word_model_irq_set_priority(value)
#define NX_STM32F1_WORD_IRQ_ENABLE()  nx_uart_word_model_irq_enable()
#define NX_STM32F1_WORD_IRQ_DISABLE() nx_uart_word_model_irq_disable()
#define NX_STM32F1_WORD_IRQ_CLEAR()   nx_uart_word_model_irq_clear()
#endif
