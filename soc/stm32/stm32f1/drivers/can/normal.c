/**
 * \file            normal.c
 * \brief           NUCLEO-F103RB CAN pin and SN65HVD230DR standby sequencer.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/can/normal.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "system/platform.h"

#ifndef NX_STM32F1_CAN_RS_WRITE
#define NX_STM32F1_CAN_RS_WRITE(reg, value) ((reg)->BSRR = (value))
#endif

#define CAN_PAD_RS (1U << 2U)
#define CAN_PAD_RX (1U << 11U)
#define CAN_PAD_TX (1U << 12U)
#define CAN_CLOCK_FIELDS                                                       \
    (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PLLSRC |         \
     RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL)
#define CAN_RUN_CLOCK                                                          \
    (RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLSRC |                \
     RCC_CFGR_PLLMULL9)
#define CAN_RUN_OSC                                                            \
    (RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP | RCC_CR_PLLON |             \
     RCC_CR_PLLRDY)

enum { CAN_PAD_POLLS = 4096U, CAN_WAKE_US = 3U };

/** \brief Observe the physical standby control, bounded even if time stalls. */
static bool standby(bool enabled) {
    const uint32_t expected = enabled ? CAN_PAD_RS : 0U;
    NX_STM32F1_CAN_RS_WRITE(GPIOC, enabled ? CAN_PAD_RS : CAN_PAD_RS << 16U);
    nx_arch_dsb();
    for (unsigned poll = 0U; poll < CAN_PAD_POLLS; ++poll) {
        if ((GPIOC->ODR & CAN_PAD_RS) == expected &&
            (GPIOC->IDR & CAN_PAD_RS) == expected)
            return true;
    }
    return false;
}

static bool phy_ready(void) {
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), CAN_WAKE_US, &deadline) !=
        NX_SUCCESS)
        return false;
    for (unsigned poll = 0U; poll < CAN_PAD_POLLS; ++poll) {
        const nx_time_us_t current = nx_time_now_us();
        if (current == NX_DEADLINE_NEVER)
            return false;
        if (nx_deadline_expired(deadline, current))
            return (GPIOC->IDR & CAN_PAD_RS) == 0U;
    }
    return false;
}

/** \brief Observe AFIO's readable CAN remap bits without writing MAPR. */
static nx_result_t check_route(nx_stm32f1_can_normal_t* owner) {
    const uint32_t gate = RCC_APB2ENR_AFIOEN;
    if ((RCC->APB2RSTR & RCC_APB2RSTR_AFIORST) != 0U)
        return NX_ERROR_BUSY;
    owner->afio_gate_added = (RCC->APB2ENR & gate) == 0U;
    if (owner->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate))
        return NX_ERROR_IO;
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_CAN_REMAP) != 0U;
    if (owner->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, 0U))
        return NX_ERROR_IO;
    owner->afio_gate_added = false;
    return remapped ? NX_ERROR_BUSY : NX_SUCCESS;
}

static nx_result_t prepare_pads(nx_stm32f1_can_normal_t* owner) {
    nx_result_t result =
        nx_stm32f1_pads_capture(&owner->rs_pad, GPIOC, CAN_PAD_RS);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t rs_shift = 8U;
    if (((owner->rs_pad.low >> rs_shift) & 0xFU) != GPIO_CRL_CNF0_0 ||
        (GPIOC->IDR & CAN_PAD_RS) == 0U) {
        owner->rs_pad.captured = false;
        return NX_ERROR_IO;
    }

    result = nx_stm32f1_pads_capture(&owner->can_pads, GPIOA,
                                     CAN_PAD_RX | CAN_PAD_TX);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t rx_shift = 12U;
    const uint32_t tx_shift = 16U;
    const uint32_t floating = GPIO_CRL_CNF0_0;
    const uint32_t can_fields = (0xFU << rx_shift) | (0xFU << tx_shift);
    if ((owner->can_pads.high & can_fields) !=
        ((floating << rx_shift) | (floating << tx_shift))) {
        owner->can_pads.captured = false;
        owner->rs_pad.captured = false;
        return NX_ERROR_BUSY;
    }

    const uint32_t rs_mode = LL_GPIO_OUTPUT_OPENDRAIN | LL_GPIO_SPEED_FREQ_LOW;
    owner->rs_configured = true;
    result = nx_stm32f1_pads_apply(&owner->rs_pad, rs_mode << rs_shift, 0U,
                                   CAN_PAD_RS);
    if (result != NX_SUCCESS || !standby(true))
        return NX_ERROR_IO;
    const uint32_t tx_mode = GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_MEDIUM;
    return nx_stm32f1_pads_apply(&owner->can_pads, 0U,
                                 (floating << rx_shift) | (tx_mode << tx_shift),
                                 CAN_PAD_TX);
}

nx_result_t nx_stm32f1_can_normal_admit(const nx_stm32f1_can_normal_t* owner) {
    if ((uintptr_t)owner % _Alignof(nx_stm32f1_can_normal_t) != 0U ||
        owner == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (owner->can_pads.registers != NULL || owner->rs_pad.registers != NULL ||
        owner->afio_gate_added || owner->rs_configured ||
        (RCC->APB2ENR & RCC_APB2ENR_USART1EN) != 0U)
        return NX_ERROR_BUSY;
    return nx_stm32f1_can_admit(&owner->port);
}

nx_result_t nx_stm32f1_can_normal_prepare(nx_stm32f1_can_normal_t* owner) {
    nx_result_t result = nx_stm32f1_can_normal_admit(owner);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_system_live() ||
        (RCC->CFGR & CAN_CLOCK_FIELDS) != CAN_RUN_CLOCK ||
        (RCC->CR & CAN_RUN_OSC) != CAN_RUN_OSC)
        return NX_ERROR_STATE;
    result = check_route(owner);
    if (result != NX_SUCCESS)
        return result;
    result = prepare_pads(owner);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32f1_can_prepare(&owner->port);
    if (result != NX_SUCCESS)
        return result;
    if (!standby(false) || !phy_ready()) {
        nx_stm32f1_can_block(&owner->port);
        return standby(true) ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_can_normal_close(nx_stm32f1_can_normal_t* owner) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((uintptr_t)owner % _Alignof(nx_stm32f1_can_normal_t) != 0U ||
        owner == NULL)
        return NX_ERROR_INVALID;
    nx_stm32f1_can_block(&owner->port);
    if (owner->rs_configured && !standby(true))
        return NX_ERROR_IO;
    nx_result_t result = nx_stm32f1_can_close(&owner->port);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32f1_pads_restore(&owner->can_pads);
    if (result != NX_SUCCESS)
        return result;
    owner->rs_configured = false;
    result = nx_stm32f1_pads_restore(&owner->rs_pad);
    if (result != NX_SUCCESS)
        return result;
    if (owner->afio_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U))
        return NX_ERROR_IO;
    owner->afio_gate_added = false;
    return NX_SUCCESS;
}
