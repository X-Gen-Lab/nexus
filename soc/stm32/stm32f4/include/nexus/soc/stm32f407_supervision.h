/**
 * \file            stm32f407_supervision.h
 * \brief           Explicit reset, first-fault retention and trace controls
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F407_SUPERVISION_H
#define NEXUS_STM32F407_SUPERVISION_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Basic exception frame selected and validated by the fault entry.
 */
typedef struct {
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
} nx_stm32_exception_frame_t;

/** \brief Caller-owned first-fault record, optionally placed in noinit RAM.
 * \note Zero-initialize before first use. The provider owns every field after
 *       capture. Readers and storage survive all nested reporters. noinit does
 *       not guarantee reset/power retention; Standby loses SRAM and CCM.
 */
typedef struct {
    uint32_t state;
    uint32_t format;
    uint32_t reason;
    uint32_t exception_return;
    uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar, shcsr, icsr;
    uint32_t reset_flags;
    uint32_t clock_flags;
    uint32_t frame_present;
    nx_stm32_exception_frame_t frame;
    uint32_t checksum;
} nx_stm32_fault_record_t;

/** \brief An explicitly enabled cycle trace is separate from monotonic time.
 */
typedef struct {
    uint32_t cycles;
    uint32_t core_hz;
    bool enabled;
} nx_stm32_cycle_trace_t;

/**
 * \brief Publish one bounded first-fault snapshot without allocation or locks.
 * \param[in,out] record: Distinct static zero storage; retained after capture.
 * \param[in] reason: Product-selected raw diagnostic value, never policy.
 * \param[in] exception_return: Raw fault entry EXC_RETURN, not dereferenced.
 * \param[in] frame: Optional caller-validated accessible basic frame or NULL.
 * \return SUCCESS, BUSY for an earlier/nested capture, INVALID/CONTEXT.
 * \note Privileged Thread/IRQ/NMI/fault. Does not walk an untrusted stack or
 *       infer the FPU/extended frame layout. Caller accounts for EXC_RETURN,
 *       stack limits and stacking errors before supplying frame. No tick,
 *       transport, observer, reset, cleanup or persistence write is invoked.
 */
nx_result_t nx_stm32f407_fault_capture(nx_stm32_fault_record_t* record,
                                       uint32_t reason,
                                       uint32_t exception_return,
                                       const nx_stm32_exception_frame_t* frame);
/**
 * \brief Copy an acquire-published immutable first-fault record.
 * \param[in] record: Retained provider-owned snapshot, never concurrently
 * reset. \param[out] output: Distinct complete call-owned record. \return
 * SUCCESS or INVALID/STATE/IO, leaving output unchanged on failure. \note
 * Bounded, no MMIO and no clock dependency. Checksum detects simple corruption;
 * it does not authenticate a record or promise retention.
 */
nx_result_t nx_stm32f407_fault_read(const nx_stm32_fault_record_t* record,
                                    nx_stm32_fault_record_t* output);
/**
 * \brief Explicitly acknowledge all raw RCC reset flags after capture.
 * \return SUCCESS, CONTEXT, STATE before startup capture, or IO. The captured
 *         nx_reset_cause stays unchanged across STOP or a clock restart.
 * \note Serialized privileged Thread; RMVF clears the complete flag group.
 *       No selective clear or automatic acknowledgement is invented.
 */
nx_result_t nx_stm32f407_reset_flags_clear(void);
/**
 * \brief Request a system reset and retain masks if hardware has not reset.
 * \note Explicit privileged product action, Thread or exception context.
 *       Preserves AIRCR priority grouping, writes the required key, issues
 *       barriers and parks until reset. Does not reclaim borrows, imply a
 *       successful reset, change backup policy or write a retention record.
 */
#ifdef __cplusplus
[[noreturn]] void nx_stm32f407_reset_request(void);
#else
_Noreturn void nx_stm32f407_reset_request(void);
#endif
/**
 * \brief Enable or disable DWT cycle tracing after explicit product selection.
 * \param[in] enable: Selected CYCCNT state; disabling leaves shared TRCENA on.
 * \param[in] reset_counter: Explicitly reset CYCCNT, only when enabling.
 * \param[out] output: Distinct call-owned actual enabled/cycle/frequency state.
 * \return SUCCESS or INVALID/CONTEXT/UNSUPPORTED/IO before false success.
 * \note Serialized privileged Thread owns debugger/DWT access. No SWO pin,
 *       ITM transport or wall-clock source is selected. CYCCNT halts in STOP
 *       and changes rate with HCLK; modulo deltas require less than one wrap.
 */
nx_result_t nx_stm32f407_cycle_trace(bool enable, bool reset_counter,
                                     nx_stm32_cycle_trace_t* output);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F407_SUPERVISION_H */
