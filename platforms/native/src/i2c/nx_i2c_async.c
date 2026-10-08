/** Nonblocking legacy submissions copy bounded TX and require bus.service(). */
#include "nx_i2c_helpers.h"
#include "osal/osal.h"
#include <limits.h>
#include <string.h>
static void completed(void* context,nx_status_t result) {
    native_i2c_device_t* d=context;
    if(result==NX_OK && d->receive_callback)
        d->receive_callback(d->receive_context,d->bus->rx_copy,d->bus->received_length);
}
static nx_status_t submit(native_i2c_device_t* d,const uint8_t* data,
                          size_t n,uint32_t timeout,bool receive) {
    uint64_t at=native_i2c_now();
    if((n && !data) || (!n && !receive)) return NX_ERR_INVALID_PARAM;
    if(n>NATIVE_I2C_PAYLOAD_CAPACITY) return NX_ERR_INVALID_SIZE;
    if(!timeout || timeout>INT32_MAX) return NX_ERR_INVALID_PARAM;
    if(osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=d->bus;
    native_i2c_lock();
    nx_status_t r=b->pending || b->worker_active || d->users ? NX_ERR_BUSY : native_i2c_bus_status(b);
    if(r==NX_OK) {
        if(n) memcpy(b->tx_copy,data,n);
        b->received_length=0;
        nx_i2c_transaction_t t={b->tx_copy,n,receive ? b->rx_copy : NULL,
            receive ? sizeof(b->rx_copy) : 0,&b->received_length,timeout,completed,d};
        r=native_i2c_submit(d,&t,at);
    }
    native_i2c_unlock(); return r;
}
static nx_status_t send(nx_tx_async_t* self,const uint8_t* data,size_t n) {
    return self ? submit(NX_CONTAINER_OF(self,native_i2c_device_t,tx_async),data,n,1000,false) : NX_ERR_NULL_PTR;
}
static nx_status_t tx_rx(nx_tx_rx_async_t* self,const uint8_t* data,size_t n,uint32_t timeout) {
    return self ? submit(NX_CONTAINER_OF(self,native_i2c_device_t,tx_rx_async),data,n,timeout,true) : NX_ERR_NULL_PTR;
}
static nx_status_t state(native_i2c_device_t* d) {
    native_i2c_lock(); nx_status_t r=native_i2c_bus_status(d->bus);
    if(r==NX_OK) r=d->users ? NX_ERR_BUSY : d->last_result;
    native_i2c_unlock(); return r;
}
static nx_status_t tx_state(nx_tx_async_t* self) {
    return self ? state(NX_CONTAINER_OF(self,native_i2c_device_t,tx_async)) : NX_ERR_NULL_PTR;
}
static nx_status_t rx_state(nx_tx_rx_async_t* self) {
    return self ? state(NX_CONTAINER_OF(self,native_i2c_device_t,tx_rx_async)) : NX_ERR_NULL_PTR;
}
void i2c_init_tx_async(nx_tx_async_t* out) { out->send=send; out->get_state=tx_state; }
void i2c_init_tx_rx_async(nx_tx_rx_async_t* out) { out->tx_rx=tx_rx; out->get_state=rx_state; }
