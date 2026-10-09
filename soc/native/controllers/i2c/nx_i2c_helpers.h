#ifndef NX_I2C_HELPERS_H
#define NX_I2C_HELPERS_H
#include "nx_i2c_types.h"
#ifdef __cplusplus
extern "C" {
#endif
void native_i2c_lock(void);
void native_i2c_unlock(void);
uint64_t native_i2c_now(void);
nx_status_t native_i2c_reset_impl(nx_i2c_impl_t* bus);
nx_status_t native_i2c_bus_status(nx_i2c_impl_t* bus);
void i2c_init_lifecycle(nx_lifecycle_t* lifecycle);
void i2c_init_power(nx_power_t* power);
nx_status_t native_i2c_admit_locked(native_i2c_device_t* device);
nx_status_t native_i2c_execute(native_i2c_device_t* device,
    const nx_i2c_transaction_t* transaction, uint64_t started, bool admitted);
nx_status_t native_i2c_submit(native_i2c_device_t* device,
    const nx_i2c_transaction_t* transaction, uint64_t started);
bool native_i2c_transaction_valid(const nx_i2c_transaction_t* transaction,
                                  bool async);
static inline nx_i2c_impl_t* i2c_get_impl(nx_i2c_bus_t* self) {
    return self ? (nx_i2c_impl_t*)self : NULL;
}
static inline void i2c_buffer_init(nx_i2c_buffer_t* b, uint8_t* data, size_t size) {
    b->data=data; b->size=size; b->head=0; b->tail=0; b->count=0;
}
static inline size_t i2c_buffer_get_count(const nx_i2c_buffer_t* b) {
    return b ? b->count : 0;
}
static inline void i2c_buffer_clear(nx_i2c_buffer_t* b) {
    b->head=0; b->tail=0; b->count=0;
}
size_t i2c_buffer_write(nx_i2c_buffer_t*,const uint8_t*,size_t);
size_t i2c_buffer_read(nx_i2c_buffer_t*,uint8_t*,size_t);
#ifdef __cplusplus
}
#endif
#endif
