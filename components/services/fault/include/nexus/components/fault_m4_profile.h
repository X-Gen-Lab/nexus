/**
 * \file            fault_m4_profile.h
 * \brief           Explicit M4F exception owner and emergency stack
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FAULT_M4_PROFILE_H
#define NEXUS_COMPONENTS_FAULT_M4_PROFILE_H
#include "nexus/components/fault_m4.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           One immutable image-specific capture composition.
 *
 * \note            The product reserves retained record and a dedicated
 * emergency stack, supplies identity/sequence facts, and keeps every original
 * static task allocation readable through capture. Task bindings are original
 * allocations, not live TCB pointers or a kernel registry. Their count is the
 * explicit scan budget. Optional after_capture runs only after successful raw
 * commit; it must be bounded and must not use ordinary Log/RTT/Kernel,
 *                  allocate, release loans or change masks. Returning still
 * parks. Recovery/reset/safe outputs belong to the external product.
 */
typedef struct {
    volatile nx_fault_record_t* retained;
    const nx_fault_capture_t* identity;
    nx_fault_stack_binding_t main_stack;
    const nx_fault_stack_binding_t* task_stacks;
    size_t task_stack_count;
    void (*after_capture)(const volatile nx_fault_record_t*,
                          const nx_fault_stack_binding_t*);
} nx_fault_m4_profile_t;

/** \brief Define exactly once at the selected image's composition site. */
extern const nx_fault_m4_profile_t nx_fault_m4_profile;

/**
 * \brief           Define a dedicated writable eight-byte-aligned stack top.
 *
 * \note            Assembly switches MSP here after recording original MSP/PSP.
 *                  Its original allocation, finite stack budget and exclusion
 *                  from all normal users are product composition requirements.
 *                  A missing, zero or misaligned top never enters the C
 * handler.
 */
extern const uintptr_t nx_fault_m4_emergency_stack_top;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_FAULT_M4_PROFILE_H */
