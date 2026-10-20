/**
 * \file            stm32u575_retained_time.c
 * \brief           Independent bounded retained RTC elapsed-time owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "retained_time.h"
#include "stm32u575_rtc_calendar.h"
#include <string.h>
#ifndef NX_U575_RTC
#define NX_U575_RTC RTC
#endif
#ifndef NX_U575_RETAINED_RCC
#define NX_U575_RETAINED_RCC RCC
#endif
#ifndef NX_U575_RETAINED_FLASH
#define NX_U575_RETAINED_FLASH FLASH
#endif
#ifndef NX_U575_RTC_IRQ_QUIET
#define NX_U575_RTC_IRQ_QUIET()                                                \
    (NVIC_GetEnableIRQ(RTC_IRQn) == 0U &&                                      \
     NVIC_GetPendingIRQ(RTC_IRQn) == 0U && NVIC_GetActive(RTC_IRQn) == 0U)
#endif
#define RETAINED_MAGIC       UINT32_C(0x55355254)
#define CALENDAR_CYCLE_TICKS (UINT64_C(36525) * 86400U * 256U)
_Static_assert(
    sizeof(nx_stm32u575_retained_time_owner_t) ==
        sizeof(nx_stm32u575_retained_time_storage_t),
    "one exact private retained-clock object must fit public storage");
_Static_assert(_Alignof(nx_stm32u575_retained_time_owner_t) ==
                   _Alignof(nx_stm32u575_retained_time_storage_t),
               "public storage must preserve private retained-clock alignment");
/** \brief Same serialized Thread may carry an incoming final-check PRIMASK. */
static bool context(bool unmasked) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U)
        return false;
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return masks.basepri == 0U && masks.faultmask == 0U &&
           (!unmasked || masks.primask == 0U);
}
/** \brief Caller object bytes must not name a known register block. */
static bool outside_mmio(const void* pointer, size_t bytes) {
    return !nx_time_overlap(pointer, bytes, NX_U575_RTC, sizeof(RTC_TypeDef)) &&
           !nx_time_overlap(pointer, bytes, NX_U575_RETAINED_RCC,
                            sizeof(RCC_TypeDef)) &&
           !nx_time_overlap(pointer, bytes, NX_U575_RETAINED_FLASH,
                            sizeof(FLASH_TypeDef));
}
/** \brief Validate the complete caller owner before private object access. */
static bool owner_span(const nx_stm32u575_retained_time_storage_t* storage) {
    return nx_time_span(storage, sizeof(*storage),
                        _Alignof(nx_stm32u575_retained_time_storage_t)) &&
           outside_mmio(storage, sizeof(*storage));
}
/** \brief Public outputs/configuration cannot alias owner or known MMIO. */
static bool external(const nx_stm32u575_retained_time_storage_t* storage,
                     const void* pointer, size_t bytes, size_t alignment) {
    return nx_time_span(pointer, bytes, alignment) &&
           !nx_time_overlap(pointer, bytes, storage, sizeof(*storage)) &&
           outside_mmio(pointer, bytes);
}
/** \brief Opaque storage is transferred as object bytes, avoiding type
 * aliasing. */
