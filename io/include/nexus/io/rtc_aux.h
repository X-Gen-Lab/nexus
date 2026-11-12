/**
 * \file            rtc_aux.h
 * \brief           Optional raw RTC sources and backup storage ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_RTC_AUX_H
#define NEXUS_IO_RTC_AUX_H
#include "nexus/io/memory_window.h"
#include "nexus/io/rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Raw periodic wake counter clock, independent of Core deadlines. */
typedef enum {
    NX_RTC_WAKE_DIV16,
    NX_RTC_WAKE_DIV8,
    NX_RTC_WAKE_DIV4,
    NX_RTC_WAKE_DIV2,
    NX_RTC_WAKE_SECOND,
    NX_RTC_WAKE_SECOND_EXTENDED
} nx_rtc_wake_clock_t;
/** \brief Enabled counter reloads after reload+1 ticks; extended adds 65536. */
typedef struct {
    bool enabled;
    nx_rtc_wake_clock_t clock;
    uint16_t reload;
} nx_rtc_wakeup_t;
/** \brief Explicit capture pins and hardware tamper semantics.
 * \note Pin 1 means RTC_AF1; pin 2 means RTC_AF2 and requires an exact fitted
 *       package/route. Tamper detection erases all hardware backup registers.
 *       filter is 0=edge, 1/2/3=2/4/8 consecutive level samples. trigger bits
 *       mean falling edge without filter or high active level with filter.
 *       sample_rate is RTCCLK/(32768 >> value), 0..7; precharge is 1<<value
 *       RTCCLK cycles, 0..3. No GPIO, pull resistor or PCB route is inferred.
 *       A provider may reject silicon-unsafe edges. A failed post-enable
 *       input check can retain an armed source; success never certifies
 *       tamper security or external setup/hold timing.
 */
typedef struct {
    bool timestamp_enabled;
    bool timestamp_falling;
    uint8_t timestamp_pin;
    uint8_t tamper_mask;
    uint8_t trigger_mask;
    uint8_t tamper1_pin;
    uint8_t filter;
    uint8_t sample_rate;
    uint8_t precharge;
    bool pullup;
    bool timestamp_on_tamper;
} nx_rtc_capture_config_t;
/** \brief Timestamp excludes year, which the hardware does not capture.
 * \note subseconds is the captured downcounter, with explicit denominator.
 *       overflow means another event was lost. Domain is the setting domain,
 *       never the Core deadline clock and never an assertion of UTC.
 */
typedef struct {
    nx_rtc_domain_t domain;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t subseconds;
    uint16_t subsecond_divisor;
    bool overflow;
} nx_rtc_capture_t;
/** \brief Latched auxiliary events; reading never acknowledges a source.
 * \note Ordinary events/acknowledge exclude ALARM_A, whose finite request
 *       owns the flag. A joined Standby preparation can explicitly authorize
 *       discarding it; wake_route can select it independently.
 */
enum nx_rtc_aux_events {
    NX_RTC_EVENT_ALARM_B = 1U,
    NX_RTC_EVENT_WAKEUP = 2U,
    NX_RTC_EVENT_TIMESTAMP = 4U,
    NX_RTC_EVENT_TIMESTAMP_OVERFLOW = 8U,
    NX_RTC_EVENT_TAMPER1 = 16U,
    NX_RTC_EVENT_TAMPER2 = 32U,
    NX_RTC_EVENT_ALARM_A = 64U
};
/** \brief Calibration method; smooth/coarse cannot run together. */
typedef enum {
    NX_RTC_CALIBRATION_SMOOTH,
    NX_RTC_CALIBRATION_COARSE
} nx_rtc_calibration_kind_t;
/** \brief Raw calibration facts, never measured oscillator accuracy.
 * \note Smooth masks 0..511 pulses per 32-second base and optionally adds
 *       512; window_seconds is 8,16,32. Short windows require low 2/1 mask bits
 *       zero. Coarse uses pulses 0..31, subtract selects the negative direction
 *       and window_seconds must be zero. Coarse restarts the calendar divider
 *       and therefore advances its setting domain.
 */
