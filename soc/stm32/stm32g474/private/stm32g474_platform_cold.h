/**
 * \file            stm32g474_platform_cold.h
 * \brief           Exact static Platform admission, pad and clock leases
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_PLATFORM_COLD_H
#define NX_STM32G474_PLATFORM_COLD_H
#include "stm32g474_system.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Constant selected IRQ membership in the four exact NVIC banks. */
#define NX_G474_PLATFORM_IRQ_MASK(irq, bank)                                   \
    (((uint32_t)(irq) / 32U == (bank))                                         \
         ? (UINT32_C(1) << ((uint32_t)(irq) % 32U))                            \
         : UINT32_C(0))
/** \brief One physical pad and its sole persistent original image slot. */
typedef struct {
    GPIO_TypeDef* gpio;
    uint16_t* snapshot;
    uint8_t port;
    uint8_t pin;
} nx_stm32g474_platform_pin_t;
/** \brief Shared pad owner; no second pin-state image is retained. */
typedef struct {
    uint32_t clocks_added;
    bool captured;
} nx_stm32g474_platform_pins_t;
/** \brief Static physical selections and selected-only RAM receipt slots.
 * \note These named fields describe exact resources, never writable register
 *       addresses, action opcodes or an interpreted peripheral program.
 *       Receipt pointers name ordinary static RAM, not MMIO.
 */
typedef struct {
    const nx_stm32g474_platform_pin_t* pins;
    size_t pin_count;
    uint32_t admit_apb1;
    uint32_t admit_apb1_2;
    uint32_t admit_apb2;
    uint32_t admit_ahb1;
    uint32_t admit_ahb2;
    uint32_t admit_reset_apb1;
    uint32_t admit_mux;
    uint32_t irq_masks[4];
    uint32_t exti_lines;
    uint32_t clocks_apb1;
    uint32_t clocks_apb1_2;
    uint32_t clocks_apb2;
    uint32_t clocks_ahb1;
    uint32_t clocks_ahb2;
    uint32_t* added_apb1;
    uint32_t* added_apb1_2;
    uint32_t* added_apb2;
    uint32_t* added_ahb1;
    uint32_t* added_ahb2;
    uint32_t* added_syscfg;
    bool* owned_i2c_mux;
    bool* owned_adc_mux;
    bool control_group;
    bool dma_channel1;
    bool strict_dma_channel1;
    bool flash;
} nx_stm32g474_platform_config_t;
/** \brief Reject selected foreign owners before acquiring hardware effects.
 * \param[in] config: Static reviewed resources; base system preflight passed.
 * \return Success or invalid/context/busy without writes or new ownership.
 */
nx_result_t
nx_stm32g474_platform_preflight(const nx_stm32g474_platform_config_t* config);
/** \brief Admit only input/analog unlocked pads in readable selected banks.
 * \param[in] config: Same immutable pad and unique snapshot records.
 * \param[in] powered_only: Do not access pads behind unacquired bank clocks.
 * \return Status without writes; clock/reset agreement is sampled per access.
 */
nx_result_t
nx_stm32g474_platform_pins_admit(const nx_stm32g474_platform_config_t* config,
                                 bool powered_only);
/** \brief Acquire GPIO gates and capture every admitted original exactly once.
 * \param[in] config: Static pads and unique snapshot slots remain live.
 * \param[in,out] receipt: Zero or successfully released shared owner.
 * \return Status; any added clock remains owned until proved release/retry.
 */
nx_result_t
nx_stm32g474_platform_pins_acquire(const nx_stm32g474_platform_config_t* config,
                                   nx_stm32g474_platform_pins_t* receipt);
/** \brief Release only GPIO clocks added by an owner with no captured pads.
 * \param[in,out] receipt: Same static retained receipt.
 * \return Status; refused readback keeps the original clock mask for retry.
 */
nx_result_t nx_stm32g474_platform_gpio_clocks_release(
    nx_stm32g474_platform_pins_t* receipt);
/** \brief Discard untouched snapshots after System failed before constructors.
 * \param[in,out] receipt: No peripheral has changed a selected pad.
 * \return Status retaining unproved GPIO gate effects.
 */
nx_result_t
nx_stm32g474_platform_pins_abort(nx_stm32g474_platform_pins_t* receipt);
/** \brief Withdraw all selected drivers, restore originals, release own gates.
 * \param[in] config: Original static connections; peripheral users joined.
 * \param[in,out] receipt: Same owner, preserved through failed cleanup/retry.
 * \return Success only after every restoration and gate readback completes.
 */
nx_result_t
nx_stm32g474_platform_pins_release(const nx_stm32g474_platform_config_t* config,
                                   nx_stm32g474_platform_pins_t* receipt);
/** \brief Acquire exact muxes and peripheral clocks, preserving partial state.
 * \param[in] config: Same static resources and selected receipt slots.
 * \note Privileged unmasked Thread context; pending gate or mux receipts
 *       reject replacement before any hardware effect.
 * \return Primary status; caller must run ordinary Platform cleanup on failure.
 */
nx_result_t nx_stm32g474_platform_resources_start(
    const nx_stm32g474_platform_config_t* config);
/** \brief Release selected clocks/muxes after providers and pads are quiet.
 * \param[in] config: Same immutable resources and retained receipt slots.
 * \note Privileged unmasked Thread context; failed release remains retryable.
 * \return Success or failure with every unproved slot retained for retry.
 */
nx_result_t nx_stm32g474_platform_resources_stop(
    const nx_stm32g474_platform_config_t* config);
#ifdef __cplusplus
}
#endif
#endif
