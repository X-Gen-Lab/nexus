/**
 * \file            stm32f407_usb_hs_cold.h
 * \brief           HS device static storage, PHY pads and clock receipt
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_HS_COLD_H
#define NEXUS_STM32F407_USB_HS_COLD_H
#include "stm32f407_usb_cold.h"
#include "stm32f407_usb_hs_clock.h"
#include "stm32f407_usb_pins.h"

typedef struct {
    nx_stm32_usb_hs_clock_state_t* clock;
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
    uint32_t ulpi_clock_hz;
    uint16_t rx_fifo_words;
    uint8_t priority;
    uint8_t pin_count;
    nx_stm32_usb_phy_t phy;
    const nx_usb_dma_storage_t* dma_storage;
    const nx_dma_memory_region_t* dma_regions;
    size_t dma_region_count;
    bool dma;
} nx_stm32_usb_hs_cold_config_t;

typedef struct {
    const nx_stm32_usb_hs_cold_config_t* config;
    uint16_t leases;
} nx_stm32_usb_hs_cold_receipt_t;

/** \brief Prepare static HS storage after explicit VBUS/PHY power handover.
 * \param[in,out] port: Unborrowed exact selected controller.
 * \param[in] config: Immutable complete endpoint/pin/clock plan.
 * \param[in,out] receipt: Fresh owner, retained until complete close.
 * \return SUCCESS or failure with its exact partial responsibility retained.
 * \note Privileged unmasked task; no allocation, worker or product policy.
 */
static inline nx_result_t
nx_stm32_usb_hs_cold_prepare(nx_usb_controller_t* port,
                             const nx_stm32_usb_hs_cold_config_t* config,
                             nx_stm32_usb_hs_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        config->clock == NULL || config->endpoints == NULL ||
        config->plan == NULL || config->events == NULL ||
        config->endpoint_count < 2U || config->endpoint_count > 12U ||
        config->capacity == 0U || config->poll_limit == 0U ||
        config->clock_timeout_us == 0U || config->drain_timeout_us == 0U ||
        config->priority > 15U ||
        (config->phy != NX_STM32_USB_HS_INTERNAL_FS &&
         config->phy != NX_STM32_USB_HS_ULPI) ||
        !nx_stm32_usb_pins_valid(config->phy, config->pins,
                                 config->pin_count) ||
        config->ulpi_clock_hz !=
            (config->phy == NX_STM32_USB_HS_ULPI ? 60000000U : 0U) ||
        port->snapshot.epoch == UINT64_MAX)
        return NX_ERROR_INVALID;
    uint8_t core = config->phy == NX_STM32_USB_HS_ULPI
                       ? NX_USB_CORE_HS_ULPI
                       : NX_USB_CORE_HS_INTERNAL_FS;
    uint8_t turnaround = nx_usb_turnaround(core, NEXUS_CORE_HZ);
    if (turnaround == 0U)
        return NX_ERROR_INVALID;
    if (receipt->config != NULL || receipt->leases != 0U ||
        port->clock_retained || port->snapshot.started ||
        port->publishers != 0U || config->clock->phase != 0U ||
        config->clock->consumers != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        if (config->endpoints[i].request != NULL)
            return NX_ERROR_BUSY;
    }
    nx_time_us_t deadline;
    nx_result_t status = nx_deadline_finite_after(
        nx_time_now_us(), config->clock_timeout_us, &deadline);
    if (status != NX_SUCCESS)
        return status;
    uint64_t epoch = port->snapshot.epoch;
    memset(port, 0, sizeof(*port));
    port->snapshot.epoch = epoch;
    port->registers = USB_OTG_HS_PERIPH_BASE;
    port->core = core;
    port->turnaround = turnaround;
    port->endpoints = config->endpoints;
    port->endpoint_count = config->endpoint_count;
    port->events = config->events;
    port->capacity = config->capacity;
    port->irq_policy = config->irq_policy;
    port->irq = (nx_irq_source_t){OTG_HS_IRQn, config->priority, 3U};
    port->poll_limit = config->poll_limit;
    port->drain_timeout_us = config->drain_timeout_us;
    port->rx_fifo_words = config->rx_fifo_words;
    port->dma = config->dma;
    port->dma_storage = config->dma_storage;
    port->dma_regions = config->dma_regions;
    port->dma_region_count = config->dma_region_count;
    port->clock = (nx_usb_clock_domain_t){
        &nx_stm32_usb_hs_clock_ops, config->clock, sizeof(*config->clock)};
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
    status = nx_usb_controller_plan_validate(port);
    if (status != NX_SUCCESS)
        return status;
    for (uint8_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if ((pin->registers->LCKR & (1U << 16U)) != 0U &&
            (pin->registers->LCKR & (1U << pin->pin)) != 0U)
            return NX_ERROR_BUSY;
    }
    receipt->config = config;
    uint8_t af = config->phy == NX_STM32_USB_HS_INTERNAL_FS ? 12U : 10U;
    for (uint8_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->leases |= (uint16_t)(1U << i);
        status =
            nx_stm32_pin_configure(pin->registers, pin->pin, 2U, af, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    status = nx_stm32_usb_hs_clock_start(config->clock,
                                         config->phy == NX_STM32_USB_HS_ULPI,
                                         config->ulpi_clock_hz, deadline);
    if (status != NX_SUCCESS)
        return status;
    return nx_stm32_usb_device_hs_initialize(
        port, &port->clock, config->phy == NX_STM32_USB_HS_ULPI);
}

/** \brief Drain USB before releasing HS/ULPI clock and restoring pads.
 * \param[in,out] port: Same exact controller with requests still alive.
 * \param[in] config: Same immutable configuration used for prepare.
 * \param[in,out] receipt: Original retained owner, never interchangeable.
 * \return SUCCESS after full release; failure preserves cleanup and loans.
 */
static inline nx_result_t
nx_stm32_usb_hs_cold_close(nx_usb_controller_t* port,
                           const nx_stm32_usb_hs_cold_config_t* config,
                           nx_stm32_usb_hs_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        config->clock == NULL || config->clock_timeout_us == 0U ||
        !nx_stm32_usb_pins_valid(config->phy, config->pins, config->pin_count))
        return NX_ERROR_INVALID;
    if (receipt->config == NULL)
        return receipt->leases == 0U && !port->snapshot.started &&
                       !port->clock_retained && port->publishers == 0U &&
                       config->clock->phase == 0U &&
                       config->clock->consumers == 0U
                   ? NX_SUCCESS
                   : NX_ERROR_BUSY;
    if (receipt->config != config)
        return NX_ERROR_BUSY;
    nx_result_t status = nx_usb_fs_device_ops.stop(port);
    if (status != NX_SUCCESS)
        return status;
    nx_time_us_t deadline;
    status = nx_deadline_finite_after(nx_time_now_us(),
                                      config->clock_timeout_us, &deadline);
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32_usb_hs_clock_stop(config->clock, deadline);
    if (status != NX_SUCCESS)
        return status;
    for (uint8_t i = config->pin_count; i != 0U; --i) {
        uint16_t bit = (uint16_t)(1U << (i - 1U));
        if ((receipt->leases & bit) == 0U)
            continue;
        status = nx_stm32_packet_pin_close(&config->pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        receipt->leases &= (uint16_t)~bit;
    }
    receipt->config = NULL;
    return NX_SUCCESS;
}
#endif
