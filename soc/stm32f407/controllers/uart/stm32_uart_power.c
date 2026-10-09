/** Power operations execute the actual lifecycle transition. */
#include "stm32_uart_types.h"
static nx_status_t enable(nx_power_t* self) {
    if (!self) return NX_ERR_NULL_PTR;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    return impl->state->suspended ? impl->lifecycle.resume(&impl->lifecycle) : NX_OK;
}
static nx_status_t disable(nx_power_t* self) {
    if (!self) return NX_ERR_NULL_PTR;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    return impl->state->suspended ? NX_OK : impl->lifecycle.suspend(&impl->lifecycle);
}
static bool is_enabled(nx_power_t* self) {
    if (!self) return false;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, power);
    return impl->state && impl->state->initialized && !impl->state->suspended && !impl->faulted;
}
static nx_status_t callback(nx_power_t* self, nx_power_callback_t cb, void* context) {
    (void)self; (void)cb; (void)context; return NX_ERR_NOT_SUPPORTED;
}
void uart_init_power(nx_power_t* self) {
    *self = (nx_power_t){.enable=enable,.disable=disable,.is_enabled=is_enabled,.set_callback=callback};
}
