/**
 * \file            uart_rtscts.c
 * \brief           Exact H743 bounded RTS/CTS FIFO specialization of owned UART
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_cold.h"
#include "stm32h743_uart_rtscts.h"
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

/* This distinct selected specialization has no extra mutable fields.
 * CTS remains active while a borrowed FIFO/shifter drains; software only
 * pauses RX sources so a full ring can apply real hardware backpressure. */
#define NX_UART_FIFO_RTSCTS 1
#define NX_UART_FIFO_HW_FLOW(uart)                                             \
    LL_USART_SetHWFlowCtrl((uart), LL_USART_HWCONTROL_RTS_CTS)

#define NX_UART_FIFO_STATE             nx_stm32h743_uart_rtscts_state_t
#define NX_UART_FIFO_BASE              nx_stm32h743_uart_state_t
#define NX_UART_FIFO_OPS               nx_stm32h743_uart_rtscts_ops
#define NX_UART_FIFO_INITIALIZE        nx_stm32h743_uart_rtscts_initialize
#define NX_UART_FIFO_IRQ               nx_stm32h743_uart_rtscts_irq
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
                  sizeof(FLASH_TypeDef)) ||                                    \
     fifo_overlap((data), (bytes), GPIOD, sizeof(GPIO_TypeDef)))

#include "uart_fifo_impl.h"

nx_result_t nx_stm32h743_usart3_rtscts_pins_prepare(void) {
    if (!fifo_context())
        return NX_ERROR_CONTEXT;
    if (!g_nx_stm32h743_system.started || g_nx_stm32h743_system.rcc == NULL)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32h743_gpio_clock_enable(3U);
    if (result != NX_SUCCESS)
        return result;
    volatile uint32_t* gate = &g_nx_stm32h743_system.rcc->APB1LENR;
    if (!fifo_checked(gate, NX_H743_READ(gate) | RCC_APB1LENR_USART3EN,
                      RCC_APB1LENR_USART3EN))
        return NX_ERROR_IO;
    if (!NX_UART_FIFO_CLOCK_READY())
        return NX_ERROR_STATE;
    if (NX_H743_READ(&USART3->CR1) != 0U || NX_H743_READ(&USART3->CR3) != 0U)
        return NX_ERROR_BUSY;
    GPIO_TypeDef* gpio = GPIOD;
    const uint32_t bits = (1U << 8U) | (1U << 9U) | (1U << 11U) | (1U << 12U);
    const uint32_t fields =
        (3U << 16U) | (3U << 18U) | (3U << 22U) | (3U << 24U);
    const uint32_t speed =
        (1U << 16U) | (1U << 18U) | (1U << 22U) | (1U << 24U);
    const uint32_t pull = (1U << 18U) | (1U << 22U);
    const uint32_t af = (15U << 0U) | (15U << 4U) | (15U << 12U) | (15U << 16U);
    const uint32_t af_value =
        (7U << 0U) | (7U << 4U) | (7U << 12U) | (7U << 16U);
    const uint32_t mode = (2U << 16U) | (2U << 18U) | (2U << 22U) | (2U << 24U);
    /* GPIO latches precede AF ownership. Inactive AF/reset electrical levels
     * and a peer's response are separate physical qualification inputs. */
    NX_H743_WRITE(&gpio->BSRR, (1U << 8U) | (1U << 12U));
    nx_arch_dsb();
    if ((NX_H743_READ(&gpio->ODR) & ((1U << 8U) | (1U << 12U))) !=
        ((1U << 8U) | (1U << 12U)))
        return NX_ERROR_IO;
    if (!fifo_checked(&gpio->OTYPER, NX_H743_READ(&gpio->OTYPER) & ~bits,
                      bits) ||
        !fifo_checked(&gpio->OSPEEDR,
                      (NX_H743_READ(&gpio->OSPEEDR) & ~fields) | speed,
                      fields) ||
        !fifo_checked(&gpio->PUPDR,
                      (NX_H743_READ(&gpio->PUPDR) & ~fields) | pull, fields) ||
        !fifo_checked(&gpio->AFR[1],
                      (NX_H743_READ(&gpio->AFR[1]) & ~af) | af_value, af))
        return NX_ERROR_IO;
    return fifo_checked(&gpio->MODER,
                        (NX_H743_READ(&gpio->MODER) & ~fields) | mode, fields)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

nx_result_t
nx_stm32h743_uart_rtscts_prepare(nx_stm32h743_uart_rtscts_state_t* port,
                                 const nx_stm32h743_uart_config_t* config,
                                 const nx_stm32h743_uart_fifo_profile_t* fifo) {
    if (port == NULL || config == NULL || fifo == NULL)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32h743_uart_configure(&port->uart, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32h743_usart3_rtscts_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    port->fifo = fifo;
    return nx_stm32h743_uart_rtscts_initialize(port, config->clock_hz,
                                               1000000U);
}
