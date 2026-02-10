/**
 * \file            stm32_spi_dma.c
 * \brief           STM32 SPI DMA support implementation
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_spi.h"
#include "stm32_spi_types.h"

/*---------------------------------------------------------------------------*/
/* DMA Initialization                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DMA for SPI
 */
nx_status_t spi_dma_init(stm32_spi_impl_t* impl) {
    if (!impl) {
        return NX_ERR_INVALID_PARAM;
    }

    /* DMA initialization would be done here */
    /* This is platform-specific and depends on DMA channel configuration */
    /* For now, we assume DMA is configured via CubeMX or manually */

    return NX_OK;
}

/**
 * \brief           Deinitialize DMA for SPI
 */
nx_status_t spi_dma_deinit(stm32_spi_impl_t* impl) {
    if (!impl) {
        return NX_ERR_INVALID_PARAM;
    }

    /* DMA deinitialization would be done here */

    return NX_OK;
}

/**
 * \brief           Transmit and receive data using DMA
 */
nx_status_t spi_dma_transmit_receive(stm32_spi_impl_t* impl,
                                     const uint8_t* tx_data, uint8_t* rx_data,
                                     size_t len) {
    if (!impl || !tx_data || !rx_data || len == 0) {
        return NX_ERR_INVALID_PARAM;
    }

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(
        &impl->hspi, (uint8_t*)tx_data, rx_data, len);

    if (status != HAL_OK) {
        return NX_ERR_IO;
    }

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* HAL Callbacks                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI TX complete callback (called by ST HAL)
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find the implementation from the handle */
    stm32_spi_impl_t* impl = NX_CONTAINER_OF(hspi, stm32_spi_impl_t, hspi);

    if (!impl || !impl->state) {
        return;
    }

    /* Update statistics */
    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Release semaphore to signal completion */
    if (impl->dma_sem) {
        osal_sem_post(impl->dma_sem);
    }
#endif
}

/**
 * \brief           SPI RX complete callback (called by ST HAL)
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find the implementation from the handle */
    stm32_spi_impl_t* impl = NX_CONTAINER_OF(hspi, stm32_spi_impl_t, hspi);

    if (!impl || !impl->state) {
        return;
    }

    /* Update statistics */
    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Release semaphore to signal completion */
    if (impl->dma_sem) {
        osal_sem_post(impl->dma_sem);
    }
#endif
}

/**
 * \brief           SPI TX/RX complete callback (called by ST HAL)
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find the implementation from the handle */
    stm32_spi_impl_t* impl = NX_CONTAINER_OF(hspi, stm32_spi_impl_t, hspi);

    if (!impl || !impl->state) {
        return;
    }

    /* Update statistics */
    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Release semaphore to signal completion */
    if (impl->dma_sem) {
        osal_sem_post(impl->dma_sem);
    }
#endif
}

/**
 * \brief           SPI error callback (called by ST HAL)
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi) {
    /* Find the implementation from the handle */
    stm32_spi_impl_t* impl = NX_CONTAINER_OF(hspi, stm32_spi_impl_t, hspi);

    if (!impl || !impl->state) {
        return;
    }

    /* Transmission complete */
    impl->state->busy = false;

#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    /* Release semaphore to signal error */
    if (impl->dma_sem) {
        osal_sem_post(impl->dma_sem);
    }
#endif
}
