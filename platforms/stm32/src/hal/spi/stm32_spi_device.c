/** STM32 SPI bus owns arbitration; each slave owns immutable configuration. */
#include "hal/base/nx_device.h"
#include "stm32_spi.h"
#include <string.h>

static stm32_spi_device_t* resolve_device(nx_spi_device_t* self) {
    if (!self || !self->owner || !self->token) return NULL;
    stm32_spi_impl_t* bus = NX_CONTAINER_OF(self->owner, stm32_spi_impl_t, base);
    for (unsigned i = 0; i < STM32_SPI_MAX_DEVICES; ++i)
        if (bus->devices[i].allocated && bus->devices[i].base.token == self->token)
            return &bus->devices[i];
    return NULL;
}
static nx_status_t device_transfer(nx_spi_device_t* self,
                                    const nx_spi_transaction_t* transaction) {
    if (!self || !self->owner) return NX_ERR_INVALID_PARAM;
    uint32_t started_at = HAL_GetTick();
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* d = resolve_device(self);
    uint64_t token = self ? self->token : 0;
    spi_critical_leave(saved);
    return d ? spi_transfer(d, transaction, started_at, false, token) : NX_ERR_INVALID_STATE;
}
static nx_status_t device_submit(nx_spi_device_t* self,
                                  const nx_spi_transaction_t* transaction) {
    if (!self || !self->owner) return NX_ERR_INVALID_PARAM;
    uint32_t started_at = HAL_GetTick();
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* d = resolve_device(self);
    uint64_t token = self ? self->token : 0;
    spi_critical_leave(saved);
    return d ? spi_submit(d, transaction, token, started_at) : NX_ERR_INVALID_STATE;
}
static nx_status_t device_cancel(nx_spi_device_t* self) {
    if (!self || !self->owner) return NX_ERR_INVALID_PARAM;
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* d = resolve_device(self);
    uint64_t token = self ? self->token : 0;
    spi_critical_leave(saved);
    return d ? spi_cancel(d, token) : NX_ERR_INVALID_STATE;
}
static bool config_valid(const nx_spi_device_config_t* c) {
    return c && c->speed && c->mode <= NX_SPI_MODE_3 &&
           c->bit_order <= NX_SPI_BIT_ORDER_LSB;
}
static bool config_equal(const nx_spi_device_config_t* a,
                           const nx_spi_device_config_t* b) {
    return a->cs_pin == b->cs_pin && a->speed == b->speed &&
           a->mode == b->mode && a->bit_order == b->bit_order;
}
static stm32_spi_device_t* allocate_device(stm32_spi_impl_t* bus,
                                           nx_spi_device_config_t config,
                                           bool legacy,
                                           nx_comm_callback_t callback,
                                           void* context) {
    stm32_spi_device_t* free_slot = NULL;
    uint32_t saved = spi_critical_enter();
    for (unsigned i = 0; i < STM32_SPI_MAX_DEVICES; ++i) {
        stm32_spi_device_t* d = &bus->devices[i];
        if (!d->allocated) {
            if (!free_slot) free_slot = d;
        } else if (legacy && d->legacy && config_equal(&d->config, &config) &&
                   d->receive_callback == callback &&
                   d->receive_context == context) {
            spi_critical_leave(saved);
            return d;
        }
    }
    if (free_slot && bus->next_token == UINT64_MAX) free_slot = NULL;
    if (free_slot) {
        /* Reservation and initialization are one atomic bounded operation. */
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->bus = bus;
        free_slot->config = config;
        free_slot->legacy = legacy;
        free_slot->receive_callback = callback;
        free_slot->receive_context = context;
        free_slot->base.owner = &bus->base;
        free_slot->base.token = ++bus->next_token;
        free_slot->base.transfer = device_transfer;
        free_slot->base.submit = device_submit;
        free_slot->base.cancel = device_cancel;
        spi_init_tx_sync(&free_slot->tx_sync);
        spi_init_tx_rx_sync(&free_slot->tx_rx_sync);
        spi_init_tx_async(&free_slot->tx_async);
        spi_init_tx_rx_async(&free_slot->tx_rx_async);
        free_slot->allocated = true;
    }
    spi_critical_leave(saved);
    return free_slot;
}
static nx_status_t open_device(nx_spi_bus_t* self,
                                const nx_spi_device_config_t* config,
                                nx_spi_device_t* out) {
    if (out) memset(out, 0, sizeof(*out));
    if (!self || !out || !config_valid(config)) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    stm32_spi_impl_t* bus = NX_CONTAINER_OF(self, stm32_spi_impl_t, base);
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* d = allocate_device(bus, *config, false, NULL, NULL);
    if (d) *out = d->base;
    spi_critical_leave(saved);
    return d ? NX_OK : NX_ERR_NO_RESOURCE;
}
static nx_status_t close_device(nx_spi_bus_t* self, nx_spi_device_t* device) {
    if (!self || !device) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* d = device->owner == self ? resolve_device(device) : NULL;
    nx_status_t r = !d ? NX_ERR_INVALID_STATE : d->legacy ? NX_ERR_NOT_SUPPORTED :
                    d->users || d->pending || d->servicing ? NX_ERR_BUSY : NX_OK;
    if (r == NX_OK) d->allocated = false;
    spi_critical_leave(saved);
    return r;
}
static stm32_spi_device_t* legacy_device(nx_spi_bus_t* self,
                                          nx_spi_device_config_t config,
                                          nx_comm_callback_t callback,
                                          void* context) {
    if (!self || !config_valid(&config) || __get_IPSR()) return NULL;
    return allocate_device(NX_CONTAINER_OF(self, stm32_spi_impl_t, base),
                            config, true, callback, context);
}
static nx_tx_sync_t* get_tx_sync(nx_spi_bus_t* self,
                                 nx_spi_device_config_t config) {
    stm32_spi_device_t* d = legacy_device(self, config, NULL, NULL);
    return d ? &d->tx_sync : NULL;
}
static nx_tx_rx_sync_t* get_tx_rx_sync(nx_spi_bus_t* self,
                                       nx_spi_device_config_t config) {
    stm32_spi_device_t* d = legacy_device(self, config, NULL, NULL);
    return d ? &d->tx_rx_sync : NULL;
}
static nx_tx_async_t* get_tx_async(nx_spi_bus_t* self,
                                   nx_spi_device_config_t config) {
    stm32_spi_device_t* d = legacy_device(self, config, NULL, NULL);
    return d ? &d->tx_async : NULL;
}
static nx_tx_rx_async_t* get_tx_rx_async(nx_spi_bus_t* self,
                                         nx_spi_device_config_t config,
                                         nx_comm_callback_t callback,
                                         void* context) {
    if (!callback) return NULL;
    stm32_spi_device_t* d = legacy_device(self, config, callback, context);
    return d ? &d->tx_rx_async : NULL;
}
static nx_lifecycle_t* get_lifecycle(nx_spi_bus_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_spi_impl_t, base)->lifecycle : NULL;
}
static nx_power_t* get_power(nx_spi_bus_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_spi_impl_t, base)->power : NULL;
}
void stm32_spi_construct(stm32_spi_impl_t* bus,
                         const stm32_spi_platform_config_t* cfg) {
    memset(bus, 0, sizeof(*bus));
    bus->state = &bus->storage;
    bus->state->instance = cfg->spi_index;
    bus->dma_tx_enabled = cfg->use_dma;
    bus->dma_rx_enabled = cfg->use_dma;
    bus->hspi.Instance = cfg->spi_base;
    bus->hspi.Init.Mode = cfg->mode;
    bus->hspi.Init.Direction = cfg->direction;
    bus->hspi.Init.DataSize = cfg->data_size;
    bus->hspi.Init.CLKPolarity = cfg->clk_polarity;
    bus->hspi.Init.CLKPhase = cfg->clk_phase;
    bus->hspi.Init.NSS = cfg->nss;
    bus->hspi.Init.BaudRatePrescaler = cfg->baud_prescaler;
    bus->hspi.Init.FirstBit = cfg->first_bit;
    bus->hspi.Init.TIMode = cfg->ti_mode;
    bus->hspi.Init.CRCCalculation = cfg->crc_calculation;
    bus->hspi.Init.CRCPolynomial = cfg->crc_polynomial;
    NX_INIT_SPI_BUS(&bus->base, get_tx_async, get_tx_rx_async, get_tx_sync,
                     get_tx_rx_sync, get_lifecycle, get_power);
    bus->base.open_device = open_device;
    bus->base.close_device = close_device;
    bus->base.service = spi_service;
    spi_init_lifecycle(&bus->lifecycle);
    spi_init_power(&bus->power);
}

