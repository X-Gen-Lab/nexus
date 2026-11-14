/**
 * \file            spi.c
 * \brief           STM32F4 short-poll SPI transactions and CS drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi_transaction.h"
#include "stm32f407_clock.h"
#include "stm32f407_provider.h"
#include "stm32f407_spi_cold.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

#ifndef NX_STM32_SPI_CS_READ
#define NX_STM32_SPI_CS_READ(gpio) LL_GPIO_ReadOutputPort(gpio)
#endif
#ifndef NX_STM32_SPI_TRANSACTION_CS
#define NX_STM32_SPI_TRANSACTION_CS(active) ((void)(active))
#endif

nx_result_t nx_stm32_spi_select(const nx_stm32_spi_endpoint_state_t* endpoint,
                                bool selected) {
    if (endpoint == NULL || endpoint->port == NULL || endpoint->cs == NULL ||
        endpoint->cs->registers == NULL)
        return NX_ERROR_INVALID;
    nx_stm32_spi_state_t* port = endpoint->port;
    if (selected) {
        if (port->held_endpoint != NULL && port->held_endpoint != endpoint)
            return NX_ERROR_BUSY;
        /* Preserve static recovery identity before the first electrical effect.
         */
        port->held_endpoint = endpoint;
    }
    uint32_t inactive = selected ? 0U : endpoint->cs_mask;
    nx_result_t result = nx_stm32_gpio_write(endpoint->cs, inactive,
                                             selected ? endpoint->cs_mask : 0U);
    if (result == NX_SUCCESS) {
        nx_arch_dsb();
        if ((NX_STM32_SPI_CS_READ(endpoint->cs->registers) &
             endpoint->cs_mask) != inactive)
            result = NX_ERROR_IO;
    }
    if (result != NX_SUCCESS) {
        port->fault = true;
        if (port->held_endpoint == NULL)
            port->held_endpoint = endpoint;
    } else {
        if (!selected && port->held_endpoint == endpoint)
            port->held_endpoint = NULL;
        NX_STM32_SPI_TRANSACTION_CS(selected);
    }
    return result;
}

/** \brief Direct hardware control requires a privileged unmasked executor. */
static bool control_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static volatile uint32_t* cold_gate(const nx_stm32_spi_config_t* config) {
    return config->registers == SPI1 ? &RCC->APB2ENR : &RCC->APB1ENR;
}

static uint32_t cold_gate_mask(const nx_stm32_spi_config_t* config) {
    return config->registers == SPI1   ? RCC_APB2ENR_SPI1EN
           : config->registers == SPI2 ? RCC_APB1ENR_SPI2EN
                                       : RCC_APB1ENR_SPI3EN;
}

/** \brief Withdraw electrical drive before altering a saved latch or mux. */
static bool cold_withdraw(const nx_stm32_spi_pin_config_t* pin) {
    uint32_t mask = 3U << (pin->pin * 2U);
    uint32_t mode = pin->registers->MODER & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(pin->registers->MODER,
                                   pin->registers->MODER | mask);
        nx_arch_dsb();
        if ((pin->registers->MODER & mask) != mask)
            return false;
    }
    return true;
}

/** \brief Inactive CS is proved before any retained pin is returned. */
static bool cold_selects_inactive(const nx_stm32_spi_config_t* config,
                                  size_t captured) {
    for (size_t i = 3U; i < captured; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        const uint32_t mask = 1U << pin->pin;
        NX_PLATFORM_REGISTER_WRITE(pin->registers->BSRR, mask);
        nx_arch_dsb();
        if ((pin->registers->ODR & mask) == 0U)
            return false;
    }
    return true;
}

