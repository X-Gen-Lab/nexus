/**
 * \file            i2c.c
 * \brief           Reviewed G474 V2 layout and fixed100kHz PB8/PB9 pad setup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v2.h"
#include "stm32g474_provider.h"
#include <stddef.h>
#define SAME(member, sdk)                                                      \
    _Static_assert(offsetof(nx_i2c_v2_registers_t, member) ==                  \
                       offsetof(I2C_TypeDef, sdk),                             \
                   "G474 V2 " #sdk " offset")
SAME(cr1, CR1);
SAME(cr2, CR2);
SAME(oar1, OAR1);
SAME(oar2, OAR2);
SAME(timing, TIMINGR);
SAME(timeout, TIMEOUTR);
SAME(isr, ISR);
SAME(icr, ICR);
SAME(pecr, PECR);
SAME(rxdr, RXDR);
SAME(txdr, TXDR);
_Static_assert(sizeof(nx_i2c_v2_registers_t) == sizeof(I2C_TypeDef),
               "G474 V2 complete layout");
_Static_assert(I2C_CR1_PE == (1U << 0) && I2C_CR2_RD_WRN == (1U << 10) &&
                   I2C_CR2_START == (1U << 13) && I2C_CR2_STOP == (1U << 14) &&
                   I2C_CR2_NBYTES == (255U << 16) &&
                   I2C_CR2_RELOAD == (1U << 24) &&
                   I2C_CR2_AUTOEND == (1U << 25),
               "G474 V2 command semantics");
_Static_assert(I2C_ISR_TXIS == (1U << 1) && I2C_ISR_RXNE == (1U << 2) &&
                   I2C_ISR_NACKF == (1U << 4) && I2C_ISR_STOPF == (1U << 5) &&
                   I2C_ISR_TC == (1U << 6) && I2C_ISR_TCR == (1U << 7) &&
                   I2C_ISR_BERR == (1U << 8) && I2C_ISR_ARLO == (1U << 9) &&
                   I2C_ISR_OVR == (1U << 10) && I2C_ISR_BUSY == (1U << 15),
               "G474 V2 status semantics");
nx_result_t nx_stm32g474_i2c_pins_prepare(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    GPIO_TypeDef* p = GPIOB;
    uint32_t bits = (1U << 8U) | (1U << 9U);
    /* External pull-ups are a Board electrical requirement. Release the latch
     * and select open drain before alternate mode, without driving SCL/SDA.
     */
    NX_G474_WRITE(&p->BSRR, bits);
    NX_G474_WRITE(&p->OTYPER, NX_G474_READ(&p->OTYPER) | bits);
    NX_G474_WRITE(&p->PUPDR, NX_G474_READ(&p->PUPDR) & ~(15U << 16U));
    NX_G474_WRITE(&p->OSPEEDR, NX_G474_READ(&p->OSPEEDR) & ~(15U << 16U));
    NX_G474_WRITE(&p->AFR[1], (NX_G474_READ(&p->AFR[1]) & ~255U) | 0x44U);
    nx_arch_dsb();
    NX_G474_WRITE(&p->MODER,
                  (NX_G474_READ(&p->MODER) & ~(15U << 16U)) | (10U << 16U));
    nx_arch_dsb();
    return (NX_G474_READ(&p->OTYPER) & bits) == bits &&
                   (NX_G474_READ(&p->AFR[1]) & 255U) == 0x44U &&
                   (NX_G474_READ(&p->PUPDR) & (15U << 16U)) == 0U &&
                   (NX_G474_READ(&p->OSPEEDR) & (15U << 16U)) == 0U &&
                   (NX_G474_READ(&p->MODER) & (15U << 16U)) == (10U << 16U) &&
                   (NX_G474_READ(&p->ODR) & bits) == bits
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

nx_result_t nx_stm32g474_i2c_prepare(nx_i2c_v2_state_t* port,
                                     const nx_i2c_v2_config_t* config) {
    nx_result_t result = nx_i2c_v2_bind(port, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32g474_i2c_pins_prepare();
    return result == NX_SUCCESS ? nx_i2c_v2_initialize(port) : result;
}
