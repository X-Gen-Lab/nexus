/** Lifecycle transitions reject all queued, waiting and active operations. */
#include "stm32_spi.h"

static bool has_work(stm32_spi_impl_t* b) {
    if (b->state->users || b->state->busy || b->worker_active) return true;
    for (unsigned i = 0; i < STM32_SPI_MAX_DEVICES; ++i)
        if (b->devices[i].pending || b->devices[i].servicing) return true;
    return false;
}
static nx_status_t transition(stm32_spi_impl_t* b) {
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    uint32_t saved = spi_critical_enter();
    nx_status_t r = b->state->transitioning || has_work(b) ? NX_ERR_BUSY : NX_OK;
    if (r == NX_OK) b->state->transitioning = true;
    spi_critical_leave(saved);
    return r;
}
static void transition_done(stm32_spi_impl_t* b) {
    uint32_t saved = spi_critical_enter();
    b->state->transitioning = false;
    spi_critical_leave(saved);
}
static nx_status_t init(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle);
    nx_status_t r = transition(b);
    if (r != NX_OK) return r;
    if (b->state->initialized) { transition_done(b); return NX_ERR_ALREADY_INIT; }
    /* Device contract currently clocks bytes as a full-duplex master. */
    if (b->hspi.Init.Mode != SPI_MODE_MASTER ||
        b->hspi.Init.Direction != SPI_DIRECTION_2LINES ||
        b->hspi.Init.DataSize != SPI_DATASIZE_8BIT ||
        b->hspi.Init.NSS != SPI_NSS_SOFT ||
        b->hspi.Init.CRCCalculation != SPI_CRCCALCULATION_DISABLE) {
        transition_done(b);
        return NX_ERR_NOT_SUPPORTED;
    }
    r = spi_board_prepare(b);
    if (r != NX_OK) { transition_done(b); return r; }
    r = spi_hal_result(HAL_SPI_Init(&b->hspi));
    if (r != NX_OK) goto fail_board;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    if (osal_mutex_create(&b->mutex) != OSAL_OK) { r = NX_ERR_NO_RESOURCE; goto fail_hal; }
    if (osal_sem_create(0, 1, &b->dma_sem) != OSAL_OK) {
        (void)osal_mutex_delete(b->mutex);
        b->mutex = NULL;
        r = NX_ERR_NO_RESOURCE;
        goto fail_hal;
    }
#endif
    if (b->dma_tx_enabled || b->dma_rx_enabled) {
        r = spi_dma_init(b);
        if (r != NX_OK) goto fail_osal;
    }
    b->state->fault = false;
    spi_registry_add(b);
    if (b->state->fault) { r = NX_ERR_NO_RESOURCE; goto fail_osal; }
    b->state->initialized = true;
    b->state->suspended = false;
    transition_done(b);
    return NX_OK;
fail_osal:
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    (void)osal_sem_delete(b->dma_sem); b->dma_sem = NULL;
    (void)osal_mutex_delete(b->mutex); b->mutex = NULL;
fail_hal:
#endif
    (void)HAL_SPI_DeInit(&b->hspi);
fail_board:
    spi_board_release(b);
    transition_done(b);
    return r;
}
static nx_status_t deinit(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle);
    nx_status_t r = transition(b);
    if (r != NX_OK) return r;
    if (!b->state->initialized) { transition_done(b); return NX_OK; }
    if (b->dma_tx_enabled || b->dma_rx_enabled) {
        r = spi_dma_deinit(b);
        if (r != NX_OK) { b->state->fault = true; transition_done(b); return r; }
    }
    r = spi_hal_result(HAL_SPI_DeInit(&b->hspi));
    if (r != NX_OK) { b->state->fault = true; transition_done(b); return r; }
    spi_registry_remove(b);
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    if (osal_sem_delete(b->dma_sem) != OSAL_OK || osal_mutex_delete(b->mutex) != OSAL_OK) {
        b->state->fault = true;
        transition_done(b);
        return NX_ERR_IO;
    }
    b->dma_sem = NULL;
    b->mutex = NULL;
#endif
    spi_board_release(b);
    for (unsigned i = 0; i < STM32_SPI_MAX_DEVICES; ++i)
        if (!b->devices[i].legacy) b->devices[i].allocated = false;
    b->state->initialized = false;
    b->state->suspended = false;
    b->state->fault = false;
    transition_done(b);
    return NX_OK;
}
static nx_status_t suspend(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle);
    nx_status_t r = transition(b);
    if (r != NX_OK) return r;
    if (!b->state->initialized) r = NX_ERR_NOT_INIT;
    else if (b->state->fault) r = NX_ERR_HARDWARE;
    else { __HAL_SPI_DISABLE(&b->hspi); b->state->suspended = true; }
    transition_done(b);
    return r;
}
static nx_status_t resume(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle);
    nx_status_t r = transition(b);
    if (r != NX_OK) return r;
    if (!b->state->initialized) r = NX_ERR_NOT_INIT;
    else if (b->state->fault) r = NX_ERR_HARDWARE;
    else { __HAL_SPI_ENABLE(&b->hspi); b->state->suspended = false; }
    transition_done(b);
    return r;
}
static nx_device_state_t state(nx_lifecycle_t* self) {
    if (!self) return NX_DEV_STATE_ERROR;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, lifecycle);
    uint32_t saved = spi_critical_enter();
    nx_device_state_t r = b->state->fault ? NX_DEV_STATE_ERROR :
        !b->state->initialized ? NX_DEV_STATE_UNINITIALIZED :
        b->state->suspended ? NX_DEV_STATE_SUSPENDED : NX_DEV_STATE_RUNNING;
    spi_critical_leave(saved);
    return r;
}
void spi_init_lifecycle(nx_lifecycle_t* iface) {
    iface->init = init; iface->deinit = deinit;
    iface->suspend = suspend; iface->resume = resume; iface->get_state = state;
}
