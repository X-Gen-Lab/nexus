/**
 * \file            fault_m4.c
 * \brief           Bounded frame copying for the explicit F407 M4F profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_m4.h"

/** \brief Reject wrapped aliases before publishing facts over their inputs. */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Only the six architectural F407 M4F return encodings are admitted. */
static bool valid_return(uint32_t value) {
    return value == UINT32_C(0xfffffff1) || value == UINT32_C(0xfffffff9) ||
           value == UINT32_C(0xfffffffd) || value == UINT32_C(0xffffffe1) ||
           value == UINT32_C(0xffffffe9) || value == UINT32_C(0xffffffed);
}

/** \brief Status gating precedes any interrupted-stack dereference. */
nx_result_t nx_fault_m4_frame_copy(nx_fault_capture_t* facts,
                                   uint32_t exception_return, uintptr_t msp,
                                   uintptr_t psp,
                                   const nx_fault_stack_binding_t* main_stack,
                                   const nx_fault_stack_binding_t* task_stack) {
    if (facts == NULL || !valid_return(exception_return) ||
        (main_stack != NULL &&
         overlaps(main_stack, sizeof(*main_stack), facts, sizeof(*facts))) ||
        (task_stack != NULL &&
         overlaps(task_stack, sizeof(*task_stack), facts, sizeof(*facts)))) {
        return NX_ERROR_INVALID;
    }
    nx_fault_capture_t result = *facts;
    result.flags &=
        ~(NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID | NX_FAULT_FRAME_EXTENDED);
    result.exception_return = exception_return;
    result.msp = (uint32_t)msp;
    result.psp = (uint32_t)psp;
    result.stack_begin = 0U;
    result.stack_end = 0U;
    for (size_t i = 0U; i < 8U; ++i) {
        result.core[i] = 0U;
    }
    if ((result.flags & NX_FAULT_STATUS_VALID) != 0U &&
        (result.status[0] & UINT32_C(0x3838)) != 0U) {
        *facts = result;
        return NX_SUCCESS;
    }
    const nx_fault_stack_binding_t* binding =
        (exception_return & 4U) != 0U ? task_stack : main_stack;
    uintptr_t sp = (exception_return & 4U) != 0U ? psp : msp;
    size_t fp_words = (exception_return & 16U) != 0U ? 0U : 18U;
    if (binding == NULL || binding->begin == NULL || binding->words == 0U ||
        binding->words >
            (UINTPTR_MAX - (uintptr_t)binding->begin) / sizeof(uint32_t)) {
        *facts = result;
        return NX_SUCCESS;
    }
    uintptr_t begin = (uintptr_t)binding->begin;
    uintptr_t end = begin + binding->words * sizeof(uint32_t);
    if (sp % sizeof(uint32_t) != 0U || sp < begin || sp > end ||
        fp_words + 8U > (end - sp) / sizeof(uint32_t) ||
        overlaps(binding->begin, binding->words * sizeof(uint32_t), facts,
                 sizeof(*facts))) {
        *facts = result;
        return NX_SUCCESS;
    }
    /* PM0214 Rev 10, Figure 12: core words always begin at raw SP. The
     * optional 18-word floating-point area follows those eight core words. */
    const uint32_t* core = (const uint32_t*)sp;
    uint32_t psr = core[7];
    size_t frame_words = fp_words + 8U + ((psr & (1U << 9U)) != 0U ? 1U : 0U);
    if (frame_words > (end - sp) / sizeof(uint32_t) ||
        (psr & (1U << 24U)) == 0U) {
        *facts = result;
        return NX_SUCCESS;
    }
    for (size_t i = 0U; i < 8U; ++i) {
        result.core[i] = core[i];
    }
    result.stack_begin = (uint32_t)begin;
    result.stack_end = (uint32_t)end;
    result.flags |= NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID;
    if (fp_words != 0U) {
        result.flags |= NX_FAULT_FRAME_EXTENDED;
    }
    *facts = result;
    return NX_SUCCESS;
}
