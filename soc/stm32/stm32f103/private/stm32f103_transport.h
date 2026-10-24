/**
 * \file            stm32f103_transport.h
 * \brief           Private exact USART2 channel-DMA and I2C1 IRQ bindings
 * \author          Nexus Team
 */
#ifndef NX_STM32F103_TRANSPORT_H
#define NX_STM32F103_TRANSPORT_H
#include "../../../common/i2c_v1_async.h"
#include "../../../common/uart_v1_channel_dma.h"
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
typedef nx_i2c_v1_endpoint_t nx_stm32f103_i2c_async_endpoint_state_t;
#define nx_stm32f103_uart_dma_ops           nx_uart_v1_channel_dma_tx_ops
#define nx_stm32f103_uart_duplex_ops        nx_uart_v1_channel_dma_ops
#define nx_stm32f103_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_stm32f103_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Admit exact USART2, DMA1 channels7/6 under Platform's gate lease.
 * \param[in,out] state: Static cold owner with explicit DMA-visible regions.
 * \return Success/error; failed cleanup retains storage and hardware leases.
 * \note Privileged unmasked task; actual wire TC precedes TX settlement.
 */
nx_result_t
nx_stm32f103_uart_dma_initialize(nx_stm32f103_uart_dma_state_t* state);
/** \brief Admit both finite DMA lanes with one embedded RX extension.
 * \param[in,out] state: Exact static duplex owner and caller DMA domains.
 * \return Success/error; no request, block or IRQ borrow is revoked.
 */
nx_result_t
nx_stm32f103_uart_duplex_initialize(nx_stm32f103_uart_duplex_state_t* state);
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
