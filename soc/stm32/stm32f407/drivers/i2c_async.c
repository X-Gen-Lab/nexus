/**
 * \file            i2c_async.c
 * \brief           Exact SDK-checked I2C V1 routes and publisher lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "stm32f407_i2c_async.h"
#include "stm32f407_i2c_cold.h"
#include <stddef.h>

nx_result_t nx_stm32_i2c_async_prepare(nx_i2c_v1_state_t* port,
                                       const nx_stm32_i2c_config_t* config,
                                       nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        !config->asynchronous)
        return NX_ERROR_INVALID;
    if (port->registers != NULL || port->active != NULL || port->initialized ||
        port->wake != NULL || port->wake_publishers != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_i2c_cold_acquire(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    port->registers = (volatile uint32_t*)config->registers;
    port->line_inputs = &GPIOB->IDR;
    port->line_mask = (1U << config->pins[0].pin) | (1U << config->pins[1].pin);
    port->peripheral_mhz = (uint8_t)(config->clock_hz / 1000000U);
    port->dma = config->dma;
    result = nx_stm32_i2c_async_initialize(port, config->priority);
    if (result == NX_SUCCESS) {
        receipt->prepared = true;
        result = nx_stm32_i2c_cold_verify(config, receipt);
        if (result != NX_SUCCESS)
            (void)nx_stm32_i2c_async_close(port, config, receipt);
    } else if (nx_stm32_i2c_cold_release(config, receipt) == NX_SUCCESS) {
        *port = (nx_i2c_v1_state_t){0};
    }
    return result;
}

nx_result_t nx_stm32_i2c_async_close(nx_i2c_v1_state_t* port,
                                     const nx_stm32_i2c_config_t* config,
                                     nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        !config->asynchronous)
        return NX_ERROR_INVALID;
    if (receipt->prepared) {
        if (port->registers != (volatile uint32_t*)config->registers)
            return NX_ERROR_INVALID;
        nx_result_t result = nx_i2c_v1_async_ops.stop(port);
        if (result != NX_SUCCESS)
            return result;
        receipt->prepared = false;
    }
    nx_result_t result = nx_stm32_i2c_cold_release(config, receipt);
    if (result == NX_SUCCESS)
        *port = (nx_i2c_v1_state_t){0};
    return result;
}

_Static_assert(offsetof(I2C_TypeDef, CR1) == 0U * sizeof(uint32_t),
               "I2C CR1 layout");
_Static_assert(offsetof(I2C_TypeDef, CR2) == 1U * sizeof(uint32_t),
               "I2C CR2 layout");
_Static_assert(offsetof(I2C_TypeDef, OAR1) == 2U * sizeof(uint32_t),
               "I2C OAR1 layout");
_Static_assert(offsetof(I2C_TypeDef, DR) == 4U * sizeof(uint32_t),
               "I2C DR layout");
_Static_assert(offsetof(I2C_TypeDef, SR1) == 5U * sizeof(uint32_t),
               "I2C SR1 layout");
_Static_assert(offsetof(I2C_TypeDef, SR2) == 6U * sizeof(uint32_t),
               "I2C SR2 layout");
_Static_assert(offsetof(I2C_TypeDef, CCR) == 7U * sizeof(uint32_t),
               "I2C CCR layout");
_Static_assert(offsetof(I2C_TypeDef, TRISE) == 8U * sizeof(uint32_t),
               "I2C TRISE layout");
_Static_assert(I2C_CR1_PE == (UINT32_C(1) << 0U), "I2C/DMA I2C_CR1_PE mask");
_Static_assert(I2C_CR1_START == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_CR1_START mask");
_Static_assert(I2C_CR1_STOP == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_CR1_STOP mask");
_Static_assert(I2C_CR1_ACK == (UINT32_C(1) << 10U), "I2C/DMA I2C_CR1_ACK mask");
_Static_assert(I2C_CR1_POS == (UINT32_C(1) << 11U), "I2C/DMA I2C_CR1_POS mask");
_Static_assert(I2C_CR1_SWRST == (UINT32_C(1) << 15U),
               "I2C/DMA I2C_CR1_SWRST mask");
_Static_assert(I2C_CR2_ITERREN == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_CR2_ITERREN mask");
_Static_assert(I2C_CR2_ITEVTEN == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_CR2_ITEVTEN mask");
_Static_assert(I2C_CR2_ITBUFEN == (UINT32_C(1) << 10U),
               "I2C/DMA I2C_CR2_ITBUFEN mask");
_Static_assert(I2C_CR2_DMAEN == (UINT32_C(1) << 11U),
               "I2C/DMA I2C_CR2_DMAEN mask");
_Static_assert(I2C_CR2_LAST == (UINT32_C(1) << 12U),
               "I2C/DMA I2C_CR2_LAST mask");
_Static_assert(I2C_SR1_SB == (UINT32_C(1) << 0U), "I2C/DMA I2C_SR1_SB mask");
_Static_assert(I2C_SR1_ADDR == (UINT32_C(1) << 1U),
               "I2C/DMA I2C_SR1_ADDR mask");
_Static_assert(I2C_SR1_BTF == (UINT32_C(1) << 2U), "I2C/DMA I2C_SR1_BTF mask");
_Static_assert(I2C_SR1_RXNE == (UINT32_C(1) << 6U),
               "I2C/DMA I2C_SR1_RXNE mask");
_Static_assert(I2C_SR1_TXE == (UINT32_C(1) << 7U), "I2C/DMA I2C_SR1_TXE mask");
_Static_assert(I2C_SR1_BERR == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_SR1_BERR mask");
_Static_assert(I2C_SR1_ARLO == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_SR1_ARLO mask");
_Static_assert(I2C_SR1_AF == (UINT32_C(1) << 10U), "I2C/DMA I2C_SR1_AF mask");
_Static_assert(I2C_SR1_OVR == (UINT32_C(1) << 11U), "I2C/DMA I2C_SR1_OVR mask");
_Static_assert(I2C_SR2_MSL == (UINT32_C(1) << 0U), "I2C/DMA I2C_SR2_MSL mask");
_Static_assert(I2C_SR2_BUSY == (UINT32_C(1) << 1U),
               "I2C/DMA I2C_SR2_BUSY mask");
_Static_assert(DMA_SxCR_EN == (UINT32_C(1) << 0U), "I2C/DMA DMA_SxCR_EN mask");
_Static_assert(DMA_SxCR_DMEIE == (UINT32_C(1) << 1U),
               "I2C/DMA DMA_SxCR_DMEIE mask");
_Static_assert(DMA_SxCR_TEIE == (UINT32_C(1) << 2U),
               "I2C/DMA DMA_SxCR_TEIE mask");
_Static_assert(DMA_SxCR_HTIE == (UINT32_C(1) << 3U),
               "I2C/DMA DMA_SxCR_HTIE mask");
_Static_assert(DMA_SxCR_TCIE == (UINT32_C(1) << 4U),
               "I2C/DMA DMA_SxCR_TCIE mask");
_Static_assert(DMA_SxCR_MINC == (UINT32_C(1) << 10U),
               "I2C/DMA DMA_SxCR_MINC mask");

/** \brief Cold publisher facts are selected by the exact retained controller.
 */
