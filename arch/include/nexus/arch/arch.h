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
 * \brief           Save incoming local mask and mask configurable exceptions.
 *
 * \return          Token to restore exactly once in reverse nesting order.
 *
 * \note            Task/IRQ short metadata sections only; do not block,
 *                  allocate, call OS functions, copy large buffers or invoke
 *                  callbacks. NMI/HardFault remain unmasked. Native uses a
 *                  recursive host exclusion model, not physical IRQ timing or
 *                  signal safety.
 */
nx_arch_irq_state_t nx_arch_irq_save(void);
/**
 * \brief           Restore the incoming mask, preserving nested exclusions.
 *
 * \param[in]       previous: Token from this CPU/thread's latest unmatched
 *                  save.
 *
 * \note            Restores PRIMASK only; BASEPRI/FAULTMASK are not modified.
 */
void nx_arch_irq_restore(nx_arch_irq_state_t previous);
/**
 * \brief           Test masks that could prevent required completion progress.
 *
 * \return          True for PRIMASK/BASEPRI/FAULTMASK or Native thread
 *                  nesting.
 *
 * \note            Bounded task/IRQ query, no state change.
 */
bool nx_arch_irq_is_masked(void);
/**
 * \brief           Query current CPU exception context.
 *
 * \return          Cortex-M4 IPSR is nonzero; Native has no hardware ISR and
 *                  always returns false. Host signals are unsupported
 *                  contexts.
 */
bool nx_arch_in_isr(void);
/**
 * \brief           Order explicit data memory accesses in the CPU domain.
 *
 * \note            Cortex-M4 DMB; Native sequentially consistent thread fence.
 *                  Does not prove DMA idle or clean noncoherent cache lines.
 */
void nx_arch_dmb(void);
/**
 * \brief           Complete preceding explicit memory accesses.
 *
 * \note            Cortex-M4 DSB; Native thread fence is a behavioral model,
 *                  not a physical bus/DMA completion guarantee.
 */
void nx_arch_dsb(void);
/**
 * \brief           Synchronize subsequent CPU instruction execution.
 *
 * \note            Cortex-M4 ISB; Native thread fence has no pipeline
 *                  semantics.
 */
void nx_arch_isb(void);
/**
 * \brief           Snapshot an already enabled free-running cycle counter.
 *
 * \param[out]      cycles: DWT CYCCNT snapshot, unchanged when unavailable.
 *
 * \return          True when a maintained CPU counter is enabled; false on
 *                  Native or disabled/unavailable DWT. Does not enable/reset
 *                  it.
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
