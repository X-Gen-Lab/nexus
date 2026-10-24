/**
 * \file            controller_main.c
 * \brief           Development-only typed enhanced I/O link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Volatile face reads retain every selected operations table.
 *                  Methods are linked but never called by this fixture. This
 *                  is software evidence, not a HIL image or Board wiring proof.
 */
#if defined(NX_FIXTURE_UART_WORD_IRQ_PROBE)
/* Ordinary Platform usage must propagate the same selected probe definition
 * even when a consumer includes its generic I/O header before the factory. */
#include "nexus/io/cycle_probe.h"
#endif
#include "nexus/io/adc.h"
#include "nexus/io/encoder.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"
#include "nexus/io/pwm_group.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/timer_capture.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#if defined(NX_FIXTURE_ADC_GROUP)
#include "nexus/io/adc_group.h"
#endif
#if defined(NX_FIXTURE_ETH_FRAMES)
#include "nexus/io/ethernet_frames.h"
#include "nexus/io/mdio.h"
#endif
#if defined(NX_FIXTURE_PWM_ADC)
#include "nexus/io/pwm_adc.h"
#endif
#if defined(NX_FIXTURE_CAN_FD)
#include "nexus/io/can_fd.h"
#endif
#if defined(NX_FIXTURE_UART_WORD)
#include "nexus/io/uart_word.h"
#endif
#if defined(NX_FIXTURE_SPI_TRANSACTION)
#include "nexus/io/spi_transaction.h"
#endif
#if defined(NX_FIXTURE_SPI_DMA_TRANSACTION)
#include "nexus/io/spi_dma_owner_binding.h"
#include "nexus/io/spi_dma_transaction.h"
#endif
#if defined(NX_FIXTURE_I2C_LONG)
#include "nexus/io/i2c_long.h"
#endif
#if defined(NX_FIXTURE_UART_STREAM_PROFILE) ||                                 \
    defined(NX_FIXTURE_ADC_STREAM_PROFILE)
#include "nexus/io/stream_profile.h"
#endif

/* Volatile-qualified face fields force real ROM reads. Their relocations keep
 * concrete ops and all non-NULL methods alive through section GC, independently
 * of controller/device names. Empty generated classes add no runtime work.
 */
#define NX_FIXTURE_RETAIN_FACES(name, type, count)                             \
    do {                                                                       \
        size_t instance_count = (count);                                       \
        for (size_t index = 0; index < instance_count; ++index) {              \
            const volatile type* face =                                        \
                nx_factory_##name((nx_##name##_id_t)index);                    \
            if (face != NULL) {                                                \
                (void)face->ops;                                               \
                (void)face->context;                                           \
            }                                                                  \
        }                                                                      \
    } while (0)