typedef struct {
    nx_rtc_calibration_kind_t kind;
    uint16_t pulses;
    uint8_t window_seconds;
    bool add512;
    bool subtract;
} nx_rtc_calibration_t;
/** \brief Explicit autonomous resources transferred to an external owner. */
enum nx_rtc_backup_effects {
    NX_RTC_BACKUP_POWER_GATE = 1U,
    NX_RTC_BACKUP_WRITE_ACCESS = 2U,
    NX_RTC_BACKUP_OSCILLATOR = 4U,
    NX_RTC_BACKUP_CLOCK_SOURCE = 8U,
    NX_RTC_BACKUP_CLOCK_GATE = 16U,
    NX_RTC_BACKUP_CALENDAR = 32U,
    NX_RTC_BACKUP_QUIET_PC13 = 128U
};
/** \brief Copied transfer receipt, never proof of physical VBAT retention. */
typedef struct {
    uint32_t resources;
    uint32_t oscillator_hz;
    nx_rtc_calendar_t calendar;
} nx_rtc_backup_handoff_t;
/** \brief Exact caller-owned Standby preparation identity, initially zero.
 * \note Never copy or alter a live receipt. The RTC owner retains this exact
 *       object through commit and until successful restore, or actual reset.
 *       It is not proof that Standby occurred or that backup power survived.
 */
typedef struct {
    const void* owner;
    uint64_t epoch;
} nx_rtc_standby_t;
/** \brief Optional methods share the exact RTC context and lifecycle owner. */
typedef struct nx_rtc_aux_ops {
    nx_result_t (*alarm_b)(void*, bool, const nx_rtc_alarm_t*);
    nx_result_t (*wakeup)(void*, const nx_rtc_wakeup_t*);
    nx_result_t (*capture_configure)(void*, const nx_rtc_capture_config_t*);
    nx_result_t (*capture_read)(void*, nx_rtc_capture_t*);
    nx_result_t (*events)(void*, uint32_t*);
    nx_result_t (*acknowledge)(void*, uint32_t);
    nx_result_t (*calibrate)(void*, const nx_rtc_calibration_t*);
    nx_result_t (*wake_route)(void*, uint32_t);
    nx_result_t (*backup_read)(void*, uint8_t, uint32_t*);
    nx_result_t (*backup_write)(void*, uint8_t, uint32_t);
    nx_result_t (*sram_prepare)(void*, bool);
    nx_result_t (*sram_acquire)(void*, nx_memory_window_lease_t*);
    nx_result_t (*sram_release)(void*, nx_memory_window_lease_t*, uint64_t);
    nx_result_t (*handoff)(void*, nx_rtc_backup_handoff_t*);
    nx_result_t (*standby_prepare)(void*, uint32_t, uint32_t,
                                   nx_rtc_standby_t*);
    nx_result_t (*standby_validate)(void*, const nx_rtc_standby_t*);
    nx_result_t (*standby_commit)(void*, nx_rtc_standby_t*);
    nx_result_t (*standby_restore)(void*, nx_rtc_standby_t*);
} nx_rtc_aux_ops_t;
/** \brief Configure/disable Alarm B; no retained caller pointer or request.
 * \param[in] port: Same serialized privileged unmasked RTC task owner.
 * \param[in] enabled: Explicit comparator enable; no wildcards.
 * \param[in] alarm: Valid copied pattern, required even when disabling.
 * \return SUCCESS or exact provider error; timeout/IO may retain hardware
 * responsibility until stop succeeds. No implicit backup reset or retry.
 */
nx_result_t nx_rtc_port_alarm_b(const nx_rtc_port_t* port, bool enabled,
                                const nx_rtc_alarm_t* alarm);
/** \brief Configure the periodic counter with a bounded provider deadline.
 * \param[in] port: Serialized RTC task owner.
 * \param[in] config: Copied clock/reload facts, never a Core deadline.
 * \return SUCCESS or provider error; failed withdrawal retains ownership.
 */
nx_result_t nx_rtc_port_wakeup(const nx_rtc_port_t* port,
                               const nx_rtc_wakeup_t* config);
/** \brief Set explicitly owned timestamp/tamper inputs; no GPIO acquisition.
 * \param[in] port: Serialized RTC task owner with externally admitted pads.
 * \param[in] config: Copied valid hardware choices; tamper erase authorized.
 * \return SUCCESS or rejection/error; no product erase or retention policy.
 */
