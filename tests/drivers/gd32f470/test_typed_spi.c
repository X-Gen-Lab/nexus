/* Vertical host integration: production platform boot/timebase, GD SPI4 and
 * typed device core. Crystal/PLL, NVIC, SysTick, pins and controller registers
 * are explicit host models. This is ownership/fault evidence, not board HIL. */
#include "hal/provider/nx_device_provider.h"
#include "model.h"
#include "hal/base/nx_device.h"
#include "hal/nx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../soc/gd32f470/interrupt.c"
#include "../../../platforms/gd32f470/src/platform.c"
#include "../../../soc/gd32f470/controllers/spi.c"

uint32_t SystemCoreClock = 200000000u;
static int clock_result;
static unsigned clock_checks, grouping_calls, systick_calls;
int nx_gd32f470_clock_validate(void) {
    ++clock_checks;
    /* Production boot must establish safe outputs before the clock change. */
    assert(fake_board_safe_inits >= clock_checks && !fake_cs && !fake_de);
    return clock_result;
}
bool osal_is_initialized(void) {
    return false;
}
int nx_gd32f470_clock_release(void) {
    return 0;
}
void NVIC_SetPriorityGrouping(uint32_t group) {
    assert(group == 3u && clock_result == 0);
    ++grouping_calls;
}
uint32_t SysTick_Config(uint32_t ticks) {
    assert(ticks == 200000u && fake_timer_config.prescaler == 99u);
    ++systick_calls;
    return 0u;
}

typedef struct {
    nx_device_ref_t controller;
    nx_device_spi_ref_t child;
    nx_device_spi_ticket_t ticket;
    nx_status_t expected;
    unsigned calls;
} completion_t;
static void terminal(void* opaque, nx_status_t status) {
    completion_t* completion = opaque;
    assert(status == completion->expected);
    ++completion->calls;
    /* Production GD port drained/reset hardware before the typed callback. */
    assert(!fake_mask && !fake_cs && !fake_spi_shift);
    assert(nx_device_close(completion->controller) == NX_ERR_BUSY);
    assert(nx_device_spi_close(completion->child) == NX_ERR_BUSY);
    assert(nx_device_spi_cancel(completion->child, completion->ticket) == NX_ERR_BUSY);
    nx_device_spi_result_t result;
    assert(nx_device_spi_poll(completion->child, completion->ticket, &result) == NX_OK);
    /* The caller callback itself is still part of the buffer lease. */
    assert(!result.settled && result.status == NX_ERR_BUSY);
}
static void assert_result(nx_device_spi_ref_t child, nx_device_spi_ticket_t ticket,
                          nx_status_t expected, bool settled) {
    nx_device_spi_result_t result;
    assert(nx_device_spi_poll(child, ticket, &result) == NX_OK);
    assert(result.status == expected && result.settled == settled);
}
static nx_device_ref_t active_controller;
static nx_device_spi_ref_t active_child;
static unsigned cancellation_requests;
static void cancel_blocking(void) {
    fake_spi_hook = NULL;
    assert(!fake_mask);
    assert(nx_device_close(active_controller) == NX_ERR_BUSY);
    assert(nx_device_spi_close(active_child) == NX_ERR_BUSY);
    assert(nx_device_spi_cancel_transfer(active_child) == NX_OK);
    /* Request acceptance does not release the blocking call's ownership. */
    assert(nx_device_spi_close(active_child) == NX_ERR_BUSY);
    ++cancellation_requests;
}

