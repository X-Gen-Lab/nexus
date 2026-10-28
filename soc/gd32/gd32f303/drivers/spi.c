/**
 * \file            spi.c
 * \brief           GD303 SPI0 finite polling and explicit active-low CS lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "nexus/core/fault.h"
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_gd32f303_spi_state_t* p) {
    return p != NULL &&
           p->registers == (nx_gd32f303_spi_registers_t*)(uintptr_t)SPI0 &&
           p->clock_hz == 8000000U && p->poll_limit != 0U;
}
static bool span(const void* p, size_t size) {
    return p != NULL && size != 0U && (uintptr_t)p <= UINTPTR_MAX - size;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected(const nx_gd32f303_spi_endpoint_state_t* e,
                      const void* data, size_t size) {
    return overlap(data, size, e, sizeof(*e)) ||
           overlap(data, size, e->port, sizeof(*e->port)) ||
           overlap(data, size, e->port->registers,
                   sizeof(*e->port->registers)) ||
           overlap(data, size, e->cs, sizeof(*e->cs)) ||
           overlap(data, size, (const void*)(uintptr_t)GPIOA, 0x40U) ||
           overlap(data, size, (const void*)(uintptr_t)RCU_BASE, 0x40U);
}
static nx_result_t reset(nx_gd32f303_spi_state_t* p) {
    NX_GD303_WRITE(&RCU_APB2RST,
                   NX_GD303_READ(&RCU_APB2RST) | RCU_APB2RST_SPI0RST);
    nx_arch_dsb();
    if ((NX_GD303_READ(&RCU_APB2RST) & RCU_APB2RST_SPI0RST) == 0U)
        return NX_ERROR_IO;
    NX_GD303_WRITE(&RCU_APB2RST, NX_GD303_READ(&RCU_APB2RST) &
                                     ~(uint32_t)RCU_APB2RST_SPI0RST);
    nx_arch_dsb();
    if ((NX_GD303_READ(&RCU_APB2RST) & RCU_APB2RST_SPI0RST) != 0U ||
        NX_GD303_READ(&p->registers->CTL0) != 0U ||
        NX_GD303_READ(&p->registers->CTL1) != 0U ||
        NX_GD303_READ(&p->registers->I2SCTL) != 0U ||
        (NX_GD303_READ(&p->registers->STAT) &
         (SPI_STAT_TRANS | SPI_STAT_RBNE)) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Retain the accepted interval if wire or CS withdrawal is unproved. */
