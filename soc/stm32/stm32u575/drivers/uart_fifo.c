/**
 * \file            uart_fifo.c
 * \brief           Exact U575 bounded FIFO byte specialization of owned UART
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_uart_fifo.h"
#include "stm32u5xx_ll_usart.h"

#ifndef NX_U575_UART_FIFO_ENABLE
#define NX_U575_UART_FIFO_ENABLE(uart) LL_USART_EnableFIFO(uart)
#endif
#ifndef NX_U575_UART_FIFO_RX_THRESHOLD
#define NX_U575_UART_FIFO_RX_THRESHOLD(uart, value)                            \
    LL_USART_SetRXFIFOThreshold((uart), (value))
#endif
#ifndef NX_U575_UART_FIFO_TX_THRESHOLD
#define NX_U575_UART_FIFO_TX_THRESHOLD(uart, value)                            \
    LL_USART_SetTXFIFOThreshold((uart), (value))
#endif
#ifndef NX_U575_UART_FIFO_RECEIVE
#define NX_U575_UART_FIFO_RECEIVE(uart) LL_USART_ReceiveData8(uart)
#endif
#ifndef NX_U575_UART_FIFO_TRANSMIT
#define NX_U575_UART_FIFO_TRANSMIT(uart, value)                                \
    LL_USART_TransmitData8((uart), (value))
#endif
#ifndef NX_U575_UART_FIFO_IRQ_ENABLED
#define NX_U575_UART_FIFO_IRQ_ENABLED(irq)                                     \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) != 0U)
#endif
#ifndef NX_U575_UART_FIFO_IRQ_PENDING
#define NX_U575_UART_FIFO_IRQ_PENDING(irq)                                     \
    (NVIC_GetPendingIRQ((IRQn_Type)(irq)) != 0U)
#endif
#ifndef NX_U575_UART_FIFO_IRQ_ACTIVE
#define NX_U575_UART_FIFO_IRQ_ACTIVE(irq)                                      \
    (NVIC_GetActive((IRQn_Type)(irq)) != 0U)
#endif

#define NX_UART_FIFO_STATE             nx_stm32u575_uart_fifo_state_t
#define NX_UART_FIFO_BASE              nx_stm32u575_uart_state_t
#define NX_UART_FIFO_OPS               nx_stm32u575_uart_fifo_ops
#define NX_UART_FIFO_INITIALIZE        nx_stm32u575_uart_fifo_initialize
#define NX_UART_FIFO_IRQ               nx_stm32u575_uart_fifo_irq
#define NX_UART_FIFO_RX_READY          USART_ISR_RXNE
#define NX_UART_FIFO_TX_READY          USART_ISR_TXE
#define NX_UART_FIFO_DEPTH             8U
#define NX_UART_FIFO_CLOCK             4000000U
#define NX_UART_FIFO_REGISTERS         USART1_NS
#define NX_UART_FIFO_VECTOR            USART1_IRQn
#define NX_UART_FIFO_READ(reg)         NX_U575_READ(reg)
#define NX_UART_FIFO_WRITE(reg, value) NX_U575_WRITE((reg), (value))
#define NX_UART_FIFO_POLL(port)        NX_U575_UART_POLL(port)
#define NX_UART_FIFO_ENABLE(uart)      NX_U575_UART_FIFO_ENABLE(uart)
#define NX_UART_FIFO_RX_THRESHOLD(uart, value)                                 \
    NX_U575_UART_FIFO_RX_THRESHOLD((uart), (value))
#define NX_UART_FIFO_TX_THRESHOLD(uart, value)                                 \
    NX_U575_UART_FIFO_TX_THRESHOLD((uart), (value))
#define NX_UART_FIFO_RECEIVE(uart) NX_U575_UART_FIFO_RECEIVE(uart)
#define NX_UART_FIFO_TRANSMIT(uart, value)                                     \
    NX_U575_UART_FIFO_TRANSMIT((uart), (value))
#define NX_UART_FIFO_IRQ_ENABLED(irq) NX_U575_UART_FIFO_IRQ_ENABLED(irq)
#define NX_UART_FIFO_IRQ_PENDING(irq) NX_U575_UART_FIFO_IRQ_PENDING(irq)
#define NX_UART_FIFO_IRQ_ACTIVE(irq)  NX_U575_UART_FIFO_IRQ_ACTIVE(irq)
#define NX_UART_FIFO_IRQ_CONTROL(irq, enabled, priority)                       \
    NX_U575_IRQ_CONTROL((irq), (enabled), (priority))
#define NX_UART_FIFO_IRQ_PRIORITY(irq) NX_U575_IRQ_PRIORITY(irq)
#define NX_UART_FIFO_IRQ_GROUP()       NX_U575_IRQ_GROUP()
/* The selected APB2/reset access proof also governs retained cleanup. */
#define NX_UART_FIFO_ACCESS_READY()                                            \
    (g_nx_stm32u575_system.rcc != NULL &&                                      \
     (NX_U575_READ(&g_nx_stm32u575_system.rcc->APB2ENR) &                      \
      RCC_APB2ENR_USART1EN) != 0U &&                                           \
     (NX_U575_READ(&g_nx_stm32u575_system.rcc->APB2RSTR) &                     \
      RCC_APB2RSTR_USART1RST) == 0U)
#define NX_UART_FIFO_CLOCK_READY()                                             \
    (NX_UART_FIFO_ACCESS_READY() && g_nx_stm32u575_system.started &&           \
     g_nx_stm32u575_system.core_hz == NX_UART_FIFO_CLOCK &&                    \
     (NX_U575_READ(&g_nx_stm32u575_system.rcc->CCIPR1) &                       \
      RCC_CCIPR1_USART1SEL) == 0U &&                                           \
     (NX_U575_READ(&g_nx_stm32u575_system.rcc->CFGR1) & RCC_CFGR1_SWS) ==      \
         0U &&                                                                 \
     (NX_U575_READ(&g_nx_stm32u575_system.rcc->CFGR2) &                        \
      (RCC_CFGR2_HPRE | RCC_CFGR2_PPRE2)) == 0U)
#define NX_UART_FIFO_CONTROL_OVERLAP(data, bytes)                              \
    (fifo_overlap((data), (bytes), &g_nx_stm32u575_system,                     \
                  sizeof(g_nx_stm32u575_system)) ||                            \
     fifo_overlap((data), (bytes), g_nx_stm32u575_system.rcc,                  \
                  sizeof(RCC_TypeDef)) ||                                      \
     fifo_overlap((data), (bytes), g_nx_stm32u575_system.timer,                \
                  sizeof(TIM_TypeDef)) ||                                      \
     fifo_overlap((data), (bytes), g_nx_stm32u575_system.flash,                \
                  sizeof(FLASH_TypeDef)))

#include "uart_fifo_impl.h"

nx_result_t nx_stm32u575_uart_fifo_prepare(
    nx_stm32u575_uart_fifo_state_t* port,
    const nx_stm32u575_uart_config_t* config,
    const nx_stm32u575_uart_fifo_profile_t* profile) {
    if (port == NULL || config == NULL || profile == NULL ||
        config->controller != 1U)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32u575_uart_bind(&port->uart, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32u575_usart1_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    port->fifo = profile;
    return nx_stm32u575_uart_fifo_initialize(port, 4000000U, 1000000U);
}
