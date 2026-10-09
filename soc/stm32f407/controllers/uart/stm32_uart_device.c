/** Statically allocated UART descriptors. Logical UART0/1/2 map to the SoC's
 * USART1/2/3; opening an unbound board resource reports UNSUPPORTED. */
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "nexus_config.h"
#include "stm32_uart.h"
#include "stm32_uart_helpers.h"
#include "stm32_uart_runtime.h"
#include <string.h>

static nx_tx_async_t* get_tx_async(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->tx_async : NULL;
}
static nx_rx_async_t* get_rx_async(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->rx_async : NULL;
}
static nx_tx_sync_t* get_tx_sync(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->tx_sync : NULL;
}
static nx_rx_sync_t* get_rx_sync(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->rx_sync : NULL;
}
static nx_lifecycle_t* get_lifecycle(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->lifecycle : NULL;
}
static nx_power_t* get_power(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->power : NULL;
}
static nx_uart_operations_t* get_operations(nx_uart_t* self) {
    return self ? &NX_CONTAINER_OF(self, stm32_uart_impl_t, base)->operations : NULL;
}
typedef struct {
    stm32_uart_platform_config_t hardware;
    stm32_uart_impl_t* impl;
    stm32_uart_state_t* state;
    uint8_t* tx_storage;
    nx_uart_rx_event_t* rx_storage;
} uart_resource_t;
static nx_status_t construct(const nx_device_t* descriptor, void** out) {
    if (!descriptor || !out) return NX_ERR_NULL_PTR;
    *out = NULL;
    const uart_resource_t* resource = descriptor->config;
    if (!resource || !resource->hardware.usart_base) return NX_ERR_INVALID_PARAM;
    stm32_uart_impl_t* impl = resource->impl;
    /* Registry caches construction. Device reopen does not reset sequence. */
    memset(impl, 0, sizeof(*impl));
    impl->state = resource->state;
    memset(impl->state, 0, sizeof(*impl->state));
    impl->state->instance = resource->hardware.uart_index;
    impl->state->config = (stm32_uart_config_t){
        .baudrate = resource->hardware.baudrate,
        .word_length = resource->hardware.word_length,
        .stop_bits = resource->hardware.stop_bits,
        .parity = resource->hardware.parity,
        .mode = resource->hardware.mode,
        .hw_flow_ctl = resource->hardware.hw_flow_ctl,
        .dma_tx_enable = resource->hardware.use_dma,
        .dma_rx_enable = resource->hardware.use_dma,
        .tx_buf_size = resource->hardware.tx_buf_size,
        .rx_buf_size = resource->hardware.rx_buf_size};
    impl->huart.Instance = resource->hardware.usart_base;
    impl->huart.Init.OverSampling = UART_OVERSAMPLING_16;
    stm32_uart_buffer_init(&impl->state->tx_buf, resource->tx_storage,
        resource->hardware.tx_buf_size, UART_OVERFLOW_ERROR);
    impl->rx_events = resource->rx_storage;
    impl->rx_capacity = resource->hardware.rx_buf_size;
    impl->device = (nx_device_t*)descriptor;
    NX_INIT_UART(&impl->base, get_tx_async, get_rx_async, get_tx_sync, get_rx_sync,
                  get_lifecycle, get_power);
    impl->base.get_operations = get_operations;
    uart_init_tx_async(&impl->tx_async);
    uart_init_rx_async(&impl->rx_async);
    uart_init_tx_sync(&impl->tx_sync);
    uart_init_rx_sync(&impl->rx_sync);
    uart_init_lifecycle(&impl->lifecycle);
    uart_init_power(&impl->power);
    uart_init_operations(impl);
    *out = &impl->base;
    return NX_OK;
}
static nx_lifecycle_t* descriptor_lifecycle(void* api) {
    return api ? ((nx_uart_t*)api)->get_lifecycle(api) : NULL;
}
#define UART_BASE_0 USART1
#define UART_BASE_1 USART2
#define UART_BASE_2 USART3
#ifdef NX_CONFIG_STM32_UART_USE_DMA
#define UART_DMA_REQUESTED true
#else
#define UART_DMA_REQUESTED false
#endif
#define UART_REGISTER(index)                                                  \
    static stm32_uart_impl_t uart_impl_##index;                                \
    static stm32_uart_state_t uart_runtime_##index;                            \
    static uint8_t uart_tx_##index[NX_CONFIG_STM32_UART##index##_TX_BUFFER_SIZE + 1]; \
    static nx_uart_rx_event_t uart_rx_##index[NX_CONFIG_STM32_UART##index##_RX_BUFFER_SIZE + 1]; \
    static const uart_resource_t uart_config_##index = {                       \
        .hardware = {.usart_base = UART_BASE_##index, .uart_index = index,     \
            .baudrate = NX_CONFIG_STM32_UART##index##_BAUDRATE,                \
            .word_length = NX_CONFIG_STM32_UART_DEFAULT_WORD_LENGTH ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B, \
            .stop_bits = NX_CONFIG_STM32_UART_DEFAULT_STOP_BITS ? UART_STOPBITS_2 : UART_STOPBITS_1, \
            .parity = NX_CONFIG_STM32_UART_DEFAULT_PARITY == 1 ? UART_PARITY_EVEN : NX_CONFIG_STM32_UART_DEFAULT_PARITY == 2 ? UART_PARITY_ODD : UART_PARITY_NONE, .mode = UART_MODE_TX_RX, \
            .hw_flow_ctl = UART_HWCONTROL_NONE, .use_dma = UART_DMA_REQUESTED, \
            .tx_buf_size = NX_CONFIG_STM32_UART##index##_TX_BUFFER_SIZE,        \
            .rx_buf_size = NX_CONFIG_STM32_UART##index##_RX_BUFFER_SIZE},       \
        .impl = &uart_impl_##index, .state = &uart_runtime_##index,            \
        .tx_storage = uart_tx_##index, .rx_storage = uart_rx_##index};          \
    static nx_device_config_state_t uart_registry_##index;                    \
    NX_DEVICE_REGISTER_TYPED(STM32_UART, index, "UART" #index, &uart_config_##index, \
        &uart_registry_##index, NX_DEVICE_CLASS_UART, 0, construct, descriptor_lifecycle)
NX_TRAVERSE_EACH_INSTANCE(UART_REGISTER, STM32_UART);
