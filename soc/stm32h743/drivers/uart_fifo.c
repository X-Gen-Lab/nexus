/**
 * \file            uart_fifo.c
 * \brief           Exact H743 bounded FIFO byte specialization of owned UART
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_uart_fifo.h"
#include "stm32h7xx_ll_usart.h"

#ifndef NX_H743_UART_FIFO_ENABLE
#define NX_H743_UART_FIFO_ENABLE(uart) LL_USART_EnableFIFO(uart)
#endif
#ifndef NX_H743_UART_FIFO_RX_THRESHOLD
#define NX_H743_UART_FIFO_RX_THRESHOLD(uart, value)                            \
    LL_USART_SetRXFIFOThreshold((uart), (value))
#endif
#ifndef NX_H743_UART_FIFO_TX_THRESHOLD
#define NX_H743_UART_FIFO_TX_THRESHOLD(uart, value)                            \
    LL_USART_SetTXFIFOThreshold((uart), (value))
#endif
#ifndef NX_H743_UART_FIFO_RECEIVE
#define NX_H743_UART_FIFO_RECEIVE(uart) LL_USART_ReceiveData8(uart)
#endif
#ifndef NX_H743_UART_FIFO_TRANSMIT
#define NX_H743_UART_FIFO_TRANSMIT(uart, value)                                \
    LL_USART_TransmitData8((uart), (value))
#endif
#ifndef NX_H743_UART_FIFO_IRQ_ENABLED
#define NX_H743_UART_FIFO_IRQ_ENABLED(irq)                                     \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) != 0U)
#endif
#ifndef NX_H743_UART_FIFO_IRQ_PENDING
#define NX_H743_UART_FIFO_IRQ_PENDING(irq)                                     \
    (NVIC_GetPendingIRQ((IRQn_Type)(irq)) != 0U)
#endif
#ifndef NX_H743_UART_FIFO_IRQ_ACTIVE
#define NX_H743_UART_FIFO_IRQ_ACTIVE(irq)                                      \
    (NVIC_GetActive((IRQn_Type)(irq)) != 0U)
#endif

#define NX_UART_FIFO_STATE             nx_stm32h743_uart_fifo_state_t
#define NX_UART_FIFO_BASE              nx_stm32h743_uart_state_t
#define NX_UART_FIFO_OPS               nx_stm32h743_uart_fifo_ops
#define NX_UART_FIFO_INITIALIZE        nx_stm32h743_uart_fifo_initialize
#define NX_UART_FIFO_IRQ               nx_stm32h743_uart_fifo_irq
#define NX_UART_FIFO_RX_READY          USART_ISR_RXNE_RXFNE
#define NX_UART_FIFO_TX_READY          USART_ISR_TXE_TXFNF
#define NX_UART_FIFO_DEPTH             16U
#define NX_UART_FIFO_CLOCK             64000000U
#define NX_UART_FIFO_REGISTERS         USART3
#define NX_UART_FIFO_VECTOR            USART3_IRQn
#define NX_UART_FIFO_READ(reg)         NX_H743_READ(reg)
#define NX_UART_FIFO_WRITE(reg, value) NX_H743_WRITE((reg), (value))
#define NX_UART_FIFO_POLL(port)        NX_H743_UART_POLL(port)
#define NX_UART_FIFO_ENABLE(uart)      NX_H743_UART_FIFO_ENABLE(uart)
#define NX_UART_FIFO_RX_THRESHOLD(uart, value)                                 \
    NX_H743_UART_FIFO_RX_THRESHOLD((uart), (value))
#define NX_UART_FIFO_TX_THRESHOLD(uart, value)                                 \
    NX_H743_UART_FIFO_TX_THRESHOLD((uart), (value))
#define NX_UART_FIFO_RECEIVE(uart) NX_H743_UART_FIFO_RECEIVE(uart)
#define NX_UART_FIFO_TRANSMIT(uart, value)                                     \
    NX_H743_UART_FIFO_TRANSMIT((uart), (value))
#define NX_UART_FIFO_IRQ_ENABLED(irq) NX_H743_UART_FIFO_IRQ_ENABLED(irq)
#define NX_UART_FIFO_IRQ_PENDING(irq) NX_H743_UART_FIFO_IRQ_PENDING(irq)
#define NX_UART_FIFO_IRQ_ACTIVE(irq)  NX_H743_UART_FIFO_IRQ_ACTIVE(irq)
#define NX_UART_FIFO_IRQ_CONTROL(irq, enabled, priority)                       \
    NX_H743_IRQ_CONTROL((irq), (enabled), (priority))
#define NX_UART_FIFO_IRQ_PRIORITY(irq) NX_H743_IRQ_PRIORITY(irq)
#define NX_UART_FIFO_IRQ_GROUP()       NX_H743_IRQ_GROUP()
/* Baseline HSI64 only: high-clock and alternate kernel profiles need
 * independent reviewed fixtures rather than an inherited rate assumption. */
#define NX_UART_FIFO_ACCESS_READY()                                            \
    (g_nx_stm32h743_system.rcc != NULL &&                                      \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->APB1LENR) &                     \
      RCC_APB1LENR_USART3EN) != 0U &&                                          \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->APB1LRSTR) &                    \
      RCC_APB1LRSTR_USART3RST) == 0U)
#define NX_UART_FIFO_CLOCK_READY()                                             \
    (NX_UART_FIFO_ACCESS_READY() && g_nx_stm32h743_system.started &&           \
     g_nx_stm32h743_system.core_hz == NX_UART_FIFO_CLOCK &&                    \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->D2CCIP2R) &                     \
      RCC_D2CCIP2R_USART28SEL) == 0U &&                                        \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->CFGR) & RCC_CFGR_SWS) == 0U &&  \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->CR) & RCC_CR_HSIDIV) == 0U &&   \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->D1CFGR) & RCC_D1CFGR_HPRE) ==   \
         0U &&                                                                 \
     (NX_H743_READ(&g_nx_stm32h743_system.rcc->D2CFGR) &                       \
      RCC_D2CFGR_D2PPRE1) == 0U)
#define NX_UART_FIFO_CONTROL_OVERLAP(data, bytes)                              \
    (fifo_overlap((data), (bytes), &g_nx_stm32h743_system,                     \
                  sizeof(g_nx_stm32h743_system)) ||                            \
     fifo_overlap((data), (bytes), g_nx_stm32h743_system.rcc,                  \
                  sizeof(RCC_TypeDef)) ||                                      \
     fifo_overlap((data), (bytes), g_nx_stm32h743_system.timer,                \
                  sizeof(TIM_TypeDef)) ||                                      \
     fifo_overlap((data), (bytes), g_nx_stm32h743_system.flash,                \
                  sizeof(FLASH_TypeDef)))

#include "uart_fifo_impl.h"
