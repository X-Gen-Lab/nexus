/**
 * \file            u575_uart_chain_factory_model.h
 * \brief           Generated hardware-chain factory external CPU and bus ports
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_CHAIN_FACTORY_MODEL_H
#define NX_U575_UART_CHAIN_FACTORY_MODEL_H

#include "u575_uart_chain_model.h"
#include "u575_uart_gpdma_factory_model.h"
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_u575_uart_chain_model_write(&(reg), (uint32_t)(value))

#endif