static nx_stm32u575_retained_time_owner_t
load(const nx_stm32u575_retained_time_storage_t* storage) {
    nx_stm32u575_retained_time_owner_t value;
    memcpy(&value, storage, sizeof(value));
    return value;
}
static void store(nx_stm32u575_retained_time_storage_t* storage,
                  const nx_stm32u575_retained_time_owner_t* value) {
    memcpy(storage, value, sizeof(*value));
}
/** \brief A closed initialized object retains generation and source facts. */
static bool known(const nx_stm32u575_retained_time_owner_t* value) {
    return value->magic == RETAINED_MAGIC && value->generation != 0U &&
           value->max_gap_ticks >= 256U &&
           value->max_gap_ticks <= 86400U * 256U;
}
/** \brief This owner writes no clock, backup, protection or interrupt field. */
static bool source(const nx_stm32u575_retained_time_owner_t* owner) {
    RCC_TypeDef* r = NX_U575_RETAINED_RCC;
    RTC_TypeDef* p = NX_U575_RTC;
    uint32_t bdcr = NX_U575_READ(&r->BDCR);
    if ((NX_U575_READ(&NX_U575_RETAINED_FLASH->OPTR) & FLASH_OPTR_TZEN) != 0U ||
        !nx_stm32u575_rtc_access(r) ||
        /* Revision X has ES0499 Rev12 2.2.3/16 low-drive limitations.
         * This crystal profile does not qualify a silicon revision. */
        (bdcr & RCC_BDCR_LSEDRV_1) == 0U ||
        (bdcr & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL |
                 RCC_BDCR_RTCEN | RCC_BDCR_BDRST | RCC_BDCR_LSEBYP)) !=
            (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 |
             RCC_BDCR_RTCEN))
        return false;
    uint32_t allowed = owner->wake_owner == NULL
                           ? 0U
                           : (RTC_CR_WUCKSEL | RTC_CR_WUTE | RTC_CR_WUTIE);
    uint32_t flags = owner->wake_owner == NULL ? 0U : RTC_SR_WUTF;
    return (NX_U575_READ(&p->CR) & ~allowed) == RTC_CR_BYPSHAD &&
           (NX_U575_READ(&p->SR) & ~flags) == 0U &&
           NX_U575_READ(&p->PRER) == 0x007f00ffU &&
           NX_U575_READ(&p->CALR) == 0U &&
           (NX_U575_READ(&p->ICSR) &
            (RTC_ICSR_INIT | RTC_ICSR_INITF | RTC_ICSR_BIN | RTC_ICSR_BCDU |
             RTC_ICSR_SHPF | RTC_ICSR_RECALPF)) == 0U &&
           (owner->wake_owner != NULL || NX_U575_RTC_IRQ_QUIET());
}
/** \brief Position inside a raw100-year cycle, never a UTC/century epoch. */
static uint64_t position(const nx_stm32u575_rtc_tuple_t* tuple) {
    const nx_rtc_calendar_t* c = &tuple->calendar;
    uint32_t days = (uint32_t)c->year * 365U + (c->year + 3U) / 4U;
    for (uint8_t month = 1U; month < c->month; ++month)
        days += nx_time_month_days(c->year, month);
    days += c->day - 1U;
    uint64_t seconds = ((uint64_t)days * 24U + c->hour) * 3600U +
                       (uint64_t)((uint32_t)c->minute * 60U) + c->second;
    return seconds * 256U + 255U - tuple->subsecond;
}
/** \brief Preserve exact quarter-microsecond carry without wide multiplication.
 */