int main(void) {
    fake_isr = 1u;
    assert(nx_hal_init() == NX_ERR_CONTEXT && !fake_board_safe_inits);
    fake_isr = 0u;
    fake_cs = fake_de = true;
    clock_result = -1;
    assert(nx_hal_init() == NX_ERR_IO);
    assert(!fake_cs && !fake_de && !grouping_calls && !systick_calls);
    assert(nx_hal_deinit() == NX_OK);
    clock_result = 0;
    assert(nx_hal_init() == NX_OK && nx_hal_init() == NX_OK);
    assert(clock_checks == 2u && grouping_calls == 1u && systick_calls == 1u);
    assert(fake_timer_config.prescaler == 99u && fake_timer_config.period == UINT32_MAX);
    assert(nx_hal_deinit() == NX_OK);
    assert(!fake_timer_ctl0 && !fake_timer_dmainten && !fake_rcu_apb1en &&
           !fake_rcu_apb1spen);
    assert(nx_hal_init() == NX_OK);

    assert(nx_device_register(&NX_SPI4) == NX_OK);
    const nx_device_t* descriptor = NULL;
    assert(nx_device_discover("SPI4", NX_DEVICE_CLASS_UART, &descriptor) == NX_ERR_TYPE_MISMATCH);
    assert(!descriptor && !fake_reset_count);
    assert(nx_device_discover("SPI4", NX_DEVICE_CLASS_SPI, &descriptor) == NX_OK);
    assert(descriptor == &NX_SPI4 && !fake_reset_count);

    nx_device_ref_t controller, rejected;
    assert(nx_device_open("SPI4", NX_DEVICE_CLASS_SPI, 1u, &controller) == NX_OK);
    assert(fake_reset_count == 1u);
    assert(nx_device_open("SPI4", NX_DEVICE_CLASS_SPI, 2u, &rejected) == NX_ERR_BUSY);
    assert(!rejected.descriptor && nx_device_registry_reset() == NX_ERR_BUSY);
    nx_device_caps_t caps;
    assert(nx_device_query(controller, &caps) == NX_OK);
    assert(caps.device_class == NX_DEVICE_CLASS_SPI && (caps.flags & NX_DEVICE_CAP_SPI_DEVICES));

    nx_spi_device_config_t config = {.cs_pin = 0u, .speed = 1000000u,
        .mode = NX_SPI_MODE_3, .bit_order = NX_SPI_BIT_ORDER_MSB};
    nx_device_spi_ref_t child;
    nx_spi_device_config_t invalid = config;
    invalid.cs_pin = 1u;
    assert(nx_device_spi_open(controller, &invalid, &child) == NX_ERR_INVALID_PARAM);
    /* Failed provider admission must not leak the facade's parent lease. */
    assert(!child.slot && nx_device_close(controller) == NX_OK);
    nx_device_ref_t old_controller = controller;
    assert(nx_device_open("SPI4", NX_DEVICE_CLASS_SPI, 2u, &controller) == NX_OK);
    assert(nx_device_query(old_controller, &caps) == NX_ERR_INVALID_STATE);
    assert(nx_device_spi_open(controller, &config, &child) == NX_OK);
    assert(nx_device_spi_query(child, &caps) == NX_OK);
    const uint32_t required = NX_DEVICE_CAP_SPI_DEVICES | NX_DEVICE_CAP_SPI_QUEUE | NX_DEVICE_CAP_SPI_CANCEL;
    assert(caps.device_class == NX_DEVICE_CLASS_SPI && (caps.flags & required) == required);
    assert(nx_device_close(controller) == NX_ERR_BUSY);

    uint8_t tx[] = {0x42u, 0xAAu}, rx[] = {0u, 0u};
    completion_t completion = {.controller = controller, .child = child, .expected = NX_OK};
    nx_spi_transaction_t request = {.tx_data = tx, .rx_data = rx, .length = sizeof(tx),
        .timeout_ms = 20u, .callback = terminal, .user_data = &completion};
    uint32_t writes = fake_spi_writes;
    assert(nx_device_spi_submit(child, &request, &completion.ticket) == NX_OK);
    assert_result(child, completion.ticket, NX_ERR_BUSY, false);
    assert(fake_spi_writes == writes && nx_device_spi_close(child) == NX_ERR_BUSY);
    assert(nx_device_spi_service(controller) == NX_OK);
    assert_result(child, completion.ticket, NX_OK, true);
    assert(completion.calls == 1u && rx[0] == 0xBDu && rx[1] == 0x55u);
    assert(fake_spi_config.prescale == (6u << 3) &&
           fake_spi_config.clock_polarity_phase == SPI_CK_PL_HIGH_PH_2EDGE);
    assert(nx_device_spi_cancel(child, completion.ticket) == NX_ERR_NO_DATA);
    nx_device_spi_ticket_t stale_ticket = completion.ticket;

    writes = fake_spi_writes;
    memset(rx, 0, sizeof(rx));
    completion.expected = NX_ERR_CANCELLED;
    assert(nx_device_spi_submit(child, &request, &completion.ticket) == NX_OK);
    assert(nx_device_spi_cancel(child, completion.ticket) == NX_OK);
    assert_result(child, completion.ticket, NX_ERR_BUSY, false);
    assert(nx_device_spi_close(child) == NX_ERR_BUSY && nx_device_close(controller) == NX_ERR_BUSY);
    assert(nx_device_spi_service(controller) == NX_ERR_CANCELLED);
    assert_result(child, completion.ticket, NX_ERR_CANCELLED, true);
    assert(completion.calls == 2u && fake_spi_writes == writes && !rx[0] && !rx[1]);

    request.timeout_ms = 5u;
    completion.expected = NX_ERR_TIMEOUT;
    assert(nx_device_spi_submit(child, &request, &completion.ticket) == NX_OK);
    fake_timer_count += 6000u;
    assert(nx_device_spi_service(controller) == NX_ERR_TIMEOUT);
    assert_result(child, completion.ticket, NX_ERR_TIMEOUT, true);
    assert(completion.calls == 3u && fake_spi_writes == writes);

    /* Blocking paths reject a mask before touching the provider/hardware. */
    request.callback = NULL;
    fake_mask = 1u;
    assert(nx_device_spi_transfer(child, &request) == NX_ERR_INVALID_STATE);
    assert(nx_device_spi_service(controller) == NX_ERR_INVALID_STATE);
    assert(fake_mask == 1u && fake_spi_writes == writes);
    fake_mask = 0u;
    active_controller = controller;
    active_child = child;
    fake_spi_hook = cancel_blocking;
    assert(nx_device_spi_transfer(child, &request) == NX_ERR_CANCELLED);
    assert(cancellation_requests == 1u && !fake_cs && !fake_spi_shift);
    assert(nx_device_spi_cancel_transfer(child) == NX_ERR_NO_DATA);

    nx_device_spi_ref_t stale_child = child;
    assert(nx_device_spi_close(child) == NX_OK);
    assert(nx_device_spi_open(controller, &config, &child) == NX_OK);
    assert(child.slot == stale_child.slot && child.generation != stale_child.generation);
    assert(nx_device_spi_query(stale_child, &caps) == NX_ERR_INVALID_STATE);
    nx_device_spi_ticket_t next_ticket;
    /* The facade supplies a terminal bridge even without a caller callback. */
    assert(nx_device_spi_submit(child, &request, &next_ticket) == NX_OK);
    assert(next_ticket.sequence > stale_ticket.sequence && next_ticket.generation != stale_ticket.generation);
    assert(nx_device_spi_cancel(child, stale_ticket) == NX_ERR_INVALID_STATE);
    assert(nx_device_spi_service(controller) == NX_OK);
    assert_result(child, next_ticket, NX_OK, true);
    assert(nx_device_spi_close(child) == NX_OK);
    assert(nx_device_close(controller) == NX_OK && !fake_cs && !fake_spi_shift);
    assert(nx_device_spi_query(child, &caps) == NX_ERR_INVALID_STATE);
    assert(nx_device_query(controller, &caps) == NX_ERR_INVALID_STATE);

    assert(nx_device_open("SPI4", NX_DEVICE_CLASS_SPI, 3u, &active_controller) == NX_OK);
    assert(active_controller.generation != controller.generation);
    assert(nx_device_spi_query(child, &caps) == NX_ERR_INVALID_STATE);
    assert(nx_device_close(active_controller) == NX_OK);
    assert(nx_hal_deinit() == NX_OK);
    assert(nx_device_registry_reset() == NX_OK);
    assert(nx_device_discover("SPI4", NX_DEVICE_CLASS_SPI, &descriptor) == NX_ERR_NOT_FOUND);
    puts("GD32 production boot, typed SPI4 leases, queue settlement, cancellation and stale generations passed");
    return 0;
}
