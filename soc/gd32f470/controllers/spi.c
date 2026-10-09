/* GD32F470 SPI4: bounded device values and task-serviced transactions.
 * Hardware is polling only; no DMA or hidden allocation is advertised. */
#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "board.h"
#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include "nexus_config.h"
#include <limits.h>
#include <string.h>

typedef struct {
    nx_spi_device_config_t config;
    nx_spi_transaction_t pending;
    uint64_t generation;
    uint64_t order;
    uint32_t submitted_ms;
    bool allocated;
    bool queued;
    bool cancelled;
} device_slot_t;
typedef struct {
    nx_spi_bus_t api;
    nx_lifecycle_t lifecycle;
    nx_device_state_t state;
    device_slot_t slots[NX_CONFIG_GD32_SPI_DEVICE_CAPACITY];
    device_slot_t* active;
    device_slot_t* callback_slot;
    uint64_t generation;
    uint64_t order;
    volatile bool cancel_active;
    bool servicing;
} spi_instance_t;
static spi_instance_t bus;
static nx_device_config_state_t device_state;

static bool config_valid(const nx_spi_device_config_t* config) {
    /* SPI4 is on APB2=100MHz; divider range 2..256. Requested speed is a
     * ceiling, and impossible lower ceilings are rejected explicitly. */
    return config && config->cs_pin == 0u && config->mode <= NX_SPI_MODE_3 &&
        config->bit_order <= NX_SPI_BIT_ORDER_LSB &&
        config->speed >= 390625u && config->speed <= 50000000u;
}
static device_slot_t* resolve(const nx_spi_device_t* device) {
    if (!device || device->owner != &bus.api || !device->token) { return NULL; }
    for (size_t i = 0; i < NX_CONFIG_GD32_SPI_DEVICE_CAPACITY; ++i) {
        device_slot_t* slot = &bus.slots[i];
        if (slot->allocated && slot->generation == device->token) { return slot; }
    }
    return NULL;
}
static bool lifecycle_busy(void) {
    if (bus.active || bus.callback_slot || bus.servicing) { return true; }
    for (size_t i = 0; i < NX_CONFIG_GD32_SPI_DEVICE_CAPACITY; ++i) {
        if (bus.slots[i].queued) { return true; }
    }
    return false;
}
static nx_status_t reset_controller(void) {
    spi_disable(SPI4);
    rcu_periph_reset_enable(RCU_SPI4RST);
    rcu_periph_reset_disable(RCU_SPI4RST);
    nx_arch_dsb();
    return nx_gd32_board_spi_cs(0u, false);
}
static nx_status_t spi_init_device(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (lifecycle_busy()) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    if (bus.state == NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_OK; }
    rcu_periph_clock_enable(RCU_SPI4);
    nx_status_t status = reset_controller();
    if (status == NX_OK) {
        status = nx_gd32_board_spi_pins(true);
    }
    if (status == NX_OK) { bus.state = NX_DEV_STATE_RUNNING; }
    nx_arch_irq_restore(saved);
    return status;
}
static nx_status_t spi_deinit_device(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (lifecycle_busy()) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    nx_status_t status = reset_controller();
    if (status == NX_OK) {
        status = nx_gd32_board_spi_pins(false);
    }
    if (status != NX_OK) {
        nx_arch_irq_restore(saved);
        return status;
    }
    for (size_t i = 0; i < NX_CONFIG_GD32_SPI_DEVICE_CAPACITY; ++i) {
        bus.slots[i].allocated = false;
    }
    bus.state = NX_DEV_STATE_UNINITIALIZED;
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t spi_suspend(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (bus.state != NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (lifecycle_busy()) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    nx_status_t status = reset_controller();
    if (status == NX_OK) {
        status = nx_gd32_board_spi_pins(false);
    }
    if (status != NX_OK) {
        nx_arch_irq_restore(saved);
        return status;
    }
    bus.state = NX_DEV_STATE_SUSPENDED;
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t spi_resume(nx_lifecycle_t* self) {
    return bus.state == NX_DEV_STATE_SUSPENDED ? spi_init_device(self) : NX_ERR_INVALID_STATE;
}
static nx_device_state_t spi_state(nx_lifecycle_t* self) { (void)self; return bus.state; }

static bool expired(uint32_t started, uint32_t timeout) {
    return timeout != UINT32_MAX && (uint32_t)(nx_gd32f470_millis() - started) >= timeout;
}
static nx_status_t wait_flag(uint32_t flag, bool asserted, uint32_t started, uint32_t timeout) {
    for (;;) {
        if (bus.cancel_active) { return NX_ERR_CANCELLED; }
        if (expired(started, timeout)) { return NX_ERR_TIMEOUT; }
        uint32_t flags = SPI_STAT(SPI4);
        if (flags & (SPI_STAT_CONFERR | SPI_STAT_RXORERR | SPI_STAT_FERR)) { return NX_ERR_IO; }
        if (((flags & flag) != 0u) == asserted) { return NX_OK; }
    }
}
static nx_status_t execute(const nx_spi_device_config_t* config,
                            const nx_spi_transaction_t* transaction, uint32_t started) {
    if (expired(started, transaction->timeout_ms)) { return NX_ERR_TIMEOUT; }
    nx_status_t status = reset_controller();
    if (status != NX_OK) {
        return status;
    }
    spi_parameter_struct parameters;
    spi_struct_para_init(&parameters);
    parameters.device_mode = SPI_MASTER;
    parameters.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    parameters.frame_size = SPI_FRAMESIZE_8BIT;
    parameters.nss = SPI_NSS_SOFT;
    parameters.endian = config->bit_order == NX_SPI_BIT_ORDER_MSB ? SPI_ENDIAN_MSB : SPI_ENDIAN_LSB;
    static const uint32_t modes[] = { SPI_CK_PL_LOW_PH_1EDGE, SPI_CK_PL_LOW_PH_2EDGE,
                                     SPI_CK_PL_HIGH_PH_1EDGE, SPI_CK_PL_HIGH_PH_2EDGE };
    parameters.clock_polarity_phase = modes[config->mode];
    uint32_t divider = 2u;
    unsigned shift = 0u;
    while (100000000u / divider > config->speed && divider < 256u) { divider *= 2u; ++shift; }
    parameters.prescale = shift << 3;
    spi_init(SPI4, &parameters);
    spi_enable(SPI4);
    status = nx_gd32_board_spi_cs(config->cs_pin, true);
    for (size_t i = 0; status == NX_OK && i < transaction->length; ++i) {
        status = wait_flag(SPI_FLAG_TBE, true, started, transaction->timeout_ms);
        if (status != NX_OK) { break; }
        spi_i2s_data_transmit(SPI4, transaction->tx_data[i]);
        status = wait_flag(SPI_FLAG_RBNE, true, started, transaction->timeout_ms);
        if (status != NX_OK) { break; }
        uint8_t received = (uint8_t)spi_i2s_data_receive(SPI4);
        if (transaction->rx_data) { transaction->rx_data[i] = received; }
    }
    if (status == NX_OK) { status = wait_flag(SPI_FLAG_TRANS, false, started, transaction->timeout_ms); }
    /* Reset disables the shifter even on timeout/cancel before releasing CS,
     * caller buffers, or invoking callbacks. A failed frame may be truncated. */
    nx_status_t cleanup = reset_controller();
    return cleanup != NX_OK ? cleanup : status;
}
static bool transaction_valid(const nx_spi_transaction_t* transaction, bool queued) {
    return transaction && transaction->tx_data && transaction->length && transaction->timeout_ms &&
        (transaction->timeout_ms <= INT32_MAX || (!queued && transaction->timeout_ms == UINT32_MAX)) &&
        (!queued || transaction->callback);
}
static nx_status_t transfer(nx_spi_device_t* device, const nx_spi_transaction_t* transaction) {
    uint32_t started = nx_gd32f470_millis();
    if (!transaction_valid(transaction, false)) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    device_slot_t* slot = resolve(device);
    if (!slot || bus.state != NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (bus.active || slot->queued || bus.callback_slot) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    nx_spi_device_config_t config = slot->config;
    bus.active = slot;
    bus.cancel_active = false;
    nx_arch_irq_restore(saved);
    nx_status_t status = execute(&config, transaction, started);
    saved = nx_arch_irq_save();
    bus.active = NULL;
    if (transaction->callback) { bus.callback_slot = slot; }
    nx_arch_irq_restore(saved);
    if (transaction->callback) {
        transaction->callback(transaction->user_data, status);
        saved = nx_arch_irq_save();
        bus.callback_slot = NULL;
        nx_arch_irq_restore(saved);
    }
    return status;
}
static nx_status_t submit(nx_spi_device_t* device, const nx_spi_transaction_t* transaction) {
    if (!transaction_valid(transaction, true)) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    device_slot_t* slot = resolve(device);
    if (!slot || bus.state != NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (slot->queued || bus.active == slot || bus.callback_slot == slot) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    if (bus.order == UINT64_MAX) { nx_arch_irq_restore(saved); return NX_ERR_FULL; }
    slot->pending = *transaction;
    slot->submitted_ms = nx_gd32f470_millis();
    slot->order = ++bus.order;
    slot->queued = true;
    slot->cancelled = false;
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_status_t cancel(nx_spi_device_t* device) {
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    device_slot_t* slot = resolve(device);
    nx_status_t status = NX_ERR_NO_DATA;
    if (!slot) { status = NX_ERR_INVALID_STATE; }
    else if (bus.active == slot) { bus.cancel_active = true; status = NX_OK; }
    else if (slot->queued) { slot->cancelled = true; status = NX_OK; }
    nx_arch_irq_restore(saved);
    return status;
}
static nx_status_t service(nx_spi_bus_t* self) {
    if (self != &bus.api) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (bus.state != NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (bus.active || bus.servicing || bus.callback_slot) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    device_slot_t* slot = NULL;
    for (size_t i = 0; i < NX_CONFIG_GD32_SPI_DEVICE_CAPACITY; ++i) {
        if (bus.slots[i].queued && (!slot || bus.slots[i].order < slot->order)) { slot = &bus.slots[i]; }
    }
    if (!slot) { nx_arch_irq_restore(saved); return NX_ERR_NO_DATA; }
    nx_spi_transaction_t transaction = slot->pending;
    nx_spi_device_config_t config = slot->config;
    uint32_t started = slot->submitted_ms;
    bool cancelled = slot->cancelled;
    slot->queued = false;
    memset(&slot->pending, 0, sizeof(slot->pending));
    bus.servicing = true;
    bus.active = slot;
    bus.cancel_active = cancelled;
    nx_arch_irq_restore(saved);
    nx_status_t status = cancelled ? NX_ERR_CANCELLED : execute(&config, &transaction, started);
    saved = nx_arch_irq_save();
    bus.active = NULL;
    bus.callback_slot = slot;
    nx_arch_irq_restore(saved);
    transaction.callback(transaction.user_data, status);
    saved = nx_arch_irq_save();
    bus.callback_slot = NULL;
    bus.servicing = false;
    nx_arch_irq_restore(saved);
    return status;
}
static nx_status_t open_device(nx_spi_bus_t* self, const nx_spi_device_config_t* config,
                               nx_spi_device_t* device) {
    if (self != &bus.api || !device || !config_valid(config)) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (bus.state != NX_DEV_STATE_RUNNING) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (bus.generation == UINT64_MAX) { nx_arch_irq_restore(saved); return NX_ERR_FULL; }
    for (size_t i = 0; i < NX_CONFIG_GD32_SPI_DEVICE_CAPACITY; ++i) {
        device_slot_t* slot = &bus.slots[i];
        if (slot->allocated) { continue; }
        slot->config = *config;
        slot->generation = ++bus.generation;
        slot->allocated = true;
        *device = (nx_spi_device_t){ .owner = &bus.api, .token = slot->generation,
            .transfer = transfer, .submit = submit, .cancel = cancel };
        nx_arch_irq_restore(saved);
        return NX_OK;
    }
    nx_arch_irq_restore(saved);
    return NX_ERR_FULL;
}
static nx_status_t close_device(nx_spi_bus_t* self, nx_spi_device_t* device) {
    if (self != &bus.api || !device) { return NX_ERR_INVALID_PARAM; }
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    device_slot_t* slot = resolve(device);
    if (!slot) { nx_arch_irq_restore(saved); return NX_ERR_INVALID_STATE; }
    if (slot->queued || bus.active == slot || bus.callback_slot == slot) { nx_arch_irq_restore(saved); return NX_ERR_BUSY; }
    slot->allocated = false;
    memset(device, 0, sizeof(*device));
    nx_arch_irq_restore(saved);
    return NX_OK;
}
static nx_tx_async_t* no_tx_async(nx_spi_bus_t* self, nx_spi_device_config_t c) { (void)self; (void)c; return NULL; }
static nx_tx_rx_async_t* no_tx_rx_async(nx_spi_bus_t* self, nx_spi_device_config_t c,
                                      nx_comm_callback_t callback, void* data) {
    (void)self; (void)c; (void)callback; (void)data; return NULL;
}
static nx_tx_sync_t* no_tx_sync(nx_spi_bus_t* self, nx_spi_device_config_t c) { (void)self; (void)c; return NULL; }
static nx_tx_rx_sync_t* no_tx_rx_sync(nx_spi_bus_t* self, nx_spi_device_config_t c) { (void)self; (void)c; return NULL; }
static nx_lifecycle_t* get_lifecycle(nx_spi_bus_t* self) { (void)self; return &bus.lifecycle; }
static nx_power_t* get_power(nx_spi_bus_t* self) { (void)self; return NULL; }
static nx_status_t construct_spi(const nx_device_t* descriptor, void** out) {
    if (!out) {
        return NX_ERR_NULL_PTR;
    }
    *out = NULL;
    if (!descriptor || descriptor->state != &device_state) {
        return NX_ERR_INVALID_PARAM;
    }
    bus.api = (nx_spi_bus_t){ .open_device = open_device, .close_device = close_device, .service = service,
        .get_tx_async_handle = no_tx_async, .get_tx_rx_async_handle = no_tx_rx_async,
        .get_tx_sync_handle = no_tx_sync, .get_tx_rx_sync_handle = no_tx_rx_sync,
        .get_lifecycle = get_lifecycle, .get_power = get_power };
    bus.lifecycle = (nx_lifecycle_t){ .init = spi_init_device, .deinit = spi_deinit_device,
        .suspend = spi_suspend, .resume = spi_resume, .get_state = spi_state };
    *out = &bus.api;
    return NX_OK;
}
NX_DEVICE_REGISTER_TYPED(NX_SPI, 4, "SPI4", NULL, &device_state,
                         NX_DEVICE_CLASS_SPI,
                         NX_DEVICE_CAP_SPI_DEVICES | NX_DEVICE_CAP_SPI_QUEUE |
                             NX_DEVICE_CAP_SPI_CANCEL,
                         construct_spi, NULL);