/* A board profile may override these hooks. Missing wiring fails explicitly. */
NX_WEAK nx_status_t stm32_spi_board_prepare(stm32_spi_impl_t* bus) {
    (void)bus;
    return NX_ERR_NOT_SUPPORTED;
}
NX_WEAK nx_status_t stm32_spi_board_select(stm32_spi_impl_t* bus, uint8_t cs,
                                           bool active) {
    (void)bus; (void)cs; (void)active;
    return NX_ERR_NOT_SUPPORTED;
}
NX_WEAK uint32_t stm32_spi_board_clock_hz(stm32_spi_impl_t* bus) {
    (void)bus;
    return 0;
}
NX_WEAK void stm32_spi_board_release(stm32_spi_impl_t* bus) { (void)bus; }
NX_WEAK bool stm32_spi_board_dma_buffer_valid(const void* data, size_t length,
                                              bool write) {
#if defined(STM32F407xx)
    uintptr_t begin = (uintptr_t)data;
    if (!length || length > UINTPTR_MAX - begin) return false;
    uintptr_t end = begin + length;
    /* F407VG: 128 KiB SRAM on AHB; 64 KiB CCM is NOT DMA-accessible. */
    if (begin >= 0x20000000U && end <= 0x20020000U) return true;
    return !write && begin >= 0x08000000U && end <= 0x08100000U;
#else
    (void)data; (void)length; (void)write;
    return false; /* Require an explicit memory map for another device. */
#endif
}

