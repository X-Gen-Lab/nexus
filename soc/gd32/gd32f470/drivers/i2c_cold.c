/**
 * \file            i2c_cold.c
 * \brief           Exact GD470 I2C pad, clock and publisher ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_i2c_cold.h"
#include "private/system.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif

static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_gd32_in_isr() &&
           !nx_gd32_irq_masked();
}

static bool config_valid(const nx_gd32_i2c_config_t* config) {
    if (config == NULL || config->pins == NULL || config->snapshots == NULL ||
        config->priority > 15U ||
        (config->registers != I2C0 && config->registers != I2C1) ||
        (!config->asynchronous &&
         (config->dma != NULL || config->address < 8U ||
          config->address > 119U)))
        return false;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &config->pins[i];
        if (pin->registers != GPIOB || pin->alternate != 4U ||
            pin->mode != 2U || pin->pull > 1U || !pin->open_drain)
            return false;
    }
    const uint8_t first = config->pins[0].pin;
    const uint8_t second = config->pins[1].pin;
    return config->registers == I2C0 ? ((first == 6U && second == 7U) ||
                                        (first == 8U && second == 9U))
                                     : (first == 10U && second == 11U);
}

static uint32_t controller_gate(const nx_gd32_i2c_config_t* config) {
    return config->registers == I2C0 ? RCU_APB1EN_I2C0EN : RCU_APB1EN_I2C1EN;
}

static size_t vector_count(const nx_gd32_i2c_config_t* config) {
    return !config->asynchronous ? 0U : config->dma == NULL ? 2U : 4U;
}

static IRQn_Type vector(const nx_gd32_i2c_config_t* config, size_t index) {
    if (config->registers == I2C0) {
        const IRQn_Type irqs[] = {I2C0_EV_IRQn, I2C0_ER_IRQn,
                                  DMA0_Channel0_IRQn, DMA0_Channel6_IRQn};
        return irqs[index];
    }
    const IRQn_Type irqs[] = {I2C1_EV_IRQn, I2C1_ER_IRQn, DMA0_Channel2_IRQn,
                              DMA0_Channel7_IRQn};
    return irqs[index];
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

nx_result_t nx_gd32_i2c_cold_release(const nx_gd32_i2c_config_t* config,
                                     nx_gd32_i2c_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL)
        return NX_ERROR_INVALID;
    if (!cold->owned)
        return NX_SUCCESS;
    if (cold->prepared)
        return NX_ERROR_BUSY;
    if (cold->vectors) {
        for (size_t i = 0U; i < vector_count(config); ++i) {
            const IRQn_Type irq = vector(config, i);
            const uint32_t index = (uint32_t)irq;
            const uint32_t bit = 1U << (index & 31U);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[index >> 5U], bit);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[index >> 5U], bit);
            nx_arch_dsb();
            if (!vector_idle(irq))
                return NX_ERROR_BUSY;
            NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[index], cold->priorities[i]);
            nx_arch_dsb();
            if (NVIC->IPR[index] != cold->priorities[i])
                return NX_ERROR_IO;
        }
        cold->vectors = false;
    }
    if (cold->pins_started) {
        nx_result_t status = nx_gd32_cold_pins_rollback(
            config->pins, 2U, config->snapshots, &cold->pins);
        if (status != NX_SUCCESS)
            return status;
        cold->pins_started = false;
    }
    const uint32_t gate = controller_gate(config);
    NX_PLATFORM_REGISTER_WRITE(RCU_APB1EN,
                               (RCU_APB1EN & ~gate) | cold->peripheral_clock);
    nx_arch_dsb();
    if ((RCU_APB1EN & gate) != cold->peripheral_clock)
        return NX_ERROR_IO;
    *cold = (nx_gd32_i2c_cold_t){0};
    return NX_SUCCESS;
}

nx_result_t nx_gd32_i2c_cold_acquire(const nx_gd32_i2c_config_t* config,
                                     nx_gd32_i2c_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL)
        return NX_ERROR_INVALID;
    if (cold->owned)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        if (!vector_idle(vector(config, i)))
            return NX_ERROR_BUSY;
    }
    const uint32_t gate = controller_gate(config);
    if ((RCU_APB1EN & gate) != 0U)
        return NX_ERROR_BUSY;
    cold->peripheral_clock = RCU_APB1EN & gate;
    cold->owned = true;
    NX_PLATFORM_REGISTER_WRITE(RCU_APB1EN, RCU_APB1EN | gate);
    nx_arch_dsb();
    nx_result_t result = NX_ERROR_IO;
    if ((RCU_APB1EN & gate) == 0U)
        goto failed;
    result = NX_ERROR_BUSY;
    if ((I2C_CTL0(config->registers) & I2C_CTL0_I2CEN) != 0U)
        goto failed;
    for (size_t i = 0U; i < vector_count(config); ++i)
        cold->priorities[i] = NVIC->IPR[(uint32_t)vector(config, i)];
    cold->vectors = config->asynchronous;
    result = nx_gd32_cold_pins_prepare(config->pins, 2U, config->snapshots,
                                       &cold->pins);
    cold->pins_started = true;
    if (result != NX_SUCCESS)
        goto failed;
    return NX_SUCCESS;
failed:
    (void)nx_gd32_i2c_cold_release(config, cold);
    return result;
}

nx_result_t nx_gd32_i2c_cold_commit(const nx_gd32_i2c_config_t* config,
                                    nx_gd32_i2c_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL || !cold->owned ||
        !cold->prepared)
        return NX_ERROR_INVALID;
    if ((RCU_APB1EN & controller_gate(config)) == 0U ||
        (I2C_CTL0(config->registers) & I2C_CTL0_I2CEN) == 0U)
        return NX_ERROR_IO;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        const uint32_t irq = (uint32_t)vector(config, i);
        const uint8_t priority = (uint8_t)(config->priority << 4U);
        NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[irq], priority);
        nx_arch_dsb();
        if (NVIC->IPR[irq] != priority ||
            NVIC_GetEnableIRQ((IRQn_Type)irq) == 0U)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
