/**
 * \file            stm32f407_packet_cold.h
 * \brief           Exact packet pad and shared RMII clock transactions
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_PACKET_COLD_H
#define NX_STM32F407_PACKET_COLD_H
#include "stm32f407_platform_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief Retained RMII pads and selector; each snapshot remains unique. */
typedef struct {
    uint32_t mux_saved;
    uint16_t leases;
    bool mux_leased;
} nx_stm32_eth_pin_receipt_t;

/** \brief Shared MAC/SYSCFG gate bits; GPIO remains the Platform lease. */
typedef struct {
    uint32_t mac_added;
    uint32_t syscfg_added;
} nx_stm32_eth_resource_receipt_t;

/** \brief Check static packet pads before any state or MMIO write. */
static inline bool
nx_stm32_packet_pins_valid(const nx_stm32_platform_pin_config_t* pins,
                           size_t count) {
    if (pins == NULL || count == 0U || count > 9U)
        return false;
    for (size_t i = 0U; i < count; ++i) {
        if (pins[i].registers == NULL || pins[i].snapshot == NULL ||
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
nx_stm32_packet_pin_close(const nx_stm32_platform_pin_config_t* pin) {
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
    return nx_stm32_pin_capture(pin->registers, pin->pin) == *pin->snapshot
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

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

/** \brief Connect RMII pads and selector after the Platform admits ownership.
 * \note Exact nine-pad state remains retained across every partial failure.
 */
static inline nx_result_t
nx_stm32_eth_pins_prepare(const nx_stm32_platform_pin_config_t* pins,
                          nx_stm32_eth_pin_receipt_t* receipt, bool frames) {
    for (size_t i = 0U; i < 9U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &pins[i];
        nx_result_t status = nx_stm32_gpio_clock_enable(pin->port_index);
        if (status != NX_SUCCESS)
            return status;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->leases |= (uint16_t)(1U << i);
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
    NX_PLATFORM_REGISTER_WRITE(SYSCFG->PMC,
                               SYSCFG->PMC | SYSCFG_PMC_MII_RMII_SEL);
    nx_arch_dsb();
    return (SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL) != 0U ? NX_SUCCESS
                                                         : NX_ERROR_IO;
}

/** \brief Restore retained RMII pads before releasing the RMII selector. */
static inline nx_result_t
nx_stm32_eth_pins_close(const nx_stm32_platform_pin_config_t* pins,
                        nx_stm32_eth_pin_receipt_t* receipt) {
    for (size_t i = 9U; i != 0U; --i) {
        uint16_t bit = (uint16_t)(1U << (i - 1U));
        if ((receipt->leases & bit) == 0U)
            continue;
        nx_result_t status = nx_stm32_packet_pin_close(&pins[i - 1U]);
        if (status != NX_SUCCESS)
            return status;
        receipt->leases &= (uint16_t)~bit;
    }
    if (receipt->mux_leased) {
        NX_PLATFORM_REGISTER_WRITE(SYSCFG->PMC,
                                   (SYSCFG->PMC & ~SYSCFG_PMC_MII_RMII_SEL) |
                                       receipt->mux_saved);
        nx_arch_dsb();
        if ((SYSCFG->PMC & SYSCFG_PMC_MII_RMII_SEL) != receipt->mux_saved)
            return NX_ERROR_IO;
        receipt->mux_leased = false;
    }
    return NX_SUCCESS;
}
#endif
