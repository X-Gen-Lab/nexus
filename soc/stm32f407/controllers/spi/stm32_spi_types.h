/**
 * \file            stm32_spi_types.h
 * \brief           STM32 SPI driver type definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_SPI_TYPES_H
#define STM32_SPI_TYPES_H

#include "nexus_config.h"
#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_spi.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

/* Private implementation types for the maintained STM32F407 SoC. */
#include "stm32f4xx_hal.h"

/* Include OSAL headers if enabled */
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
#include "osal/osal_mutex.h"
#include "osal/osal_sem.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded pools: no allocation occurs on the transfer path. */
#define STM32_SPI_MAX_DEVICES 8U
#define STM32_SPI_ASYNC_BYTES 256U
#define STM32_SPI_MAX_BUSES 6U

typedef struct stm32_spi_impl_s stm32_spi_impl_t;
typedef struct stm32_spi_platform_config_s {
    SPI_TypeDef* spi_base;
    uint8_t spi_index;
    uint32_t mode, direction, data_size, clk_polarity, clk_phase, nss;
    uint32_t baud_prescaler, first_bit, ti_mode, crc_calculation, crc_polynomial;
    bool use_osal, use_dma;
} stm32_spi_platform_config_t;

typedef struct stm32_spi_state_s {
    uint8_t instance;
    bool initialized, suspended, busy, transitioning, fault;
    /* Attempts own cleanup even when initialization returns an error. */
    bool board_owned, hal_owned, dma_owned;
    unsigned users;
} stm32_spi_state_t;

typedef enum stm32_spi_phase_e {
    STM32_SPI_IDLE, STM32_SPI_WAITING, STM32_SPI_TERMINAL
} stm32_spi_phase_t;

typedef struct stm32_spi_device_s {
    nx_spi_device_t base;
    nx_tx_async_t tx_async;
    nx_tx_rx_async_t tx_rx_async;
    nx_tx_sync_t tx_sync;
    nx_tx_rx_sync_t tx_rx_sync;
    stm32_spi_impl_t* bus;
    nx_spi_device_config_t config; /* immutable while allocated */
    bool allocated, legacy, pending, servicing, completing, cancelled;
    unsigned users;
    uint32_t queued_at, sequence;
    nx_spi_transaction_t queued;
    nx_status_t last_result;
    nx_comm_callback_t receive_callback;
    void* receive_context;
    uint8_t tx_copy[STM32_SPI_ASYNC_BYTES];
    uint8_t rx_copy[STM32_SPI_ASYNC_BYTES];
} stm32_spi_device_t;

struct stm32_spi_impl_s {
    nx_spi_bus_t base;
    nx_lifecycle_t lifecycle;
    nx_power_t power;
    SPI_HandleTypeDef hspi;
    stm32_spi_state_t storage;
    stm32_spi_state_t* state;
    stm32_spi_device_t devices[STM32_SPI_MAX_DEVICES];
    stm32_spi_device_t* active;
    volatile stm32_spi_phase_t phase;
    volatile nx_status_t result;
    bool dma_tx_enabled, dma_rx_enabled, dma_active;
    uint32_t sequence;
    uint64_t next_token;
    bool worker_active;
    nx_power_callback_t power_callback;
    void* power_context;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    osal_mutex_handle_t mutex;
    osal_sem_handle_t dma_sem;
#endif
};

#ifdef __cplusplus
}
#endif
#endif /* STM32_SPI_TYPES_H */
