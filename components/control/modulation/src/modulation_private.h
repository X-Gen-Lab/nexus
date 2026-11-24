/**
 * \file            modulation_private.h
 * \brief           Shared pulse geometry and bounded observation windows
 * \author          Nexus Team
 */
#ifndef NEXUS_MODULATION_PRIVATE_H
#define NEXUS_MODULATION_PRIVATE_H

#include "nexus/control/modulation.h"

/** \brief Constant-switching-state interval in one unfolded carrier. */
typedef struct {
    uint32_t begin;
    uint32_t end;
    uint8_t vector;
} nx_modulation_window_t;

/** \brief Validate caller storage and prepare private quantized pulses.
 * \param[in] config: Immutable selected profile.
 * \param[in] requested_voltage: Finite stationary volts.
 * \param[in] bus_voltage_v: Finite positive bus volts.
 * \param[in] destination: Caller output, inspected only for legal span/aliases.
 * \param[out] next: Private stack plan, never the public destination.
 * \return SUCCESS or exact input/numeric error; destination is not modified.
 * \note Internal bounded Task/IRQ helper; next is known disjoint local storage.
 */
nx_result_t nx_modulation_begin(const nx_modulation_config_t* config,
                                nx_motor_ab_t requested_voltage,
                                float bus_voltage_v,
                                const nx_modulation_output_t* destination,
                                nx_modulation_output_t* next);

/** \brief Enumerate physical three-leg switching intervals without allocation.
 * \param[in] config: Previously validated period.
 * \param[in] plan: Previously prepared positive bounded pulse widths/starts.
 * \param[out] out: Seven local windows suffice for six physical pulse edges.
 * \return Actual window count, at most seven.
 * \note Internal pure helper; no state or hardware is accessed.
 */
size_t nx_modulation_windows(const nx_modulation_config_t* config,
                             const nx_modulation_output_t* plan,
                             nx_modulation_window_t out[7]);

/** \brief Select an aperture satisfying analog and final-submit budgets.
 * \param[in] config: Validated conservative selected budgets.
 * \param[in] window: One actual constant-vector interval.
 * \param[in] not_before: Earliest resource-rearmed aperture tick.
 * \param[in] midpoint: Choose center when true, otherwise earliest legal tick.
 * \param[out] out: Private candidate, unchanged when no interval fits.
 * \return True only if the entire S/H envelope and result deadline fit.
 * \note Internal pure helper; declared budgets do not establish measured WCET.
 */
bool nx_modulation_aperture(const nx_modulation_config_t* config,
                            const nx_modulation_window_t* window,
                            uint32_t not_before, bool midpoint,
                            nx_modulation_sample_t* out);

#endif /* NEXUS_MODULATION_PRIVATE_H */
