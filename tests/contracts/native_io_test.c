/**
 * \file            native_io_test.c
 *
 * \brief           Model behavior regressions for all maintained first-mode
 *                  IO.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/model.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                      \
        }                                                                      \
    } while (0)

/** \brief Verify atomic batch authorization and one-port input snapshots. */
static bool gpio_batch(void) {
    CHECK(nx_native_gpio_configure(0x0f, 1) == NX_SUCCESS);
    CHECK(nx_gpio_port_write(nx_native_gpio, 6, 1) == NX_SUCCESS);
    CHECK(nx_native_gpio_output() == 6);
    CHECK(nx_gpio_port_write(nx_native_gpio, 2, 2) == NX_ERROR_INVALID);
    CHECK(nx_native_gpio_output() == 6);
    CHECK(nx_gpio_port_write(nx_native_gpio, 0x10, 0) == NX_ERROR_PERMISSION);
    CHECK(nx_gpio_port_toggle(nx_native_gpio, 0x10) == NX_ERROR_PERMISSION);
    CHECK(nx_native_gpio_output() == 6);
    CHECK(nx_gpio_port_toggle(nx_native_gpio, 5) == NX_SUCCESS);
    CHECK(nx_native_gpio_output() == 3);
    nx_native_gpio_input(0xa5);
    uint32_t input;
    CHECK(nx_gpio_port_read(nx_native_gpio, &input) == NX_SUCCESS);
    CHECK(input == 5);
    CHECK(nx_gpio_port_read(NULL, &input) == NX_ERROR_INVALID);
    return true;
}

/** \brief TX completion is TC-based and late IRQ has no retained payload. */
static bool uart_tx_and_late_irq(void) {
    nx_uart_rx_event_t rx[3];
    uint8_t log[8];
    nx_native_uart_config_t config = {NX_UART_RX_EVENTS, rx, 3, log, 8, false};
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    uint8_t data[] = {1, 2, 3};
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    CHECK(nx_uart_tx_prepare(&request, data, 3, 100) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    nx_uart_tx_request_t rejected;
    nx_request_initialize(&rejected.base);
    CHECK(nx_uart_tx_prepare(&rejected, data, 3, 100) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &rejected) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&rejected.base) == NX_REQUEST_READY);
    for (unsigned i = 0; i < 3; i++) {
        nx_native_uart_irq_step();
    }
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    CHECK(nx_native_uart_transmitted() == 3);
    nx_native_uart_irq_step();
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_SETTLED);
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 3);
    CHECK(memcmp(log, data, 3) == 0);
    memset(data, 0xaa, 3);
    nx_native_uart_irq_step();
    CHECK(nx_native_uart_transmitted() == 3);
    CHECK(log[0] == 1);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    return true;
}

/** \brief Partial-start failure is accepted; quarantine keeps the borrow alive.
 */
static bool uart_fault_cancel_stop(void) {
    uint8_t rx[2];
    nx_native_uart_config_t config = {NX_UART_RX_BYTES, rx, 2, NULL, 0, false};
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    uint8_t data[] = {7, 8};
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 100) == NX_SUCCESS);
    nx_native_uart_fault(true, false);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    nx_uart_port_service(nx_native_uart);
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_IO && transferred == 0);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 100) == NX_SUCCESS);
    nx_native_uart_fault(false, true);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    nx_native_uart_irq_step();
    CHECK(nx_uart_port_cancel(nx_native_uart, &request) == NX_SUCCESS);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_ACTIVE);
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_QUARANTINED);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 100) == NX_ERROR_STATE);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_ERROR_BUSY);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_ERROR_BUSY);
    CHECK(nx_native_uart_configure(&config) == NX_ERROR_BUSY);
    CHECK(nx_native_clock_advance(101) == NX_SUCCESS);
    nx_native_uart_fault(false, false);
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_CANCELLED && transferred == 1);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    return true;
}

