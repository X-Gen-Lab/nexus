/** STM32 driver internals. No vendor types escape the common HAL. */
#ifndef STM32_SPI_H
#define STM32_SPI_H
#include "arch/nx_arch.h"
#include "stm32_spi_types.h"
#include "stm32_spi_resource.h"
#ifdef __cplusplus
extern "C" {
#endif

static inline stm32_spi_board_port_t spi_board_port(stm32_spi_impl_t* bus) {
    return (stm32_spi_board_port_t){.handle=&bus->hspi,
        .instance=bus->state->instance,.dma_tx_enabled=bus->dma_tx_enabled,
        .dma_rx_enabled=bus->dma_rx_enabled};
}
static inline nx_status_t spi_board_prepare(stm32_spi_impl_t* bus) {
    stm32_spi_board_port_t port=spi_board_port(bus);
    return stm32_spi_board_prepare(&port);
}
static inline void spi_board_release(stm32_spi_impl_t* bus) {
    stm32_spi_board_port_t port=spi_board_port(bus);
    stm32_spi_board_release(&port);
}
bool stm32_spi_board_dma_buffer_valid(const void* data, size_t length, bool write);
/* Fatal hardware inability to stop DMA cannot return caller-owned buffers. */
NX_NORETURN void stm32_spi_dma_failstop(stm32_spi_impl_t* bus);

void stm32_spi_construct(stm32_spi_impl_t* bus,
                         const stm32_spi_platform_config_t* config);
void spi_init_lifecycle(nx_lifecycle_t* lifecycle);
void spi_init_power(nx_power_t* power);
void spi_init_tx_sync(nx_tx_sync_t* iface);
void spi_init_tx_rx_sync(nx_tx_rx_sync_t* iface);
void spi_init_tx_async(nx_tx_async_t* iface);
void spi_init_tx_rx_async(nx_tx_rx_async_t* iface);
nx_status_t spi_transfer(stm32_spi_device_t* device,
                          const nx_spi_transaction_t* transaction,
                          uint32_t started_at, bool worker, uint64_t token);
nx_status_t spi_submit(stm32_spi_device_t* device,
                        const nx_spi_transaction_t* transaction, uint64_t token, uint32_t started_at);
nx_status_t spi_cancel(stm32_spi_device_t* device, uint64_t token);
nx_status_t spi_service(nx_spi_bus_t* self);
nx_status_t spi_dma_init(stm32_spi_impl_t* bus);
nx_status_t spi_dma_deinit(stm32_spi_impl_t* bus);
nx_status_t spi_dma_drain(stm32_spi_impl_t* bus);
bool spi_publish(stm32_spi_impl_t* bus, nx_status_t result);
void spi_registry_add(stm32_spi_impl_t* bus);
void spi_registry_remove(stm32_spi_impl_t* bus);

static inline uint32_t spi_critical_enter(void) {
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return saved;
}
static inline void spi_critical_leave(uint32_t saved) {
    __DMB();
    __set_PRIMASK(saved);
}
static inline uint32_t spi_remaining(uint32_t started_at, uint32_t timeout_ms) {
    uint32_t elapsed = HAL_GetTick() - started_at;
    return timeout_ms == UINT32_MAX ? UINT32_MAX :
           elapsed >= timeout_ms ? 0U : timeout_ms - elapsed;
}
static inline nx_status_t spi_hal_result(HAL_StatusTypeDef status) {
    return status == HAL_OK ? NX_OK : status == HAL_TIMEOUT ? NX_ERR_TIMEOUT :
           status == HAL_BUSY ? NX_ERR_BUSY : NX_ERR_IO;
}
#ifdef __cplusplus
}
#endif
#endif
