#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
/** Native SPI uses the same bounded bus/device/transaction contract as STM32.
 * This host backend captures transmitted bytes and echoes uninjected RX data;
 * it does not validate electrical timing, DMA or a vendor peripheral. */
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "nexus_config.h"
#include "nx_spi_helpers.h"
#include "osal/osal.h"
#include <stdatomic.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
#define DEVICE_TYPE NX_SPI
static atomic_flag metadata = ATOMIC_FLAG_INIT;
void native_spi_lock(void) {
    while (atomic_flag_test_and_set_explicit(&metadata, memory_order_acquire)) {}
}
void native_spi_unlock(void) {
    atomic_flag_clear_explicit(&metadata, memory_order_release);
}
uint32_t native_spi_now(void) {
#ifdef _WIN32
    return (uint32_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}
static uint32_t remaining(uint32_t at, uint32_t budget) {
    uint32_t elapsed = native_spi_now() - at;
    return budget == UINT32_MAX ? UINT32_MAX : elapsed >= budget ? 0 : budget-elapsed;
}
static bool config_valid(const nx_spi_device_config_t* c, nx_spi_impl_t* b) {
    return c && c->speed && c->speed <= b->state->config.max_speed &&
           c->mode <= NX_SPI_MODE_3 && c->bit_order <= NX_SPI_BIT_ORDER_LSB;
}
static bool equal(const nx_spi_device_config_t* a, const nx_spi_device_config_t* b) {
    return a->cs_pin==b->cs_pin && a->speed==b->speed && a->mode==b->mode && a->bit_order==b->bit_order;
}
/* Call only under metadata guard. Caller-owned handles never alias pool slots. */
static native_spi_device_t* resolve(nx_spi_impl_t* b, uint64_t token) {
    if (!token) return NULL;
    for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i)
        if(b->devices[i].allocated && b->devices[i].base.token==token) return &b->devices[i];
    return NULL;
}
static nx_status_t status(nx_spi_impl_t* b) {
    return !b->state->initialized ? NX_ERR_NOT_INIT : b->state->suspended ? NX_ERR_SUSPENDED : NX_OK;
}
static bool valid_transaction(const nx_spi_transaction_t* t, bool async) {
    return t && t->tx_data && t->length && t->length<=UINT16_MAX &&
        (t->timeout_ms<=INT32_MAX || (!async && t->timeout_ms==UINT32_MAX)) &&
        (!async || (t->callback && t->timeout_ms));
}
static nx_status_t transfer(nx_spi_impl_t* b, uint64_t token,
    const nx_spi_transaction_t* t, uint32_t at, bool worker) {
    if(!valid_transaction(t,false)) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    native_spi_lock();
    native_spi_device_t* d=resolve(b,token);
    nx_status_t r=d ? status(b) : NX_ERR_INVALID_STATE;
    if(r==NX_OK && (d->users || d->pending || (d->servicing && !worker))) r=NX_ERR_BUSY;
    if(r!=NX_OK) { native_spi_unlock(); return r; }
    ++b->users; ++d->users; d->completing=false;
    native_spi_unlock();
    bool locked=false;
    uint32_t left=remaining(at,t->timeout_ms);
    if(!left) { r=NX_ERR_TIMEOUT; goto finish; }
    /* Poll at bounded intervals so cancellation of a lock waiter is observable.
     * Every iteration consumes the original total deadline. */
    for(;;) {
        native_spi_lock(); bool cancelled=d->cancelled; native_spi_unlock();
        if(cancelled) { r=NX_ERR_CANCELLED; goto finish; }
        left=remaining(at,t->timeout_ms);
        if(!left) { r=NX_ERR_TIMEOUT; goto finish; }
        osal_status_t os=osal_mutex_lock(b->mutex,left==UINT32_MAX || left>2 ? 2 : left);
        if(os==OSAL_OK) { locked=true; break; }
        if(os!=OSAL_ERROR_TIMEOUT) { r=NX_ERR_IO; goto finish; }
    }
    native_spi_lock(); b->active=d; b->state->busy=true;
    b->state->current_device.config=d->config; /* Last executed, informational. */
    b->state->current_device.in_use=true;
    uint32_t delay=b->transfer_delay_ms; native_spi_unlock();
    uint32_t io_at=native_spi_now();
    for(;;) {
        native_spi_lock(); bool cancelled=d->cancelled; native_spi_unlock();
        if(cancelled) { r=NX_ERR_CANCELLED; goto finish; }
        if(!remaining(at,t->timeout_ms)) { r=NX_ERR_TIMEOUT; goto finish; }
        if(native_spi_now()-io_at>=delay) break;
        if(osal_task_delay(1)!=OSAL_OK) { r=NX_ERR_IO; goto finish; }
    }
    native_spi_lock();
    if(d->cancelled) r=NX_ERR_CANCELLED;
    else if(!remaining(at,t->timeout_ms)) r=NX_ERR_TIMEOUT;
    else if(t->length>b->state->tx_buf.size-b->state->tx_buf.count) r=NX_ERR_FULL;
    else {
        spi_buffer_write(&b->state->tx_buf,t->tx_data,t->length);
        b->state->stats.tx_count+=(uint32_t)t->length;
        if(b->trace_count<NATIVE_SPI_TRACE_CAPACITY)
            b->trace[b->trace_count++]=(native_spi_trace_t){d->config,d->base.token,t->tx_data[0]};
        if(t->rx_data) {
            if(b->state->rx_buf.count>=t->length)
                spi_buffer_read(&b->state->rx_buf,t->rx_data,t->length);
            else { memmove(t->rx_data,t->tx_data,t->length); b->state->stats.rx_count+=(uint32_t)t->length; }
        }
        r=NX_OK;
    }
    d->completing=true; /* No accepted cancellation after the last buffer access. */
    native_spi_unlock();
finish:
    if(locked) {
        native_spi_lock(); b->active=NULL; b->state->busy=false; native_spi_unlock();
        if(osal_mutex_unlock(b->mutex)!=OSAL_OK) r=NX_ERR_IO;
    }
    native_spi_lock();
    bool notify=!worker && t->callback;
    if(notify) ++b->users; /* Bus lifecycle stays pinned through callback. */
    --b->users; --d->users; d->cancelled=false; d->completing=true;
    d->last_result=r;
    if(r!=NX_OK) ++b->state->stats.error_count;
    native_spi_unlock();
    /* No use of d after notification: callback can close and reuse its slot. */
    if(notify) {
        t->callback(t->user_data,r);
        native_spi_lock(); --b->users; native_spi_unlock();
    }
    return r;
}
static nx_status_t submit_locked(nx_spi_impl_t* b, native_spi_device_t* d,
                                  const nx_spi_transaction_t* t, uint32_t at) {
    nx_status_t r=d ? status(b) : NX_ERR_INVALID_STATE;
    if(r==NX_OK && (d->users || d->pending || d->servicing)) r=NX_ERR_BUSY;
    if(r==NX_OK && b->next_sequence==UINT64_MAX) r=NX_ERR_NO_RESOURCE;
    if(r==NX_OK) {
        d->queued=*t; d->queued_at=at; d->sequence=++b->next_sequence;
        d->pending=true; d->cancelled=false; d->completing=false; d->last_result=NX_ERR_BUSY;
    }
    return r;
}
static nx_status_t device_transfer(nx_spi_device_t* self,const nx_spi_transaction_t* t) {
    if(!self || !self->owner) return NX_ERR_INVALID_PARAM;
    return transfer(spi_get_impl(self->owner),self->token,t,native_spi_now(),false);
}
static nx_status_t device_submit(nx_spi_device_t* self,const nx_spi_transaction_t* t) {
    uint32_t at=native_spi_now();
    if(!self || !self->owner || !valid_transaction(t,true)) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_spi_impl_t* b=spi_get_impl(self->owner);
    native_spi_lock(); nx_status_t r=submit_locked(b,resolve(b,self->token),t,at); native_spi_unlock();
    return r;
}
static nx_status_t device_cancel(nx_spi_device_t* self) {
    if(!self || !self->owner) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=spi_get_impl(self->owner);
    native_spi_lock(); native_spi_device_t* d=resolve(b,self->token);
    nx_status_t r=!d ? NX_ERR_INVALID_STATE :
        d->pending || ((d->users || d->servicing) && !d->completing) ? NX_OK : NX_ERR_NOT_FOUND;
    if(r==NX_OK) d->cancelled=true;
    native_spi_unlock(); return r;
}
static nx_status_t service(nx_spi_bus_t* self) {
    if(!self) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_spi_impl_t* b=spi_get_impl(self);
    native_spi_lock();
    if(b->worker_active) { native_spi_unlock(); return NX_ERR_BUSY; }
    native_spi_device_t* d=NULL;
    for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i)
        if(b->devices[i].pending && (!d || b->devices[i].sequence<d->sequence)) d=&b->devices[i];
    if(!d) { native_spi_unlock(); return NX_ERR_NO_DATA; }
    b->worker_active=true; d->pending=false; d->servicing=true;
    bool cancelled=d->cancelled; nx_spi_transaction_t t=d->queued;
    uint64_t token=d->base.token; uint32_t at=d->queued_at;
    native_spi_unlock();
    nx_status_t r=cancelled ? NX_ERR_CANCELLED : transfer(b,token,&t,at,true);
    native_spi_lock(); d->last_result=r; d->servicing=false; d->completing=true; d->cancelled=false; native_spi_unlock();
    t.callback(t.user_data,r);
    native_spi_lock(); b->worker_active=false; native_spi_unlock();
    return r;
}
static void legacy_terminal(void* context,nx_status_t r) {
    native_spi_device_t* d=context;
    if(r==NX_OK && d->receive_callback) d->receive_callback(d->receive_context,d->rx_copy,d->queued.length);
}
static nx_status_t legacy_submit(native_spi_device_t* d,const uint8_t* data,size_t n,uint32_t timeout,bool rx) {
    uint32_t at=native_spi_now();
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    if(!data || !n) return NX_ERR_INVALID_PARAM;
    if(n>NATIVE_SPI_ASYNC_CAPACITY) return NX_ERR_INVALID_SIZE;
    if(!timeout || timeout>INT32_MAX) return NX_ERR_INVALID_PARAM;
    native_spi_lock();
    nx_status_t r=status(d->bus);
    if(r==NX_OK && (d->users || d->pending || d->servicing)) r=NX_ERR_BUSY;
    if(r==NX_OK) {
        memcpy(d->tx_copy,data,n);
        nx_spi_transaction_t t={d->tx_copy,rx ? d->rx_copy : NULL,n,timeout,legacy_terminal,d};
        r=submit_locked(d->bus,d,&t,at);
    }
    native_spi_unlock(); return r;
}
static nx_status_t send(nx_tx_sync_t* self,const uint8_t* data,size_t n,uint32_t timeout) {
    if(!self) return NX_ERR_INVALID_PARAM;
    native_spi_device_t* d=NX_CONTAINER_OF(self,native_spi_device_t,tx_sync);
    nx_spi_transaction_t t={data,NULL,n,timeout,NULL,NULL};
    return transfer(d->bus,d->base.token,&t,native_spi_now(),false);
}
static nx_status_t tx_rx(nx_tx_rx_sync_t* self,const uint8_t* tx,size_t n,uint8_t* rx,size_t* rx_n,uint32_t timeout) {
    if(!self || !rx || !rx_n) return NX_ERR_INVALID_PARAM;
    size_t cap=*rx_n; *rx_n=0;
    if(cap<n) return NX_ERR_INVALID_SIZE;
    native_spi_device_t* d=NX_CONTAINER_OF(self,native_spi_device_t,tx_rx_sync);
    nx_spi_transaction_t t={tx,rx,n,timeout,NULL,NULL};
    nx_status_t r=transfer(d->bus,d->base.token,&t,native_spi_now(),false);
    if(r==NX_OK) *rx_n=n;
    return r;
}
static nx_status_t async_send(nx_tx_async_t* self,const uint8_t* data,size_t n) {
    return self ? legacy_submit(NX_CONTAINER_OF(self,native_spi_device_t,tx_async),data,n,1000,false) : NX_ERR_INVALID_PARAM;
}
static nx_status_t async_rx(nx_tx_rx_async_t* self,const uint8_t* data,size_t n,uint32_t timeout) {
    return self ? legacy_submit(NX_CONTAINER_OF(self,native_spi_device_t,tx_rx_async),data,n,timeout,true) : NX_ERR_INVALID_PARAM;
}
static nx_status_t get_state(native_spi_device_t* d) {
    native_spi_lock(); nx_status_t r=status(d->bus);
    if(r==NX_OK) r=d->users || d->pending || d->servicing ? NX_ERR_BUSY : d->last_result;
    native_spi_unlock(); return r;
}
static nx_status_t async_state(nx_tx_async_t* self) { return self ? get_state(NX_CONTAINER_OF(self,native_spi_device_t,tx_async)) : NX_ERR_INVALID_PARAM; }
static nx_status_t async_rx_state(nx_tx_rx_async_t* self) { return self ? get_state(NX_CONTAINER_OF(self,native_spi_device_t,tx_rx_async)) : NX_ERR_INVALID_PARAM; }
static native_spi_device_t* allocate(nx_spi_impl_t* b,nx_spi_device_config_t config,bool legacy,nx_comm_callback_t cb,void* ctx) {
    native_spi_device_t* d=NULL;
    for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY;++i) {
        native_spi_device_t* slot=&b->devices[i];
        if(!slot->allocated) { if(!d) d=slot; }
        else if(legacy && slot->legacy && equal(&slot->config,&config) && slot->receive_callback==cb && slot->receive_context==ctx) return slot;
    }
    if(!d || b->next_token==UINT64_MAX) return NULL;
    memset(d,0,sizeof(*d)); d->bus=b; d->allocated=true; d->legacy=legacy;
    d->config=config; d->receive_callback=cb; d->receive_context=ctx;
    d->base=(nx_spi_device_t){&b->base,++b->next_token,device_transfer,device_submit,device_cancel};
    NX_INIT_TX_SYNC(&d->tx_sync,send); NX_INIT_TX_RX_SYNC(&d->tx_rx_sync,tx_rx);
    NX_INIT_TX_ASYNC(&d->tx_async,async_send,async_state);
    NX_INIT_TX_RX_ASYNC(&d->tx_rx_async,async_rx,async_rx_state);
    return d;
}
static nx_status_t open_device(nx_spi_bus_t* self,const nx_spi_device_config_t* config,nx_spi_device_t* out) {
    if(out) memset(out,0,sizeof(*out));
    if(!self || !out || !config_valid(config,spi_get_impl(self))) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    native_spi_lock(); native_spi_device_t* d=allocate(spi_get_impl(self),*config,false,NULL,NULL);
    if(d) *out=d->base;
    native_spi_unlock(); return d ? NX_OK : NX_ERR_NO_RESOURCE;
}
static nx_status_t close_device(nx_spi_bus_t* self,nx_spi_device_t* value) {
    if(!self || !value) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    native_spi_lock(); native_spi_device_t* d=value->owner==self ? resolve(spi_get_impl(self),value->token) : NULL;
    nx_status_t r=!d ? NX_ERR_INVALID_STATE : d->legacy ? NX_ERR_NOT_SUPPORTED : d->users || d->pending || d->servicing ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK) d->allocated=false;
    native_spi_unlock(); return r;
}
static native_spi_device_t* legacy(nx_spi_bus_t* self,nx_spi_device_config_t config,nx_comm_callback_t cb,void* ctx) {
    if(!self || !config_valid(&config,spi_get_impl(self)) || osal_is_isr()) return NULL;
    native_spi_lock(); native_spi_device_t* d=allocate(spi_get_impl(self),config,true,cb,ctx); native_spi_unlock(); return d;
}
static nx_tx_sync_t* get_tx_sync(nx_spi_bus_t* self,nx_spi_device_config_t config) { native_spi_device_t* d=legacy(self,config,NULL,NULL); return d ? &d->tx_sync : NULL; }
static nx_tx_rx_sync_t* get_tx_rx_sync(nx_spi_bus_t* self,nx_spi_device_config_t config) { native_spi_device_t* d=legacy(self,config,NULL,NULL); return d ? &d->tx_rx_sync : NULL; }
static nx_tx_async_t* get_tx_async(nx_spi_bus_t* self,nx_spi_device_config_t config) { native_spi_device_t* d=legacy(self,config,NULL,NULL); return d ? &d->tx_async : NULL; }
static nx_tx_rx_async_t* get_tx_rx_async(nx_spi_bus_t* self,nx_spi_device_config_t config,nx_comm_callback_t cb,void* ctx) { native_spi_device_t* d=legacy(self,config,cb,ctx); return d ? &d->tx_rx_async : NULL; }
static nx_lifecycle_t* get_lifecycle(nx_spi_bus_t* self) { return self ? &spi_get_impl(self)->lifecycle : NULL; }
static nx_power_t* get_power(nx_spi_bus_t* self) { return self ? &spi_get_impl(self)->power : NULL; }
typedef struct {
    nx_device_config_state_t core;
    nx_spi_impl_t bus;
    nx_spi_state_t state;
    uint8_t* tx;
    uint8_t* rx;
    size_t tx_size, rx_size;
} native_spi_storage_t;
static nx_status_t nx_spi_construct(const nx_device_t* dev, void** api) {
    if (!api)
        return NX_ERR_NULL_PTR;
    *api=NULL;
    if (!dev || !dev->state)
        return NX_ERR_INVALID_PARAM;
    const nx_spi_platform_config_t* cfg=dev->config;
    native_spi_storage_t* storage=NX_CONTAINER_OF(dev->state,native_spi_storage_t,core);
    if(!cfg || !cfg->tx_buf_size || !cfg->rx_buf_size || !storage->tx || !storage->rx ||
       storage->tx_size!=cfg->tx_buf_size || storage->rx_size!=cfg->rx_buf_size)
        return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=&storage->bus;
    memset(b,0,sizeof(*b)); b->state=&storage->state;
    memset(b->state,0,sizeof(*b->state)); b->state->index=cfg->spi_index;
    b->state->config=(nx_spi_config_t){cfg->max_speed,cfg->mosi_pin,cfg->miso_pin,cfg->sck_pin,false,false,cfg->tx_buf_size,cfg->rx_buf_size};
    b->state->tx_buf.data=storage->tx; b->state->tx_buf.size=cfg->tx_buf_size;
    b->state->rx_buf.data=storage->rx; b->state->rx_buf.size=cfg->rx_buf_size;
    NX_INIT_SPI_BUS(&b->base,get_tx_async,get_tx_rx_async,get_tx_sync,get_tx_rx_sync,get_lifecycle,get_power);
    b->base.open_device=open_device; b->base.close_device=close_device; b->base.service=service;
    spi_init_lifecycle(&b->lifecycle); spi_init_power(&b->power);
    *api=&b->base;
    return NX_OK;
}
#define NX_SPI_CONFIG(index)                                                   \
    static const nx_spi_platform_config_t spi_config_##index = {               \
        .spi_index = index,                                                    \
        .max_speed = NX_CONFIG_SPI##index##_MAX_SPEED,                         \
        .mosi_pin = 1,                                                         \
        .miso_pin = 2,                                                         \
        .sck_pin = 3,                                                          \
        .tx_buf_size = NX_CONFIG_SPI##index##_TX_BUFFER_SIZE,                  \
        .rx_buf_size = NX_CONFIG_SPI##index##_RX_BUFFER_SIZE,                  \
    }

