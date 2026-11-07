/**
 * \file            stm32f4_lifecycle.h
 * \brief           Static C lifecycle bindings for reviewed F407 I/O modes.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F4_LIFECYCLE_H
#define NEXUS_STM32F4_LIFECYCLE_H

#include "nexus/core/lifecycle.h"
#include "stm32f407_basic_cold.h"
#include "stm32f407_spi_cold.h"
#include "stm32f407_uart_cold.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Readonly UART binding; receipt storage exists exactly once. */
typedef struct {
    const nx_stm32_uart_cold_config_t* config;
    nx_stm32_uart_cold_receipt_t* receipt;
} nx_stm32f4_uart_lifecycle_config_t;

/** \brief Readonly polling SPI binding, without another mutable wrapper. */
typedef struct {
    const nx_stm32_spi_config_t* config;
    nx_stm32_spi_cold_receipt_t* receipt;
} nx_stm32f4_spi_lifecycle_config_t;

/**
 * \brief           Admit privileged, unmasked Thread execution without effects.
 * \param[in]       context: Reserved; pass NULL.
 * \param[in]       operation: START or STOP, both requiring cold context.
 * \return          SUCCESS, CONTEXT or INVALID; never alters CPU state.
 * \note            The system clock owner is already established externally.
 */
nx_result_t nx_stm32f4_lifecycle_context(const void* context,
                                         nx_lifecycle_operation_t operation);

/**
 * \brief           Shared pad owner precedes every selected peripheral entry.
 * \note            state is nx_stm32_platform_pin_receipt_t; config is the
 *                  unchanged nx_stm32_platform_config_t. All selected gates,
 *                  vectors and pads are admitted before any acquisition.
 */
extern const nx_lifecycle_ops_t nx_stm32f4_pins_lifecycle_ops;
/**
 * \brief           GPIO input/output with Platform-owned pin snapshots.
 * \note            state is nx_stm32_gpio_state_t; config is
 *                  nx_stm32_gpio_config_t. Stop uses the initial level.
 *                  Shared pins retain partial electrical changes until their
 *                  final owner restores the original pads and gates.
 */
extern const nx_lifecycle_ops_t nx_stm32f4_gpio_lifecycle_ops;
/**
 * \brief           Reviewed IRQ byte/event UART.
 * \note            state is nx_stm32_uart_state_t; config is readonly
 *                  nx_stm32f4_uart_lifecycle_config_t. DMA/word/block modes
 *                  are excluded. All callers, requests and IRQ users must
 *                  drain before the enclosing plan stops.
 */
extern const nx_lifecycle_ops_t nx_stm32f4_uart_irq_lifecycle_ops;
/**
 * \brief           Reviewed short polling SPI controller and static endpoints.
 * \note            state is nx_stm32_spi_state_t; config is the readonly
 *                  nx_stm32f4_spi_lifecycle_config_t. DMA is excluded.
 *                  All endpoint calls must join before stop.
 */
extern const nx_lifecycle_ops_t nx_stm32f4_spi_poll_lifecycle_ops;

#ifdef __cplusplus
}
#endif
#endif