/** \brief Deadline observation and admitted-queue handoff do not readmit. */
static bool uart_deadline_and_queue(void) {
    uint8_t rx[2];
    nx_native_uart_config_t config = {NX_UART_RX_BYTES, rx, 2, NULL, 0, true};
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    uint8_t data[] = {7, 8};
    nx_uart_tx_request_t request;
    nx_request_initialize(&request.base);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 10) == NX_SUCCESS);
    CHECK(nx_request_admit(&request.base, NX_REQUEST_QUEUED) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(10) == NX_SUCCESS);
    CHECK(nx_uart_port_start_admitted(nx_native_uart, &request) ==
          NX_ERROR_TIMEOUT);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_QUEUED);
    nx_request_settle(&request.base, NX_ERROR_TIMEOUT, 0);
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && transferred == 0);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 10) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_ERROR_TIMEOUT);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_READY);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 11) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(1) == NX_SUCCESS);
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && transferred == 0);
    CHECK(nx_uart_tx_prepare(&request, data, 2, 100) == NX_SUCCESS);
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_ERROR_CONTEXT);
    nx_arch_irq_restore(mask);
    CHECK(nx_request_state(&request.base) == NX_REQUEST_READY);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    for (unsigned i = 0; i < 3; i++) {
        nx_uart_port_service(nx_native_uart);
    }
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 2);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    config.automatic_irq = false;
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 2);
    CHECK(nx_uart_tx_prepare(&request, data, 2, deadline) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    CHECK(nx_native_clock_advance(3) == NX_SUCCESS);
    CHECK(nx_uart_port_cancel(nx_native_uart, &request) == NX_SUCCESS);
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_SUCCESS && transferred == 2);
    deadline = nx_deadline_after(nx_time_now_us(), 1);
    CHECK(nx_uart_tx_prepare(&request, data, 2, deadline) == NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &request) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    CHECK(nx_native_clock_advance(1) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_result(&request.base, &result, &transferred) ==
          NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && transferred == 2);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    return true;
}

/** \brief Non-power-of-two capacity preserves FIFO and reports actual loss. */
static bool uart_rx_profiles(void) {
    nx_uart_rx_event_t rx[3];
    nx_native_uart_config_t config = {NX_UART_RX_EVENTS, rx, 3, NULL, 0, false};
    nx_native_uart_config_t invalid = config;
    invalid.rx_capacity = SIZE_MAX;
    CHECK(nx_native_uart_configure(&invalid) == NX_ERROR_INVALID);
    _Alignas(nx_uart_rx_event_t)
        uint8_t misaligned[sizeof(nx_uart_rx_event_t) + 1];
    invalid = config;
    invalid.rx_storage = misaligned + 1;
    CHECK(nx_native_uart_configure(&invalid) == NX_ERROR_INVALID);
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(1, 0) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(5) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(2, NX_UART_EVENT_PARITY) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(3, 0) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(4, 0) == NX_ERROR_OVERFLOW);
    nx_uart_rx_event_t events[3];
    size_t count;
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 2, &count) ==
          NX_SUCCESS);
    CHECK(count == 2 && events[0].byte == 1 && events[1].byte == 2);
    CHECK(events[0].flags == 0);
    CHECK(events[1].flags == NX_UART_EVENT_PARITY);
    CHECK(events[1].timestamp_us - events[0].timestamp_us == 5);
    CHECK(nx_native_uart_receive(5, 0) == NX_ERROR_OVERFLOW);
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 3, &count) ==
          NX_SUCCESS);
    CHECK(count == 2 && events[0].byte == 3);
    CHECK(events[1].flags == (NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE));
    CHECK(events[1].timestamp_us == 5);
    CHECK(nx_native_uart_receive(5, 0) == NX_SUCCESS);
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 3, &count) ==
          NX_SUCCESS);
    CHECK(count == 1 && events[0].byte == 5 && events[0].flags == 0);
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 3, &count) ==
          NX_ERROR_EMPTY);
    CHECK(nx_native_uart_receive(99, NX_UART_EVENT_OVERRUN |
                                         NX_UART_EVENT_NO_BYTE) == NX_SUCCESS);
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 3, &count) ==
          NX_SUCCESS);
    CHECK(count == 1 && events[0].byte == 0);
    CHECK(events[0].flags == (NX_UART_EVENT_OVERRUN | NX_UART_EVENT_NO_BYTE));
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    uint8_t bytes[3];
    config =
        (nx_native_uart_config_t){NX_UART_RX_BYTES, bytes, 3, NULL, 0, false};
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(10, 0) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(11, NX_UART_EVENT_FRAMING) == NX_SUCCESS);
    CHECK(nx_native_uart_receive(99, NX_UART_EVENT_OVERRUN |
                                         NX_UART_EVENT_NO_BYTE) == NX_SUCCESS);
    uint8_t output[3];
    CHECK(nx_uart_port_read_bytes(nx_native_uart, output, 3, &count) ==
          NX_ERROR_OVERFLOW);
    CHECK(count == 2 && output[0] == 10 && output[1] == 11);
    CHECK(nx_uart_port_read_events(nx_native_uart, events, 3, &count) ==
          NX_ERROR_UNSUPPORTED);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    return true;
}

