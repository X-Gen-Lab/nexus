/**
 * \file            stm32f407_platform_cold.h
 * \brief           Exact Platform admission and retained shared resources
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_PLATFORM_COLD_H
#define NX_STM32F407_PLATFORM_COLD_H

#include "stm32f407_provider.h"

/** \brief Constant-expression membership in the three F407 NVIC banks. */
#define NX_STM32_PLATFORM_IRQ_MASK(irq, bank)                                  \
    (((uint32_t)(irq) / 32U == (bank))                                         \
         ? (UINT32_C(1) << ((uint32_t)(irq) % 32U))                            \
         : UINT32_C(0))

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One selected pad and its unique persistent snapshot storage. */
typedef struct {
    GPIO_TypeDef* registers;
    uint16_t* snapshot;
    uint8_t port_index;
    uint8_t pin;
} nx_stm32_platform_pin_config_t;

/** \brief Immutable physical selections, never an action or opcode stream.
 * \note GPIO/DMA and IRQ masks describe exact selected resources. No pointer
 *       in this record is a callback or an arbitrary writable register port.
 */
typedef struct {
    const nx_stm32_platform_pin_config_t* pins;
    size_t pin_count;
    DMA_Stream_TypeDef* const* dma_streams;
    size_t dma_stream_count;
    uint32_t apb1_clocks;
    uint32_t apb2_clocks;
    uint32_t ahb1_clocks;
    uint32_t ahb2_clocks;
    uint32_t irq_masks[3];
    uint32_t exti_lines;
} nx_stm32_platform_config_t;

/** \brief The shared GPIO owner; each pad value exists in only one slot. */
typedef struct {
    uint32_t clocks_added;
    bool captured;
} nx_stm32_platform_pin_receipt_t;

/** \brief Reject selected foreign owners without changing hardware.
 * \param[in] config: Exact reviewed static resources; base system is admitted.
 * \return Success or invalid/context/busy, with no acquired effects.
 * \note Privileged unmasked Thread mode. The system preflight runs first.
 */
nx_result_t
nx_stm32_platform_selected_preflight(const nx_stm32_platform_config_t* config);

/** \brief Observe powered pads before capture or any peripheral constructor.
 * \param[in] config: Unchanged static resources and unique snapshot slots.
 * \param[in] powered_only: Ignore banks whose clock has not been acquired.
 * \return Success or invalid/context/busy without writes or snapshot changes.
 */
nx_result_t
nx_stm32_platform_pins_admit(const nx_stm32_platform_config_t* config,
                             bool powered_only);

/** \brief Acquire all shared GPIO clocks and capture admitted original pads.
 * \param[in] config: Exact sorted pads; shared peripheral calls use these
 * slots. \param[in,out] receipt: Initially zero, retained after failed clock
 * cleanup. \return Success or failure; acquired clocks require release or
 * retry. \note No snapshot is accepted until every selected pad passes
 * admission.
 */
nx_result_t
nx_stm32_platform_pins_acquire(const nx_stm32_platform_config_t* config,
                               nx_stm32_platform_pin_receipt_t* receipt);

/** \brief Discard untouched snapshots after system failure, before
 * constructors. \param[in,out] receipt: Same receipt; no peripheral changed any
 * selected pad. \return Success or failure retaining only unproved GPIO gate
 * effects.
 */
nx_result_t
nx_stm32_platform_pins_abort(nx_stm32_platform_pin_receipt_t* receipt);

/** \brief Withdraw all selected outputs before restoring original pad values.
 * \param[in] config: Same immutable resource set supplied to acquisition.
 * \param[in,out] receipt: Same owner, retained until restore and gates read
 * back. \return Success or failure retaining snapshots and clocks for later
 * retry. \note Submitters, publishers, loans and all peripheral callers have
 * joined.
 */
nx_result_t
nx_stm32_platform_pins_release(const nx_stm32_platform_config_t* config,
                               nx_stm32_platform_pin_receipt_t* receipt);

/** \brief Release only GPIO gates actually added by this shared owner.
 * \param[in,out] receipt: No captured pad may remain when gates are withdrawn.
 * \return Success or failure retaining refused gate bits for retry.
 */
nx_result_t
nx_stm32_platform_gpio_clocks_release(nx_stm32_platform_pin_receipt_t* receipt);

/** \brief Acquire the selected DMA1/DMA2 interface clocks with readback.
 * \param[in] mask: Nonzero subset of the two DMA clock bits.
 * \param[in,out] added: Initially zero, actual attempted additions on failure.
 * \return Success or invalid/context/state/IO; failure still requires release.
 */
nx_result_t nx_stm32_platform_dma_acquire(uint32_t mask, uint32_t* added);

/** \brief Release added DMA gates after every selected engine and IRQ drains.
 * \param[in,out] added: Retained until all selected gate removals read back.
 * \return Success or failure; no stream or unrelated clock is modified.
 */
nx_result_t nx_stm32_platform_dma_release(uint32_t* added);

/** \brief Acquire the shared SYSCFG clock before selected EXTI constructors.
 * \param[in,out] added: Initially zero, retained until observed release.
 * \return Success or invalid/context/state/IO without changing existing gates.
 */
nx_result_t nx_stm32_platform_syscfg_acquire(uint32_t* added);

/** \brief Release only this owner's added SYSCFG gate after EXTI withdrawal.
 * \param[in,out] added: Same state supplied to acquisition.
 * \return Success or failure retaining refused withdrawal for retry.
 */
nx_result_t nx_stm32_platform_syscfg_release(uint32_t* added);

#ifdef __cplusplus
}
#endif
#endif
