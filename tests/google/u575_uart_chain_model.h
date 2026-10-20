/**
 * \file            u575_uart_chain_model.h
 * \brief           External GPDMA linked-fetch and payload bus observations
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_CHAIN_MODEL_H
#define NX_U575_UART_CHAIN_MODEL_H

#include "u575_uart_gpdma_model.h"

#ifdef __cplusplus
extern "C" {
#endif
/** \brief External bus events; no request or provider inspection. */
void nx_u575_uart_chain_advance(size_t bytes);
void nx_u575_uart_chain_fetch(void);
void nx_u575_uart_chain_reset(void);
uint32_t nx_u575_uart_chain_model_read(volatile uint32_t* reg);
void nx_u575_uart_chain_model_write(volatile uint32_t* reg, uint32_t value);
uintptr_t nx_u575_uart_chain_node_bus(const void* node);
#define NX_U575_CHAIN_NODE_BUS(node) nx_u575_uart_chain_node_bus(node)
extern uint32_t g_u575_chain_fetches;
extern bool g_u575_chain_fetch_pending;
extern bool g_u575_chain_hold_fetch;
extern bool g_u575_chain_refuse_fetch;
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg)         nx_u575_uart_chain_model_read(reg)
#define NX_U575_WRITE(reg, value) nx_u575_uart_chain_model_write((reg), (value))
#ifdef __cplusplus
}
#endif
#endif