/* SPI instances are bounded static objects. STM32 uses SPI1..SPI6, not SPI0. */
#define REGISTER_SPI(index)                                                    \
    static stm32_spi_impl_t spi_bus_##index;                                   \
    static const stm32_spi_platform_config_t spi_cfg_##index = {               \
        .spi_base = SPI##index, .spi_index = index, .mode = SPI_MODE_MASTER,    \
        .direction = SPI_DIRECTION_2LINES, .data_size = SPI_DATASIZE_8BIT,     \
        .clk_polarity = SPI_POLARITY_LOW, .clk_phase = SPI_PHASE_1EDGE,         \
        .nss = SPI_NSS_SOFT, .baud_prescaler = SPI_BAUDRATEPRESCALER_16,       \
        .first_bit = SPI_FIRSTBIT_MSB, .ti_mode = SPI_TIMODE_DISABLE,           \
        .crc_calculation = SPI_CRCCALCULATION_DISABLE, .crc_polynomial = 7,    \
        .use_dma = SPI_DMA_ENABLED,                                           \
    };                                                                        \
    static void* spi_create_##index(const nx_device_t* dev) {                  \
        stm32_spi_construct(&spi_bus_##index, dev->config);                    \
        return &spi_bus_##index.base;                                         \
    }                                                                         \
    static nx_device_config_state_t spi_reg_##index;                           \
    NX_DEVICE_REGISTER(NX_SPI, index, "SPI" #index, &spi_cfg_##index,         \
                        &spi_reg_##index, spi_create_##index)
#ifdef NX_CONFIG_STM32_SPI_USE_DMA
#define SPI_DMA_ENABLED true
#else
#define SPI_DMA_ENABLED false
#endif
#if defined(NX_CONFIG_STM32_SPI0_ENABLE)
#error "STM32 SPI0 does not exist; select SPI1, SPI2 or SPI3"
#endif
#if defined(NX_CONFIG_STM32_SPI1_ENABLE)
REGISTER_SPI(1);
#endif
#if defined(NX_CONFIG_STM32_SPI2_ENABLE)
REGISTER_SPI(2);
#endif
#if defined(NX_CONFIG_STM32_SPI3_ENABLE)
REGISTER_SPI(3);
#endif
#if defined(NX_CONFIG_STM32_SPI4_ENABLE)
REGISTER_SPI(4);
#endif
#if defined(NX_CONFIG_STM32_SPI5_ENABLE)
REGISTER_SPI(5);
#endif
#if defined(NX_CONFIG_STM32_SPI6_ENABLE)
REGISTER_SPI(6);
#endif