static bool first_controller(const nx_i2c_v1_state_t* state) {
    return state->registers == (volatile uint32_t*)(uintptr_t)I2C1;
}
/** \brief No shared vector registry or per-byte interrupt indirection. */
static IRQn_Type event_irq(const nx_i2c_v1_state_t* state) {
    return first_controller(state) ? I2C1_EV_IRQn : I2C2_EV_IRQn;
}
/** \brief Error source belongs to the same retained controller identity. */
static IRQn_Type error_irq(const nx_i2c_v1_state_t* state) {
    return first_controller(state) ? I2C1_ER_IRQn : I2C2_ER_IRQn;
}
/** \brief DMA stream ownership and request selectors remain distinct facts. */
static IRQn_Type dma_irq(const nx_i2c_v1_state_t* state, bool receive) {
    if (first_controller(state)) {
        return receive ? DMA1_Stream0_IRQn : DMA1_Stream6_IRQn;
    }
    return receive ? DMA1_Stream2_IRQn : DMA1_Stream7_IRQn;
}

/** \brief Clear only pending publishers owned by this selected controller. */
static void acknowledge(void* context) {
    const nx_i2c_v1_state_t* state = context;
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(event_irq(state));
    NVIC_ClearPendingIRQ(error_irq(state));
    if (state->dma != NULL) {
        NVIC_ClearPendingIRQ(dma_irq(state, true));
        NVIC_ClearPendingIRQ(dma_irq(state, false));
    }
#else
    (void)state;
#endif
}

