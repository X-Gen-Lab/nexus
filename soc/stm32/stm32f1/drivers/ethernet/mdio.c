/**
 * \file            mdio.c
 * \brief           STM32F107 private Clause22 management on the PLL2 profile.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/ethernet/mdio.h"
#include "dispatch_checks.h"
#include "drivers/ethernet/ethernet_clock.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "system/clock_profile.h"
#include "system/platform.h"

#if !defined(STM32F107xC) || !defined(NX_STM32F1_CONNECTIVITY_PLL2)
#error "F107 MDIO requires the exact connectivity PLL2 profile"
#endif

#if !defined(NEXUS_CMSIS_HOST_MODEL)
_Static_assert(NEXUS_CORE_HZ == 72000000U && NEXUS_HSE_HZ == 25000000U,
               "MDIO uses the verified F107 HCLK/42 clock range");
#endif
_Static_assert(ETH_MACMIIAR_CR_DIV42 == 0U && 72000000U / 42U < 2500000U,
               "RM0008 Table 211 and the Clause22 MDC ceiling");

#define MAC_GATES                                                              \
    (RCC_AHBENR_ETHMACEN | RCC_AHBENR_ETHMACRXEN | RCC_AHBENR_ETHMACTXEN)
#define MDC_PIN      (UINT16_C(1) << 1U)
#define MDIO_PIN     (UINT16_C(1) << 2U)
#define AF_PUSH_PULL (LL_GPIO_MODE_ALTERNATE | LL_GPIO_SPEED_FREQ_HIGH)

enum { MAC_GATE_OWNED = 1U, CHECK_IDLE = 2U, LIVE = 4U };

/** \brief Admit only the isolated AHB station-management register interface. */
static bool mac_access(void) {
    return (RCC->AHBENR & MAC_GATES) == RCC_AHBENR_ETHMACEN &&
           (RCC->AHBRSTR & RCC_AHBRSTR_ETHMACRST) == 0U;
}

/** \brief Establish that neither the DMA nor MAC data path is running. */
static bool data_path_idle(void) {
    return (ETH->DMABMR & ETH_DMABMR_SR) == 0U &&
           (ETH->MACCR & (ETH_MACCR_TE | ETH_MACCR_RE)) == 0U &&
           (ETH->DMAOMR & (ETH_DMAOMR_ST | ETH_DMAOMR_SR)) == 0U;
}

nx_result_t nx_stm32f1_mdio_prepare(nx_stm32f1_mdio_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->effects != 0U || !nx_stm32f1_ethernet_clock_ready()) {
        return NX_ERROR_STATE;
    }
    if ((RCC->AHBENR & MAC_GATES) != 0U ||
        (RCC->AHBRSTR & RCC_AHBRSTR_ETHMACRST) != 0U) {
        return NX_ERROR_BUSY;
    }

    state->effects = MAC_GATE_OWNED;
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_ETHMACEN,
                                 RCC_AHBENR_ETHMACEN)) {
        return NX_ERROR_IO;
    }
    state->effects |= CHECK_IDLE;
    if (!data_path_idle() || (ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U) {
        return NX_ERROR_BUSY;
    }
    /* PC1 and PA2 are invariant across the MII/RMII and MAC remap settings. */
    if (nx_stm32f1_pads_capture(&state->mdc, GPIOC, MDC_PIN) != NX_SUCCESS ||
        nx_stm32f1_pads_apply(&state->mdc, AF_PUSH_PULL << 4U, 0U,
                              state->mdc.latch) != NX_SUCCESS ||
        nx_stm32f1_pads_capture(&state->mdio, GPIOA, MDIO_PIN) != NX_SUCCESS ||
        nx_stm32f1_pads_apply(&state->mdio, AF_PUSH_PULL << 8U, 0U,
                              state->mdio.latch) != NX_SUCCESS) {
        return NX_ERROR_IO;
    }
    state->effects |= LIVE;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_mdio_close(nx_stm32f1_mdio_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & CHECK_IDLE) != 0U) {
        if (!mac_access() || !data_path_idle() ||
            (ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U) {
            return NX_ERROR_BUSY;
        }
        state->effects &= (uint8_t)~CHECK_IDLE;
    }
    if (nx_stm32f1_pads_restore(&state->mdio) != NX_SUCCESS ||
        nx_stm32f1_pads_restore(&state->mdc) != NX_SUCCESS ||
        ((state->effects & MAC_GATE_OWNED) != 0U &&
         !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_ETHMACEN, 0U))) {
        return NX_ERROR_IO;
    }
    state->effects = 0U;
    return NX_SUCCESS;
}

/** \brief Transfer one station-management word using an absolute deadline. */
static nx_result_t transfer(nx_stm32f1_mdio_t* state, uint8_t phy, uint8_t reg,
                            uint16_t* value, bool write,
                            nx_time_us_t deadline) {
    if (state == NULL || value == NULL || phy > 31U || reg > 31U ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & LIVE) == 0U || !nx_stm32f1_ethernet_clock_ready() ||
        !mac_access()) {
        return NX_ERROR_STATE;
    }
    if (!write &&
        (!nx_io_dispatch_span(value, sizeof(*value), _Alignof(uint16_t)) ||
         nx_io_dispatch_overlap(value, sizeof(*value), state, sizeof(*state)) ||
         nx_io_dispatch_overlap(value, sizeof(*value),
                                (const void*)UINT32_C(0x40000000),
                                UINT32_C(0x20000000)))) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if ((ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (write) {
        NX_STM32F1_WRITE(&ETH->MACMIIDR, *value);
    }
    NX_STM32F1_WRITE(&ETH->MACMIIAR,
                     ((uint32_t)phy << ETH_MACMIIAR_PA_Pos) |
                         ((uint32_t)reg << ETH_MACMIIAR_MR_Pos) |
                         ETH_MACMIIAR_CR_DIV42 |
                         (write ? ETH_MACMIIAR_MW : 0U) | ETH_MACMIIAR_MB);
    do {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if (!mac_access()) {
            return NX_ERROR_STATE;
        }
    } while ((ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U);
    if (!nx_stm32f1_ethernet_clock_ready()) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (!write) {
        *value = (uint16_t)ETH->MACMIIDR;
    }
    return NX_SUCCESS;
}

/** \brief Receive one completed PHY word. */
static nx_result_t read_word(void* context, uint8_t phy, uint8_t reg,
                             uint16_t* value, nx_time_us_t deadline) {
    return transfer(context, phy, reg, value, false, deadline);
}

/** \brief Notify the PHY with one word and retain no caller storage. */
static nx_result_t write_word(void* context, uint8_t phy, uint8_t reg,
                              uint16_t value, nx_time_us_t deadline) {
    return transfer(context, phy, reg, &value, true, deadline);
}

const nx_mdio_ops_t nx_stm32f1_mdio_ops = {read_word, write_word};
