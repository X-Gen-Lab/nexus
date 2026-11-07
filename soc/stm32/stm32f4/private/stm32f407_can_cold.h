/**
 * \file            stm32f407_can_cold.h
 * \brief           Static classic CAN assembly and retained pin withdrawal
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_CAN_COLD_H
#define NX_STM32F407_CAN_COLD_H
#include "stm32f407_can.h"
#include "stm32f407_platform_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief Immutable controller policy and the shared Platform snapshot slots.
 */
typedef struct {
    nx_can_filter_domain_t* domain;
    nx_can_event_t* events;
    const nx_stm32_platform_pin_config_t* pins;
    size_t capacity;
    nx_can_timing_t timing;
    nx_irq_policy_t irq_policy;
    uint32_t drain_timeout_us;
    uint32_t poll_limit;
    nx_can_mode_t mode;
    uint8_t index;
    uint8_t priority;
    bool fifo_order;
} nx_stm32_can_config_t;

/** \brief Prepare an exact controller; partial acquisition retains pin leases.
 * \note Privileged unmasked Thread only. Config, storage, domain and snapshots
 *       remain static through successful close. Platform owns GPIO clocks.
 */
static inline nx_result_t
nx_stm32_can_prepare(nx_can_controller_t* port,
                     const nx_stm32_can_config_t* config, uint8_t* pin_leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || pin_leases == NULL ||
        config->domain == NULL || config->events == NULL ||
        config->pins == NULL || config->capacity == 0U || config->index > 1U ||
        config->priority > 15U)
        return NX_ERROR_INVALID;
    if (port->snapshot.started || port->publishers != 0U || *pin_leases != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < 3U; ++i) {
        if (port->slots[i].request != NULL)
            return NX_ERROR_BUSY;
    }
    port->domain = config->domain;
    port->events = config->events;
    port->capacity = config->capacity;
    port->timing = config->timing;
    port->irq_policy = config->irq_policy;
    port->drain_timeout_us = config->drain_timeout_us;
    port->poll_limit = config->poll_limit;
    port->mode = config->mode;
    port->index = config->index;
    port->fifo_order = config->fifo_order;
    for (size_t i = 0U; i < 4U; ++i)
        port->sources[i].priority = config->priority;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        nx_result_t status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        *pin_leases |= (uint8_t)(1U << i);
        status =
            nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 9U, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    if (!config->domain->configured) {
        nx_result_t status = nx_stm32_can_domain_initialize(config->domain);
        if (status != NX_SUCCESS)
            return status;
    }
    return nx_stm32_can_initialize(port);
}

/** \brief Stop all actual publishers before restoring either owned pad.
 * \note Failed withdrawal or restore leaves its lease bit for the same retry.
 *       No clock or loan is released on a failed controller stop.
 */
static inline nx_result_t
nx_stm32_can_close(nx_can_controller_t* port,
                   const nx_stm32_can_config_t* config, uint8_t* pin_leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || pin_leases == NULL ||
        config->pins == NULL)
        return NX_ERROR_INVALID;
    if (*pin_leases == 0U) {
        if (port->snapshot.started || port->publishers != 0U)
            return NX_ERROR_BUSY;
        for (size_t i = 0U; i < 3U; ++i) {
            if (port->slots[i].request != NULL)
                return NX_ERROR_BUSY;
        }
        return NX_SUCCESS;
    }
    nx_result_t result = nx_classic_can_ops.stop(port);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 2U; i != 0U; --i) {
        uint8_t bit = (uint8_t)(1U << (i - 1U));
        if ((*pin_leases & bit) == 0U)
            continue;
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
        uint32_t mask = 3U << (pin->pin * 2U);
        uint32_t mode = pin->registers->MODER & mask;
        if (mode != 0U && mode != mask) {
            NX_PLATFORM_REGISTER_WRITE(pin->registers->MODER,
                                       pin->registers->MODER | mask);
            nx_arch_dsb();
            if ((pin->registers->MODER & mask) != mask)
                return NX_ERROR_IO;
        }
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        *pin_leases &= (uint8_t)~bit;
    }
    return NX_SUCCESS;
}

/** \brief Release the unique shared filter/clock domain after both stops. */
static inline nx_result_t
nx_stm32_can_domain_close(nx_can_filter_domain_t* domain) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (domain == NULL)
        return NX_ERROR_INVALID;
    return domain->primary != 0U ? nx_can_filter_domain_stop(domain)
                                 : NX_SUCCESS;
}
#endif
