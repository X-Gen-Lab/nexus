/**
 * \file            gd32f303_transport.h
 * \brief           Private exact USART0 channel-DMA and I2C0 IRQ bindings
 * \author          Nexus Team
 */
#ifndef NX_GD32F303_TRANSPORT_H
#define NX_GD32F303_TRANSPORT_H
#include "../../../../drivers/i2c/i2c_v1_async/i2c_v1_async.h"
#include "../../../../drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.h"
#include "gd32f303_provider.h"
#include "gd32f30x_dma.h"
#include "gd32f30x_i2c.h"
typedef nx_uart_v1_dma_state_t nx_gd32f303_uart_dma_state_t;
typedef nx_uart_v1_dma_duplex_state_t nx_gd32f303_uart_duplex_state_t;
typedef struct {
    nx_i2c_v1_state_t core;
    nx_irq_policy_t irq_policy;
    uint32_t saved_control;
    uint32_t saved_latch;
    uint32_t gates_added;
    bool retained;
    bool pins_captured;
    bool core_owned;
} nx_gd32f303_i2c_async_state_t;
typedef nx_i2c_v1_endpoint_t nx_gd32f303_i2c_async_endpoint_state_t;
#define nx_gd32f303_uart_dma_ops           nx_uart_v1_channel_dma_tx_ops
#define nx_gd32f303_uart_duplex_ops        nx_uart_v1_channel_dma_ops
#define nx_gd32f303_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_gd32f303_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Admit exact USART0, DMA0 channel3 TX/channel4 RX at IRC8.
 * \param[in,out] state: Static cold owner and explicit DMA-visible domains.
 * \return Success/error; retained storage survives failed drain or rollback.
 * \note Privileged unmasked task. Platform owns exact pin and gate leases.
 */
nx_result_t
nx_gd32f303_uart_dma_initialize(nx_gd32f303_uart_dma_state_t* state);
/** \brief Admit both finite DMA lanes with one embedded RX extension.
 * \param[in,out] state: Exact static duplex owner and caller DMA domains.
 * \return Success/error; no request, block or IRQ borrow is revoked.
 */
nx_result_t
nx_gd32f303_uart_duplex_initialize(nx_gd32f303_uart_duplex_state_t* state);
/** \brief Admit default PB6/PB7 I2C0 at 8MHz/80kHz with IRQ messages.
 * \param[in,out] state: Static cold owner; fixed stream DMA is unsupported.
 * \return Success/error; foreign hardware is never reset or adopted.
 */
nx_result_t
nx_gd32f303_i2c_async_initialize(nx_gd32f303_i2c_async_state_t* state);
/** \brief Join core publishers before restoring only selected PB6/PB7 pads.
 * \param[in,out] state: Same owner; unsuccessful readback retains leases.
 * \return Success only after core, pads and added gates detach.
 */
nx_result_t nx_gd32f303_i2c_async_stop(nx_gd32f303_i2c_async_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
