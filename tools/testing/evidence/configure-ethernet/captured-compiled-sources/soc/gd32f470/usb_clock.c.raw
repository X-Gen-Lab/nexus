/**
 * \file            usb_clock.c
 * \brief           Independent PLLSAIP 48 MHz with retained cleanup ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_usb_clock.h"
#include "gd32f4xx_rcu.h"
#ifndef NX_USB_CLOCK_WRITE
#define NX_USB_CLOCK_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_USB_CLOCK_POLL
#define NX_USB_CLOCK_POLL() ((void)0)
#endif
#define USB_PLL_MASK  (RCU_PLLSAI_PLLSAIN | RCU_PLLSAI_PLLSAIP)
#define USB_PLL_VALUE ((192U << 6U) | (1U << 16U))
#define USB_MUX_MASK  (RCU_ADDCTL_CK48MSEL | RCU_ADDCTL_PLL48MSEL)
enum { CLOCK_IDLE, CLOCK_CLEANUP, CLOCK_READY, CLOCK_POLL_LIMIT = 100000 };

/** \brief MMIO is cold privileged work, never a raw user or IRQ operation. */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}

/** \brief Enabled peers prohibit changing their CK48 or PLLSAIR source. */
static bool shared_busy(void) {
    return (RCU_AHB1EN & RCU_AHB1EN_USBHSEN) != 0U ||
           (RCU_AHB2EN & RCU_AHB2EN_TRNGEN) != 0U ||
           (RCU_APB2EN & (RCU_APB2EN_SDIOEN | RCU_APB2EN_TLIEN)) != 0U;
}

/** \brief Shared PLLPSC25 gives PLLSAI a 1 MHz crystal-derived input. */
static bool source_ready(void) {
    const uint32_t mask = RCU_PLL_PLLPSC | RCU_PLL_PLLN | RCU_PLL_PLLP |
                          RCU_PLL_PLLSEL | RCU_PLL_PLLQ;
    const uint32_t expected =
        25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
    const uint32_t ready =
        RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
    return (RCU_PLL & mask) == expected && (RCU_CTL & ready) == ready;
}

/** \brief Ready wins at the boundary; the poll bound also rejects a dead clock.
 */
