/**
 * \file            main.c
 * \brief           Link selected STM32F103 I2C and SPI faces and endpoints
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Verify valid SPI spans expire before CS assertion or data writes. */
static bool check_spi(const nx_spi_port_t* port) {
    size_t transferred = 3U;
    const uint8_t byte = 0x5aU;
#if defined(NX_SPI_TRANSACTION_DEVICE_REF_FIXTURE_SPI)
    const nx_spi_transaction_endpoint_t* endpoint =
        nx_factory_spi_transaction_device_ref(
            NX_SPI_TRANSACTION_DEVICE_REF_FIXTURE_SPI);
    const uint16_t word = 0xa55aU;
    const nx_spi_segment_t byte_segment = {.tx = &byte, .words = 1U};
    const nx_spi_segment_t word_segment = {.tx = &word, .words = 1U};
    return port != NULL && endpoint != NULL &&
           nx_spi_transaction_transfer(endpoint, &byte_segment, 1U, 8U, 0U,
                                       &transferred) == NX_ERROR_TIMEOUT &&
           nx_spi_transaction_transfer(endpoint, &word_segment, 1U, 16U, 0U,
                                       &transferred) == NX_ERROR_TIMEOUT &&
           transferred == 3U;
#elif defined(NX_SPI_DEVICE_REF_FIXTURE_SPI)
    const nx_spi_endpoint_t* endpoint =
        nx_factory_spi_device_ref(NX_SPI_DEVICE_REF_FIXTURE_SPI);
    return nx_spi_endpoint_on_port(endpoint, port) &&
           nx_spi_endpoint_transfer(endpoint, &byte, NULL, 1U, 0U,
                                    &transferred) == NX_ERROR_TIMEOUT &&
           transferred == 3U;
#else
#error "Select an SPI endpoint for this software reference"
#endif
}

/** \brief Exercise typed factory and controller identity without transfers. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_i2c_port_t* i2c = nx_factory_i2c_ref(NX_I2C_REF_I2C1);
    const nx_i2c_endpoint_t* sensor =
        nx_factory_i2c_device_ref(NX_I2C_DEVICE_REF_FIXTURE_I2C);
    const nx_spi_port_t* spi = nx_factory_spi_ref(NX_SPI_REF_SPI2);
    size_t transferred = 0U;
    const bool valid =
        nx_i2c_endpoint_on_port(sensor, i2c) &&
        nx_i2c_endpoint_transaction(sensor, NULL, 0U, 0U, &transferred) ==
            NX_ERROR_INVALID &&
        check_spi(spi);
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