nx_result_t
nx_rtc_port_capture_configure(const nx_rtc_port_t* port,
                              const nx_rtc_capture_config_t* config);
/** \brief Copy the first captured event without clearing timestamp flags.
 * \param[in] port: Serialized RTC task owner.
 * \param[out] capture: Disjoint output, unchanged on failure.
 * \return SUCCESS, BUSY when no capture, or exact provider error.
 */
nx_result_t nx_rtc_port_capture_read(const nx_rtc_port_t* port,
                                     nx_rtc_capture_t* capture);
/** \brief Copy latched auxiliary flags without acknowledging them.
 * \param[in] port: Serialized RTC task owner.
 * \param[out] events: Disjoint output, unchanged on failure.
 * \return SUCCESS or exact provider error.
 */
nx_result_t nx_rtc_port_events(const nx_rtc_port_t* port, uint32_t* events);
/** \brief Clear only selected observed auxiliary flags, preserving others.
 * \param[in] port: Serialized RTC task owner.
 * \param[in] events: Explicit NX_RTC_EVENT mask; unknown bits reject.
 * \return SUCCESS or IO/BUSY when the source remains asserted.
 * \note Read a timestamp before acknowledging it. Hardware is a single latch,
 *       not an event queue; concurrent arrivals may be coalesced by hardware.
 */
nx_result_t nx_rtc_port_acknowledge(const nx_rtc_port_t* port, uint32_t events);
/** \brief Apply explicit hardware calibration, without changing Core time.
 * \param[in] port: Serialized owner; no alarm/capture loans for coarse mode.
 * \param[in] calibration: Copied raw settings.
 * \return SUCCESS or provider error; no measured ppm claim or auto tuning.
 */
nx_result_t nx_rtc_port_calibrate(const nx_rtc_port_t* port,
                                  const nx_rtc_calibration_t* calibration);
/** \brief Explicitly route sources to EXTI wake events, without NVIC handlers.
 * \param[in] port: Serialized RTC task owner, external STOP owner joined.
 * \param[in] events: Alarm A/B, wakeup, timestamp and/or tamper event mask.
 * \return SUCCESS or provider error; zero withdraws the event route.
 * \note Successful route enables source interrupt generation plus EXTI event
 *       delivery. Core time may stop in STOP; caller owns clock reconciliation.
 */
nx_result_t nx_rtc_port_wake_route(const nx_rtc_port_t* port, uint32_t events);
/** \brief Read one of the exact provider's backup words, without allocation.
 * \param[in] port: Serialized RTC task owner.
 * \param[in] index: Hardware word index; F407 has 20 words.
 * \param[out] value: Disjoint output, unchanged on failure.
 * \return SUCCESS or exact provider error; no checksum/provenance policy.
 */
nx_result_t nx_rtc_port_backup_read(const nx_rtc_port_t* port, uint8_t index,
                                    uint32_t* value);
/** \brief Write/read back one backup word without clearing other words.
 * \param[in] port: Serialized RTC task owner.
 * \param[in] index: Exact hardware word index.
 * \param[in] value: Literal data; UID or CRC grants no secret provenance.
 * \return SUCCESS or INVALID/STATE/IO; tamper may erase hardware words.
 */
nx_result_t nx_rtc_port_backup_write(const nx_rtc_port_t* port, uint8_t index,
                                     uint32_t value);
/** \brief Explicitly acquire SRAM gate and optional backup regulator.
 * \param[in] port: Serialized RTC task owner, no competing DBP/BRE writer.
 * \param[in] retain: Request regulator enable/readiness, not proof of VBAT.
 * \return SUCCESS or provider error; failure retains acquired effects until
 * stop. SRAM bytes are never cleared, tested or given a product layout.
 */
nx_result_t nx_rtc_port_sram_prepare(const nx_rtc_port_t* port, bool retain);
/** \brief Borrow the whole prepared CPU-only backup SRAM until exact release.
 * \param[in] port: Serialized RTC task owner.
 * \param[in,out] lease: Stable zero disjoint storage, retained until release.
 * \return SUCCESS or rejection with no new loan; no DMA/execution permission.
 */
nx_result_t nx_rtc_port_sram_acquire(const nx_rtc_port_t* port,
                                     nx_memory_window_lease_t* lease);
