/** Serialized Native I2C transactions. No electrical/DMA emulation or echo. */
#include "nx_i2c_helpers.h"
#include "osal/osal.h"
#include <limits.h>
#include <string.h>

bool native_i2c_transaction_valid(const nx_i2c_transaction_t* t,bool async) {
    return t && (!t->tx_length || t->tx_data) &&
        (!t->rx_capacity || (t->rx_data && t->received_length)) &&
        (t->tx_length || t->rx_capacity) && t->tx_length<=UINT32_MAX &&
        (t->timeout_ms<=INT32_MAX || (!async && t->timeout_ms==UINT32_MAX)) &&
        (!async || (t->callback && t->timeout_ms));
}
nx_status_t native_i2c_bus_status(nx_i2c_impl_t* b) {
    return !b->state->initialized ? NX_ERR_NOT_INIT :
        b->state->suspended ? NX_ERR_SUSPENDED : NX_OK;
}
nx_status_t native_i2c_admit_locked(native_i2c_device_t* d) {
    nx_status_t r=native_i2c_bus_status(d->bus);
    if(r==NX_OK && d->users) r=NX_ERR_BUSY;
    if(r==NX_OK) {
        ++d->users; ++d->bus->users; d->cancelled=false;
        d->completing=false; d->last_result=NX_ERR_BUSY;
    }
    return r;
}
static uint32_t remaining(uint64_t at,uint32_t budget) {
    if(budget==UINT32_MAX) return UINT32_MAX;
    uint64_t elapsed=native_i2c_now()-at;
    return elapsed>=budget ? 0 : budget-(uint32_t)elapsed;
}
static native_i2c_response_t* response(nx_i2c_impl_t* b,uint8_t address) {
    for(unsigned i=0;i<NATIVE_I2C_RESPONSE_CAPACITY;++i)
        if(b->responses[i].used && b->responses[i].address==address)
            return &b->responses[i];
    return NULL;
}
static void finish(native_i2c_device_t* d,const nx_i2c_transaction_t* t,
                   nx_status_t result) {
    nx_i2c_impl_t* b=d->bus;
    native_i2c_lock();
    d->completing=true; d->last_result=result;
    if(result==NX_ERR_NACK) ++b->state->stats.nack_count;
    else if(result==NX_ERR_BUS || result==NX_ERR_IO || result==NX_ERR_ARBITRATION)
        ++b->state->stats.bus_error_count;
    native_i2c_unlock();
    /* Pins deliberately survive notification: close/deinit inside callback
     * returns BUSY, and a reused slot cannot change callback ownership. */
    if(t->callback) t->callback(t->user_data,result);
    native_i2c_lock();
    --d->users; --b->users; d->cancelled=false;
    native_i2c_unlock();
}
nx_status_t native_i2c_execute(native_i2c_device_t* d,
    const nx_i2c_transaction_t* t,uint64_t at,bool admitted) {
    if(!native_i2c_transaction_valid(t,false)) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=d->bus;
    nx_status_t r=NX_OK;
    if(!admitted) {
        native_i2c_lock(); r=native_i2c_admit_locked(d); native_i2c_unlock();
        if(r!=NX_OK) return r;
    }
    if(t->received_length) *t->received_length=0;
    bool locked=false;
    for(;;) {
        native_i2c_lock(); bool cancelled=d->cancelled; native_i2c_unlock();
        if(cancelled) { r=NX_ERR_CANCELLED; goto done; }
        uint32_t left=remaining(at,t->timeout_ms);
        if(!left) { r=NX_ERR_TIMEOUT; goto done; }
        osal_status_t os=osal_mutex_lock(b->mutex,left==UINT32_MAX || left>2 ? 2 : left);
        if(os==OSAL_OK) { locked=true; break; }
        if(os!=OSAL_ERROR_TIMEOUT) { r=NX_ERR_IO; goto done; }
    }
    native_i2c_lock();
    b->active=d; b->state->busy=true;
    b->state->current_dev_addr=d->address;
    b->state->current_device=(nx_i2c_device_handle_t){
        d->address,d->receive_callback,d->receive_context,true};
    uint32_t delay=b->transfer_delay_ms;
    nx_status_t failure=b->next_failure; b->next_failure=NX_OK;
    native_i2c_unlock();
    uint64_t io_at=native_i2c_now();
    for(;;) {
        native_i2c_lock();
        if(d->cancelled) r=NX_ERR_CANCELLED;
        else if(!remaining(at,t->timeout_ms)) r=NX_ERR_TIMEOUT;
        else if(native_i2c_now()-io_at>=delay) {
            native_i2c_response_t* packet=response(b,d->address);
            bool ready=!t->rx_capacity || packet || b->state->rx_buf.count;
            if(failure!=NX_OK) r=failure;
            else if(t->tx_length>b->state->tx_buf.size-b->state->tx_buf.count)
                r=NX_ERR_FULL;
            else if(ready) {
                if(t->tx_length) {
                    i2c_buffer_write(&b->state->tx_buf,t->tx_data,t->tx_length);
                    b->state->stats.tx_count+=(uint32_t)t->tx_length;
                }
                if(t->rx_capacity) {
                    size_t received;
                    if(packet) {
                        size_t available=packet->length-packet->offset;
                        received=available<t->rx_capacity ? available : t->rx_capacity;
                        memcpy(t->rx_data,packet->data+packet->offset,received);
                        packet->offset+=received;
                        if(packet->offset==packet->length) packet->used=false;
                    } else received=i2c_buffer_read(&b->state->rx_buf,t->rx_data,t->rx_capacity);
                    *t->received_length=received;
                    b->state->stats.rx_count+=(uint32_t)received;
                }
                r=NX_OK;
            } else { native_i2c_unlock(); goto wait; }
            d->completing=true;
            native_i2c_unlock();
            break;
        } else { native_i2c_unlock(); goto wait; }
        d->completing=true;
        native_i2c_unlock();
        break;
wait:
        if(osal_task_delay(1)!=OSAL_OK) { r=NX_ERR_IO; break; }
    }
done:
    native_i2c_lock();
    if(d->cancelled && !d->completing) r=NX_ERR_CANCELLED;
    d->completing=true;
    native_i2c_unlock();
    if(locked) {
        native_i2c_lock(); b->active=NULL; b->state->busy=false; native_i2c_unlock();
        if(osal_mutex_unlock(b->mutex)!=OSAL_OK) r=NX_ERR_IO;
    }
    finish(d,t,r);
    return r;
}
nx_status_t native_i2c_submit(native_i2c_device_t* d,
    const nx_i2c_transaction_t* t,uint64_t started) {
    nx_i2c_impl_t* b=d->bus;
    nx_status_t r=b->pending || b->worker_active ? NX_ERR_BUSY : native_i2c_admit_locked(d);
    if(r==NX_OK) {
        b->queued=*t; b->queued_at=started; b->pending=d;
    }
    return r;
}
