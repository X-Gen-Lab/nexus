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
 * \note The generated instance requires 1024-byte alignment for VTOR.
 *       Its dedicated stack owns the busy interval. Incoming MSP and active
 *       Thread stacks each own 260 readable/writable bytes below entry SP:
 *       36 bytes of saves plus 224 for nested extended exception frames and
 *       alignment. These reserves apply before and after the private stack.
 *       The caller supplies this ownership. The private-stack ELF bound
 *       cannot establish arbitrary incoming stack ownership. sizeof is not
 *       a linked RAM-cost claim. No instance exists without Flash. Private
 *       state cannot serve as caller-authored execution proof.
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
/* The selected physical pod uses the existing generated object and fixed
 * CMSIS register identities. Host ports map only CPU/MMIO observations. */
extern nx_stm32h743_flash_ram_workspace_t g_nx_stm32h743_flash_ram_workspace;
#if defined(NX_H743_MODEL)
#define NX_H743_RAM_BIND(execution)   ((void)(execution))
#define NX_H743_RAM_FLASH(execution)  ((execution)->port->registers)
#define NX_H743_RAM_SYSTEM(execution) ((execution)->system)
#define NX_H743_RAM_TIMER(execution)  ((execution)->system->timer)
#define NX_H743_RAM_CORE(execution)   ((execution)->system->core)
#define NX_H743_RAM_GUARD(guard)      ((void)(guard))
#define NX_H743_RAM_MPU(registers)    ((void)(registers))
#else
#define NX_H743_RAM_BIND(execution)                                            \
    ((execution) = &g_nx_stm32h743_flash_ram_workspace, (void)(execution))
#define NX_H743_RAM_FLASH(execution)  FLASH
#define NX_H743_RAM_SYSTEM(execution) (&g_nx_stm32h743_system)
#define NX_H743_RAM_TIMER(execution)  TIM2
#define NX_H743_RAM_CORE(execution)   SCB
#define NX_H743_RAM_GUARD(guard)                                               \
    ((guard) = &g_nx_stm32h743_flash_ram_workspace.guard)
#define NX_H743_RAM_MPU(registers) ((registers) = MPU)
#define NX_H743_RAM_OFFSET(member, expected)                                   \
    _Static_assert(offsetof(nx_stm32h743_flash_ram_workspace_t, member) ==     \
                       (expected),                                             \
                   "RAM assembly field offset differs")
NX_H743_RAM_OFFSET(destination, NX_H743_RAM_DESTINATION_OFFSET);
NX_H743_RAM_OFFSET(words, NX_H743_RAM_WORDS_OFFSET);
NX_H743_RAM_OFFSET(saved_control, NX_H743_RAM_SAVED_CR_OFFSET);
NX_H743_RAM_OFFSET(bank, NX_H743_RAM_BANK_OFFSET);
NX_H743_RAM_OFFSET(fault, NX_H743_RAM_FAULT_OFFSET);
_Static_assert(offsetof(nx_stm32h743_flash_ram_workspace_t, guard) +
                       offsetof(nx_stm32h743_flash_mpu_state_t,
                                remaining_effects) ==
                   NX_H743_RAM_GUARD_EFFECT_OFFSET,
               "RAM assembly guard offset differs");
#undef NX_H743_RAM_OFFSET
_Static_assert(sizeof(nx_stm32h743_system_t) == 48U &&
                   offsetof(nx_stm32h743_system_t, overflow_us) == 24U &&
                   offsetof(nx_stm32h743_system_t, timer_error) == 44U &&
                   offsetof(nx_stm32h743_system_t, started) == 45U,
               "RAM System epoch ABI differs");
