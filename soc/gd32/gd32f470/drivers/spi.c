/**
 * \file            spi.c
 * \brief           GD32 SPI4 single-owner polling and complete CS intervals
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f470_spi_cold.h"
#include "gd32f4xx.h"
#include "nexus/io/spi_transaction.h"
#include "private/system.h"

const nx_gd32_spi_controller_t nx_gd32_spi0_controller = {SPI0, 100000000U,
                                                          RCU_SPI0};
const nx_gd32_spi_controller_t nx_gd32_spi4_controller = {SPI4, 100000000U,
                                                          RCU_SPI4};

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_gd32_in_isr() &&
           !nx_gd32_irq_masked();
}

bool nx_gd32_spi_config_valid(const nx_gd32_spi_config_t* config) {
    if (config == NULL || config->pins == NULL || config->endpoints == NULL ||
        config->endpoint_count == 0U || config->endpoint_count > 13U ||
        config->pin_count != 3U + config->endpoint_count ||
        (config->controller != &nx_gd32_spi0_controller &&
         config->controller != &nx_gd32_spi4_controller))
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_gd32_spi_pin_config_t* pin = &config->pins[i];
        if (pin->snapshot == NULL || pin->port_index > 8U || pin->pin > 15U ||
            pin->alternate > 15U ||
            pin->registers != GPIOA + (uint32_t)pin->port_index * 0x400U)
            return false;
        for (size_t prior = 0U; prior < i; ++prior) {
            const nx_gd32_spi_pin_config_t* other = &config->pins[prior];
            if (pin->snapshot == other->snapshot ||
                (pin->registers == other->registers && pin->pin == other->pin))
                return false;
        }
    }
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        const nx_gd32_spi_endpoint_config_t* endpoint = &config->endpoints[i];
        if (endpoint->endpoint == NULL || endpoint->mode > 3U ||
            endpoint->frequency_hz < (config->controller->clock_hz >> 8U) ||
            endpoint->frequency_hz > config->controller->clock_hz / 2U)
            return false;
        for (size_t prior = 0U; prior < i; ++prior) {
            if (endpoint->endpoint == config->endpoints[prior].endpoint)
                return false;
        }
    }
    return true;
}

nx_result_t nx_gd32_spi_pins_prepare(const nx_gd32_spi_config_t* config,
                                     nx_gd32_spi_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (cold == NULL || !nx_gd32_spi_config_valid(config))
        return NX_ERROR_INVALID;
    if (cold->pin_leases != 0U || cold->constructed || cold->clock_owned)
        return NX_ERROR_BUSY;
    const uint32_t clock = config->controller->clock;
    if ((RCU_REG_VAL(clock) & BIT(RCU_BIT_POS(clock))) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_gd32_spi_pin_config_t* pin = &config->pins[i];
        nx_result_t status = nx_gd32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_gd32_pin_capture(pin->registers, pin->pin);
        cold->pin_leases |= (uint16_t)(1U << i);
        if (i >= 3U) {
            const uint32_t bit = 1U << pin->pin;
            NX_PLATFORM_REGISTER_WRITE(GPIO_BOP(pin->registers), bit);
            nx_arch_dsb();
            if ((GPIO_OCTL(pin->registers) & bit) == 0U)
                return NX_ERROR_IO;
        }
        status =
            nx_gd32_pin_configure(pin->registers, pin->pin, i >= 3U ? 1U : 2U,
                                  pin->alternate, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_spi_pins_close(const nx_gd32_spi_config_t* config,
                                   nx_gd32_spi_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (cold == NULL || !nx_gd32_spi_config_valid(config))
        return NX_ERROR_INVALID;
    /* Prove every acquired CS inactive before restoring any selected pad. */
    for (size_t i = 3U; i < config->pin_count; ++i) {
        if ((cold->pin_leases & (1U << i)) == 0U)
            continue;
        const nx_gd32_spi_pin_config_t* pin = &config->pins[i];
        const uint32_t bit = 1U << pin->pin;
        NX_PLATFORM_REGISTER_WRITE(GPIO_BOP(pin->registers), bit);
        nx_arch_dsb();
        if ((GPIO_OCTL(pin->registers) & bit) == 0U)
            return NX_ERROR_IO;
    }
    for (size_t count = config->pin_count; count != 0U; --count) {
        const uint16_t owned = (uint16_t)(1U << (count - 1U));
        if ((cold->pin_leases & owned) == 0U)
            continue;
        const nx_gd32_spi_pin_config_t* pin = &config->pins[count - 1U];
        const uint32_t mask = 3U << (pin->pin * 2U);
        const uint32_t mode = GPIO_CTL(pin->registers) & mask;
        if (mode != 0U && mode != mask) {
            NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(pin->registers),
                                       GPIO_CTL(pin->registers) | mask);
            nx_arch_dsb();
            if ((GPIO_CTL(pin->registers) & mask) != mask)
                return NX_ERROR_IO;
        }
        nx_gd32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_gd32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        cold->pin_leases &= (uint16_t)~owned;
    }
    if (cold->clock_owned) {
        const uint32_t clock = config->controller->clock;
        const uint32_t mask = BIT(RCU_BIT_POS(clock));
        NX_PLATFORM_REGISTER_WRITE(RCU_REG_VAL(clock),
                                   RCU_REG_VAL(clock) & ~mask);
        nx_arch_dsb();
        if ((RCU_REG_VAL(clock) & mask) != 0U)
            return NX_ERROR_IO;
        cold->clock_owned = false;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_spi_prepare(nx_gd32_spi_state_t* port,
                                const nx_gd32_spi_config_t* config,
                                nx_gd32_spi_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL)
        return NX_ERROR_INVALID;
    if (port->initialized || port->active)
        return NX_ERROR_BUSY;
    nx_result_t status = nx_gd32_spi_pins_prepare(config, cold);
    if (status != NX_SUCCESS)
        return status;
    const nx_gd32_spi_endpoint_config_t* first = &config->endpoints[0];
    const nx_gd32_spi_pin_config_t* cs = &config->pins[3];
    status = nx_gd32_spi_initialize_at(
        port, first->endpoint, config->controller, cs->registers, 1U << cs->pin,
        first->frequency_hz, first->mode);
    if (status != NX_SUCCESS)
        return status;
    cold->constructed = true;
    cold->clock_owned = true;
    for (size_t i = 1U; i < config->endpoint_count; ++i) {
        const nx_gd32_spi_endpoint_config_t* endpoint = &config->endpoints[i];
        cs = &config->pins[3U + i];
        status = nx_gd32_spi_endpoint_initialize(
            port, endpoint->endpoint, cs->registers, 1U << cs->pin,
            endpoint->frequency_hz, endpoint->mode);
        if (status != NX_SUCCESS)
            return status;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_spi_close(nx_gd32_spi_state_t* port,
                              const nx_gd32_spi_config_t* config,
                              nx_gd32_spi_cold_t* cold) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL || !nx_gd32_spi_config_valid(config))
        return NX_ERROR_INVALID;
    if (cold->constructed) {
        nx_result_t status = nx_gd32_spi_stop(port);
        if (status != NX_SUCCESS)
            return status;
        cold->constructed = false;
    }
    return nx_gd32_spi_pins_close(config, cold);
}

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
    const uint32_t clock = controller->clock;
    const uint32_t gate = BIT(RCU_BIT_POS(clock));
    NX_PLATFORM_REGISTER_WRITE(RCU_REG_VAL(clock), RCU_REG_VAL(clock) | gate);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(clock) & gate) == 0U) {
        return NX_ERROR_IO;
    }
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
        spi_i2s_data_transmit(port->controller->registers,
                              tx ? tx[i] : UINT8_MAX);
        status = wait_status(port, SPI_STAT_RBNE, true, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        uint8_t byte =
            (uint8_t)spi_i2s_data_receive(port->controller->registers);
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
    spi_disable(port->controller->registers);
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
    const uint32_t clock = port->controller->clock;
    const uint32_t gate = BIT(RCU_BIT_POS(clock));
    NX_PLATFORM_REGISTER_WRITE(RCU_REG_VAL(clock), RCU_REG_VAL(clock) & ~gate);
    nx_gd32_peripheral_barrier();
    if ((RCU_REG_VAL(clock) & gate) != 0U) {
        return NX_ERROR_IO;
    }
    port->initialized = false;
    return NX_SUCCESS;
}

