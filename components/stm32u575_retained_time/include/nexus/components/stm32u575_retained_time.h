/**
 * \file            stm32u575_retained_time.h
 * \brief           Independent monotonic domain from a retained U575 calendar
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32U575_RETAINED_TIME_H
#define NEXUS_COMPONENTS_STM32U575_RETAINED_TIME_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
#if UINTPTR_MAX == UINT32_MAX
#define NX_U575_RETAINED_TIME_STORAGE_BYTES 40U
#elif UINTPTR_MAX == UINT64_MAX
#define NX_U575_RETAINED_TIME_STORAGE_BYTES 48U
#else
#error "U575 retained clock maintains ARM32 and host64 software models"
#endif
/**
 * \brief One zero-initialized caller owner, not a pool or allocation service.
 * \note Do not copy or mutate. Keep this storage alive through every copied
 *       clock face, deadline consumer and observer. Memory retention for any
 *       future power mode is an assembly obligation, never implied by a RAM
 *       address. This read-only clock does not require SRAM4 or enter Stop2.
 */
typedef union {
    uint64_t alignment;
    uintptr_t pointer_alignment;
    unsigned char private_bytes[NX_U575_RETAINED_TIME_STORAGE_BYTES];
} nx_stm32u575_retained_time_storage_t;
/** \brief Exclusive exact RTC source; no second owner may configure it. */
typedef enum { NX_U575_RETAINED_TIME_RTC = 1 } nx_u575_retained_time_resource_t;
/**
 * \brief Bounded observation gap, declared by the serialized lifecycle owner.
 * \note Observe at least once per max_gap_seconds, in 1..86400 seconds. Missed
 *       complete 100-year periods and small external clock settings cannot be
 *       detected from calendar snapshots. Exclude every calendar writer and
 *       other RTC configuration owner, including the polling AlarmA factory
 *       selection. The sole explicit WUT delegate is described below. The
 *       complete elapsed interval, including wake latency or recovery, must
 *       fit this gap; the nominal WUT duration alone is not a bound.
 */
typedef struct {
    nx_u575_retained_time_resource_t resource;
    uint32_t max_gap_seconds;
    /** \brief NULL for RTC-only; otherwise stable identity of one WUT owner.
     * \note Delegates only WUCKSEL/WUTE/WUTIE, WUTR, WUTF/SCR.CWUTF and RTC
     *       IRQ ownership; ICSR.WUTWF may change with that bounded operation.
     *       That same serialized owner preserves every calendar/source field.
     *       Identity is compared without dereference; it is not authorization.
     */
    const void* wake_owner;
} nx_stm32u575_retained_time_config_t;
/** \brief A successful sample copied together in this owner's boot lifetime.
 * \note Microseconds are floor(ticks*1000000/256); resolution is 3906.25us,
 *       with exact carried fractional quarters. Generation changes only on
 *       explicit successful initialize/reopen, never natural 99->00 rollover.
 */
