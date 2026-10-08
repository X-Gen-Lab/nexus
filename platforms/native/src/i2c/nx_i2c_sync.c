/** Immutable legacy synchronous interfaces over the transaction engine. */
#include "nx_i2c_helpers.h"
static nx_status_t send(nx_tx_sync_t* self,const uint8_t* data,size_t n,uint32_t timeout) {
    if(!self) return NX_ERR_NULL_PTR;
    if(!data) return NX_ERR_NULL_PTR;
    if(!n) return NX_ERR_INVALID_SIZE;
    native_i2c_device_t* d=NX_CONTAINER_OF(self,native_i2c_device_t,tx_sync);
    nx_i2c_transaction_t t={data,n,NULL,0,NULL,timeout,NULL,NULL};
    return native_i2c_execute(d,&t,native_i2c_now(),false);
}
static nx_status_t tx_rx(nx_tx_rx_sync_t* self,const uint8_t* tx,size_t n,
    uint8_t* rx,size_t* rx_n,uint32_t timeout) {
    if(!self || !rx_n) return NX_ERR_NULL_PTR;
    size_t cap=*rx_n; *rx_n=0;
    if((n && !tx) || (cap && !rx)) return NX_ERR_NULL_PTR;
    native_i2c_device_t* d=NX_CONTAINER_OF(self,native_i2c_device_t,tx_rx_sync);
    nx_i2c_transaction_t t={tx,n,rx,cap,rx_n,timeout,NULL,NULL};
    return native_i2c_execute(d,&t,native_i2c_now(),false);
}
void i2c_init_tx_sync(nx_tx_sync_t* out) { out->send=send; }
void i2c_init_tx_rx_sync(nx_tx_rx_sync_t* out) { out->tx_rx=tx_rx; }