/** \brief Initialize assembly and retain every typed controller and endpoint.
 */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return 1;
    }
    NX_FIXTURE_RETAIN_FACES(gpio, nx_gpio_port_t, NX_GPIO_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(uart, nx_uart_port_t, NX_UART_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(spi, nx_spi_port_t, NX_SPI_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(i2c, nx_i2c_port_t, NX_I2C_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(flash, nx_flash_port_t, NX_FLASH_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(watchdog, nx_watchdog_port_t, NX_WATCHDOG_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(exti, nx_exti_port_t, NX_EXTI_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(pwm, nx_pwm_port_t, NX_PWM_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(adc, nx_adc_port_t, NX_ADC_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(pwm_group, nx_pwm_group_t, NX_PWM_GROUP_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(timer_capture, nx_timer_capture_t,
                            NX_TIMER_CAPTURE_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(encoder, nx_encoder_t, NX_ENCODER_ID_COUNT);
#if defined(NX_FIXTURE_ADC_GROUP)
    NX_FIXTURE_RETAIN_FACES(adc_group, nx_adc_group_port_t,
                            NX_ADC_GROUP_ID_COUNT);
#endif
#if defined(NX_FIXTURE_ETH_FRAMES)
    NX_FIXTURE_RETAIN_FACES(eth_frames, nx_eth_frames_port_t,
                            NX_ETH_FRAMES_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(mdio, nx_mdio_port_t, NX_MDIO_ID_COUNT);
#endif
#if defined(NX_FIXTURE_PWM_ADC)
    NX_FIXTURE_RETAIN_FACES(pwm_adc, nx_pwm_adc_t, NX_PWM_ADC_ID_COUNT);
#endif
#if defined(NX_FIXTURE_CAN_FD)
    NX_FIXTURE_RETAIN_FACES(can_fd, nx_can_fd_port_t, NX_CAN_FD_ID_COUNT);
#endif
    NX_FIXTURE_RETAIN_FACES(spi_device, nx_spi_endpoint_t,
                            NX_SPI_DEVICE_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(i2c_device, nx_i2c_endpoint_t,
                            NX_I2C_DEVICE_ID_COUNT);
#if defined(NX_FIXTURE_UART_WORD)
    NX_FIXTURE_RETAIN_FACES(uart_word, nx_uart_word_port_t,
                            NX_UART_WORD_ID_COUNT);
#endif
#if defined(NX_FIXTURE_SPI_TRANSACTION)
    NX_FIXTURE_RETAIN_FACES(spi_transaction_device,
                            nx_spi_transaction_endpoint_t,
                            NX_SPI_TRANSACTION_DEVICE_ID_COUNT);
#endif
#if defined(NX_FIXTURE_SPI_DMA_TRANSACTION)
    NX_FIXTURE_RETAIN_FACES(spi_dma_transaction, nx_spi_dma_transaction_port_t,
                            NX_SPI_DMA_TRANSACTION_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(spi_dma_transaction_device,
                            nx_spi_dma_transaction_endpoint_t,
                            NX_SPI_DMA_TRANSACTION_DEVICE_ID_COUNT);
    for (size_t index = 0; index < NX_SPI_DMA_TRANSACTION_ID_COUNT; ++index) {
        const volatile nx_spi_dma_owner_binding_t* owner =
            nx_factory_spi_dma_owner_binding(
                (nx_spi_dma_transaction_id_t)index);
        if (owner != NULL) {
            (void)owner->port;
            (void)owner->contains;
            (void)owner->attach_wake;
        }
    }
#endif
#if defined(NX_FIXTURE_UART_WORD_IRQ_PROBE)
    for (size_t index = 0; index < NX_UART_WORD_ID_COUNT; ++index) {
        const volatile nx_io_cycle_probe_t* probe =
            nx_factory_uart_word_irq_probe((nx_uart_word_id_t)index);
        if (probe != NULL) {
            (void)probe->observation.maximum_modulo_cycles;
        }
    }
    nx_result_t (*volatile snapshot)(const nx_io_cycle_probe_t*,
                                     nx_io_cycle_observation_t*) =
        nx_io_cycle_probe_snapshot;
    (void)snapshot;
#endif
#if defined(NX_FIXTURE_I2C_LONG)
    NX_FIXTURE_RETAIN_FACES(i2c_long_device, nx_i2c_long_endpoint_t,
                            NX_I2C_LONG_DEVICE_ID_COUNT);
#endif
#if defined(NX_FIXTURE_UART_STREAM_PROFILE)
    for (size_t index = 0; index < NX_UART_ID_COUNT; ++index) {
        const volatile nx_uart_stream_profile_t* profile =
            nx_factory_uart_stream_profile((nx_uart_id_t)index);
        if (profile != NULL) {
            (void)profile->port;
            (void)profile->budget.configured_blocks;
        }
    }
    nx_result_t (*volatile uart_start)(const nx_uart_stream_profile_t*,
                                       nx_stream_t*) =
        nx_uart_stream_profile_start;
    (void)uart_start;
#endif
#if defined(NX_FIXTURE_ADC_STREAM_PROFILE)
    for (size_t index = 0; index < NX_ADC_ID_COUNT; ++index) {
        const volatile nx_adc_stream_profile_t* profile =
            nx_factory_adc_stream_profile((nx_adc_id_t)index);
        if (profile != NULL) {
            (void)profile->port;
            (void)profile->budget.configured_blocks;
        }
    }
    nx_result_t (*volatile adc_start)(const nx_adc_stream_profile_t*,
                                      nx_stream_t*, uint32_t) =
        nx_adc_stream_profile_start;
    (void)adc_start;
#endif
    for (;;) {
    }
}
