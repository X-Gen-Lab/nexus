/**
 * \file            stm32_spi_sync.c
 * \brief           STM32 SPI synchronous interface implementation
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
 * \brief           Get implementation from TX sync interface
 */
static inline stm32_spi_impl_t* spi_tx_sync_get_impl(nx_tx_sync_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, tx_sync) : NULL;
}

/**
 * \brief           Get implementation from TX/RX sync interface
 */
static inline stm32_spi_impl_t* spi_tx_rx_sync_get_impl(nx_tx_rx_sync_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, tx_rx_sync) : NULL;
}

/*---------------------------------------------------------------------------*/
/* TX Sync Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Transmit data synchronously
 */
static nx_status_t spi_tx_sync_transmit(nx_tx_sync_t* self, const uint8_t* data,
                                        size_t len, uint32_t timeout_ms) {
    stm32_spi_impl_t* impl = spi_tx_sync_get_impl(self);
    if (!impl || !impl->state || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    HAL_StatusTypeDef status;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Lock mutex for thread safety */
    if (impl->mutex) {
        osal_mutex_lock(impl->mutex, OSAL_WAIT_FOREVER);
    }
#endif

    impl->state->busy = true;

    /* Use DMA or blocking transfer based on configuration */
    if (impl->dma.dma_tx_enabled) {
        status = HAL_SPI_Transmit_DMA(&impl->hspi, (uint8_t*)data, len);
        if (status == HAL_OK) {
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
            /* Wait for DMA completion */
            if (impl->dma_sem) {
                if (osal_sem_wait(impl->dma_sem, timeout_ms) != OSAL_OK) {
                    impl->state->busy = false;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
                    if (impl->mutex) {
                        osal_mutex_unlock(impl->mutex);
                    }
#endif
                    return NX_ERR_TIMEOUT;
                }
            }
#endif
        }
    } else {
        /* Blocking transfer */
        status = HAL_SPI_Transmit(&impl->hspi, (uint8_t*)data, len, timeout_ms);
    }

    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Unlock mutex */
    if (impl->mutex) {
        osal_mutex_unlock(impl->mutex);
    }
#endif

    /* Return result */
    if (status == HAL_OK) {
        return NX_OK;
    } else {
        return (status == HAL_TIMEOUT) ? NX_ERR_TIMEOUT : NX_ERR_IO;
    }
}

/*---------------------------------------------------------------------------*/
/* TX/RX Sync Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Transmit and receive data synchronously
 */
static nx_status_t spi_tx_rx_sync_transmit_receive(nx_tx_rx_sync_t* self,
                                                   const uint8_t* tx_data,
                                                   uint8_t* rx_data, size_t len,
                                                   uint32_t timeout_ms) {
    stm32_spi_impl_t* impl = spi_tx_rx_sync_get_impl(self);
    if (!impl || !impl->state || !tx_data || !rx_data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    HAL_StatusTypeDef status;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Lock mutex for thread safety */
    if (impl->mutex) {
        osal_mutex_lock(impl->mutex, OSAL_WAIT_FOREVER);
    }
#endif

    impl->state->busy = true;

    /* Use DMA or blocking transfer based on configuration */
    if (impl->dma.dma_tx_enabled && impl->dma.dma_rx_enabled) {
        status = spi_dma_transmit_receive(impl, tx_data, rx_data, len);
        if (status == NX_OK) {
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
            /* Wait for DMA completion */
            if (impl->dma_sem) {
                if (osal_sem_wait(impl->dma_sem, timeout_ms) != OSAL_OK) {
                    impl->state->busy = false;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
                    if (impl->mutex) {
                        osal_mutex_unlock(impl->mutex);
                    }
#endif
                    return NX_ERR_TIMEOUT;
                }
            }
#endif
        }
    } else {
        /* Blocking transfer */
        status = HAL_SPI_TransmitReceive(&impl->hspi, (uint8_t*)tx_data,
                                         rx_data, len, timeout_ms);
    }

    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Unlock mutex */
    if (impl->mutex) {
        osal_mutex_unlock(impl->mutex);
    }
#endif

    /* Return result */
    if (status == HAL_OK) {
        return NX_OK;
    } else {
        return (status == HAL_TIMEOUT) ? NX_ERR_TIMEOUT : NX_ERR_IO;
    }
}

/**
 * \brief           Receive data synchronously
 */
static nx_status_t spi_tx_rx_sync_receive(nx_tx_rx_sync_t* self, uint8_t* data,
                                          size_t len, uint32_t timeout_ms) {
    stm32_spi_impl_t* impl = spi_tx_rx_sync_get_impl(self);
    if (!impl || !impl->state || !data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    HAL_StatusTypeDef status;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Lock mutex for thread safety */
    if (impl->mutex) {
        osal_mutex_lock(impl->mutex, OSAL_WAIT_FOREVER);
    }
#endif

    impl->state->busy = true;

    /* Use DMA or blocking transfer based on configuration */
    if (impl->dma.dma_rx_enabled) {
        status = HAL_SPI_Receive_DMA(&impl->hspi, data, len);
        if (status == HAL_OK) {
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
            /* Wait for DMA completion */
            if (impl->dma_sem) {
                if (osal_sem_wait(impl->dma_sem, timeout_ms) != OSAL_OK) {
                    impl->state->busy = false;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
                    if (impl->mutex) {
                        osal_mutex_unlock(impl->mutex);
                    }
#endif
                    return NX_ERR_TIMEOUT;
                }
            }
#endif
        }
    } else {
        /* Blocking transfer */
        status = HAL_SPI_Receive(&impl->hspi, data, len, timeout_ms);
    }

    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Unlock mutex */
    if (impl->mutex) {
        osal_mutex_unlock(impl->mutex);
    }
#endif

    /* Return result */
    if (status == HAL_OK) {
        return NX_OK;
    } else {
        return (status == HAL_TIMEOUT) ? NX_ERR_TIMEOUT : NX_ERR_IO;
    }
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize TX sync interface
 */
void spi_init_tx_sync(nx_tx_sync_t* tx_sync) {
    NX_INIT_TX_SYNC(tx_sync, spi_tx_sync_transmit);
}

/**
 * \brief           Initialize TX/RX sync interface
 */
void spi_init_tx_rx_sync(nx_tx_rx_sync_t* tx_rx_sync) {
    NX_INIT_TX_RX_SYNC(tx_rx_sync, spi_tx_rx_sync_transmit_receive,
                       spi_tx_rx_sync_receive);
}
