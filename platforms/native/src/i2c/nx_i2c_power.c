/** Observable simulated power transitions; no physical clock/power control. */
#include "nx_i2c_helpers.h"
#include "osal/osal.h"
static nx_status_t change(nx_power_t* self,bool enabled) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,power);
    native_i2c_lock(); nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT : NX_OK;
    bool changed=b->state->suspended==enabled;
    if(r==NX_OK && changed && (b->users || b->worker_active)) r=NX_ERR_BUSY;
    nx_power_callback_t callback=NULL; void* context=NULL;
    if(r==NX_OK && changed) {
        b->state->suspended=!enabled; callback=b->power_callback; context=b->power_context;
        ++b->users;
    }
    native_i2c_unlock();
    if(r==NX_OK && changed) {
        if(callback) callback(context,enabled);
        native_i2c_lock(); --b->users; native_i2c_unlock();
    }
    return r;
}
static nx_status_t enable(nx_power_t* self) { return change(self,true); }
static nx_status_t disable(nx_power_t* self) { return change(self,false); }
static bool is_enabled(nx_power_t* self) {
    if(!self) return false;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,power);
    native_i2c_lock(); bool enabled=b->state->initialized && !b->state->suspended;
    native_i2c_unlock(); return enabled;
}
static nx_status_t set_callback(nx_power_t* self,nx_power_callback_t cb,void* context) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_STATE;
    nx_i2c_impl_t* b=NX_CONTAINER_OF(self,nx_i2c_impl_t,power);
    native_i2c_lock(); b->power_callback=cb; b->power_context=context; native_i2c_unlock();
    return NX_OK;
}
void i2c_init_power(nx_power_t* out) {
    out->enable=enable; out->disable=disable; out->is_enabled=is_enabled; out->set_callback=set_callback;
}
