/** Test-only addressed response and latency injection. Never linked in Firmware. */
#include "hal/base/nx_device.h"
#include "hal/provider/nx_device_provider.h"
#include "../../../soc/native/controllers/i2c/nx_i2c_helpers.h"
#include <string.h>
nx_status_t typed_i2c_response(nx_device_ref_t ref, uint8_t address, const uint8_t* data, size_t len) {
    if (!data || !len || len > NATIVE_I2C_PAYLOAD_CAPACITY || address > 127) return NX_ERR_INVALID_PARAM;
    nx_i2c_impl_t* b = i2c_get_impl(ref.descriptor->state->api);
    native_i2c_lock(); nx_status_t s = native_i2c_bus_status(b); native_i2c_response_t* empty = NULL;
    if (s == NX_OK) {
        for (unsigned i=0; i<NATIVE_I2C_RESPONSE_CAPACITY; ++i) {
            native_i2c_response_t* p = &b->responses[i];
            if (p->used && p->address == address) { s = NX_ERR_BUSY; break; }
            if (!p->used && !empty) empty = p;
        }
        if (s == NX_OK && !empty) s = NX_ERR_NO_RESOURCE;
        if (s == NX_OK) {
            empty->used = true; empty->address = address; empty->length = len; empty->offset = 0;
            memcpy(empty->data, data, len);
        }
    }
    native_i2c_unlock(); return s;
}
nx_status_t typed_i2c_delay(nx_device_ref_t ref, uint32_t delay) {
    nx_i2c_impl_t* b = i2c_get_impl(ref.descriptor->state->api);
    native_i2c_lock(); nx_status_t s = b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if (s == NX_OK) b->transfer_delay_ms = delay;
    native_i2c_unlock(); return s;
}
nx_status_t typed_i2c_failure(nx_device_ref_t ref, nx_status_t failure) {
    nx_i2c_impl_t* b = i2c_get_impl(ref.descriptor->state->api);
    native_i2c_lock(); nx_status_t s = b->users || b->worker_active ? NX_ERR_BUSY : NX_OK;
    if (s == NX_OK) b->next_failure = failure;
    native_i2c_unlock(); return s;
}
