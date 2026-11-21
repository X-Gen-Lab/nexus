/**
 * \file            cm_backtrace_adapter.c
 * \brief           Explicit M4F record bounds around the unchanged upstream
 * scan \author          Nexus Team \version         1.0.0 \date 2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/cm_backtrace_adapter.h"
#include "nexus/arch/arch.h"
#include <cm_backtrace.h>

/* This public upstream implementation is missing from its pinned header. Keep
 * the exact declaration private; the implementation remains the original C. */
extern size_t cm_backtrace_call_stack_any(uint32_t* buffer, size_t size,
                                          uint32_t sp, uint32_t stack_start,
                                          uint32_t stack_bytes);
extern const unsigned char __nx_msp_start;
extern const unsigned char __nx_msp_end;
extern const unsigned char __nx_flash_live_start;
extern const unsigned char __nx_flash_live_end;

static bool initialized;

/** \brief Names must satisfy the upstream fixed identity fields before copying.
 */
static bool valid_name(const char* name) {
    if (name == NULL) {
        return false;
    }
    for (size_t i = 0U; i <= 32U; ++i) {
        if (name[i] == '\0') {
            return true;
        }
    }
    return false;
}

/** \brief Reject output aliases before an upstream scan can modify addresses.
 */
static bool overlaps(const volatile void* first, size_t bytes,
                     const void* second, size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Initialize the unchanged scanner with the actual image linker bounds.
 */
nx_result_t nx_cm_backtrace_init(const char* firmware, const char* hardware,
                                 const char* software) {
    if (!valid_name(firmware) || !valid_name(hardware) ||
        !valid_name(software) ||
        (uintptr_t)&__nx_msp_end <= (uintptr_t)&__nx_msp_start ||
        (uintptr_t)&__nx_flash_live_end <= (uintptr_t)&__nx_flash_live_start) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (initialized) {
        return NX_ERROR_STATE;
    }
    cm_backtrace_init(firmware, hardware, software);
    initialized = true;
    return NX_SUCCESS;
}

/** \brief Bound the real source scan using the committed original M4F frame. */
nx_result_t nx_cm_backtrace_record(const volatile nx_fault_record_t* record,
                                   const nx_fault_stack_binding_t* binding,
                                   uint32_t* addresses, size_t capacity,
                                   size_t maximum_scan_words, size_t* depth) {
    if (record == NULL || binding == NULL || binding->begin == NULL ||
        binding->words == 0U || addresses == NULL || depth == NULL ||
        capacity == 0U || capacity > 32U || maximum_scan_words == 0U ||
        maximum_scan_words > 1024U ||
        binding->words >
            (UINTPTR_MAX - (uintptr_t)binding->begin) / sizeof(uint32_t) ||
        overlaps(record, sizeof(*record), addresses,
                 capacity * sizeof(*addresses)) ||
        overlaps(binding, sizeof(*binding), addresses,
                 capacity * sizeof(*addresses)) ||
        overlaps(record, sizeof(*record), depth, sizeof(*depth)) ||
        overlaps(binding, sizeof(*binding), depth, sizeof(*depth)) ||
        overlaps(addresses, capacity * sizeof(*addresses), depth,
                 sizeof(*depth))) {
        return NX_ERROR_INVALID;
    }
    if (!initialized) {
        return NX_ERROR_STATE;
    }
    nx_fault_capture_t facts;
    nx_result_t result = nx_fault_record_read(record, &facts);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((facts.flags & (NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID)) !=
        (NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID)) {
        return NX_ERROR_STATE;
    }
    uintptr_t begin = (uintptr_t)binding->begin;
    uintptr_t end = begin + binding->words * sizeof(uint32_t);
    uintptr_t sp = (facts.exception_return & 4U) != 0U ? facts.psp : facts.msp;
    size_t frame_words = (facts.exception_return & 16U) != 0U ? 8U : 26U;
    if ((facts.core[7] & (1U << 9U)) != 0U) {
        ++frame_words;
    }
    if (facts.stack_begin != (uint32_t)begin ||
        facts.stack_end != (uint32_t)end || sp < begin || sp > end ||
        sp % sizeof(uint32_t) != 0U ||
        frame_words > (end - sp) / sizeof(uint32_t) ||
        overlaps(binding->begin, binding->words * sizeof(uint32_t), addresses,
                 capacity * sizeof(*addresses)) ||
        overlaps(binding->begin, binding->words * sizeof(uint32_t), depth,
                 sizeof(*depth))) {
        return NX_ERROR_INVALID;
    }
    sp += frame_words * sizeof(uint32_t);
    size_t words = (end - sp) / sizeof(uint32_t);
    if (words > maximum_scan_words) {
        words = maximum_scan_words;
    }
    size_t count = 0U;
    if (words != 0U) {
        count = cm_backtrace_call_stack_any(
            addresses, capacity, (uint32_t)sp, (uint32_t)sp,
            (uint32_t)(words * sizeof(uint32_t)));
    }
    *depth = count;
    return NX_SUCCESS;
}
