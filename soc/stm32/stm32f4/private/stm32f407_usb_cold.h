/**
 * \file            stm32f407_usb_cold.h
 * \brief           Concrete FS endpoint assembly and retained cold lifecycle
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_USB_COLD_H
#define NX_STM32F407_USB_COLD_H
#include "stm32f407_clock.h"
#include "stm32f407_packet_cold.h"
#include "stm32f407_usb_device.h"
#include <string.h>

/** \brief Readonly endpoint properties, never a second mutable endpoint. */
typedef struct {
    uint8_t number;
    bool in;
    uint8_t transfer;
    uint16_t max_packet;
    uint16_t fifo_start;
    uint16_t fifo_words;
} nx_stm32_usb_endpoint_config_t;

/** \brief Static FS storage and exact selected finite cold policy. */
typedef struct {
    nx_stm32_usb_clock_state_t* clock;
    const nx_stm32_platform_pin_config_t* pins;
    nx_usb_endpoint_t* endpoints;
    const nx_stm32_usb_endpoint_config_t* plan;
    nx_usb_event_t* events;
    size_t endpoint_count;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    uint32_t poll_limit;
    uint32_t drain_timeout_us;
    uint32_t clock_timeout_us;
    uint16_t rx_fifo_words;
    uint8_t priority;
    nx_usb_remote_runtime_t* remote;
} nx_stm32_usb_cold_config_t;

/** \brief A selected runtime must not be erased by its cold assembly.
 * \param[in] remote: Optional static remote runtime.
 * \param[in] object: One live cold metadata or snapshot region.
 * \param[in] bytes: Region extent, with no wrapping arithmetic.
 * \return True when the remote object is naturally aligned and disjoint.
 */
static inline bool
nx_stm32_usb_remote_separate(const nx_usb_remote_runtime_t* remote,
                             const void* object, size_t bytes) {
    if (remote == NULL)
        return true;
    uintptr_t r = (uintptr_t)remote, p = (uintptr_t)object;
#ifdef __cplusplus
    const size_t alignment = alignof(nx_usb_remote_runtime_t);
#else
    const size_t alignment = _Alignof(nx_usb_remote_runtime_t);
#endif
    return r % alignment == 0U && sizeof(*remote) <= UINTPTR_MAX - r &&
           bytes <= UINTPTR_MAX - p &&
           (bytes == 0U || r >= p + bytes || p >= r + sizeof(*remote));
}
/** \brief Gate withdrawal has no wait; exhausted room never means infinite.
 * \param[in] timeout_us: Original finite cold budget.
 * \param[out] deadline: Finite future bound, or current observation at
 * exhaustion. \return SUCCESS or an invalid clock value; output remains finite
 * on success. \note Only the already-drained, bounded USB clock gate stop uses
 * this helper. An expired finite observation permits a final readback; it
 *       cannot authorize waiting, signal shortening or a new acquisition.
 */
static inline nx_result_t
nx_stm32_usb_withdraw_deadline(uint32_t timeout_us, nx_time_us_t* deadline) {
    const nx_time_us_t now = nx_time_now_us();
    nx_result_t result = nx_deadline_finite_after(now, timeout_us, deadline);
    if (result == NX_ERROR_EXHAUSTED && now != NX_DEADLINE_NEVER) {
        *deadline = now;
        return NX_SUCCESS;
    }
    return result;
}
/** \brief Assemble FS storage and acquire pins/clock without hidden workers.
 * \note Privileged unmasked Thread only. Config, endpoint/event/snapshot and
 *       clock storage outlive successful close. Failed prepare retains effects
 *       for close; it never overwrites a live controller or borrowed request.
 */
