/**
 * \file            main.c
 * \brief           Exercise two independent F103 SPI transaction bindings.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Touch both generated endpoint paths without writing the bus. */
static bool expired(const nx_spi_transaction_endpoint_t* target) {
    const uint8_t command = 0x5AU;
    const nx_spi_segment_t segment = {.tx = &command, .words = 1U};
    size_t transferred = 7U;
    return nx_spi_transaction_transfer(target, &segment, 1U, 8U, 0U,
                                       &transferred) == NX_ERROR_TIMEOUT &&
           transferred == 7U;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
#if defined(NX_SPI_REF_SPI3)
    const nx_spi_port_t* port = nx_factory_spi_ref(NX_SPI_REF_SPI3);
    const nx_spi_transaction_endpoint_t* target =
        nx_factory_spi_transaction_device_ref(
            NX_SPI_TRANSACTION_DEVICE_REF_SPI3_TARGET);
    const uint16_t command = 0x519AU;
    const nx_spi_segment_t word = {.tx = &command, .words = 1U};
    size_t transferred = 7U;
    const bool valid =
        port != NULL && expired(target) &&
        nx_spi_transaction_transfer(target, &word, 1U, 16U, 0U, &transferred) ==
            NX_ERROR_TIMEOUT &&
        transferred == 7U;
#else
    const nx_spi_port_t* first = nx_factory_spi_ref(NX_SPI_REF_SPI1);
    const nx_spi_port_t* second = nx_factory_spi_ref(NX_SPI_REF_SPI2);
    const nx_spi_transaction_endpoint_t* first_target =
        nx_factory_spi_transaction_device_ref(
            NX_SPI_TRANSACTION_DEVICE_REF_SPI1_TARGET);
    const nx_spi_transaction_endpoint_t* second_target =
        nx_factory_spi_transaction_device_ref(
            NX_SPI_TRANSACTION_DEVICE_REF_SPI2_TARGET);
    const bool valid = first != NULL && second != NULL && first != second &&
                       first_target != second_target && expired(first_target) &&
                       expired(second_target);
#endif
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
