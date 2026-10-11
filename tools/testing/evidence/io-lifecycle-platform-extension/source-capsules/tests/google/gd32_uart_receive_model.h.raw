/**
 * \file            gd32_uart_receive_model.h
 * \brief           Separate selected TX and RX DMA disable fault injection
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_UART_RECEIVE_MODEL_H
#define NEXUS_GD32_UART_RECEIVE_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply one real channel7 disable store, with a selected fault. */
void nx_gd32_dma_model_disable(uint32_t mask);
/** \brief Apply one real channel2 disable store, with a selected fault. */
void nx_gd32_uart_rx_model_disable(uint32_t mask);
/** \brief Apply a selected inactive-bank register write or access fault. */
void nx_gd32_uart_receive_model_target(unsigned bank, uint32_t address);
/** \brief Apply hardware write-one-to-clear to channel2 facts only. */
void nx_gd32_uart_receive_model_ack(uint32_t flags);
/** \brief Apply the hardware status/data clear sequence after a DATA read. */
uint32_t nx_gd32_uart_receive_model_data(uint32_t uart);
#ifdef __cplusplus
}
#endif
#define NX_GD32_DMA_DISABLE(mask) nx_gd32_dma_model_disable((uint32_t)(mask))
#define NX_GD32_UART_RX_DMA_DISABLE(mask)                                      \
    nx_gd32_uart_rx_model_disable((uint32_t)(mask))
#define NX_GD32_UART_RECEIVE_TARGET(bank, address)                             \
    nx_gd32_uart_receive_model_target((bank), (address))
#define NX_GD32_UART_RECEIVE_ACK(flags) nx_gd32_uart_receive_model_ack(flags)

#define NX_GD32_UART_RECEIVE_DATA(uart) nx_gd32_uart_receive_model_data((uart))
#endif
