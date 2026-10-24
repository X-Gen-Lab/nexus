/**
 * \file            i2c_async.c
 * \brief           Exact legacy 8 MHz I2C IRQ and independent PB6/PB7 lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_transport.h"
#include <stddef.h>
#define PIN_MASK UINT32_C(0xc0)
#define FIELDS   UINT32_C(0xff000000)
#define FLOATING UINT32_C(0x44000000)
_Static_assert(offsetof(I2C_TypeDef, CR2) == 4U &&
                   offsetof(I2C_TypeDef, OAR1) == 8U &&
                   offsetof(I2C_TypeDef, DR) == 16U &&
                   offsetof(I2C_TypeDef, SR1) == 20U &&
                   offsetof(I2C_TypeDef, SR2) == 24U &&
                   offsetof(I2C_TypeDef, CCR) == 28U &&
                   offsetof(I2C_TypeDef, TRISE) == 32U,
               "I2C V1 register layout");
_Static_assert(I2C_CR1_PE == 1U && I2C_CR1_START == 256U &&
                   I2C_CR1_STOP == 512U && I2C_CR1_ACK == 1024U &&
                   I2C_CR1_POS == 2048U && I2C_CR2_ITERREN == 256U &&
                   I2C_CR2_ITEVTEN == 512U && I2C_CR2_ITBUFEN == 1024U &&
                   I2C_SR1_SB == 1U && I2C_SR1_ADDR == 2U &&
                   I2C_SR1_BTF == 4U && I2C_SR1_RXNE == 64U &&
                   I2C_SR1_TXE == 128U && I2C_SR1_BERR == 256U &&
                   I2C_SR1_ARLO == 512U && I2C_SR1_AF == 1024U &&
                   I2C_SR1_OVR == 2048U && I2C_SR2_MSL == 1U,
               "I2C V1 protocol masks");
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
static bool pins_idle(void) {
    uint32_t value = NX_F103_READ(&GPIOB->CRL);
    return (NX_F103_READ(&GPIOB->LCKR) & PIN_MASK) == 0U &&
           (((value >> 24U) & 15U) == 0U || ((value >> 24U) & 15U) == 4U) &&
           (((value >> 28U) & 15U) == 0U || ((value >> 28U) & 15U) == 4U);
}
static nx_result_t stop_irqs(void* context) {
    (void)context;
    nx_result_t event = NX_F103_IRQ_CONTROL(I2C1_EV_IRQn, false, 5U);
    nx_result_t error = NX_F103_IRQ_CONTROL(I2C1_ER_IRQn, false, 5U);
    return event != NX_SUCCESS ? event : error;
}
static nx_result_t validate_wake(void* context, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    nx_stm32f103_i2c_async_state_t* s = context;
    if (ceiling != s->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    const int irqs[2] = {I2C1_EV_IRQn, I2C1_ER_IRQn};
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_irq_source_t source = {(int16_t)irqs[i],
                                  (uint8_t)NX_F103_IRQ_PRIORITY(irqs[i]),
                                  (uint8_t)NX_F103_IRQ_GROUP()};
        nx_result_t result =
            nx_irq_wake_validate(wake, &s->irq_policy, &source);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}
nx_result_t nx_stm32f103_i2c_async_stop(nx_stm32f103_i2c_async_state_t* s) {
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
        if (!checked(&GPIOB->CRL,
                     (NX_F103_READ(&GPIOB->CRL) & ~FIELDS) | FLOATING, FIELDS))
            return NX_ERROR_IO;
        NX_F103_WRITE(&GPIOB->BSRR,
                      s->saved_latch | ((PIN_MASK & ~s->saved_latch) << 16U));
        nx_arch_dsb();
        if ((NX_F103_READ(&GPIOB->ODR) & PIN_MASK) != s->saved_latch ||
            !checked(&GPIOB->CRL,
                     (NX_F103_READ(&GPIOB->CRL) & ~FIELDS) | s->saved_control,
                     FIELDS))
            return NX_ERROR_IO;
        s->pins_captured = false;
    }
    if ((s->gates_added & UINT32_C(0x80000000)) != 0U &&
        !checked(&RCC->APB1ENR,
                 NX_F103_READ(&RCC->APB1ENR) & ~(uint32_t)RCC_APB1ENR_I2C1EN,
                 RCC_APB1ENR_I2C1EN))
        return NX_ERROR_IO;
    s->gates_added &= ~UINT32_C(0x80000000);
    uint32_t gates = s->gates_added & (RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN);
    if (!checked(&RCC->APB2ENR, NX_F103_READ(&RCC->APB2ENR) & ~gates, gates))
        return NX_ERROR_IO;
    s->gates_added = 0U;
    s->retained = false;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32f103_i2c_async_initialize(nx_stm32f103_i2c_async_state_t* s) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->core.registers != (volatile uint32_t*)I2C1 ||
        s->core.line_inputs != &GPIOB->IDR || s->core.line_mask != PIN_MASK ||
        s->core.peripheral_mhz != 8U || s->core.dma != NULL)
        return NX_ERROR_INVALID;
    if (!g_nx_stm32f103_system.started ||
        g_nx_stm32f103_system.core_hz != 8000000U)
        return NX_ERROR_STATE;
    if (s->retained || s->core.initialized || s->core.active != NULL ||
        !NX_F103_IRQ_AVAILABLE(I2C1_EV_IRQn) ||
        !NX_F103_IRQ_AVAILABLE(I2C1_ER_IRQn) ||
        (NX_F103_READ(&RCC->APB1ENR) & RCC_APB1ENR_I2C1EN) != 0U ||
        ((NX_F103_READ(&RCC->APB2ENR) & RCC_APB2ENR_IOPBEN) != 0U &&
         !pins_idle()) ||
        ((NX_F103_READ(&RCC->APB2ENR) & RCC_APB2ENR_AFIOEN) != 0U &&
         (NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_I2C1_REMAP) != 0U))
        return NX_ERROR_BUSY;
    s->retained = true;
    s->gates_added = (~NX_F103_READ(&RCC->APB2ENR) &
                      (RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN)) |
                     UINT32_C(0x80000000);
    bool valid = checked(&RCC->APB2ENR,
                         NX_F103_READ(&RCC->APB2ENR) |
                             (RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN),
                         (RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN));
    if (valid && pins_idle() &&
        (NX_F103_READ(&AFIO->MAPR) & AFIO_MAPR_I2C1_REMAP) == 0U) {
        s->saved_control = NX_F103_READ(&GPIOB->CRL) & FIELDS;
        s->saved_latch = NX_F103_READ(&GPIOB->ODR) & PIN_MASK;
        s->pins_captured = true;
        NX_F103_WRITE(&GPIOB->BSRR, PIN_MASK);
        nx_arch_dsb();
        valid =
            (NX_F103_READ(&GPIOB->ODR) & PIN_MASK) == PIN_MASK &&
            checked(&GPIOB->CRL, NX_F103_READ(&GPIOB->CRL) | FIELDS, FIELDS);
    } else
        valid = false;
    valid =
        checked(&RCC->APB1ENR, NX_F103_READ(&RCC->APB1ENR) | RCC_APB1ENR_I2C1EN,
                RCC_APB1ENR_I2C1EN) &&
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
        result = NX_F103_IRQ_CONTROL(I2C1_EV_IRQn, true, 0U);
    if (result == NX_SUCCESS)
        result = NX_F103_IRQ_CONTROL(I2C1_ER_IRQn, true, 0U);
    if (result != NX_SUCCESS)
        (void)nx_stm32f103_i2c_async_stop(s);
    return result;
}