/** \brief Disabled NVIC sources and pending detach precede sink release. */
static nx_result_t stop_irqs(void* context) {
    const nx_i2c_v1_state_t* state = context;
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(event_irq(state));
    NVIC_DisableIRQ(error_irq(state));
    if (state->dma != NULL) {
        NVIC_DisableIRQ(dma_irq(state, true));
        NVIC_DisableIRQ(dma_irq(state, false));
    }
#else
    (void)state;
#endif
    acknowledge(context);
#ifndef NEXUS_STM32_MODEL
    if (NVIC_GetEnableIRQ(event_irq(state)) != 0U ||
        NVIC_GetEnableIRQ(error_irq(state)) != 0U ||
        NVIC_GetPendingIRQ(event_irq(state)) != 0U ||
        NVIC_GetPendingIRQ(error_irq(state)) != 0U ||
        (state->dma != NULL &&
         (NVIC_GetEnableIRQ(dma_irq(state, true)) != 0U ||
          NVIC_GetEnableIRQ(dma_irq(state, false)) != 0U ||
          NVIC_GetPendingIRQ(dma_irq(state, true)) != 0U ||
          NVIC_GetPendingIRQ(dma_irq(state, false)) != 0U))) {
        return NX_ERROR_IO;
    }
#endif
    return NX_SUCCESS;
}

/** \brief Validate every possible publisher using actual selected IRQ policy.
 */
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    const nx_i2c_v1_state_t* state = context;
    nx_result_t result =
        nx_stm32_irq_wake_validate(wake, event_irq(state), ceiling);
    if (result == NX_SUCCESS) {
        result = nx_stm32_irq_wake_validate(wake, error_irq(state), ceiling);
    }
    if (result == NX_SUCCESS && state->dma != NULL) {
        result =
            nx_stm32_irq_wake_validate(wake, dma_irq(state, true), ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
        result =
            nx_stm32_irq_wake_validate(wake, dma_irq(state, false), ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return result;
}

/** \brief Immutable routes match every physical register and selector fact. */
static bool same_engine(const nx_i2c_v1_dma_engine_t* actual,
                        const nx_i2c_v1_dma_engine_t* expected) {
    return actual->control == expected->control &&
           actual->count == expected->count &&
           actual->peripheral == expected->peripheral &&
           actual->memory == expected->memory &&
           actual->fifo == expected->fifo && actual->flags == expected->flags &&
           actual->clear == expected->clear &&
           actual->configuration == expected->configuration &&
           actual->flag_shift == expected->flag_shift;
}

/** \brief Cold physical validation has no descriptor, clock or pin side
 * effects. */
nx_result_t nx_stm32_i2c_async_initialize(nx_i2c_v1_state_t* state,
                                          uint8_t priority) {
    if (state == NULL || priority > 15U) {
        return NX_ERROR_INVALID;
    }
    if ((state->registers != (volatile uint32_t*)I2C1 &&
         state->registers != (volatile uint32_t*)I2C2) ||
        state->peripheral_mhz != 42U) {
        return NX_ERROR_UNSUPPORTED;
    }
    bool known_lines = state->line_inputs == &GPIOB->IDR &&
                       (first_controller(state)
                            ? (state->line_mask == ((1U << 6U) | (1U << 7U)) ||
                               state->line_mask == ((1U << 8U) | (1U << 9U)))
                            : state->line_mask == ((1U << 10U) | (1U << 11U)));
    if (!known_lines) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->dma != NULL) {
        const nx_i2c_v1_dma_t expected =
            first_controller(state)
                ? (nx_i2c_v1_dma_t)NX_STM32_I2C1_DMA(NULL, 0U)
                : (nx_i2c_v1_dma_t)NX_STM32_I2C2_DMA(NULL, 0U);
        if (!same_engine(&state->dma->tx, &expected.tx) ||
            !same_engine(&state->dma->rx, &expected.rx)) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    /* A second private object cannot reinitialize an already enabled
     * controller. Exact clock/pin/vector ownership still belongs to static cold
     * assembly. */
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->registers[0] & 1U) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_i2c_v1_initialize(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->ack_irqs = acknowledge;
    state->stop_irqs = stop_irqs;
    state->validate_wake = validate_wake;
    acknowledge(state);
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(event_irq(state), priority);
    NVIC_EnableIRQ(event_irq(state));
    NVIC_SetPriority(error_irq(state), priority);
    NVIC_EnableIRQ(error_irq(state));
    if (state->dma != NULL) {
        NVIC_SetPriority(dma_irq(state, true), priority);
        NVIC_EnableIRQ(dma_irq(state, true));
        NVIC_SetPriority(dma_irq(state, false), priority);
        NVIC_EnableIRQ(dma_irq(state, false));
    }
#endif
    return NX_SUCCESS;
}
