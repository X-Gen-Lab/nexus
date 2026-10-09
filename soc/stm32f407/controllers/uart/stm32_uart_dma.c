/** UART DMA cannot be enabled without a validated SoC/Board allocation and
 * completion/abort ownership port. IT is the maintained production backend. */
#include "stm32_uart_dma.h"
nx_status_t uart_dma_init(stm32_uart_impl_t* impl) {
    if (!impl || !impl->state) return NX_ERR_INVALID_PARAM;
    return impl->state->config.dma_tx_enable || impl->state->config.dma_rx_enable
        ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
nx_status_t uart_dma_deinit(stm32_uart_impl_t* impl) {
    return impl ? NX_OK : NX_ERR_INVALID_PARAM;
}
nx_status_t uart_dma_transmit(stm32_uart_impl_t* impl, const uint8_t* data, size_t length) {
    (void)impl; (void)data; (void)length; return NX_ERR_NOT_SUPPORTED;
}
nx_status_t uart_dma_receive(stm32_uart_impl_t* impl, uint8_t* data, size_t length) {
    (void)impl; (void)data; (void)length; return NX_ERR_NOT_SUPPORTED;
}
nx_status_t uart_dma_transmit_zerocopy(stm32_uart_impl_t* impl) {
    (void)impl; return NX_ERR_NOT_SUPPORTED;
}
nx_status_t uart_dma_receive_zerocopy(stm32_uart_impl_t* impl, size_t length) {
    (void)impl; (void)length; return NX_ERR_NOT_SUPPORTED;
}
