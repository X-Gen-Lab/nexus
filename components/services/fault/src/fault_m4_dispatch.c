/**
 * \file            fault_m4_dispatch.c
 * \brief           Raw-first selected M4F exception dispatch without kernel
 * calls \author          Nexus Team \version         1.0.0 \date 2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_m4_profile.h"

_Noreturn void nx_fault_m4_dispatch(uint32_t exception_return, uintptr_t msp,
                                    uintptr_t psp, uint32_t exception);

/** \brief Match only original static allocations, never a live kernel object.
 */
static const nx_fault_stack_binding_t* task_stack(uintptr_t psp) {
    const nx_fault_stack_binding_t* match = NULL;
    for (size_t i = 0U; i < nx_fault_m4_profile.task_stack_count; ++i) {
        const nx_fault_stack_binding_t* binding =
            &nx_fault_m4_profile.task_stacks[i];
        uintptr_t begin = (uintptr_t)binding->begin;
        if (binding->begin == NULL || binding->words == 0U ||
            binding->words > (UINTPTR_MAX - begin) / sizeof(uint32_t)) {
            continue;
        }
        uintptr_t end = begin + binding->words * sizeof(uint32_t);
        if (psp >= begin && psp < end) {
            if (match != NULL) {
                return NULL;
            }
            match = binding;
        }
    }
    return match;
}

/** \brief The selected M4 profile reads only implemented architectural SCB
 * words. */
static void status_words(uint32_t* output) {
    output[0] = *(volatile const uint32_t*)UINT32_C(0xe000ed28);
    output[1] = *(volatile const uint32_t*)UINT32_C(0xe000ed2c);
    output[2] = *(volatile const uint32_t*)UINT32_C(0xe000ed30);
    output[3] = *(volatile const uint32_t*)UINT32_C(0xe000ed3c);
    output[4] = *(volatile const uint32_t*)UINT32_C(0xe000ed34);
    output[5] = *(volatile const uint32_t*)UINT32_C(0xe000ed38);
    output[6] = *(volatile const uint32_t*)UINT32_C(0xe000ed24);
    output[7] = *(volatile const uint32_t*)UINT32_C(0xe000ed04);
}

/** \brief Commit raw facts on the dedicated stack before optional
 * interpretation. */
_Noreturn void nx_fault_m4_dispatch(uint32_t exception_return, uintptr_t msp,
                                    uintptr_t psp, uint32_t exception) {
    nx_fault_capture_t facts = {0};
    if (nx_fault_m4_profile.identity != NULL) {
        facts.sequence = nx_fault_m4_profile.identity->sequence;
        for (size_t i = 0U; i < NX_FAULT_BUILD_ID_BYTES; ++i) {
            facts.build_id[i] = nx_fault_m4_profile.identity->build_id[i];
        }
    }
    facts.domain = NX_FAULT_PLATFORM;
    facts.reason = NX_ERROR_IO;
    facts.flags = 0U;
    facts.exception = exception;
    facts.exception_return = exception_return;
    facts.msp = (uint32_t)msp;
    facts.psp = (uint32_t)psp;
    const nx_fault_stack_binding_t* selected = NULL;
    if ((exception_return & 4U) != 0U &&
        nx_fault_m4_profile.task_stacks != NULL) {
        selected = task_stack(psp);
    }
    status_words(facts.status);
    facts.flags |= NX_FAULT_STATUS_VALID;
    /* Stacking errors can leave a numerically in-range but incomplete frame.
     * Preserve raw SP/status instead of labelling stale RAM as core registers.
     */
    if ((facts.status[0] & UINT32_C(0x3838)) == 0U) {
        (void)nx_fault_m4_frame_copy(&facts, exception_return, msp, psp,
                                     &nx_fault_m4_profile.main_stack, selected);
    }
    nx_result_t captured =
        nx_fault_record_capture(nx_fault_m4_profile.retained, &facts);
    if (captured == NX_SUCCESS && nx_fault_m4_profile.after_capture != NULL &&
        (facts.flags & NX_FAULT_STACK_VALID) != 0U) {
        if ((exception_return & 4U) == 0U) {
            selected = &nx_fault_m4_profile.main_stack;
        }
        nx_fault_m4_profile.after_capture(nx_fault_m4_profile.retained,
                                          selected);
    }
    for (;;) {
        __asm__ volatile("nop");
    }
}

/** \brief Normal terminal callers retain their existing park/recovery
 * ownership. */
void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                      const char* file, unsigned line) {
    (void)file;
    nx_fault_capture_t facts = {0};
    if (nx_fault_m4_profile.identity != NULL) {
        facts.sequence = nx_fault_m4_profile.identity->sequence;
        for (size_t i = 0U; i < NX_FAULT_BUILD_ID_BYTES; ++i) {
            facts.build_id[i] = nx_fault_m4_profile.identity->build_id[i];
        }
    }
    facts.domain = (uint32_t)domain;
    facts.reason = (uint32_t)reason;
    facts.source_line = line;
    (void)nx_fault_record_capture(nx_fault_m4_profile.retained, &facts);
}
