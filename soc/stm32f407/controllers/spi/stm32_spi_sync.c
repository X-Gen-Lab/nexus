/** Deadline-bounded transfers. DMA is always drained before releasing buffers. */
#include "stm32_spi.h"

static nx_status_t configure_slave(stm32_spi_device_t* d) {
    stm32_spi_impl_t* b = d->bus;
    uint32_t hz = stm32_spi_board_clock_hz(b->state->instance);
    if (!hz) return NX_ERR_NOT_SUPPORTED;
    uint32_t divider = 2;
    static const uint32_t prescalers[] = {
        SPI_BAUDRATEPRESCALER_2, SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8, SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32, SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256,
    };
    unsigned index = 0;
    /* Ceiling division ensures the requested maximum clock is never exceeded. */
    while (((uint64_t)hz + divider - 1) / divider > d->config.speed && index < 7) {
        divider <<= 1;
        ++index;
    }
    if (((uint64_t)hz + divider - 1) / divider > d->config.speed)
        return NX_ERR_NOT_SUPPORTED;
    b->hspi.Init.BaudRatePrescaler = prescalers[index];
    b->hspi.Init.CLKPolarity = d->config.mode & 2U ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
    b->hspi.Init.CLKPhase = d->config.mode & 1U ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;
    b->hspi.Init.FirstBit = d->config.bit_order == NX_SPI_BIT_ORDER_MSB ?
                            SPI_FIRSTBIT_MSB : SPI_FIRSTBIT_LSB;
    return spi_hal_result(HAL_SPI_Init(&b->hspi));
}

static nx_status_t admit(stm32_spi_device_t* d, bool worker, uint64_t token) {
    stm32_spi_impl_t* b = d->bus;
    uint32_t saved = spi_critical_enter();
    nx_status_t r = !d->allocated || d->base.token != token ? NX_ERR_INVALID_STATE :
                    !b->state->initialized ? NX_ERR_NOT_INIT :
                    b->state->transitioning ? NX_ERR_BUSY :
                    b->state->fault ? NX_ERR_HARDWARE :
                    b->state->suspended ? NX_ERR_SUSPENDED :
                    d->users || d->pending || (d->servicing && !worker) ? NX_ERR_BUSY : NX_OK;
    if (r == NX_OK) { ++b->state->users; ++d->users; }
    spi_critical_leave(saved);
    return r;
}
static void relinquish(stm32_spi_device_t* d) {
    uint32_t saved = spi_critical_enter();
    --d->users;
    --d->bus->state->users;
    spi_critical_leave(saved);
}

