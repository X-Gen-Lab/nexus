/**
 * \file            stm32g474_fdcan.h
 * \brief           Private fixed G474 reduced M_CAN ownership boundary
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_FDCAN_H
#define NX_STM32G474_FDCAN_H
#include "nexus/arch/arch.h"
#include "nexus/io/can_fd.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Physical 212-word RAM lease, never arbitrary caller SRAM. */
typedef struct {
    RCC_TypeDef* rcc;
    FDCAN_Config_TypeDef* configuration;
    volatile uint32_t* ram;
    uint32_t kernel_hz;
    uint8_t acquired_mask;
    bool retained;
} nx_stm32g474_fdcan_domain_t;
typedef struct {
    nx_can_fd_tx_request_t* request;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    bool added;
} nx_stm32g474_fdcan_tx_slot_t;
/** \brief Fixed ISO FD profiles, immutable throughout a retained epoch. */
typedef enum {
    NX_STM32G474_FDCAN_INTERNAL_LOOPBACK = 0,
    NX_STM32G474_FDCAN_NORMAL = 1,
    NX_STM32G474_FDCAN_MODE_COUNT = 2
} nx_stm32g474_fdcan_mode_t;
/* Exactly three physical TX elements, not a software maximum object pool. */
typedef struct {
    FDCAN_GlobalTypeDef* registers;
    nx_stm32g474_fdcan_domain_t* domain;
    const nx_can_fd_filter_t* filters;
    size_t filter_count;
    nx_can_fd_event_t* events;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t lost;
    nx_time_us_t loss_time;
    uint32_t loss_flags;
    nx_stm32g474_fdcan_tx_slot_t slots[3];
    const nx_irq_wake_t* wake;
    nx_irq_policy_t irq_policy;
    nx_irq_source_t sources[2];
    uint32_t drain_timeout_us;
    uint32_t poll_limit;
    uint32_t wake_publishers;
    nx_can_fd_state_t snapshot;
    nx_stm32g474_fdcan_mode_t mode;
    bool retained;
    /* Separate cold ownership survives public stop and failed initialization.
     */
    bool cold_domain_owned;
} nx_stm32g474_fdcan_state_t;
/** \brief Immutable exact FDCAN1 domain, rings and reviewed mode. */
typedef struct {
    nx_stm32g474_fdcan_domain_t* domain;
    const nx_can_fd_filter_t* filters;
    size_t filter_count;
    nx_can_fd_event_t* events;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    uint32_t drain_timeout_us;
    uint32_t poll_limit;
    nx_stm32g474_fdcan_mode_t mode;
} nx_stm32g474_fdcan_config_t;
/** \brief Bind and initialize the domain/controller before connecting TX.
 * \param[in,out] state: Static unretained physical controller state.
 * \param[in] config: Reviewed immutable domain/filter/ring/poll resources.
 * \return Status; close and Platform restore retain partial-failure effects.
 */
nx_result_t
nx_stm32g474_fdcan_prepare(nx_stm32g474_fdcan_state_t* state,
                           const nx_stm32g474_fdcan_config_t* config);
/** \brief Release only the domain acquired by this cold preparation.
 * \param[in,out] state: Same static controller with submitters joined.
 * \param[in] config: Original static domain/configuration association.
 * \return Success when this preparation owns no effects, or after its own
 *         controller and domain release complete. Rejected foreign ownership
 *         is never adopted or released.
 */
nx_result_t nx_stm32g474_fdcan_close(nx_stm32g474_fdcan_state_t* state,
                                     const nx_stm32g474_fdcan_config_t* config);
/** \brief Shared readonly optional table, independent of classic CAN storage.
 */
extern const nx_can_fd_ops_t nx_stm32g474_fdcan_ops;
/**
 * \brief Acquire the exact fixed message RAM domain after Platform clock setup.
 * \param[in,out] domain: Static physical RCC/config/RAM lease; kernel24MHz.
 * \return Success, INVALID, STATE, BUSY or CONTEXT with no hardware changes.
 * \note Privileged unmasked task. Reject live peers; never adopt another owner.
 *       Platform keeps FDCANEN through controller stop and domain release.
 */
nx_result_t
nx_stm32g474_fdcan_domain_initialize(nx_stm32g474_fdcan_domain_t* domain);
/**
 * \brief Release domain responsibility only after all acquired instances stop.
 * \param[in,out] domain: Same static domain, preserved across failed cleanup.
 * \return Success or BUSY while a controller retains effects or borrowed data.
 * \note Task-only; no shared RCC reset or clock disable occurs here.
 */
nx_result_t nx_stm32g474_fdcan_domain_stop(nx_stm32g474_fdcan_domain_t* domain);
/**
 * \brief Start fixed FDCAN1 ISO FD normal or internal-loopback operation.
 * \param[in,out] state: Static controller, acquired domain, filters and ring.
 * \return Success or failure retaining any acquired effects for stop/retry.
 * \note Filters/ring remain live through successful stop. Immutable binding
 *       fields cannot change while retained. Both reserved vectors are quiet
 *       before acquisition; only line0 publishes at numerical priority5.
 */
nx_result_t nx_stm32g474_fdcan_initialize(nx_stm32g474_fdcan_state_t* state);
/**
 * \brief Service bounded three-slot/six-frame facts and signal a borrowed sink.
 * \param[in,out] state: Exact live instance bound to FDCAN1 IT0.
 * \note Privileged configurable IRQ or serialized task service. Metadata is
 *       protected by local interrupt exclusion; notifications run outside it.
 *       This entry does not qualify external electrical wiring or HIL timing.
 */
void nx_stm32g474_fdcan_irq(nx_stm32g474_fdcan_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
