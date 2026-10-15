/**
 * \file            usb_clock.c
 * \brief           Fixed PLL48 validation and explicit USBFS gate ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_usb_clock.h"
#include "stm32f407xx.h"
#ifndef NX_USB_CLOCK_WRITE
#define NX_USB_CLOCK_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
enum { CLOCK_IDLE, CLOCK_CLEANUP, CLOCK_READY };

/** \brief Do not access privileged clock registers from a task gateway bypass.
 */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}

/** \brief Exact established profile avoids truncation-based frequency claims.
 */
static bool source_ready(void) {
    const uint32_t mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |
                          RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLSRC |
                          RCC_PLLCFGR_PLLQ;
    const uint32_t expected =
        8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
    const uint32_t ready =
        RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    return (RCC->PLLCFGR & mask) == expected && (RCC->CR & ready) == ready;
}

/** \brief A retained failed acquisition cannot expose USB registers as ready.
 */
static nx_result_t validate(const void* context) {
    const nx_stm32_usb_clock_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->phase != CLOCK_READY) {
        return NX_ERROR_STATE;
    }
    return source_ready() && (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Increment only after actual source and gate validation. */
static nx_result_t retain(void* context) {
    nx_stm32_usb_clock_state_t* state = context;
    nx_result_t result = validate(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (state->consumers == UINT32_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    ++state->consumers;
    return NX_SUCCESS;
}

/** \brief Caller proves USB references detached; the lease is not that proof.
 */
static nx_result_t release(void* context) {
    nx_stm32_usb_clock_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->consumers == 0U) {
        return NX_ERROR_STATE;
    }
    --state->consumers;
    return NX_SUCCESS;
}

const nx_usb_clock_ops_t nx_stm32_usb_clock_ops = {retain, release, validate};

/** \brief Establish only the independent USBFS gate after profile validation.
 */
nx_result_t nx_stm32_usb_clock_start(nx_stm32_usb_clock_state_t* state,
                                     bool exclusive, nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!exclusive) {
        return NX_ERROR_PERMISSION;
    }
    if (state->phase != CLOCK_IDLE || state->consumers != 0U ||
        (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!source_ready()) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->phase = CLOCK_CLEANUP;
    NX_USB_CLOCK_WRITE(RCC->AHB2ENR, RCC->AHB2ENR | RCC_AHB2ENR_OTGFSEN);
    nx_arch_dsb();
    if ((RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) == 0U) {
        (void)nx_stm32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    state->phase = CLOCK_READY;
    if (validate(state) != NX_SUCCESS) {
        (void)nx_stm32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Failed gate readback keeps the exact owner control available. */
nx_result_t nx_stm32_usb_clock_stop(nx_stm32_usb_clock_state_t* state,
                                    nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->consumers != 0U) {
        return NX_ERROR_BUSY;
    }
    if (state->phase == CLOCK_IDLE) {
        return NX_SUCCESS;
    }
    state->phase = CLOCK_CLEANUP;
    NX_USB_CLOCK_WRITE(RCC->AHB2ENR,
                       RCC->AHB2ENR & ~(uint32_t)RCC_AHB2ENR_OTGFSEN);
    nx_arch_dsb();
    if ((RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) != 0U) {
        return NX_ERROR_IO;
    }
    state->phase = CLOCK_IDLE;
    return NX_SUCCESS;
}
