/** Static dispatch identity table. Foreign SDK handles are never container-cast. */
#include "stm32_uart_runtime.h"
#include "stm32_uart_helpers.h"
#include "hal/resource/nx_isr_manager.h"

static stm32_uart_impl_t* volatile connected[6];
static void handle_irq(void* context) {
    stm32_uart_impl_t* impl = context;
    if (impl && impl->state && impl->state->initialized && !impl->state->suspended)
        HAL_UART_IRQHandler(&impl->huart);
}
stm32_uart_impl_t* stm32_uart_from_handle(UART_HandleTypeDef* handle) {
    for (size_t i = 0; i < 6; i++) {
        if (connected[i] && &connected[i]->huart == handle) return connected[i];
    }
    return NULL;
}
nx_status_t uart_register_isr(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state || impl->state->instance >= 6) return NX_ERR_INVALID_PARAM;
    nx_isr_manager_t* manager = nx_isr_manager_get();
    if (!manager) return NX_ERR_NOT_INIT;
    uint8_t index = impl->state->instance;
    IRQn_Type irq = stm32_uart_get_irq_number(index);
    if (irq < 0) return NX_ERR_INVALID_PARAM;
    if (connected[index] && connected[index] != impl) return NX_ERR_BUSY;
    connected[index] = impl;
    nx_status_t status = manager->connect(manager, (uint32_t)irq, handle_irq, impl, 5);
    if (status != NX_OK) { connected[index] = NULL; return status; }
    impl->irq_connected = true;
    return NX_OK;
}
nx_status_t uart_unregister_isr(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state || impl->state->instance >= 6) return NX_ERR_INVALID_PARAM;
    if (!impl->irq_connected) return NX_OK;
    nx_isr_manager_t* manager = nx_isr_manager_get();
    if (!manager) return NX_ERR_NOT_INIT;
    IRQn_Type irq = stm32_uart_get_irq_number(impl->state->instance);
    HAL_NVIC_DisableIRQ(irq);
    HAL_NVIC_ClearPendingIRQ(irq);
    nx_status_t status = manager->disconnect(manager, (uint32_t)irq);
    if (status != NX_OK) return status;
    connected[impl->state->instance] = NULL;
    impl->irq_connected = false;
    return NX_OK;
}
__weak nx_status_t stm32_uart_board_prepare(uint8_t instance) {
    (void)instance; return NX_ERR_NOT_SUPPORTED;
}
__weak nx_status_t stm32_uart_board_release(uint8_t instance) {
    (void)instance; return NX_ERR_NOT_SUPPORTED;
}
__weak nx_status_t stm32_uart_board_direction(uint8_t instance, bool active) {
    (void)instance; return active ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
__weak uint64_t stm32_uart_board_timestamp_us(void) { return (uint64_t)HAL_GetTick() * 1000; }
__weak uint32_t stm32_uart_board_timestamp_resolution_us(void) { return 1000; }