bool nx_stm32_spi_config_valid(const nx_stm32_spi_config_t* config) {
    if (config == NULL || config->pins == NULL || config->endpoints == NULL ||
        config->endpoint_count == 0U ||
        config->endpoint_count > SIZE_MAX - 3U ||
        config->pin_count != 3U + config->endpoint_count ||
        (config->registers != SPI1 && config->registers != SPI2 &&
         config->registers != SPI3) ||
        config->clock_hz !=
            (config->registers == SPI1 ? NEXUS_APB2_HZ : NEXUS_APB1_HZ))
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        if (pin->snapshot == NULL || pin->port_index > 8U || pin->pin > 15U ||
            pin->alternate > 15U ||
            (uintptr_t)pin->registers !=
                GPIOA_BASE + (uintptr_t)pin->port_index * 0x400U)
            return false;
        for (size_t prior = 0U; prior < i; ++prior) {
            const nx_stm32_spi_pin_config_t* other = &config->pins[prior];
            if (pin->snapshot == other->snapshot ||
                (pin->registers == other->registers && pin->pin == other->pin))
                return false;
        }
    }
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        const nx_stm32_spi_endpoint_config_t* endpoint = &config->endpoints[i];
        if (endpoint->endpoint == NULL || endpoint->chip_select == NULL ||
            endpoint->mode > 3U || endpoint->frequency_hz == 0U ||
            endpoint->frequency_hz > config->clock_hz / 2U)
            return false;
    }
    return true;
}

