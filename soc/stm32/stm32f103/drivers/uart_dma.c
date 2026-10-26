/**
 * \file            uart_dma.c
 * \brief           Exact F103 USART2 and DMA1 channel7 TX/channel6 RX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_transport.h"
#include <stddef.h>
_Static_assert(offsetof(USART_TypeDef, SR) == 0U, "USART SR layout");
_Static_assert(offsetof(USART_TypeDef, DR) == 4U, "USART DR layout");
_Static_assert(offsetof(USART_TypeDef, BRR) == 8U, "USART BRR layout");
_Static_assert(offsetof(USART_TypeDef, CR1) == 12U, "USART CR1 layout");
_Static_assert(offsetof(USART_TypeDef, CR2) == 16U, "USART CR2 layout");
_Static_assert(offsetof(USART_TypeDef, CR3) == 20U, "USART CR3 layout");
_Static_assert(offsetof(DMA_Channel_TypeDef, CCR) == 0U, "DMA CCR layout");
_Static_assert(offsetof(DMA_Channel_TypeDef, CNDTR) == 4U, "DMA CNDTR layout");
_Static_assert(offsetof(DMA_Channel_TypeDef, CPAR) == 8U, "DMA CPAR layout");
_Static_assert(offsetof(DMA_Channel_TypeDef, CMAR) == 12U, "DMA CMAR layout");
_Static_assert(USART_CR3_DMAT == (1U << 7U) && USART_CR3_DMAR == (1U << 6U) &&
                   USART_SR_TC == (1U << 6U) && USART_SR_IDLE == (1U << 4U),
               "USART legacy source masks");
_Static_assert(DMA_CCR_EN == 1U && DMA_CCR_TCIE == 2U && DMA_CCR_HTIE == 4U &&
                   DMA_CCR_TEIE == 8U && DMA_CCR_DIR == 16U &&
                   DMA_CCR_MINC == 128U && DMA_ISR_TCIF7 == (2U << 24U) &&
                   DMA_ISR_TEIF6 == (8U << 20U),
               "DMA legacy channel masks");
static const IRQn_Type s_irqs[] = {USART2_IRQn, DMA1_Channel7_IRQn,
                                   DMA1_Channel6_IRQn};
nx_result_t nx_stm32f103_uart_dma_stop_irqs(void* context) {
    nx_uart_v1_dma_state_t* s = context;
    const nx_uart_v1_dma_config_t* config = s->config;
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i) {
        nx_result_t current = NX_F103_IRQ_CONTROL(s_irqs[i], false, 5U);
        if (current != NX_SUCCESS)
            result = current;
    }
    return result;
}
nx_result_t nx_stm32f103_uart_dma_validate_wake(void* context,
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
            s->initialized ? (uint8_t)NX_F103_IRQ_PRIORITY(s_irqs[i]) : 5U,
            (uint8_t)NX_F103_IRQ_GROUP()};
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
    if (config->registers != (nx_uart_v1_registers_t*)USART2 ||
        config->tx != (nx_uart_v1_channel_t*)DMA1_Channel7 ||
        (config->profile == NX_UART_RX_BLOCKS &&
         (config->receive == NULL ||
          config->rx != (nx_uart_v1_channel_t*)DMA1_Channel6)) ||
        config->flags != &DMA1->ISR || config->clear != &DMA1->IFCR ||
        config->tx_shift != 24U || config->rx_shift != 20U)
        return NX_ERROR_UNSUPPORTED;
    if (!g_nx_stm32f103_system.started ||
        g_nx_stm32f103_system.core_hz != 8000000U ||
        (NX_F103_READ(&RCC->AHBENR) & RCC_AHBENR_DMA1EN) == 0U ||
        (NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_USART2EN) == 0U)
        return NX_ERROR_STATE;
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i)
        if (!NX_F103_IRQ_AVAILABLE(s_irqs[i]))
            return NX_ERROR_BUSY;
    if (config->stop_irqs != nx_stm32f103_uart_dma_stop_irqs ||
        config->validate_wake != nx_stm32f103_uart_dma_validate_wake)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}
static nx_result_t enable_irqs(const nx_uart_v1_dma_config_t* config) {
    for (size_t i = 0U; i < (config->profile == NX_UART_RX_BLOCKS ? 3U : 2U);
         ++i) {
        nx_result_t result = NX_F103_IRQ_CONTROL(s_irqs[i], true, 5U);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
nx_result_t
nx_stm32f103_uart_dma_initialize(nx_stm32f103_uart_dma_state_t* s,
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
nx_stm32f103_uart_duplex_initialize(nx_stm32f103_uart_duplex_state_t* state,
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
nx_stm32f103_uart_dma_prepare(nx_stm32f103_uart_dma_state_t* state,
                              const nx_uart_v1_dma_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || config->profile != NX_UART_RX_BYTES)
        return NX_ERROR_INVALID;
    if (!nx_uart_v1_channel_dma_cold(state))
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32f103_usart2_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32f103_uart_dma_initialize(state, config);
}
nx_result_t
nx_stm32f103_uart_duplex_prepare(nx_stm32f103_uart_duplex_state_t* state,
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
    nx_result_t result = nx_stm32f103_usart2_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32f103_uart_duplex_initialize(state, config);
}
