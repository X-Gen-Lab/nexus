/**
 * \file            stm32f407_usb_host_cold.h
 * \brief           Static Host pin handover and retained cold rollback
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_HOST_COLD_H
#define NEXUS_STM32F407_USB_HOST_COLD_H
#include "stm32f407_packet_cold.h"
#include "stm32f407_usb_host.h"
#include "stm32f407_usb_pins.h"

typedef struct {
    nx_stm32_usb_host_config_t transport;
    const nx_stm32_platform_pin_config_t* pins;
    uint32_t init_timeout_us;
    uint8_t pin_count;
} nx_stm32_usb_host_cold_config_t;

typedef struct {
    const nx_stm32_usb_host_cold_config_t* config;
    uint16_t leases;
} nx_stm32_usb_host_cold_receipt_t;

/** \brief Exact pad identities match DS8626, independently of external PCB. */
static inline bool
nx_stm32_usb_host_cold_valid(const nx_stm32_usb_host_cold_config_t* config) {
    return config != NULL && config->init_timeout_us != 0U &&
           nx_stm32_usb_pins_valid(config->transport.phy, config->pins,
                                   config->pin_count);
}

/** \brief Privileged cold pin/transport acquisition after power handover.
 * \param[in,out] state: Static unborrowed host controller storage.
 * \param[in] config: Stable complete physical/transport selection.
 * \param[in,out] receipt: Zero-initialized storage outliving successful close.
 * \return SUCCESS or failure retaining exact partial effects for close.
 * \note Caller owns actual VBUS/PHY power and restores it only after close.
 */
static inline nx_result_t
nx_stm32_usb_host_cold_prepare(nx_stm32_usb_host_t* state,
                               const nx_stm32_usb_host_cold_config_t* config,
                               nx_stm32_usb_host_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || receipt == NULL ||
        !nx_stm32_usb_host_cold_valid(config) ||
        nx_stm32_usb_host_validate(state, &config->transport) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    if (receipt->config != NULL || receipt->leases != 0U ||
        state->config != NULL || state->clock_receipt != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_time_us_t deadline;
    nx_result_t status = nx_deadline_finite_after(
        nx_time_now_us(), config->init_timeout_us, &deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    for (uint8_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if ((pin->registers->LCKR & (1U << 16U)) != 0U &&
            (pin->registers->LCKR & (1U << pin->pin)) != 0U) {
            return NX_ERROR_BUSY;
        }
    }
    receipt->config = config;
    uint8_t af =
        config->transport.phy == NX_STM32_USB_HS_INTERNAL_FS ? 12U : 10U;
    for (uint8_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS) {
            return status;
        }
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->leases |= (uint16_t)(1U << i);
        status =
            nx_stm32_pin_configure(pin->registers, pin->pin, 2U, af, 0U, false);
        if (status != NX_SUCCESS) {
            return status;
        }
    }
    return nx_stm32_usb_host_initialize(state, &config->transport, deadline);
}

/** \brief Drain transport before restoring any retained PHY pad.
 * \param[in,out] state: Original exact host storage, kept alive on failure.
 * \param[in] config: Same immutable config used by prepare.
 * \param[in,out] receipt: Same exact retained transaction receipt.
 * \return SUCCESS after all effects withdraw; failure retains responsibility.
 */
static inline nx_result_t
nx_stm32_usb_host_cold_close(nx_stm32_usb_host_t* state,
                             const nx_stm32_usb_host_cold_config_t* config,
                             nx_stm32_usb_host_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || receipt == NULL ||
        !nx_stm32_usb_host_cold_valid(config)) {
        return NX_ERROR_INVALID;
    }
    if (receipt->config == NULL) {
        return receipt->leases == 0U && state->config == NULL ? NX_SUCCESS
                                                              : NX_ERROR_BUSY;
    }
    if (receipt->config != config ||
        (state->config != NULL && state->config != &config->transport)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t status = nx_stm32_usb_host_ops.stop(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    for (uint8_t i = config->pin_count; i != 0U; --i) {
        uint16_t bit = (uint16_t)(1U << (i - 1U));
        if ((receipt->leases & bit) == 0U) {
            continue;
        }
        status = nx_stm32_packet_pin_close(&config->pins[i - 1U]);
        if (status != NX_SUCCESS) {
            return status;
        }
        receipt->leases &= (uint16_t)~bit;
    }
    receipt->config = NULL;
    return NX_SUCCESS;
}
#endif