/**
 * \brief           Device registration macro
 */
#define NX_SPI_DEVICE_REGISTER(index)                                          \
    NX_SPI_CONFIG(index);                                                      \
    static uint8_t spi_tx_##index[NX_CONFIG_SPI##index##_TX_BUFFER_SIZE];       \
    static uint8_t spi_rx_##index[NX_CONFIG_SPI##index##_RX_BUFFER_SIZE];       \
    static native_spi_storage_t spi_storage_##index = {                        \
        .tx = spi_tx_##index, .rx = spi_rx_##index,                             \
        .tx_size = sizeof(spi_tx_##index), .rx_size = sizeof(spi_rx_##index),    \
    };                                                                        \
    NX_DEVICE_REGISTER_TYPED(DEVICE_TYPE, index, "SPI" #index,                \
        &spi_config_##index, &spi_storage_##index.core, NX_DEVICE_CLASS_SPI,    \
        NX_DEVICE_CAP_SPI_DEVICES | NX_DEVICE_CAP_SPI_QUEUE |                  \
            NX_DEVICE_CAP_SPI_CANCEL,                                          \
        nx_spi_construct, NULL);

/**
 * \brief           Register all enabled SPI instances
 */
NX_TRAVERSE_EACH_INSTANCE(NX_SPI_DEVICE_REGISTER, DEVICE_TYPE)
