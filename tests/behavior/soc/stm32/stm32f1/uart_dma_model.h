/**
 * \file            uart_dma_model.h
 * \brief           Host STM32F1 TX DMA, GPIO, USART and NVIC models.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_F1_UART_DMA_MODEL_H
#define NEXUS_TEST_F1_UART_DMA_MODEL_H

#include "stm32f1xx.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t __LDREXW(volatile uint32_t* address);
uint32_t __STREXW(uint32_t value, volatile uint32_t* address);
void nx_dma_model_write(volatile uint32_t* reg, uint32_t value);
void nx_dma_model_bsrr(GPIO_TypeDef* reg, uint32_t value);
uint8_t nx_dma_model_rx(USART_TypeDef* reg);
void nx_dma_model_tc_clear(USART_TypeDef* reg);
bool nx_dma_model_irq_enabled(IRQn_Type irq);
bool nx_dma_model_irq_pending(IRQn_Type irq);
bool nx_dma_model_irq_active(IRQn_Type irq);
uint32_t nx_dma_model_irq_priority(IRQn_Type irq);
void nx_dma_model_irq_priority_set(IRQn_Type irq, uint32_t value);
void nx_dma_model_irq_enable(IRQn_Type irq);
void nx_dma_model_irq_disable(IRQn_Type irq);
void nx_dma_model_irq_clear(IRQn_Type irq);
#ifdef __cplusplus
}
#endif

#define NX_STM32F1_WRITE(reg, value)         nx_dma_model_write((reg), (value))
#define NX_STM32F1_GPIO_BSRR(reg, value)     nx_dma_model_bsrr((reg), (value))
#define NX_STM32F1_UART_STATUS(reg)          ((reg)->SR)
#define NX_STM32F1_UART_RECEIVE(reg)         nx_dma_model_rx(reg)
#define NX_STM32F1_UART_TRANSMIT(reg, value) ((reg)->DR = (value))
#define NX_STM32F1_DMA_CLEAR_TC(reg)         nx_dma_model_tc_clear(reg)
#define NX_STM32F1_IRQ_ENABLED(irq)          nx_dma_model_irq_enabled(irq)
#define NX_STM32F1_IRQ_PENDING(irq)          nx_dma_model_irq_pending(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)           nx_dma_model_irq_active(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)         nx_dma_model_irq_priority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value)                                \
    nx_dma_model_irq_priority_set((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)  nx_dma_model_irq_enable(irq)
#define NX_STM32F1_IRQ_DISABLE(irq) nx_dma_model_irq_disable(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)   nx_dma_model_irq_clear(irq)
#endif