static inline nx_result_t
nx_stm32_usb_cold_prepare(nx_usb_controller_t* port,
                          const nx_stm32_usb_cold_config_t* config,
                          uint8_t* leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || leases == NULL ||
        config->clock == NULL || config->endpoints == NULL ||
        config->events == NULL || config->plan == NULL ||
        config->endpoint_count == 0U || config->endpoint_count > 8U ||
        config->capacity == 0U || config->poll_limit == 0U ||
        config->clock_timeout_us == 0U || config->drain_timeout_us == 0U ||
        config->priority > 15U || !nx_stm32_packet_pins_valid(config->pins, 2U))
        return NX_ERROR_INVALID;
    if (config->remote != NULL) {
        const nx_usb_remote_runtime_t* remote = config->remote;
        if (config->capacity > SIZE_MAX / sizeof(*config->events) ||
            !nx_stm32_usb_remote_separate(remote, port, sizeof(*port)) ||
            !nx_stm32_usb_remote_separate(remote, config, sizeof(*config)) ||
            !nx_stm32_usb_remote_separate(remote, leases, sizeof(*leases)) ||
            !nx_stm32_usb_remote_separate(remote, config->clock,
                                          sizeof(*config->clock)) ||
            !nx_stm32_usb_remote_separate(remote, &nx_stm32_usb_clock_ops,
                                          sizeof(nx_stm32_usb_clock_ops)) ||
            !nx_stm32_usb_remote_separate(remote, config->endpoints,
                                          config->endpoint_count *
                                              sizeof(*config->endpoints)) ||
            !nx_stm32_usb_remote_separate(remote, config->plan,
                                          config->endpoint_count *
                                              sizeof(*config->plan)) ||
            !nx_stm32_usb_remote_separate(remote, config->events,
                                          config->capacity *
                                              sizeof(*config->events)) ||
            !nx_stm32_usb_remote_separate(remote, config->pins,
                                          2U * sizeof(*config->pins)))
            return NX_ERROR_INVALID;
        for (size_t i = 0U; i < 2U; ++i)
            if (!nx_stm32_usb_remote_separate(
                    remote, config->pins[i].snapshot,
                    sizeof(*config->pins[i].snapshot)))
                return NX_ERROR_INVALID;
        if (remote->request != NULL)
            return NX_ERROR_BUSY;
    }
    if (nx_usb_turnaround(NX_USB_CORE_FS, NEXUS_CORE_HZ) == 0U)
        return NX_ERROR_INVALID;
    if (port->snapshot.epoch == UINT64_MAX)
        return NX_ERROR_INVALID;
    if (*leases != 0U || port->clock_retained || port->snapshot.started ||
        port->publishers != 0U || config->clock->phase != 0U ||
        config->clock->consumers != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        if (config->endpoints[i].request != NULL)
            return NX_ERROR_BUSY;
    }
    nx_time_us_t deadline;
    const nx_result_t budget_status = nx_deadline_finite_after(
        nx_time_now_us(), config->clock_timeout_us, &deadline);
    if (budget_status != NX_SUCCESS)
        return budget_status;
    uint64_t epoch = port->snapshot.epoch;
    memset(port, 0, sizeof(*port));
    port->snapshot.epoch = epoch;
    port->endpoints = config->endpoints;
    port->endpoint_count = config->endpoint_count;
    port->events = config->events;
    port->capacity = config->capacity;
    port->irq_policy = config->irq_policy;
    port->irq.priority = config->priority;
    port->poll_limit = config->poll_limit;
    port->drain_timeout_us = config->drain_timeout_us;
    port->rx_fifo_words = config->rx_fifo_words;
    port->remote = config->remote;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        nx_usb_endpoint_t* endpoint = &config->endpoints[i];
        const nx_stm32_usb_endpoint_config_t* selected = &config->plan[i];
        memset(endpoint, 0, sizeof(*endpoint));
        endpoint->number = selected->number;
        endpoint->in = selected->in;
        endpoint->transfer = selected->transfer;
        endpoint->max_packet = selected->max_packet;
        endpoint->fifo_start = selected->fifo_start;
        endpoint->fifo_words = selected->fifo_words;
    }
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        nx_result_t status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        *leases |= (uint8_t)(1U << i);
        status = nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 10U, 0U,
                                        false);
        if (status != NX_SUCCESS)
            return status;
    }
    nx_result_t status =
        nx_stm32_usb_clock_start(config->clock, true, deadline);
    if (status != NX_SUCCESS)
        return status;
    return nx_stm32_usb_device_initialize(port, config->clock);
}

/** \brief Drain USB, release USB48 and finally restore the original pads.
 * \note A failed drain, gate withdrawal or restore retains its exact owner for
 *       retry. An endpoint/request is never reclaimed by a failed stop.
 */
static inline nx_result_t
nx_stm32_usb_cold_close(nx_usb_controller_t* port,
                        const nx_stm32_usb_cold_config_t* config,
                        uint8_t* leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || leases == NULL ||
        config->clock == NULL || !nx_stm32_packet_pins_valid(config->pins, 2U))
        return NX_ERROR_INVALID;
    /* No failed prepare can own a running USB core before its first pad. */
    if (*leases == 0U &&
        (port->clock_retained || port->snapshot.started ||
         port->publishers != 0U || config->clock->phase != 0U ||
         config->clock->consumers != 0U))
        return NX_ERROR_BUSY;
    nx_result_t status = nx_usb_fs_device_ops.stop(port);
    if (status != NX_SUCCESS)
        return status;
    nx_time_us_t deadline;
    status =
        nx_stm32_usb_withdraw_deadline(config->clock_timeout_us, &deadline);
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32_usb_clock_stop(config->clock, deadline);
    if (status != NX_SUCCESS)
        return status;
    for (size_t i = 2U; i != 0U; --i) {
        uint8_t bit = (uint8_t)(1U << (i - 1U));
        if ((*leases & bit) == 0U)
            continue;
        status = nx_stm32_packet_pin_close(&config->pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        *leases &= (uint8_t)~bit;
    }
    return NX_SUCCESS;
}
#endif
