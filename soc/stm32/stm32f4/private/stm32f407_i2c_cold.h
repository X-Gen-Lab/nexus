/**
 * \file            stm32f407_i2c_cold.h
 * \brief           Static I2C resources and retained cold cleanup
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_I2C_COLD_H
#define NEXUS_STM32F407_I2C_COLD_H

#include "stm32f407_i2c_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact pad and the existing Platform-owned snapshot slot. */
typedef struct {
    uint16_t* snapshot;
    uint8_t pin;
} nx_stm32_i2c_pin_config_t;

/** \brief Immutable selected controller, GPIOB AF4 and publisher facts. */
typedef struct {
    I2C_TypeDef* registers;
    const nx_stm32_i2c_pin_config_t* pins;
    const nx_i2c_v1_dma_t* dma;
    uint32_t clock_hz;
    uint8_t priority;
    bool asynchronous;
} nx_stm32_i2c_config_t;

/** \brief Semantic effects; clock bits preserve the original enabled state. */
enum {
    NX_STM32_I2C_COLD_OWNED = 1U << 0U,
    NX_STM32_I2C_COLD_PERIPHERAL_CLOCK = (1U << 1U) | (1U << 2U),
    NX_STM32_I2C_COLD_GPIO_CLOCK = 1U << 3U,
    NX_STM32_I2C_COLD_VECTORS = 1U << 4U,
    NX_STM32_I2C_COLD_PREPARED = 1U << 5U
};

/** \brief Retain exact publisher bytes and two original gate facts in six
 * bytes. \note Lifecycle calls serialize this private receipt; IRQs never
 * modify it. Pad values live only in the existing Platform-owned snapshot
 * slots.
 */
typedef struct {
    uint8_t priorities[4];
    uint8_t captured;
    uint8_t effects;
} nx_stm32_i2c_cold_receipt_t;

/** \brief Acquire exact polling resources in privileged unmasked Thread mode.
 * \param[in] config: Immutable resources with two distinct snapshot slots.
 * \param[in,out] receipt: Cold owner retained after unsuccessful restoration.
 * \return Success or rejection; acquired effects require cold release.
 * \note Platform first admits the pads and serializes the shared GPIO clock
 *       lease. It keeps these same snapshots until final resource cleanup.
 */
nx_result_t nx_stm32_i2c_cold_acquire(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Restore polling pads and clock gates after all calls join.
 * \param[in] config: The same immutable resources used for acquisition.
 * \param[in,out] receipt: Cleared only after observed restoration.
 * \return Success or failure retaining cleanup responsibility for retry.
 * \note The caller first stops the selected engine and all its publishers.
 */
nx_result_t nx_stm32_i2c_cold_release(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Observe polling clocks and the prepared peripheral.
 * \param[in] config: The unchanged selected resources.
 * \param[in] receipt: The prepared owner retaining all acquired effects.
 * \return Success only when selected gates and the prepared peripheral match.
 * \note Failure does not erase effects; the concrete owner must close them.
 */
nx_result_t
nx_stm32_i2c_cold_verify(const nx_stm32_i2c_config_t* config,
                         const nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Acquire async pads, gates and initially idle IRQ/DMA publishers.
 * \param[in] config: Exact asynchronous I2C1/I2C2 static resources.
 * \param[in,out] receipt: Initially clear; retained after failed restoration.
 * \return Success or failure preserving any remaining cleanup obligation.
 * \note Privileged unmasked Thread context; Platform serializes shared gates
 *       and admits pads before this call. Publishers retain all raw priorities.
 */
nx_result_t
nx_stm32_i2c_async_cold_acquire(const nx_stm32_i2c_config_t* config,
                                nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Join async publishers before restoring pads and original gates.
 * \param[in] config: Unchanged asynchronous resources used for acquisition.
 * \param[in,out] receipt: Cleared only after observed complete restoration.
 * \return Success or failure retaining effects for retry.
 * \note The engine and all callers have drained in privileged unmasked Thread
 *       context. An active/pending publisher never permits early pin release.
 */
nx_result_t
nx_stm32_i2c_async_cold_release(const nx_stm32_i2c_config_t* config,
                                nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Observe async clocks, peripheral and every initialized publisher.
 * \param[in] config: Unchanged asynchronous static resources.
 * \param[in] receipt: Prepared owner retaining all acquired effects.
 * \return Success only when selected gates, enables and priorities match.
 * \note Called by the serialized privileged prepare path; failure retains
 *       ownership until the concrete close path proves withdrawal.
 */
nx_result_t
nx_stm32_i2c_async_cold_verify(const nx_stm32_i2c_config_t* config,
                               const nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Prepare a cold polling owner without a generated hardware body.
 * \param[in,out] port: Serialized initially cold owner.
 * \param[in] config: Exact I2C1 standard-mode static resources.
 * \param[in,out] receipt: Retained through failed cleanup.
 * \return Success or failure; close retries any remaining acquired effects.
 */
nx_result_t nx_stm32_i2c_prepare(nx_stm32_i2c_state_t* port,
                                 const nx_stm32_i2c_config_t* config,
                                 nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Close a polling owner after all calls have joined.
 * \param[in,out] port: The owner supplied to prepare.
 * \param[in] config: Unchanged selected resources.
 * \param[in,out] receipt: The same retained cold effects.
 * \return Success, busy or failure retaining effects for retry.
 * \note An empty receipt never authorizes replacing an existing bound owner.
 */
nx_result_t nx_stm32_i2c_close(nx_stm32_i2c_state_t* port,
                               const nx_stm32_i2c_config_t* config,
                               nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Prepare an exact IRQ or finite-DMA owner in unmasked Thread mode.
 * \param[in,out] port: Initially cold owner, no outstanding calls or loans.
 * \param[in] config: Immutable I2C1/I2C2 resources and publisher priority.
 * \param[in,out] receipt: Retained through failed cleanup.
 * \return Success or failure; close retains any unproved withdrawal.
 */
nx_result_t nx_stm32_i2c_async_prepare(nx_i2c_v1_state_t* port,
                                       const nx_stm32_i2c_config_t* config,
                                       nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Drain the asynchronous owner before releasing cold resources.
 * \param[in,out] port: Owner whose submitters and observers have joined.
 * \param[in] config: Unchanged selected resources.
 * \param[in,out] receipt: The same retained effects supplied to prepare.
 * \return Success or failure retaining loans, publishers and clock leases.
 * \note An empty receipt never authorizes replacing an existing bound owner.
 */
nx_result_t nx_stm32_i2c_async_close(nx_i2c_v1_state_t* port,
                                     const nx_stm32_i2c_config_t* config,
                                     nx_stm32_i2c_cold_receipt_t* receipt);

#ifdef __cplusplus
}
#endif
#endif
