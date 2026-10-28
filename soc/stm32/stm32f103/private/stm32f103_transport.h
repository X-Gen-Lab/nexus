/**
 * \file            stm32f103_transport.h
 * \brief           Private exact USART2 channel-DMA and I2C1 IRQ bindings
 * \author          Nexus Team
 */
#ifndef NX_STM32F103_TRANSPORT_H
#define NX_STM32F103_TRANSPORT_H
#include "../../../../drivers/i2c/i2c_v1_async/i2c_v1_async.h"
#include "../../../../drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.h"
#include "stm32f103_provider.h"
typedef nx_uart_v1_dma_state_t nx_stm32f103_uart_dma_state_t;
typedef nx_uart_v1_dma_duplex_state_t nx_stm32f103_uart_duplex_state_t;
typedef struct {
    nx_i2c_v1_state_t core;
    nx_irq_policy_t irq_policy;
    uint32_t saved_control;
    uint32_t saved_latch;
    uint32_t gates_added;
    bool retained;
    bool pins_captured;
    bool core_owned;
} nx_stm32f103_i2c_async_state_t;
/** \brief Immutable selected IRQ policy; exact MMIO topology belongs to C. */
typedef struct {
    nx_irq_policy_t irq_policy;
} nx_stm32f103_i2c_async_config_t;
typedef nx_i2c_v1_endpoint_t nx_stm32f103_i2c_async_endpoint_state_t;
#define nx_stm32f103_uart_dma_ops           nx_uart_v1_channel_dma_tx_ops
#define nx_stm32f103_uart_duplex_ops        nx_uart_v1_channel_dma_ops
#define nx_stm32f103_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_stm32f103_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Withdraw every IRQ selected by this exact retained UART config.
 * \param[in,out] context: Bound UART owner; caller retains it on failure.
 * \return Success only when every selected vector is withdrawn.
 */
nx_result_t nx_stm32f103_uart_dma_stop_irqs(void* context);
/** \brief Validate the exact UART sources against the configured IRQ policy.
 * \param[in] context: Bound UART owner and immutable policy.
 * \param[in] wake: Optional stable caller notification target.
 * \param[in] ceiling: Required syscall ceiling from the immutable policy.
 * \return Success or the first source policy rejection; no hardware changes.
 */
nx_result_t nx_stm32f103_uart_dma_validate_wake(void* context,
                                                const nx_irq_wake_t* wake,
                                                uint8_t ceiling);
/** \brief Prepare the exact default UART pins, then admit finite TX DMA.
 * \param[in,out] state: Static cold owner under Platform's pin/gate leases.
 * \param[in] config: Static immutable resources, live as long as state.
 * \return Success/error; live or retained owners have no new pin effects.
 * \note Privileged unmasked task; Platform retains pin rollback duties.
 */
nx_result_t
nx_stm32f103_uart_dma_prepare(nx_stm32f103_uart_dma_state_t* state,
                              const nx_uart_v1_dma_config_t* config);
/** \brief Prepare default pins, then admit both exact DMA lanes.
 * \param[in,out] state: Static cold owner with no RX stream or fill borrow.
 * \param[in] config: Static immutable resources and this owner's RX extension.
 * \return Success/error; failed cleanup retains all outstanding borrows.
 * \note Privileged unmasked task; Platform retains pin rollback duties.
 */
nx_result_t
nx_stm32f103_uart_duplex_prepare(nx_stm32f103_uart_duplex_state_t* state,
                                 const nx_uart_v1_dma_config_t* config);
/** \brief Admit exact USART2, DMA1 channels7/6 under Platform's gate lease.
 * \param[in,out] state: Static cold owner with explicit DMA-visible regions.
 * \param[in] config: Static immutable resources, live as long as state.
 * \return Success/error; failed cleanup retains storage and hardware leases.
 * \note Privileged unmasked task; actual wire TC precedes TX settlement.
 */
nx_result_t
nx_stm32f103_uart_dma_initialize(nx_stm32f103_uart_dma_state_t* state,
                                 const nx_uart_v1_dma_config_t* config);
/** \brief Admit both finite DMA lanes with one embedded RX extension.
 * \param[in,out] state: Exact static duplex owner and caller DMA domains.
 * \param[in] config: Static immutable resources and this owner's RX extension.
 * \return Success/error; no request, block or IRQ borrow is revoked.
 */
nx_result_t
nx_stm32f103_uart_duplex_initialize(nx_stm32f103_uart_duplex_state_t* state,
                                    const nx_uart_v1_dma_config_t* config);
/**
 * \brief Bind exact I2C resources only after all old borrowers detach.
 * \param[in,out] state: Cold static owner, privileged unmasked task only.
 * \param[in] config: Immutable selected IRQ policy, read during this call.
 * \return SUCCESS/error; partial hardware effects require stop retry.
 * \note A live request, wake or publisher rejects before replacing any binding.
 */
nx_result_t
nx_stm32f103_i2c_async_prepare(nx_stm32f103_i2c_async_state_t* state,
                               const nx_stm32f103_i2c_async_config_t* config);
/** \brief Admit exact default PB6/PB7 I2C1, 8MHz/80kHz IRQ messages.
 * \param[in,out] state: Static cold owner; no channel-DMA compatibility claim.
 * \return Success/error; non-idle hardware is never reset or adopted.
 */
nx_result_t
nx_stm32f103_i2c_async_initialize(nx_stm32f103_i2c_async_state_t* state);
/** \brief Join I2C/IRQ/wake before restoring only selected PB6/PB7 leases.
 * \param[in,out] state: Same owner; failed readback retains cleanup duties.
 * \return Success only after core, pads and added gates detach.
 */
nx_result_t nx_stm32f103_i2c_async_stop(nx_stm32f103_i2c_async_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