/** \brief Complete CS transactions return every buffer even after partial
 * failure. */
static bool spi_transaction(void) {
    CHECK(nx_native_spi_configure(NULL, 0) == NX_SUCCESS);
    uint8_t tx[] = {1, 2, 3, 4};
    uint8_t rx[4] = {0};
    size_t transferred;
    CHECK(nx_spi_endpoint_transfer(nx_native_spi, tx, rx, 4, 100,
                                   &transferred) == NX_SUCCESS);
    CHECK(transferred == 4 && memcmp(tx, rx, 4) == 0);
    CHECK(!nx_native_spi_cs_active());
    nx_native_spi_fault(2, false);
    CHECK(nx_spi_endpoint_transfer(nx_native_spi, tx, rx, 4, 100,
                                   &transferred) == NX_ERROR_IO);
    CHECK(transferred == 2 && !nx_native_spi_cs_active());
    nx_native_spi_fault(SIZE_MAX, true);
    CHECK(nx_spi_endpoint_transfer(nx_native_spi, tx, rx, 4, 100,
                                   &transferred) == NX_ERROR_BUSY);
    CHECK(transferred == 0 && !nx_native_spi_cs_active());
    nx_native_spi_fault(SIZE_MAX, false);
    nx_time_us_t deadline = nx_time_now_us() + 2;
    CHECK(nx_spi_endpoint_transfer(nx_native_spi, tx, rx, 4, deadline,
                                   &transferred) == NX_ERROR_TIMEOUT);
    CHECK(transferred == 2 && !nx_native_spi_cs_active());
    CHECK(nx_spi_endpoint_transfer(nx_native_spi, tx, rx, 4, 100,
                                   &transferred) == NX_SUCCESS);
    return true;
}

/** \brief Repeated START read follows register address without extra STOP
 * effects. */
static bool i2c_transaction_and_recovery(void) {
    uint8_t memory[32] = {0};
    memory[10] = 42;
    memory[11] = 43;
    CHECK(nx_native_i2c_configure(memory, 32, 0x76) == NX_SUCCESS);
    uint8_t address = 10;
    uint8_t output[2] = {0};
    nx_i2c_message_t messages[] = {{&address, 1, false}, {output, 2, true}};
    size_t transferred;
    CHECK(nx_i2c_endpoint_transaction(nx_native_i2c, messages, 2, 100,
                                      &transferred) == NX_SUCCESS);
    CHECK(transferred == 3 && output[0] == 42 && output[1] == 43);
    nx_native_i2c_fault(NX_ERROR_NACK, false);
    CHECK(nx_i2c_endpoint_transaction(nx_native_i2c, messages, 2, 100,
                                      &transferred) == NX_ERROR_NACK);
    CHECK(transferred == 0);
    CHECK(nx_i2c_port_recover(nx_native_i2c_port) == NX_SUCCESS);
    nx_native_i2c_fault(NX_ERROR_IO, true);
    CHECK(nx_i2c_port_recover(nx_native_i2c_port) == NX_ERROR_IO);
    nx_native_i2c_fault(NX_SUCCESS, false);
    CHECK(nx_i2c_port_recover(nx_native_i2c_port) == NX_SUCCESS);
    CHECK(nx_i2c_endpoint_transaction(nx_native_i2c, messages, 2,
                                      nx_time_now_us(),
                                      &transferred) == NX_ERROR_TIMEOUT);
    CHECK(transferred == 0);
    return true;
}

/** \brief Physical bounds, NOR alignment, partial pulses and explicit regions.
 */
