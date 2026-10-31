/**
 * \file            spi.c
 * \brief           U575 finite full-duplex byte FIFO polling transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"
#include "stm32u5xx_ll_spi.h"
#ifndef NX_U575_SPI_READ8
#define NX_U575_SPI_READ8(port) LL_SPI_ReceiveData8(port)
#endif
#ifndef NX_U575_SPI_WRITE8
#define NX_U575_SPI_WRITE8(port, value) LL_SPI_TransmitData8((port), (value))
#endif
#define BYTE_CONFIG (LL_SPI_DATAWIDTH_8BIT | (7U << SPI_CFG1_CRCSIZE_Pos))
#define FIFO_LEVELS ((uint32_t)(SPI_SR_RXPLVL | SPI_SR_RXWNE | SPI_SR_RXP))
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_spi_state_t* p) {
    return p != NULL && p->registers != NULL && p->reset != NULL &&
           p->reset_mask != 0U &&
           (p->reset_mask & (p->reset_mask - 1U)) == 0U &&
           p->clock_hz == 4000000U && p->poll_limit != 0U;
}
static bool clock_ready(const nx_stm32u575_spi_state_t* p) {
    RCC_TypeDef* r = g_nx_stm32u575_system.rcc;
    if (r == NULL || !g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != p->clock_hz)
        return false;
    if (p->reset == &r->APB2RSTR && p->reset_mask == RCC_APB2RSTR_SPI1RST)
        return (NX_U575_READ(&r->APB2ENR) & RCC_APB2ENR_SPI1EN) != 0U &&
               (NX_U575_READ(&r->CCIPR1) & RCC_CCIPR1_SPI1SEL) == 0U;

    return false;
}
static bool span(const void* value, size_t size) {
    return value != NULL && size != 0U &&
           (uintptr_t)value <= UINTPTR_MAX - size;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool protected(const nx_stm32u575_spi_endpoint_state_t* e,
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
static bool disjoint_transfer(const nx_stm32u575_spi_endpoint_state_t* e,
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
/** \brief U5 reset stops its FIFO shifter; verify both reset edges and idle. */
static nx_result_t reset(nx_stm32u575_spi_state_t* p) {
    NX_U575_WRITE(p->reset, NX_U575_READ(p->reset) | p->reset_mask);
    nx_arch_dsb();
    if ((NX_U575_READ(p->reset) & p->reset_mask) == 0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(p->reset, NX_U575_READ(p->reset) & ~p->reset_mask);
    nx_arch_dsb();
    if ((NX_U575_READ(p->reset) & p->reset_mask) != 0U ||
        (NX_U575_READ(&p->registers->CR1) & SPI_CR1_SPE) != 0U ||
        (NX_U575_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Failed CS withdrawal remains a concrete lease through recovery. */
static nx_result_t release_cs(nx_stm32u575_spi_state_t* p) {
    if (p->held_cs == NULL)
        return NX_SUCCESS;
    const nx_gpio_port_t face = {&nx_stm32u575_gpio_ops, p->held_cs};
    nx_result_t result = nx_gpio_port_write(&face, p->held_cs_mask, 0U);
    nx_arch_dsb();
    if (result != NX_SUCCESS || (NX_U575_READ(&p->held_cs->registers->ODR) &
                                 p->held_cs_mask) != p->held_cs_mask)
        return NX_ERROR_IO;
    p->held_cs = NULL;
    p->held_cs_mask = 0U;
    return NX_SUCCESS;
}
/** \brief Polling cannot return while wire/CS withdrawal remains unproved. */
static _Noreturn void retain_and_park(nx_stm32u575_spi_state_t* p) {
    p->fault = true;
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
static nx_result_t wait(nx_stm32u575_spi_state_t* p, uint32_t mask,
                        uint32_t expected, nx_time_us_t deadline) {
    uint32_t remaining = p->poll_limit;
    do {
        NX_U575_SPI_POLL(p);
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        uint32_t status = NX_U575_READ(&p->registers->SR);
        if ((status & (SPI_SR_MODF | SPI_SR_OVR | SPI_SR_UDR | SPI_SR_TIFRE)) !=
            0U)
            return NX_ERROR_IO;
        if ((status & mask) == expected)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
nx_result_t nx_stm32u575_spi_initialize(nx_stm32u575_spi_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized || p->active || p->held_cs != NULL ||
        NX_U575_READ(&p->registers->CR1) != 0U ||
        NX_U575_READ(&p->registers->CR2) != 0U ||
        NX_U575_READ(&p->registers->IER) != 0U ||
        (NX_U575_READ(&p->registers->CFG1) &
         ~(uint32_t)(SPI_CFG1_DSIZE | SPI_CFG1_CRCSIZE)) != 0U ||
        NX_U575_READ(&p->registers->CFG2) != 0U ||
        (NX_U575_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    p->retained = true;
    p->fault = false;
    NX_U575_WRITE(&p->registers->CFG1, BYTE_CONFIG);
    nx_arch_dsb();
    if (NX_U575_READ(&p->registers->CFG1) != BYTE_CONFIG ||
        LL_SPI_GetDataWidth(p->registers) != LL_SPI_DATAWIDTH_8BIT)
        return NX_ERROR_IO;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    const nx_stm32u575_spi_endpoint_state_t* e = context;
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
    nx_stm32u575_spi_state_t* p = e->port;
    if (!p->initialized || !e->cs->initialized || p->fault)
        return NX_ERROR_STATE;
    if (p->active || (NX_U575_READ(&p->registers->SR) & FIFO_LEVELS) != 0U)
        return NX_ERROR_BUSY;
    /* SPI1 is PCLK2-clocked in the admitted reset kernel selector. Restrict
     * the reference to <=2MHz; electrical validation remains external. */
    if (length > 256U || deadline == NX_DEADLINE_NEVER ||
        e->frequency_hz > 2000000U)
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
    uint32_t config = SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_AFCNTR |
                      ((e->mode & 1U) != 0U ? SPI_CFG2_CPHA : 0U) |
                      ((e->mode & 2U) != 0U ? SPI_CFG2_CPOL : 0U);
    uint32_t config1 = BYTE_CONFIG | (baud << SPI_CFG1_MBR_Pos);
    NX_U575_WRITE(&p->registers->CR1, SPI_CR1_SSI);
    NX_U575_WRITE(&p->registers->CFG1, config1);
    NX_U575_WRITE(&p->registers->CFG2, config);
    NX_U575_WRITE(&p->registers->CR2, (uint32_t)length);
    NX_U575_WRITE(&p->registers->IFCR, SPI_IFCR_EOTC | SPI_IFCR_TXTFC);
    nx_arch_dsb();
    nx_result_t result = NX_SUCCESS;
    if (NX_U575_READ(&p->registers->CFG2) != config ||
        NX_U575_READ(&p->registers->CFG1) != config1 ||
        NX_U575_READ(&p->registers->CR2) != length ||
        NX_U575_READ(&p->registers->CR1) != SPI_CR1_SSI)
        result = NX_ERROR_IO;
    const nx_gpio_port_t cs = {&nx_stm32u575_gpio_ops, e->cs};
    if (result == NX_SUCCESS) {
        p->held_cs = e->cs;
        p->held_cs_mask = e->cs_mask;
        result = nx_gpio_port_write(&cs, 0U, e->cs_mask);
    }
    if (result == NX_SUCCESS &&
        (NX_U575_READ(&e->cs->registers->ODR) & e->cs_mask) != 0U)
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS) {
        NX_U575_WRITE(&p->registers->CR1, SPI_CR1_SSI | SPI_CR1_SPE);
        nx_arch_dsb();
        if (NX_U575_READ(&p->registers->CR1) != (SPI_CR1_SSI | SPI_CR1_SPE))
            result = NX_ERROR_IO;
        else
            NX_U575_WRITE(&p->registers->CR1,
                          SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART);
    }
    for (size_t index = 0U; result == NX_SUCCESS && index < length; ++index) {
        result = wait(p, SPI_SR_TXP, SPI_SR_TXP, deadline);
        if (result != NX_SUCCESS)
            break;
        NX_U575_SPI_WRITE8(p->registers, tx == NULL ? 0xffU : tx[index]);
        result = wait(p, SPI_SR_RXP, SPI_SR_RXP, deadline);
        if (result != NX_SUCCESS)
            break;
        uint8_t value = NX_U575_SPI_READ8(p->registers);
        if (rx != NULL)
            rx[index] = value;
        ++*transferred;
    }
    if (result == NX_SUCCESS)
        result = wait(p, SPI_SR_EOT | SPI_SR_TXC | FIFO_LEVELS,
                      SPI_SR_EOT | SPI_SR_TXC, deadline);
    if (result == NX_SUCCESS) {
        /* ES0499 Rev12 2.23.7: EOT samples the last bit before its complete
         * SCK period. Keep outputs owned for one actual SCK period plus two
         * TIM2 ticks; the extra tick covers the unknown snapshot phase. */
        nx_time_us_t hold =
            ((uint64_t)divisor * 1000000U + p->clock_hz - 1U) / p->clock_hz +
            2U;
        nx_time_us_t settled = nx_deadline_after(nx_time_now_us(), hold);
        uint32_t remaining = p->poll_limit;
        while (!nx_deadline_expired(settled, nx_time_now_us())) {
            NX_U575_SPI_POLL(p);
            if (--remaining == 0U &&
                !nx_deadline_expired(settled, nx_time_now_us())) {
                result = NX_ERROR_TIMEOUT;
                break;
            }
        }
    }
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
    nx_stm32u575_spi_state_t* p = context;
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
    NX_U575_WRITE(&p->registers->CFG1, BYTE_CONFIG);
    nx_arch_dsb();
    if (NX_U575_READ(&p->registers->CFG1) != BYTE_CONFIG)
        return NX_ERROR_IO;
    p->fault = false;
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32u575_spi_state_t* p = context;
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
    const nx_stm32u575_spi_endpoint_state_t* e = context;
    return e != NULL && port != NULL && port->ops == &nx_stm32u575_spi_ops &&
           e->port == port->context;
}
const nx_spi_ops_t nx_stm32u575_spi_ops = {recover, NULL, NULL, stop, NULL};
const nx_spi_endpoint_ops_t nx_stm32u575_spi_endpoint_ops = {
    transfer, NULL, NULL, on_port, NULL};

nx_result_t nx_stm32u575_spi_prepare(nx_stm32u575_spi_state_t* port,
                                     const nx_stm32u575_spi_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->endpoints == NULL ||
        config->endpoint_count == 0U || config->endpoint_count > UINT16_MAX ||
        config->pins == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized || port->active || port->cold_owned)
        return NX_ERROR_BUSY;
    /* Validate every static child before acquiring the first wire effect. */
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        const nx_stm32u575_spi_endpoint_config_t* endpoint =
            &config->endpoints[i];
        if (endpoint->cs == NULL || endpoint->endpoint == NULL ||
            endpoint->registers == NULL || endpoint->mask == 0U ||
            (endpoint->mask & ~UINT32_C(0xFFFF)) != 0U ||
            endpoint->frequency_hz < 15625U ||
            endpoint->frequency_hz > 2000000U || endpoint->mode > 3U)
            return NX_ERROR_INVALID;
        if (endpoint->cs->retained || endpoint->cs->initialized)
            return NX_ERROR_BUSY;
        for (size_t j = 0U; j < i; ++j)
            if (endpoint->cs == config->endpoints[j].cs ||
                endpoint->endpoint == config->endpoints[j].endpoint)
                return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < config->pin_count; ++i)
        if (config->pins[i].registers == NULL || config->pins[i].pin > 15U ||
            config->pins[i].alternate > 15U)
            return NX_ERROR_INVALID;
    port->registers = SPI1;
    port->clock_hz = 4000000U;
    port->poll_limit = 1000000U;
    port->reset = &RCC->APB2RSTR;
    port->reset_mask = RCC_APB2RSTR_SPI1RST;
    port->cold_owned = true;
    port->cold_acquired = 0U;
    nx_result_t result = nx_stm32u575_spi_initialize(port);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        const nx_stm32u575_spi_endpoint_config_t* endpoint =
            &config->endpoints[i];
        endpoint->cs->registers = endpoint->registers;
        endpoint->cs->mask = endpoint->mask;
        endpoint->cs->output = true;
        /* Record this child before its first possibly partial pad effect. */
        port->cold_acquired = (uint16_t)(i + 1U);
        result = nx_stm32u575_gpio_initialize(endpoint->cs, endpoint->mask);
        if (result != NX_SUCCESS)
            return result;
        endpoint->endpoint->port = port;
        endpoint->endpoint->cs = endpoint->cs;
        endpoint->endpoint->cs_mask = endpoint->mask;
        endpoint->endpoint->frequency_hz = endpoint->frequency_hz;
        endpoint->endpoint->mode = endpoint->mode;
    }
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32u575_alternate_config_t* pin = &config->pins[i];
        result = nx_stm32u575_pin_alternate(pin->registers, pin->pin,
                                            pin->alternate);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32u575_spi_release(nx_stm32u575_spi_state_t* port,
                                     const nx_stm32u575_spi_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->endpoints == NULL)
        return NX_ERROR_INVALID;
    if (!port->cold_owned)
        return NX_SUCCESS;
    if (port->cold_acquired > config->endpoint_count)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32u575_spi_ops.stop(port);
    if (result != NX_SUCCESS)
        return result;
    /* Wire stop and child stop have independent ownership. A CS refusal must
     * remain retryable after the ordinary wire has already become cold. */
    while (port->cold_acquired != 0U) {
        const nx_stm32u575_spi_endpoint_config_t* endpoint =
            &config->endpoints[port->cold_acquired - 1U];
        result = nx_stm32u575_gpio_stop(endpoint->cs, endpoint->mask);
        if (result != NX_SUCCESS)
            return result;
        --port->cold_acquired;
    }
    port->cold_owned = false;
    return NX_SUCCESS;
}