/** \brief Release the original SRAM loan after all derived users joined.
 * \param[in] port: Same serialized owner that granted the loan.
 * \param[in,out] lease: Original stable loan storage, never a copied token.
 * \param[in] epoch: Exact generation returned on acquisition.
 * \return SUCCESS or rejection retaining the original loan and all effects.
 */
nx_result_t nx_rtc_port_sram_release(const nx_rtc_port_t* port,
                                     nx_memory_window_lease_t* lease,
                                     uint64_t epoch);
/** \brief Transfer retained provisioning effects after actual source stop.
 * \param[in] port: Closed serialized RTC owner; every external user joined.
 * \param[out] handoff: Disjoint receipt, unchanged on failure. The caller
 * explicitly accepts continuing power/access/oscillator/calendar ownership,
 * including any reported crystal-oscillator quiet-pad responsibility.
 * \return SUCCESS, BUSY for live loans/sources, STATE without transferable
 * effects, or exact provider error. No reset, erase or register write occurs.
 * \note This is a responsibility transfer. It does not disable autonomous
 * clocks or choose a later shutdown policy. Platform close may succeed only
 * after this transfer. The receipt does not permit concurrent backup writers.
 */
nx_result_t nx_rtc_port_backup_handoff(const nx_rtc_port_t* port,
                                       nx_rtc_backup_handoff_t* handoff);
/** \brief Disable RTC wake front ends and explicitly discard observed events.
 * \param[in] port: Suspended RTC owner; every request/SRAM user is joined.
 * \param[in] discard_events: Explicit NX_RTC_EVENT authorization, including
 *                          ALARM_A only after its finite request is joined.
 * \param[in] poll_limit: Positive finite write-ready observation budget.
 * \param[in,out] receipt: Fresh zero stable internal disjoint caller storage.
 * \return SUCCESS with sources disabled; BUSY for unauthorized pending events;
 *         IO/TIMEOUT can retain receipt and partial effects for restore.
 * \note Privileged unmasked Thread. Preserve EXTI route configuration and
 *       backup/calendar. Only explicitly authorized observed flags/PR clear.
 *       No Core deadline/tick, implicit event discard or backup reset occurs.
 */
nx_result_t nx_rtc_port_standby_prepare(const nx_rtc_port_t* port,
                                        uint32_t discard_events,
                                        uint32_t poll_limit,
                                        nx_rtc_standby_t* receipt);
/** \brief Read-only proof of the exact disabled independent-clock RTC receipt.
 * \param[in] port: Same suspended serialized owner, privileged Thread.
 * \param[in] receipt: Exact live original storage, never a copied token.
 * \return SUCCESS or INVALID/STATE/IO without hardware or receipt mutation.
 * \note Masked or unmasked Thread; independent of any old Core epoch.
 */
nx_result_t nx_rtc_port_standby_validate(const nx_rtc_port_t* port,
                                         const nx_rtc_standby_t* receipt);
/** \brief Reenable original RTC sources after the Power owner clears WUF.
 * \param[in] port: Same suspended owner, privileged masked Thread.
 * \param[in,out] receipt: Exact prepared identity retained through entry.
 * \return SUCCESS after source readback, or retained error requiring restore.
 * \note Power owns the disable-EWUP/CWUF/reenable-EWUP/Standby sequence.
 *       This operation never clears RTC events, EXTI pending or PWR flags.
 *       Actual Standby resets the image; a returning entry needs restore.
 */
nx_result_t nx_rtc_port_standby_commit(const nx_rtc_port_t* port,
                                       nx_rtc_standby_t* receipt);
/** \brief Restore source controls and release the exact preparation receipt.
 * \param[in] port: Same suspended serialized privileged Thread owner.
 * \param[in,out] receipt: Original live storage through cleanup/commit failure.
 * \return SUCCESS only after full readback and final detach; failure retains
 *         the receipt and responsibility for all partially restored sources.
 * \note Masked or unmasked, including a new System epoch. No old Core clock
 *       callback/deadline is used. Preserve every event arriving after prepare.
 */
nx_result_t nx_rtc_port_standby_restore(const nx_rtc_port_t* port,
                                        nx_rtc_standby_t* receipt);
#ifdef __cplusplus
}
#endif
#endif
