/** Explicit Native I2C fixtures. Observation never completes async work. */
#include "native_i2c_helpers.h"
#include "hal/nx_factory.h"
#include "../../../../soc/native/controllers/i2c/nx_i2c_helpers.h"
#include <limits.h>
#include <string.h>
#define NX_I2C_MAX_INSTANCES 8
static nx_i2c_impl_t* get(uint8_t index) {
    return index<NX_I2C_MAX_INSTANCES ? i2c_get_impl(nx_factory_i2c(index)) : NULL;
}
nx_status_t native_i2c_get_state(uint8_t instance,native_i2c_state_t* out) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || !out) return NX_ERR_INVALID_PARAM;
    native_i2c_lock();
    *out=(native_i2c_state_t){b->state->initialized,b->state->suspended,
        b->users!=0,b->state->config.speed,b->state->config.scl_pin,
        b->state->config.sda_pin,b->state->current_dev_addr,
        b->state->stats.tx_count,b->state->stats.rx_count,b->state->stats.nack_count,
        b->state->stats.bus_error_count,b->state->tx_buf.count,b->state->rx_buf.count};
    native_i2c_unlock(); return NX_OK;
}
nx_status_t native_i2c_inject_rx_data(uint8_t instance,const uint8_t* data,size_t len) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || !data || !len) return NX_ERR_INVALID_PARAM;
    native_i2c_lock(); nx_status_t r=native_i2c_bus_status(b);
    if(r==NX_OK) {
        if(len>b->state->rx_buf.size-b->state->rx_buf.count) r=NX_ERR_NO_MEMORY;
        else i2c_buffer_write(&b->state->rx_buf,data,len);
    }
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_inject_rx_for_device(uint8_t instance,uint8_t address,
                                           const uint8_t* data,size_t len) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || address>0x7f || !data || !len) return NX_ERR_INVALID_PARAM;
    if(len>NATIVE_I2C_PAYLOAD_CAPACITY) return NX_ERR_INVALID_SIZE;
    native_i2c_lock(); nx_status_t r=native_i2c_bus_status(b);
    native_i2c_response_t* empty=NULL;
    if(r==NX_OK) {
        for(unsigned i=0;i<NATIVE_I2C_RESPONSE_CAPACITY;++i) {
            native_i2c_response_t* p=&b->responses[i];
            if(p->used && p->address==address) { r=NX_ERR_BUSY; break; }
            if(!p->used && !empty) empty=p;
        }
        if(r==NX_OK && !empty) r=NX_ERR_NO_RESOURCE;
        if(r==NX_OK) {
            empty->used=true; empty->address=address;
            empty->length=len; empty->offset=0; memcpy(empty->data,data,len);
        }
    }
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_get_tx_data(uint8_t instance,uint8_t* data,size_t* len) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || !data || !len || !*len) return NX_ERR_INVALID_PARAM;
    native_i2c_lock(); nx_status_t r=!b->state->initialized ? NX_ERR_INVALID_STATE : NX_OK;
    if(r==NX_OK) *len=i2c_buffer_read(&b->state->tx_buf,data,*len);
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_set_transfer_delay(uint8_t instance,uint32_t delay_ms) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || delay_ms>INT32_MAX) return NX_ERR_INVALID_PARAM;
    native_i2c_lock(); nx_status_t r=b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK) b->transfer_delay_ms=delay_ms;
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_fail_next_transfer(uint8_t instance,nx_status_t failure) {
    nx_i2c_impl_t* b=get(instance);
    if(!b || (failure!=NX_ERR_NACK && failure!=NX_ERR_BUS &&
              failure!=NX_ERR_ARBITRATION && failure!=NX_ERR_IO)) return NX_ERR_INVALID_PARAM;
    native_i2c_lock(); nx_status_t r=b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if(r==NX_OK) b->next_failure=failure;
    native_i2c_unlock(); return r;
}
nx_status_t native_i2c_reset(uint8_t instance) {
    nx_i2c_impl_t* b=get(instance);
    return b ? native_i2c_reset_impl(b) : NX_ERR_INVALID_PARAM;
}
void native_i2c_reset_all(void) {
    for(uint8_t index=0;index<NX_I2C_MAX_INSTANCES;++index) (void)native_i2c_reset(index);
}