static nx_result_t wait_pll(bool enabled, nx_time_us_t deadline) {
    const uint32_t mask = RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB;
    const uint32_t expected = enabled ? mask : 0U;
    for (unsigned poll = 0U; poll < CLOCK_POLL_LIMIT; ++poll) {
        NX_USB_CLOCK_POLL();
        if ((RCU_CTL & mask) == expected) {
            return NX_SUCCESS;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
    }
    return NX_ERROR_TIMEOUT;
}

/** \brief Validate actual mux, PLL divisors, readiness and peripheral access.
 */
static nx_result_t validate(const void* context) {
    const nx_gd32_usb_clock_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->phase != CLOCK_READY) {
        return NX_ERROR_STATE;
    }
    const uint32_t ready = RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB;
    return source_ready() && (RCU_CTL & ready) == ready &&
                   (RCU_PLLSAI & USB_PLL_MASK) == USB_PLL_VALUE &&
                   (RCU_ADDCTL & USB_MUX_MASK) == RCU_ADDCTL_PLL48MSEL &&
                   (RCU_AHB2EN & RCU_AHB2EN_USBFSEN) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief No consumer reference is created by failed actual clock validation.
 */
static nx_result_t retain(void* context) {
    nx_gd32_usb_clock_state_t* state = context;
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

/** \brief Release follows the controller's independent hardware detach proof.
 */
static nx_result_t release(void* context) {
    nx_gd32_usb_clock_state_t* state = context;
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

const nx_usb_clock_ops_t nx_gd32_usb_clock_ops = {retain, release, validate};

/** \brief No SYSCLK write; a separate PLLSAIP192/4 supplies CK48. */
nx_result_t nx_gd32_usb_clock_start(nx_gd32_usb_clock_state_t* state,
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
    if (state->phase != CLOCK_IDLE || state->consumers != 0U || shared_busy() ||
        (RCU_AHB2EN & RCU_AHB2EN_USBFSEN) != 0U ||
        (RCU_CTL & (RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB)) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!source_ready()) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->saved_pll = RCU_PLLSAI & USB_PLL_MASK;
    state->saved_mux = RCU_ADDCTL & USB_MUX_MASK;
    state->phase = CLOCK_CLEANUP;
    NX_USB_CLOCK_WRITE(RCU_PLLSAI,
                       (RCU_PLLSAI & ~USB_PLL_MASK) | USB_PLL_VALUE);
    nx_arch_dsb();
    if ((RCU_PLLSAI & USB_PLL_MASK) != USB_PLL_VALUE) {
        (void)nx_gd32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    NX_USB_CLOCK_WRITE(RCU_CTL, RCU_CTL | RCU_CTL_PLLSAIEN);
    nx_result_t result = wait_pll(true, deadline);
    if (result != NX_SUCCESS) {
        (void)nx_gd32_usb_clock_stop(state, deadline);
        return result;
    }
    NX_USB_CLOCK_WRITE(RCU_ADDCTL,
                       (RCU_ADDCTL & ~USB_MUX_MASK) | RCU_ADDCTL_PLL48MSEL);
    nx_arch_dsb();
    if ((RCU_ADDCTL & USB_MUX_MASK) != RCU_ADDCTL_PLL48MSEL) {
        (void)nx_gd32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    NX_USB_CLOCK_WRITE(RCU_AHB2EN, RCU_AHB2EN | RCU_AHB2EN_USBFSEN);
    nx_arch_dsb();
    if ((RCU_AHB2EN & RCU_AHB2EN_USBFSEN) == 0U) {
        (void)nx_gd32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    state->phase = CLOCK_READY;
    if (validate(state) != NX_SUCCESS) {
        (void)nx_gd32_usb_clock_stop(state, deadline);
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Restore only owned fields after proving every consumer detached. */
nx_result_t nx_gd32_usb_clock_stop(nx_gd32_usb_clock_state_t* state,
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
    uint32_t mux = RCU_ADDCTL & USB_MUX_MASK;
    if (shared_busy() ||
        ((RCU_PLLSAI & USB_PLL_MASK) != USB_PLL_VALUE &&
         (RCU_PLLSAI & USB_PLL_MASK) != state->saved_pll) ||
        (mux != RCU_ADDCTL_PLL48MSEL && mux != state->saved_mux)) {
        return NX_ERROR_BUSY;
    }
    state->phase = CLOCK_CLEANUP;
    NX_USB_CLOCK_WRITE(RCU_AHB2EN, RCU_AHB2EN & ~(uint32_t)RCU_AHB2EN_USBFSEN);
    nx_arch_dsb();
    if ((RCU_AHB2EN & RCU_AHB2EN_USBFSEN) != 0U) {
        return NX_ERROR_IO;
    }
    NX_USB_CLOCK_WRITE(RCU_ADDCTL,
                       (RCU_ADDCTL & ~USB_MUX_MASK) | state->saved_mux);
    nx_arch_dsb();
    if ((RCU_ADDCTL & USB_MUX_MASK) != state->saved_mux) {
        return NX_ERROR_IO;
    }
    NX_USB_CLOCK_WRITE(RCU_CTL, RCU_CTL & ~(uint32_t)RCU_CTL_PLLSAIEN);
    nx_result_t result = wait_pll(false, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    NX_USB_CLOCK_WRITE(RCU_PLLSAI,
                       (RCU_PLLSAI & ~USB_PLL_MASK) | state->saved_pll);
    nx_arch_dsb();
    if ((RCU_PLLSAI & USB_PLL_MASK) != state->saved_pll) {
        return NX_ERROR_IO;
    }
    state->phase = CLOCK_IDLE;
    return NX_SUCCESS;
}
