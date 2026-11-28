/**
 * \file            raw.c
 * \brief           STM32F107 Ethernet pins, clock retention and IRQ withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/ethernet/raw.h"
#include "drivers/ethernet/ethernet_clock.h"
#include "private/hardware.h"
#include "system/platform.h"
#include <string.h>

#if !defined(STM32F107xC) || !defined(NX_STM32F1_CONNECTIVITY_PLL2)
#error "Raw Ethernet requires the exact STM32F107 Connectivity PLL2 profile"
#endif
_Static_assert(NEXUS_CORE_HZ == 72000000U && NEXUS_HSE_HZ == 25000000U,
               "Review HCLK and MDC divider together");
_Static_assert(ETH_IRQn == 61 && offsetof(ETH_TypeDef, RESERVED1[1]) == 0x34U,
               "Exact F107 vector and absence of MAC debug register");

#define F107_ETH_MAPR_MASK (AFIO_MAPR_ETH_REMAP | AFIO_MAPR_MII_RMII_SEL)
#if defined(NX_STM32F1_ETH_RMII)
#define F107_ETH_MAPR_SELECTED F107_ETH_MAPR_MASK
#define F107_ETH_MII           false
#else
#define F107_ETH_MAPR_SELECTED AFIO_MAPR_ETH_REMAP
#define F107_ETH_MII           true
#endif
#define F107_ETH_GATES                                                         \
    (RCC_AHBENR_ETHMACEN | RCC_AHBENR_ETHMACTXEN | RCC_AHBENR_ETHMACRXEN |     \
     RCC_AHBENR_DMA2EN)
#define PAD(pin) (UINT16_C(1) << (pin))

enum {
    AFIO_ADDED = 1U,
    REMAP_OWNED = 2U,
    MAC_GATES_OWNED = 4U,
    IRQ_PRIORITY_OWNED = 8U,
    ETH_IRQ_WORD = (unsigned)ETH_IRQn / 32U,
};
#define ETH_IRQ_BIT (UINT32_C(1) << ((unsigned)ETH_IRQn % 32U))

static const struct {
    GPIO_TypeDef* port;
    uint16_t pins;
    uint16_t output;
} ethernet_pads[] = {
#if defined(NX_STM32F1_ETH_RMII)
    {GPIOA, PAD(1) | PAD(2), PAD(2)},
    {GPIOB, PAD(11) | PAD(12) | PAD(13), PAD(11) | PAD(12) | PAD(13)},
    {GPIOC, PAD(1), PAD(1)},
    {GPIOD, PAD(8) | PAD(9) | PAD(10), 0U},
#else
    {GPIOA, PAD(0) | PAD(1) | PAD(2) | PAD(3), PAD(2)},
    {GPIOB, PAD(8) | PAD(10) | PAD(11) | PAD(12) | PAD(13),
     PAD(8) | PAD(11) | PAD(12) | PAD(13)},
    {GPIOC, PAD(1) | PAD(2) | PAD(3), PAD(1) | PAD(2)},
    {GPIOD, PAD(8) | PAD(9) | PAD(10) | PAD(11) | PAD(12), 0U},
#endif
};

/** \brief Preserve SWJ state and verify only the selected Ethernet bits. */
static bool set_remap(uint32_t selected) {
    const uint32_t mask = F107_ETH_MAPR_MASK | AFIO_MAPR_SWJ_CFG;
    NX_STM32F1_WRITE(&AFIO->MAPR, (AFIO->MAPR & ~mask) |
                                      (selected & F107_ETH_MAPR_MASK) |
                                      AFIO_MAPR_SWJ_CFG);
    nx_arch_dsb();
    return (AFIO->MAPR & F107_ETH_MAPR_MASK) == (selected & F107_ETH_MAPR_MASK);
}

/** \brief Publish or join only the exact MAC external interrupt. */
static bool irq_idle(void) {
    return ((NVIC->ISER[ETH_IRQ_WORD] | NVIC->ISPR[ETH_IRQ_WORD] |
             NVIC->IABR[ETH_IRQ_WORD]) &
            ETH_IRQ_BIT) == 0U;
}

