/**
 * \file            gd32f470_cold_pins.h
 * \brief           Selected GD470 cold pin transaction and local rollback
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_COLD_PINS_H
#define NEXUS_GD32F470_COLD_PINS_H

#include "gd32f470_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable selected pad; no runtime route lookup is performed. */
typedef struct {
    uint32_t registers;
    uint8_t pin;
    uint8_t alternate;
    uint8_t mode;
    uint8_t pull;
    bool open_drain;
} nx_gd32_cold_pin_config_t;

/** \brief Local constructor rollback; Platform owns cross-call recovery. */
typedef struct {
    uint32_t saved_clocks;
    uint16_t captured;
} nx_gd32_cold_pins_receipt_t;

/**
 * \brief Prepare selected pins before calling a concrete controller driver.
 * \param[in] pins: Distinct immutable pad configurations, at most sixteen.
 * \param[in] count: Number of exact selected pads, never a capacity pool.
 * \param[out] saved: Caller storage for count local pin snapshots.
 * \param[in,out] receipt: Initially zero; actual acquisitions on failure.
 * \return SUCCESS or error. Call rollback after a failed constructor.
 * \note Serialized privileged task with interrupts unmasked. Platform retains
 *       its persistent pin/clock lease until complete cleanup succeeds.
 */
nx_result_t nx_gd32_cold_pins_prepare(const nx_gd32_cold_pin_config_t* pins,
                                      size_t count, uint16_t* saved,
                                      nx_gd32_cold_pins_receipt_t* receipt);
/**
 * \brief Restore locally acquired pads and preserve failed bank clocks.
 * \param[in] pins: The same immutable pad configurations used by prepare.
 * \param[in] count: Unchanged selected count.
 * \param[in] saved: Local snapshots produced by prepare.
 * \param[in,out] receipt: Actual acquisitions, updated after restoration.
 * \return SUCCESS or IO; failed pads retain clocks for Platform recovery.
 */
nx_result_t nx_gd32_cold_pins_rollback(const nx_gd32_cold_pin_config_t* pins,
                                       size_t count, const uint16_t* saved,
                                       nx_gd32_cold_pins_receipt_t* receipt);
/**
 * \brief Withdraw drained controller pads before Platform restoration.
 * \param[in] pins: Exact selected pads, after their concrete driver stops.
 * \param[in] count: Number of selected pads.
 * \return SUCCESS or an error retaining the Platform cleanup obligation.
 * \note This does not release Platform clocks or restore its snapshots.
 */
nx_result_t nx_gd32_cold_pins_release(const nx_gd32_cold_pin_config_t* pins,
                                      size_t count);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_GD32F470_COLD_PINS_H */
