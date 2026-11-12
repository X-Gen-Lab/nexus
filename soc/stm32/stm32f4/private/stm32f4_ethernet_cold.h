/**
 * \file            stm32f4_ethernet_cold.h
 * \brief           Ethernet-only pad, MII/RMII selector and clock receipts
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_ETHERNET_COLD_H
#define NEXUS_STM32F4_ETHERNET_COLD_H
#include "stm32f407_packet_cold.h"

/** \brief Retained Ethernet pads/selector; each snapshot remains unique. */
typedef struct {
    uint32_t mux_saved;
    uint32_t leases;
    bool mux_leased;
} nx_stm32_eth_pin_receipt_t;

/** \brief Shared MAC/SYSCFG gate bits; GPIO remains the Platform lease. */
typedef struct nx_stm32_eth_resource_receipt {
    uint32_t mac_added;
    uint32_t syscfg_added;
} nx_stm32_eth_resource_receipt_t;

/** \brief Capture shared RMII gates before any selected constructor.
 * \note Platform cold admission and restart exclusion precede this call.
 *       GPIO clocks are exclusively accounted by the common Platform owner.
 */
static inline void
nx_stm32_eth_resources_capture(nx_stm32_eth_resource_receipt_t* receipt,
                               bool frames) {
    uint32_t mask =
        RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACTXEN | RCC_AHB1ENR_ETHMACRXEN;
    if (frames)
        mask |= RCC_AHB1ENR_ETHMACPTPEN;
    receipt->mac_added = ~RCC->AHB1ENR & mask;
    receipt->syscfg_added = ~RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;
}

/** \brief Release MAC then selector gates after every selected port drains.
 * \note Retained bits survive refused readback. No unrelated gate is changed.
 */
static inline nx_result_t
nx_stm32_eth_resources_close(nx_stm32_eth_resource_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->mac_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,
                                   RCC->AHB1ENR & ~receipt->mac_added);
        nx_arch_dsb();
        if ((RCC->AHB1ENR & receipt->mac_added) != 0U)
            return NX_ERROR_IO;
        receipt->mac_added = 0U;
    }
    if (receipt->syscfg_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR,
                                   RCC->APB2ENR & ~receipt->syscfg_added);
        nx_arch_dsb();
        if ((RCC->APB2ENR & receipt->syscfg_added) != 0U)
            return NX_ERROR_IO;
        receipt->syscfg_added = 0U;
    }
    return NX_SUCCESS;
}

/** \brief Connect selected pads and selector under Platform ownership.
 * \note Exact nine/seventeen-pad state survives every partial failure.
 */
static inline nx_result_t
nx_stm32_eth_pins_prepare_mode(const nx_stm32_platform_pin_config_t* pins,
                               nx_stm32_eth_pin_receipt_t* receipt, bool frames,
                               bool mii) {
    size_t count = mii ? 17U : 9U;
    for (size_t i = 0U; i < count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &pins[i];
        nx_result_t status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->leases |= 1U << i;
        status = nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 11U, 0U,
                                        false);
        if (status != NX_SUCCESS)
            return status;
    }
    uint32_t mask =
        RCC_AHB1ENR_ETHMACEN | RCC_AHB1ENR_ETHMACTXEN | RCC_AHB1ENR_ETHMACRXEN;
    if (frames)
        mask |= RCC_AHB1ENR_ETHMACPTPEN;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR,
                               RCC->APB2ENR | RCC_APB2ENR_SYSCFGEN);
    NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR, RCC->AHB1ENR | mask);
    nx_arch_dsb();
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U ||
        (RCC->AHB1ENR & mask) != mask)
        return NX_ERROR_IO;
    receipt->mux_saved = SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL;
    receipt->mux_leased = true;
    NX_PLATFORM_REGISTER_WRITE(
        SYSCFG->PMC, (SYSCFG->PMC & ~(uint32_t)SYSCFG_PMC_MII_RMII_SEL) |
                         (mii ? 0U : SYSCFG_PMC_MII_RMII_SEL));
    nx_arch_dsb();
    return ((SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL) != 0U) != mii ? NX_SUCCESS
                                                                  : NX_ERROR_IO;
}

/** \brief Restore every selected pad before releasing the media selector. */
static inline nx_result_t
nx_stm32_eth_pins_close_mode(const nx_stm32_platform_pin_config_t* pins,
                             nx_stm32_eth_pin_receipt_t* receipt, bool mii) {
    for (size_t i = mii ? 17U : 9U; i != 0U; --i) {
        uint32_t bit = 1U << (i - 1U);
        if ((receipt->leases & bit) == 0U)
            continue;
        nx_result_t status = nx_stm32_packet_pin_close(&pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        receipt->leases &= ~bit;
    }
    if (receipt->mux_leased) {
        NX_PLATFORM_REGISTER_WRITE(
            SYSCFG->PMC, (SYSCFG->PMC & ~(uint32_t)SYSCFG_PMC_MII_RMII_SEL) |
                             receipt->mux_saved);
        nx_arch_dsb();
        if ((SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL) != receipt->mux_saved)
            return NX_ERROR_IO;
        receipt->mux_leased = false;
    }
    return NX_SUCCESS;
}
/** \brief Preserve the existing nine-pad RMII cold entry point. */
static inline nx_result_t
nx_stm32_eth_pins_prepare(const nx_stm32_platform_pin_config_t* pins,
                          nx_stm32_eth_pin_receipt_t* receipt, bool frames) {
    return nx_stm32_eth_pins_prepare_mode(pins, receipt, frames, false);
}
/** \brief Preserve the existing nine-pad RMII withdrawal entry point. */
static inline nx_result_t
nx_stm32_eth_pins_close(const nx_stm32_platform_pin_config_t* pins,
                        nx_stm32_eth_pin_receipt_t* receipt) {
    return nx_stm32_eth_pins_close_mode(pins, receipt, false);
}
#endif
