/**
 * \file            spi.c
 * \brief           F103 SPI1 finite polling, wire drain and exact static CS
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32f103_provider.h"
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
static bool span(const void* p, size_t size) {
    return p != NULL && size != 0U && (uintptr_t)p <= UINTPTR_MAX - size;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected_span(const nx_stm32f103_spi_endpoint_state_t* e,
                           const void* p, size_t size) {
    return overlap(p, size, e, sizeof(*e)) ||
           overlap(p, size, e->port, sizeof(*e->port)) ||
           overlap(p, size, e->port->registers, sizeof(*e->port->registers)) ||
           overlap(p, size, e->port->rcc, sizeof(*e->port->rcc)) ||
           overlap(p, size, e->cs, sizeof(*e->cs)) ||
           overlap(p, size, e->cs->registers, sizeof(*e->cs->registers));
}
static bool reset_controller(nx_stm32f103_spi_state_t* p) {
    uint32_t value = NX_F103_READ(&p->rcc->APB2RSTR);
    if (!checked(&p->rcc->APB2RSTR, value | RCC_APB2RSTR_SPI1RST,
                 RCC_APB2RSTR_SPI1RST)) {
        return false;
    }
    if (!checked(&p->rcc->APB2RSTR, value & ~(uint32_t)RCC_APB2RSTR_SPI1RST,
                 RCC_APB2RSTR_SPI1RST)) {
        return false;
    }
    return NX_F103_READ(&p->registers->CR1) == 0U &&
           NX_F103_READ(&p->registers->CR2) == 0U &&
           (NX_F103_READ(&p->registers->SR) & SPI_SR_BSY) == 0U;
}
static _Noreturn void retain_and_park(nx_stm32f103_spi_state_t* p) {
    p->fault = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
nx_result_t nx_stm32f103_spi_initialize(nx_stm32f103_spi_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || p->rcc == NULL)
        return NX_ERROR_INVALID;
    if (p->clock_hz != 8000000U)
        return NX_ERROR_UNSUPPORTED;
    if (p->initialized || p->retained || p->active ||
        NX_F103_READ(&p->registers->CR1) != 0U ||
        NX_F103_READ(&p->registers->CR2) != 0U ||
        (NX_F103_READ(&p->registers->SR) &
         (SPI_SR_BSY | SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_MODF)) != 0U)
        return NX_ERROR_BUSY;
    if (!g_nx_stm32f103_system.started ||
        (NX_F103_READ(&p->rcc->APB2ENR) &
         (RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN)) !=
            (RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN) ||
        (NX_F103_READ(&NX_F103_AFIO->MAPR) & AFIO_MAPR_SPI1_REMAP) != 0U)
        return NX_ERROR_STATE;
    p->retained = true;
    /* PA5 SCK/PA7 MOSI alternate push-pull50MHz; PA6 floating MISO. The
     * platform already admitted and leased every nibble, including child CS. */
    uint32_t mask = (15U << 20U) | (15U << 24U) | (15U << 28U);
    uint32_t modes = (11U << 20U) | (4U << 24U) | (11U << 28U);
    if (!checked(&NX_F103_GPIOA->CRL,
                 (NX_F103_READ(&NX_F103_GPIOA->CRL) & ~mask) | modes, mask) ||
        !checked(&p->registers->CR1, SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI,
                 UINT32_MAX) ||
        !checked(&p->registers->CR2, 0U, UINT32_MAX))
        return NX_ERROR_IO;
    p->initialized = true;
    p->fault = false;
    return NX_SUCCESS;
}
static nx_result_t wait_status(nx_stm32f103_spi_state_t* p, uint32_t mask,
                               uint32_t expected, nx_time_us_t deadline) {
    for (;;) {
        NX_F103_SPI_POLL(p);
        uint32_t status = NX_F103_READ(&p->registers->SR);
        if ((status & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_CRCERR)) != 0U)
            return NX_ERROR_IO;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        if ((status & mask) == expected)
            return NX_SUCCESS;
    }
}
static nx_result_t cs_level(nx_stm32f103_spi_endpoint_state_t* e, bool high) {
    NX_F103_WRITE(&e->cs->registers->BSRR,
                  high ? e->cs_mask : e->cs_mask << 16U);
    nx_arch_dsb();
    return (NX_F103_READ(&e->cs->registers->ODR) & e->cs_mask) ==
                   (high ? e->cs_mask : 0U)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    nx_stm32f103_spi_endpoint_state_t* e = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (e == NULL || e->port == NULL || e->cs == NULL ||
        e->port->registers == NULL || e->port->rcc == NULL ||
        e->cs->registers == NULL || !span(transferred, sizeof(*transferred)) ||
        length == 0U || (tx == NULL && rx == NULL) || e->mode > 3U ||
        e->frequency_hz == 0U || e->cs_mask != 16U ||
        (e->cs_mask & ~e->cs->mask) != 0U ||
        protected_span(e, transferred, sizeof(*transferred)) ||
        (tx != NULL &&
         (!span(tx, length) || protected_span(e, tx, length) ||
          overlap(tx, length, transferred, sizeof(*transferred)))) ||
        (rx != NULL &&
         (!span(rx, length) || protected_span(e, rx, length) ||
          overlap(rx, length, transferred, sizeof(*transferred)))) ||
        (tx != NULL && rx != NULL && tx != rx &&
         overlap(tx, length, rx, length)))
        return NX_ERROR_INVALID;
    if (length > 256U || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    nx_stm32f103_spi_state_t* p = e->port;
    if (!p->initialized || !e->cs->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_F103_READ(&p->registers->SR) & SPI_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    uint32_t divider = 2U;
    uint32_t baud = 0U;
    while (p->clock_hz / divider > e->frequency_hz && divider < 256U) {
        divider *= 2U;
        ++baud;
    }
    if (p->clock_hz / divider > e->frequency_hz)
        return NX_ERROR_UNSUPPORTED;
    *transferred = 0U;
    p->active = true;
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (baud << 3U) |
                   ((e->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                   ((e->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U);
    nx_result_t result = NX_SUCCESS;
    if (!checked(&p->registers->CR1, cr1 | SPI_CR1_SPE, UINT32_MAX) ||
        cs_level(e, false) != NX_SUCCESS) {
        result = NX_ERROR_IO;
    }
    if (result == NX_SUCCESS) {
        for (size_t i = 0U; i < length; ++i) {
            result = wait_status(p, SPI_SR_TXE, SPI_SR_TXE, deadline);
            if (result != NX_SUCCESS)
                break;
            NX_F103_SPI_WRITE_DATA(p->registers, tx == NULL ? 0xffU : tx[i]);
            result = wait_status(p, SPI_SR_RXNE, SPI_SR_RXNE, deadline);
            if (result != NX_SUCCESS)
                break;
            uint8_t byte = NX_F103_SPI_READ_DATA(p->registers);
            if (rx != NULL)
                rx[i] = byte;
            ++*transferred;
        }
    }
    if (result == NX_SUCCESS)
        result = wait_status(p, SPI_SR_BSY, 0U, deadline);
    if (result != NX_SUCCESS) {
        p->fault = true;
        if (!reset_controller(p))
            retain_and_park(p);
    } else if (!checked(&p->registers->CR1, cr1, UINT32_MAX)) {
        p->fault = true;
        result = NX_ERROR_IO;
        if (!reset_controller(p))
            retain_and_park(p);
    }
    if (cs_level(e, true) != NX_SUCCESS) {
        /* Returning would falsely release the whole-CS interval. Retain the
         * executor and caller buffers for the product's terminal recovery. */
        retain_and_park(p);
    }
    p->active = false;
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        result = NX_ERROR_TIMEOUT;
    }
    return result;
}
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_stm32f103_spi_endpoint_state_t* e = context;
    return e != NULL && port != NULL && port->ops == &nx_stm32f103_spi_ops &&
           port->context == e->port;
}
static nx_result_t recover(void* context) {
    nx_stm32f103_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized)
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!reset_controller(p))
        return NX_ERROR_IO;
    p->fault = false;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32f103_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!checked(&p->registers->CR2, 0U, UINT32_MAX) ||
        !checked(&p->registers->CR1, 0U, UINT32_MAX) ||
        (NX_F103_READ(&p->registers->SR) & SPI_SR_BSY) != 0U)
        return NX_ERROR_IO;
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
const nx_spi_ops_t nx_stm32f103_spi_ops = {.recover = recover, .stop = stop};
const nx_spi_endpoint_ops_t nx_stm32f103_spi_endpoint_ops = {
    .transfer = transfer, .on_port = on_port};

nx_result_t nx_stm32f103_spi_prepare(nx_stm32f103_spi_state_t* p,
                                     nx_stm32f103_gpio_state_t* cs) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || cs == NULL) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized || p->active || cs->retained ||
        cs->initialized) {
        return NX_ERROR_BUSY;
    }
    p->registers = SPI1;
    p->rcc = RCC;
    p->clock_hz = 8000000U;
    nx_result_t result = nx_stm32f103_gpio_prepare(cs, 16U, true, 16U);
    return result == NX_SUCCESS ? nx_stm32f103_spi_initialize(p) : result;
}
nx_result_t nx_stm32f103_spi_close(nx_stm32f103_spi_state_t* p,
                                   nx_stm32f103_gpio_state_t* cs) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || cs == NULL) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized || p->active) {
        nx_result_t result = stop(p);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return cs->retained ? nx_stm32f103_gpio_stop(cs, 16U) : NX_SUCCESS;
}
