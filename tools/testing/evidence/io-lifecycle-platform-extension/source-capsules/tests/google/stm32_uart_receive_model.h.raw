/**
 * \file            stm32_uart_receive_model.h
 * \brief           Real receive target-store and flag-ack hardware boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_UART_RECEIVE_MODEL_H
#define NEXUS_STM32_UART_RECEIVE_MODEL_H
#include "stm32_dma_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply a selected inactive-bank register write or access fault. */
void nx_stm32_uart_receive_model_target(void* engine, unsigned bank,
                                        uint32_t address);
/** \brief Apply hardware write-one-to-clear to stream2 facts only. */
void nx_stm32_uart_receive_model_ack(void* dma, uint32_t flags);
/** \brief Apply the hardware status/data clear sequence after a DATA read. */
uint32_t nx_stm32_uart_receive_model_data(void* uart);
#ifdef __cplusplus
}
#endif
#define NX_STM32_UART_RECEIVE_TARGET(engine, bank, address)                    \
    nx_stm32_uart_receive_model_target((engine), (bank), (address))
#define NX_STM32_UART_RECEIVE_ACK(dma, flags)                                  \
    nx_stm32_uart_receive_model_ack((dma), (flags))

#define NX_STM32_UART_RECEIVE_DATA(uart)                                       \
    nx_stm32_uart_receive_model_data((uart))
#endif
