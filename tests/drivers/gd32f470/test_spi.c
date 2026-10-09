/* Production SPI4 queue, ownership, drain and fault outcomes. */
#include "board.h"
#include "gd32f470_platform.h"
#include "model.h"
#include <assert.h>
#include <stdio.h>
static nx_status_t release_error;
static nx_status_t provider_test_spi_pins(bool enabled) {
    return !enabled && release_error != NX_OK ? release_error
                                              : nx_gd32_board_spi_pins(enabled);
}
#define nx_gd32_board_spi_pins provider_test_spi_pins
// NOLINTNEXTLINE(bugprone-suspicious-include): deliberate same-TU production
// fault fixture; retain private factory/state checks.
#include "../../../soc/gd32f470/controllers/spi.c"
#undef nx_gd32_board_spi_pins
uint32_t nx_gd32f470_millis(void) {
    return fake_millis;
}
uint64_t nx_gd32f470_timestamp_us(void) {
    return (uint64_t)fake_millis * 1000u;
}
static nx_spi_device_t first;
static nx_lifecycle_t* life;
static unsigned callbacks;
static nx_status_t terminal;
static void complete(void* unused, nx_status_t status) {
    (void)unused;
    callbacks++;
    terminal = status;
    assert(!fake_cs && !fake_spi_shift && !fake_mask);
    assert(life->deinit(life) == NX_ERR_BUSY);
    assert(bus.api.close_device(&bus.api, &first) == NX_ERR_BUSY);
}
static void cancel_during_poll(void) {
    fake_spi_hook = NULL;
    assert(first.cancel(&first) == NX_OK);
}
int main(void) {
    bool typed_descriptor =
        NX_SPI4.device_init == NULL && NX_SPI4.construct == construct_spi;
    assert(typed_descriptor);
    uint8_t rejected_marker = 0;
    void* rejected = &rejected_marker;
    nx_status_t status = construct_spi(NULL, &rejected);
    assert(status == NX_ERR_INVALID_PARAM && rejected == NULL);
    status = construct_spi(&NX_SPI4, NULL);
    assert(status == NX_ERR_NULL_PTR);
    void* constructed = NULL;
    status = construct_spi(&NX_SPI4, &constructed);
    assert(status == NX_OK);
    nx_spi_bus_t* api = constructed;
    life = api->get_lifecycle(api);
    assert(fake_reset_count == 0 && life->init(life) == NX_OK);
    nx_spi_device_config_t config = {
        .cs_pin = 0, .speed = 1000000, .mode = 3, .bit_order = 0};
    assert(api->open_device(api, &config, &first) == NX_OK);
    nx_spi_device_t stale = first;
    uint8_t tx[] = {0x42, 0xAA}, rx[2] = {0};
    nx_spi_transaction_t request = {.tx_data = tx,
                                    .rx_data = rx,
                                    .length = 2,
                                    .timeout_ms = 20,
                                    .callback = complete};
    assert(first.transfer(&first, &request) == NX_OK && callbacks == 1 &&
           terminal == NX_OK && rx[0] == 0xBD && rx[1] == 0x55);
    assert(fake_spi_config.prescale == (6u << 3) &&
           fake_spi_config.clock_polarity_phase == SPI_CK_PL_HIGH_PH_2EDGE);
    request.callback = NULL;
    fake_spi_stall = true;
    fake_clock_step = 1;
    assert(first.transfer(&first, &request) == NX_ERR_TIMEOUT &&
           !fake_spi_shift && !fake_cs);
    fake_spi_stall = false;
    fake_clock_step = 0;
    fake_spi_hold_busy = true;
    fake_clock_step = 1;
    assert(first.transfer(&first, &request) == NX_ERR_TIMEOUT &&
           !fake_spi_shift && !fake_cs);
    fake_spi_hold_busy = false;
    fake_clock_step = 0;
    fake_spi_hook = cancel_during_poll;
    assert(first.transfer(&first, &request) == NX_ERR_CANCELLED &&
           !fake_spi_shift && !fake_cs);
    request.callback = complete;
    request.timeout_ms = 5;
    assert(first.submit(&first, &request) == NX_OK);
    assert(api->close_device(api, &first) == NX_ERR_BUSY &&
           life->suspend(life) == NX_ERR_BUSY);
    fake_millis += 6;
    uint32_t writes = fake_spi_writes;
    assert(api->service(api) == NX_ERR_TIMEOUT && terminal == NX_ERR_TIMEOUT &&
           callbacks == 2 && fake_spi_writes == writes);
    assert(first.submit(&first, &request) == NX_OK &&
           first.cancel(&first) == NX_OK);
    assert(api->service(api) == NX_ERR_CANCELLED && callbacks == 3 &&
           terminal == NX_ERR_CANCELLED);
    assert(api->close_device(api, &first) == NX_OK &&
           api->open_device(api, &config, &first) == NX_OK);
    assert(stale.transfer(&stale, &request) == NX_ERR_INVALID_STATE &&
           stale.cancel(&stale) == NX_ERR_INVALID_STATE);
    nx_spi_device_t devices[3];
    for (unsigned i = 0; i < 3; i++)
        assert(api->open_device(api, &config, &devices[i]) == NX_OK);
    nx_spi_device_t extra;
    assert(api->open_device(api, &config, &extra) == NX_ERR_FULL);
    fake_mask = 1;
    assert(first.transfer(&first, &request) == NX_ERR_INVALID_STATE &&
           fake_mask == 1);
    fake_mask = 0;
    assert(life->deinit(life) == NX_OK && life->init(life) == NX_OK);
    assert(first.transfer(&first, &request) == NX_ERR_INVALID_STATE);
    bus.generation = UINT64_MAX;
    assert(api->open_device(api, &config, &extra) == NX_ERR_FULL);
    release_error = NX_ERR_IO;
    assert(life->deinit(life) == NX_ERR_IO);
    assert(life->get_state(life) == NX_DEV_STATE_RUNNING);
    release_error = NX_OK;
    assert(life->deinit(life) == NX_OK);
    assert(life->get_state(life) == NX_DEV_STATE_UNINITIALIZED);
    puts("GD32 SPI4 mode/divider, timeout/drain, queued deadlines, callback "
         "ownership and stale handles passed");
    return 0;
}
