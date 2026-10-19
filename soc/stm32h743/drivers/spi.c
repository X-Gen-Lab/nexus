/**
 * \file            spi.c
 * \brief           H743 finite full-duplex byte FIFO polling transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include "stm32h7xx_ll_spi.h"
/* ES0392 2.22.5: CRC-disabled finite reception still requires CRCSIZE=7. */
#define BYTE_CONFIG (7U | (7U << SPI_CFG1_CRCSIZE_Pos))
#define FIFO_LEVELS ((uint32_t)(SPI_SR_RXPLVL | SPI_SR_RXWNE | SPI_SR_RXP))
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32h743_spi_state_t* p) {
    return p != NULL && p->registers != NULL && p->reset != NULL &&
           p->reset_mask != 0U &&
           (p->reset_mask & (p->reset_mask - 1U)) == 0U &&
           (p->clock_hz == 32000000U || p->clock_hz == 64000000U) &&
           p->poll_limit != 0U;
}
static bool clock_ready(const nx_stm32h743_spi_state_t* p) {
    RCC_TypeDef* r = g_nx_stm32h743_system.rcc;
    if (r == NULL || !g_nx_stm32h743_system.started ||
        g_nx_stm32h743_system.core_hz != p->clock_hz)
        return false;
    if (p->reset == &r->APB2RSTR && p->reset_mask == RCC_APB2RSTR_SPI4RST)
        return (NX_H743_READ(&r->APB2ENR) & RCC_APB2ENR_SPI4EN) != 0U &&
               (NX_H743_READ(&r->D2CCIP1R) & RCC_D2CCIP1R_SPI45SEL) == 0U;

    return false;
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
/** \brief Check loans after endpoint pointers and complete spans are valid. */
static bool disjoint_transfer(const nx_stm32h743_spi_endpoint_state_t* e,
                              const uint8_t* tx, uint8_t* rx, size_t length,
                              size_t* transferred) {
    if (protected(e, transferred, sizeof(*transferred)))
        return false;
    if (tx != NULL && (protected(e, tx, length) ||
                       overlap(transferred, sizeof(*transferred), tx, length)))
        return false;
    if (rx != NULL && (protected(e, rx, length) ||
                       overlap(transferred, sizeof(*transferred), rx, length)))
        return false;
    /* Exact in-place full duplex is valid; partial overlap is not. */
    return tx == NULL || rx == NULL || tx == rx ||
           !overlap(tx, length, rx, length);
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
        (NX_H743_READ(&p->registers->CR1) & SPI_CR1_SPE) != 0U ||
        (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Failed CS withdrawal remains a concrete lease through recovery. */
static nx_result_t release_cs(nx_stm32h743_spi_state_t* p) {
    if (p->held_cs == NULL)
        return NX_SUCCESS;
    const nx_gpio_port_t face = {&nx_stm32h743_gpio_ops, p->held_cs};
    nx_result_t result = nx_gpio_port_write(&face, p->held_cs_mask, 0U);
    nx_arch_dsb();
    if (result != NX_SUCCESS || (NX_H743_READ(&p->held_cs->registers->ODR) &
                                 p->held_cs_mask) != p->held_cs_mask)
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
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    p->retained = true;
    p->fault = false;
    NX_H743_WRITE(&p->registers->CFG1, BYTE_CONFIG);
    nx_arch_dsb();
    if (NX_H743_READ(&p->registers->CFG1) != BYTE_CONFIG)
        return NX_ERROR_IO;
    p->initialized = true;
    return NX_SUCCESS;
}
/**
 * \brief Poll a finite transfer while retaining its wire and CS responsibility.
 *
 * Storage validation precedes output or hardware writes. Started paths reset
 * the shifter before releasing CS; failed withdrawal parks with this frame
 * and the caller's storage retained, even after its deadline expires.
 */
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    const nx_stm32h743_spi_endpoint_state_t* e = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (e == NULL || !valid(e->port) || e->cs == NULL ||
        e->cs->registers == NULL)
        return NX_ERROR_INVALID;
    if (!span(transferred, sizeof(*transferred)) || length == 0U ||
        (tx == NULL && rx == NULL) || (tx != NULL && !span(tx, length)) ||
        (rx != NULL && !span(rx, length)))
        return NX_ERROR_INVALID;
    if (e->mode > 3U || e->frequency_hz == 0U || e->cs_mask == 0U ||
        (e->cs_mask & ~e->cs->mask) != 0U || !e->cs->output)
        return NX_ERROR_INVALID;
    if (!disjoint_transfer(e, tx, rx, length, transferred))
        return NX_ERROR_INVALID;
    *transferred = 0U;
    nx_stm32h743_spi_state_t* p = e->port;
    if (!p->initialized || !e->cs->initialized || p->fault || !clock_ready(p))
        return NX_ERROR_STATE;
    if (p->active || (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    /* SPI4 is APB2-clocked in the admitted reset kernel selector. Restrict
     * the reference to <=16MHz; electrical validation remains external. */
    if (length > 256U || deadline == NX_DEADLINE_NEVER ||
        e->frequency_hz > 16000000U)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    uint32_t divisor = 2U;
    uint32_t baud = 0U;
    while ((uint64_t)p->clock_hz > (uint64_t)e->frequency_hz * divisor &&
           divisor < 256U) {
        divisor *= 2U;
        ++baud;
    }
    if ((uint64_t)p->clock_hz > (uint64_t)e->frequency_hz * divisor)
        return NX_ERROR_UNSUPPORTED;
    /* Own the controller before programming it, but assert CS only after
     * configuration readback succeeds. CSTART follows observed CS assertion. */
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
    for (size_t index = 0U; result == NX_SUCCESS && index < length; ++index) {
        result = wait(p, SPI_SR_TXP, SPI_SR_TXP, deadline);
        if (result != NX_SUCCESS)
            break;
        NX_H743_SPI_WRITE8(&p->registers->TXDR, tx == NULL ? 0xffU : tx[index]);
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
    /* EOT/TXC are not sufficient for the erratum's final trailing edge.
     * The quiet guard and reset still run on an expired request deadline. */
    if ((NX_H743_READ(&p->registers->SR) & SPI_SR_EOT) != 0U)
        finish_clock(p, divisor);
    if (result != NX_SUCCESS)
        p->fault = true;
    if (reset(p) != NX_SUCCESS)
        retain_and_park(p);
    if (release_cs(p) != NX_SUCCESS)
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
    if (!p->retained || !clock_ready(p))
        return NX_ERROR_STATE;
    if (reset(p) != NX_SUCCESS || release_cs(p) != NX_SUCCESS)
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
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    if (reset(p) != NX_SUCCESS || release_cs(p) != NX_SUCCESS)
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