nx_status_t spi_transfer(stm32_spi_device_t* d,
                          const nx_spi_transaction_t* t,
                          uint32_t started_at, bool worker, uint64_t token) {
    if (!d || !d->bus || !t || !t->tx_data || !t->length || t->length > UINT16_MAX ||
        (t->timeout_ms != UINT32_MAX && t->timeout_ms > INT32_MAX))
        return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    nx_status_t result = admit(d, worker, token);
    if (result != NX_OK) return result;
    stm32_spi_impl_t* b = d->bus;
    bool locked = false, selected = false, dma_started = false;
    uint32_t remaining = spi_remaining(started_at, t->timeout_ms);
    if (!remaining) { result = NX_ERR_TIMEOUT; goto done; }
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    osal_status_t lock_result = osal_mutex_lock(b->mutex, remaining);
    if (lock_result != OSAL_OK) {
        result = lock_result == OSAL_ERROR_TIMEOUT ? NX_ERR_TIMEOUT : NX_ERR_IO;
        goto done;
    }
    locked = true;
#else
    uint32_t saved = spi_critical_enter();
    if (b->state->busy) { spi_critical_leave(saved); result = NX_ERR_BUSY; goto done; }
    b->state->busy = true;
    spi_critical_leave(saved);
    locked = true;
#endif
    b->state->busy = true;
    if (!spi_remaining(started_at, t->timeout_ms)) { result = NX_ERR_TIMEOUT; goto done; }
    result = configure_slave(d);
    if (result != NX_OK) goto done;
    if (!spi_remaining(started_at, t->timeout_ms)) { result = NX_ERR_TIMEOUT; goto done; }
    result = stm32_spi_board_select(b->state->instance, d->config.cs_pin, true);
    if (result != NX_OK) goto done;
    selected = true;

    {
        uint32_t saved = spi_critical_enter();
        b->active = d;
        b->phase = STM32_SPI_WAITING;
        b->result = NX_ERR_BUSY;
        b->dma_active = b->dma_tx_enabled && (!t->rx_data || b->dma_rx_enabled);
        bool cancelled = d->cancelled;
        d->cancelled = false;
        spi_critical_leave(saved);
        if (cancelled) { spi_publish(b, NX_ERR_CANCELLED); goto settle; }
    }
    remaining = spi_remaining(started_at, t->timeout_ms);
    if (!remaining) { spi_publish(b, NX_ERR_TIMEOUT); goto settle; }
    if (b->dma_active) {
        if (!stm32_spi_board_dma_buffer_valid(t->tx_data, t->length, false) ||
            (t->rx_data && !stm32_spi_board_dma_buffer_valid(t->rx_data, t->length, true))) {
            spi_publish(b, NX_ERR_DMA_CONFIG);
            goto settle;
        }
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
        /* Drain stale notifications before a new operation. */
        while (osal_sem_take(b->dma_sem, 0) == OSAL_OK) {}
#endif
        dma_started = true; /* Even a failed start may have armed one DMA stream. */
        uint32_t launch_saved = spi_critical_enter();
        HAL_StatusTypeDef status = HAL_OK;
        if (b->phase == STM32_SPI_WAITING) status = t->rx_data ?
            HAL_SPI_TransmitReceive_DMA(&b->hspi, (uint8_t*)t->tx_data,
                                        t->rx_data, (uint16_t)t->length) :
            HAL_SPI_Transmit_DMA(&b->hspi, (uint8_t*)t->tx_data, (uint16_t)t->length);
        if (status != HAL_OK) spi_publish(b, spi_hal_result(status));
        spi_critical_leave(launch_saved);
        while (b->phase == STM32_SPI_WAITING) {
            remaining = spi_remaining(started_at, t->timeout_ms);
            if (!remaining) { spi_publish(b, NX_ERR_TIMEOUT); break; }
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
            osal_status_t wait = osal_sem_take(b->dma_sem, remaining);
            if (wait != OSAL_OK) {
                spi_publish(b, wait == OSAL_ERROR_TIMEOUT ? NX_ERR_TIMEOUT : NX_ERR_IO);
            }
#else
            /* HAL tick and completion IRQs remain enabled while polling. */
            __NOP();
#endif
        }
    } else {
        HAL_StatusTypeDef status = t->rx_data ?
            HAL_SPI_TransmitReceive(&b->hspi, (uint8_t*)t->tx_data, t->rx_data,
                                    (uint16_t)t->length, remaining) :
            HAL_SPI_Transmit(&b->hspi, (uint8_t*)t->tx_data,
                              (uint16_t)t->length, remaining);
        spi_publish(b, spi_hal_result(status));
    }
settle:
    result = b->result;
    if (dma_started) {
        nx_status_t drain = spi_dma_drain(b);
        if (drain != NX_OK) { b->state->fault = true; result = drain; }
    } else if (result != NX_OK) {
        /* Polling timeout may leave peripheral state incomplete. */
        if (HAL_SPI_Abort(&b->hspi) != HAL_OK) {
            b->state->fault = true;
            result = NX_ERR_HARDWARE;
        }
    }
done:
    if (selected) {
        nx_status_t cs = stm32_spi_board_select(b->state->instance, d->config.cs_pin, false);
        if (cs != NX_OK) { b->state->fault = true; result = cs; }
    }
    if (locked) {
        uint32_t saved = spi_critical_enter();
        b->active = NULL;
        b->phase = STM32_SPI_IDLE;
        b->dma_active = false;
        b->state->busy = false;
        spi_critical_leave(saved);
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
        if (osal_mutex_unlock(b->mutex) != OSAL_OK) {
            b->state->fault = true;
            result = NX_ERR_IO;
        }
#endif
    }
    {
        uint32_t saved = spi_critical_enter();
        if (worker) d->completing = true;
        d->cancelled = false;
        spi_critical_leave(saved);
    }
    bool notify = !worker && t->callback;
    if (notify) {
        uint32_t saved = spi_critical_enter();
        ++b->state->users; /* Device can close; bus lifecycle stays pinned. */
        spi_critical_leave(saved);
    }
    relinquish(d);
    /* Worker publishes its terminal notification after clearing pending state. */
    if (notify) {
        t->callback(t->user_data, result);
        uint32_t saved = spi_critical_enter();
        --b->state->users;
        spi_critical_leave(saved);
    }
    return result;
}

static nx_status_t send(nx_tx_sync_t* self, const uint8_t* data, size_t len,
                        uint32_t timeout) {
    if (!self) return NX_ERR_INVALID_PARAM;
    nx_spi_transaction_t t = {data, NULL, len, timeout, NULL, NULL};
    return spi_transfer(NX_CONTAINER_OF(self, stm32_spi_device_t, tx_sync),
                        &t, HAL_GetTick(), false, NX_CONTAINER_OF(self, stm32_spi_device_t, tx_sync)->base.token);
}
static nx_status_t transceive(nx_tx_rx_sync_t* self, const uint8_t* tx, size_t len,
                               uint8_t* rx, size_t* rx_len, uint32_t timeout) {
    if (!self || !rx || !rx_len) return NX_ERR_INVALID_PARAM;
    size_t capacity = *rx_len;
    *rx_len = 0;
    if (capacity < len) return NX_ERR_INVALID_SIZE;
    nx_spi_transaction_t t = {tx, rx, len, timeout, NULL, NULL};
    nx_status_t r = spi_transfer(NX_CONTAINER_OF(self, stm32_spi_device_t, tx_rx_sync),
                                 &t, HAL_GetTick(), false, NX_CONTAINER_OF(self, stm32_spi_device_t, tx_rx_sync)->base.token);
    if (r == NX_OK) *rx_len = len;
    return r;
}
void spi_init_tx_sync(nx_tx_sync_t* iface) { NX_INIT_TX_SYNC(iface, send); }
void spi_init_tx_rx_sync(nx_tx_rx_sync_t* iface) { NX_INIT_TX_RX_SYNC(iface, transceive); }