static bool microseconds(uint64_t ticks, nx_time_us_t* value) {
    uint64_t seconds = ticks / 256U;
    uint64_t fraction = (ticks % 256U) * 1000000U / 256U;
    if (seconds > (UINT64_MAX - 1U - fraction) / 1000000U)
        return false;
    *value = seconds * 1000000U + fraction;
    return true;
}
nx_result_t nx_stm32u575_retained_time_initialize(
    nx_stm32u575_retained_time_storage_t* storage,
    const nx_stm32u575_retained_time_config_t* config, nx_clock_t* clock) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    if (!owner_span(storage) ||
        !external(storage, config, sizeof(*config),
                  _Alignof(nx_stm32u575_retained_time_config_t)) ||
        !external(storage, clock, sizeof(*clock), _Alignof(nx_clock_t)) ||
        nx_time_overlap(config, sizeof(*config), clock, sizeof(*clock)) ||
        config->resource != NX_U575_RETAINED_TIME_RTC ||
        config->max_gap_seconds == 0U || config->max_gap_seconds > 86400U ||
        (config->wake_owner != NULL &&
         !external(storage, config->wake_owner, 1U, 1U)))
        return NX_ERROR_INVALID;
    nx_stm32u575_retained_time_owner_t old = load(storage);
    if (known(&old)) {
        if (old.opened)
            return NX_ERROR_STATE;
        if (old.generation == UINT64_MAX)
            return NX_ERROR_EXHAUSTED;
    } else {
        for (size_t n = 0U; n < sizeof(*storage); ++n)
            if (storage->private_bytes[n] != 0U)
                return NX_ERROR_STATE;
    }
    if ((NX_U575_READ(&NX_U575_RETAINED_FLASH->OPTR) & FLASH_OPTR_TZEN) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (!nx_stm32u575_rtc_access(NX_U575_RETAINED_RCC))
        return NX_ERROR_STATE;
    nx_stm32u575_retained_time_owner_t fresh;
    memset(&fresh, 0, sizeof(fresh));
    fresh.wake_owner = config->wake_owner;
    fresh.max_gap_ticks = config->max_gap_seconds * 256U;
    if ((NX_U575_READ(&NX_U575_RTC->CR) &
         (RTC_CR_ALRAE | RTC_CR_ALRAIE | RTC_CR_ALRBE | RTC_CR_ALRBIE |
          RTC_CR_WUTE | RTC_CR_WUTIE)) != 0U ||
        NX_U575_READ(&NX_U575_RTC->SR) != 0U || !NX_U575_RTC_IRQ_QUIET())
        return NX_ERROR_BUSY;
    if (!source(&fresh))
        return NX_ERROR_STATE;
    nx_stm32u575_rtc_tuple_t tuple;
    nx_result_t result =
        nx_stm32u575_rtc_tuple_read(NX_U575_RTC, NX_U575_RETAINED_RCC, &tuple);
    if (result != NX_SUCCESS)
        return result;
    fresh.previous = position(&tuple);
    fresh.generation = known(&old) ? old.generation + 1U : 1U;
    fresh.magic = RETAINED_MAGIC;
    fresh.opened = true;
    store(storage, &fresh);
    *clock = (nx_clock_t){nx_stm32u575_retained_time_now, storage};
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_retained_time_sample(
    nx_stm32u575_retained_time_storage_t* storage,
    nx_stm32u575_retained_time_snapshot_t* snapshot) {
    if (!context(false))
        return NX_ERROR_CONTEXT;
    if (!owner_span(storage) ||
        !external(storage, snapshot, sizeof(*snapshot),
                  _Alignof(nx_stm32u575_retained_time_snapshot_t)))
        return NX_ERROR_INVALID;
    nx_stm32u575_retained_time_owner_t owner = load(storage);
    if (!known(&owner) || !owner.opened || owner.tainted)
        return NX_ERROR_STATE;
    nx_result_t result = NX_ERROR_STATE;
    nx_stm32u575_rtc_tuple_t tuple;
    if (!source(&owner))
        goto failed;
    result =
        nx_stm32u575_rtc_tuple_read(NX_U575_RTC, NX_U575_RETAINED_RCC, &tuple);
    if (result == NX_ERROR_BUSY)
        return result;
    if (result != NX_SUCCESS)
        goto failed;
    uint64_t current = position(&tuple);
    uint64_t delta = current >= owner.previous
                         ? current - owner.previous
                         : CALENDAR_CYCLE_TICKS - owner.previous + current;
    if (delta > owner.max_gap_ticks) {
        result = NX_ERROR_STATE;
        goto failed;
    }
    nx_time_us_t now;
    if (UINT64_MAX - owner.ticks < delta ||
        !microseconds(owner.ticks + delta, &now)) {
        result = NX_ERROR_EXHAUSTED;
        goto failed;
    }
    owner.ticks += delta;
    owner.previous = current;
    store(storage, &owner);
    *snapshot = (nx_stm32u575_retained_time_snapshot_t){
        storage, owner.generation, now, owner.ticks};
    return NX_SUCCESS;
failed:
    owner.tainted = true;
    store(storage, &owner);
    return result;
}
nx_result_t nx_stm32u575_retained_time_validate_wake(
    nx_stm32u575_retained_time_storage_t* storage, const void* wake_owner,
    uint64_t generation) {
    if (!context(false))
        return NX_ERROR_CONTEXT;
    if (!owner_span(storage) || !external(storage, wake_owner, 1U, 1U) ||
        generation == 0U)
        return NX_ERROR_INVALID;
    nx_stm32u575_retained_time_owner_t owner = load(storage);
    if (!known(&owner) || !owner.opened || owner.tainted ||
        owner.wake_owner != wake_owner || owner.generation != generation)
        return NX_ERROR_STATE;
    if (!source(&owner)) {
        owner.tainted = true;
        store(storage, &owner);
        return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}
nx_time_us_t nx_stm32u575_retained_time_now(void* context_raw) {
    nx_stm32u575_retained_time_storage_t* storage = context_raw;
    nx_stm32u575_retained_time_snapshot_t snapshot;
    nx_result_t result = nx_stm32u575_retained_time_sample(storage, &snapshot);
    if (result == NX_SUCCESS)
        return snapshot.now;
    if (owner_span(storage)) {
        nx_stm32u575_retained_time_owner_t owner = load(storage);
        if (known(&owner)) {
            owner.tainted = true;
            store(storage, &owner);
        }
    }
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, result, __FILE__, __LINE__);
    for (;;) {
    }
}
nx_result_t
nx_stm32u575_retained_time_stop(nx_stm32u575_retained_time_storage_t* storage) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    if (!owner_span(storage))
        return NX_ERROR_INVALID;
    nx_stm32u575_retained_time_owner_t owner = load(storage);
    if (!known(&owner))
        return NX_ERROR_STATE;
    owner.opened = false;
    store(storage, &owner);
    return NX_SUCCESS;
}
