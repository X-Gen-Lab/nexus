/**
 * \file            uart_format_model.h
 * \brief           Observe actual first USART enable with unchanged MMIO state.
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_FORMAT_MODEL_H
#define NEXUS_UART_FORMAT_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_uart_format_model_enable(uintptr_t registers, uint32_t control);
void nx_uart_format_model_write(volatile uint32_t* address, uint32_t value);
uint32_t nx_uart_format_model_irq(unsigned command, int irq, uint32_t value);
void nx_uart_format_model_clock(bool enabled, uint32_t clock);
#ifdef __cplusplus
}
#endif
#define NX_UART_REG_WRITE(address, value)                                      \
    nx_uart_format_model_write((address), (uint32_t)(value))
#define NX_UART_MODEL_IRQ        1
#define NX_UART_IRQ_ENABLE(irq)  nx_uart_format_model_irq(0U, (int)(irq), 0U)
#define NX_UART_IRQ_DISABLE(irq) nx_uart_format_model_irq(1U, (int)(irq), 0U)
#define NX_UART_IRQ_CLEAR_PENDING(irq)                                         \
    nx_uart_format_model_irq(2U, (int)(irq), 0U)
#define NX_UART_IRQ_SET_PRIORITY(irq, value)                                   \
    nx_uart_format_model_irq(3U, (int)(irq), (uint32_t)(value))
#define NX_UART_IRQ_ENABLED(irq)  nx_uart_format_model_irq(4U, (int)(irq), 0U)
#define NX_UART_IRQ_PENDING(irq)  nx_uart_format_model_irq(5U, (int)(irq), 0U)
#define NX_UART_IRQ_ACTIVE(irq)   nx_uart_format_model_irq(6U, (int)(irq), 0U)
#define NX_UART_IRQ_PRIORITY(irq) nx_uart_format_model_irq(7U, (int)(irq), 0U)
#define NX_UART_CLOCK_ON(clock)                                                \
    nx_uart_format_model_clock(true, (uint32_t)(clock))
#define NX_UART_CLOCK_OFF(clock)                                               \
    nx_uart_format_model_clock(false, (uint32_t)(clock))
#define NX_STM32_UART_ENABLE(registers)                                        \
    nx_uart_format_model_enable((uintptr_t)(registers),                        \
                                (registers)->CR1 | USART_CR1_UE)
#define NX_GD32_UART_ENABLE(registers, control)                                \
    nx_uart_format_model_enable((uintptr_t)(registers), (control))
#endif
