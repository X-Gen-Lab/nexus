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
/** \brief Immutable selected IRQ policy; exact MMIO topology belongs to C. */
typedef struct {
    nx_irq_policy_t irq_policy;
} nx_gd32f303_i2c_async_config_t;
typedef nx_i2c_v1_endpoint_t nx_gd32f303_i2c_async_endpoint_state_t;
#define nx_gd32f303_uart_dma_ops           nx_uart_v1_channel_dma_tx_ops
#define nx_gd32f303_uart_duplex_ops        nx_uart_v1_channel_dma_ops
#define nx_gd32f303_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_gd32f303_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Withdraw every IRQ selected by this exact retained UART config.
 * \param[in,out] context: Bound UART owner; caller retains it on failure.
 * \return Success only when every selected vector is withdrawn.
 */
nx_result_t nx_gd32f303_uart_dma_stop_irqs(void* context);
/** \brief Validate the exact UART sources against the configured IRQ policy.
 * \param[in] context: Bound UART owner and immutable policy.
 * \param[in] wake: Optional stable caller notification target.
 * \param[in] ceiling: Required syscall ceiling from the immutable policy.
 * \return Success or the first source policy rejection; no hardware changes.
 */
nx_result_t nx_gd32f303_uart_dma_validate_wake(void* context,
                                               const nx_irq_wake_t* wake,
                                               uint8_t ceiling);
/** \brief Prepare the exact default UART pins, then admit finite TX DMA.
 * \param[in,out] state: Static cold owner under Platform's pin/gate leases.
 * \param[in] config: Static immutable resources, live as long as state.
 * \return Success/error; live or retained owners have no new pin effects.
 * \note Privileged unmasked task; Platform retains pin rollback duties.
 */
nx_result_t nx_gd32f303_uart_dma_prepare(nx_gd32f303_uart_dma_state_t* state,
                                         const nx_uart_v1_dma_config_t* config);
/** \brief Prepare default pins, then admit both exact DMA lanes.
 * \param[in,out] state: Static cold owner with no RX stream or fill borrow.
 * \param[in] config: Static immutable resources and this owner's RX extension.
 * \return Success/error; failed cleanup retains all outstanding borrows.
 * \note Privileged unmasked task; Platform retains pin rollback duties.
 */
nx_result_t
nx_gd32f303_uart_duplex_prepare(nx_gd32f303_uart_duplex_state_t* state,
                                const nx_uart_v1_dma_config_t* config);
/** \brief Admit exact USART0, DMA0 channel3 TX/channel4 RX at IRC8.
 * \param[in,out] state: Static cold owner and explicit DMA-visible domains.
 * \param[in] config: Static immutable resources, live as long as state.
 * \return Success/error; retained storage survives failed drain or rollback.
 * \note Privileged unmasked task. Platform owns exact pin and gate leases.
 */
nx_result_t
nx_gd32f303_uart_dma_initialize(nx_gd32f303_uart_dma_state_t* state,
                                const nx_uart_v1_dma_config_t* config);
/** \brief Admit both finite DMA lanes with one embedded RX extension.
 * \param[in,out] state: Exact static duplex owner and caller DMA domains.
 * \param[in] config: Static immutable resources and this owner's RX extension.
 * \return Success/error; no request, block or IRQ borrow is revoked.
 */
nx_result_t
nx_gd32f303_uart_duplex_initialize(nx_gd32f303_uart_duplex_state_t* state,
                                   const nx_uart_v1_dma_config_t* config);
/**
 * \brief Bind exact I2C resources only after all old borrowers detach.
 * \param[in,out] state: Cold static owner, privileged unmasked task only.
 * \param[in] config: Immutable selected IRQ policy, read during this call.
 * \return SUCCESS/error; partial hardware effects require stop retry.
 * \note A live request, wake or publisher rejects before replacing any binding.
 */
nx_result_t
nx_gd32f303_i2c_async_prepare(nx_gd32f303_i2c_async_state_t* state,
                              const nx_gd32f303_i2c_async_config_t* config);
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
