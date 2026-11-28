/**
 * \file            counter.c
 * \brief           F103xB retained RTC counter and read-only backup words.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/rtc/instance.h"
#include "nexus/core/time.h"
#include "nexus/io/rtc_aux.h"
#include "private/hardware.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "Retained RTC requires the reviewed STM32F103xB register layout"
#endif

#ifndef NX_STM32F1_RTC_READ
#define NX_STM32F1_RTC_READ(reg) (*(reg))
#endif

#define GATES  (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN)
#define RESETS (RCC_APB1RSTR_PWRRST | RCC_APB1RSTR_BKPRST)
#define RTC_CLOCK                                                              \
    (RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE | RCC_BDCR_LSEON | RCC_BDCR_LSERDY)
#define RTC_FLAGS  (RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF)
#define BKP_WORDS  10U
#define SYNC_US    UINT32_C(10000)
#define SYNC_POLLS UINT32_C(16384)

enum { OWN_GATES = 1U, OWN_DBP = 2U, LIVE = 4U };

_Static_assert(offsetof(BKP_TypeDef, DR10) - offsetof(BKP_TypeDef, DR1) ==
                   (BKP_WORDS - 1U) * sizeof(uint32_t),
               "RM0008 F103xB backup words are contiguous APB registers");

/** \brief Reject known MMIO aliases on hardware and in the register model. */
static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(pointer, bytes, PWR, sizeof(*PWR)) &&
           !nx_io_dispatch_overlap(pointer, bytes, RTC, sizeof(*RTC)) &&
           !nx_io_dispatch_overlap(pointer, bytes, BKP, sizeof(*BKP)) &&
           !nx_io_dispatch_overlap(pointer, bytes, (void*)PERIPH_BASE,
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(pointer, bytes, (void*)UINT32_C(0xE0000000),
                                   UINT32_C(0x10000000));
}

static bool retained_clock(void) {
    const uint32_t mask = RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL | RCC_BDCR_LSEBYP |
                          RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_BDRST;
    return (RCC->BDCR & mask) == RTC_CLOCK;
}

static bool interfaces_live(void) {
    return (RCC->APB1ENR & GATES) == GATES && (RCC->APB1RSTR & RESETS) == 0U;
}

/** \brief Change DBP alone. PWR_CR includes write-one status clears. */
static bool write_access(bool enable) {
    const uint32_t protected_bits = PWR_CR_DBP | PWR_CR_CWUF | PWR_CR_CSBF;
    const uint32_t selected = enable ? PWR_CR_DBP : 0U;
    NX_STM32F1_WRITE(&PWR->CR, (PWR->CR & ~protected_bits) | selected);
    nx_arch_dsb();
    return (PWR->CR & PWR_CR_DBP) == selected;
}

nx_result_t nx_stm32f1_rtc_close(nx_stm32f1_rtc_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(state, sizeof(*state), _Alignof(nx_stm32f1_rtc_t)))
        return NX_ERROR_INVALID;

    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & OWN_DBP) != 0U) {
        if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) == 0U ||
            (RCC->APB1RSTR & RCC_APB1RSTR_PWRRST) != 0U || !write_access(false))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~OWN_DBP;
    }
    if ((state->effects & OWN_GATES) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, GATES, 0U))
        return NX_ERROR_IO;
    state->effects = 0U;
    return NX_SUCCESS;
}

static nx_result_t unwind(nx_stm32f1_rtc_t* state, nx_result_t result) {
    return nx_stm32f1_rtc_close(state) == NX_SUCCESS ? result : NX_ERROR_IO;
}

