/**
 * \file            h743-high.c
 * \brief           Public installed-SDK high-clock link verification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"

static nx_uart_tx_request_t s_request;
static uint8_t s_transmit[2] = {0x12U, 0x34U};
static uint8_t s_receive[2];
static uint16_t s_samples[1];
static nx_i2c_message_t s_message = {s_receive, sizeof(s_receive), true};

/** \brief Keep every static loan alive if final source withdrawal fails. */
static void keep_storage(void) {
    for (;;) {
    }
}

/** \brief Close outer leases without pretending a failed drain succeeded. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS) {
        keep_storage();
    }
    return result;
}

/** \brief Link actual public methods; ARM verification does not run this. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return close_platform(1);
    }
    const nx_uart_port_t* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    const nx_flash_port_t* flash = nx_factory_flash(NX_FLASH_ID_STORAGE);
    if (uart == NULL || flash == NULL ||
        nx_factory_spi(NX_SPI_ID_BUS4) == NULL ||
        nx_factory_i2c(NX_I2C_ID_BUS1) == NULL ||
        nx_factory_pwm(NX_PWM_ID_WAVE3) == NULL) {
        return close_platform(2);
    }
    uint32_t gpio = 0U;
    size_t count = 0U;
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 10000U);
    if (nx_gpio_port_read(nx_factory_gpio(NX_GPIO_ID_LED), &gpio) !=
            NX_SUCCESS ||
        nx_spi_endpoint_transfer(nx_device_device4, s_transmit, s_receive,
                                 sizeof(s_transmit), deadline,
                                 &count) != NX_SUCCESS ||
        nx_i2c_endpoint_transaction(nx_device_device1, &s_message, 1U, deadline,
                                    &count) != NX_SUCCESS ||
        nx_adc_port_sample(nx_factory_adc(NX_ADC_ID_SENSOR), s_samples, 1U,
                           deadline, &count) != NX_SUCCESS ||
        nx_flash_port_geometry(flash) == NULL ||
        nx_flash_port_read(flash, 0U, s_receive, sizeof(s_receive)) !=
            NX_SUCCESS) {
        return close_platform(3);
    }
    nx_request_initialize(&s_request.base);
    if (nx_uart_tx_prepare(&s_request, s_transmit, sizeof(s_transmit),
                           nx_deadline_after(nx_time_now_us(), 10000U)) !=
            NX_SUCCESS ||
        nx_uart_port_submit(uart, &s_request) != NX_SUCCESS) {
        return close_platform(4);
    }
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_uart_port_cancel(uart, &s_request);
    }
    for (unsigned i = 0U;
         i != 64U && nx_request_state(&s_request.base) != NX_REQUEST_SETTLED;
         ++i) {
        nx_uart_port_service(uart);
    }
    (void)nx_uart_port_stop(uart);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
            keep_storage();
        }
    }
    return close_platform(0);
}
