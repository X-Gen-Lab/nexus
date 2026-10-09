/** Resource opening and closing are task-context operations. Successful close
 * guarantees IRQ/RX storage quiescence. Errors retain the descriptor owner. */
#include "stm32_uart_runtime.h"
#include "stm32_uart_helpers.h"
#include <string.h>

static nx_status_t deinit(nx_lifecycle_t* self);
static nx_status_t init(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);
    if (!impl->state) return NX_ERR_NOT_INIT;
    if (impl->state->initialized) return NX_ERR_ALREADY_INIT;
    if (!impl->state->config.baudrate || impl->state->config.baudrate > 3000000)
        return NX_ERR_INVALID_PARAM;
    if (impl->state->config.word_length == UART_WORDLENGTH_9B &&
        impl->state->config.parity == UART_PARITY_NONE) return NX_ERR_NOT_SUPPORTED;
    if (impl->state->config.dma_tx_enable || impl->state->config.dma_rx_enable)
        return NX_ERR_NOT_SUPPORTED;
    nx_status_t status = stm32_uart_board_prepare(impl->state->instance);
    if (status != NX_OK) return status;
    impl->board_prepared = true;
    /* Track partial ownership so a failed vendor init cannot masquerade as
     * an untouched device. Failed cleanup is retried/quarantined by the core. */
    impl->state->initialized = true;
    impl->huart.Init.BaudRate = impl->state->config.baudrate;
    impl->huart.Init.WordLength = impl->state->config.word_length;
    impl->huart.Init.StopBits = impl->state->config.stop_bits;
    impl->huart.Init.Parity = impl->state->config.parity;
    impl->huart.Init.Mode = impl->state->config.mode;
    impl->huart.Init.HwFlowCtl = impl->state->config.hw_flow_ctl;
    status = stm32_uart_hal_to_nx_status(HAL_UART_Init(&impl->huart));
    if (status == NX_OK) status = uart_register_isr(impl);
    if (status != NX_OK) {
        impl->faulted = true;
        (void)deinit(self);
        return status;
    }
    impl->state->initialized = true;
    impl->state->suspended = impl->state->tx_busy = false;
    impl->faulted = false;
    impl->notification_pending = impl->callback_active = impl->ticket_active = false;
    impl->rx_head = impl->rx_tail = impl->rx_count = 0;
    impl->rx_lost = 0;
    impl->rx_fault_pending = false;
    status = stm32_uart_arm_rx(impl);
    if (status != NX_OK) {
        impl->faulted = true;
        (void)deinit(self);
    }
    return status;
}
static nx_status_t deinit(nx_lifecycle_t* self) {
    if (!self) return NX_ERR_NULL_PTR;
    if (nx_arch_in_isr()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);
    nx_arch_irq_state_t key = nx_arch_irq_save();
    if (!impl->state || !impl->state->initialized) { nx_arch_irq_restore(key); return NX_ERR_NOT_INIT; }
    if (impl->state->tx_busy || impl->callback_active || impl->closing) {
        nx_arch_irq_restore(key); return NX_ERR_BUSY;
    }
    /* Reserve close before leaving the metadata section, so another task
     * cannot begin callback dispatch or borrow a new buffer during teardown. */
    impl->closing = true;
    nx_arch_dmb(); nx_arch_irq_restore(key);
    /* Disable all RX/TX sources before detaching the dispatch context. */
    HAL_StatusTypeDef hal_status = HAL_UART_Abort(&impl->huart);
    nx_status_t status = stm32_uart_hal_to_nx_status(hal_status);
    if (status != NX_OK) goto failed;
    status = uart_unregister_isr(impl);
    if (status != NX_OK) goto failed;
    hal_status = HAL_UART_DeInit(&impl->huart);
    status = stm32_uart_hal_to_nx_status(hal_status);
    if (status != NX_OK) goto failed;
    status = stm32_uart_board_release(impl->state->instance);
    if (status != NX_OK) goto failed;
    key = nx_arch_irq_save();
    impl->state->initialized = false;
    impl->faulted = false;
    impl->state->suspended = false;
    impl->board_prepared = false;
    impl->rx_count = impl->rx_lost = 0;
    impl->rx_fault_pending = false;
    impl->notification_pending = impl->callback_active = impl->ticket_active = false;
    impl->closing = false;
    memset(&impl->callbacks, 0, sizeof(impl->callbacks));
    nx_arch_dmb(); nx_arch_irq_restore(key);
    return NX_OK;
failed:
    key = nx_arch_irq_save();
    if (!impl->faulted) {
        impl->rx_fault_pending = true;
        impl->rx_fault_timestamp = stm32_uart_board_timestamp_us();
    }
    impl->faulted = true;
    impl->closing = false;
    nx_arch_dmb(); nx_arch_irq_restore(key);
    return status;
}
static nx_status_t suspend(nx_lifecycle_t* self) {
    if (!self || nx_arch_in_isr()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    if (impl->state->suspended) return NX_ERR_INVALID_STATE;
    if (impl->state->tx_busy || impl->callback_active || impl->closing) return NX_ERR_BUSY;
    HAL_StatusTypeDef status = HAL_UART_AbortReceive(&impl->huart);
    if (status != HAL_OK) return stm32_uart_hal_to_nx_status(status);
    HAL_NVIC_DisableIRQ(stm32_uart_get_irq_number(impl->state->instance));
    __HAL_UART_DISABLE(&impl->huart);
    impl->state->suspended = true;
    return NX_OK;
}
static nx_status_t resume(nx_lifecycle_t* self) {
    if (!self || nx_arch_in_isr()) return NX_ERR_INVALID_STATE;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);
    if (!impl->state || !impl->state->initialized) return NX_ERR_NOT_INIT;
    if (!impl->state->suspended) return NX_ERR_INVALID_STATE;
    if (impl->callback_active || impl->closing) return NX_ERR_BUSY;
    __HAL_UART_ENABLE(&impl->huart);
    nx_status_t status = stm32_uart_arm_rx(impl);
    if (status != NX_OK) { __HAL_UART_DISABLE(&impl->huart); return status; }
    impl->state->suspended = false;
    IRQn_Type irq = stm32_uart_get_irq_number(impl->state->instance);
    HAL_NVIC_ClearPendingIRQ(irq);
    HAL_NVIC_EnableIRQ(irq);
    return NX_OK;
}
static nx_device_state_t state(nx_lifecycle_t* self) {
    if (!self) return NX_DEV_STATE_ERROR;
    stm32_uart_impl_t* impl = NX_CONTAINER_OF(self, stm32_uart_impl_t, lifecycle);
    if (!impl->state || impl->faulted) return NX_DEV_STATE_ERROR;
    if (!impl->state->initialized) return NX_DEV_STATE_UNINITIALIZED;
    return impl->state->suspended ? NX_DEV_STATE_SUSPENDED : NX_DEV_STATE_RUNNING;
}
void uart_init_lifecycle(nx_lifecycle_t* self) {
    *self = (nx_lifecycle_t){.init = init, .deinit = deinit, .suspend = suspend,
                           .resume = resume, .get_state = state};
}
