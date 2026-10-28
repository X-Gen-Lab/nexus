/**
 * \file            i2c_async.c
 * \brief           Exact legacy 8 MHz I2C IRQ and independent PB6/PB7 lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_transport.h"
#include "gd32f30x_i2c.h"
#define PIN_MASK UINT32_C(0xc0)
#define FIELDS   UINT32_C(0xff000000)
#define FLOATING UINT32_C(0x44000000)
_Static_assert(I2C_CTL1_REG_OFFSET == 4U && I2C_STAT0_REG_OFFSET == 20U &&
                   I2C_STAT1_REG_OFFSET == 24U,
               "I2C V1 register layout");
_Static_assert(I2C_CTL0_I2CEN == 1U && I2C_CTL0_START == 256U &&
                   I2C_CTL0_STOP == 512U && I2C_CTL0_ACKEN == 1024U &&
                   I2C_CTL0_POAP == 2048U && I2C_CTL1_ERRIE == 256U &&
                   I2C_CTL1_EVIE == 512U && I2C_CTL1_BUFIE == 1024U &&
                   I2C_STAT0_SBSEND == 1U && I2C_STAT0_ADDSEND == 2U &&
                   I2C_STAT0_BTC == 4U && I2C_STAT0_RBNE == 64U &&
                   I2C_STAT0_TBE == 128U && I2C_STAT0_BERR == 256U &&
                   I2C_STAT0_LOSTARB == 512U && I2C_STAT0_AERR == 1024U &&
                   I2C_STAT0_OUERR == 2048U && I2C_STAT1_MASTER == 1U,
               "I2C V1 protocol masks");
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_GD303_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_GD303_READ(reg) & mask) == (value & mask);
}
static bool pins_idle(void) {
    uint32_t value = NX_GD303_READ(&GPIO_CTL0(GPIOB));
    return (NX_GD303_READ(&GPIO_LOCK(GPIOB)) & PIN_MASK) == 0U &&
           (((value >> 24U) & 15U) == 0U || ((value >> 24U) & 15U) == 4U) &&
           (((value >> 28U) & 15U) == 0U || ((value >> 28U) & 15U) == 4U);
}
static nx_result_t stop_irqs(void* context) {
    (void)context;
    nx_result_t event = NX_GD303_IRQ_CONTROL(I2C0_EV_IRQn, false, 5U);
    nx_result_t error = NX_GD303_IRQ_CONTROL(I2C0_ER_IRQn, false, 5U);
    return event != NX_SUCCESS ? event : error;
}
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    nx_gd32f303_i2c_async_state_t* s = context;
    if (ceiling != s->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    const int irqs[2] = {I2C0_EV_IRQn, I2C0_ER_IRQn};
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_irq_source_t source = {(int16_t)irqs[i],
                                  (uint8_t)NX_GD303_IRQ_PRIORITY(irqs[i]),
                                  (uint8_t)NX_GD303_IRQ_GROUP()};
        nx_result_t result =
            nx_irq_wake_validate(wake, &s->irq_policy, &source);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_i2c_async_stop(nx_gd32f303_i2c_async_state_t* s) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->retained)
        return NX_SUCCESS;
    nx_result_t result = NX_SUCCESS;
    if (s->core_owned) {
        result = nx_i2c_v1_async_ops.stop(&s->core);
        if (result != NX_SUCCESS)
            return result;
        s->core_owned = false;
    }
    if (s->pins_captured) {
        if (!checked(&GPIO_CTL0(GPIOB),
                     (NX_GD303_READ(&GPIO_CTL0(GPIOB)) & ~FIELDS) | FLOATING,
                     FIELDS))
            return NX_ERROR_IO;
        NX_GD303_WRITE(&GPIO_BOP(GPIOB),
                       s->saved_latch | ((PIN_MASK & ~s->saved_latch) << 16U));
        nx_arch_dsb();
        if ((NX_GD303_READ(&GPIO_OCTL(GPIOB)) & PIN_MASK) != s->saved_latch ||
            !checked(&GPIO_CTL0(GPIOB),
                     (NX_GD303_READ(&GPIO_CTL0(GPIOB)) & ~FIELDS) |
                         s->saved_control,
                     FIELDS))
            return NX_ERROR_IO;
        s->pins_captured = false;
    }
    if ((s->gates_added & UINT32_C(0x80000000)) != 0U &&
        !checked(&RCU_APB1EN, NX_GD303_READ(&RCU_APB1EN) & ~RCU_APB1EN_I2C0EN,
                 RCU_APB1EN_I2C0EN))
        return NX_ERROR_IO;
    s->gates_added &= ~UINT32_C(0x80000000);
    uint32_t gates = s->gates_added & (RCU_APB2EN_PBEN | RCU_APB2EN_AFEN);
    if (!checked(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) & ~gates, gates))
        return NX_ERROR_IO;
    s->gates_added = 0U;
    s->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_i2c_async_initialize(nx_gd32f303_i2c_async_state_t* s) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->core.registers != (volatile uint32_t*)(uintptr_t)I2C0 ||
        s->core.line_inputs != &GPIO_ISTAT(GPIOB) ||
        s->core.line_mask != PIN_MASK || s->core.peripheral_mhz != 8U ||
        s->core.dma != NULL)
        return NX_ERROR_INVALID;
    if (!g_nx_gd32f303_system.started ||
        g_nx_gd32f303_system.core_hz != 8000000U)
        return NX_ERROR_STATE;
    if (s->retained || s->core.initialized || s->core.active != NULL ||
        !NX_GD303_IRQ_AVAILABLE(I2C0_EV_IRQn) ||
        !NX_GD303_IRQ_AVAILABLE(I2C0_ER_IRQn) ||
        (NX_GD303_READ(&RCU_APB1EN) & RCU_APB1EN_I2C0EN) != 0U ||
        ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PBEN) != 0U &&
         !pins_idle()) ||
        ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_AFEN) != 0U &&
         (NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_I2C0_REMAP) != 0U))
        return NX_ERROR_BUSY;
    s->retained = true;
    s->gates_added =
        (~NX_GD303_READ(&RCU_APB2EN) & (RCU_APB2EN_PBEN | RCU_APB2EN_AFEN)) |
        UINT32_C(0x80000000);
    bool valid = checked(&RCU_APB2EN,
                         NX_GD303_READ(&RCU_APB2EN) |
                             (RCU_APB2EN_PBEN | RCU_APB2EN_AFEN),
                         (RCU_APB2EN_PBEN | RCU_APB2EN_AFEN));
    if (valid && pins_idle() &&
        (NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_I2C0_REMAP) == 0U) {
        s->saved_control = NX_GD303_READ(&GPIO_CTL0(GPIOB)) & FIELDS;
        s->saved_latch = NX_GD303_READ(&GPIO_OCTL(GPIOB)) & PIN_MASK;
        s->pins_captured = true;
        NX_GD303_WRITE(&GPIO_BOP(GPIOB), PIN_MASK);
        nx_arch_dsb();
        valid = (NX_GD303_READ(&GPIO_OCTL(GPIOB)) & PIN_MASK) == PIN_MASK &&
                checked(&GPIO_CTL0(GPIOB),
                        NX_GD303_READ(&GPIO_CTL0(GPIOB)) | FIELDS, FIELDS);
    } else
        valid = false;
    valid = checked(&RCU_APB1EN, NX_GD303_READ(&RCU_APB1EN) | RCU_APB1EN_I2C0EN,
                    RCU_APB1EN_I2C0EN) &&
            valid;
    s->core.stop_irqs = stop_irqs;
    s->core.validate_wake = validate_wake;
    nx_result_t result = NX_ERROR_IO;
    if (valid) {
        if (s->core.registers[0] != 0U || s->core.registers[1] != 0U)
            result = NX_ERROR_BUSY;
        else {
            s->core_owned = true;
            result = nx_i2c_v1_initialize(&s->core);
        }
    }
    if (result == NX_SUCCESS)
        result = NX_GD303_IRQ_CONTROL(I2C0_EV_IRQn, true, 0U);
    if (result == NX_SUCCESS)
        result = NX_GD303_IRQ_CONTROL(I2C0_ER_IRQn, true, 0U);
    if (result != NX_SUCCESS)
        (void)nx_gd32f303_i2c_async_stop(s);
    return result;
}

nx_result_t
nx_gd32f303_i2c_async_prepare(nx_gd32f303_i2c_async_state_t* s,
                              const nx_gd32f303_i2c_async_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s == NULL || config == NULL) {
        return NX_ERROR_INVALID;
    }
    if (s->retained || s->core.initialized || s->core.active != NULL ||
        s->core.wake != NULL || s->core.wake_publishers != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_irq_policy_t policy = config->irq_policy;
    s->core.registers = (volatile uint32_t*)(uintptr_t)I2C0;
    s->core.line_inputs = &GPIO_ISTAT(GPIOB);
    s->core.line_mask = PIN_MASK;
    s->core.peripheral_mhz = 8U;
    s->core.dma = NULL;
    s->irq_policy = policy;
    return nx_gd32f303_i2c_async_initialize(s);
}
