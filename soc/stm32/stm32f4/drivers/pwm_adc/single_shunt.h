/**
 * \file            single_shunt.h
 * \brief           Granted edge-carrier DC-link injected-conversion operations
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_SINGLE_SHUNT_H
#define NEXUS_STM32F4_SINGLE_SHUNT_H

#include "drivers/ip/av/stm32f407_av.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           One granted DC-link converter and immediate TIM1/8 CC4.
 * \note            The owner supplies clocks, pads, common mode and IRQ joins.
 *                  This leaf has no state, registration or request. The carrier
 *                  is edge-aligned/upcounting, with channel-four preload off.
 *                  Declared deadlines do not prove physical sampling precision.
 */
typedef struct {
    TIM_TypeDef* timer;
    ADC_TypeDef* adc;
    uint8_t channel;
    uint8_t sample_time;
    uint32_t trigger_selector;
    uint32_t period_ticks;
    uint32_t rearm_guard_ticks;
    uint32_t maximum_lateness_ticks;
} nx_stm32_single_shunt_config_t;

/**
 * \brief           Validate representable immutable fields without MMIO.
 * \param[in]       config: Stable granted descriptor.
 * \return          True for the selected edge-carrier capture profile.
 */
bool nx_stm32_single_shunt_valid(const nx_stm32_single_shunt_config_t* config);

/**
 * \brief           Prepare one right-aligned injected rank with sources closed.
 * \param[in]       config: Idle converter and immediate upcounting carrier.
 * \return          SUCCESS, INVALID, BUSY, STATE or IO on readback failure.
 * \note            Cold serialized context; ADON must be clear. Partial effects
 *                  remain caller-owned. Common mode, clocks and NVIC stay owned
 *                  by the domain and are untouched here.
 */
nx_result_t
nx_stm32_single_shunt_prepare(const nx_stm32_single_shunt_config_t* config);

/**
 * \brief           Arm a future compare after the owner publishes its slot.
 * \param[in]       config: Warm converter and running immediate carrier.
 * \param[in]       trigger_ticks: Absolute compare inside this carrier.
 * \return          SUCCESS or exact error; partial rearm stays caller-owned.
 * \note            Bounded IRQ context. Rejects pending conversion and elapsed
 *                  compare. Core monotonic time must bound one carrier; this
 *                  leaf cannot detect whole missed periods. Sources stay closed
 *                  during CCR4 writes. Output channels remain owner-controlled.
 */
nx_result_t
nx_stm32_single_shunt_arm(const nx_stm32_single_shunt_config_t* config,
                          uint32_t trigger_ticks);

/**
 * \brief           Read a completed DC code with carrier provenance.
 * \param[in]       config: Same converter and running carrier.
 * \param[in]       expected_tick: Compare selected for this sample.
 * \param[out]      out: Separate aligned output, unchanged on failure.
 * \return          SUCCESS, BUSY or an exact failure code.
 * \note            Bounded IRQ context. Success and conversion faults close
 *                  sampling IRQ/trigger while retaining ADON. The owner stores
 *                  vector/plan identity and arms the second sample. This leaf
 *                  never publishes a complete two-sample frame.
 */
nx_result_t
nx_stm32_single_shunt_read(const nx_stm32_single_shunt_config_t* config,
                           uint32_t expected_tick, uint16_t* out);

/**
 * \brief           Attempt to inhibit the converter after any mode fault.
 * \param[in]       config: Its converter gate remains caller-readable.
 * \return          SUCCESS, INVALID or IO; failure retains cleanup duty.
 * \note            Closes sampling IRQ, trigger and ADON, then clears status.
 *                  Joining IRQ and releasing resources remain with the owner.
 */
nx_result_t
nx_stm32_single_shunt_disable(const nx_stm32_single_shunt_config_t* config);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F4_SINGLE_SHUNT_H */
