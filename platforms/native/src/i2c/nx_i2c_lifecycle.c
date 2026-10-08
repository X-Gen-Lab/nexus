/** Lifecycle transitions reject queued, waiting, active and callback owners. */
#include "nx_i2c_helpers.h"
#include "osal/osal.h"
#include <string.h>
static void clear(nx_i2c_impl_t* b) {
    i2c_buffer_clear(&b->state->tx_buf); i2c_buffer_clear(&b->state->rx_buf);
    memset(b->responses,0,sizeof(b->responses));
    for(unsigned i=0;i<NATIVE_I2C_DEVICE_CAPACITY;++i) b->devices[i].allocated=false;
    b->state->initialized=false; b->state->suspended=false; b->state->busy=false;
    b->pending=NULL; b->active=NULL; b->next_failure=NX_OK;
}
static nx_status_t init(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,lifecycle);
    native_i2c_lock();
    nx_status_t r=b->state->initialized ? NX_ERR_ALREADY_INIT :
        b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK && !b->mutex && osal_mutex_create(&b->mutex)!=OSAL_OK) r=NX_ERR_NO_RESOURCE;
    if(r==NX_OK) { clear(b); b->state->initialized=true; }
    native_i2c_unlock(); return r;
}
static nx_status_t deinit(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,lifecycle);
    native_i2c_lock(); nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT :
        b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK && b->mutex) {
        if(osal_mutex_delete(b->mutex)!=OSAL_OK) r=NX_ERR_IO;
        else b->mutex=NULL;
    }
    if(r==NX_OK) clear(b);
    native_i2c_unlock(); return r;
}
static nx_status_t suspend(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,lifecycle);
    native_i2c_lock(); nx_status_t r=native_i2c_bus_status(b);
    if(r==NX_OK && (b->users || b->worker_active)) r=NX_ERR_BUSY;
    if(r==NX_OK) b->state->suspended=true;
    native_i2c_unlock(); return r;
}
static nx_status_t resume(nx_lifecycle_t* self) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,lifecycle);
    native_i2c_lock(); nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT :
        !b->state->suspended ? NX_ERR_INVALID_STATE : NX_OK;
    if(r==NX_OK) b->state->suspended=false;
    native_i2c_unlock(); return r;
}
static nx_device_state_t state(nx_lifecycle_t* self) {
    if(!self) return NX_DEV_STATE_ERROR;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,lifecycle);
    native_i2c_lock(); nx_device_state_t r=!b->state->initialized ? NX_DEV_STATE_UNINITIALIZED :
        b->state->suspended ? NX_DEV_STATE_SUSPENDED : NX_DEV_STATE_RUNNING;
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_reset_impl(nx_i2c_impl_t* b) {
    if(!b || !b->state || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    native_i2c_lock(); nx_status_t r=b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK && b->mutex) {
        if(osal_mutex_delete(b->mutex)!=OSAL_OK) r=NX_ERR_IO;
        else b->mutex=NULL;
    }
    if(r==NX_OK) {
        clear(b); memset(&b->state->stats,0,sizeof(b->state->stats));
        memset(&b->state->current_device,0,sizeof(b->state->current_device));
        b->state->current_dev_addr=0; b->transfer_delay_ms=0;
        for(unsigned i=0;i<NATIVE_I2C_LEGACY_CAPACITY;++i) b->legacy_devices[i].last_result=NX_OK;
    }
    native_i2c_unlock(); return r;
}
void i2c_init_lifecycle(nx_lifecycle_t* out) {
    out->init=init; out->deinit=deinit; out->suspend=suspend; out->resume=resume; out->get_state=state;
}
