/**
 * \file            spi.c
 * \brief           H743 finite full-duplex byte FIFO polling transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32h743_cold.h"
#include "stm32h743_spi_wire.h"
#include "stm32h7xx_ll_spi.h"
/* ES0392 2.22.5: CRC-disabled finite reception still requires CRCSIZE=7. */
#define BYTE_CONFIG NX_STM32H743_SPI_BYTE_CONFIG
#define FIFO_LEVELS NX_STM32H743_SPI_FIFO_LEVELS
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
    nx_time_us_t quiet = nx_deadline_after(
        nx_time_now_us(), nx_stm32h743_spi_wire_quiet_us(p->clock_hz, divisor));
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
        nx_stm32h743_spi_wire_suspend_once(p);
        uint32_t remaining = p->poll_limit;
        bool stopped = false;
        do {
            NX_H743_SPI_POLL(p);
            if (nx_stm32h743_spi_wire_stopped(p)) {
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
    return nx_stm32h743_spi_wire_disable(p);
}
/** \brief No live caller frame exists on an idle recover/stop path. */
static nx_result_t withdraw(nx_stm32h743_spi_state_t* p) {
    const uint32_t divisor =
        2U << ((NX_H743_READ(&p->registers->CFG1) & SPI_CFG1_MBR) >>
               SPI_CFG1_MBR_Pos);
    if (drain(p, divisor, NULL, 0U, NULL) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_release_cs(p) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_sck_idle(p) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_reset(p) != NX_SUCCESS) {
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
    const nx_result_t readiness = nx_stm32h743_spi_wire_clock_ready(p);
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
/**
 * \brief Poll a finite transfer while retaining its wire and CS responsibility.
 *
 * Storage validation precedes output or hardware writes. Started paths prove
 * wire stop and disable before releasing CS; failed withdrawal parks with this
 * frame and the caller's storage retained, even after its deadline expires.
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
    if (!p->initialized || !e->cs->initialized || p->fault ||
        nx_stm32h743_spi_wire_clock_ready(p) != NX_SUCCESS)
        return NX_ERROR_STATE;
    if (p->active || (NX_H743_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    if (!nx_stm32h743_spi_wire_pins_ready() ||
        !nx_stm32h743_spi_wire_cs_driven(e->cs, e->cs_mask) ||
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
    nx_result_t selected = nx_stm32h743_spi_wire_divider(
        p->clock_hz, e->frequency_hz, &divisor, &baud);
    if (selected != NX_SUCCESS)
        return selected;
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
    if (result == NX_SUCCESS &&
        nx_stm32h743_spi_wire_sck_alternate() != NX_SUCCESS)
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
        nx_stm32h743_spi_wire_release_cs(p) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_sck_idle(p) != NX_SUCCESS ||
        nx_stm32h743_spi_wire_reset(p) != NX_SUCCESS)
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
    if (!p->retained || nx_stm32h743_spi_wire_clock_ready(p) != NX_SUCCESS)
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
    if (nx_stm32h743_spi_wire_clock_ready(p) != NX_SUCCESS)
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

nx_result_t nx_stm32h743_spi_prepare(nx_stm32h743_spi_state_t* port,
                                     const nx_stm32h743_spi_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->endpoints == NULL ||
        config->count == 0U || config->count > UINT16_MAX)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->active || port->cold_owned)
        return NX_ERROR_BUSY;
    /* Validate the complete endpoint set before enabling the wire or changing
     * an earlier CS. A later rejection cannot authorize partial outputs. */
    for (size_t i = 0U; i < config->count; ++i) {
        const nx_stm32h743_spi_endpoint_config_t* item = &config->endpoints[i];
        if (item->cs == NULL || item->endpoint == NULL || item->gpio == NULL ||
            item->mask == 0U || (item->mask & ~0xFFFFU) != 0U ||
            item->mode > 3U || item->frequency_hz == 0U)
            return NX_ERROR_INVALID;
        if (item->cs->retained || item->cs->initialized)
            return NX_ERROR_BUSY;
        for (size_t j = 0U; j < i; ++j)
            if (item->cs == config->endpoints[j].cs ||
                item->endpoint == config->endpoints[j].endpoint)
                return NX_ERROR_INVALID;
    }
    port->registers = SPI4;
    port->clock_hz = config->clock_hz;
    port->poll_limit = 1000000U;
    port->reset = &RCC->APB2RSTR;
    port->reset_mask = RCC_APB2RSTR_SPI4RST;
    port->cold_owned = true;
    port->cold_acquired = 0U;
    nx_result_t result = nx_stm32h743_spi_initialize(port);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < config->count; ++i) {
        const nx_stm32h743_spi_endpoint_config_t* item = &config->endpoints[i];
        nx_stm32h743_gpio_state_t* cs = item->cs;
        if (cs == NULL || item->endpoint == NULL)
            return NX_ERROR_INVALID;
        if (cs->retained || cs->initialized)
            return NX_ERROR_BUSY;
        cs->registers = item->gpio;
        cs->mask = item->mask;
        cs->output = true;
        /* This child owns cleanup even if its first pad write is refused. */
        port->cold_acquired = (uint16_t)(i + 1U);
        result = nx_stm32h743_gpio_initialize(cs, item->mask);
        if (result != NX_SUCCESS)
            return result;
        item->endpoint->port = port;
        item->endpoint->cs = cs;
        item->endpoint->cs_mask = item->mask;
        item->endpoint->frequency_hz = item->frequency_hz;
        item->endpoint->mode = item->mode;
    }
    for (unsigned pin = 12U; pin <= 14U; ++pin) {
        result = nx_stm32h743_pin_alternate(GPIOE, pin, 5U);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32h743_spi_release(nx_stm32h743_spi_state_t* port,
                                     const nx_stm32h743_spi_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->endpoints == NULL ||
        config->count == 0U)
        return NX_ERROR_INVALID;
    if (!port->cold_owned)
        return NX_SUCCESS;
    if (port->cold_acquired > config->count)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32h743_spi_ops.stop(port);
    if (result != NX_SUCCESS)
        return result;
    /* The wire may already be stopped while a CS still owes withdrawal. */
    while (port->cold_acquired != 0U) {
        const nx_stm32h743_spi_endpoint_config_t* item =
            &config->endpoints[port->cold_acquired - 1U];
        result = nx_stm32h743_gpio_stop(item->cs, item->mask);
        if (result != NX_SUCCESS)
            return result;
        --port->cold_acquired;
    }
    port->cold_owned = false;
    return NX_SUCCESS;
}
