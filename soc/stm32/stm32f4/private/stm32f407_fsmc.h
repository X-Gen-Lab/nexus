/**
 * \file            stm32f407_fsmc.h
 * \brief           One F407 FSMC owner with exact SRAM and LCD bank leases
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_FSMC_H
#define NEXUS_STM32F407_FSMC_H

#include "nexus/io/external_bus.h"
#include "nexus/io/memory_window.h"
#include "nexus/io/parallel16.h"
#include "stm32f407_fsmc_profile.h"
#include "stm32f407_platform_cold.h"
#include "stm32f407_sram_boot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Selected shared pads and exact bank configuration
 * \note            Platform has admitted/captured these pads and owns their
 *                  GPIO snapshots/gates until successful FSMC stop. This owner
 *                  acquires one AHB3 FSMC gate; no second bank owns that gate.
 *                  No concurrent CPU/DMA/external HAL/debugger access or
 *                  foreign running master is permitted during cold changes.
 *                  All selected bank enables must initially be clear.
 */
typedef struct {
    const nx_stm32_platform_pin_config_t* pins;
    size_t pin_count;
    const nx_stm32_fsmc_bank_config_t* banks;
    size_t bank_count;
    nx_clock_t clock;
    bool boot_backed;
    /** Optional exact bank_count array; NULL retains legacy NE3/NE4 facts. */
    const nx_stm32_fsmc_parameters_t* parameters;
    /** Generic profiles require exact LQFP package 100 or 144. */
    uint16_t package_pins;
} nx_stm32_fsmc_config_t;

struct nx_stm32_fsmc;

/**
 * \brief           One externally stored exclusive hardware-bank lease
 * \note            Stable address, fresh zero before acquire. Never copy a
 *                  live lease. SRAM exposes direct CPU memory, not an I/O
 *                  dispatch per load. Before release, the same owner joins
 *                  all CPU/DMA users and abandons every derived pointer.
 *                  Timeout alone never establishes quiescence. The monotonic
 *                  epoch must be captured when submitting a later release.
 */
typedef nx_memory_window_lease_t nx_stm32_fsmc_lease_t;

/** \brief Runtime storage exists only for each actually selected bank. */
typedef struct {
    nx_stm32_fsmc_lease_t* lease;
    uint32_t saved_control;
    uint32_t saved_read;
    uint32_t saved_write;
    bool captured;
} nx_stm32_fsmc_bank_state_t;

/**
 * \brief           One caller-owned bus and its exact selected state array
 * \note            Ordinary owners are fresh zero before first prepare.
 *                  Startup-backed owners use NX_STM32_FSMC_BOOT_OWNER at C
 *                  static initialization, never a later runtime assignment.
 *                  Configuration and state
 *                  arrays remain stable/disjoint through cleanup. Epoch is
 *                  preserved across stop and never wraps; exhausted state
 *                  retires. A partial prepare retains config/gate/bank effects
 *                  until stop proves withdrawal. Platform restores pads only
 *                  afterward. Ordinary prepare rejects boot_backed. Explicit
 *                  adoption preserves reset-owned SRAM/pads/gates for the
 *                  whole image lifetime; stop never withdraws that ownership.
 */
typedef struct nx_stm32_fsmc {
    const nx_stm32_fsmc_config_t* config;
    nx_stm32_fsmc_bank_state_t* banks;
    uint64_t epoch;
    bool added_gate;
    bool prepared;
    bool boot_owned;
} nx_stm32_fsmc_t;

/**
 * \brief           C11 static initializer for an explicitly selected boot bus
 * \note            Loaded before main; preserves reset-owned memory even if
 *                  Platform startup or later boot-proof admission fails. Stop
 *                  returns BUSY before any config is bound. Ordinary runtime
 *                  buses must use zero initialization instead.
 */
#define NX_STM32_FSMC_BOOT_OWNER                                               \
    { .boot_owned = true }

/** \brief Caller-owned settled LCD endpoint and its exclusive bank lease. */
typedef struct {
    nx_stm32_fsmc_lease_t lease;
    nx_time_us_t last_clock;
    bool in_call;
} nx_stm32_fsmc_lcd_t;

