/**
 * \file            spi.c
 * \brief           GD32 SPI4 single-owner polling and complete CS intervals
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_spi_port_t* s_spi;

/** \brief           Poll a hardware fact without silently ignoring faults. */
static nx_result_t wait_status(uint32_t mask, bool set, nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        uint32_t flags = SPI_STAT(SPI4);
        if ((flags & (SPI_STAT_CONFERR | SPI_STAT_RXORERR | SPI_STAT_FERR)) !=
            0u) {
            return NX_ERROR_IO;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if (((flags & mask) != 0u) == set) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief           Preload CS before AF configuration and controller enable.
 */
nx_result_t nx_gd32_spi_initialize(nx_spi_port_t* port,
                                   nx_spi_endpoint_t* endpoint,
                                   uint32_t clock_hz, unsigned mode) {
    if (!port || !endpoint || !clock_hz || mode > 3u) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_spi) {
        return NX_ERROR_BUSY;
    }
    unsigned divisor = 0u;
    while (divisor < 7u && (100000000u >> (divisor + 1u)) > clock_hz) {
        ++divisor;
    }
    if ((100000000u >> (divisor + 1u)) > clock_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_SPI4);
    GPIO_BOP(GPIOF) = GPIO_PIN_6;
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_af_set(GPIOF, GPIO_AF_5, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    uint32_t control = SPI_CTL0_MSTMOD | SPI_CTL0_SWNSSEN | SPI_CTL0_SWNSS |
                       ((uint32_t)divisor << 3) |
                       (mode & 1u ? SPI_CTL0_CKPH : 0u) |
                       (mode & 2u ? SPI_CTL0_CKPL : 0u);
    spi_i2s_deinit(SPI4);
    SPI_CTL0(SPI4) = control;
    SPI_CTL1(SPI4) = 0u;
    *port = (nx_spi_port_t){.initialized = true};
    *endpoint = (nx_spi_endpoint_t){port, control};
    s_spi = port;
    return NX_SUCCESS;
}

/** \brief           Execute one wire-active interval and drain/reset on error.
 */
nx_result_t nx_spi_endpoint_transfer(const nx_spi_endpoint_t* endpoint,
                                     const uint8_t* tx, uint8_t* rx,
                                     size_t length, nx_time_us_t deadline,
                                     size_t* transferred) {
    if (!endpoint || !endpoint->port || !endpoint->port->initialized ||
        endpoint->port != s_spi || !length || length > 256u ||
        deadline == NX_DEADLINE_NEVER || (!tx && !rx) || !transferred) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0u;
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_spi_port_t* port = endpoint->port;
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    SPI_CTL0(SPI4) = endpoint->control | SPI_CTL0_SPIEN;
    GPIO_BOP(GPIOF) = GPIO_PIN_6 << 16;
    nx_result_t status = NX_SUCCESS;
    for (size_t i = 0u; i < length; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        status = wait_status(SPI_STAT_TBE, true, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        SPI_DATA(SPI4) = tx ? tx[i] : UINT8_MAX;
        status = wait_status(SPI_STAT_RBNE, true, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        uint8_t byte = (uint8_t)SPI_DATA(SPI4);
        if (rx) {
            rx[i] = byte;
        }
        ++*transferred;
    }
    if (status == NX_SUCCESS) {
        status = wait_status(SPI_STAT_TRANS, false, deadline);
    }
    if (status != NX_SUCCESS) {
        /* Reset stops the shifter, does not complete a truncated transaction.
         */
        spi_i2s_deinit(SPI4);
    }
    SPI_CTL0(SPI4) &= ~SPI_CTL0_SPIEN;
    nx_gd32_peripheral_barrier();
    GPIO_BOP(GPIOF) = GPIO_PIN_6;
    port->active = false;
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Release SPI only after its serialized executor has exited.
 */
nx_result_t nx_gd32_spi_stop(nx_spi_port_t* port) {
    if (!port || port != s_spi || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    spi_i2s_deinit(SPI4);
    GPIO_BOP(GPIOF) = GPIO_PIN_6;
    nx_gd32_peripheral_barrier();
    port->initialized = false;
    s_spi = NULL;
    rcu_periph_clock_disable(RCU_SPI4);
    return NX_SUCCESS;
}
