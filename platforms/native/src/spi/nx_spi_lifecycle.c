/** Lifecycle changes reject executing, queued, waiting and callback work. */
#include "nx_spi_helpers.h"
#include "osal/osal.h"
#include <string.h>
static bool occupied(nx_spi_impl_t* b) {
    if(b->users || b->worker_active) return true;
    for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i)
        if(b->devices[i].pending || b->devices[i].servicing) return true;
    return false;
}
static nx_status_t init(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,lifecycle);
    native_spi_lock();
    nx_status_t r=b->state->initialized ? NX_ERR_ALREADY_INIT :
        occupied(b) ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK && osal_mutex_create(&b->mutex)!=OSAL_OK) r=NX_ERR_NO_RESOURCE;
    if(r==NX_OK) {
        spi_buffer_clear(&b->state->tx_buf); spi_buffer_clear(&b->state->rx_buf);
        b->state->initialized=true; b->state->suspended=false; b->state->busy=false;
    }
    native_spi_unlock(); return r;
}
static nx_status_t deinit(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,lifecycle);
    native_spi_lock();
    nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT : occupied(b) ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK && osal_mutex_delete(b->mutex)!=OSAL_OK) r=NX_ERR_BUSY;
    if(r==NX_OK) {
        b->mutex=NULL; b->state->initialized=false; b->state->suspended=false;
        spi_buffer_clear(&b->state->tx_buf); spi_buffer_clear(&b->state->rx_buf);
        for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i)
            if(!b->devices[i].legacy) b->devices[i].allocated=false;
    }
    native_spi_unlock(); return r;
}
static nx_status_t suspend(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,lifecycle);
    native_spi_lock();
    nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT : occupied(b) ? NX_ERR_BUSY : b->state->suspended ? NX_ERR_INVALID_STATE : NX_OK;
    if(r==NX_OK) b->state->suspended=true;
    native_spi_unlock(); return r;
}
static nx_status_t resume(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,lifecycle);
    native_spi_lock();
    nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT : !b->state->suspended ? NX_ERR_INVALID_STATE : NX_OK;
    if(r==NX_OK) b->state->suspended=false;
    native_spi_unlock(); return r;
}
static nx_device_state_t state(nx_lifecycle_t* self) {
    if(!self) return NX_DEV_STATE_ERROR;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,lifecycle);
    native_spi_lock(); nx_device_state_t r=!b->state->initialized ? NX_DEV_STATE_UNINITIALIZED : b->state->suspended ? NX_DEV_STATE_SUSPENDED : NX_DEV_STATE_RUNNING;
    native_spi_unlock(); return r;
}
nx_status_t native_spi_reset_impl(nx_spi_impl_t* b) {
    if(!b) return NX_ERR_INVALID_PARAM;
    native_spi_lock();
    if(occupied(b)) { native_spi_unlock(); return NX_ERR_BUSY; }
    if(b->state->initialized && osal_mutex_delete(b->mutex)!=OSAL_OK) { native_spi_unlock(); return NX_ERR_BUSY; }
    b->mutex=NULL; b->state->initialized=false; b->state->suspended=false; b->state->busy=false;
    b->state->locked=false; b->transfer_delay_ms=0; b->trace_count=0;
    memset(&b->state->stats,0,sizeof(b->state->stats));
    memset(&b->state->current_device,0,sizeof(b->state->current_device));
    spi_buffer_clear(&b->state->tx_buf); spi_buffer_clear(&b->state->rx_buf);
    /* Test reset invalidates handles; token/sequence counters never reset. */
    for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i) b->devices[i].allocated=false;
    native_spi_unlock(); return NX_OK;
}
void spi_init_lifecycle(nx_lifecycle_t* self) {
    self->init=init; self->deinit=deinit; self->suspend=suspend; self->resume=resume; self->get_state=state;
}
