/**
 * \file            fault_m4.h
 * \brief           Bounded Cortex-M4F exception frame capture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FAULT_M4_H
#define NEXUS_COMPONENTS_FAULT_M4_H
#include "nexus/components/fault_record.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Original CPU-readable static stack allocation, never a TCB.
 *
 * \note            The application supplies immutable original creation facts.
 *                  Live storage and exception frame remain unchanged during
 *                  capture. Neither a private RTOS layout nor a kernel query is
 *                  used. A readable C pointer cannot be proven from its number.
 */
typedef struct {
    const uint32_t* begin;
    size_t words;
} nx_fault_stack_binding_t;

/**
 * \brief           Copy the core exception frame only within an explicit
 * binding.
 *
 * \param[in,out]   facts: Exclusive initialized facts; hardware fields
 * updated.
 *
 * \param[in]       exception_return: Raw F407 M4F EXC_RETURN from
 * handler entry.
 *
 * \param[in]       msp: Raw handler-entry MSP, before C prologue
 * changes it.
 *
 * \param[in]       psp: Raw PSP captured by that same assembly
 * entry.
 *
 * \param[in]       main_stack: Original static MSP allocation.
 *
 * \param[in]       task_stack: Original interrupted PSP allocation, or NULL.
 *
 * \return          SUCCESS with validity flags for an admitted basic/extended
 *                  frame; INVALID leaves facts unchanged for invalid arguments
 *                  or unsupported EXC_RETURN. Out-of-range frames retain raw
 *                  SP/EXC_RETURN but clear CORE/STACK validity and return
 * SUCCESS.
 *
 * \note            Fixed reads of the eight core words; verifies full
 * basic or M4F extended-frame space and optional xPSR alignment padding before
 * publication. Core words stay at raw SP; floating-point state follows them.
 * Floating-point registers are not copied. Does not inspect
 * Kernel/TCB, read a clock, allocate, print, change masks or touch fault-status
 * MMIO. Caller supplies any status words separately. This profile excludes
 * TrustZone and non-M4F exception layouts.
 */
nx_result_t nx_fault_m4_frame_copy(nx_fault_capture_t* facts,
                                   uint32_t exception_return, uintptr_t msp,
                                   uintptr_t psp,
                                   const nx_fault_stack_binding_t* main_stack,
                                   const nx_fault_stack_binding_t* task_stack);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_FAULT_M4_H */