static _Noreturn void retain_and_park(nx_gd32f303_spi_state_t* p) {
    p->fault = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static nx_result_t release_cs(nx_gd32f303_spi_state_t* p) {
    if (p->held_cs == NULL)
        return NX_SUCCESS;
    const nx_gpio_port_t face = {&nx_gd32f303_gpio_ops, p->held_cs};
    nx_result_t result = nx_gpio_port_write(&face, p->held_cs_mask, 0U);
    nx_arch_dsb();
    if (result != NX_SUCCESS ||
        (NX_GD303_READ(&GPIO_OCTL(GPIOA)) & p->held_cs_mask) != p->held_cs_mask)
        return NX_ERROR_IO;
    p->held_cs = NULL;
    p->held_cs_mask = 0U;
    return NX_SUCCESS;
}
static nx_result_t wait(nx_gd32f303_spi_state_t* p, uint32_t mask,
                        uint32_t expected, nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_GD303_SPI_POLL();
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        uint32_t status = NX_GD303_READ(&p->registers->STAT);
        if ((status & (SPI_STAT_CONFERR | SPI_STAT_RXORERR | SPI_STAT_FERR)) !=
            0U)
            return NX_ERROR_IO;
        if ((status & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
nx_result_t nx_gd32f303_spi_initialize(nx_gd32f303_spi_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->active || p->held_cs != NULL ||
        NX_GD303_READ(&p->registers->CTL0) != 0U ||
        NX_GD303_READ(&p->registers->CTL1) != 0U ||
        NX_GD303_READ(&p->registers->I2SCTL) != 0U ||
        !NX_GD303_IRQ_AVAILABLE(SPI0_IRQn) ||
        (NX_GD303_READ(&p->registers->STAT) &
         (SPI_STAT_TRANS | SPI_STAT_RBNE)) != 0U)
        return NX_ERROR_BUSY;
    if (!g_nx_gd32f303_system.started ||
        g_nx_gd32f303_system.core_hz != p->clock_hz ||
        (NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_SPI0EN) == 0U)
        return NX_ERROR_STATE;
    p->retained = true;
    p->fault = false;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    const nx_gd32f303_spi_endpoint_state_t* e = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (e == NULL || !valid(e->port) || e->cs == NULL ||
        !span(transferred, sizeof(*transferred)) || length == 0U ||
        (tx == NULL && rx == NULL) || (tx != NULL && !span(tx, length)) ||
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
    nx_gd32f303_spi_state_t* p = e->port;
    if (!p->initialized || !e->cs->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_GD303_READ(&p->registers->STAT) &
                      (SPI_STAT_TRANS | SPI_STAT_RBNE)) != 0U)
        return NX_ERROR_BUSY;
    if (length > 256U || deadline == NX_DEADLINE_NEVER ||
        e->frequency_hz > 4000000U)
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
    p->active = true;
    uint32_t config = SPI_CTL0_MSTMOD | SPI_CTL0_SWNSSEN | SPI_CTL0_SWNSS |
                      (baud << 3U) |
                      ((e->mode & 1U) != 0U ? SPI_CTL0_CKPH : 0U) |
                      ((e->mode & 2U) != 0U ? SPI_CTL0_CKPL : 0U);
    NX_GD303_WRITE(&p->registers->CTL0, config);
    nx_arch_dsb();
    nx_result_t result = NX_SUCCESS;
    if (NX_GD303_READ(&p->registers->CTL0) != config ||
        NX_GD303_READ(&p->registers->CTL1) != 0U)
        result = NX_ERROR_IO;
    const nx_gpio_port_t cs = {&nx_gd32f303_gpio_ops, e->cs};
    if (result == NX_SUCCESS) {
        p->held_cs = e->cs;
        p->held_cs_mask = e->cs_mask;
        result = nx_gpio_port_write(&cs, 0U, e->cs_mask);
    }
    if (result == NX_SUCCESS &&
        (NX_GD303_READ(&GPIO_OCTL(GPIOA)) & e->cs_mask) != 0U)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS) {
        NX_GD303_WRITE(&p->registers->CTL0, config | SPI_CTL0_SPIEN);
        nx_arch_dsb();
        if (NX_GD303_READ(&p->registers->CTL0) != (config | SPI_CTL0_SPIEN))
            result = NX_ERROR_IO;
    }
    for (size_t index = 0U; result == NX_SUCCESS && index < length; ++index) {
        result = wait(p, SPI_STAT_TBE, SPI_STAT_TBE, deadline);
        if (result != NX_SUCCESS)
            break;
        NX_GD303_WRITE(&p->registers->DATA, tx == NULL ? 0xffU : tx[index]);
        result = wait(p, SPI_STAT_RBNE, SPI_STAT_RBNE, deadline);
        if (result != NX_SUCCESS)
            break;
        uint8_t value = (uint8_t)NX_GD303_READ(&p->registers->DATA);
        if (rx != NULL)
            rx[index] = value;
        ++*transferred;
    }
    if (result == NX_SUCCESS)
        result = wait(p, SPI_STAT_TRANS, 0U, deadline);
    if (result != NX_SUCCESS) {
        p->fault = true;
        if (reset(p) != NX_SUCCESS)
            retain_and_park(p);
    } else {
        NX_GD303_WRITE(&p->registers->CTL0, config);
        nx_arch_dsb();
        if ((NX_GD303_READ(&p->registers->CTL0) & SPI_CTL0_SPIEN) != 0U) {
            p->fault = true;
            result = NX_ERROR_IO;
            if (reset(p) != NX_SUCCESS)
                retain_and_park(p);
        }
    }
    if (release_cs(p) != NX_SUCCESS)
        retain_and_park(p);
    p->active = false;
    if (result == NX_SUCCESS && nx_deadline_expired(deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    return result;
}
static nx_result_t recover(void* context) {
    nx_gd32f303_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_ERROR_STATE;
    if (reset(p) != NX_SUCCESS || release_cs(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    p->fault = false;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_gd32f303_spi_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    if (reset(p) != NX_SUCCESS || release_cs(p) != NX_SUCCESS)
        return NX_ERROR_IO;
    p->initialized = p->retained = p->fault = false;
    return NX_SUCCESS;
}
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_gd32f303_spi_endpoint_state_t* e = context;
    return e != NULL && port != NULL && port->ops == &nx_gd32f303_spi_ops &&
           e->port == port->context;
}
const nx_spi_ops_t nx_gd32f303_spi_ops = {recover, NULL, NULL, stop, NULL};
const nx_spi_endpoint_ops_t nx_gd32f303_spi_endpoint_ops = {
    transfer, NULL, NULL, on_port, NULL};

nx_result_t nx_gd32f303_spi_prepare(nx_gd32f303_spi_state_t* p,
                                    nx_gd32f303_gpio_state_t* cs) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || cs == NULL) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized || p->active || p->held_cs != NULL ||
        cs->retained || cs->initialized) {
        return NX_ERROR_BUSY;
    }
    p->registers = (nx_gd32f303_spi_registers_t*)(uintptr_t)SPI0;
    p->clock_hz = 8000000U;
    p->poll_limit = 1000000U;
    nx_result_t result = nx_gd32f303_spi_initialize(p);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_gd32f303_gpio_prepare(cs, 16U, true, 16U);
    return result == NX_SUCCESS ? nx_gd32f303_spi0_pins_prepare() : result;
}
nx_result_t nx_gd32f303_spi_close(nx_gd32f303_spi_state_t* p,
                                  nx_gd32f303_gpio_state_t* cs) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || cs == NULL) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized || p->active || p->held_cs != NULL) {
        nx_result_t result = stop(p);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return cs->retained ? nx_gd32f303_gpio_stop(cs, 16U) : NX_SUCCESS;
}