static bool flash_regions_and_pulses(void) {
    uint8_t memory[64];
    const nx_flash_sector_t sectors[] = {{0, 16}, {16, 16}, {32, 32}};
    const nx_flash_geometry_t geometry = {0x08000000, 64, 4, sectors, 3};
    CHECK(nx_native_flash_configure(memory, &geometry) == NX_SUCCESS);
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(nx_flash_port_program(nx_native_flash, UINT32_MAX - 1, data, 8,
                                100) == NX_ERROR_INVALID);
    CHECK(nx_flash_port_program(nx_native_flash, 1, data, 8, 100) ==
          NX_ERROR_INVALID);
    CHECK(nx_flash_port_program(nx_native_flash, 0, data, 8, 100) ==
          NX_SUCCESS);
    CHECK(memcmp(memory, data, 8) == 0);
    uint8_t all_ones[4] = {255, 255, 255, 255};
    CHECK(nx_flash_port_program(nx_native_flash, 0, all_ones, 4, 100) ==
          NX_ERROR_IO);
    nx_flash_region_t region = {nx_native_flash, 16, 16, false};
    CHECK(nx_flash_region_program(&region, 0, data, 8, 100) ==
          NX_ERROR_PERMISSION);
    region.writable = true;
    CHECK(nx_flash_region_program(&region, 12, data, 8, 100) ==
          NX_ERROR_INVALID);
    nx_native_flash_fault(1);
    CHECK(nx_flash_region_program(&region, 0, data, 8, 100) == NX_ERROR_IO);
    CHECK(memcmp(&memory[16], data, 4) == 0 && memory[20] == 255);
    nx_native_flash_fault(SIZE_MAX);
    CHECK(nx_flash_region_erase(&region, 1, 15, 1000) == NX_ERROR_INVALID);
    CHECK(nx_flash_region_erase(&region, 0, 16, 1000) == NX_SUCCESS);
    for (size_t i = 16; i < 32; i++) {
        CHECK(memory[i] == 255);
    }
    region.offset = UINT32_MAX;
    CHECK(nx_flash_region_validate(&region) == NX_ERROR_INVALID);
    CHECK(nx_flash_port_erase(nx_native_flash, 0, 16, nx_time_now_us()) ==
          NX_ERROR_TIMEOUT);
    return true;
}

/** \brief Existing IWDG activation is a retained effect and timeout latches
 * cause. */
static bool watchdog_effect(void) {
    nx_native_watchdog_boot(NX_RESET_POWER_ON);
    nx_watchdog_state_t state;
    CHECK(nx_watchdog_port_feed(nx_native_watchdog) == NX_ERROR_STATE);
    CHECK(nx_watchdog_port_enable(nx_native_watchdog, 999, false, &state) ==
          NX_ERROR_INVALID);
    CHECK(!state.enabled);
    CHECK(nx_watchdog_port_enable(nx_native_watchdog, 1000, true, &state) ==
          NX_SUCCESS);
    CHECK(state.enabled && state.irreversible && state.debug_freeze);
    CHECK(state.minimum_timeout_us == 800 && state.maximum_timeout_us == 1200);
    CHECK(nx_watchdog_port_enable(nx_native_watchdog, 2000, false, &state) ==
          NX_ERROR_STATE);
    CHECK(state.enabled && state.debug_freeze);
    CHECK(nx_native_clock_advance(500) == NX_SUCCESS);
    CHECK(nx_watchdog_port_feed(nx_native_watchdog) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(999) == NX_SUCCESS);
    CHECK(!nx_native_watchdog_expired());
    CHECK(nx_native_clock_advance(1) == NX_SUCCESS);
    CHECK(nx_native_watchdog_expired());
    CHECK((nx_reset_cause() & NX_RESET_IWDG) != 0);
    CHECK(nx_watchdog_port_feed(nx_native_watchdog) == NX_ERROR_IO);
    return true;
}

/** \brief Fixed EXTI FIFO loss and shutdown reject future producer access. */
static bool exti_events(void) {
    nx_exti_event_t storage[3];
    CHECK(nx_native_exti_configure(5, NX_EXTI_BOTH, storage, 3) == NX_SUCCESS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_RISING) == NX_SUCCESS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_FALLING) == NX_SUCCESS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_RISING) == NX_SUCCESS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_FALLING) == NX_ERROR_OVERFLOW);
    nx_exti_event_t output[3];
    size_t count;
    CHECK(nx_exti_port_read(nx_native_exti, output, 2, &count) == NX_SUCCESS);
    CHECK(count == 2 && output[0].edge == NX_EXTI_RISING &&
          output[1].edge == NX_EXTI_FALLING);
    CHECK(output[0].flags == 0 && output[1].flags == 0);
    CHECK(nx_native_exti_emit(5, NX_EXTI_FALLING) == NX_ERROR_OVERFLOW);
    CHECK(nx_exti_port_read(nx_native_exti, output, 3, &count) == NX_SUCCESS);
    CHECK(count == 2 && output[0].edge == NX_EXTI_RISING &&
          output[1].flags == NX_EXTI_EVENT_LOSS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_FALLING) == NX_SUCCESS);
    CHECK(nx_exti_port_read(nx_native_exti, output, 3, &count) == NX_SUCCESS);
    CHECK(count == 1 && output[0].edge == NX_EXTI_FALLING);
    CHECK(nx_exti_port_stop(nx_native_exti) == NX_SUCCESS);
    CHECK(nx_native_exti_emit(5, NX_EXTI_RISING) == NX_ERROR_STATE);
    CHECK(nx_exti_port_read(nx_native_exti, output, 3, &count) ==
          NX_ERROR_EMPTY);
    return true;
}

