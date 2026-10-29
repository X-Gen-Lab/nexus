/**
 * \file            gd32_uart_cold_factory_model.h
 * \brief           External raw NVIC and GPIO command semantics
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_UART_COLD_FACTORY_MODEL_H
#define NEXUS_GD32_UART_COLD_FACTORY_MODEL_H
#include "gd32f4xx.h"
#ifndef NVIC_BASE
#define NVIC_BASE ((uintptr_t)NVIC)
#endif
#ifdef __cplusplus
extern "C" {
#endif
void nx_gd32_uart_cold_model_write(volatile uint32_t* reg, uint32_t value);
void nx_gd32_uart_cold_model_enable(IRQn_Type irq);
void nx_gd32_uart_cold_model_priority(IRQn_Type irq, uint32_t priority);
void nx_uart_cold_model_priority_write(volatile uint8_t* reg, uint8_t value);
void nx_uart_cold_model_disable(IRQn_Type irq);
void nx_uart_cold_model_pending_clear(IRQn_Type irq);
uint32_t nx_uart_cold_model_priority_read(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_gd32_uart_cold_model_write(&(reg), (uint32_t)(value))
#define NX_PLATFORM_PRIORITY_WRITE(reg, value)                                 \
    nx_uart_cold_model_priority_write(&(reg), (uint8_t)(value))
#define NX_UART_REG_WRITE(address, value)                                      \
    nx_gd32_uart_cold_model_write((address), (uint32_t)(value))
#define NVIC_EnableIRQ(irq)       nx_gd32_uart_cold_model_enable(irq)
#define NVIC_DisableIRQ(irq)      nx_uart_cold_model_disable(irq)
#define NVIC_ClearPendingIRQ(irq) nx_uart_cold_model_pending_clear(irq)
#define NVIC_SetPriority(irq, priority)                                        \
    nx_gd32_uart_cold_model_priority(irq, priority)
#define NVIC_GetPriority(irq) nx_uart_cold_model_priority_read(irq)
#endif
