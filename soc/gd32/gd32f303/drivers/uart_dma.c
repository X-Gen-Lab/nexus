/**
 * \file            uart_dma.c
 * \brief           Exact GD303 USART0 DMA0 channel3 TX/channel4 RX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_transport.h"
#include <stddef.h>
_Static_assert(offsetof(nx_gd32f303_uart_registers_t, STAT0) == 0U &&
                   offsetof(nx_gd32f303_uart_registers_t, DATA) == 4U &&
                   offsetof(nx_gd32f303_uart_registers_t, BAUD) == 8U &&
                   offsetof(nx_gd32f303_uart_registers_t, CTL0) == 12U &&
                   offsetof(nx_gd32f303_uart_registers_t, CTL1) == 16U &&
                   offsetof(nx_gd32f303_uart_registers_t, CTL2) == 20U,
               "USART legacy layout");
_Static_assert(USART_CTL2_DENT == (1U << 7U) && USART_CTL2_DENR == (1U << 6U) &&
                   USART_STAT0_TC == (1U << 6U) &&
                   USART_STAT0_IDLEF == (1U << 4U),
               "USART legacy source masks");
_Static_assert(DMA_CHXCTL_CHEN == 1U && DMA_CHXCTL_FTFIE == 2U &&
                   DMA_CHXCTL_HTFIE == 4U && DMA_CHXCTL_ERRIE == 8U &&
                   DMA_CHXCTL_DIR == 16U && DMA_CHXCTL_MNAGA == 128U &&
                   DMA_INTF_FTFIF == 2U && DMA_INTF_ERRIF == 8U,
               "DMA legacy channel masks");
static const IRQn_Type s_irqs[] = {USART0_IRQn, DMA0_Channel3_IRQn,
                                   DMA0_Channel4_IRQn};
nx_result_t nx_gd32f303_uart_dma_stop_irqs(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i) {
        nx_result_t current = NX_GD303_IRQ_CONTROL(s_irqs[i], false, 5U);
        if (current != NX_SUCCESS)
            result = current;
    }
    return result;
}
nx_result_t nx_gd32f303_uart_dma_validate_wake(void* context,
                                               const nx_irq_wake_t* wake,
                                               uint8_t ceiling) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s->config;
    if (ceiling != config->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i) {
        nx_irq_source_t source = {
            (int16_t)s_irqs[i],
            s->initialized ? (uint8_t)NX_GD303_IRQ_PRIORITY(s_irqs[i]) : 5U,
            (uint8_t)NX_GD303_IRQ_GROUP()};
        nx_result_t result =
            nx_irq_wake_validate(wake, &config->irq_policy, &source);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
static nx_result_t preflight(const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (config == NULL)
        return NX_ERROR_INVALID;
    if (config->registers != (nx_uart_v1_registers_t*)(uintptr_t)USART0 ||
        config->tx != (nx_uart_v1_channel_t*)(uintptr_t)&DMA_CH3CTL(DMA0) ||
        (config->profile == NX_UART_RX_BLOCKS &&
         (config->receive == NULL ||
          config->rx != (nx_uart_v1_channel_t*)(uintptr_t)&DMA_CH4CTL(DMA0))) ||
        config->flags != &DMA_INTF(DMA0) || config->clear != &DMA_INTC(DMA0) ||
        config->tx_shift != 12U || config->rx_shift != 16U)
        return NX_ERROR_UNSUPPORTED;
    if (!g_nx_gd32f303_system.started ||
        g_nx_gd32f303_system.core_hz != 8000000U ||
        (NX_GD303_READ(&RCU_AHBEN) & RCU_AHBEN_DMA0EN) == 0U ||
        (NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_USART0EN) == 0U)
        return NX_ERROR_STATE;
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i)
        if (!NX_GD303_IRQ_AVAILABLE(s_irqs[i]))
            return NX_ERROR_BUSY;
    if (config->stop_irqs != nx_gd32f303_uart_dma_stop_irqs ||
        config->validate_wake != nx_gd32f303_uart_dma_validate_wake)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}
static nx_result_t enable_irqs(const nx_uart_v1_dma_config_t* config) {
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i) {
        nx_result_t result = NX_GD303_IRQ_CONTROL(s_irqs[i], true, 5U);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
nx_result_t
nx_gd32f303_uart_dma_initialize(nx_gd32f303_uart_dma_state_t* s,
                                const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (s == NULL || config == NULL || config->profile != NX_UART_RX_BYTES)
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(s))
        return NX_ERROR_BUSY;
    nx_result_t result = preflight(config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_uart_v1_channel_dma_tx_initialize(s, config, 8000000U);
    if (result != NX_SUCCESS)
        return result;
    result = enable_irqs(config);
    if (result != NX_SUCCESS && s->retained)
        (void)nx_uart_v1_channel_dma_tx_ops.stop(s);
    return result;
}
nx_result_t
nx_gd32f303_uart_duplex_initialize(nx_gd32f303_uart_duplex_state_t* state,
                                   const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL ||
        config->profile != NX_UART_RX_BLOCKS ||
        config->receive != &state->receive)
        return NX_ERROR_INVALID;
    nx_uart_v1_dma_state_t* s = &state->uart;
    if (!nx_uart_v1_channel_dma_cold(s))
        return NX_ERROR_BUSY;
    nx_result_t result = preflight(config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_uart_v1_channel_dma_initialize(s, config, 8000000U);
    if (result != NX_SUCCESS)
        return result;
    result = enable_irqs(config);
    if (result != NX_SUCCESS && s->retained)
        (void)nx_uart_v1_channel_dma_ops.stop(s);
    return result;
}
nx_result_t
nx_gd32f303_uart_dma_prepare(nx_gd32f303_uart_dma_state_t* state,
                             const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || config->profile != NX_UART_RX_BYTES)
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(state))
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32f303_usart0_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    return nx_gd32f303_uart_dma_initialize(state, config);
}
nx_result_t
nx_gd32f303_uart_duplex_prepare(nx_gd32f303_uart_duplex_state_t* state,
                                const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL ||
        config->profile != NX_UART_RX_BLOCKS ||
        config->receive != &state->receive)
        return NX_ERROR_INVALID;
    /* The candidate extension can hold a borrow before config is bound. */
    if (!nx_uart_v1_channel_dma_cold(&state->uart) ||
        state->receive.stream != NULL || state->receive.filling)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32f303_usart0_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    return nx_gd32f303_uart_duplex_initialize(state, config);
}
