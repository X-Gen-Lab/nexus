/**
 * \file            controller_main.c
 * \brief           Link-only selected controller and endpoint identities
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static const void* volatile s_observed_identity;
static volatile uint16_t s_identity_count;

int main(void) {
    s_identity_count = NX_UART_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_uart(id);
    }
    s_identity_count = NX_SPI_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi(id);
    }
    s_identity_count = NX_I2C_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_i2c(id);
    }
    s_identity_count = NX_SPI_DEVICE_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi_device(id);
    }
    s_identity_count = NX_I2C_DEVICE_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_i2c_device(id);
    }
    /* These source-reviewed pad fixtures establish software assembly only.
     * No bytes are sent and no PCB wiring or observed device is claimed.
     */
    (void)nx_platform_start();
    (void)nx_platform_stop();
    for (;;) {
    }
}
