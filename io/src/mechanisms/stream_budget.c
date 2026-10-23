/**
 * \file            stream_budget.c
 * \brief           Optional bounded metadata and checked engineering budget
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/io/stream_budget.h"
#include "nexus/arch/arch.h"

/** \brief Checked byte spans; no ordering of unrelated C pointers. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief Check the full time domain before multiplying either operand. */
static bool multiply_u64(uint64_t first, uint64_t second, uint64_t* product) {
    if (first != 0U && second > UINT64_MAX / first) {
        return false;
    }
    *product = first * second;
    return true;
}

/** \brief Copy only maintained aggregates; never scan the slot population. */
nx_result_t nx_stream_observe(const nx_stream_t* stream,
                              nx_stream_observation_t* out) {
    if (stream == NULL || out == NULL || stream->slots == NULL ||
        stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        overlaps(out, sizeof(*out), stream, sizeof(*stream)) ||
        overlaps(out, sizeof(*out), stream->slots,
                 stream->count * sizeof(*stream->slots))) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (stream->producer >= stream->count ||
        stream->consumer >= stream->count ||
        stream->outstanding > stream->count) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    nx_stream_observation_t observation = {
        .capacity = stream->count,
        .outstanding = stream->outstanding,
        .free_blocks = stream->count - stream->outstanding,
        .epoch = stream->epoch,
        .losses = stream->losses,
        .next_free =
            !stream->stopping && stream->epoch != UINT64_MAX &&
            stream->slots[stream->producer].state == NX_STREAM_SLOT_FREE,
        .stopping = stream->stopping};
    nx_arch_irq_restore(saved);
    *out = observation;
    return NX_SUCCESS;
}
/** \brief Reject overflow instead of silently qualifying a clamped budget. */
nx_result_t nx_stream_budget_calculate(const nx_stream_budget_t* budget,
                                       nx_stream_budget_result_t* out) {
    if (budget == NULL || out == NULL || budget->units_per_second == 0U ||
        budget->units_per_block == 0U || budget->unit_bytes == 0U ||
        budget->configured_blocks == 0U ||
        budget->release_delay_us == NX_DEADLINE_NEVER ||
        budget->target_delay_us == NX_DEADLINE_NEVER ||
        overlaps(budget, sizeof(*budget), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    uint64_t denominator;
    uint64_t numerator;
    if (!multiply_u64(budget->units_per_block, UINT64_C(1000000),
                      &denominator) ||
        !multiply_u64(budget->units_per_second, budget->release_delay_us,
                      &numerator)) {
        return NX_ERROR_EXHAUSTED;
    }
    uint64_t retained = numerator / denominator;
    if (numerator % denominator != 0U) {
        ++retained;
    }
    if (retained > SIZE_MAX ||
        budget->hardware_targets > SIZE_MAX - budget->spare_blocks ||
        (size_t)retained >
            SIZE_MAX - budget->hardware_targets - budget->spare_blocks) {
        return NX_ERROR_EXHAUSTED;
    }
    size_t required =
        (size_t)retained + budget->hardware_targets + budget->spare_blocks;
    if (required > SIZE_MAX / budget->units_per_block ||
        required * budget->units_per_block > SIZE_MAX / budget->unit_bytes) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_time_us_t fill = denominator / budget->units_per_second;
    *out = (nx_stream_budget_result_t){
        .retained_blocks = (size_t)retained,
        .required_blocks = required,
        .required_payload_bytes =
            required * budget->units_per_block * budget->unit_bytes,
        .block_fill_us = fill,
        .capacity_sufficient = budget->configured_blocks >= required,
        .target_latency_sufficient = budget->target_delay_us < fill};
    return NX_SUCCESS;
}
