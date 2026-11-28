/**
 * \file            instance.h
 * \brief           Private F103 IRQ USART binding and single-owner state.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_INSTANCE_H
#define NEXUS_STM32F1_UART_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Generated default route, clock and profile-specific RX storage. */
typedef struct {
    USART_TypeDef* registers;
    GPIO_TypeDef* tx_registers;
    GPIO_TypeDef* rx_registers;
    IRQn_Type irq;
    uint32_t clock_hz;
    uint32_t baud;
    void* rx_storage;
    size_t rx_capacity;
    nx_uart_rx_profile_t profile;
    uint8_t tx_pin;
    uint8_t rx_pin;
    uint8_t priority;
} nx_stm32f1_uart_config_t;

/** \brief Retained cold receipts and one IRQ TX / RX stream per instance. */
typedef struct {
    const nx_stm32f1_uart_config_t* config;
    nx_stm32f1_pads_t pads;
    nx_uart_tx_request_t* active;
    size_t sent;
    size_t rx_head;
    size_t rx_count;
    nx_time_us_t drain_deadline;
    nx_time_us_t loss_time;
    nx_result_t terminal;
    uint8_t previous_priority;
    bool loss;
    bool wire_done;
    bool live;
    bool closing;
    bool vector_owned;
    bool controls_owned;
    bool usart_gate_added;
    bool afio_gate_added;
} nx_stm32f1_uart_t;

nx_result_t nx_stm32f1_uart_admit(const nx_stm32f1_uart_t* state,
                                  const nx_stm32f1_uart_config_t* config);
nx_result_t nx_stm32f1_uart_prepare(nx_stm32f1_uart_t* state,
                                    const nx_stm32f1_uart_config_t* config);
nx_result_t nx_stm32f1_uart_close(nx_stm32f1_uart_t* state);
nx_result_t nx_stm32f1_uart_validate(void* context,
                                     const nx_uart_tx_request_t* request);
void nx_stm32f1_uart_irq(nx_stm32f1_uart_t* state);
extern const nx_uart_ops_t nx_stm32f1_uart_ops;

#ifdef __cplusplus
}
#endif
#endif
