/**
 * \file            uart_dma.c
 * \brief           U575 finite single-block GPDMA TX provider
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_gpdma_impl.h"

const nx_uart_ops_t nx_stm32u575_uart_gpdma_ops = {
    submit,     start_admitted, cancel,      service, NULL,
    read_bytes, stop,           attach_wake, NULL,    NULL};
