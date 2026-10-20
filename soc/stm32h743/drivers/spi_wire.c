/**
 * \file            spi_wire.c
 * \brief           Shared fixed-work H743 SPI clock, stop and pin leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_clock.h"
#include "stm32h743_spi_wire.h"
#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() NX_H743_READ(&DBGMCU->IDCODE)
#endif
#ifndef NX_H743_GPIOE
#define NX_H743_GPIOE GPIOE
#endif
#define BYTE_CONFIG NX_STM32H743_SPI_BYTE_CONFIG
#define FIFO_LEVELS NX_STM32H743_SPI_FIFO_LEVELS
#define SCK_BIT     (1U << 12U)
#define SCK_MODE    (3U << 24U)
/** \brief These observations check an existing static lease, never grant it. */
bool nx_stm32h743_spi_wire_pins_ready(void) {
    GPIO_TypeDef* gpio = NX_H743_GPIOE;
    RCC_TypeDef* rcc = g_nx_stm32h743_system.rcc;
    if (rcc == NULL ||
        (NX_H743_READ(&rcc->AHB4ENR) & RCC_AHB4ENR_GPIOEEN) == 0U)
        return false;
    const uint32_t mode = NX_H743_READ(&gpio->MODER);
    const uint32_t sck = (mode >> 24U) & 3U;
    const uint32_t af = NX_H743_READ(&gpio->AFR[1]);
    return (sck == 1U || sck == 2U) && ((mode >> 26U) & 3U) == 2U &&
           ((mode >> 28U) & 3U) == 2U && ((af >> 16U) & 15U) == 5U &&
           ((af >> 20U) & 15U) == 5U && ((af >> 24U) & 15U) == 5U &&
           (NX_H743_READ(&gpio->OTYPER) & (SCK_BIT | (1U << 14U))) == 0U;
}
bool nx_stm32h743_spi_wire_cs_driven(const nx_stm32h743_gpio_state_t* gpio,
                                     uint32_t mask) {
    const uint32_t mode = NX_H743_READ(&gpio->registers->MODER);
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((mask & (1U << pin)) != 0U && ((mode >> (pin * 2U)) & 3U) != 1U)
            return false;
    return (NX_H743_READ(&gpio->registers->OTYPER) & mask) == 0U;
}
/** \brief Return the already leased SCK to its configured SPI idle driver. */
nx_result_t nx_stm32h743_spi_wire_sck_alternate(void) {
    if (!nx_stm32h743_spi_wire_pins_ready())
        return NX_ERROR_STATE;
    GPIO_TypeDef* gpio = NX_H743_GPIOE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    NX_H743_WRITE(&gpio->MODER,
                  (NX_H743_READ(&gpio->MODER) & ~SCK_MODE) | (2U << 24U));
    nx_arch_dsb();
    const bool ready = (NX_H743_READ(&gpio->MODER) & SCK_MODE) == (2U << 24U);
    nx_arch_irq_restore(saved);
    return ready ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief With CS released, retain CPOL using GPIO before resetting SPI. */
nx_result_t nx_stm32h743_spi_wire_sck_idle(nx_stm32h743_spi_state_t* p) {
    GPIO_TypeDef* gpio = NX_H743_GPIOE;
    RCC_TypeDef* rcc = g_nx_stm32h743_system.rcc;
    if ((NX_H743_READ(&rcc->AHB4ENR) & RCC_AHB4ENR_GPIOEEN) == 0U)
        return NX_ERROR_STATE;
    const uint32_t mode = (NX_H743_READ(&gpio->MODER) >> 24U) & 3U;
    /* Failed cold prepare can own a still-unrouted pad. No SPI output or
     * selected interval was ever established; do not invent a transition. */
    if ((mode == 0U || mode == 3U) && !p->active && p->held_cs == NULL &&
        NX_H743_READ(&p->registers->CR1) == 0U &&
        NX_H743_READ(&p->registers->CR2) == 0U &&
        NX_H743_READ(&p->registers->CFG1) == BYTE_CONFIG &&
        NX_H743_READ(&p->registers->CFG2) == 0U &&
        NX_H743_READ(&p->registers->IER) == 0U)
        return NX_SUCCESS;
    if (!nx_stm32h743_spi_wire_pins_ready())
        return NX_ERROR_STATE;
    if (mode == 1U)
        return NX_SUCCESS; /* Our previous handoff already owns this latch. */
    const bool high = (NX_H743_READ(&p->registers->CFG2) & SPI_CFG2_CPOL) != 0U;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    NX_H743_WRITE(&gpio->BSRR, high ? SCK_BIT : SCK_BIT << 16U);
    nx_arch_dsb();
    bool ready = ((NX_H743_READ(&gpio->ODR) & SCK_BIT) != 0U) == high;
    if (ready) {
        NX_H743_WRITE(&gpio->MODER,
                      (NX_H743_READ(&gpio->MODER) & ~SCK_MODE) | (1U << 24U));
        nx_arch_dsb();
        ready = (NX_H743_READ(&gpio->MODER) & SCK_MODE) == (1U << 24U);
    }
    nx_arch_irq_restore(saved);
    return ready ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief The SPI clock envelope requires exact V identity and default trim. */
nx_result_t
nx_stm32h743_spi_wire_clock_ready(const nx_stm32h743_spi_state_t* p) {
    RCC_TypeDef* r = g_nx_stm32h743_system.rcc;
    if (r == NULL || !g_nx_stm32h743_system.started)
        return NX_ERROR_STATE;
    /* HSICFGR has the reviewed layout only on revision V. Do not read that
     * register until the independent DEV_ID/REV_ID check admits this cut. */
    if (!nx_stm32h743_is_revision_v(NX_H743_SILICON_ID()))
        return NX_ERROR_UNSUPPORTED;
    if ((NX_H743_READ(&r->HSICFGR) & RCC_HSICFGR_HSITRIM) !=
        (0x40U << RCC_HSICFGR_HSITRIM_Pos))
        return NX_ERROR_STATE;
#if NEXUS_CORE_HZ == 384000000U
    if (!nx_stm32h743_pll384_clocks_ready())
#else
    if (g_nx_stm32h743_system.core_hz != p->clock_hz)
#endif
        return NX_ERROR_STATE;
    if (p->reset == &r->APB2RSTR && p->reset_mask == RCC_APB2RSTR_SPI4RST)
        return (NX_H743_READ(&r->APB2ENR) & RCC_APB2ENR_SPI4EN) != 0U &&
                       (NX_H743_READ(&r->D2CCIP1R) & RCC_D2CCIP1R_SPI45SEL) ==
                           0U
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    return NX_ERROR_STATE;
}
/** \brief H743 V3 reset stops its FIFO shifter; verify both reset edges and
 * idle. */
nx_result_t nx_stm32h743_spi_wire_reset(nx_stm32h743_spi_state_t* p) {
    NX_H743_WRITE(p->reset, NX_H743_READ(p->reset) | p->reset_mask);
    nx_arch_dsb();
    if ((NX_H743_READ(p->reset) & p->reset_mask) == 0U)
        return NX_ERROR_IO;
    NX_H743_WRITE(p->reset, NX_H743_READ(p->reset) & ~p->reset_mask);
    nx_arch_dsb();
    if ((NX_H743_READ(p->reset) & p->reset_mask) != 0U ||
        NX_H743_READ(&p->registers->CR1) != 0U ||
        NX_H743_READ(&p->registers->CR2) != 0U ||
        NX_H743_READ(&p->registers->CFG1) != BYTE_CONFIG ||
        NX_H743_READ(&p->registers->CFG2) != 0U ||
        NX_H743_READ(&p->registers->IER) != 0U ||
        (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Failed CS withdrawal remains a concrete lease through recovery. */
nx_result_t nx_stm32h743_spi_wire_release_cs(nx_stm32h743_spi_state_t* p) {
    if (p->held_cs == NULL)
        return NX_SUCCESS;
    if (!nx_stm32h743_spi_wire_cs_driven(p->held_cs, p->held_cs_mask))
        return NX_ERROR_IO;
    const nx_gpio_port_t face = {&nx_stm32h743_gpio_ops, p->held_cs};
    nx_result_t result = nx_gpio_port_write(&face, p->held_cs_mask, 0U);
    nx_arch_dsb();
    if (result != NX_SUCCESS ||
        !nx_stm32h743_spi_wire_cs_driven(p->held_cs, p->held_cs_mask) ||
        (NX_H743_READ(&p->held_cs->registers->ODR) & p->held_cs_mask) !=
            p->held_cs_mask)
        return NX_ERROR_IO;
    p->held_cs = NULL;
    p->held_cs_mask = 0U;
    return NX_SUCCESS;
}
/** \brief Frequency envelopes select divisors, never the quiet time ratio. */
nx_result_t nx_stm32h743_spi_wire_divider(uint32_t clock_hz,
                                          uint32_t maximum_hz,
                                          uint32_t* divisor, uint32_t* baud) {
    if (maximum_hz == 0U || divisor == NULL || baud == NULL)
        return NX_ERROR_INVALID;
#if NEXUS_CORE_HZ == 384000000U
    if (clock_hz != 96000000U)
        return NX_ERROR_INVALID;
    /* Existing HSI/PLL/APB upper envelope; not a frequency measurement. */
    const uint32_t source_max = 99450000U;
#else
    if (clock_hz != 32000000U && clock_hz != 64000000U)
        return NX_ERROR_INVALID;
    const uint32_t source_max = clock_hz == 32000000U ? 32500000U : 65000000U;
#endif
    uint32_t selected = 2U;
    uint32_t encoding = 0U;
    while ((uint64_t)source_max > (uint64_t)maximum_hz * selected &&
           selected < 256U) {
        selected *= 2U;
        ++encoding;
    }
    if ((uint64_t)source_max > (uint64_t)maximum_hz * selected)
        return NX_ERROR_UNSUPPORTED;
    *divisor = selected;
    *baud = encoding;
    return NX_SUCCESS;
}
uint32_t nx_stm32h743_spi_wire_quiet_us(uint32_t clock_hz, uint32_t divisor) {
    if ((clock_hz != 32000000U && clock_hz != 64000000U &&
         clock_hz != 96000000U) ||
        divisor < 2U || divisor > 256U || (divisor & (divisor - 1U)) != 0U)
        return 0U;
    /* TIM and SPI derive from the same selected source. Two timer quanta
     * cover observation phase/rounding independently of request deadlines. */
    return (uint32_t)(((uint64_t)divisor * 1000000U + clock_hz - 1U) /
                      clock_hz) +
           2U;
}
void nx_stm32h743_spi_wire_suspend_once(nx_stm32h743_spi_state_t* p) {
    /* CSTART is read/set: zero does not cancel an ongoing frame. */
    NX_H743_WRITE(&p->registers->CR1, (NX_H743_READ(&p->registers->CR1) &
                                       ~(uint32_t)SPI_CR1_CSTART) |
                                          SPI_CR1_CSUSP);
    nx_arch_dsb();
}
bool nx_stm32h743_spi_wire_stopped(const nx_stm32h743_spi_state_t* p) {
    const uint32_t control = NX_H743_READ(&p->registers->CR1);
    const uint32_t status = NX_H743_READ(&p->registers->SR);
    const bool finite_complete =
        (NX_H743_READ(&p->registers->CR2) & SPI_CR2_TSIZE) != 0U &&
        (status & (SPI_SR_EOT | SPI_SR_TXC)) == (SPI_SR_EOT | SPI_SR_TXC);
    /* Finite EOT can win the CSUSP race without producing SUSP. TXC then
     * copies EOT, so independent quiet remains the owner's responsibility. */
    return (control & SPI_CR1_CSTART) == 0U &&
           ((status & SPI_SR_SUSP) != 0U || finite_complete);
}
nx_result_t nx_stm32h743_spi_wire_disable(nx_stm32h743_spi_state_t* p) {
    if ((NX_H743_READ(&p->registers->SR) & SPI_SR_SUSP) != 0U) {
        NX_H743_WRITE(&p->registers->IFCR, SPI_IFCR_SUSPC);
        nx_arch_dsb();
        if ((NX_H743_READ(&p->registers->SR) & SPI_SR_SUSP) != 0U)
            return NX_ERROR_IO;
    }
    NX_H743_WRITE(&p->registers->CR1,
                  NX_H743_READ(&p->registers->CR1) & ~(uint32_t)SPI_CR1_SPE);
    nx_arch_dsb();
    return (NX_H743_READ(&p->registers->CR1) &
            (SPI_CR1_SPE | SPI_CR1_CSTART)) == 0U &&
                   (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
