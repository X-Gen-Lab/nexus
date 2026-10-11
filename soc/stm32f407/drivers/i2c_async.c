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
#include <stddef.h>

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

/** \brief Clear only pending publishers owned by this selected controller. */
static void acknowledge(void* context) {
    const nx_i2c_v1_state_t* state = context;
#ifndef NEXUS_STM32_MODEL
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_ClearPendingIRQ(DMA1_Stream0_IRQn);
        NVIC_ClearPendingIRQ(DMA1_Stream6_IRQn);
    }
#else
    (void)state;
#endif
}

/** \brief Disabled NVIC sources and pending detach precede sink release. */
static void stop_irqs(void* context) {
    const nx_i2c_v1_state_t* state = context;
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_DisableIRQ(DMA1_Stream0_IRQn);
        NVIC_DisableIRQ(DMA1_Stream6_IRQn);
    }
#else
    (void)state;
#endif
    acknowledge(context);
}

/** \brief Validate every possible publisher using actual selected IRQ policy.
 */
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    const nx_i2c_v1_state_t* state = context;
    nx_result_t result =
        nx_stm32_irq_wake_validate(wake, I2C1_EV_IRQn, ceiling);
    if (result == NX_SUCCESS) {
        result = nx_stm32_irq_wake_validate(wake, I2C1_ER_IRQn, ceiling);
    }
    if (result == NX_SUCCESS && state->dma != NULL) {
        result = nx_stm32_irq_wake_validate(wake, DMA1_Stream0_IRQn, ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
        result = nx_stm32_irq_wake_validate(wake, DMA1_Stream6_IRQn, ceiling);
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
    if (state->registers != (volatile uint32_t*)I2C1 ||
        state->peripheral_mhz != 42U) {
        return NX_ERROR_UNSUPPORTED;
    }
    bool known_lines = state->line_inputs == &GPIOB->IDR &&
                       (state->line_mask == ((1U << 6U) | (1U << 7U)) ||
                        state->line_mask == ((1U << 8U) | (1U << 9U)));
    if (!known_lines) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->dma != NULL) {
        const nx_i2c_v1_dma_t expected = NX_STM32_I2C1_DMA(NULL, 0U);
        if (!same_engine(&state->dma->tx, &expected.tx) ||
            !same_engine(&state->dma->rx, &expected.rx)) {
            return NX_ERROR_UNSUPPORTED;
        }
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
    NVIC_SetPriority(I2C1_EV_IRQn, priority);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_SetPriority(I2C1_ER_IRQn, priority);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_SetPriority(DMA1_Stream0_IRQn, priority);
        NVIC_EnableIRQ(DMA1_Stream0_IRQn);
        NVIC_SetPriority(DMA1_Stream6_IRQn, priority);
        NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    }
#endif
    return NX_SUCCESS;
}
