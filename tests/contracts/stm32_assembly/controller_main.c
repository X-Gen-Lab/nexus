/**
 * \file            controller_main.c
 * \brief           Development-only actual typed STM32 controller link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Volatile face reads retain every selected operations table.
 *                  Methods are linked but never called by this fixture. This
 *                  is software evidence, not a HIL image or Board wiring proof.
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

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
    NX_FIXTURE_RETAIN_FACES(spi_device, nx_spi_endpoint_t,
                            NX_SPI_DEVICE_ID_COUNT);
    NX_FIXTURE_RETAIN_FACES(i2c_device, nx_i2c_endpoint_t,
                            NX_I2C_DEVICE_ID_COUNT);
    for (;;) {
    }
}
