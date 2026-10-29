/**
 * \file            gd32f470_usb_cold.h
 * \brief           Concrete FS endpoint assembly and retained cold lifecycle
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_USB_COLD_H
#define NX_GD32F470_USB_COLD_H
#include "gd32f470_packet_cold.h"
#include "gd32f470_usb_device.h"
#include <string.h>

/** \brief Readonly endpoint properties, never a second mutable endpoint. */
typedef struct {
    uint8_t number;
    bool in;
    uint8_t transfer;
    uint16_t max_packet;
    uint16_t fifo_start;
    uint16_t fifo_words;
} nx_gd32_usb_endpoint_config_t;

/** \brief Static FS storage and exact selected finite cold policy. */
typedef struct {
    nx_gd32_usb_clock_state_t* clock;
    const nx_gd32_platform_pin_config_t* pins;
    nx_usb_endpoint_t* endpoints;
    const nx_gd32_usb_endpoint_config_t* plan;
    nx_usb_event_t* events;
    size_t endpoint_count;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    uint32_t poll_limit;
    uint32_t drain_timeout_us;
    uint32_t clock_timeout_us;
    uint16_t rx_fifo_words;
    uint8_t priority;
} nx_gd32_usb_cold_config_t;

/** \brief Assemble FS storage and acquire pins/clock without hidden workers.
 * \note Privileged unmasked Thread only. Config, endpoint/event/snapshot and
 *       clock storage outlive successful close. Failed prepare retains effects
 *       for close; it never overwrites a live controller or borrowed request.
 */
static inline nx_result_t
nx_gd32_usb_cold_prepare(nx_usb_controller_t* port,
                         const nx_gd32_usb_cold_config_t* config,
                         uint8_t* leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || leases == NULL ||
        config->clock == NULL || config->endpoints == NULL ||
        config->events == NULL || config->plan == NULL ||
        config->endpoint_count == 0U || config->endpoint_count > 8U ||
        config->capacity == 0U || config->poll_limit == 0U ||
        config->clock_timeout_us == 0U || config->drain_timeout_us == 0U ||
        config->priority > 15U || !nx_gd32_packet_pins_valid(config->pins, 2U))
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
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        nx_usb_endpoint_t* endpoint = &config->endpoints[i];
        const nx_gd32_usb_endpoint_config_t* selected = &config->plan[i];
        memset(endpoint, 0, sizeof(*endpoint));
        endpoint->number = selected->number;
        endpoint->in = selected->in;
        endpoint->transfer = selected->transfer;
        endpoint->max_packet = selected->max_packet;
        endpoint->fifo_start = selected->fifo_start;
        endpoint->fifo_words = selected->fifo_words;
    }
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_gd32_platform_pin_config_t* pin = &config->pins[i];
        nx_result_t status = nx_gd32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_gd32_pin_capture(pin->registers, pin->pin);
        *leases |= (uint8_t)(1U << i);
        status =
            nx_gd32_pin_configure(pin->registers, pin->pin, 2U, 10U, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    nx_result_t status = nx_gd32_usb_clock_start(
        config->clock, true,
        nx_deadline_after(nx_time_now_us(), config->clock_timeout_us));
    if (status != NX_SUCCESS)
        return status;
    return nx_gd32_usb_device_initialize(port, config->clock);
}

/** \brief Drain USB, release USB48 and finally restore the original pads.
 * \note A failed drain, gate withdrawal or restore retains its exact owner for
 *       retry. An endpoint/request is never reclaimed by a failed stop.
 */
static inline nx_result_t
nx_gd32_usb_cold_close(nx_usb_controller_t* port,
                       const nx_gd32_usb_cold_config_t* config,
                       uint8_t* leases) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || leases == NULL ||
        config->clock == NULL || !nx_gd32_packet_pins_valid(config->pins, 2U))
        return NX_ERROR_INVALID;
    if (*leases == 0U &&
        (port->clock_retained || port->snapshot.started ||
         port->publishers != 0U || config->clock->phase != 0U ||
         config->clock->consumers != 0U))
        return NX_ERROR_BUSY;
    nx_result_t status = nx_usb_fs_device_ops.stop(port);
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_usb_clock_stop(
        config->clock,
        nx_deadline_after(nx_time_now_us(), config->clock_timeout_us));
    if (status != NX_SUCCESS)
        return status;
    for (size_t i = 2U; i != 0U; --i) {
        uint8_t bit = (uint8_t)(1U << (i - 1U));
        if ((*leases & bit) == 0U)
            continue;
        status = nx_gd32_packet_pin_close(&config->pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        *leases &= (uint8_t)~bit;
    }
    return NX_SUCCESS;
}
#endif
