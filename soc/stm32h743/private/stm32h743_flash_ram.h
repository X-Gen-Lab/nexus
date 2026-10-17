/**
 * \file            stm32h743_flash_ram.h
 * \brief           Private H743 RAM maintenance and exception responsibility
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_FLASH_RAM_H
#define NX_STM32H743_FLASH_RAM_H
#include "stm32h743_flash_mpu.h"
#include "stm32h743_flash_ram_layout.h"
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief RAM-only terminal reasons; no Flash string or observer dependency. */
typedef enum {
    NX_H743_FLASH_RAM_FAULT_NONE = 0,
    NX_H743_FLASH_RAM_FAULT_DRAIN = 1,
    NX_H743_FLASH_RAM_FAULT_LOCK = 2,
    NX_H743_FLASH_RAM_FAULT_TIME = 3,
    NX_H743_FLASH_RAM_FAULT_EXCEPTION = 4
} nx_stm32h743_flash_ram_fault_t;
/**
 * \brief Selected physical-controller workspace; cold owner derives every
 *        binding and command field before the bridge can enter this pod.
 * \note The generated instance requires 1024-byte alignment for VTOR. The
 *       dedicated stack closes Thread MSP/PSP and exception stacking without
 *       trusting an arbitrary application stack extent. sizeof is not an
 *       actual linked RAM-cost claim. No instance exists without Flash.
 *       Its private state cannot serve as a caller-authored execution proof.
 */
struct nx_stm32h743_flash_ram_workspace {
    uint32_t vectors[NX_H743_FLASH_RAM_VECTOR_WORDS];
    uint32_t stack[NX_H743_FLASH_RAM_STACK_WORDS];
    nx_stm32h743_flash_state_t* port;
    nx_stm32h743_system_t* system;
    volatile uint32_t* destination;
    uint32_t words[8];
    uint32_t configuration;
    uint32_t saved_control;
    uint32_t bank;
    uint32_t fault_status;
    uint32_t fault_exception;
    nx_stm32h743_flash_ram_fault_t fault;
    bool program;
    bool prepared;
    bool active;
    uint32_t saved_vtor;
    nx_time_us_t deadline;
    nx_stm32h743_flash_mpu_state_t guard;
};
#if defined(__cplusplus)
static_assert(offsetof(nx_stm32h743_flash_ram_workspace_t, vectors) == 0,
              "RAM vectors must begin the physical workspace");
static_assert(offsetof(nx_stm32h743_flash_ram_workspace_t, stack) +
                      sizeof(((nx_stm32h743_flash_ram_workspace_t*)0)->stack) ==
                  (size_t)NX_H743_FLASH_RAM_STACK_TOP_BYTES,
              "RAM bridge stack prefix differs from C layout");
#else
_Static_assert(offsetof(nx_stm32h743_flash_ram_workspace_t, vectors) == 0,
               "RAM vectors must begin the physical workspace");
_Static_assert(
    offsetof(nx_stm32h743_flash_ram_workspace_t, stack) +
            sizeof(((nx_stm32h743_flash_ram_workspace_t*)0)->stack) ==
        (size_t)NX_H743_FLASH_RAM_STACK_TOP_BYTES,
    "RAM bridge stack prefix differs from C layout");
#endif
/**
 * \brief Copy the exact linked pod and prepare its private exception vectors.
 * \param[in,out] port: Exclusive cold physical controller, with no active
 *        command. The generated Platform owns the physical lease.
 * \param[in,out] execution: Zeroed, 1024-aligned selected AXI workspace, fully
 *        disjoint from controller, System, MMIO and linked execution images.
 * \return SUCCESS only after copy readback and default NMI/HardFault alias
 *         admission; otherwise error without published execution or VTOR.
 * \note Privileged unmasked Thread, Cache-off, no live lazy-FP frame and no
 *       active foreign MPU region. Constructor never starts a physical pulse.
 *       Preparation does not waive the final live CPU/protection admission.
 */
nx_result_t nx_stm32h743_flash_ram_initialize(
    nx_stm32h743_flash_state_t* port,
    nx_stm32h743_flash_ram_workspace_t* execution);
/**
 * \brief Release a prepared workspace only after both banks and owners join.
 * \param[in,out] execution: The exact workspace associated with its port.
 * \return SUCCESS on release or an empty zeroed workspace, BUSY while retained,
 *         or STATE for a stale owner. An empty stop never releases another
 *         physical controller's prepared workspace.
 * \note Does not change VTOR, protection, clocks or another workspace.
 */
