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

const nx_gd32_spi_controller_t nx_gd32_spi0_controller = {SPI0, 100000000U,
                                                          RCU_SPI0};
const nx_gd32_spi_controller_t nx_gd32_spi4_controller = {SPI4, 100000000U,
                                                          RCU_SPI4};

/** \brief           Poll a hardware fact without silently ignoring faults. */
static nx_result_t wait_status(nx_gd32_spi_state_t* port, uint32_t mask,
                               bool set, nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        uint32_t flags = SPI_STAT(port->controller->registers);
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

/** \brief Bind a cold child without reacquiring its already-owned controller.
 */
nx_result_t nx_gd32_spi_endpoint_initialize(
    nx_gd32_spi_state_t* port, nx_gd32_spi_endpoint_state_t* endpoint,
    uint32_t cs_gpio, uint32_t cs_mask, uint32_t clock_hz, unsigned mode) {
    if (port == NULL || endpoint == NULL || !port->initialized ||
        port->controller == NULL || clock_hz == 0U || mode > 3U ||
        cs_gpio < GPIOA || cs_gpio > GPIOI ||
        (cs_gpio - GPIOA) % (GPIOB - GPIOA) != 0U || cs_mask == 0U ||
        (cs_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    unsigned divisor = 0U;
    while (divisor < 7U &&
           (port->controller->clock_hz >> (divisor + 1U)) > clock_hz) {
        ++divisor;
    }
    if ((port->controller->clock_hz >> (divisor + 1U)) > clock_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t control = SPI_CTL0_MSTMOD | SPI_CTL0_SWNSSEN | SPI_CTL0_SWNSS |
                       ((uint32_t)divisor << 3U) |
                       ((mode & 1U) != 0U ? SPI_CTL0_CKPH : 0U) |
                       ((mode & 2U) != 0U ? SPI_CTL0_CKPL : 0U);
    *endpoint = (nx_gd32_spi_endpoint_state_t){.port = port,
                                               .control = control,
                                               .cs_gpio = cs_gpio,
                                               .cs_mask = cs_mask};
    return NX_SUCCESS;
}

/** \brief Acquire exactly one controller after reviewed Board pin preparation.
 */
nx_result_t nx_gd32_spi_initialize_at(
    nx_gd32_spi_state_t* port, nx_gd32_spi_endpoint_state_t* endpoint,
    const nx_gd32_spi_controller_t* controller, uint32_t cs_gpio,
    uint32_t cs_mask, uint32_t clock_hz, unsigned mode) {
    if (port == NULL || endpoint == NULL || clock_hz == 0U || mode > 3U ||
        (controller != &nx_gd32_spi0_controller &&
         controller != &nx_gd32_spi4_controller) ||
        cs_gpio < GPIOA || cs_gpio > GPIOI ||
        (cs_gpio - GPIOA) % (GPIOB - GPIOA) != 0U || cs_mask == 0U ||
        (cs_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    unsigned divisor = 0U;
    while (divisor < 7U &&
           (controller->clock_hz >> (divisor + 1U)) > clock_hz) {
        ++divisor;
    }
    if ((controller->clock_hz >> (divisor + 1U)) > clock_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t control = SPI_CTL0_MSTMOD | SPI_CTL0_SWNSSEN | SPI_CTL0_SWNSS |
                       ((uint32_t)divisor << 3U) |
                       ((mode & 1U) != 0U ? SPI_CTL0_CKPH : 0U) |
                       ((mode & 2U) != 0U ? SPI_CTL0_CKPL : 0U);
    rcu_periph_clock_enable((rcu_periph_enum)controller->clock);
    spi_i2s_deinit(controller->registers);
    SPI_CTL0(controller->registers) = control;
    SPI_CTL1(controller->registers) = 0U;
    *port =
        (nx_gd32_spi_state_t){.controller = controller, .initialized = true};
    *endpoint = (nx_gd32_spi_endpoint_state_t){.port = port,
                                               .control = control,
                                               .cs_gpio = cs_gpio,
                                               .cs_mask = cs_mask};
    return NX_SUCCESS;
}

/** \brief Explicit SPI4 fixture; generated boards prepare AF/CS separately. */
nx_result_t nx_gd32_spi_initialize(nx_gd32_spi_state_t* port,
                                   nx_gd32_spi_endpoint_state_t* endpoint,
                                   uint32_t clock_hz, unsigned mode) {
    nx_result_t result =
        nx_gd32_spi_initialize_at(port, endpoint, &nx_gd32_spi4_controller,
                                  GPIOF, GPIO_PIN_6, clock_hz, mode);
    if (result != NX_SUCCESS) {
        return result;
    }
    rcu_periph_clock_enable(RCU_GPIOF);
    GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask;
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_af_set(GPIOF, GPIO_AF_5, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    return NX_SUCCESS;
}

/** \brief           Execute one wire-active interval and drain/reset on error.
 */
nx_result_t nx_gd32_spi_endpoint_transfer(void* context, const uint8_t* tx,
                                          uint8_t* rx, size_t length,
                                          nx_time_us_t deadline,
                                          size_t* transferred) {
    const nx_gd32_spi_endpoint_state_t* endpoint = context;
    if (!endpoint || !endpoint->port || !endpoint->port->initialized ||
        endpoint->port->controller == NULL || !length || length > 256u ||
        deadline == NX_DEADLINE_NEVER || (!tx && !rx) || !transferred) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0u;
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_spi_state_t* port = endpoint->port;
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    SPI_CTL0(port->controller->registers) = endpoint->control | SPI_CTL0_SPIEN;
    GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask << 16U;
    nx_result_t status = NX_SUCCESS;
    for (size_t i = 0u; i < length; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        status = wait_status(port, SPI_STAT_TBE, true, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        SPI_DATA(port->controller->registers) = tx ? tx[i] : UINT8_MAX;
        status = wait_status(port, SPI_STAT_RBNE, true, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        uint8_t byte = (uint8_t)SPI_DATA(port->controller->registers);
        if (rx) {
            rx[i] = byte;
        }
        ++*transferred;
    }
    if (status == NX_SUCCESS) {
        status = wait_status(port, SPI_STAT_TRANS, false, deadline);
    }
    if (status != NX_SUCCESS) {
        /* Reset stops the shifter, does not complete a truncated transaction.
         */
        spi_i2s_deinit(port->controller->registers);
    }
    SPI_CTL0(port->controller->registers) &= ~SPI_CTL0_SPIEN;
    nx_gd32_peripheral_barrier();
    GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask;
    port->active = false;
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Release SPI only after its serialized executor has exited.
 */
nx_result_t nx_gd32_spi_stop(nx_gd32_spi_state_t* port) {
    if (!port || port->controller == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    spi_i2s_deinit(port->controller->registers);
    nx_gd32_peripheral_barrier();
    port->initialized = false;
    rcu_periph_clock_disable((rcu_periph_enum)port->controller->clock);
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_spi_endpoint_ops_t nx_gd32_spi_endpoint_ops = {
    .transfer = nx_gd32_spi_endpoint_transfer,
};

/** \brief Reset only an idle initialized controller without replaying traffic.
 */
nx_result_t nx_gd32_spi_recover(void* context) {
    nx_gd32_spi_state_t* port = context;
    if (port == NULL || !port->initialized || port->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active ||
        (SPI_STAT(port->controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    spi_i2s_deinit(port->controller->registers);
    nx_gd32_peripheral_barrier();
    return (SPI_STAT(port->controller->registers) & SPI_STAT_TRANS) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Shared controller recovery methods independent of child devices. */
const nx_spi_ops_t nx_gd32_spi_ops = {
    .recover = nx_gd32_spi_recover,
};
