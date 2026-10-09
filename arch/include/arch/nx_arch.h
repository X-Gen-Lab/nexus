/** CPU-local interrupt and ordering primitives. No HAL, OSAL or SDK dependency. */
#ifndef NX_ARCH_H
#define NX_ARCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque saved state. Never substitute a FreeRTOS BASEPRI/syscall token.
 * Cortex-M4: previous PRIMASK. Native: previous calling-thread nesting level.
 * Tokens must be restored on the same CPU/thread in strict reverse order.
 */
typedef struct { uint32_t value; } nx_arch_irq_state_t;

/** Save local state and mask configurable exceptions / acquire Native lock.
 * Short metadata sections only: do not block, allocate, call OSAL or callbacks.
 * NMI/HardFault are not masked. This primitive does not implement an SMP lock.
 */
nx_arch_irq_state_t nx_arch_irq_save(void);
void nx_arch_irq_restore(nx_arch_irq_state_t previous);

/** True if a CPU exception mask is active. Cortex-M4 checks PRIMASK, BASEPRI
 * and FAULTMASK, conservatively rejecting blocking work under any mask.
 * This query does not change them; save/restore still owns only PRIMASK.
 * Native reports whether the calling thread holds its architecture lock.
 */
bool nx_arch_irq_is_masked(void);

/** Cortex-M4 reads IPSR; Native has no hardware ISR and returns false.
 * Native operations are not POSIX signal-safe and are not an ISR timing model.
 */
bool nx_arch_in_isr(void);

/** CPU data/order/instruction barriers. Native uses sequentially consistent
 * thread fences; it does not simulate DMA, instruction pipeline or cache.
 */
void nx_arch_dmb(void);
void nx_arch_dsb(void);
void nx_arch_isb(void);

#ifdef __cplusplus
}
#endif
#endif