nx_result_t
nx_stm32h743_flash_ram_stop(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Reobserve raw CPU/protection before any command staging or borrow.
 * \note This is private cold-context admission, never a runtime registry.
 */
nx_result_t
nx_stm32h743_flash_ram_admission(const nx_stm32h743_flash_state_t* port);
/** \brief Reject every prepared workspace/code alias using whole spans. */
bool nx_stm32h743_flash_ram_external(const nx_stm32h743_flash_state_t* port,
                                     const void* value, size_t bytes);
/** \brief An internal RAM return cannot establish an unproved Flash LOCK. */
typedef enum {
    NX_H743_FLASH_RAM_PULSE_SUCCESS = 0,
    NX_H743_FLASH_RAM_PULSE_IO = 1,
    NX_H743_FLASH_RAM_PULSE_LOCK_UNPROVEN = 2
} nx_stm32h743_flash_ram_pulse_outcome_t;
/**
 * \brief Observe TIM2 and acknowledge each proven overflow before counting it.
 * \param[in,out] execution: Exact active RAM transaction and System owner.
 * \return Current absolute time in the existing epoch; invalid acknowledgement
 *         or epoch exhaustion remains in the RAM terminal path.
 * \note No ordinary time callback or Arch masking helper is reachable.
 */
nx_time_us_t
nx_stm32h743_flash_ram_time(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Prove both banks idle and restore/observe only the target LOCK.
 * \note False returns only to a RAM caller retaining every guard effect.
 */
bool nx_stm32h743_flash_ram_lock(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Validate the live internal RAM stack, mask, mode and vector entry.
 */
nx_result_t nx_stm32h743_flash_ram_context(
    const nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Nonreturning RAM diagnostic path; retains controller and workspace.
 */
void nx_stm32h743_flash_ram_terminal(
    nx_stm32h743_flash_ram_workspace_t* execution,
    nx_stm32h743_flash_ram_fault_t reason) __attribute__((noreturn));
/**
 * \brief Trigger once and retain RAM until both physical banks actually idle.
 * \param[in,out] execution: Exact command already admitted under the MPU guard.
 * \param[in] deadline: Finite absolute TIM2/epoch deadline.
 * \param[out] timed_out: Explicit exclusive RAM result slot, set on expiry.
 * \return SUCCESS or IO only with observed LOCK, or LOCK_UNPROVEN exclusively
 *         to a RAM caller which must retain protection and terminate there.
 * \note Expiry suppresses a not-yet-issued trigger or latches a late pulse;
 *       it never aborts physical drain. poll_limit cannot end this loop.
 *       No function pointer, hidden mode flag or workspace-global lookup.
 */
nx_stm32h743_flash_ram_pulse_outcome_t nx_stm32h743_flash_ram_pulse_until_idle(
    nx_stm32h743_flash_ram_workspace_t* execution, nx_time_us_t deadline,
    bool* timed_out);
/**
 * \brief Run the fully protected transaction after the real MSP bridge.
 * \param[in,out] execution: Exact prepared command with a finite absolute
 *        deadline. The bridge already owns private MSP and PRIMASK.
 * \return Only after both banks drain, LOCK, protection and VTOR restoration.
 * \note Any unproved post-trigger return condition remains in RAM forever;
 *       no Flash observer, callback, string or application recovery policy.
 */
nx_result_t nx_stm32h743_flash_ram_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Real selected assembly bridge; model supplies only CPU register I/O.
 */
nx_result_t
nx_stm32h743_flash_ram_enter(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief RAM exception entries retain the exact VTOR-backed workspace. */
void nx_stm32h743_flash_ram_nmi(void);
void nx_stm32h743_flash_ram_hardfault(void);
/**
 * \brief Run the RAM trigger/drain/lock leaf on the already-owned RAM stack.
 * \param[in,out] execution: Exact cold-prepared controller/epoch workspace.
 * \return SUCCESS or IO only after observed BSY/QW/WBNE clear and LOCK.
 * \note Internal bridge-only entry: privileged Thread, dedicated stack,
 *       PRIMASK=1 and the private RAM VTOR must all be observed before writes.
 *       It performs no normal Arch critical-section API, time callback,
 *       allocator, fault observer or indirect call while a bank can be busy.
 *       Poll exhaustion cannot cancel a physical pulse or release its caller:
 *       RAM diagnostic state is retained and a RAM-only terminal path parks.
 *       This leaf alone is not cold copy/vector/stack/ELF qualification.
 */
nx_result_t
nx_stm32h743_flash_ram_pulse(nx_stm32h743_flash_ram_workspace_t* execution);
#ifdef __cplusplus
}
#endif
#endif
