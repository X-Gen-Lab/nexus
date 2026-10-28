/**
 * \file            gd32f470_i2c_cold.h
 * \brief           Static GD470 I2C resources and retained cold ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_I2C_COLD_H
#define NEXUS_GD32F470_I2C_COLD_H

#include "gd32f470_cold_pins.h"
#include "gd32f470_i2c_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact controller, two GPIOB AF4 pads and immutable DMA facts. */
typedef struct {
    uint32_t registers;
    const nx_gd32_cold_pin_config_t* pins;
    uint16_t* snapshots;
    const nx_i2c_v1_dma_t* dma;
    uint8_t address;
    uint8_t priority;
    bool asynchronous;
} nx_gd32_i2c_config_t;

/** \brief Actual effects retained until checked pin/vector/gate restoration. */
typedef struct {
    nx_gd32_cold_pins_receipt_t pins;
    uint32_t peripheral_clock;
    uint8_t priorities[4];
    bool owned;
    bool vectors;
    bool prepared;
    bool pins_started;
} nx_gd32_i2c_cold_t;

/**
 * \brief Acquire exact cold I2C resources in privileged unmasked task context.
 * \param[in] config: Static resources with two Platform-owned snapshot slots.
 * \param[in,out] cold: Initially zero receipt, retained through failed cleanup.
 * \return Success or error; actual acquisitions require explicit release.
 */
nx_result_t nx_gd32_i2c_cold_acquire(const nx_gd32_i2c_config_t* config,
                                     nx_gd32_i2c_cold_t* cold);
/**
 * \brief Observe initialized publishers and the selected controller gate.
 * \param[in] config: The unchanged selected configuration.
 * \param[in,out] cold: Receipt whose engine has actually initialized.
 * \return Success or IO. Even failed observation retains an initialized owner.
 */
nx_result_t nx_gd32_i2c_cold_commit(const nx_gd32_i2c_config_t* config,
                                    nx_gd32_i2c_cold_t* cold);
/**
 * \brief Restore cold resources after the actual engine and loans drain.
 * \param[in] config: The same immutable resources supplied to acquisition.
 * \param[in,out] cold: Cleared only after all acquired effects are restored.
 * \return Success or error retaining all unproved cleanup responsibilities.
 */
nx_result_t nx_gd32_i2c_cold_release(const nx_gd32_i2c_config_t* config,
                                     nx_gd32_i2c_cold_t* cold);
/**
 * \brief Prepare the exact polling controller and its first static endpoint.
 * \param[in,out] port: Cold serialized controller state.
 * \param[in,out] endpoint: First addressed child belonging to that controller.
 * \param[in] config: Static standard-mode resources and first child address.
 * \param[in,out] cold: Retained receipt requiring close after partial failure.
 * \return Success or error; no allocation, registry or worker is introduced.
 */
nx_result_t nx_gd32_i2c_prepare(nx_gd32_i2c_state_t* port,
                                nx_gd32_i2c_endpoint_state_t* endpoint,
                                const nx_gd32_i2c_config_t* config,
                                nx_gd32_i2c_cold_t* cold);
/**
 * \brief Close polling I2C after the caller has joined all direct I/O calls.
 * \param[in,out] port: The same serialized controller owner.
 * \param[in] config: Unchanged configuration from prepare.
 * \param[in,out] cold: Actual acquired effects, retained on cleanup failure.
 * \return Success, BUSY or an error preserving explicit retry responsibility.
 */
nx_result_t nx_gd32_i2c_close(nx_gd32_i2c_state_t* port,
                              const nx_gd32_i2c_config_t* config,
                              nx_gd32_i2c_cold_t* cold);
/**
 * \brief Prepare one exact IRQ/DMA I2C owner and all selected publishers.
 * \param[in,out] port: Initially cold owner, with no outstanding calls or
 * loans. \param[in] config: Static exact controller, pads, DMA and IRQ
 * selection. \param[in,out] cold: Receipt retained until successful close.
 * \return Success or error; a failed initialized owner still requires close.
 */
nx_result_t nx_gd32_i2c_async_prepare(nx_i2c_v1_state_t* port,
                                      const nx_gd32_i2c_config_t* config,
                                      nx_gd32_i2c_cold_t* cold);
/**
 * \brief Drain asynchronous I2C before releasing pins, publishers and clocks.
 * \param[in,out] port: Owner whose submitters and observers have joined.
 * \param[in] config: Unchanged exact resources from prepare.
 * \param[in,out] cold: Actual effects retained on failed drain or restoration.
 * \return Success or error; cancellation never revokes an outstanding borrow.
 */
nx_result_t nx_gd32_i2c_async_close(nx_i2c_v1_state_t* port,
                                    const nx_gd32_i2c_config_t* config,
                                    nx_gd32_i2c_cold_t* cold);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_GD32F470_I2C_COLD_H */