/**
 * \brief           Acquire one FSMC gate and configure admitted shared pads
 * \param[in,out]   bus: Zero or completely stopped persistent bus owner.
 * \param[in]       config: Stable exact banks/pads/timings/clock facts.
 * \param[in,out]   banks: Fresh zero array of exactly config bank_count states.
 * \param[in]       bank_count: Exact supplied runtime-state array length.
 * \return          SUCCESS, malformed/context/busy rejection or IO. A failed
 *                  acquisition may retain effects; call stop while config and
 *                  state remain alive. No GPIO snapshot is duplicated.
 * \note            Privileged, unmasked Thread mode; no hardware reset, probe,
 *                  SRAM clear/test, heap, scheduler or implicit memory use.
 */
nx_result_t nx_stm32_fsmc_prepare(nx_stm32_fsmc_t* bus,
                                  const nx_stm32_fsmc_config_t* config,
                                  nx_stm32_fsmc_bank_state_t* banks,
                                  size_t bank_count);

/**
 * \brief           Adopt SRAM already prepared by the exact reset-only plan
 * \param[in,out]   bus: Static BOOT_OWNER receipt in internal RAM, no other
 *                  runtime ownership, effects or prior epochs.
 * \param[in]       config: Boot-backed exact union; NE3 is plan's same bank.
 * \param[in,out]   banks: Fresh exact per-selected-bank internal runtime state.
 * \param[in]       bank_count: Exactly config bank_count.
 * \param[in]       plan: Original immutable internal-Flash reset plan.
 * \return          SUCCESS after read-only boot proof and optional NE4 setup;
 *                  rejection without effects before proof/admission. A failure
 *                  after adoption permanently retains the boot-owned receipt.
 * \note            Boot-owned SRAM pads carry NULL snapshots; only NE4-only
 *                  PG12 has an ordinary Platform snapshot. C runtime globals
 *                  outlive every loan. This bus cannot be cleanly stopped;
 *                  reset is the only supported whole-image withdrawal.
 *                  The selected build links fsmc_boot.c and real boot proof.
 */
nx_result_t nx_stm32_fsmc_adopt_boot(nx_stm32_fsmc_t* bus,
                                     const nx_stm32_fsmc_config_t* config,
                                     nx_stm32_fsmc_bank_state_t* banks,
                                     size_t bank_count,
                                     const nx_stm32_sram_boot_plan_t* plan);

/**
 * \brief           Private core entry reached only after actual boot proof
 * \param[in,out]   bus: Same internal persistent owner as adopt_boot.
 * \param[in]       config: Exact boot-backed selected facts.
 * \param[in,out]   banks: Fresh exact internal receipt storage.
 * \param[in]       bank_count: Exact selected state count.
 * \param[in]       plan: Plan for which boot_matches just returned true.
 * \return          Exact admission/configuration result.
 * \note            Provider-internal ABI, never emitted or called by factory.
 *                  fsmc_boot.c alone performs the real register proof before
 *                  this entry; this core still enforces descriptor identity,
 *                  exact boot pad ownership and permanent lifetime retention.
 */
nx_result_t nx_stm32_fsmc_adopt_proven(nx_stm32_fsmc_t* bus,
                                       const nx_stm32_fsmc_config_t* config,
                                       nx_stm32_fsmc_bank_state_t* banks,
                                       size_t bank_count,
                                       const nx_stm32_sram_boot_plan_t* plan);

/**
 * \brief           Disable/restore banks and withdraw only the added gate
 * \param[in,out]   bus: Same serialized owner with every bank lease released.
 * \return          SUCCESS; BUSY while a lease or boot receipt survives;
 *                  error retaining exact
 *                  unfinished bank/gate effects. A deadline never releases
 *                  retained CPU/DMA references. Unselected banks are unchanged.
 * \note            Platform may restore its shared GPIO lease only afterward.
 */
nx_result_t nx_stm32_fsmc_stop(nx_stm32_fsmc_t* bus);

/**
 * \brief           Borrow the selected NE3 one-MiB SRAM direct-access window
 * \param[in,out]   bus: Prepared same-owner bus with NE3 selected.
 * \param[in,out]   lease: Fresh zero, distinct stable caller storage.
 * \return          SUCCESS with 68000000h/one-MiB or rejection without writes;
 *                  UNSUPPORTED for a boot-backed bank with live globals.
 * \note            CPU accessibility does not authorize a DMA master, startup
 *                  data/bss placement, executable storage or physical fitting.
 */