/** \brief Validate the complete polling binding before shared pin capture. */
nx_result_t nx_stm32_spi_admit(const nx_stm32_spi_state_t* port,
                               const nx_stm32_spi_config_t* config,
                               const nx_stm32_spi_cold_receipt_t* receipt) {
    if (!control_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || receipt == NULL || !nx_stm32_spi_config_valid(config))
        return NX_ERROR_INVALID;
    if (port->registers != NULL || port->active || port->fault ||
        port->held_endpoint != NULL || receipt->rollback_pending ||
        receipt->registers_captured || receipt->captured != 0U ||
        receipt->gpio_mask != 0U || receipt->gpio_clocks != 0U ||
        receipt->peripheral_clock != 0U || receipt->cr1 != 0U ||
        receipt->cr2 != 0U)
        return NX_ERROR_BUSY;
    for (size_t index = 0U; index < config->endpoint_count; ++index) {
        if (config->endpoints[index].chip_select->initialized)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_spi_cold_rollback(nx_stm32_spi_state_t* port,
                                       const nx_stm32_spi_config_t* config,
                                       nx_stm32_spi_cold_receipt_t* receipt) {
    if (!receipt->rollback_pending)
        return NX_SUCCESS;
    if (!cold_selects_inactive(config, receipt->captured))
        return NX_ERROR_IO;
    if (receipt->registers_captured) {
        NX_PLATFORM_REGISTER_WRITE(config->registers->CR2, receipt->cr2);
        NX_PLATFORM_REGISTER_WRITE(config->registers->CR1, receipt->cr1);
        nx_arch_dsb();
        if (config->registers->CR1 != receipt->cr1 ||
            config->registers->CR2 != receipt->cr2)
            return NX_ERROR_IO;
        receipt->registers_captured = false;
    }
    volatile uint32_t* gate = cold_gate(config);
    uint32_t mask = cold_gate_mask(config);
    NX_PLATFORM_REGISTER_WRITE(*gate,
                               (*gate & ~mask) | receipt->peripheral_clock);
    nx_arch_dsb();
    if ((*gate & mask) != receipt->peripheral_clock)
        return NX_ERROR_IO;
    while (receipt->captured != 0U) {
        size_t index = receipt->captured - 1U;
        const nx_stm32_spi_pin_config_t* pin = &config->pins[index];
        if (!cold_withdraw(pin))
            return NX_ERROR_IO;
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        if (index >= 3U)
            config->endpoints[index - 3U].chip_select->initialized = false;
        --receipt->captured;
    }
    NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,
                               (RCC->AHB1ENR & ~receipt->gpio_mask) |
                                   receipt->gpio_clocks);
    nx_arch_dsb();
    if ((RCC->AHB1ENR & receipt->gpio_mask) != receipt->gpio_clocks)
        return NX_ERROR_IO;
    *receipt = (nx_stm32_spi_cold_receipt_t){0};
    port->registers = NULL;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_spi_cold_acquire(nx_stm32_spi_state_t* port,
                                      const nx_stm32_spi_config_t* config,
                                      nx_stm32_spi_cold_receipt_t* receipt) {
    if (!control_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (port->registers != NULL || port->active ||
        port->held_endpoint != NULL || receipt->rollback_pending)
        return NX_ERROR_BUSY;
    if (!nx_stm32_spi_config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        if (config->endpoints[i].chip_select->initialized)
            return NX_ERROR_BUSY;
    }
    *receipt = (nx_stm32_spi_cold_receipt_t){0};
    for (size_t i = 0U; i < config->pin_count; ++i)
        receipt->gpio_mask |= 1U << config->pins[i].port_index;
    receipt->gpio_clocks = RCC->AHB1ENR & receipt->gpio_mask;
    volatile uint32_t* gate = cold_gate(config);
    receipt->peripheral_clock = *gate & cold_gate_mask(config);
    receipt->rollback_pending = true;
    port->registers = config->registers;
    port->rcc = RCC;
    port->clock_hz = config->clock_hz;
    NX_PLATFORM_REGISTER_WRITE(*gate, *gate | cold_gate_mask(config));
    nx_arch_dsb();
    nx_result_t status = NX_ERROR_IO;
    if ((*gate & cold_gate_mask(config)) == 0U)
        goto failed;
    receipt->cr1 = config->registers->CR1;
    receipt->cr2 = config->registers->CR2;
    status = NX_ERROR_BUSY;
    if ((receipt->cr1 & SPI_CR1_SPE) != 0U ||
        (config->registers->SR & SPI_SR_BSY) != 0U)
        goto failed;
    receipt->registers_captured = true;
    NX_PLATFORM_REGISTER_WRITE(config->registers->CR1, 0U);
    NX_PLATFORM_REGISTER_WRITE(config->registers->CR2, 0U);
    nx_arch_dsb();
    status = NX_ERROR_IO;
    if (config->registers->CR1 != 0U || config->registers->CR2 != 0U)
        goto failed;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            goto failed;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->captured = i + 1U;
        if (i < 3U) {
            status = nx_stm32_pin_configure(pin->registers, pin->pin, 2U,
                                            pin->alternate, 0U, false);
        } else {
            const nx_stm32_spi_endpoint_config_t* endpoint =
                &config->endpoints[i - 3U];
            nx_stm32_gpio_state_t* cs = endpoint->chip_select;
            cs->registers = pin->registers;
            cs->mask = 1U << pin->pin;
            cs->output = true;
            /* Prove inactive CS before connecting its output driver. */
            NX_PLATFORM_REGISTER_WRITE(pin->registers->BSRR, cs->mask);
            nx_arch_dsb();
            if ((pin->registers->ODR & cs->mask) == 0U) {
                status = NX_ERROR_IO;
                goto failed;
            }
            status = nx_stm32_gpio_initialize(cs, cs->mask);
            if (status == NX_SUCCESS) {
                endpoint->endpoint->port = port;
                endpoint->endpoint->cs = cs;
                endpoint->endpoint->cs_mask = cs->mask;
                endpoint->endpoint->mode = endpoint->mode;
                endpoint->endpoint->frequency_hz = endpoint->frequency_hz;
                endpoint->endpoint->lsb_first = endpoint->lsb_first;
            }
        }
        if (status != NX_SUCCESS)
            goto failed;
    }
    receipt->rollback_pending = false;
    return NX_SUCCESS;
failed:
    (void)nx_stm32_spi_cold_rollback(port, config, receipt);
    return status;
}

nx_result_t nx_stm32_spi_prepare(nx_stm32_spi_state_t* port,
                                 const nx_stm32_spi_config_t* config,
                                 nx_stm32_spi_cold_receipt_t* receipt) {
    return nx_stm32_spi_cold_acquire(port, config, receipt);
}

nx_result_t nx_stm32_spi_close(nx_stm32_spi_state_t* port,
                               const nx_stm32_spi_config_t* config,
                               nx_stm32_spi_cold_receipt_t* receipt) {
    if (!control_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || receipt == NULL)
        return NX_ERROR_INVALID;
    if (port->registers == NULL)
        return NX_SUCCESS;
    if (!nx_stm32_spi_config_valid(config) ||
        port->registers != config->registers)
        return NX_ERROR_INVALID;
    if (port->active)
        return NX_ERROR_BUSY;
    if (receipt->rollback_pending)
        return nx_stm32_spi_cold_rollback(port, config, receipt);
    NX_PLATFORM_REGISTER_WRITE(port->registers->CR1, 0U);
    NX_PLATFORM_REGISTER_WRITE(port->registers->CR2, 0U);
    nx_arch_dsb();
    if (port->registers->CR1 != 0U || port->registers->CR2 != 0U)
        return NX_ERROR_IO;
    if (!cold_selects_inactive(config, receipt->captured))
        return NX_ERROR_IO;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        nx_stm32_gpio_state_t* cs = config->endpoints[i].chip_select;
        if (!cs->initialized)
            continue;
        nx_result_t result =
            nx_stm32_spi_select(config->endpoints[i].endpoint, false);
        if (result != NX_SUCCESS)
            return result;
        cs->initialized = false;
    }
    for (size_t i = 0U; i < 3U; ++i) {
        const nx_stm32_spi_pin_config_t* pin = &config->pins[i];
        if (!cold_withdraw(pin))
            return NX_ERROR_IO;
        (void)nx_stm32_pin_configure(pin->registers, pin->pin, 0U, 0U, 0U,
                                     false);
    }
    volatile uint32_t* gate = cold_gate(config);
    uint32_t mask = cold_gate_mask(config);
    NX_PLATFORM_REGISTER_WRITE(*gate,
                               (*gate & ~mask) | receipt->peripheral_clock);
    nx_arch_dsb();
    if ((*gate & mask) != receipt->peripheral_clock)
        return NX_ERROR_IO;
    *receipt = (nx_stm32_spi_cold_receipt_t){0};
    port->registers = NULL;
    port->held_endpoint = NULL;
    port->fault = false;
    return NX_SUCCESS;
}
#ifndef NX_STM32_SPI_RESET
#define NX_STM32_SPI_RESET(reset, mask, active)                                \
    do {                                                                       \
        if (active) {                                                          \
            *(reset) |= (uint32_t)(mask);                                      \
        } else {                                                               \
            *(reset) &= ~(uint32_t)(mask);                                     \
        }                                                                      \
    } while (0)
#endif

/** \brief Pulse only the retained controller's actual APB reset domain. */
static void reset_controller(nx_stm32_spi_state_t* port) {
    bool first = port->registers != SPI2 && port->registers != SPI3;
    volatile uint32_t* reset =
        first ? &port->rcc->APB2RSTR : &port->rcc->APB1RSTR;
    uint32_t mask = first                     ? RCC_APB2RSTR_SPI1RST
                    : port->registers == SPI2 ? RCC_APB1RSTR_SPI2RST
                                              : RCC_APB1RSTR_SPI3RST;
    NX_STM32_SPI_RESET(reset, mask, true);
    nx_arch_dsb();
    NX_STM32_SPI_RESET(reset, mask, false);
    nx_arch_dsb();
}

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
    LL_SPI_Disable(port->registers);
    reset_controller(port);
    port->fault = true;
}

