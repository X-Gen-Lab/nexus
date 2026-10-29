/**
 * \file            gd32f470_platform_cold.h
 * \brief           Exact Platform admission and retained shared resources
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_PLATFORM_COLD_H
#define NX_GD32F470_PLATFORM_COLD_H

#include "gd32f470_provider.h"

/** \brief Constant-expression membership in the three GD470 NVIC banks. */
#define NX_GD32_PLATFORM_IRQ_MASK(irq, bank)                                   \
    (((uint32_t)(irq) / 32U == (bank))                                         \
         ? (UINT32_C(1) << ((uint32_t)(irq) % 32U))                            \
         : UINT32_C(0))

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One selected pad and its unique persistent snapshot storage. */
typedef struct {
    uint32_t registers;
    uint16_t* snapshot;
    uint8_t port_index;
    uint8_t pin;
} nx_gd32_platform_pin_config_t;

/** \brief One exact DMA controller and channel, without register opcodes. */
typedef struct {
    uint32_t registers;
    uint8_t channel;
} nx_gd32_platform_dma_lane_t;

/** \brief Immutable physical selections, never an action or opcode stream.
 * \note GPIO/DMA and IRQ masks describe exact selected resources. No pointer
 *       in this record is a callback or an arbitrary writable register port.
 */
typedef struct {
    const nx_gd32_platform_pin_config_t* pins;
    size_t pin_count;
    const nx_gd32_platform_dma_lane_t* dma_lanes;
    size_t dma_lane_count;
    uint32_t apb1_clocks;
    uint32_t apb2_clocks;
    uint32_t ahb1_clocks;
    uint32_t ahb2_clocks;
    uint32_t irq_masks[3];
    uint32_t exti_lines;
} nx_gd32_platform_config_t;

/** \brief The shared GPIO owner; each pad value exists in only one slot. */
typedef struct {
    uint32_t clocks_added;
    bool captured;
} nx_gd32_platform_pin_receipt_t;

/** \brief Reject selected foreign owners without changing hardware.
 * \param[in] config: Exact reviewed static resources; base system is admitted.
 * \return Success or invalid/context/busy, with no acquired effects.
 * \note Privileged unmasked Thread mode. The system preflight runs first.
 */
nx_result_t
nx_gd32_platform_selected_preflight(const nx_gd32_platform_config_t* config);

/** \brief Observe powered pads before capture or any peripheral constructor.
 * \param[in] config: Unchanged static resources and unique snapshot slots.
 * \param[in] powered_only: Ignore banks whose clock has not been acquired.
 * \return Success or invalid/context/busy without writes or snapshot changes.
 */
nx_result_t nx_gd32_platform_pins_admit(const nx_gd32_platform_config_t* config,
                                        bool powered_only);

/** \brief Acquire all shared GPIO clocks and capture admitted original pads.
 * \param[in] config: Exact sorted pads; shared peripheral calls use these
 * slots. \param[in,out] receipt: Initially zero, retained after failed clock
 * cleanup. \return Success or failure; acquired clocks require release or
 * retry. \note No snapshot is accepted until every selected pad passes
 * admission.
 */
nx_result_t
nx_gd32_platform_pins_acquire(const nx_gd32_platform_config_t* config,
                              nx_gd32_platform_pin_receipt_t* receipt);

/** \brief Discard untouched snapshots after system failure, before
 * constructors. \param[in,out] receipt: Same receipt; no peripheral changed any
 * selected pad. \return Success or failure retaining only unproved GPIO gate
 * effects.
 */
nx_result_t
nx_gd32_platform_pins_abort(nx_gd32_platform_pin_receipt_t* receipt);

/** \brief Withdraw all selected outputs before restoring original pad values.
 * \param[in] config: Same immutable resource set supplied to acquisition.
 * \param[in,out] receipt: Same owner, retained until restore and gates read
 * back. \return Success or failure retaining snapshots and clocks for later
 * retry. \note Submitters, publishers, loans and all peripheral callers have
 * joined.
 */
nx_result_t
nx_gd32_platform_pins_release(const nx_gd32_platform_config_t* config,
                              nx_gd32_platform_pin_receipt_t* receipt);

/** \brief Release only GPIO gates actually added by this shared owner.
 * \param[in,out] receipt: No captured pad may remain when gates are withdrawn.
 * \return Success or failure retaining refused gate bits for retry.
 */
nx_result_t
nx_gd32_platform_gpio_clocks_release(nx_gd32_platform_pin_receipt_t* receipt);

/** \brief Acquire the selected DMA0/DMA1 interface clocks with readback.
 * \param[in] mask: Nonzero subset of the two DMA clock bits.
 * \param[in,out] added: Initially zero, actual attempted additions on failure.
 * \return Success or invalid/context/state/IO; failure still requires release.
 */
nx_result_t nx_gd32_platform_dma_acquire(uint32_t mask, uint32_t* added);

/** \brief Release added DMA gates after every selected engine and IRQ drains.
 * \param[in,out] added: Retained until all selected gate removals read back.
 * \return Success or failure; no stream or unrelated clock is modified.
 */
nx_result_t nx_gd32_platform_dma_release(uint32_t* added);

#ifdef __cplusplus
}
#endif
#endif