static nx_result_t wait_control(uint32_t flag, nx_time_us_t until) {
    for (uint32_t poll = 0U; poll < SYNC_POLLS; ++poll) {
        if (!interfaces_live() || !retained_clock() ||
            !nx_stm32f1_system_live())
            return NX_ERROR_IO;
        if (nx_deadline_expired(until, nx_time_now_us()))
            break;
        if ((NX_STM32F1_RTC_READ(&RTC->CRL) & flag) != 0U)
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}

nx_result_t nx_stm32f1_rtc_prepare(nx_stm32f1_rtc_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(state, sizeof(*state), _Alignof(nx_stm32f1_rtc_t)))
        return NX_ERROR_INVALID;
    if (state->effects != 0U || (RCC->APB1ENR & GATES) != 0U ||
        (RCC->APB1RSTR & RESETS) != 0U)
        return NX_ERROR_BUSY;
    if (!retained_clock() || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;

    state->effects = OWN_GATES;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, GATES, GATES))
        return unwind(state, NX_ERROR_IO);
    if ((PWR->CR & (PWR_CR_DBP | PWR_CR_PVDE)) != 0U)
        return unwind(state, NX_ERROR_BUSY);
    const nx_time_us_t until = nx_deadline_after(nx_time_now_us(), SYNC_US);
    nx_result_t result = wait_control(RTC_CRL_RTOFF, until);
    if (result != NX_SUCCESS)
        return unwind(state, result);
    if ((NX_STM32F1_RTC_READ(&RTC->CRL) & RTC_CRL_CNF) != 0U)
        return unwind(state, NX_ERROR_BUSY);

    if (!write_access(true))
        return unwind(state, NX_ERROR_IO);
    state->effects |= OWN_DBP;
    /* RM0008 §18.4.2: clear stale synchronization, preserve W0C events. */
    NX_STM32F1_WRITE(&RTC->CRL, RTC_FLAGS);
    nx_arch_dsb();
    if (!write_access(false))
        return unwind(state, NX_ERROR_IO);
    state->effects &= (uint8_t)~OWN_DBP;

    result = wait_control(RTC_CRL_RSF, until);
    if (result != NX_SUCCESS)
        return unwind(state, result);
    state->effects |= LIVE;
    return NX_SUCCESS;
}

static bool rtc_ready(void) {
    return interfaces_live() && retained_clock() &&
           (NX_STM32F1_RTC_READ(&RTC->CRL) &
            (RTC_CRL_RSF | RTC_CRL_RTOFF | RTC_CRL_CNF)) ==
               (RTC_CRL_RSF | RTC_CRL_RTOFF);
}

static nx_result_t read_admit(const nx_stm32f1_rtc_t* state, uint32_t* out) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(state, sizeof(*state), _Alignof(nx_stm32f1_rtc_t)) ||
        !storage(out, sizeof(*out), _Alignof(uint32_t)) ||
        nx_io_dispatch_overlap(out, sizeof(*out), state, sizeof(*state)))
        return NX_ERROR_INVALID;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    return rtc_ready() ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_rtc_ticks(const nx_stm32f1_rtc_t* state, uint32_t* out) {
    const nx_result_t result = read_admit(state, out);
    if (result != NX_SUCCESS)
        return result;
    /* Re-read the high half across a carry; a 32-bit wrap is also a carry. */
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        const uint32_t high = NX_STM32F1_RTC_READ(&RTC->CNTH) & UINT16_MAX;
        const uint32_t low = NX_STM32F1_RTC_READ(&RTC->CNTL) & UINT16_MAX;
        if (high == (NX_STM32F1_RTC_READ(&RTC->CNTH) & UINT16_MAX)) {
            if (!rtc_ready())
                return NX_ERROR_IO;
            *out = (high << 16U) | low;
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

static nx_result_t backup_read(void* context, uint8_t index, uint32_t* out) {
    if (index >= BKP_WORDS)
        return NX_ERROR_INVALID;
    const nx_result_t result = read_admit(context, out);
    if (result != NX_SUCCESS)
        return result;
    const uintptr_t address =
        (uintptr_t)BKP + offsetof(BKP_TypeDef, DR1) + index * sizeof(uint32_t);
    const uint32_t value =
        NX_STM32F1_RTC_READ((volatile const uint32_t*)address) & UINT16_MAX;
    if (!rtc_ready())
        return NX_ERROR_IO;
    *out = value;
    return NX_SUCCESS;
}

static nx_result_t backup_stop(void* context) {
    return nx_stm32f1_rtc_close(context);
}

static const nx_rtc_aux_ops_t backup_ops = {.backup_read = backup_read};
const nx_rtc_ops_t nx_stm32f1_rtc_backup_ops = {.stop = backup_stop,
                                                .auxiliary = &backup_ops};