static nx_result_t irq_control(nx_eth_mac_t* mac, bool enabled) {
    if (enabled) {
        NVIC_ClearPendingIRQ(ETH_IRQn);
        NVIC_SetPriority(ETH_IRQn, mac->source.priority);
        if (NVIC_GetPriority(ETH_IRQn) != mac->source.priority)
            return NX_ERROR_IO;
        NVIC_EnableIRQ(ETH_IRQn);
        nx_arch_dsb();
        return (NVIC->ISER[ETH_IRQ_WORD] & ETH_IRQ_BIT) != 0U ? NX_SUCCESS
                                                              : NX_ERROR_IO;
    }
    NVIC_DisableIRQ(ETH_IRQn);
    NVIC_ClearPendingIRQ(ETH_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    return irq_idle() ? NX_SUCCESS : NX_ERROR_BUSY;
}

static void pend_irq(nx_eth_mac_t* mac) {
    (void)mac;
    NVIC_SetPendingIRQ(ETH_IRQn);
}

static nx_result_t validate_wake(nx_eth_mac_t* mac, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    const nx_irq_source_t actual = {ETH_IRQn,
                                    (uint8_t)NVIC_GetPriority(ETH_IRQn),
                                    (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}

/** \brief Install the selected, externally clocked MB784 pad roles. */
static nx_result_t prepare_pads(nx_stm32f1_eth_receipt_t* receipt) {
    for (size_t group = 0U;
         group < sizeof(ethernet_pads) / sizeof(ethernet_pads[0]); ++group) {
        nx_stm32f1_pads_t* pad = &receipt->pads[group];
        nx_result_t result = nx_stm32f1_pads_capture(
            pad, ethernet_pads[group].port, ethernet_pads[group].pins);
        if (result != NX_SUCCESS)
            return result;
        uint32_t cr[2] = {0U, 0U};
        for (unsigned pin = 0U; pin < 16U; ++pin) {
            const uint32_t nibble =
                (ethernet_pads[group].output & PAD(pin)) != 0U ? 11U : 4U;
            cr[pin / 8U] |= nibble << (4U * (pin % 8U));
        }
        result = nx_stm32f1_pads_apply(pad, cr[0], cr[1], pad->latch);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_eth_prepare(nx_eth_mac_t* mac,
                                   const nx_stm32f1_eth_config_t* config,
                                   nx_stm32f1_eth_receipt_t* receipt) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (config->mii != F107_ETH_MII ||
        config->irq_policy.kernel != NX_IRQ_KERNEL_NONE ||
        config->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        config->priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_ethernet_clock_ready() || receipt->effects != 0U)
        return NX_ERROR_STATE;
    if ((RCC->AHBENR & F107_ETH_GATES) != 0U ||
        (RCC->AHBRSTR & RCC_AHBRSTR_ETHMACRST) != 0U || !irq_idle())
        return NX_ERROR_BUSY;

    receipt->prior_irq_priority = (uint8_t)NVIC_GetPriority(ETH_IRQn);
    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        receipt->effects |= AFIO_ADDED;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN))
            return NX_ERROR_IO;
    }
    receipt->prior_mapr = AFIO->MAPR & F107_ETH_MAPR_MASK;
    receipt->effects |= REMAP_OWNED;
    if (!set_remap(F107_ETH_MAPR_SELECTED))
        return NX_ERROR_IO;
    nx_result_t pads = prepare_pads(receipt);
    if (pads != NX_SUCCESS)
        return pads;

    /* ES022 §2.14.11: keep another AHB DMA master clocked while MAC owns
     * storage, including idle. No other controller may own the DMA2 gate. */
    receipt->effects |= MAC_GATES_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, F107_ETH_GATES, F107_ETH_GATES))
        return NX_ERROR_IO;

    mac->registers = ETH_BASE;
    mac->regions = config->regions;
    mac->region_count = 1U;
    mac->tx_descriptors = config->tx_descriptors;
    mac->rx_descriptors = config->rx_descriptors;
    mac->tx_slots = config->tx_slots;
    mac->rx_slots = config->rx_slots;
    mac->tx_count = config->tx_count;
    mac->rx_count = config->rx_count;
    mac->drain_timeout_us = config->drain_timeout_us;
    mac->hclk_hz = NEXUS_CORE_HZ;
    mac->irq_policy = config->irq_policy;
    mac->source = (nx_irq_source_t){ETH_IRQn, config->priority, 0U};
    mac->mii = F107_ETH_MII;
    mac->irq_control = irq_control;
    mac->pend_irq = pend_irq;
    mac->validate_wake = validate_wake;
    memcpy(mac->address, config->address, sizeof(mac->address));
    for (size_t slot = 0U; slot < mac->rx_count; ++slot) {
        mac->rx_slots[slot].data =
            config->storage + slot * config->rx_storage_bytes;
        mac->rx_slots[slot].capacity = config->rx_storage_bytes;
    }
    nx_time_us_t deadline;
    nx_result_t valid = nx_deadline_finite_after(
        nx_time_now_us(), config->init_timeout_us, &deadline);
    if (valid != NX_SUCCESS)
        return valid;
    receipt->effects |= IRQ_PRIORITY_OWNED;
    return nx_eth_mac_initialize(mac, deadline);
}

nx_result_t nx_stm32f1_eth_close(nx_eth_mac_t* mac,
                                 const nx_stm32f1_eth_config_t* config,
                                 nx_stm32f1_eth_receipt_t* receipt) {
    (void)config;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((receipt->effects & MAC_GATES_OWNED) != 0U) {
        if (mac->retained && (RCC->AHBENR & F107_ETH_GATES) != F107_ETH_GATES)
            return NX_ERROR_STATE;
        nx_result_t stopped = nx_eth_mac_ops.stop(mac);
        if (stopped != NX_SUCCESS)
            return stopped;
        if ((receipt->effects & IRQ_PRIORITY_OWNED) != 0U) {
            if (!irq_idle())
                return NX_ERROR_BUSY;
            NVIC_SetPriority(ETH_IRQn, receipt->prior_irq_priority);
            if (NVIC_GetPriority(ETH_IRQn) != receipt->prior_irq_priority)
                return NX_ERROR_IO;
            receipt->effects &= (uint8_t)~IRQ_PRIORITY_OWNED;
        }
        if (!nx_stm32f1_write_masked(&RCC->AHBENR, F107_ETH_GATES, 0U))
            return NX_ERROR_IO;
        receipt->effects &= (uint8_t)~MAC_GATES_OWNED;
    }
    for (size_t group = sizeof(ethernet_pads) / sizeof(ethernet_pads[0]);
         group != 0U; --group) {
        nx_result_t result =
            nx_stm32f1_pads_restore(&receipt->pads[group - 1U]);
        if (result != NX_SUCCESS)
            return result;
    }
    if ((receipt->effects & REMAP_OWNED) != 0U) {
        if (!set_remap(receipt->prior_mapr))
            return NX_ERROR_IO;
        receipt->effects &= (uint8_t)~REMAP_OWNED;
    }
    if ((receipt->effects & AFIO_ADDED) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U))
        return NX_ERROR_IO;
    receipt->effects = 0U;
    return NX_SUCCESS;
}
