/**
 * \file            spi.c
 * \brief           Fixed SPI1 short polling transactions with explicit CS drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

/** \brief Poll one hardware state against the shared absolute deadline. */
static nx_result_t wait_status(nx_stm32_spi_state_t* port, uint32_t mask,
                               uint32_t expected, nx_time_us_t deadline) {
    for (;;) {
        NX_STM32_IO_POLL(1U, port);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        uint32_t status = port->registers->SR;
        if ((status & (SPI_SR_MODF | SPI_SR_OVR)) != 0U) {
            return NX_ERROR_IO;
        }
        if ((status & mask) == expected) {
            return NX_SUCCESS;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
    }
}

/** \brief Abort without a hidden DMA borrower; hardware reset stops the wire.
 */
static void abort_controller(nx_stm32_spi_state_t* port) {
    port->registers->CR2 = 0U;
    port->registers->CR1 &= ~(uint32_t)SPI_CR1_SPE;
    port->rcc->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    nx_arch_dsb();
    port->rcc->APB2RSTR &= ~(uint32_t)RCC_APB2RSTR_SPI1RST;
    nx_arch_dsb();
    port->fault = true;
}

/** \brief Keep one endpoint's config and CS stable for the complete
 * transaction. */
nx_result_t nx_stm32_spi_endpoint_transfer(void* context, const uint8_t* tx,
                                           uint8_t* rx, size_t length,
                                           nx_time_us_t deadline,
                                           size_t* transferred) {
    const nx_stm32_spi_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL || endpoint->cs == NULL ||
        transferred == NULL || length == 0U || (tx == NULL && rx == NULL) ||
        endpoint->mode > 3U || endpoint->frequency_hz == 0U) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0U;
    nx_stm32_spi_state_t* port = endpoint->port;
    if (port->registers == NULL || port->rcc == NULL || port->clock_hz == 0U ||
        (endpoint->cs_mask & ~endpoint->cs->mask) != 0U ||
        endpoint->cs_mask == 0U || !endpoint->cs->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (length > 256U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (port->fault) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    uint32_t divider = 2U;
    uint32_t baud_bits = 0U;
    while (port->clock_hz / divider > endpoint->frequency_hz &&
           divider < 256U) {
        divider *= 2U;
        ++baud_bits;
    }
    if (port->clock_hz / divider > endpoint->frequency_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    if ((port->registers->SR & SPI_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    port->active = true;
    port->registers->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                           (baud_bits << 3U) |
                           ((endpoint->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                           ((endpoint->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U);
    port->registers->CR2 = 0U;
    port->registers->CR1 |= SPI_CR1_SPE;
    nx_result_t result =
        nx_stm32_gpio_write(endpoint->cs, 0U, endpoint->cs_mask);
    if (result == NX_SUCCESS) {
        for (size_t index = 0U; index < length; ++index) {
            result = wait_status(port, SPI_SR_TXE, SPI_SR_TXE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            port->registers->DR = tx != NULL ? tx[index] : 0xFFU;
            result = wait_status(port, SPI_SR_RXNE, SPI_SR_RXNE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint8_t byte = (uint8_t)port->registers->DR;
            if (rx != NULL) {
                rx[index] = byte;
            }
            ++*transferred;
        }
    }
    if (result == NX_SUCCESS) {
        result = wait_status(port, SPI_SR_BSY, 0U, deadline);
    }
    if (result != NX_SUCCESS) {
        abort_controller(port);
    } else {
        port->registers->CR1 &= ~(uint32_t)SPI_CR1_SPE;
    }
    nx_result_t cs_result =
        nx_stm32_gpio_write(endpoint->cs, endpoint->cs_mask, 0U);
    port->active = false;
    if (result != NX_SUCCESS) {
        return result;
    }
    if (cs_result != NX_SUCCESS) {
        return cs_result;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_spi_endpoint_ops_t nx_stm32_spi_endpoint_ops = {
    .transfer = nx_stm32_spi_endpoint_transfer,
};

/** \brief Recover only an unborrowed controller after an observed wire idle. */
nx_result_t nx_stm32_spi_recover(void* context) {
    nx_stm32_spi_state_t* port = context;
    if (port == NULL || port->registers == NULL || port->rcc == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || (port->registers->SR & SPI_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    port->registers->CR2 = 0U;
    port->registers->CR1 &= ~(uint32_t)SPI_CR1_SPE;
    port->rcc->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    nx_arch_dsb();
    port->rcc->APB2RSTR &= ~(uint32_t)RCC_APB2RSTR_SPI1RST;
    nx_arch_dsb();
    if ((port->registers->SR & (SPI_SR_BSY | SPI_SR_MODF | SPI_SR_OVR)) != 0U) {
        return NX_ERROR_IO;
    }
    port->fault = false;
    return NX_SUCCESS;
}

/** \brief Shared controller recovery methods independent of endpoint selection.
 */
const nx_spi_ops_t nx_stm32_spi_ops = {
    .recover = nx_stm32_spi_recover,
};