/** \brief Reject wrapped extents and overlap without touching either span. */
static bool storage_outside(const void* storage, size_t bytes,
                            const void* control, size_t control_bytes) {
    uintptr_t first = (uintptr_t)storage;
    uintptr_t second = (uintptr_t)control;
    return storage != NULL && bytes <= UINTPTR_MAX - first &&
           control_bytes <= UINTPTR_MAX - second &&
           (first <= second ? second - first >= bytes
                            : first - second >= control_bytes);
}

/** \brief Admission never lends this controller's control or live MMIO bytes.
 */
static bool polling_storage(const nx_stm32_spi_endpoint_state_t* endpoint,
                            const void* storage, size_t bytes) {
    const nx_stm32_spi_state_t* port = endpoint->port;
    return storage_outside(storage, bytes, endpoint, sizeof(*endpoint)) &&
           storage_outside(storage, bytes, port, sizeof(*port)) &&
           storage_outside(storage, bytes, endpoint->cs,
                           sizeof(*endpoint->cs)) &&
           storage_outside(storage, bytes, port->registers,
                           sizeof(*port->registers)) &&
           storage_outside(storage, bytes, port->rcc, sizeof(*port->rcc)) &&
           storage_outside(storage, bytes, endpoint->cs->registers,
                           sizeof(*endpoint->cs->registers));
}

/** \brief Validate a result and optional byte spans before their first write.
 */
