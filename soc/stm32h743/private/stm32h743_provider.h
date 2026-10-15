/**
 * \file            stm32h743_provider.h
 * \brief           Private allocation-free GPIO and USART3 IRQ-byte bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32H743_PROVIDER_H
#define NX_STM32H743_PROVIDER_H
#include "nexus/io/gpio.h"
#include "nexus/io/uart.h"
#include "stm32h743_system.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_stm32h743_gpio_state_t;
typedef struct {
    USART_TypeDef* registers;
    const nx_irq_wake_t* wake;
    nx_uart_tx_request_t* active;
    void* rx_storage;
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    size_t tx_position;
    uint32_t rx_loss;
    uint32_t baud;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    nx_irq_policy_t irq_policy;
    IRQn_Type irq;
    nx_uart_rx_profile_t profile;
    uint16_t wake_publishers;
    bool tx_complete;
    bool closing;
    bool initialized;
    bool retained;
} nx_stm32h743_uart_state_t;
extern const nx_gpio_ops_t nx_stm32h743_gpio_ops;
extern const nx_uart_ops_t nx_stm32h743_uart_ops;
/** \brief Cold owner configures authorized pins after acquiring clocks. */
nx_result_t nx_stm32h743_gpio_initialize(nx_stm32h743_gpio_state_t* port,
                                         uint32_t initial);
/** \brief Preserve inactive output levels before withdrawing the face. */
nx_result_t nx_stm32h743_gpio_stop(nx_stm32h743_gpio_state_t* port,
                                   uint32_t inactive);
/** \brief Acquire a reviewed GPIO port clock with readback and barrier. */
nx_result_t nx_stm32h743_gpio_clock_enable(unsigned port_index);
/** \brief Source-reviewed PD8/PD9 AF7; caller owns Board bridge choice. */
nx_result_t nx_stm32h743_usart3_pins_prepare(void);
/**
 * \brief Configure explicit byte ring,8N1,PCLK/PRESC1; no FIFO/DMA.
 * \param[in,out] port: Exact static instance with unused hardware and ring.
 * \param[in] clock_hz: Verified 32MHz or 64MHz APB1 fixed profile.
 * \param[in] poll_limit: Nonzero bounded transmitter/receiver ACK budget.
 * \return Success or error; retained/closing expose failed hardware cleanup.
 * \note Privileged single task owner; IRQ only moves one TX and one RX byte.
 *       Ring and wake live until stop succeeds and external consumers join.
 */
nx_result_t nx_stm32h743_uart_initialize(nx_stm32h743_uart_state_t* port,
                                         uint32_t clock_hz,
                                         uint32_t poll_limit);
/** \brief Move at most one byte each direction and latch TC/error facts. */
void nx_stm32h743_uart_irq(nx_stm32h743_uart_state_t* port);
/** \brief Private bounded RX path shared by IRQ-byte and DMA TX providers. */
void nx_stm32h743_uart_receive_irq(nx_stm32h743_uart_state_t* port,
                                   uint32_t status);
/** \brief Publish an IRQ hint while accounting for a retained wake observer. */
void nx_stm32h743_uart_signal(nx_stm32h743_uart_state_t* port);
#ifndef NX_H743_UART_POLL
#define NX_H743_UART_POLL(port) ((void)(port))
#endif
#ifndef NX_H743_IRQ_PRIORITY
#define NX_H743_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_H743_IRQ_GROUP
#define NX_H743_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
#ifdef __cplusplus
}
#endif
#endif