typedef struct {
    const void* identity;
    uint64_t generation;
    nx_time_us_t now;
    uint64_t ticks;
} nx_stm32u575_retained_time_snapshot_t;
/**
 * \brief Open a separate elapsed clock without reading or changing Core TIM2.
 * \param[in,out] storage: Fresh zero or fully stopped, externally joined owner.
 * \param[in] config: Separate immutable configuration, copied during this call.
 * \param[out] clock: Separate writable face, copied only on success.
 * \return SUCCESS, INVALID, STATE, BUSY, CONTEXT, UNSUPPORTED or EXHAUSTED;
 *         rejection leaves owner and output unchanged, with no hardware writes.
 * \note Exact U575 TZEN0, privileged unmasked Thread, baremetal integration.
 *       External boot owns a valid raw calendar/LSE32768/RTCEN/RTCAPBEN,
 *       crystal mode LSEBYP=0 and conservative drive 10/11. ES0499 Rev12
 *       2.2.3/16 affect revision X; this profile does not qualify revision.
 *       External boot/Board owners exclude PC13 transitions on the LQFP
 *       reference per 2.2.1; reads cannot prove electrical silence.
 * \note The selected MSIS4/equal-bus profile is checked before every tuple
 *       pair, with no concurrent clock-profile writer. This conservative
 *       profile is not a general RTC-write frequency rule. Require PRER127/255,
 *       BCD24h BYPSHAD, no shift/calibration/alarms.
 * \note Both standalone and delegated construction require quiet RTC IRQ,
 *       WUTE/WUTIE clear and no RTC flags. The explicit wake_owner may operate
 *       only its WUT/IRQ fields after successful construction. Calendar
 *       provenance, including intentional YY00, cannot be inferred from INITS.
 *       DBP/WPR need not be unlocked; no hardware writes occur.
 * \note This face has a distinct domain from nx_time_now_us(). Old Core
 *       deadlines are never translated. Stop/reopen invalidates old faces'
 *       numerical deadlines after all consumers and observers have joined.
 */
nx_result_t nx_stm32u575_retained_time_initialize(
    nx_stm32u575_retained_time_storage_t* storage,
    const nx_stm32u575_retained_time_config_t* config, nx_clock_t* clock);
/**
 * \brief Observe a bounded coherent tuple and extend the independent domain.
 * \param[in,out] storage: Same serialized privileged Thread owner.
 * \param[out] snapshot: Separate aligned writable output, unchanged on failure.
 * \return SUCCESS, INVALID, STATE, BUSY, CONTEXT or EXHAUSTED.
 * \note Thread may hold PRIMASK; BASEPRI/FAULTMASK and Handler mode are
 *       excluded. BUSY means no coherent tuple after eight bounded retries,
 *       and changes neither owner nor output. A source/configuration change,
 *       backward/out-of-window calendar or time exhaustion permanently taints
 *       this live epoch. Later samples return STATE; no stale value is valid.
 */
nx_result_t nx_stm32u575_retained_time_sample(
    nx_stm32u575_retained_time_storage_t* storage,
    nx_stm32u575_retained_time_snapshot_t* snapshot);
/**
 * \brief Check one explicit WUT identity against this live clock generation.
 * \return SUCCESS, INVALID, STATE or CONTEXT; no output or hardware effects.
 * \note Same serialized privileged Thread, PRIMASK0/1 allowed. The WUT owner
 *       obtains generation through sample and checks it before and after each
 *       transition. Reopen invalidates old bindings; no second owner is
 * inferred.
 */
nx_result_t nx_stm32u575_retained_time_validate_wake(
    nx_stm32u575_retained_time_storage_t* storage, const void* wake_owner,
    uint64_t generation);
/**
 * \brief Clock-face read; failure cannot return a stale or invalid timestamp.
 * \param[in,out] context: Exact live storage from successful initialization.
 * \return A new valid sample in the same independent monotonic domain.
 * \note On any failed sample, taint the epoch, retain the interrupt mask,
 *       call nx_fault_observe(NX_FAULT_PLATFORM, reason, ...) and park if the
 *       observer returns. Products may supply separately reviewed nonreturning
 *       recovery. No loan or observer is released by fault observation.
 */
nx_time_us_t nx_stm32u575_retained_time_now(void* context);
/**
 * \brief Close clock admission after every external face/deadline user joins.
 * \return SUCCESS, INVALID, CONTEXT or STATE; clean repeated stop is
 * idempotent. \note Does not reset generations or alter the source. An explicit
 * reopen starts a new zero-based elapsed domain, including after a tainted one.
 *       No scheduler, wake IRQ, WFI, Core restoration, Stop2 or VBAT retention
 *       guarantee is established by this read-only source component.
 */
nx_result_t
nx_stm32u575_retained_time_stop(nx_stm32u575_retained_time_storage_t* storage);
#ifdef __cplusplus
}
#endif
#endif
