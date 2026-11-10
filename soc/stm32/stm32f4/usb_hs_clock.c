/**
 * \file            usb_hs_clock.c
 * \brief           HS internal-FS and external ULPI static gate ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_usb_hs_clock.h"

#ifndef NX_USB_CLOCK_WRITE
#define NX_USB_CLOCK_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
enum { IDLE, CLEANUP, READY };

/** \brief Clock control requires the same explicit cold task executor. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Internal FS needs the selected exact PLL48 tuple, never reclocking.
 */
static bool source_ready(bool ulpi) {
    if (NEXUS_CORE_HZ <= 30000000U ||
        !nx_stm32_clock_profile_matches(RCC, NULL))
        return false;
    if (ulpi) {
        /* External 60 MHz has been handed over by the physical PHY owner. */
        return true;
    }
    const uint32_t mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |
                          RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLSRC |
                          RCC_PLLCFGR_PLLQ;
    const uint32_t ready = RCC_CR_PLLON | RCC_CR_PLLRDY |
#if NEXUS_CLOCK_SOURCE_HSE
                           RCC_CR_HSEON | RCC_CR_HSERDY;
#else
                           RCC_CR_HSION | RCC_CR_HSIRDY;
#endif
    return NEXUS_CLOCK_SOURCE_HSE != 0U && NEXUS_PLL48_HZ == 48000000U &&
           (RCC->PLLCFGR & mask) == NEXUS_CLOCK_PLLCFGR &&
           (RCC->CR & ready) == ready;
}

/** \brief One owner remembers its actual PHY until complete gate withdrawal. */
static uint32_t gates(const nx_stm32_usb_hs_clock_state_t* state) {
    return RCC_AHB1ENR_OTGHSEN | (state->ulpi ? RCC_AHB1ENR_OTGHSULPIEN : 0U);
}

/** \brief Readonly validation cannot turn failed acquisition into readiness. */
static nx_result_t validate(const void* context) {
    const nx_stm32_usb_hs_clock_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->phase != READY) {
        return NX_ERROR_STATE;
    }
    return source_ready(state->ulpi) &&
                   (RCC->AHB1ENR & gates(state)) == gates(state)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Retain only a validated clock, with no implicit global registry. */
static nx_result_t retain(void* context) {
    nx_stm32_usb_hs_clock_state_t* state = context;
    nx_result_t status = validate(state);
    if (status != NX_SUCCESS) {
        return status;
    }
    if (state->consumers == UINT32_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    ++state->consumers;
    return NX_SUCCESS;
}

/** \brief The transport releases only after its own IRQ and request drain. */
static nx_result_t release(void* context) {
    nx_stm32_usb_hs_clock_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->consumers == 0U) {
        return NX_ERROR_STATE;
    }
    --state->consumers;
    return NX_SUCCESS;
}

/** \brief Acquire only the exact selected gate set after complete admission. */
nx_result_t nx_stm32_usb_hs_clock_start(nx_stm32_usb_hs_clock_state_t* state,
                                        bool ulpi, uint32_t ulpi_clock_hz,
                                        nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER ||
        ulpi_clock_hz != (ulpi ? 60000000U : 0U)) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->phase != IDLE || state->consumers != 0U ||
        (RCC->AHB1ENR & (RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN)) !=
            0U) {
        return NX_ERROR_BUSY;
    }
    if (!source_ready(ulpi)) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->ulpi = ulpi;
    state->phase = CLEANUP;
    NX_USB_CLOCK_WRITE(RCC->AHB1ENR, RCC->AHB1ENR | gates(state));
    nx_arch_dsb();
    if ((RCC->AHB1ENR & gates(state)) != gates(state) || !source_ready(ulpi)) {
        return NX_ERROR_IO;
    }
    state->phase = READY;
    return NX_SUCCESS;
}
/** \brief A refused clear retains the original exact cleanup identity. */
nx_result_t nx_stm32_usb_hs_clock_stop(nx_stm32_usb_hs_clock_state_t* state,
                                       nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->phase == IDLE) {
        return NX_SUCCESS;
    }
    if (state->consumers != 0U) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->phase = CLEANUP;
    NX_USB_CLOCK_WRITE(RCC->AHB1ENR, RCC->AHB1ENR & ~gates(state));
    nx_arch_dsb();
    if ((RCC->AHB1ENR & gates(state)) != 0U) {
        return NX_ERROR_IO;
    }
    state->phase = IDLE;
    return NX_SUCCESS;
}
const nx_usb_clock_ops_t nx_stm32_usb_hs_clock_ops = {retain, release,
                                                      validate};
