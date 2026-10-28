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

/** \brief Effects retained until every pad and publisher is restored. */
typedef struct {
    uint32_t gpio_clock;
    uint32_t peripheral_clock;
    uint8_t priorities[4];
    uint8_t captured;
    bool owned;
    bool vectors;
    bool prepared;
} nx_stm32_i2c_cold_receipt_t;

/** \brief Acquire only exact resources in privileged unmasked Thread mode.
 * \param[in] config: Immutable resources with two distinct snapshot slots.
 * \param[in,out] receipt: Cold owner retained after unsuccessful restoration.
 * \return Success or rejection; acquired effects require cold release.
 * \note Platform first admits the pads and serializes the shared GPIO clock
 *       lease. It keeps these same snapshots until final resource cleanup.
 */
nx_result_t nx_stm32_i2c_cold_acquire(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Restore acquired pads, publishers and clock gates after drain.
 * \param[in] config: The same immutable resources used for acquisition.
 * \param[in,out] receipt: Cleared only after observed restoration.
 * \return Success or failure retaining cleanup responsibility for retry.
 * \note The caller first stops the selected engine and all its publishers.
 */
nx_result_t nx_stm32_i2c_cold_release(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt);

/** \brief Observe selected clocks and every initialized publisher.
 * \param[in] config: The unchanged selected resources.
 * \param[in] receipt: The prepared owner retaining all acquired effects.
 * \return Success only when gates, priorities and enabled sources match.
 * \note Failure does not erase effects; the concrete owner must close them.
 */
nx_result_t
nx_stm32_i2c_cold_verify(const nx_stm32_i2c_config_t* config,
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
 */
nx_result_t nx_stm32_i2c_async_close(nx_i2c_v1_state_t* port,
                                     const nx_stm32_i2c_config_t* config,
                                     nx_stm32_i2c_cold_receipt_t* receipt);

#ifdef __cplusplus
}
#endif
#endif
