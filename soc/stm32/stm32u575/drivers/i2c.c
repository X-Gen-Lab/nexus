/**
 * \file            i2c.c
 * \brief           Exact U575 I2C1 clock, layout and pin admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include "stm32u5xx_ll_i2c.h"
#include <stddef.h>
#define ASSERT_OFFSET(owned, vendor)                                           \
    _Static_assert(offsetof(nx_i2c_v2_registers_t, owned) ==                   \
                       offsetof(I2C_TypeDef, vendor),                          \
                   "U575 I2C V2 register offset")
ASSERT_OFFSET(cr1, CR1);
ASSERT_OFFSET(cr2, CR2);
ASSERT_OFFSET(oar1, OAR1);
ASSERT_OFFSET(oar2, OAR2);
ASSERT_OFFSET(timing, TIMINGR);
ASSERT_OFFSET(timeout, TIMEOUTR);
ASSERT_OFFSET(isr, ISR);
ASSERT_OFFSET(icr, ICR);
ASSERT_OFFSET(pecr, PECR);
ASSERT_OFFSET(rxdr, RXDR);
ASSERT_OFFSET(txdr, TXDR);
_Static_assert(sizeof(nx_i2c_v2_registers_t) <= sizeof(I2C_TypeDef),
               "U575 I2C V2 prefix extent");
_Static_assert(I2C_CR1_PE == 1U && I2C_CR2_AUTOEND == (1U << 25U) &&
                   I2C_ISR_BUSY == (1U << 15U),
               "U575 I2C V2 protocol bit identity");
_Static_assert(
    I2C_CR2_RD_WRN == (1U << 10U) && I2C_CR2_START == (1U << 13U) &&
        I2C_CR2_STOP == (1U << 14U) && I2C_CR2_NBYTES == (255U << 16U) &&
        I2C_CR2_RELOAD == (1U << 24U) && I2C_ISR_TXIS == (1U << 1U) &&
        I2C_ISR_RXNE == (1U << 2U) && I2C_ISR_NACKF == (1U << 4U) &&
        I2C_ISR_STOPF == (1U << 5U) && I2C_ISR_TC == (1U << 6U) &&
        I2C_ISR_TCR == (1U << 7U) && I2C_ISR_BERR == (1U << 8U) &&
        I2C_ISR_ARLO == (1U << 9U) && I2C_ISR_OVR == (1U << 10U) &&
        I2C_ICR_NACKCF == I2C_ISR_NACKF && I2C_ICR_STOPCF == I2C_ISR_STOPF &&
        I2C_ICR_BERRCF == I2C_ISR_BERR && I2C_ICR_ARLOCF == I2C_ISR_ARLO &&
        I2C_ICR_OVRCF == I2C_ISR_OVR,
    "U575 I2C V2 command/status/W1C identity");
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_i2c_v2_state_t* port) {
    return port != NULL && port->config != NULL &&
           port->config->registers != NULL && port->config->poll_limit != 0U &&
           port->config->reset == &NX_U575_RCC->APB1RSTR1 &&
           port->config->reset_mask == RCC_APB1RSTR1_I2C1RST;
}
static nx_result_t source_wait(const nx_stm32u575_i2c_clock_t* clock,
                               bool ready) {
    uint32_t remaining = clock->port->config->poll_limit;
    do {
        if (((NX_U575_READ(&NX_U575_RCC->CR) & RCC_CR_HSIRDY) != 0U) == ready)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
nx_result_t nx_stm32u575_i2c_clock_start(nx_stm32u575_i2c_clock_t* clock,
                                         nx_i2c_v2_state_t* port) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (clock == NULL || !valid(port))
        return NX_ERROR_INVALID;
    if (clock->retained || port->retained || port->initialized || port->active)
        return NX_ERROR_BUSY;
    if (!g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != 4000000U ||
        (NX_U575_READ(&NX_U575_RCC->APB1ENR1) & RCC_APB1ENR1_I2C1EN) == 0U)
        return NX_ERROR_STATE;
    if ((NX_U575_READ(&NX_U575_RCC->CR) &
         (RCC_CR_HSION | RCC_CR_HSIKERON | RCC_CR_HSIRDY)) != 0U ||
        (NX_U575_READ(&NX_U575_RCC->ICSCR3) & RCC_ICSCR3_HSITRIM) !=
            RCC_ICSCR3_HSITRIM_4 ||
        (NX_U575_READ(&NX_U575_RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL) != 0U)
        return NX_ERROR_BUSY;
    clock->port = port;
    clock->retained = true;
    clock->source_added = true;
    clock->mux_owned = false;
    NX_U575_WRITE(&NX_U575_RCC->CR,
                  NX_U575_READ(&NX_U575_RCC->CR) | RCC_CR_HSION);
    nx_arch_dsb();
    if ((NX_U575_READ(&NX_U575_RCC->CR) & RCC_CR_HSION) == 0U)
        return NX_ERROR_IO;
    nx_result_t result = source_wait(clock, true);
    if (result != NX_SUCCESS)
        return result;
    clock->mux_owned = true;
    NX_U575_WRITE(&NX_U575_RCC->CCIPR1, (NX_U575_READ(&NX_U575_RCC->CCIPR1) &
                                         ~(uint32_t)RCC_CCIPR1_I2C1SEL) |
                                            RCC_CCIPR1_I2C1SEL_1);
    nx_arch_dsb();
    return (NX_U575_READ(&NX_U575_RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL) ==
                   RCC_CCIPR1_I2C1SEL_1
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_stm32u575_i2c_clock_stop(nx_stm32u575_i2c_clock_t* clock) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (clock == NULL)
        return NX_ERROR_INVALID;
    if (!clock->retained)
        return NX_SUCCESS;
    if (!valid(clock->port))
        return NX_ERROR_INVALID;
    if (clock->port->retained || clock->port->initialized ||
        clock->port->active)
        return NX_ERROR_BUSY;
    if (clock->mux_owned) {
        uint32_t selector =
            NX_U575_READ(&NX_U575_RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL;
        if (selector != 0U && selector != RCC_CCIPR1_I2C1SEL_1)
            return NX_ERROR_BUSY;
        NX_U575_WRITE(&NX_U575_RCC->CCIPR1, NX_U575_READ(&NX_U575_RCC->CCIPR1) &
                                                ~(uint32_t)RCC_CCIPR1_I2C1SEL);
        nx_arch_dsb();
        if ((NX_U575_READ(&NX_U575_RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL) != 0U)
            return NX_ERROR_IO;
        clock->mux_owned = false;
    }
    if (clock->source_added) {
        /* Exclusive source lease: a newly observed Stop-mode force owner is
         * never permission to remove another consumer's oscillator. */
        if ((NX_U575_READ(&NX_U575_RCC->CR) & RCC_CR_HSIKERON) != 0U)
            return NX_ERROR_BUSY;
        NX_U575_WRITE(&NX_U575_RCC->CR,
                      NX_U575_READ(&NX_U575_RCC->CR) & ~(uint32_t)RCC_CR_HSION);
        nx_arch_dsb();
        if ((NX_U575_READ(&NX_U575_RCC->CR) & RCC_CR_HSION) != 0U)
            return NX_ERROR_IO;
        nx_result_t result = source_wait(clock, false);
        if (result != NX_SUCCESS)
            return result;
        clock->source_added = false;
    }
    clock->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_i2c_initialize_timing(nx_stm32u575_i2c_state_t* port,
                                               uint32_t timing) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(port) || timing == 0U || (timing & 0x0f000000U) != 0U)
        return NX_ERROR_INVALID;
    if (port->initialized || port->retained || port->active)
        return NX_ERROR_BUSY;
    if (!g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != 4000000U ||
        (NX_U575_READ(&NX_U575_RCC->APB1ENR1) & RCC_APB1ENR1_I2C1EN) == 0U ||
        (NX_U575_READ(&NX_U575_RCC->CR) & (RCC_CR_HSION | RCC_CR_HSIRDY)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (NX_U575_READ(&NX_U575_RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL) !=
            RCC_CCIPR1_I2C1SEL_1)
        return NX_ERROR_STATE;
    /* The assembly supplies immutable HSI16/electrical timing and ES0499
     * Rev12 master-BERR policy; recover observes the same selected values. */
    if (timing != port->config->timing || !port->config->ignore_master_berr ||
        port->config->receive_reload_one)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_i2c_v2_initialize(port);
    if (result == NX_SUCCESS &&
        LL_I2C_GetTimingPrescaler((I2C_TypeDef*)port->config->registers) !=
            ((timing >> 28U) & 15U))
        return NX_ERROR_IO;
    return result;
}
nx_result_t nx_stm32u575_i2c_initialize(nx_stm32u575_i2c_state_t* port) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(port))
        return NX_ERROR_INVALID;
    return nx_stm32u575_i2c_initialize_timing(port, port->config->timing);
}
nx_result_t nx_stm32u575_i2c1_pins_prepare(void) {
    nx_result_t result = nx_stm32u575_gpio_clock_enable(1U);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t bits = (1U << 8U) | (1U << 9U);
    NX_U575_WRITE(&GPIOB->BSRR, bits);
    NX_U575_WRITE(&GPIOB->OTYPER, NX_U575_READ(&GPIOB->OTYPER) | bits);
    NX_U575_WRITE(&GPIOB->PUPDR, NX_U575_READ(&GPIOB->PUPDR) & ~(15U << 16U));
    nx_arch_dsb();
    if ((NX_U575_READ(&GPIOB->OTYPER) & bits) != bits ||
        (NX_U575_READ(&GPIOB->PUPDR) & (15U << 16U)) != 0U)
        return NX_ERROR_IO;
    result = nx_stm32u575_pin_alternate(GPIOB, 8U, 4U);
    return result == NX_SUCCESS ? nx_stm32u575_pin_alternate(GPIOB, 9U, 4U)
                                : result;
}

nx_result_t nx_stm32u575_i2c_prepare(nx_i2c_v2_state_t* port,
                                     nx_stm32u575_i2c_clock_t* clock,
                                     const nx_i2c_v2_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (clock == NULL)
        return NX_ERROR_INVALID;
    if (clock->retained)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_i2c_v2_bind(port, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32u575_i2c1_pins_prepare();
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32u575_i2c_clock_start(clock, port);
    return result == NX_SUCCESS ? nx_stm32u575_i2c_initialize(port) : result;
}
nx_result_t nx_stm32u575_i2c_close(nx_i2c_v2_state_t* port,
                                   nx_stm32u575_i2c_clock_t* clock) {
    nx_result_t result = nx_i2c_v2_ops.stop(port);
    return result == NX_SUCCESS ? nx_stm32u575_i2c_clock_stop(clock) : result;
}