_Static_assert(FLASH_R_BASE == UINT32_C(0x52002000), "H743 Flash MMIO");
_Static_assert(TIM2_BASE == UINT32_C(0x40000000), "H743 TIM2 MMIO");
_Static_assert(SCB_BASE == UINT32_C(0xe000ed00), "M7 SCB MMIO");
_Static_assert(MPU_BASE == UINT32_C(0xe000ed90), "M7 MPU MMIO");
#endif
/**
 * \brief Copy the exact linked pod and prepare its private exception vectors.
 * \param[in,out] port: Exclusive cold physical controller, with no active
 *        command. The generated Platform owns the physical lease.
 * \param[in,out] execution: The exact existing generated workspace, zeroed
 *        and 1024-aligned, fully disjoint from controller, System, MMIO and
 *        linked execution images. Foreign storage rejects before effects.
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
 *         or STATE for a stale owner, missing core or changed live VTOR. An
 *         empty stop never releases another controller's prepared workspace.
 * \note Refusal retains the owner and workspace. Does not change VTOR,
 *       protection, clocks or another workspace.
 */
nx_result_t
nx_stm32h743_flash_ram_stop(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Reobserve raw CPU/protection before any command staging or borrow.
 * \note Each prepared pulse rechecks a live core and the saved original VTOR
 *       before staging, hardware writes or bridge entry. Rejection retains
 *       ownership. Existing CPU/context and busy errors precede this vector
 *       gate. This is private cold-context admission, never a runtime registry.
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
 *       No function pointer or hidden mode flag. Physical RAM code names the
 *       fixed generated workspace and official MMIO identities directly.
 */
nx_stm32h743_flash_ram_pulse_outcome_t nx_stm32h743_flash_ram_pulse_until_idle(
    nx_stm32h743_flash_ram_workspace_t* execution, nx_time_us_t deadline,
    bool* timed_out);
/**
 * \brief Assembly-only final hardware boundary after the C transaction.
 * \param[in] result: Settled or rejected C outcome in r0, preserved on success.
 * \return The result only after observed both-bank drain, exact target control
 *         plus LOCK, zero guard effects, restored MPU control/region/selector,
 *         and linker-known g_pfnVectors publication/readback with barriers.
 * \note Private non-C ABI: the bridge supplies r8-r11 containing independently
 *       captured CTRL, RNR and region15 masked RBAR/RASR. Actual selected C
 *       must never write these registers; the ELF gate checks that restriction.
 *       Failure tail-branches to the nonreturning RAM terminal. The caller
 *       retains PRIMASK, private MSP and both command/physical ownership.
 *       The host CPU/MMIO model uses the separate ordinary C signature below;
 *       it does not emulate the assembly register-capture proof.
 */
#if defined(NX_H743_MODEL)
nx_result_t
nx_stm32h743_flash_ram_return(nx_stm32h743_flash_ram_workspace_t* execution,
                              uint32_t original, nx_result_t result);
#else
nx_result_t nx_stm32h743_flash_ram_return(nx_result_t result);
#endif
/**
 * \brief Run the fully protected transaction after the real MSP bridge.
 * \param[in,out] execution: Exact prepared command with a finite absolute
 *        deadline. The bridge already owns private MSP and PRIMASK.
 * \return To the private assembly caller after drain, LOCK and MPU cleanup;
 *         RAM VTOR remains installed for its independent final hardware gate.
 * \note Any unproved post-trigger return condition remains in RAM forever;
 *       no Flash observer, callback, string or application recovery policy.
 */
nx_result_t nx_stm32h743_flash_ram_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Exact eight-read/eight-store assembly flashword pulse. */
void nx_stm32h743_flash_ram_program_words(void);
/** \brief Selected assembly bridge; model supplies only CPU register I/O.
 * \note Privileged exclusive Thread and 260-byte incoming MSP/active-stack
 *       ownership reserves are mandatory. The bridge preserves distinct
 *       MSP/PSP, CONTROL/FPCA, PRIMASK and callee-saved registers. Its C
 *       callees obey the admitted compiler AAPCS and private-frame contract.
 */
nx_result_t
nx_stm32h743_flash_ram_enter(nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief RAM exception entries name the fixed generated workspace. */
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
/** \brief Bind an idle flashword provider to its exact static RAM workspace.
 * \note Task-only; Platform owns the same workspace, clock and flash timing
 *       through failed cleanup. Active operations reject state replacement.
 */
nx_result_t
nx_stm32h743_flash_prepare(nx_stm32h743_flash_state_t* port,
                           nx_stm32h743_flash_ram_workspace_t* workspace,
                           uint32_t supply_mv);

#ifdef __cplusplus
}
#endif
#endif
