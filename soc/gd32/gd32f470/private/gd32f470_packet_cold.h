/**
 * \file            gd32f470_packet_cold.h
 * \brief           Exact packet pad and shared RMII clock transactions
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_PACKET_COLD_H
#define NX_GD32F470_PACKET_COLD_H
#include "gd32f470_platform_cold.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include "nexus/arch/arch.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief Retained RMII pads and selector; each snapshot remains unique. */
typedef struct {
    uint32_t mux_saved;
    uint16_t leases;
    bool mux_leased;
} nx_gd32_eth_pin_receipt_t;

/** \brief Shared MAC/SYSCFG gate bits; GPIO remains the Platform lease. */
typedef struct {
    uint32_t mac_added;
    uint32_t syscfg_added;
} nx_gd32_eth_resource_receipt_t;

/** \brief Check static packet pads before any state or MMIO write. */
static inline bool
nx_gd32_packet_pins_valid(const nx_gd32_platform_pin_config_t* pins,
                          size_t count) {
    if (pins == NULL || count == 0U || count > 9U)
        return false;
    for (size_t i = 0U; i < count; ++i) {
        if (pins[i].registers == 0U || pins[i].snapshot == NULL ||
            pins[i].port_index > 8U || pins[i].pin > 15U)
            return false;
        for (size_t j = 0U; j < i; ++j) {
            if (pins[i].snapshot == pins[j].snapshot ||
                (pins[i].registers == pins[j].registers &&
                 pins[i].pin == pins[j].pin))
                return false;
        }
    }
    return true;
}

/** \brief Withdraw and read back one leased packet pad before restoration.
 * \note The caller keeps its lease bit until the exact original fields read
 *       back. Failed withdrawal does not restore a still-driven latch.
 */
static inline nx_result_t
nx_gd32_packet_pin_close(const nx_gd32_platform_pin_config_t* pin) {
    uint32_t mask = 3U << (pin->pin * 2U);
    uint32_t mode = GPIO_CTL(pin->registers) & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(pin->registers),
                                   GPIO_CTL(pin->registers) | mask);
        nx_arch_dsb();
        if ((GPIO_CTL(pin->registers) & mask) != mask)
            return NX_ERROR_IO;
    }
    nx_gd32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
    return nx_gd32_pin_capture(pin->registers, pin->pin) == *pin->snapshot
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Capture shared RMII gates before any selected constructor.
 * \note Platform cold admission and restart exclusion precede this call.
 *       GPIO clocks are exclusively accounted by the common Platform owner.
 */
static inline void
nx_gd32_eth_resources_capture(nx_gd32_eth_resource_receipt_t* receipt,
                              bool frames) {
    uint32_t mask =
        RCU_AHB1EN_ENETEN | RCU_AHB1EN_ENETTXEN | RCU_AHB1EN_ENETRXEN;
    if (frames)
        mask |= RCU_AHB1EN_ENETPTPEN;
    receipt->mac_added = ~RCU_AHB1EN & mask;
    receipt->syscfg_added = ~RCU_APB2EN & RCU_APB2EN_SYSCFGEN;
}

/** \brief Release MAC then selector gates after every selected port drains.
 * \note Retained bits survive refused readback. No unrelated gate is changed.
 */
static inline nx_result_t
nx_gd32_eth_resources_close(nx_gd32_eth_resource_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->mac_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN,
                                   RCU_AHB1EN & ~receipt->mac_added);
        nx_arch_dsb();
        if ((RCU_AHB1EN & receipt->mac_added) != 0U)
            return NX_ERROR_IO;
        receipt->mac_added = 0U;
    }
    if (receipt->syscfg_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_APB2EN,
                                   RCU_APB2EN & ~receipt->syscfg_added);
        nx_arch_dsb();
        if ((RCU_APB2EN & receipt->syscfg_added) != 0U)
            return NX_ERROR_IO;
        receipt->syscfg_added = 0U;
    }
    return NX_SUCCESS;
}

/** \brief Connect RMII pads and selector after the Platform admits ownership.
 * \note Exact nine-pad state remains retained across every partial failure.
 */
static inline nx_result_t
nx_gd32_eth_pins_prepare(const nx_gd32_platform_pin_config_t* pins,
                         nx_gd32_eth_pin_receipt_t* receipt, bool frames) {
    for (size_t i = 0U; i < 9U; ++i) {
        const nx_gd32_platform_pin_config_t* pin = &pins[i];
        nx_result_t status = nx_gd32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_gd32_pin_capture(pin->registers, pin->pin);
        receipt->leases |= (uint16_t)(1U << i);
        status =
            nx_gd32_pin_configure(pin->registers, pin->pin, 2U, 11U, 0U, false);
        if (status != NX_SUCCESS)
            return status;
    }
    uint32_t mask =
        RCU_AHB1EN_ENETEN | RCU_AHB1EN_ENETTXEN | RCU_AHB1EN_ENETRXEN;
    if (frames)
        mask |= RCU_AHB1EN_ENETPTPEN;
    NX_PLATFORM_REGISTER_WRITE(RCU_APB2EN, RCU_APB2EN | RCU_APB2EN_SYSCFGEN);
    NX_PLATFORM_REGISTER_WRITE(RCU_AHB1EN, RCU_AHB1EN | mask);
    nx_arch_dsb();
    if ((RCU_APB2EN & RCU_APB2EN_SYSCFGEN) == 0U || (RCU_AHB1EN & mask) != mask)
        return NX_ERROR_IO;
    receipt->mux_saved = SYSCFG_CFG1 & ((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL);
    receipt->mux_leased = true;
    NX_PLATFORM_REGISTER_WRITE(
        SYSCFG_CFG1, SYSCFG_CFG1 | ((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL));
    nx_arch_dsb();
    return (SYSCFG_CFG1 & ((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL)) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Restore retained RMII pads before releasing the RMII selector. */
static inline nx_result_t
nx_gd32_eth_pins_close(const nx_gd32_platform_pin_config_t* pins,
                       nx_gd32_eth_pin_receipt_t* receipt) {
    for (size_t i = 9U; i != 0U; --i) {
        uint16_t bit = (uint16_t)(1U << (i - 1U));
        if ((receipt->leases & bit) == 0U)
            continue;
        nx_result_t status = nx_gd32_packet_pin_close(&pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        receipt->leases &= (uint16_t)~bit;
    }
    if (receipt->mux_leased) {
        NX_PLATFORM_REGISTER_WRITE(
            SYSCFG_CFG1, (SYSCFG_CFG1 & ~((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL)) |
                             receipt->mux_saved);
        nx_arch_dsb();
        if ((SYSCFG_CFG1 & ((uint32_t)SYSCFG_CFG1_ENET_PHY_SEL)) !=
            receipt->mux_saved)
            return NX_ERROR_IO;
        receipt->mux_leased = false;
    }
    return NX_SUCCESS;
}
#endif
