/**
 * \file            i2c_async.c
 * \brief           Exact SDK-checked I2C V1 routes and publisher lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_i2c_async.h"
#include "gd32f4xx_dma.h"
#include "nexus/arch/arch.h"
#include <stddef.h>

_Static_assert(I2C_CTL1_REG_OFFSET == 1U * sizeof(uint32_t),
               "I2C I2C_CTL1_REG_OFFSET layout");
_Static_assert(I2C_STAT0_REG_OFFSET == 5U * sizeof(uint32_t),
               "I2C I2C_STAT0_REG_OFFSET layout");
_Static_assert(I2C_STAT1_REG_OFFSET == 6U * sizeof(uint32_t),
               "I2C I2C_STAT1_REG_OFFSET layout");
_Static_assert(I2C_CTL0_I2CEN == (UINT32_C(1) << 0U),
               "I2C/DMA I2C_CTL0_I2CEN mask");
_Static_assert(I2C_CTL0_START == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_CTL0_START mask");
_Static_assert(I2C_CTL0_STOP == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_CTL0_STOP mask");
_Static_assert(I2C_CTL0_ACKEN == (UINT32_C(1) << 10U),
               "I2C/DMA I2C_CTL0_ACKEN mask");
_Static_assert(I2C_CTL0_POAP == (UINT32_C(1) << 11U),
               "I2C/DMA I2C_CTL0_POAP mask");
_Static_assert(I2C_CTL0_SRESET == (UINT32_C(1) << 15U),
               "I2C/DMA I2C_CTL0_SRESET mask");
_Static_assert(I2C_CTL1_ERRIE == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_CTL1_ERRIE mask");
_Static_assert(I2C_CTL1_EVIE == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_CTL1_EVIE mask");
_Static_assert(I2C_CTL1_BUFIE == (UINT32_C(1) << 10U),
               "I2C/DMA I2C_CTL1_BUFIE mask");
_Static_assert(I2C_CTL1_DMAON == (UINT32_C(1) << 11U),
               "I2C/DMA I2C_CTL1_DMAON mask");
_Static_assert(I2C_CTL1_DMALST == (UINT32_C(1) << 12U),
               "I2C/DMA I2C_CTL1_DMALST mask");
_Static_assert(I2C_STAT0_SBSEND == (UINT32_C(1) << 0U),
               "I2C/DMA I2C_STAT0_SBSEND mask");
_Static_assert(I2C_STAT0_ADDSEND == (UINT32_C(1) << 1U),
               "I2C/DMA I2C_STAT0_ADDSEND mask");
_Static_assert(I2C_STAT0_BTC == (UINT32_C(1) << 2U),
               "I2C/DMA I2C_STAT0_BTC mask");
_Static_assert(I2C_STAT0_RBNE == (UINT32_C(1) << 6U),
               "I2C/DMA I2C_STAT0_RBNE mask");
_Static_assert(I2C_STAT0_TBE == (UINT32_C(1) << 7U),
               "I2C/DMA I2C_STAT0_TBE mask");
_Static_assert(I2C_STAT0_BERR == (UINT32_C(1) << 8U),
               "I2C/DMA I2C_STAT0_BERR mask");
_Static_assert(I2C_STAT0_LOSTARB == (UINT32_C(1) << 9U),
               "I2C/DMA I2C_STAT0_LOSTARB mask");
_Static_assert(I2C_STAT0_AERR == (UINT32_C(1) << 10U),
               "I2C/DMA I2C_STAT0_AERR mask");
_Static_assert(I2C_STAT0_OUERR == (UINT32_C(1) << 11U),
               "I2C/DMA I2C_STAT0_OUERR mask");
_Static_assert(I2C_STAT1_MASTER == (UINT32_C(1) << 0U),
               "I2C/DMA I2C_STAT1_MASTER mask");
_Static_assert(I2C_STAT1_I2CBSY == (UINT32_C(1) << 1U),
               "I2C/DMA I2C_STAT1_I2CBSY mask");
_Static_assert(DMA_CHXCTL_CHEN == (UINT32_C(1) << 0U),
               "I2C/DMA DMA_CHXCTL_CHEN mask");
_Static_assert(DMA_CHXCTL_SDEIE == (UINT32_C(1) << 1U),
               "I2C/DMA DMA_CHXCTL_SDEIE mask");
_Static_assert(DMA_CHXCTL_TAEIE == (UINT32_C(1) << 2U),
               "I2C/DMA DMA_CHXCTL_TAEIE mask");
_Static_assert(DMA_CHXCTL_HTFIE == (UINT32_C(1) << 3U),
               "I2C/DMA DMA_CHXCTL_HTFIE mask");
_Static_assert(DMA_CHXCTL_FTFIE == (UINT32_C(1) << 4U),
               "I2C/DMA DMA_CHXCTL_FTFIE mask");
_Static_assert(DMA_CHXCTL_MNAGA == (UINT32_C(1) << 10U),
               "I2C/DMA DMA_CHXCTL_MNAGA mask");

/** \brief Clear only pending publishers owned by this selected controller. */
static void acknowledge(void* context) {
    const nx_i2c_v1_state_t* state = context;
    NVIC_ClearPendingIRQ(I2C0_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C0_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_ClearPendingIRQ(DMA0_Channel0_IRQn);
        NVIC_ClearPendingIRQ(DMA0_Channel6_IRQn);
    }
}

