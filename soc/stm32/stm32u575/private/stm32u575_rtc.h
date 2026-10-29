/**
 * \file            stm32u575_rtc.h
 * \brief           Retained BCD calendar and one polling RUN Alarm A owner
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_RTC_H
#define NX_STM32U575_RTC_H
#include "nexus/io/rtc.h"
#include "stm32u575_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Static caller-owned, serialized privileged RUN-only RTC owner.
 * \note External boot policy supplies TZEN0, live LSE32768, RTCEN/RTCAPBEN,
 *       PWREN/DBP and an initialized BCD24h BYPSHAD calendar with PRER127/255.
 *       Crystal mode LSEBYP=0 and LSEDRV=2/3 are required: this conservative
 *       revision-independent profile excludes revision X limitations ES0499
 *       Rev12 2.2.3/16 without changing drive. External owners exclude PC13
 *       transitions on this LQFP part per section 2.2.1; register reads cannot
 *       establish that electrical condition. The clock face is copied; its
 *       disjoint context remains borrowed through all requests and observers.
 *       Calendar setting, calibration, oscillator selection, backup reset,
 *       low-power entry and VBAT guarantees are absent. Initialization and stop
 *       never write calendar, clock, power or TAMP.
 *       Alarm A is exclusively leased with ALRAIE clear; no vector is enabled.
 *       WPR must be locked at handoff and between calls, with no other writer.
 *       Each protected write briefly unlocks CA/53 and always relocks FF;
 *       unreadable WPR state is not discovered or saved. Only accepted service
 *       phases write WPR. Cold preflight and admission stay read-only.
 *       Owner storage and external resources outlive loans and all joins.
 */
typedef struct {
    RTC_TypeDef* registers;
    PWR_TypeDef* power;
    nx_clock_t clock;
    nx_rtc_request_t* active;
    uint64_t generation;
    nx_time_us_t ready_at;
    nx_result_t terminal;
    uint32_t saved_alarm;
    uint32_t saved_subsecond;
    uint8_t stage;
    bool retained;
    bool opened;
    bool closing;
    bool terminal_set;
    bool tainted;
} nx_stm32u575_rtc_state_t;
extern const nx_rtc_ops_t nx_stm32u575_rtc_ops;
/** \brief Reject unprepared or foreign backup state before platform effects.
 * \return SUCCESS, STATE, BUSY, INVALID or CONTEXT; no hardware writes.
 * \note The external boot owner asserts calendar provenance, including YY00.
 *       Valid register fields alone cannot prove retained calendar validity.
 */
nx_result_t nx_stm32u575_rtc_preflight(void);
/**
 * \brief Read-only admission of an exact prepared, idle retained RTC.
 * \param[in,out] state: Zero-initialized stable owner with exact register
 * bases. \param[in] clock: Separate immutable monotonic Core clock face,
 * copied. \return SUCCESS, INVALID, STATE, BUSY or EXHAUSTED without hardware
 * writes. \note Reopen only after stop and external joins. Generation never
 * wraps.
 */
nx_result_t nx_stm32u575_rtc_initialize(nx_stm32u575_rtc_state_t* state,
                                        const nx_clock_t* clock);
/** \brief Bind exact cold resources without resetting retained generations. */
nx_result_t nx_stm32u575_rtc_prepare(nx_stm32u575_rtc_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
