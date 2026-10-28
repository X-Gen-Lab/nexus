/**
 * \file            i2c_async.c
 * \brief           Exact SDK-checked I2C V1 routes and publisher lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_i2c_async.h"
#include "gd32f470_i2c_cold.h"
#include "gd32f4xx_dma.h"
#include "nexus/arch/arch.h"
#include <stddef.h>

nx_result_t nx_gd32_i2c_async_prepare(nx_i2c_v1_state_t* port,
                                      const nx_gd32_i2c_config_t* config,
                                      nx_gd32_i2c_cold_t* cold) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || cold == NULL || !config->asynchronous)
        return NX_ERROR_INVALID;
    if (port->registers != NULL || port->active != NULL || port->initialized ||
        port->wake != NULL || port->wake_publishers != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_gd32_i2c_cold_acquire(config, cold);
    if (result != NX_SUCCESS)
        return result;
    port->registers = (volatile uint32_t*)(uintptr_t)config->registers;
    port->line_inputs = &GPIO_ISTAT(GPIOB);
    port->line_mask = (1U << config->pins[0].pin) | (1U << config->pins[1].pin);
    port->peripheral_mhz = 50U;
    port->dma = config->dma;
    result = nx_gd32_i2c_async_initialize(port, config->priority);
    if (result == NX_SUCCESS) {
        cold->prepared = true;
        result = nx_gd32_i2c_cold_commit(config, cold);
    } else if (nx_gd32_i2c_cold_release(config, cold) == NX_SUCCESS) {
        *port = (nx_i2c_v1_state_t){0};
    }
    return result;
}

nx_result_t nx_gd32_i2c_async_close(nx_i2c_v1_state_t* port,
                                    const nx_gd32_i2c_config_t* config,
                                    nx_gd32_i2c_cold_t* cold) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || cold == NULL || !config->asynchronous)
        return NX_ERROR_INVALID;
    if (cold->prepared) {
        if (port->registers != (volatile uint32_t*)(uintptr_t)config->registers)
            return NX_ERROR_INVALID;
        nx_result_t result = nx_i2c_v1_async_ops.stop(port);
        if (result != NX_SUCCESS)
            return result;
        cold->prepared = false;
    }
    nx_result_t result = nx_gd32_i2c_cold_release(config, cold);
    if (result == NX_SUCCESS)
        *port = (nx_i2c_v1_state_t){0};
    return result;
}

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

/** \brief Cold publisher facts are selected by the exact retained controller.
 */
static bool first_controller(const nx_i2c_v1_state_t* state) {
    return state->registers == (volatile uint32_t*)(uintptr_t)I2C0;
}
/** \brief No shared vector registry or per-byte interrupt indirection. */
static IRQn_Type event_irq(const nx_i2c_v1_state_t* state) {
    return first_controller(state) ? I2C0_EV_IRQn : I2C1_EV_IRQn;
}
/** \brief Error source belongs to the same retained controller identity. */
static IRQn_Type error_irq(const nx_i2c_v1_state_t* state) {
    return first_controller(state) ? I2C0_ER_IRQn : I2C1_ER_IRQn;
}
/** \brief DMA stream ownership and request selectors remain distinct facts. */
static IRQn_Type dma_irq(const nx_i2c_v1_state_t* state, bool receive) {
    if (first_controller(state)) {
        return receive ? DMA0_Channel0_IRQn : DMA0_Channel6_IRQn;
    }
    return receive ? DMA0_Channel2_IRQn : DMA0_Channel7_IRQn;
}

/** \brief Clear only pending publishers owned by this selected controller. */
static void acknowledge(void* context) {
    const nx_i2c_v1_state_t* state = context;
    NVIC_ClearPendingIRQ(event_irq(state));
    NVIC_ClearPendingIRQ(error_irq(state));
    if (state->dma != NULL) {
        NVIC_ClearPendingIRQ(dma_irq(state, true));
        NVIC_ClearPendingIRQ(dma_irq(state, false));
    }
}

/** \brief Disabled NVIC sources and pending detach precede sink release. */
static bool vector_idle(IRQn_Type irq) {
    uint32_t index = (uint32_t)irq >> 5U;
    uint32_t bit = UINT32_C(1) << ((uint32_t)irq & 31U);
    return ((NVIC->ISER[index] | NVIC->ISPR[index]) & bit) == 0U;
}
static nx_result_t stop_irqs(void* context) {
    const nx_i2c_v1_state_t* state = context;
    NVIC_DisableIRQ(event_irq(state));
    NVIC_DisableIRQ(error_irq(state));
    if (state->dma != NULL) {
        NVIC_DisableIRQ(dma_irq(state, true));
        NVIC_DisableIRQ(dma_irq(state, false));
    }
    acknowledge(context);
    if (!vector_idle(event_irq(state)) || !vector_idle(error_irq(state)) ||
        (state->dma != NULL && (!vector_idle(dma_irq(state, true)) ||
                                !vector_idle(dma_irq(state, false))))) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Validate every possible publisher using actual selected IRQ policy.
 */
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    const nx_i2c_v1_state_t* state = context;
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, event_irq(state), ceiling);
    if (result == NX_SUCCESS) {
        result = nx_gd32_irq_wake_validate(wake, error_irq(state), ceiling);
    }
    if (result == NX_SUCCESS && state->dma != NULL) {
        result = nx_gd32_irq_wake_validate(wake, dma_irq(state, true), ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
        result =
            nx_gd32_irq_wake_validate(wake, dma_irq(state, false), ceiling);
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
    if ((state->registers != (volatile uint32_t*)(uintptr_t)I2C0 &&
         state->registers != (volatile uint32_t*)(uintptr_t)I2C1) ||
        state->peripheral_mhz != 50U || (uintptr_t)&I2C_CTL0(I2C0) != I2C0 ||
        (uintptr_t)&I2C_SADDR0(I2C0) != I2C0 + 8U ||
        (uintptr_t)&I2C_DATA(I2C0) != I2C0 + 16U ||
        (uintptr_t)&I2C_CKCFG(I2C0) != I2C0 + 28U ||
        (uintptr_t)&I2C_RT(I2C0) != I2C0 + 32U) {
        return NX_ERROR_UNSUPPORTED;
    }
    bool known_lines = state->line_inputs == &GPIO_ISTAT(GPIOB) &&
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
                ? (nx_i2c_v1_dma_t)NX_GD32_I2C0_DMA(NULL, 0U)
                : (nx_i2c_v1_dma_t)NX_GD32_I2C1_DMA(NULL, 0U);
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
    return NX_SUCCESS;
}
