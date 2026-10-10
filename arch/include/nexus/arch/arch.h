/**
 * \file            arch.h
 *
 * \brief           CPU-local mask, context, ordering and cycle snapshot
 *                  contract.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_ARCH_H
#define NEXUS_ARCH_ARCH_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Saved incoming PRIMASK or calling-thread Native nesting.
 *
 * \note            Restore on the same CPU/thread in strict reverse order.
 *                  This is neither a FreeRTOS BASEPRI token nor an SMP lock.
 */
typedef struct {
    uint32_t value;
} nx_arch_irq_state_t;
/**
 * \brief           Read-only snapshot of implemented local interrupt masks.
 *
 * \note            Absent BASEPRI/FAULTMASK registers are zero. Native reports
 *                  PRIMASK as zero or one according to current thread nesting.
 *                  This snapshot is neither a restore token nor a kernel lock.
 */
typedef struct {
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
} nx_arch_irq_masks_t;
/**
 * \brief           Save incoming local mask and mask configurable exceptions.
 *
 * \return          Token to restore exactly once in reverse nesting order.
 *
 * \note            Privileged Task/configurable-IRQ short metadata sections
 *                  only; do not block,
 *                  allocate, call OS functions, copy large buffers or invoke
 *                  callbacks. NMI/HardFault remain unmasked. Native uses a
 *                  recursive host exclusion model, not physical IRQ timing or
 *                  signal safety. NMI/HardFault must not access shared
 *                  metadata protected by this mask. ARM unprivileged callers
 *                  need an explicit privileged gateway owned by their OS.
 */
nx_arch_irq_state_t nx_arch_irq_save(void);
/**
 * \brief           Restore the incoming mask, preserving nested exclusions.
 *
 * \param[in]       previous: Token from this CPU/thread's latest unmatched
 *                  save.
 *
 * \note            Privileged contexts only. Restores PRIMASK only;
 *                  BASEPRI/FAULTMASK, where present, are not modified.
 */
void nx_arch_irq_restore(nx_arch_irq_state_t previous);
/**
 * \brief           Snapshot the implemented local interrupt-mask registers.
 *
 * \return          Current PRIMASK and implemented BASEPRI/FAULTMASK values.
 *
 * \note            Bounded read-only privileged task/IRQ query. Does not mask
 *                  interrupts or grant a stable critical section. Values refer
 *                  to this CPU/security state, not another security world or
 *                  an SMP peer. Unprivileged reads do not establish mask state.
 */
nx_arch_irq_masks_t nx_arch_irq_masks(void);
/**
 * \brief           Test masks that could prevent required completion progress.
 *
 * \return          True for PRIMASK, implemented BASEPRI/FAULTMASK, or Native
 *                  thread nesting. Baseline Cortex-M cores query PRIMASK only.
 *
 * \note            Bounded privileged task/IRQ query, no state change. ARM
 *                  unprivileged reads do not establish the actual mask state.
 */
bool nx_arch_irq_is_masked(void);
/**
 * \brief           Query the exact current CPU exception identity.
 *
 * \return          IPSR exception number, zero for Thread mode or Native.
 *
 * \note            Bounded read-only task/IRQ query. The identity belongs to
 *                  the current CPU/security state; it is not a portable SoC
 *                  IRQ number or evidence that a kernel ISR call is allowed.
 */
uint32_t nx_arch_exception_number(void);
/**
 * \brief           Query privilege in the current CPU execution context.
 *
 * \return          True in Handler mode or privileged Thread mode. Native
 *                  always returns true because it has no CPU privilege model.
 *
 * \note            Bounded read-only task/IRQ query. Handler privilege does
 *                  not depend on the interrupted Thread's CONTROL.nPRIV.
 *                  This query does not establish Secure-world ownership.
 */
bool nx_arch_is_privileged(void);
/**
 * \brief           Query current CPU exception context.
 *
 * \return          Cortex-M IPSR is nonzero; Native has no hardware ISR and
 *                  always returns false. Host signals are unsupported
 *                  contexts.
 */
bool nx_arch_in_isr(void);
/**
 * \brief           Order explicit data memory accesses in the CPU domain.
 *
 * \note            Cortex-M DMB; Native sequentially consistent thread fence.
 *                  Does not prove DMA idle or clean noncoherent cache lines.
 */
void nx_arch_dmb(void);
/**
 * \brief           Complete preceding explicit memory accesses.
 *
 * \note            Cortex-M DSB; Native thread fence is a behavioral model,
 *                  not a physical bus/DMA completion guarantee.
 */
void nx_arch_dsb(void);
/**
 * \brief           Synchronize subsequent CPU instruction execution.
 *
 * \note            Cortex-M ISB; Native thread fence has no pipeline
 *                  semantics.
 */
void nx_arch_isb(void);
/**
 * \brief           Snapshot an already enabled free-running cycle counter.
 *
 * \param[out]      cycles: DWT CYCCNT snapshot, unchanged when unavailable.
 *
 * \return          True when a maintained CPU counter is enabled; false on
 *                  Native, an unreviewed/disabled/unavailable DWT, or an
 *                  unprivileged caller. Does not enable/reset it. Only a
 *                  reviewed CPU profile may read the optional DWT registers.
 *
 * \note            Counter frequency follows CPU clock. Use unsigned snapshot
 *                  subtraction only for intervals shorter than 2^32 cycles;
 *                  this counter is neither shared monotonic time nor a
 *                  deadline.
 */
bool nx_arch_cycle_snapshot(uint32_t* cycles);
#ifdef __cplusplus
}
#endif
#endif
