/**
 * \file            stm32f407_uart_hal.h
 * \brief           Private F407 HAL TX and disjoint LL RX provider
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_UART_HAL_H
#define NEXUS_STM32F407_UART_HAL_H

#include "stm32f407_uart_cold.h"
#include "stm32f4xx_hal.h"

/** \brief One exact static owner with shared byte/event RX mechanics.
 * \note HAL owns finite TX; LL observes RX under explicit IRQ masking.
 */
typedef struct {
    nx_stm32_uart_state_t uart;
    UART_HandleTypeDef handle;
    DMA_HandleTypeDef dma;
    const nx_stm32_uart_dma_cold_config_t* config;
    uint8_t hal_depth;
    uint8_t masked_vectors;
    uint8_t flags;
} nx_stm32_uart_hal_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Initialize a cold runtime after resources have been acquired.
 * \param[in,out] state: Exact static owner, no retained request or publisher.
 * \param[in] config: Immutable selected route and caller-owned RX storage.
 * \return Success or rejection; partial effects require ordinary close.
 * \note Internal entry only. Full prepare owns the enclosing cold lease.
 *       Platform must first establish the fixed clock and live Nexus timebase;
 *       HAL never bootstraps the monotonic epoch or takes over SysTick.
 */
nx_result_t
nx_stm32_uart_hal_initialize(nx_stm32_uart_hal_state_t* state,
                             const nx_stm32_uart_dma_cold_config_t* config);

/** \brief Acquire the exact cold lease before initializing the HAL provider.
 * \param[in,out] state: Static runtime owner, initially zero and unborrowed.
 * \param[in] config: Immutable selected route, pins and RX storage.
 * \param[in,out] receipt: Zero resource owner retained through failed cleanup.
 * \return Success or refusal; every acquired effect requires the same close.
 * \note Platform must already own its live clock and monotonic timebase.
 */
nx_result_t
nx_stm32_uart_hal_cold_prepare(nx_stm32_uart_hal_state_t* state,
                               const nx_stm32_uart_dma_cold_config_t* config,
                               nx_stm32_uart_cold_receipt_t* receipt);

/** \brief Join all DMA, wire and IRQ references before returning resources.
 * \param[in,out] state: Same owner supplied to cold prepare.
 * \param[in] config: Same immutable route supplied to cold prepare.
 * \param[in,out] receipt: Same retained observations, released only on proof.
 * \return Success or retryable failure without early request revocation.
 */
nx_result_t
nx_stm32_uart_hal_cold_close(nx_stm32_uart_hal_state_t* state,
                             const nx_stm32_uart_dma_cold_config_t* config,
                             nx_stm32_uart_cold_receipt_t* receipt);

/** \brief Handle the selected UART IRQ without sharing RX flags with HAL.
 * \param[in,out] state: Live owner retained through complete withdrawal.
 */
void nx_stm32_uart_hal_irq(nx_stm32_uart_hal_state_t* state);

/** \brief Handle the selected DMA IRQ with one HAL transfer owner.
 * \param[in,out] state: Live owner retained through complete withdrawal.
 */
void nx_stm32_uart_hal_dma_irq(nx_stm32_uart_hal_state_t* state);

extern const nx_uart_ops_t nx_stm32_uart_hal_ops;

#ifdef __cplusplus
}
#endif
#endif
