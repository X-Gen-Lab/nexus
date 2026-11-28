/**
 * \file            stm32f103rb_fault.h
 * \brief           Explicit Cortex-M3 status capture for F103RB
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F103RB_FAULT_H
#define NEXUS_STM32F103RB_FAULT_H
#include "nexus/components/fault_record.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Sample SCB status and publish one immutable first-fault record.
 * \param[in,out] record: Aligned call-owned internal SRAM. Keep writable and
 *           disjoint from the executing stack.
 * \param[in] sequence: Caller-owned identity for acknowledgement.
 * \param[in] reason: Caller-owned raw diagnostic reason.
 * \param[in] exception_return: Raw EXC_RETURN from a trusted entry, or zero.
 * \return SUCCESS, INVALID, CONTEXT or BUSY when publication is unavailable.
 * \note Privileged execution with a usable stack is required. A nested
 *       attempt cannot replace in-progress or committed data. An interrupted
 *       exclusive store may return BUSY before publication. F1 captures
 *       CFSR, HFSR, MMFAR, BFAR, SHCSR and ICSR. Address fields are zero if
 *       invalid. DFSR, AFSR and stack/frame fields remain zero. The function
 *       installs no vectors and never uses EXC_RETURN to dereference memory.
 *       Decode after Platform boot with nx_reset_record_read and
 *       nx_reset_cause. Retention through a specific reset remains a product
 *       memory placement and hardware qualification requirement.
 */
nx_result_t nx_stm32f103rb_fault_capture(volatile nx_fault_record_t* record,
                                         uint32_t sequence, uint32_t reason,
                                         uint32_t exception_return);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F103RB_FAULT_H */
