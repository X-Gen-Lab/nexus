#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
/** Native I2C bus registry, bounded devices and caller-owned generations. */
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "nexus_config.h"
#include "nx_i2c_helpers.h"
#include "osal/osal.h"
#include <stdatomic.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
#define DEVICE_TYPE NX_I2C
static atomic_flag metadata=ATOMIC_FLAG_INIT;
void native_i2c_lock(void) {
    while(atomic_flag_test_and_set_explicit(&metadata,memory_order_acquire)) {}
}
void native_i2c_unlock(void) { atomic_flag_clear_explicit(&metadata,memory_order_release); }
uint64_t native_i2c_now(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0) return 0;
    return (uint64_t)ts.tv_sec*1000u+(uint64_t)ts.tv_nsec/1000000u;
#endif
}
extern void i2c_init_tx_sync(nx_tx_sync_t*);
extern void i2c_init_tx_rx_sync(nx_tx_rx_sync_t*);
extern void i2c_init_tx_async(nx_tx_async_t*);
extern void i2c_init_tx_rx_async(nx_tx_rx_async_t*);
static native_i2c_device_t* resolve(nx_i2c_impl_t* b,uint64_t token) {
    if(!token) return NULL;
    for(unsigned i=0;i<NATIVE_I2C_DEVICE_CAPACITY;++i)
        if(b->devices[i].allocated && b->devices[i].base.token==token) return &b->devices[i];
    return NULL;
}
static nx_status_t transfer(nx_i2c_device_t* self,const nx_i2c_transaction_t* t) {
    uint64_t at=native_i2c_now();
    if(!self || !self->owner || !native_i2c_transaction_valid(t,false)) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=i2c_get_impl(self->owner);
    native_i2c_lock(); native_i2c_device_t* d=resolve(b,self->token);
    nx_status_t r=d ? native_i2c_admit_locked(d) : NX_ERR_INVALID_STATE;
    native_i2c_unlock();
    return r==NX_OK ? native_i2c_execute(d,t,at,true) : r;
}
static nx_status_t submit(nx_i2c_device_t* self,const nx_i2c_transaction_t* t) {
    uint64_t at=native_i2c_now();
    if(!self || !self->owner || !native_i2c_transaction_valid(t,true)) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=i2c_get_impl(self->owner);
    native_i2c_lock(); native_i2c_device_t* d=resolve(b,self->token);
    nx_status_t r=d ? native_i2c_submit(d,t,at) : NX_ERR_INVALID_STATE;
    native_i2c_unlock(); return r;
}
static nx_status_t cancel(nx_i2c_device_t* self) {
    if(!self || !self->owner) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    native_i2c_lock(); native_i2c_device_t* d=resolve(i2c_get_impl(self->owner),self->token);
    nx_status_t r=!d ? NX_ERR_INVALID_STATE : d->users && !d->completing ? NX_OK : NX_ERR_NOT_FOUND;
    if(r==NX_OK) d->cancelled=true;
    native_i2c_unlock(); return r;
}
static void initialize(native_i2c_device_t* d,nx_i2c_impl_t* b,uint8_t addr,bool legacy,
                       nx_comm_callback_t cb,void* context) {
    memset(d,0,sizeof(*d)); d->bus=b; d->address=addr;
    d->allocated=true; d->legacy=legacy;
    d->receive_callback=cb; d->receive_context=context;
    d->base=(nx_i2c_device_t){&b->base,++b->next_token,transfer,submit,cancel};
    i2c_init_tx_sync(&d->tx_sync); i2c_init_tx_rx_sync(&d->tx_rx_sync);
    i2c_init_tx_async(&d->tx_async); i2c_init_tx_rx_async(&d->tx_rx_async);
}
static nx_status_t open_device(nx_i2c_bus_t* self,uint8_t addr,nx_i2c_device_t* out) {
    if(out) memset(out,0,sizeof(*out));
    if(!self || !out || addr>0x7f) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=i2c_get_impl(self);
    native_i2c_lock(); nx_status_t r=native_i2c_bus_status(b);
    native_i2c_device_t* d=NULL;
    if(r==NX_OK) {
        for(unsigned i=0;i<NATIVE_I2C_DEVICE_CAPACITY;++i)
            if(!b->devices[i].allocated) { d=&b->devices[i]; break; }
        if(!d || b->next_token==UINT64_MAX) r=NX_ERR_NO_RESOURCE;
        else { initialize(d,b,addr,false,NULL,NULL); *out=d->base; }
    }
    native_i2c_unlock(); return r;
}
static nx_status_t close_device(nx_i2c_bus_t* self,nx_i2c_device_t* value) {
    if(!self || !value) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=i2c_get_impl(self);
    native_i2c_lock(); native_i2c_device_t* d=value->owner==self ? resolve(b,value->token) : NULL;
    nx_status_t r=!d ? NX_ERR_INVALID_STATE : d->users ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK) d->allocated=false;
    native_i2c_unlock(); return r;
}
static nx_status_t service(nx_i2c_bus_t* self) {
    if(!self) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=i2c_get_impl(self);
    native_i2c_lock();
    if(b->worker_active) { native_i2c_unlock(); return NX_ERR_BUSY; }
    native_i2c_device_t* d=b->pending;
    if(!d) { native_i2c_unlock(); return NX_ERR_NO_DATA; }
    b->pending=NULL; b->worker_active=true;
    nx_i2c_transaction_t t=b->queued; uint64_t at=b->queued_at;
    native_i2c_unlock();
    nx_status_t r=native_i2c_execute(d,&t,at,true);
    native_i2c_lock(); b->worker_active=false; native_i2c_unlock();
    return r;
}
static native_i2c_device_t* legacy(nx_i2c_bus_t* self,uint8_t addr,
                                  nx_comm_callback_t cb,void* context) {
    if(!self || addr>0x7f || osal_is_isr()) return NULL;
    nx_i2c_impl_t* b=i2c_get_impl(self);
    native_i2c_lock(); native_i2c_device_t* empty=NULL;
    for(unsigned i=0;i<NATIVE_I2C_LEGACY_CAPACITY;++i) {
        native_i2c_device_t* d=&b->legacy_devices[i];
        if(d->allocated && d->address==addr && d->receive_callback==cb && d->receive_context==context) {
            native_i2c_unlock(); return d;
        }
        if(!d->allocated && !empty) empty=d;
    }
    if(empty && b->next_token!=UINT64_MAX) initialize(empty,b,addr,true,cb,context);
    else empty=NULL;
    native_i2c_unlock(); return empty;
}
static nx_tx_sync_t* get_tx_sync(nx_i2c_bus_t* b,uint8_t addr) {
    native_i2c_device_t* d=legacy(b,addr,NULL,NULL); return d ? &d->tx_sync : NULL;
}
static nx_tx_rx_sync_t* get_tx_rx_sync(nx_i2c_bus_t* b,uint8_t addr) {
    native_i2c_device_t* d=legacy(b,addr,NULL,NULL); return d ? &d->tx_rx_sync : NULL;
}
static nx_tx_async_t* get_tx_async(nx_i2c_bus_t* b,uint8_t addr) {
    native_i2c_device_t* d=legacy(b,addr,NULL,NULL); return d ? &d->tx_async : NULL;
}
static nx_tx_rx_async_t* get_tx_rx_async(nx_i2c_bus_t* b,uint8_t addr,nx_comm_callback_t cb,void* ctx) {
    native_i2c_device_t* d=legacy(b,addr,cb,ctx); return d ? &d->tx_rx_async : NULL;
}
static nx_lifecycle_t* get_lifecycle(nx_i2c_bus_t* b) { return b ? &i2c_get_impl(b)->lifecycle : NULL; }
static nx_power_t* get_power(nx_i2c_bus_t* b) { return b ? &i2c_get_impl(b)->power : NULL; }
typedef struct {
    nx_device_config_state_t core;
    nx_i2c_impl_t bus;
    nx_i2c_state_t state;
    uint8_t* tx;
    uint8_t* rx;
    size_t tx_size, rx_size;
} native_i2c_storage_t;
static nx_status_t nx_i2c_construct(const nx_device_t* dev, void** out) {
    if(!out) return NX_ERR_NULL_PTR;
    *out=NULL;
    if(!dev || !dev->state || !dev->config) return NX_ERR_INVALID_PARAM;
    const nx_i2c_platform_config_t* cfg=dev->config;
    native_i2c_storage_t* storage=NX_CONTAINER_OF(dev->state,native_i2c_storage_t,core);
    if(!cfg->speed || !cfg->tx_buf_size || !cfg->rx_buf_size || !storage->tx ||
       !storage->rx || storage->tx_size!=cfg->tx_buf_size ||
       storage->rx_size!=cfg->rx_buf_size) return NX_ERR_INVALID_PARAM;
    nx_i2c_impl_t* b=&storage->bus;
    memset(b,0,sizeof(*b)); b->state=&storage->state;
    memset(b->state,0,sizeof(*b->state)); b->state->index=cfg->i2c_index;
    b->state->config=(nx_i2c_config_t){cfg->speed,cfg->scl_pin,cfg->sda_pin,false,false,cfg->tx_buf_size,cfg->rx_buf_size};
    b->state->tx_buf.data=storage->tx; b->state->tx_buf.size=cfg->tx_buf_size;
    b->state->rx_buf.data=storage->rx; b->state->rx_buf.size=cfg->rx_buf_size;
    b->device=(nx_device_t*)dev;
    NX_INIT_I2C_BUS(&b->base,get_tx_sync,get_tx_rx_sync,get_tx_async,get_tx_rx_async,get_lifecycle,get_power);
    b->base.open_device=open_device; b->base.close_device=close_device; b->base.service=service;
    i2c_init_lifecycle(&b->lifecycle); i2c_init_power(&b->power);
    *out=&b->base;
    return NX_OK;
}
#define NX_I2C_CONFIG(index) \
    static const nx_i2c_platform_config_t i2c_config_##index={ \
        index,NX_CONFIG_I2C##index##_SPEED,0,1, \
        NX_CONFIG_I2C##index##_TX_BUFFER_SIZE,NX_CONFIG_I2C##index##_RX_BUFFER_SIZE}
#define NX_I2C_DEVICE_REGISTER(index) \
    NX_I2C_CONFIG(index); \
    static uint8_t i2c_tx_##index[NX_CONFIG_I2C##index##_TX_BUFFER_SIZE]; \
    static uint8_t i2c_rx_##index[NX_CONFIG_I2C##index##_RX_BUFFER_SIZE]; \
    static native_i2c_storage_t i2c_storage_##index={ \
        .tx=i2c_tx_##index,.rx=i2c_rx_##index, \
        .tx_size=sizeof(i2c_tx_##index),.rx_size=sizeof(i2c_rx_##index)}; \
    NX_DEVICE_REGISTER_TYPED(DEVICE_TYPE,index,"I2C" #index,&i2c_config_##index, \
        &i2c_storage_##index.core,NX_DEVICE_CLASS_I2C, \
        NX_DEVICE_CAP_I2C_DEVICES|NX_DEVICE_CAP_I2C_QUEUE|NX_DEVICE_CAP_I2C_CANCEL, \
        nx_i2c_construct,NULL);
NX_TRAVERSE_EACH_INSTANCE(NX_I2C_DEVICE_REGISTER,DEVICE_TYPE)
