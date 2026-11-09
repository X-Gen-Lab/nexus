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
#ifdef NX_FIXTURE_UART_WORD
    s_identity_count = NX_UART_WORD_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_uart_word(id);
    }
#endif
    s_identity_count = NX_SPI_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi(id);
    }
    s_identity_count = NX_I2C_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_i2c(id);
    }
    s_identity_count = NX_PWM_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_pwm(id);
    }
    s_identity_count = NX_ADC_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_adc(id);
    }
    s_identity_count = NX_WATCHDOG_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_watchdog(id);
    }
#ifdef NX_FIXTURE_ENCODER
    s_identity_count = NX_ENCODER_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_encoder(id);
    }
#endif
    s_identity_count = NX_SPI_DEVICE_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi_device(id);
    }
#ifdef NX_FIXTURE_SPI_TRANSACTION
    s_identity_count = NX_SPI_TRANSACTION_DEVICE_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi_transaction_device(id);
    }
#endif
#ifdef NX_FIXTURE_SPI_DMA_TRANSACTION
    s_identity_count = NX_SPI_DMA_TRANSACTION_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi_dma_transaction(id);
    }
    s_identity_count = NX_SPI_DMA_TRANSACTION_DEVICE_ID_COUNT;
    for (uint16_t id = 0U; id < s_identity_count; ++id) {
        s_observed_identity = nx_factory_spi_dma_transaction_device(id);
    }
#endif
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