static bool polling_buffers(const nx_stm32_spi_endpoint_state_t* endpoint,
                            const uint8_t* tx, uint8_t* rx, size_t bytes,
                            size_t* transferred) {
    if ((uintptr_t)transferred % _Alignof(size_t) != 0U ||
        !polling_storage(endpoint, transferred, sizeof(*transferred)) ||
        (tx != NULL &&
         (!polling_storage(endpoint, tx, bytes) ||
          !storage_outside(transferred, sizeof(*transferred), tx, bytes))) ||
        (rx != NULL &&
         (!polling_storage(endpoint, rx, bytes) ||
          !storage_outside(transferred, sizeof(*transferred), rx, bytes))))
        return false;
    return tx == NULL || rx == NULL || tx == rx ||
           storage_outside(rx, bytes, tx, bytes);
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
    nx_stm32_spi_state_t* port = endpoint->port;
    if (port->registers == NULL || port->rcc == NULL || port->clock_hz == 0U ||
        endpoint->cs->registers == NULL || !endpoint->cs->output ||
        (endpoint->cs_mask & ~endpoint->cs->mask) != 0U ||
        endpoint->cs_mask == 0U || !endpoint->cs->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!polling_buffers(endpoint, tx, rx, length, transferred))
        return NX_ERROR_INVALID;
    if (!control_context()) {
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
    if (LL_SPI_IsActiveFlag_BSY(port->registers) != 0U) {
        return NX_ERROR_BUSY;
    }
    *transferred = 0U;
    port->active = true;
    port->registers->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                           (baud_bits << 3U) |
                           ((endpoint->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                           ((endpoint->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U) |
                           (endpoint->lsb_first ? SPI_CR1_LSBFIRST : 0U);
    port->registers->CR2 = 0U;
    LL_SPI_Enable(port->registers);
    nx_result_t result = nx_stm32_spi_select(endpoint, true);
    if (result == NX_SUCCESS) {
        for (size_t index = 0U; index < length; ++index) {
            result = wait_status(port, SPI_SR_TXE, SPI_SR_TXE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            LL_SPI_TransmitData8(port->registers,
                                 tx != NULL ? tx[index] : 0xFFU);
            result = wait_status(port, SPI_SR_RXNE, SPI_SR_RXNE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint8_t byte = LL_SPI_ReceiveData8(port->registers);
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
        LL_SPI_Disable(port->registers);
    }
    nx_result_t cs_result = nx_stm32_spi_select(endpoint, false);
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

/** \brief Prove a polling child's controller without asynchronous methods. */
static bool on_port(const void* context, const nx_spi_port_t* binding) {
    const nx_stm32_spi_endpoint_state_t* endpoint = context;
    return endpoint != NULL && binding != NULL &&
           binding->ops == &nx_stm32_spi_ops &&
           endpoint->port == binding->context;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_spi_endpoint_ops_t nx_stm32_spi_endpoint_ops = {
    .transfer = nx_stm32_spi_endpoint_transfer,
    .on_port = on_port,
};

/** \brief Recover only an unborrowed controller after an observed wire idle. */
nx_result_t nx_stm32_spi_recover(void* context) {
    nx_stm32_spi_state_t* port = context;
    if (port == NULL || port->registers == NULL || port->rcc == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!control_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || LL_SPI_IsActiveFlag_BSY(port->registers) != 0U) {
        return NX_ERROR_BUSY;
    }
    port->registers->CR2 = 0U;
    LL_SPI_Disable(port->registers);
    reset_controller(port);
    if ((port->registers->SR & (SPI_SR_BSY | SPI_SR_MODF | SPI_SR_OVR)) != 0U) {
        return NX_ERROR_IO;
    }
    if (port->held_endpoint != NULL) {
        nx_result_t result = nx_stm32_spi_select(port->held_endpoint, false);
        if (result != NX_SUCCESS)
            return result;
    }
    port->fault = false;
    return NX_SUCCESS;
}

/** \brief Shared controller recovery methods independent of endpoint selection.
 */
const nx_spi_ops_t nx_stm32_spi_ops = {
    .recover = nx_stm32_spi_recover,
};

#ifndef NX_STM32_SPI_TRANSACTION_WRITE
#define NX_STM32_SPI_TRANSACTION_WRITE(port, value, bits)                      \
    do {                                                                       \
        if ((bits) == 16U) {                                                   \
            LL_SPI_TransmitData16((port)->registers, (value));                 \
        } else {                                                               \
            LL_SPI_TransmitData8((port)->registers, (uint8_t)(value));         \
        }                                                                      \
    } while (0)
#endif

/** \brief Optional word-mode polling keeps all phases inside one CS interval.
 */
static nx_result_t transaction_transfer(void* context,
                                        const nx_spi_segment_t* segments,
                                        size_t count, uint8_t word_bits,
                                        nx_time_us_t deadline,
                                        size_t* transferred) {
    const nx_stm32_spi_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL || endpoint->cs == NULL ||
        endpoint->mode > 3U || endpoint->frequency_hz == 0U) {
        return NX_ERROR_INVALID;
    }
    nx_stm32_spi_state_t* port = endpoint->port;
    if (port->registers == NULL || port->rcc == NULL || port->clock_hz == 0U ||
        endpoint->cs->registers == NULL || !endpoint->cs->initialized ||
        !endpoint->cs->output || endpoint->cs_mask == 0U ||
        (endpoint->cs_mask & ~endpoint->cs->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (segments == NULL || count == 0U || count > NX_SPI_MAX_SEGMENTS ||
        (word_bits != 8U && word_bits != 16U) ||
        (uintptr_t)transferred % _Alignof(size_t) != 0U ||
        !polling_storage(endpoint, segments, count * sizeof(*segments)) ||
        !polling_storage(endpoint, transferred, sizeof(*transferred)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < count; ++i) {
        size_t bytes = segments[i].words * (word_bits / 8U);
        if ((segments[i].tx != NULL &&
             !polling_storage(endpoint, segments[i].tx, bytes)) ||
            (segments[i].rx != NULL &&
             !polling_storage(endpoint, segments[i].rx, bytes)))
            return NX_ERROR_INVALID;
    }
    if (!control_context()) {
        return NX_ERROR_CONTEXT;
    }
    /* Bound the entire CPU transaction before acquiring CS or touching SPI. */
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        total += segments[i].words;
    }
    if (total > 256U || deadline == NX_DEADLINE_NEVER) {
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
    /* Round down the wire rate so this endpoint never exceeds its limit. */
    uint32_t divisor = 2U;
    uint32_t baud = 0U;
    while (port->clock_hz / divisor > endpoint->frequency_hz &&
           divisor < 256U) {
        divisor *= 2U;
        ++baud;
    }
    if (port->clock_hz / divisor > endpoint->frequency_hz) {
        return NX_ERROR_UNSUPPORTED;
    }
    if ((port->registers->SR & SPI_SR_BSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    *transferred = 0U;
    port->active = true;
    /* DFF is programmed with SPE clear, once for the complete transaction. */
    LL_SPI_Disable(port->registers);
    port->registers->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                           (baud << 3U) |
                           (word_bits == 16U ? SPI_CR1_DFF : 0U) |
                           ((endpoint->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                           ((endpoint->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U) |
                           (endpoint->lsb_first ? SPI_CR1_LSBFIRST : 0U);
    port->registers->CR2 = 0U;
    LL_SPI_Enable(port->registers);
    nx_result_t result = nx_stm32_spi_select(endpoint, true);
    for (size_t phase = 0U; phase < count && result == NX_SUCCESS; ++phase) {
        const nx_spi_segment_t* segment = &segments[phase];
        for (size_t word = 0U; word < segment->words; ++word) {
            result = wait_status(port, SPI_SR_TXE, SPI_SR_TXE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint16_t value = segment->dummy;
            if (segment->tx != NULL) {
                value = word_bits == 8U ? ((const uint8_t*)segment->tx)[word]
                                        : ((const uint16_t*)segment->tx)[word];
            }
            NX_STM32_SPI_TRANSACTION_WRITE(port, value, word_bits);
            result = wait_status(port, SPI_SR_RXNE, SPI_SR_RXNE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint16_t received = word_bits == 8U
                                    ? LL_SPI_ReceiveData8(port->registers)
                                    : LL_SPI_ReceiveData16(port->registers);
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
    /* RXNE proves each received word; BSY separately proves wire completion. */
    if (result == NX_SUCCESS) {
        result = wait_status(port, SPI_SR_BSY, 0U, deadline);
    }
    if (result == NX_SUCCESS) {
        LL_SPI_Disable(port->registers);
    } else {
        abort_controller(port);
    }
    /* Deassert CS on every path while preserving the primary transfer error. */
    nx_result_t release = nx_stm32_spi_select(endpoint, false);
    port->active = false;
    if (result != NX_SUCCESS) {
        return result;
    }
    if (release != NX_SUCCESS) {
        return release;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

/** \brief Selected enhanced face adds no fields to legacy tables or state. */
const nx_spi_transaction_ops_t nx_stm32_spi_transaction_ops = {
    .transfer = transaction_transfer,
};
