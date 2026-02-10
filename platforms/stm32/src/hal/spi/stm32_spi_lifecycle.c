/**
 * \file            stm32_spi_lifecycle.c
 * \brief           STM32 SPI lifecycle interface implementation
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_spi.h"
#include "stm32_spi_types.h"

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from lifecycle interface
 */
static inline stm32_spi_impl_t* spi_lifecycle_get_impl(nx_lifecycle_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle) : NULL;
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Interface Implementation                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize SPI peripheral
 */
static nx_status_t spi_lifecycle_init(nx_lifecycle_t* self) {
    stm32_spi_impl_t* impl = spi_lifecycle_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if already initialized */
    if (impl->state->initialized) {
        return NX_OK;
    }

    /* Initialize ST HAL SPI */
    if (HAL_SPI_Init(&impl->hspi) != HAL_OK) {
        return NX_ERR_IO;
    }

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Create OSAL mutex for thread safety */
    if (osal_mutex_create(&impl->mutex) != OSAL_OK) {
        HAL_SPI_DeInit(&impl->hspi);
        return NX_ERR_RESOURCE;
    }

    /* Create DMA semaphore if DMA is enabled */
    if (impl->dma.dma_tx_enabled || impl->dma.dma_rx_enabled) {
        if (osal_sem_create(&impl->dma_sem, 0, 1) != OSAL_OK) {
            osal_mutex_delete(impl->mutex);
            HAL_SPI_DeInit(&impl->hspi);
            return NX_ERR_RESOURCE;
        }
    }
#endif

    /* Initialize DMA if enabled */
    if (impl->dma.dma_tx_enabled || impl->dma.dma_rx_enabled) {
        nx_status_t status = spi_dma_init(impl);
        if (status != NX_OK) {
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
            if (impl->dma_sem) {
                osal_sem_delete(impl->dma_sem);
            }
            if (impl->mutex) {
                osal_mutex_delete(impl->mutex);
            }
#endif
            HAL_SPI_DeInit(&impl->hspi);
            return status;
        }
    }

    impl->state->initialized = true;
    return NX_OK;
}

/**
 * \brief           Deinitialize SPI peripheral
 */
static nx_status_t spi_lifecycle_deinit(nx_lifecycle_t* self) {
    stm32_spi_impl_t* impl = spi_lifecycle_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check if not initialized */
    if (!impl->state->initialized) {
        return NX_OK;
    }

    /* Deinitialize DMA if enabled */
    if (impl->dma.dma_tx_enabled || impl->dma.dma_rx_enabled) {
        spi_dma_deinit(impl);
    }

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Delete OSAL objects */
    if (impl->dma_sem) {
        osal_sem_delete(impl->dma_sem);
        impl->dma_sem = NULL;
    }
    if (impl->mutex) {
        osal_mutex_delete(impl->mutex);
        impl->mutex = NULL;
    }
#endif

    /* Deinitialize ST HAL SPI */
    HAL_SPI_DeInit(&impl->hspi);

    impl->state->initialized = false;
    return NX_OK;
}

/**
 * \brief           Reset SPI peripheral
 */
static nx_status_t spi_lifecycle_reset(nx_lifecycle_t* self) {
    stm32_spi_impl_t* impl = spi_lifecycle_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Deinitialize and reinitialize */
    nx_status_t status = spi_lifecycle_deinit(self);
    if (status != NX_OK) {
        return status;
    }

    return spi_lifecycle_init(self);
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize lifecycle interface
 */
void spi_init_lifecycle(nx_lifecycle_t* lifecycle) {
    NX_INIT_LIFECYCLE(lifecycle, spi_lifecycle_init, spi_lifecycle_deinit,
                      spi_lifecycle_reset);
}
