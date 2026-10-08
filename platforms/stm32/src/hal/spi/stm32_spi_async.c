/** Bounded task-serviced queue; no hidden worker and no heap on submit. */
#include "stm32_spi.h"
#include <string.h>

nx_status_t spi_submit(stm32_spi_device_t* d, const nx_spi_transaction_t* t, uint64_t token, uint32_t started_at) {
    if (!d || !d->bus || !t || !t->tx_data || !t->length ||
        t->length > UINT16_MAX || !t->callback || !t->timeout_ms ||
        t->timeout_ms > INT32_MAX) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    uint32_t saved = spi_critical_enter();
    stm32_spi_impl_t* b = d->bus;
    nx_status_t result = !d->allocated || d->base.token != token ? NX_ERR_INVALID_STATE :
                         !b->state->initialized ? NX_ERR_NOT_INIT :
                         b->state->fault ? NX_ERR_HARDWARE :
                         b->state->suspended ? NX_ERR_SUSPENDED :
                         b->state->transitioning || d->pending || d->servicing || d->users ?
                             NX_ERR_BUSY : NX_OK;
    if (result == NX_OK) {
        d->queued = *t;
        d->queued_at = started_at;
        d->sequence = ++b->sequence;
        d->cancelled = false;
        d->completing = false;
        d->last_result = NX_ERR_BUSY;
        d->pending = true;
    }
    spi_critical_leave(saved);
    return result;
}
nx_status_t spi_cancel(stm32_spi_device_t* d, uint64_t token) {
    if (!d || !d->bus) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = d->bus;
    uint32_t saved = spi_critical_enter();
    nx_status_t r = NX_ERR_NOT_FOUND;
    bool signal = false;
    if (!d->allocated || d->base.token != token) r = NX_ERR_INVALID_STATE;
    else if (d->pending || ((d->users || (d->servicing && !d->completing)) && b->active != d)) {
        d->cancelled = true;
        r = NX_OK;
    } else if (b->active == d && b->phase == STM32_SPI_WAITING) {
        if (!b->dma_active) r = NX_ERR_NOT_SUPPORTED;
        else { signal = spi_publish(b, NX_ERR_CANCELLED); r = NX_OK; }
    }
    spi_critical_leave(saved);
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    if (signal && b->dma_sem) {
        if (__get_IPSR()) (void)osal_sem_give_from_isr(b->dma_sem);
        else (void)osal_sem_give(b->dma_sem);
    }
#else
    (void)signal;
#endif
    return r;
}
nx_status_t spi_service(nx_spi_bus_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, base);
    uint32_t saved = spi_critical_enter();
    stm32_spi_device_t* oldest = NULL;
    if (b->worker_active) { spi_critical_leave(saved); return NX_ERR_BUSY; }
    /* Only one service call may execute/callback at a time. */
    for (unsigned i = 0; i < STM32_SPI_MAX_DEVICES; ++i) {
        stm32_spi_device_t* d = &b->devices[i];
        if (d->servicing) { spi_critical_leave(saved); return NX_ERR_BUSY; }
        if (d->pending && (!oldest || (int32_t)(d->sequence - oldest->sequence) < 0))
            oldest = d;
    }
    if (!oldest) { spi_critical_leave(saved); return NX_ERR_NO_DATA; }
    b->worker_active = true;
    oldest->pending = false;
    oldest->servicing = true;
    oldest->completing = false;
    bool cancelled = oldest->cancelled;
    nx_spi_transaction_t transaction = oldest->queued;
    uint32_t started_at = oldest->queued_at;
    spi_critical_leave(saved);
    nx_status_t result = cancelled ? NX_ERR_CANCELLED :
        spi_transfer(oldest, &transaction, started_at, true, oldest->base.token);
    saved = spi_critical_enter();
    oldest->last_result = result;
    oldest->cancelled = false;
    oldest->servicing = false;
    oldest->completing = true;
    /* Callback may resubmit or close this device. The worker flag prevents
     * recursive service and lifecycle changes until the callback returns. */
    spi_critical_leave(saved);
    transaction.callback(transaction.user_data, result);
    saved = spi_critical_enter();
    b->worker_active = false;
    spi_critical_leave(saved);
    return result;
}

static void legacy_terminal(void* user_data, nx_status_t result) {
    stm32_spi_device_t* d = user_data;
    if (result == NX_OK && d->receive_callback)
        d->receive_callback(d->receive_context, d->rx_copy, d->queued.length);
}
static nx_status_t legacy_submit(stm32_spi_device_t* d, const uint8_t* data,
                                  size_t length, uint32_t timeout, bool receive) {
    uint32_t started_at = HAL_GetTick();
    if (!data || !length || length > STM32_SPI_ASYNC_BYTES) return NX_ERR_INVALID_SIZE;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    uint32_t saved = spi_critical_enter();
    if (d->pending || d->users || d->servicing) {
        spi_critical_leave(saved);
        return NX_ERR_BUSY;
    }
    /* Legacy send owns a copy. Mutating caller data after send is safe. */
    memcpy(d->tx_copy, data, length);
    nx_spi_transaction_t t = {d->tx_copy, receive ? d->rx_copy : NULL,
                               length, timeout, legacy_terminal, d};
    nx_status_t r = spi_submit(d, &t, d->base.token, started_at);
    spi_critical_leave(saved);
    return r;
}
static nx_status_t send(nx_tx_async_t* self, const uint8_t* data, size_t length) {
    return self ? legacy_submit(NX_CONTAINER_OF(self, stm32_spi_device_t, tx_async),
                                 data, length, 1000, false) : NX_ERR_INVALID_PARAM;
}
static nx_status_t tx_state(nx_tx_async_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_device_t* d = NX_CONTAINER_OF(self, stm32_spi_device_t, tx_async);
    uint32_t saved = spi_critical_enter();
    nx_status_t r = d->pending || d->servicing || d->users ? NX_ERR_BUSY : d->last_result;
    spi_critical_leave(saved);
    return r;
}
static nx_status_t tx_rx(nx_tx_rx_async_t* self, const uint8_t* data,
                          size_t length, uint32_t timeout) {
    return self ? legacy_submit(NX_CONTAINER_OF(self, stm32_spi_device_t, tx_rx_async),
                                 data, length, timeout, true) : NX_ERR_INVALID_PARAM;
}
static nx_status_t tx_rx_state(nx_tx_rx_async_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_device_t* d = NX_CONTAINER_OF(self, stm32_spi_device_t, tx_rx_async);
    uint32_t saved = spi_critical_enter();
    nx_status_t r = d->pending || d->servicing || d->users ? NX_ERR_BUSY : d->last_result;
    spi_critical_leave(saved);
    return r;
}
void spi_init_tx_async(nx_tx_async_t* iface) { NX_INIT_TX_ASYNC(iface, send, tx_state); }
void spi_init_tx_rx_async(nx_tx_rx_async_t* iface) { NX_INIT_TX_RX_ASYNC(iface, tx_rx, tx_rx_state); }
