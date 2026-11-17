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

/** \brief Retain only identities present in this assembly. */
#define NX_FIXTURE_RETAIN_IDENTITIES(name, count)                              \
    do {                                                                       \
        s_identity_count = (count);                                            \
        for (uint16_t id = 0U; id < s_identity_count; ++id) {                  \
            s_observed_identity = nx_factory_##name(id);                       \
        }                                                                      \
    } while (0)

int main(void) {
#if defined(NX_FACTORY_HAS_UART)
    NX_FIXTURE_RETAIN_IDENTITIES(uart, NX_UART_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_UART_WORD)
    NX_FIXTURE_RETAIN_IDENTITIES(uart_word, NX_UART_WORD_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_SPI)
    NX_FIXTURE_RETAIN_IDENTITIES(spi, NX_SPI_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_I2C)
    NX_FIXTURE_RETAIN_IDENTITIES(i2c, NX_I2C_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_PWM)
    NX_FIXTURE_RETAIN_IDENTITIES(pwm, NX_PWM_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_ADC)
    NX_FIXTURE_RETAIN_IDENTITIES(adc, NX_ADC_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_WATCHDOG)
    NX_FIXTURE_RETAIN_IDENTITIES(watchdog, NX_WATCHDOG_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_EXTI)
    NX_FIXTURE_RETAIN_IDENTITIES(exti, NX_EXTI_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_ADVANCED_PWM)
    NX_FIXTURE_RETAIN_IDENTITIES(advanced_pwm, NX_ADVANCED_PWM_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_TIMER_CAPTURE)
    NX_FIXTURE_RETAIN_IDENTITIES(timer_capture, NX_TIMER_CAPTURE_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_ENCODER)
    NX_FIXTURE_RETAIN_IDENTITIES(encoder, NX_ENCODER_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_SPI_DEVICE)
    NX_FIXTURE_RETAIN_IDENTITIES(spi_device, NX_SPI_DEVICE_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_SPI_TRANSACTION_DEVICE)
    NX_FIXTURE_RETAIN_IDENTITIES(spi_transaction_device,
                                 NX_SPI_TRANSACTION_DEVICE_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_SPI_DMA_TRANSACTION)
    NX_FIXTURE_RETAIN_IDENTITIES(spi_dma_transaction,
                                 NX_SPI_DMA_TRANSACTION_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_SPI_DMA_TRANSACTION_DEVICE)
    NX_FIXTURE_RETAIN_IDENTITIES(spi_dma_transaction_device,
                                 NX_SPI_DMA_TRANSACTION_DEVICE_ID_COUNT);
#endif
#if defined(NX_FACTORY_HAS_I2C_DEVICE)
    NX_FIXTURE_RETAIN_IDENTITIES(i2c_device, NX_I2C_DEVICE_ID_COUNT);
#endif
    /* These source-reviewed pad fixtures establish software assembly only.
     * No bytes are sent and no PCB wiring or observed device is claimed.
     */
    (void)nx_platform_start();
    (void)nx_platform_stop();
    for (;;) {
    }
}
