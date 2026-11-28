/**
 * \file            instance.h
 * \brief           Private STM32F1 IRQ USART binding and single-owner state.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_UART_INSTANCE_H
#define NEXUS_STM32F1_UART_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/uart.h"
#include "nexus/io/uart_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Encoded wire facts in the otherwise unused configuration padding. */
#define NX_STM32F1_UART_WORD9  1U
#define NX_STM32F1_UART_PARITY 2U
#define NX_STM32F1_UART_ODD    4U
#define NX_STM32F1_UART_STOP2  8U

/** \brief Encode the reviewed payload/parity/stop facts for immutable C. */
#define NX_STM32F1_UART_FRAME(bits, parity, stop)                              \
    (((bits) == 8U && (parity) != 0U ? NX_STM32F1_UART_WORD9 : 0U) |           \
     (parity) | ((stop) == 2U ? NX_STM32F1_UART_STOP2 : 0U))

#define NX_STM32F1_UART_SYNC_LBCL 1U
#define NX_STM32F1_UART_SYNC_CPHA 2U
#define NX_STM32F1_UART_SYNC_CPOL 4U

/** \brief Encode the three USART clock options for immutable C. */
#define NX_STM32F1_UART_SYNC_OPTIONS(polarity, phase, last)                    \
    (((polarity) ? NX_STM32F1_UART_SYNC_CPOL : 0U) |                           \
     ((phase) ? NX_STM32F1_UART_SYNC_CPHA : 0U) |                              \
     ((last) ? NX_STM32F1_UART_SYNC_LBCL : 0U))

/** \brief Pack Smartcard guard clocks and automatic parity NACK. */
#define NX_STM32F1_UART_CARD_NACK (UINT16_C(1))
#define NX_STM32F1_UART_CARD_OPTIONS(guard, nack)                              \
    (((guard) << 1U) | ((nack) ? NX_STM32F1_UART_CARD_NACK : 0U))

/** \brief Plain byte IRQ when another instance selects a wire protocol. */
#define NX_STM32F1_UART_WIRE_NONE UINT8_MAX

/** \brief Generated route, framing, clock and selected RX storage. */
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
    uint8_t frame;
    uint8_t wire;
    uint16_t wire_options;
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
#if defined(STM32F107xC)
    bool remap_owned;
#endif
} nx_stm32f1_uart_t;

nx_result_t nx_stm32f1_uart_admit(const nx_stm32f1_uart_t* state,
                                  const nx_stm32f1_uart_config_t* config);
nx_result_t nx_stm32f1_uart_prepare(nx_stm32f1_uart_t* state,
                                    const nx_stm32f1_uart_config_t* config);
nx_result_t nx_stm32f1_uart_stop(nx_stm32f1_uart_t* state);
nx_result_t nx_stm32f1_uart_close(nx_stm32f1_uart_t* state);
nx_result_t nx_stm32f1_uart_validate(void* context,
                                     const nx_uart_tx_request_t* request);
void nx_stm32f1_uart_irq(nx_stm32f1_uart_t* state);
void nx_stm32f1_uart_parity_irq(nx_stm32f1_uart_t* state);
extern const nx_uart_ops_t nx_stm32f1_uart_ops;

#ifdef __cplusplus
}
#endif
#endif
