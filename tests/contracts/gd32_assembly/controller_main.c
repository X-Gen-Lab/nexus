/**
 * \file            controller_main.c
 * \brief           Development-only actual typed GD32 controller link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Volatile typed function references retain actual providers.
 *                  They do not execute Flash pulses, enable FWDGT, or assert
 *                  external electrical wiring. This is not a HIL image.
 */
#include "nexus_bindings.h"
#include "nexus_config.h"

/** \brief Initialize selected construction and retain the real typed functions.
 */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_SPI0_SELECTED)
    nx_result_t (*volatile transfer)(const nx_spi_endpoint_t*, const uint8_t*,
                                     uint8_t*, size_t, nx_time_us_t, size_t*) =
        nx_spi_endpoint_transfer;
    (void)transfer;
#elif defined(NEXUS_I2C0_SELECTED)
    nx_result_t (*volatile transaction)(const nx_i2c_endpoint_t*,
                                        nx_i2c_message_t*, size_t, nx_time_us_t,
                                        size_t*) = nx_i2c_endpoint_transaction;
    nx_result_t (*volatile recover)(nx_i2c_port_t*) = nx_i2c_port_recover;
    (void)transaction;
    (void)recover;
#elif defined(NEXUS_FLASH0_SELECTED)
    nx_result_t (*volatile program)(nx_flash_port_t*, uint32_t, const void*,
                                    size_t, nx_time_us_t) =
        nx_flash_port_program;
    nx_result_t (*volatile erase)(nx_flash_port_t*, uint32_t, size_t,
                                  nx_time_us_t) = nx_flash_port_erase;
    (void)program;
    (void)erase;
#elif defined(NEXUS_WATCHDOG0_SELECTED)
    nx_result_t (*volatile enable)(nx_watchdog_port_t*, uint32_t, bool,
                                   nx_watchdog_state_t*) =
        nx_watchdog_port_enable;
    nx_result_t (*volatile feed)(nx_watchdog_port_t*) = nx_watchdog_port_feed;
    (void)enable;
    (void)feed;
#elif defined(NEXUS_EDGE3_SELECTED)
    nx_result_t (*volatile read)(nx_exti_port_t*, nx_exti_event_t*, size_t,
                                 size_t*) = nx_exti_port_read;
    (void)read;
#elif defined(NEXUS_PWM0_SELECTED)
    nx_result_t (*volatile set)(nx_pwm_port_t*, uint32_t, uint32_t) =
        nx_pwm_port_set;
    nx_result_t (*volatile start)(nx_pwm_port_t*) = nx_pwm_port_start;
    (void)set;
    (void)start;
#elif defined(NEXUS_ADC0_SELECTED)
    nx_result_t (*volatile sample)(nx_adc_port_t*, uint16_t*, size_t,
                                   nx_time_us_t, size_t*) = nx_adc_port_sample;
    (void)sample;
#endif
    for (;;) {
    }
}
