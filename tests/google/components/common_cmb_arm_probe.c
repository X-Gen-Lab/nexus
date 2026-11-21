/**
 * \file            common_cmb_arm_probe.c
 * \brief           ARM link consumer of raw M4F records and real CmBacktrace
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/cm_backtrace_adapter.h"

/** \brief Synthetic quiescent data establishes linkage, not physical faults. */
static _Alignas(8) uint32_t stack_words[64];
static nx_fault_record_t retained;
static uint32_t addresses[32];

/** \brief Reference both wrapper APIs and the actual upstream scan dependency.
 */
int main(void) {
    nx_fault_capture_t facts = {0};
    facts.sequence = 1U;
    facts.flags = NX_FAULT_CORE_VALID | NX_FAULT_STACK_VALID;
    facts.exception_return = UINT32_C(0xfffffff9);
    facts.msp = (uint32_t)(uintptr_t)stack_words;
    facts.stack_begin = facts.msp;
    facts.stack_end = facts.stack_begin + (uint32_t)sizeof(stack_words);
    nx_fault_stack_binding_t binding = {stack_words, 64U};
    nx_result_t result = nx_fault_record_capture(&retained, &facts);
    if (result != NX_SUCCESS) {
        return (int)result;
    }
    result = nx_cm_backtrace_init("nexus", "f407", "link-probe");
    if (result != NX_SUCCESS) {
        return (int)result;
    }
    size_t depth = 0U;
    return (int)nx_cm_backtrace_record(&retained, &binding, addresses, 32U, 64U,
                                       &depth);
}
