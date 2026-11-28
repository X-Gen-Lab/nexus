/**
 * \file            word.h
 * \brief           Private F103RB USART2 9N1 static instance.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_WORD_H
#define NEXUS_STM32F1_UART_WORD_H

#include "drivers/gpio/instance.h"
#include "nexus/io/uart_word.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t* storage;
    uint16_t capacity;
    uint32_t clock_hz;
    uint32_t baud;
    uint8_t priority;
} nx_stm32f1_uart_word_config_t;

typedef struct {
    const nx_stm32f1_uart_word_config_t* config;
    nx_stm32f1_pads_t pads;
    nx_uart_word_tx_request_t* active;
    size_t sent;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint16_t head;
    uint16_t count;
    uint8_t previous_priority;
    bool live;
    bool closing;
    bool loss;
    bool wire_done;
    bool vector_owned;
    bool controls_owned;
    bool usart_gate_added;
    bool afio_gate_added;
} nx_stm32f1_uart_word_t;

nx_result_t
nx_stm32f1_uart_word_admit(const nx_stm32f1_uart_word_t* state,
                           const nx_stm32f1_uart_word_config_t* config);
nx_result_t
nx_stm32f1_uart_word_prepare(nx_stm32f1_uart_word_t* state,
                             const nx_stm32f1_uart_word_config_t* config);
nx_result_t nx_stm32f1_uart_word_close(nx_stm32f1_uart_word_t* state);
void nx_stm32f1_uart_word_irq(nx_stm32f1_uart_word_t* state);
extern const nx_uart_word_ops_t nx_stm32f1_uart_word_ops;

#ifdef __cplusplus
}
#endif
#endif