nx_result_t nx_stm32_fsmc_sram_acquire(nx_stm32_fsmc_t* bus,
                                       nx_stm32_fsmc_lease_t* lease);

/** \brief Borrow one explicitly selected bank through the same FSMC owner.
 * \param[in,out] bus: Prepared serialized privileged Thread owner.
 * \param[in] index: Selected configuration-array index, not a bank number.
 * \param[in,out] lease: Fresh zero, internal stable distinct storage.
 * \return SUCCESS or rejection without retained references or new effects.
 * \note Generic NOR/NAND command owners keep this loan until actual idle.
 */
nx_result_t nx_stm32_fsmc_acquire(nx_stm32_fsmc_t* bus, size_t index,
                                  nx_stm32_fsmc_lease_t* lease);

/** \brief Validate an exact lease and observe its selected decoder/gate.
 * \param[in] lease: Same stable storage and immutable granted geometry.
 * \return SUCCESS, STATE for stale lease or IO for missing hardware.
 * \note Privileged serialized Thread call, no writes or ownership change.
 */
nx_result_t nx_stm32_fsmc_check(const nx_stm32_fsmc_lease_t* lease);

/** \brief Validate a command endpoint/payload against all bus control spans.
 * \param[in] bus: Prepared serialized unique owner.
 * \param[in] storage: Complete representable internal/Flash span.
 * \param[in] bytes: Positive extent.
 * \param[in] writable: Reject physical Flash for mutable spans.
 * \return True only for disjoint admitted storage; no hardware access.
 * \note External payload memory requires separate lifetime handling and is
 *       deliberately excluded by the finite command endpoints.
 */
bool nx_stm32_fsmc_storage(const nx_stm32_fsmc_t* bus, const void* storage,
                           size_t bytes, bool writable);

/** \brief Immutable endpoint context for a selected PSRAM memory window. */
typedef struct {
    nx_stm32_fsmc_t* bus;
    size_t index;
} nx_stm32_fsmc_window_t;

/** \brief Same bus owner, context is an immutable selected bank binding. */
extern const nx_memory_window_ops_t nx_stm32_fsmc_window_ops;

/**
 * \brief           Return an exact SRAM or LCD bank lease after caller join
 * \param[in,out]   lease: Exact stable storage originally granted by acquire.
 * \param[in]       epoch: Originally captured generation, never a guessed one.
 * \return          SUCCESS or stale/invalid rejection without affecting users.
 * \note            The caller proves all derived CPU/DMA/component users have
 *                  joined. A compiler/hardware DSB precedes publication of
 *                  release, but cannot establish a missing DMA join. A copied
 *                  lease cannot release the original slot.
 */
nx_result_t nx_stm32_fsmc_release(nx_stm32_fsmc_lease_t* lease, uint64_t epoch);

/**
 * \brief           Borrow NE4 for an already statically connected endpoint
 * \param[in,out]   bus: Prepared same-owner bus with NE4 selected.
 * \param[in,out]   lcd: Fresh zero stable endpoint storage.
 * \return          SUCCESS or rejection without publishing a face.
 * \note            A12 and 16-bit width map command 6C000000h/data 6C002000h.
 *                  Stop every component before closing the endpoint. Word
 *                  loops check time every 32 words and retain no spans.
 */
nx_result_t nx_stm32_fsmc_lcd_prepare(nx_stm32_fsmc_t* bus,
                                      nx_stm32_fsmc_lcd_t* lcd);

/**
 * \brief           Close the exact LCD lease after all component users join
 * \param[in,out]   lcd: Same serialized endpoint, no call still in progress.
 * \return          SUCCESS for closed state, or exact retained-loan failure.
 * \note            This changes no immutable face and does not stop the bus.
 */
nx_result_t nx_stm32_fsmc_lcd_close(nx_stm32_fsmc_lcd_t* lcd);

/** \brief Shared immutable tables; their context is the actual bus or LCD. */
extern const nx_external_bus_ops_t nx_stm32_fsmc_external_bus_ops;
extern const nx_parallel16_ops_t nx_stm32_fsmc_parallel16_ops;
extern const nx_memory_window_ops_t nx_stm32_fsmc_memory_window_ops;

#ifdef __cplusplus
}
#endif
#endif
