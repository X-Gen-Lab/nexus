/**
 * \file            legacy_devices.h
 * \brief           Reviewed F103RB/GD303RC legacy physical device engines
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_LEGACY_DEVICES_H
#define NX_LEGACY_DEVICES_H
#include "nexus/arch/arch.h"
#include "nexus/io/adc.h"
#include "nexus/io/flash.h"
#include "nexus/io/timer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Private reviewed ADC register ABI, not a generic family promise. */
typedef struct {
    volatile uint32_t status, control1, control2, sample1, sample2;
    volatile uint32_t injected_offset[4];
    volatile uint32_t high, low, sequence1, sequence2, sequence3;
    volatile uint32_t injected_sequence, injected_data[4], data;
} nx_legacy_adc_registers_t;
/** \brief Independent channel zero polling, no DMA or retained output. */
typedef struct {
    nx_legacy_adc_registers_t* registers;
    volatile uint32_t* gate;
    volatile uint32_t* divider;
    volatile uint32_t* pin_mode;
    volatile uint32_t* pin_lock;
    const volatile bool* clock_started;
    uint32_t gate_mask, divider_mask, reference_mv, poll_limit;
    uint32_t startup_delay_us;
    bool initialized, retained, active, fault;
} nx_legacy_adc_state_t;
/** \brief Exact two-byte program ABI shared only after register review. */
typedef struct {
    volatile uint32_t access, key, option_key, status, control, address;
} nx_legacy_flash_registers_t;
/** \brief Full physical geometry; callers explicitly reserve their regions. */
typedef struct {
    nx_legacy_flash_registers_t* registers;
    volatile uint8_t* memory;
    const nx_flash_geometry_t* geometry;
    uint32_t supply_mv;
    bool initialized, active, fault;
} nx_legacy_flash_state_t;
/** \brief Private fixed CH1 timer ABI reviewed against each exact SDK. */
typedef struct {
    volatile uint32_t control1, control2, slave, interrupt, status, event;
    volatile uint32_t channel_mode1, channel_mode2, enable, counter;
    volatile uint32_t prescaler, reload, repetition, compare1;
} nx_legacy_pwm_registers_t;
/** \brief One fixed PA6 channel with Board-declared inactive level. */
typedef struct {
    nx_legacy_pwm_registers_t* registers;
    volatile uint32_t* gate;
    volatile uint32_t* pin_mode;
    volatile uint32_t* pin_lock;
    volatile uint32_t* pin_latch;
    volatile uint32_t* pin_set;
    const volatile bool* clock_started;
    uint32_t gate_mask, tick_hz, period_ticks, duty_ticks, inactive;
    uint32_t poll_limit;
    bool initialized, retained, running, fault;
} nx_legacy_pwm_state_t;
extern const nx_adc_ops_t nx_legacy_adc_ops;
extern const nx_pwm_ops_t nx_legacy_pwm_ops;
extern const nx_flash_ops_t nx_legacy_flash_ops;
/** \brief Calibrate exact ADC1/ADC0 PA0 after clock and pin leases.
 * \param[in,out] port: Privileged unmasked cold owner, finite poll budget.
 * \return SUCCESS or error; retained requires close before storage reuse.
 * \note Fixed 4MHz ADC clock, 239.5-cycle sampling, raw12bit counts.
 */
nx_result_t nx_legacy_adc_initialize(nx_legacy_adc_state_t* port);
/** \brief Withdraw converter before releasing the retained cold lease.
 * \param[in,out] port: Serialized owner, acquisition calls already joined.
 * \return SUCCESS after ADON/source readback, error retains responsibility.
 */
nx_result_t nx_legacy_adc_close(nx_legacy_adc_state_t* port);
/** \brief Configure stopped exclusive TIM3/TIMER2 CH1, reset-like source.
 * \param[in,out] port: Static cold owner after GPIO/timer gate acquisition.
 * \return SUCCESS or error; retained requires close before lease release.
 */
nx_result_t nx_legacy_pwm_initialize(nx_legacy_pwm_state_t* port);
/** \brief Stop timer and drive declared PA6 inactive GPIO level.
 * \param[in,out] port: Serialized owner with all consumers joined.
 * \return SUCCESS after readback; failure retains the concrete lease.
 */
nx_result_t nx_legacy_pwm_close(nx_legacy_pwm_state_t* port);
/** \brief Bind exact full geometry without erase/program side effects.
 * \param[in,out] port: Static task owner; supply declared 2.7..3.6V.
 * \return SUCCESS or INVALID/BUSY/CONTEXT; no product partition inferred.
 */
nx_result_t nx_legacy_flash_initialize(nx_legacy_flash_state_t* port);
/** \brief Prove no pulse, clear only owned operation modes, relock.
 * \param[in,out] port: Same owner, all reads/writes already joined.
 * \return SUCCESS or BUSY/IO; failed relock retains initialized state.
 */
nx_result_t nx_legacy_flash_close(nx_legacy_flash_state_t* port);
#ifndef NX_LEGACY_READ
#define NX_LEGACY_READ(reg) (*(reg))
#endif
#ifndef NX_LEGACY_WRITE
#define NX_LEGACY_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_LEGACY_POLL
#define NX_LEGACY_POLL(kind, port) ((void)(kind), (void)(port))
#endif
#ifndef NX_LEGACY_PROGRAM
#define NX_LEGACY_PROGRAM(pointer, value) (*(pointer) = (value))
#endif
#ifdef __cplusplus
}
#endif
#endif
