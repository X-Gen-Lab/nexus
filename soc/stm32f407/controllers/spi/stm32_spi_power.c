/** Power interface uses the same serialized lifecycle contract. */
#include "stm32_spi.h"
static nx_status_t enable(nx_power_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, power);
    bool changed = b->state->suspended;
    nx_status_t r = b->lifecycle.resume(&b->lifecycle);
    if (r == NX_OK && changed && b->power_callback) b->power_callback(b->power_context, true);
    return r;
}
static nx_status_t disable(nx_power_t* self) {
    if (!self) return NX_ERR_INVALID_PARAM;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, power);
    bool changed = !b->state->suspended;
    nx_status_t r = b->lifecycle.suspend(&b->lifecycle);
    if (r == NX_OK && changed && b->power_callback) b->power_callback(b->power_context, false);
    return r;
}
static bool enabled(nx_power_t* self) {
    if (!self) return false;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, power);
    return b->state->initialized && !b->state->suspended && !b->state->fault;
}
static nx_status_t set_callback(nx_power_t* self, nx_power_callback_t callback, void* context) {
    if (!self) return NX_ERR_INVALID_PARAM;
    if (__get_IPSR()) return NX_ERR_INVALID_STATE;
    stm32_spi_impl_t* b = NX_CONTAINER_OF(self, stm32_spi_impl_t, power);
    uint32_t saved = spi_critical_enter();
    b->power_callback = callback; b->power_context = context;
    spi_critical_leave(saved);
    return NX_OK;
}
void spi_init_power(nx_power_t* iface) {
    iface->enable = enable; iface->disable = disable;
    iface->is_enabled = enabled; iface->set_callback = set_callback;
}
