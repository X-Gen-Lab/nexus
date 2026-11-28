/**
 * \file            main.c
 * \brief           Link MPU-free SPI2 DMA consumer without clocking a target
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

#if defined(NX_SPI_DMA_TRANSACTION_REF_SPI2)
static nx_spi_dma_transaction_request_t request;
static uint16_t transmit[4];
static uint16_t receive[4];
static const nx_spi_dma_segment_t segments[] = {
    {.tx = transmit, .rx = receive, .words = 4U},
};
#else
static nx_spi_request_t request;
static uint8_t transmit[4];
static uint8_t receive[4];
#endif

int main(void) {
    const nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS)
        return 1;
#if defined(NX_SPI_DMA_TRANSACTION_REF_SPI2)
    const nx_spi_dma_transaction_port_t* port =
        nx_factory_spi_dma_transaction_ref(NX_SPI_DMA_TRANSACTION_REF_SPI2);
    const nx_spi_dma_transaction_endpoint_t* endpoint =
        nx_factory_spi_dma_transaction_device_ref(
            NX_SPI_DMA_TRANSACTION_DEVICE_REF_FLASH);
    nx_request_initialize(&request.base);
    if (port == NULL || endpoint == NULL ||
        nx_spi_dma_transaction_prepare(&request, segments, 1U, 16U, 0U) !=
            NX_SUCCESS ||
        nx_spi_dma_transaction_submit(endpoint, &request) != NX_ERROR_TIMEOUT)
        return 2;
    nx_spi_dma_transaction_service(port);
#else
    const nx_spi_port_t* port = nx_factory_spi_ref(NX_SPI_REF_SPI2);
    const nx_spi_endpoint_t* endpoint =
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_FLASH);
    nx_request_initialize(&request.base);
    if (!nx_spi_endpoint_on_port(endpoint, port) ||
        nx_spi_request_prepare(&request, transmit, receive, sizeof(transmit),
                               0U) != NX_SUCCESS ||
        nx_spi_endpoint_submit(endpoint, &request) != NX_ERROR_TIMEOUT)
        return 2;
    nx_spi_port_service(port);
#endif
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