/** \brief Disabled NVIC sources and pending detach precede sink release. */
static void stop_irqs(void* context) {
    const nx_i2c_v1_state_t* state = context;
    NVIC_DisableIRQ(I2C0_EV_IRQn);
    NVIC_DisableIRQ(I2C0_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_DisableIRQ(DMA0_Channel0_IRQn);
        NVIC_DisableIRQ(DMA0_Channel6_IRQn);
    }
    acknowledge(context);
}

/** \brief Validate every possible publisher using actual selected IRQ policy.
 */
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    const nx_i2c_v1_state_t* state = context;
    nx_result_t result = nx_gd32_irq_wake_validate(wake, I2C0_EV_IRQn, ceiling);
    if (result == NX_SUCCESS) {
        result = nx_gd32_irq_wake_validate(wake, I2C0_ER_IRQn, ceiling);
    }
    if (result == NX_SUCCESS && state->dma != NULL) {
        result = nx_gd32_irq_wake_validate(wake, DMA0_Channel0_IRQn, ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
        result = nx_gd32_irq_wake_validate(wake, DMA0_Channel6_IRQn, ceiling);
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
nx_result_t nx_gd32_i2c_async_initialize(nx_i2c_v1_state_t* state,
                                         uint8_t priority) {
    if (state == NULL || priority > 15U) {
        return NX_ERROR_INVALID;
    }
    if (state->registers != (volatile uint32_t*)(uintptr_t)I2C0 ||
        state->peripheral_mhz != 50U || (uintptr_t)&I2C_CTL0(I2C0) != I2C0 ||
        (uintptr_t)&I2C_SADDR0(I2C0) != I2C0 + 8U ||
        (uintptr_t)&I2C_DATA(I2C0) != I2C0 + 16U ||
        (uintptr_t)&I2C_CKCFG(I2C0) != I2C0 + 28U ||
        (uintptr_t)&I2C_RT(I2C0) != I2C0 + 32U) {
        return NX_ERROR_UNSUPPORTED;
    }
    bool known_lines = state->line_inputs == &GPIO_ISTAT(GPIOB) &&
                       (state->line_mask == ((1U << 6U) | (1U << 7U)) ||
                        state->line_mask == ((1U << 8U) | (1U << 9U)));
    if (!known_lines) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->dma != NULL) {
        const nx_i2c_v1_dma_t expected = NX_GD32_I2C0_DMA(NULL, 0U);
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
    NVIC_SetPriority(I2C0_EV_IRQn, priority);
    NVIC_EnableIRQ(I2C0_EV_IRQn);
    NVIC_SetPriority(I2C0_ER_IRQn, priority);
    NVIC_EnableIRQ(I2C0_ER_IRQn);
    if (state->dma != NULL) {
        NVIC_SetPriority(DMA0_Channel0_IRQn, priority);
        NVIC_EnableIRQ(DMA0_Channel0_IRQn);
        NVIC_SetPriority(DMA0_Channel6_IRQn, priority);
        NVIC_EnableIRQ(DMA0_Channel6_IRQn);
    }
    return NX_SUCCESS;
}
