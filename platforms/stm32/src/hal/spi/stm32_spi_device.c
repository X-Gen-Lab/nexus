/**
 * \file            stm32_spi_device.c
 * \brief           STM32 SPI device registration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-02-05
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI device registration using Kconfig-driven
 *                  configuration with dynamic memory allocation.
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include "hal/system/nx_mem.h"
#include "nexus_config.h"
#include "stm32_spi.h"
#include "stm32_spi_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define DEVICE_TYPE NX_SPI

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Base interface getters */
static nx_tx_async_t* spi_get_tx_async_handle(nx_spi_bus_t* self,
                                              nx_spi_device_config_t config);
static nx_tx_rx_async_t*
spi_get_tx_rx_async_handle(nx_spi_bus_t* self, nx_spi_device_config_t config,
                           nx_comm_callback_t callback, void* user_data);
static nx_tx_sync_t* spi_get_tx_sync_handle(nx_spi_bus_t* self,
                                            nx_spi_device_config_t config);
static nx_tx_rx_sync_t*
spi_get_tx_rx_sync_handle(nx_spi_bus_t* self, nx_spi_device_config_t config);
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_bus_t* self);
static nx_power_t* spi_get_power(nx_spi_bus_t* self);

/* Interface implementations (defined in separate files) */
extern void spi_init_tx_async(nx_tx_async_t* tx_async);
extern void spi_init_tx_rx_async(nx_tx_rx_async_t* tx_rx_async);
extern void spi_init_tx_sync(nx_tx_sync_t* tx_sync);
extern void spi_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync);
extern void spi_init_lifecycle(nx_lifecycle_t* lifecycle);
extern void spi_init_power(nx_power_t* power);

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from base interface
 */
static inline stm32_spi_impl_t* spi_get_impl(nx_spi_bus_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, base) : NULL;
}

/*---------------------------------------------------------------------------*/
/* Base Interface Getters                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get TX async handle
 */
static nx_tx_async_t* spi_get_tx_async_handle(nx_spi_bus_t* self,
                                              nx_spi_device_config_t config) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    if (!impl) {
        return NULL;
    }

    /* Store current device configuration */
    impl->current_config = config;

    return &impl->tx_async;
}

/**
 * \brief           Get TX/RX async handle
 */
static nx_tx_rx_async_t*
spi_get_tx_rx_async_handle(nx_spi_bus_t* self, nx_spi_device_config_t config,
                           nx_comm_callback_t callback, void* user_data) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    if (!impl) {
        return NULL;
    }

    /* Store current device configuration */
    impl->current_config = config;

    /* Store callback (if needed for async operations) */
    (void)callback;
    (void)user_data;

    return &impl->tx_rx_async;
}

/**
 * \brief           Get TX sync handle
 */
static nx_tx_sync_t* spi_get_tx_sync_handle(nx_spi_bus_t* self,
                                            nx_spi_device_config_t config) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    if (!impl) {
        return NULL;
    }

    /* Store current device configuration */
    impl->current_config = config;

    return &impl->tx_sync;
}

/**
 * \brief           Get TX/RX sync handle
 */
static nx_tx_rx_sync_t*
spi_get_tx_rx_sync_handle(nx_spi_bus_t* self, nx_spi_device_config_t config) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    if (!impl) {
        return NULL;
    }

    /* Store current device configuration */
    impl->current_config = config;

    return &impl->tx_rx_sync;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* spi_get_lifecycle(nx_spi_bus_t* self) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->lifecycle : NULL;
}

/**
 * \brief           Get power interface
 */
static nx_power_t* spi_get_power(nx_spi_bus_t* self) {
    stm32_spi_impl_t* impl = spi_get_impl(self);
    return impl ? &impl->power : NULL;
}

/*---------------------------------------------------------------------------*/
/* Instance Initialization                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize SPI instance with platform configuration
 */
