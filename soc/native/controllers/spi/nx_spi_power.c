/** Native logical power transitions share lifecycle arbitration. */
#include "nx_spi_helpers.h"
#include "osal/osal.h"
static nx_status_t transition(nx_power_t* self,bool on) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,power);
    native_spi_lock();
    nx_status_t r=!b->state->initialized ? NX_ERR_NOT_INIT : NX_OK;
    bool changed=b->state->suspended==on;
    if(r==NX_OK && changed) {
        if(b->users || b->worker_active) r=NX_ERR_BUSY;
        for(unsigned i=0;i<NATIVE_SPI_DEVICE_CAPACITY && r==NX_OK;++i)
            if(b->devices[i].pending || b->devices[i].servicing) r=NX_ERR_BUSY;
        if(r==NX_OK) b->state->suspended=!on;
    }
    nx_power_callback_t cb=b->power_callback;
    void* context=b->power_context;
    native_spi_unlock();
    if(r==NX_OK && changed && cb) cb(context,on);
    return r;
}
static nx_status_t enable(nx_power_t* self) { return transition(self,true); }
static nx_status_t disable(nx_power_t* self) { return transition(self,false); }
static bool enabled(nx_power_t* self) {
    if(!self) return false;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,power);
    native_spi_lock(); bool r=b->state->initialized && !b->state->suspended; native_spi_unlock(); return r;
}
static nx_status_t callback(nx_power_t* self,nx_power_callback_t cb,void* context) {
    if(!self || osal_is_isr()) return NX_ERR_INVALID_PARAM;
    nx_spi_impl_t* b=NX_CONTAINER_OF(self,nx_spi_impl_t,power);
    native_spi_lock(); b->power_callback=cb; b->power_context=context; native_spi_unlock(); return NX_OK;
}
void spi_init_power(nx_power_t* self) { self->enable=enable; self->disable=disable; self->is_enabled=enabled; self->set_callback=callback; }
