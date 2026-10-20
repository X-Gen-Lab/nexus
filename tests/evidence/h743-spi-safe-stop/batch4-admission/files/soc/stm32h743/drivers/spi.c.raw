/**
 * \file            spi.c
 * \brief           H743 finite full-duplex byte FIFO polling transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32h743_clock.h"
#include "stm32h743_provider.h"
#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() NX_H743_READ(&DBGMCU->IDCODE)
#endif
#include "stm32h7xx_ll_spi.h"
/* ES0392 2.22.5: CRC-disabled finite reception still requires CRCSIZE=7. */
#define BYTE_CONFIG (7U | (7U << SPI_CFG1_CRCSIZE_Pos))
#define FIFO_LEVELS ((uint32_t)(SPI_SR_RXPLVL | SPI_SR_RXWNE | SPI_SR_RXP))
#ifndef NX_H743_GPIOE
#define NX_H743_GPIOE GPIOE
#endif
#define SCK_BIT  (1U << 12U)
#define SCK_MODE (3U << 24U)
/** \brief These observations check an existing static lease, never grant it. */
static bool pins_ready(void) {
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
static bool cs_driven(const nx_stm32h743_gpio_state_t* gpio, uint32_t mask) {
    const uint32_t mode = NX_H743_READ(&gpio->registers->MODER);
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((mask & (1U << pin)) != 0U && ((mode >> (pin * 2U)) & 3U) != 1U)
            return false;
    return (NX_H743_READ(&gpio->registers->OTYPER) & mask) == 0U;
}
/** \brief Return the already leased SCK to its configured SPI idle driver. */
static nx_result_t sck_alternate(void) {
    if (!pins_ready())
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
static nx_result_t sck_idle(nx_stm32h743_spi_state_t* p) {
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
    if (!pins_ready())
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
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32h743_spi_state_t* p) {
    return p != NULL && p->registers != NULL && p->reset != NULL &&
           p->reset_mask != 0U &&
           (p->reset_mask & (p->reset_mask - 1U)) == 0U &&
#if NEXUS_CORE_HZ == 384000000U
           p->clock_hz == 96000000U &&
#else
           (p->clock_hz == 32000000U || p->clock_hz == 64000000U) &&
#endif
           p->poll_limit != 0U;
}
/** \brief The SPI clock envelope requires exact V identity and default trim. */
static nx_result_t clock_ready(const nx_stm32h743_spi_state_t* p) {
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
static bool span(const void* value, size_t size) {
    return value != NULL && size != 0U &&
           (uintptr_t)value <= UINTPTR_MAX - size;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected(const nx_stm32h743_spi_endpoint_state_t* e,
                      const void* data, size_t size) {
    return overlap(data, size, e, sizeof(*e)) ||
           overlap(data, size, e->port, sizeof(*e->port)) ||
           overlap(data, size, e->port->registers,
                   sizeof(*e->port->registers)) ||
           overlap(data, size, (const void*)e->port->reset,
                   sizeof(*e->port->reset)) ||
           overlap(data, size, e->cs, sizeof(*e->cs)) ||
           overlap(data, size, e->cs->registers, sizeof(*e->cs->registers));
}
/** \brief H743 V3 reset stops its FIFO shifter; verify both reset edges and
 * idle. */
static nx_result_t reset(nx_stm32h743_spi_state_t* p) {
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
static nx_result_t release_cs(nx_stm32h743_spi_state_t* p) {
    if (p->held_cs == NULL)
        return NX_SUCCESS;
    if (!cs_driven(p->held_cs, p->held_cs_mask))
        return NX_ERROR_IO;
    const nx_gpio_port_t face = {&nx_stm32h743_gpio_ops, p->held_cs};
    nx_result_t result = nx_gpio_port_write(&face, p->held_cs_mask, 0U);
    nx_arch_dsb();
    if (result != NX_SUCCESS || !cs_driven(p->held_cs, p->held_cs_mask) ||
        (NX_H743_READ(&p->held_cs->registers->ODR) & p->held_cs_mask) !=
            p->held_cs_mask)
        return NX_ERROR_IO;
    p->held_cs = NULL;
    p->held_cs_mask = 0U;
    return NX_SUCCESS;
}
/** \brief Polling cannot return while wire/CS withdrawal remains unproved. */
static _Noreturn void retain_and_park(nx_stm32h743_spi_state_t* p) {
    p->fault = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static nx_result_t wait(nx_stm32h743_spi_state_t* p, uint32_t mask,
                        uint32_t expected, nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_H743_SPI_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        uint32_t status = NX_H743_READ(&p->registers->SR);
        if ((status & (SPI_SR_MODF | SPI_SR_OVR | SPI_SR_UDR | SPI_SR_TIFRE)) !=
            0U)
            return NX_ERROR_IO;
        if ((status & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
/** \brief ES0392 2.22.6 EOT/TXC may precede the final trailing SCK edge. */
static void finish_clock(nx_stm32h743_spi_state_t* p, uint32_t divisor) {
    uint32_t period =
        (uint32_t)(((uint64_t)divisor * 1000000U + p->clock_hz - 1U) /
                   p->clock_hz);
    /* Two timer quanta cover observation phase/rounding. This drain deadline
     * remains independent of the request deadline: expiry cannot truncate SCK.
     */
    nx_time_us_t quiet = nx_deadline_after(nx_time_now_us(), period + 2U);
    uint32_t remaining = p->poll_limit;
    while (!nx_deadline_expired(quiet, nx_time_now_us())) {
        if (remaining-- == 0U)
            retain_and_park(p);
        NX_H743_SPI_POLL(p);
    }
}
/** \brief Stop the real frame before disabling, flushing or releasing CS.
 * \note Nonzero TSIZE makes TXC a copy of EOT, not independent wire proof.
 *       CSUSP can leave unsent TXFIFO data; SPE0 may flush it after SUSP.
 *       Only complete RX bytes in this call's submitted prefix count. */
static nx_result_t drain(nx_stm32h743_spi_state_t* p, uint32_t divisor,
                         uint8_t* rx, size_t submitted, size_t* complete) {
    if ((NX_H743_READ(&p->registers->CR1) & SPI_CR1_CSTART) != 0U) {
        /* CSTART is read/set: zero leaves an ongoing transfer unchanged.
         * Never replay a stale start request with the suspend command. */
        NX_H743_WRITE(&p->registers->CR1, (NX_H743_READ(&p->registers->CR1) &
                                           ~(uint32_t)SPI_CR1_CSTART) |
                                              SPI_CR1_CSUSP);
        nx_arch_dsb();
        uint32_t remaining = p->poll_limit;
        bool stopped = false;
        do {
            NX_H743_SPI_POLL(p);
            const uint32_t control = NX_H743_READ(&p->registers->CR1);
            const uint32_t status = NX_H743_READ(&p->registers->SR);
            /* EOT can clear CSTART before CSUSP reaches the peripheral;
             * that idle command need not produce SUSP. Nonzero TSIZE and
             * both completion flags admit only the finite-completion path.
             * TXC is then a copy of EOT; the independent quiet guard below
             * still covers the final trailing edge. */
            const bool finite_complete =
                (NX_H743_READ(&p->registers->CR2) & SPI_CR2_TSIZE) != 0U &&
                (status & (SPI_SR_EOT | SPI_SR_TXC)) ==
                    (SPI_SR_EOT | SPI_SR_TXC);
            if ((control & SPI_CR1_CSTART) == 0U &&
                ((status & SPI_SR_SUSP) != 0U || finite_complete)) {
                stopped = true;
                break;
            }
        } while (--remaining != 0U);
        if (!stopped)
            return NX_ERROR_IO;
    }
    /* CSTART=0 is the master's actual idle state. EOT/SUSP can precede the
     * trailing edge, so caller expiry cannot replace this independent guard. */
    finish_clock(p, divisor);
    uint32_t remaining = p->poll_limit;
    while ((NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U) {
        if (remaining-- == 0U ||
            (NX_H743_READ(&p->registers->SR) & SPI_SR_RXP) == 0U)
            return NX_ERROR_IO;
        const uint8_t value = NX_H743_SPI_READ8(&p->registers->RXDR);
        if (complete != NULL) {
            if (*complete >= submitted)
                return NX_ERROR_IO;
            if (rx != NULL)
                rx[*complete] = value;
            ++*complete;
        }
    }
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
/** \brief No live caller frame exists on an idle recover/stop path. */
static nx_result_t withdraw(nx_stm32h743_spi_state_t* p) {
    const uint32_t divisor =
        2U << ((NX_H743_READ(&p->registers->CFG1) & SPI_CFG1_MBR) >>
               SPI_CFG1_MBR_Pos);
    if (drain(p, divisor, NULL, 0U, NULL) != NX_SUCCESS ||
        release_cs(p) != NX_SUCCESS || sck_idle(p) != NX_SUCCESS ||
        reset(p) != NX_SUCCESS) {
        p->fault = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_spi_initialize(nx_stm32h743_spi_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->active || p->held_cs != NULL ||
        NX_H743_READ(&p->registers->CR1) != 0U ||
        NX_H743_READ(&p->registers->CR2) != 0U ||
        NX_H743_READ(&p->registers->IER) != 0U ||
        NX_H743_READ(&p->registers->CFG1) != BYTE_CONFIG ||
        NX_H743_READ(&p->registers->CFG2) != 0U ||
        (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    const nx_result_t readiness = clock_ready(p);
    if (readiness != NX_SUCCESS)
        return readiness;
    p->retained = true;
    p->fault = false;
    NX_H743_WRITE(&p->registers->CFG1, BYTE_CONFIG);
    nx_arch_dsb();
    if (NX_H743_READ(&p->registers->CFG1) != BYTE_CONFIG)
        return NX_ERROR_IO;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    const nx_stm32h743_spi_endpoint_state_t* e = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (e == NULL || !valid(e->port) || e->cs == NULL ||
        e->cs->registers == NULL || !span(transferred, sizeof(*transferred)) ||
        length == 0U || (tx == NULL && rx == NULL) ||
        (tx != NULL && !span(tx, length)) ||
        (rx != NULL && !span(rx, length)) || e->mode > 3U ||
        e->frequency_hz == 0U || e->cs_mask == 0U ||
        (e->cs_mask & ~e->cs->mask) != 0U || !e->cs->output)
        return NX_ERROR_INVALID;
    if (protected(e, transferred, sizeof(*transferred)) ||
        (tx != NULL &&
         (protected(e, tx, length) ||
          overlap(transferred, sizeof(*transferred), tx, length))) ||
        (rx != NULL &&
         (protected(e, rx, length) ||
          overlap(transferred, sizeof(*transferred), rx, length))) ||
        (tx != NULL && rx != NULL && tx != rx &&
         overlap(tx, length, rx, length)))
        return NX_ERROR_INVALID;
    *transferred = 0U;
    nx_stm32h743_spi_state_t* p = e->port;
    if (!p->initialized || !e->cs->initialized || p->fault ||
        clock_ready(p) != NX_SUCCESS)
        return NX_ERROR_STATE;
    if (p->active || (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    if (!pins_ready() || !cs_driven(e->cs, e->cs_mask) ||
        (NX_H743_READ(&e->cs->registers->ODR) & e->cs_mask) != e->cs_mask)
        return NX_ERROR_STATE;
    /* SPI4 is APB2-clocked in the admitted reset kernel selector. Restrict
     * the reference to <=16MHz; electrical validation remains external. */
    if (length > 256U || deadline == NX_DEADLINE_NEVER ||
        e->frequency_hz > 16000000U)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    uint32_t divisor = 2U;
    uint32_t baud = 0U;
#if NEXUS_CORE_HZ == 384000000U
    /* HSI upper65MHz times PLL ratio6 and ready tolerance1.02, APB/4.
     * Retain clock_hz as the nominal96MHz; never report the bound as a
     * measurement. The fixed timer/SPI ratio also bounds trailing-edge drain.
     */
    const uint32_t source_max = 99450000U;
#else
    /* Fixed HSI64 and HSIDIV2 require the 65/32.5MHz source envelope.
     * Keep clock_hz nominal for the same-source TIM/SPI quiet ratio. */
    const uint32_t source_max =
        p->clock_hz == 32000000U ? 32500000U : 65000000U;
#endif
    while ((uint64_t)source_max > (uint64_t)e->frequency_hz * divisor &&
           divisor < 256U) {
        divisor *= 2U;
        ++baud;
    }
    if ((uint64_t)source_max > (uint64_t)e->frequency_hz * divisor)
        return NX_ERROR_UNSUPPORTED;
    p->active = true;
    uint32_t config = SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_AFCNTR |
                      ((e->mode & 1U) != 0U ? SPI_CFG2_CPHA : 0U) |
                      ((e->mode & 2U) != 0U ? SPI_CFG2_CPOL : 0U);
    uint32_t config1 = BYTE_CONFIG | (baud << SPI_CFG1_MBR_Pos);
    NX_H743_WRITE(&p->registers->CR1, SPI_CR1_SSI);
    NX_H743_WRITE(&p->registers->CFG1, config1);
    NX_H743_WRITE(&p->registers->CFG2, config);
    NX_H743_WRITE(&p->registers->CR2, (uint32_t)length);
    NX_H743_WRITE(&p->registers->IFCR, SPI_IFCR_EOTC | SPI_IFCR_TXTFC);
    nx_arch_dsb();
    nx_result_t result = NX_SUCCESS;
    if (NX_H743_READ(&p->registers->CFG2) != config ||
        NX_H743_READ(&p->registers->CFG1) != config1 ||
        LL_SPI_GetTransferSize(p->registers) != length ||
        NX_H743_READ(&p->registers->CR1) != SPI_CR1_SSI)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS && sck_alternate() != NX_SUCCESS)
        result = NX_ERROR_IO;
    const nx_gpio_port_t cs = {&nx_stm32h743_gpio_ops, e->cs};
    if (result == NX_SUCCESS) {
        p->held_cs = e->cs;
        p->held_cs_mask = e->cs_mask;
        result = nx_gpio_port_write(&cs, 0U, e->cs_mask);
    }
    if (result == NX_SUCCESS &&
        (NX_H743_READ(&e->cs->registers->ODR) & e->cs_mask) != 0U)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS) {
        NX_H743_WRITE(&p->registers->CR1, SPI_CR1_SSI | SPI_CR1_SPE);
        nx_arch_dsb();
        if (NX_H743_READ(&p->registers->CR1) != (SPI_CR1_SSI | SPI_CR1_SPE))
            result = NX_ERROR_IO;
        else
            NX_H743_WRITE(&p->registers->CR1,
                          SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART);
    }
    size_t submitted = 0U;
    for (size_t index = 0U; result == NX_SUCCESS && index < length; ++index) {
        result = wait(p, SPI_SR_TXP, SPI_SR_TXP, deadline);
        if (result != NX_SUCCESS)
            break;
        NX_H743_SPI_WRITE8(&p->registers->TXDR, tx == NULL ? 0xffU : tx[index]);
        ++submitted;
        result = wait(p, SPI_SR_RXP, SPI_SR_RXP, deadline);
        if (result != NX_SUCCESS)
            break;
        uint8_t value = NX_H743_SPI_READ8(&p->registers->RXDR);
        if (rx != NULL)
            rx[index] = value;
        ++*transferred;
    }
    if (result == NX_SUCCESS)
        result = wait(p, SPI_SR_EOT | SPI_SR_TXC | FIFO_LEVELS,
                      SPI_SR_EOT | SPI_SR_TXC, deadline);
    if (result != NX_SUCCESS)
        p->fault = true;
    if (drain(p, divisor, rx, submitted, transferred) != NX_SUCCESS ||
        release_cs(p) != NX_SUCCESS || sck_idle(p) != NX_SUCCESS ||
        reset(p) != NX_SUCCESS)
        retain_and_park(p);
    p->active = false;
    if (result == NX_SUCCESS && nx_deadline_expired(deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    return result;
}
static nx_result_t recover(void* context) {
    nx_stm32h743_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained || clock_ready(p) != NX_SUCCESS)
        return NX_ERROR_STATE;
    if (withdraw(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    NX_H743_WRITE(&p->registers->CFG1, BYTE_CONFIG);
    nx_arch_dsb();
    if (NX_H743_READ(&p->registers->CFG1) != BYTE_CONFIG)
        return NX_ERROR_IO;
    p->fault = false;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32h743_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    if (clock_ready(p) != NX_SUCCESS)
        return NX_ERROR_STATE;
    if (withdraw(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    p->initialized = false;
    p->retained = false;
    p->fault = false;
    return NX_SUCCESS;
}
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_stm32h743_spi_endpoint_state_t* e = context;
    return e != NULL && port != NULL && port->ops == &nx_stm32h743_spi_ops &&
           e->port == port->context;
}
const nx_spi_ops_t nx_stm32h743_spi_ops = {recover, NULL, NULL, stop, NULL};
const nx_spi_endpoint_ops_t nx_stm32h743_spi_endpoint_ops = {
    transfer, NULL, NULL, on_port, NULL};