static void spi_init_instance(stm32_spi_impl_t* impl, uint8_t index,
                              const stm32_spi_platform_config_t* platform_cfg) {
    /* Initialize base interface using NX_INIT_SPI_BUS macro */
    NX_INIT_SPI_BUS(&impl->base, spi_get_tx_async_handle,
                    spi_get_tx_rx_async_handle, spi_get_tx_sync_handle,
                    spi_get_tx_rx_sync_handle, spi_get_lifecycle,
                    spi_get_power);

    /* Initialize interfaces (implemented in separate files) */
    spi_init_tx_async(&impl->tx_async);
    spi_init_tx_rx_async(&impl->tx_rx_async);
    spi_init_tx_sync(&impl->tx_sync);
    spi_init_tx_rx_sync(&impl->tx_rx_sync);
    spi_init_lifecycle(&impl->lifecycle);
    spi_init_power(&impl->power);

    /* Allocate and initialize state */
    impl->state = (stm32_spi_state_t*)nx_mem_alloc(sizeof(stm32_spi_state_t));
    if (!impl->state) {
        return;
    }
    memset(impl->state, 0, sizeof(stm32_spi_state_t));

    impl->state->instance = index;
    impl->state->initialized = false;
    impl->state->suspended = false;
    impl->state->busy = false;

    /* Configure ST HAL SPI handle */
    impl->hspi.Instance = platform_cfg->spi_base;
    impl->hspi.Init.Mode = platform_cfg->mode;
    impl->hspi.Init.Direction = platform_cfg->direction;
    impl->hspi.Init.DataSize = platform_cfg->data_size;
    impl->hspi.Init.CLKPolarity = platform_cfg->clk_polarity;
    impl->hspi.Init.CLKPhase = platform_cfg->clk_phase;
    impl->hspi.Init.NSS = platform_cfg->nss;
    impl->hspi.Init.BaudRatePrescaler = platform_cfg->baud_prescaler;
    impl->hspi.Init.FirstBit = platform_cfg->first_bit;
    impl->hspi.Init.TIMode = platform_cfg->ti_mode;
    impl->hspi.Init.CRCCalculation = platform_cfg->crc_calculation;
    impl->hspi.Init.CRCPolynomial = platform_cfg->crc_polynomial;


    /* Initialize DMA configuration */
    memset(&impl->dma, 0, sizeof(stm32_spi_dma_t));
    impl->dma.dma_tx_enabled = platform_cfg->use_dma;
    impl->dma.dma_rx_enabled = platform_cfg->use_dma;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* OSAL objects will be created in lifecycle_init */
    impl->mutex = NULL;
    impl->dma_sem = NULL;
#endif
}

/*---------------------------------------------------------------------------*/
/* Device Registration                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device initialization function for Kconfig registration
 */
NX_UNUSED static void* stm32_spi_device_init(const nx_device_t* dev) {
    const stm32_spi_platform_config_t* config =
        (const stm32_spi_platform_config_t*)dev->config;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate implementation structure */
    stm32_spi_impl_t* impl =
        (stm32_spi_impl_t*)nx_mem_alloc(sizeof(stm32_spi_impl_t));
    if (!impl) {
        return NULL;
    }
    memset(impl, 0, sizeof(stm32_spi_impl_t));

    /* Initialize instance with platform configuration */
    spi_init_instance(impl, config->spi_index, config);

    /* Check if state allocation succeeded */
    if (!impl->state) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Store device reference */
    impl->device = (nx_device_t*)dev;

    /* Device is created but not initialized - user will call lifecycle init()
     */
    return &impl->base;
}

/**
 * \brief           Configuration macro - reads from Kconfig
 */
#define STM32_SPI_CONFIG(index)                                                \
    static const stm32_spi_platform_config_t spi_config_##index = {            \
        .spi_index = index,                                                    \
        .spi_base = SPI##index,                                                \
        .mode = SPI_MODE_MASTER,                                               \
        .direction = SPI_DIRECTION_2LINES,                                     \
        .data_size = SPI_DATASIZE_8BIT,                                        \
        .clk_polarity = SPI_POLARITY_LOW,                                      \
        .clk_phase = SPI_PHASE_1EDGE,                                          \
        .nss = SPI_NSS_SOFT,                                                   \
        .baud_prescaler = SPI_BAUDRATEPRESCALER_16,                            \
        .first_bit = SPI_FIRSTBIT_MSB,                                         \
        .ti_mode = SPI_TIMODE_DISABLE,                                         \
        .crc_calculation = SPI_CRCCALCULATION_DISABLE,                         \
        .crc_polynomial = 7,                                                   \
        .use_osal = false,                                                     \
        .use_dma = false,                                                      \
    }

/**
 * \brief           Device registration macro
 */
#define STM32_SPI_DEVICE_REGISTER(index)                                       \
    STM32_SPI_CONFIG(index);                                                   \
    static nx_device_config_state_t spi_kconfig_state_##index = {              \
        .init_res = 0,                                                         \
        .initialized = false,                                                  \
    };                                                                         \
    NX_DEVICE_REGISTER(DEVICE_TYPE, index, "SPI" #index, &spi_config_##index,  \
                       &spi_kconfig_state_##index, stm32_spi_device_init);

/**
 * \brief           Register all enabled SPI instances
 */
NX_TRAVERSE_EACH_INSTANCE(STM32_SPI_DEVICE_REGISTER, DEVICE_TYPE)
