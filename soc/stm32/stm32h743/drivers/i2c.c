/**
 * \file            i2c.c
 * \brief           Reviewed H743 V2 layout and fixed100kHz PB8/PB9 pad setup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v2.h"
#include "stm32h743_provider.h"
#if NEXUS_CORE_HZ == 384000000U
#include "stm32h743_clock.h"
#endif
#include <stddef.h>
#define SAME(member, sdk)                                                      \
    _Static_assert(offsetof(nx_i2c_v2_registers_t, member) ==                  \
                       offsetof(I2C_TypeDef, sdk),                             \
                   "H743 V2 " #sdk " offset")
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
               "H743 V2 complete layout");
_Static_assert(I2C_CR1_PE == (1U << 0) && I2C_CR2_RD_WRN == (1U << 10) &&
                   I2C_CR2_START == (1U << 13) && I2C_CR2_STOP == (1U << 14) &&
                   I2C_CR2_NBYTES == (255U << 16) &&
                   I2C_CR2_RELOAD == (1U << 24) &&
                   I2C_CR2_AUTOEND == (1U << 25),
               "H743 V2 command semantics");
_Static_assert(I2C_ISR_TXIS == (1U << 1) && I2C_ISR_RXNE == (1U << 2) &&
                   I2C_ISR_NACKF == (1U << 4) && I2C_ISR_STOPF == (1U << 5) &&
                   I2C_ISR_TC == (1U << 6) && I2C_ISR_TCR == (1U << 7) &&
                   I2C_ISR_BERR == (1U << 8) && I2C_ISR_ARLO == (1U << 9) &&
                   I2C_ISR_OVR == (1U << 10) && I2C_ISR_BUSY == (1U << 15),
               "H743 V2 status semantics");
nx_result_t nx_stm32h743_i2c_pins_prepare(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    RCC_TypeDef* rcc = g_nx_stm32h743_system.rcc;
    if (!g_nx_stm32h743_system.started || rcc == NULL ||
#if NEXUS_CORE_HZ == 384000000U
        !nx_stm32h743_pll384_clocks_ready() ||
        (NX_H743_READ(&rcc->D2CCIP2R) & RCC_D2CCIP2R_I2C123SEL) !=
            (2U << RCC_D2CCIP2R_I2C123SEL_Pos) ||
#else
        (g_nx_stm32h743_system.core_hz != 32000000U &&
         g_nx_stm32h743_system.core_hz != 64000000U) ||
        (NX_H743_READ(&rcc->D2CCIP2R) & RCC_D2CCIP2R_I2C123SEL) != 0U ||
#endif
        (NX_H743_READ(&rcc->APB1LENR) & RCC_APB1LENR_I2C1EN) == 0U ||
        (NX_H743_READ(&rcc->AHB4ENR) & RCC_AHB4ENR_GPIOBEN) == 0U ||
        (NX_H743_READ(&rcc->APB1LRSTR) & RCC_APB1LRSTR_I2C1RST) != 0U)
        return NX_ERROR_STATE;
    GPIO_TypeDef* p = GPIOB;
    uint32_t bits = (1U << 8U) | (1U << 9U);
    /* External pull-ups are a Board electrical requirement. Release the latch
     * and select open drain before alternate mode, without driving SCL/SDA.
     */
    NX_H743_WRITE(&p->BSRR, bits);
    NX_H743_WRITE(&p->OTYPER, NX_H743_READ(&p->OTYPER) | bits);
    NX_H743_WRITE(&p->PUPDR, NX_H743_READ(&p->PUPDR) & ~(15U << 16U));
    NX_H743_WRITE(&p->OSPEEDR, NX_H743_READ(&p->OSPEEDR) & ~(15U << 16U));
    NX_H743_WRITE(&p->AFR[1], (NX_H743_READ(&p->AFR[1]) & ~255U) | 0x44U);
    nx_arch_dsb();
    if ((NX_H743_READ(&p->ODR) & bits) != bits ||
        (NX_H743_READ(&p->OTYPER) & bits) != bits ||
        (NX_H743_READ(&p->PUPDR) & (15U << 16U)) != 0U ||
        (NX_H743_READ(&p->OSPEEDR) & (15U << 16U)) != 0U ||
        (NX_H743_READ(&p->AFR[1]) & 255U) != 0x44U)
        return NX_ERROR_IO;
    NX_H743_WRITE(&p->MODER,
                  (NX_H743_READ(&p->MODER) & ~(15U << 16U)) | (10U << 16U));
    nx_arch_dsb();
    return (NX_H743_READ(&p->OTYPER) & bits) == bits &&
                   (NX_H743_READ(&p->AFR[1]) & 255U) == 0x44U &&
                   (NX_H743_READ(&p->MODER) & (15U << 16U)) == (10U << 16U) &&
                   (NX_H743_READ(&p->ODR) & bits) == bits
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