/** \brief Prove a polling child's controller without asynchronous methods. */
static bool on_port(const void* context, const nx_spi_port_t* binding) {
    const nx_gd32_spi_endpoint_state_t* endpoint = context;
    return endpoint != NULL && binding != NULL &&
           binding->ops == &nx_gd32_spi_ops &&
           endpoint->port == binding->context;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_spi_endpoint_ops_t nx_gd32_spi_endpoint_ops = {
    .transfer = nx_gd32_spi_endpoint_transfer,
    .on_port = on_port,
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

#ifndef NX_GD32_SPI_TRANSACTION_CS
#define NX_GD32_SPI_TRANSACTION_CS(active) ((void)(active))
#endif
#ifndef NX_GD32_SPI_TRANSACTION_WRITE
#define NX_GD32_SPI_TRANSACTION_WRITE(port, value, bits)                       \
    do {                                                                       \
        (void)(bits);                                                          \
        spi_i2s_data_transmit((port)->controller->registers, (value));         \
    } while (0)
#endif

/** \brief Native-width data spans retain one CS across every phase. */
static nx_result_t transaction_transfer(void* context,
                                        const nx_spi_segment_t* segments,
                                        size_t count, uint8_t word_bits,
                                        nx_time_us_t deadline,
                                        size_t* transferred) {
    const nx_gd32_spi_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL ||
        !endpoint->port->initialized || endpoint->port->controller == NULL ||
        endpoint->cs_gpio < GPIOA || endpoint->cs_gpio > GPIOI ||
        (endpoint->cs_gpio - GPIOA) % (GPIOB - GPIOA) != 0U ||
        endpoint->cs_mask == 0U ||
        (endpoint->cs_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        total += segments[i].words;
    }
    if (total > 256U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_gd32_spi_state_t* port = endpoint->port;
    if (port->active ||
        (SPI_STAT(port->controller->registers) & SPI_STAT_TRANS) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    /* FF16 changes only while SPI is disabled, never between phases. */
    spi_disable(port->controller->registers);
    SPI_CTL0(port->controller->registers) =
        (endpoint->control & ~(SPI_CTL0_SPIEN | SPI_CTL0_FF16)) |
        (word_bits == 16U ? SPI_CTL0_FF16 : 0U) | SPI_CTL0_SPIEN;
    GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask << 16U;
    NX_GD32_SPI_TRANSACTION_CS(true);
    nx_result_t result = NX_SUCCESS;
    for (size_t phase = 0U; phase < count && result == NX_SUCCESS; ++phase) {
        const nx_spi_segment_t* segment = &segments[phase];
        for (size_t word = 0U; word < segment->words; ++word) {
            result = wait_status(port, SPI_STAT_TBE, true, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint16_t value = segment->dummy;
            if (segment->tx != NULL) {
                value = word_bits == 8U ? ((const uint8_t*)segment->tx)[word]
                                        : ((const uint16_t*)segment->tx)[word];
            }
            NX_GD32_SPI_TRANSACTION_WRITE(port, value, word_bits);
            result = wait_status(port, SPI_STAT_RBNE, true, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint16_t received =
                spi_i2s_data_receive(port->controller->registers);
            if (segment->rx != NULL) {
                if (word_bits == 8U) {
                    ((uint8_t*)segment->rx)[word] = (uint8_t)received;
                } else {
                    ((uint16_t*)segment->rx)[word] = received;
                }
            }
            ++*transferred;
        }
    }
    if (result == NX_SUCCESS) {
        result = wait_status(port, SPI_STAT_TRANS, false, deadline);
    }
    if (result != NX_SUCCESS) {
        spi_i2s_deinit(port->controller->registers);
    }
    spi_disable(port->controller->registers);
    nx_gd32_peripheral_barrier();
    GPIO_BOP(endpoint->cs_gpio) = endpoint->cs_mask;
    NX_GD32_SPI_TRANSACTION_CS(false);
    port->active = false;
    return result == NX_SUCCESS &&
                   nx_deadline_expired(deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : result;
}

/** \brief Independent enhanced methods leave default faces/state unchanged. */
const nx_spi_transaction_ops_t nx_gd32_spi_transaction_ops = {
    .transfer = transaction_transfer,
};