/** \brief PWM zero/full-duty boundaries do not allow shared-base mutation. */
static bool pwm_boundaries(void) {
    CHECK(nx_native_pwm_configure(1000000, 1000) == NX_SUCCESS);
    CHECK(nx_pwm_port_set(nx_native_pwm, 1000, 0) == NX_SUCCESS);
    CHECK(nx_pwm_port_start(nx_native_pwm) == NX_SUCCESS);
    CHECK(nx_pwm_port_set(nx_native_pwm, 1000, 1000) == NX_SUCCESS);
    CHECK(nx_pwm_port_set(nx_native_pwm, 1000, 1001) == NX_ERROR_INVALID);
    CHECK(nx_pwm_port_set(nx_native_pwm, 500, 250) == NX_ERROR_BUSY);
    nx_pwm_state_t state;
    CHECK(nx_pwm_port_state(nx_native_pwm, &state) == NX_SUCCESS);
    CHECK(state.period_ticks == 1000 && state.duty_ticks == 1000 &&
          state.running);
    CHECK(nx_pwm_port_stop(nx_native_pwm) == NX_SUCCESS);
    CHECK(nx_pwm_port_state(nx_native_pwm, &state) == NX_SUCCESS);
    CHECK(!state.running && state.tick_hz == 1000000);
    return true;
}

/** \brief ADC only exposes complete valid prefix samples on faults/deadlines.
 */
static bool adc_prefix_and_deadline(void) {
    const uint16_t fixture[] = {100, 200, 300};
    CHECK(nx_native_adc_configure(fixture, 3, 12, 3300) == NX_SUCCESS);
    nx_adc_info_t info;
    CHECK(nx_adc_port_info(nx_native_adc, &info) == NX_SUCCESS);
    CHECK(info.channel_count == 3 && info.resolution_bits == 12 &&
          info.reference_mv == 3300);
    uint16_t samples[] = {999, 999, 999};
    size_t count;
    CHECK(nx_adc_port_sample(nx_native_adc, samples, 2, 100, &count) ==
          NX_ERROR_INVALID);
    nx_native_adc_fault(1);
    CHECK(nx_adc_port_sample(nx_native_adc, samples, 3, 100, &count) ==
          NX_ERROR_IO);
    CHECK(count == 1 && samples[0] == 100 && samples[1] == 999);
    nx_native_adc_fault(SIZE_MAX);
    CHECK(nx_adc_port_sample(nx_native_adc, samples, 3, nx_time_now_us() + 1,
                             &count) == NX_ERROR_TIMEOUT);
    CHECK(count == 1 && samples[1] == 999);
    CHECK(nx_adc_port_sample(nx_native_adc, samples, 3, 100, &count) ==
          NX_SUCCESS);
    CHECK(count == 3 && samples[2] == 300);
    return true;
}

/** \brief Run all first-mode IO behavior cases without hardware qualification.
 */
int main(void) {
    const struct {
        const char* name;
        bool (*run)(void);
    } cases[] = {{"GPIO batch", gpio_batch},
                 {"UART TX and late IRQ", uart_tx_and_late_irq},
                 {"UART fault cancel stop", uart_fault_cancel_stop},
                 {"UART deadline and queue", uart_deadline_and_queue},
                 {"UART RX profiles", uart_rx_profiles},
                 {"SPI transaction", spi_transaction},
                 {"I2C transaction and recovery", i2c_transaction_and_recovery},
                 {"Flash regions and pulses", flash_regions_and_pulses},
                 {"Watchdog irreversible effect", watchdog_effect},
                 {"EXTI events", exti_events},
                 {"PWM boundaries", pwm_boundaries},
                 {"ADC prefix and deadline", adc_prefix_and_deadline}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (nx_native_clock_configure(true, 0) != NX_SUCCESS ||
            !cases[i].run()) {
            fprintf(stderr, "FAIL %s\n", cases[i].name);
            return 1;
        }
        printf("PASS %s\n", cases[i].name);
    }
    return 0;
}
