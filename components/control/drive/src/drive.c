/**
 * \file            drive.c
 * \brief           Explicit current-reference and continuous-feedback exchange
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/drive.h"
#include "nexus/arch/atomic.h"
#include <math.h>

static bool span(const void* object, size_t bytes, size_t alignment) {
    const uintptr_t address = (uintptr_t)object;
    return object != NULL && bytes != 0U && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

static bool capacity_valid(uint32_t count, size_t item_bytes) {
    return count != 0U && count < UINT32_C(0x80000000) &&
           (count & (count - 1U)) == 0U && count <= SIZE_MAX / item_bytes;
}

static bool references_valid(const nx_drive_references_t* queue) {
    return span(queue, sizeof(*queue), _Alignof(nx_drive_references_t)) &&
           queue->initialized &&
           capacity_valid(queue->capacity, sizeof(*queue->slots)) &&
           span(queue->slots, queue->capacity * sizeof(*queue->slots),
                _Alignof(nx_drive_reference_t)) &&
           queue->clock_domain != 0U && queue->epoch != 0U;
}

static nx_result_t latched(uint32_t* word, nx_result_t reason) {
    uint32_t expected = NX_SUCCESS;
    (void)nx_atomic_u32_compare_exchange_acq_rel(word, &expected,
                                                 (uint32_t)reason);
    return (nx_result_t)nx_atomic_u32_load_acquire(word);
}

nx_result_t nx_drive_references_initialize(nx_drive_references_t* queue,
                                           nx_drive_reference_t* slots,
                                           uint32_t capacity,
                                           uint32_t clock_domain,
                                           uint64_t epoch) {
    if (!span(queue, sizeof(*queue), _Alignof(nx_drive_references_t)) ||
        !capacity_valid(capacity, sizeof(*slots)) ||
        !span(slots, capacity * sizeof(*slots),
              _Alignof(nx_drive_reference_t)) ||
        overlap(queue, sizeof(*queue), slots, capacity * sizeof(*slots)) ||
        clock_domain == 0U || epoch == 0U) {
        return NX_ERROR_INVALID;
    }
    const nx_drive_references_t next = {.slots = slots,
                                        .capacity = capacity,
                                        .clock_domain = clock_domain,
                                        .epoch = epoch,
                                        .initialized = true};
    *queue = next;
    return NX_SUCCESS;
}

nx_result_t nx_drive_reference_publish(nx_drive_references_t* queue,
                                       const nx_drive_reference_t* reference) {
    if (!references_valid(queue) ||
        !span(reference, sizeof(*reference), _Alignof(nx_drive_reference_t)) ||
        overlap(queue, sizeof(*queue), reference, sizeof(*reference)) ||
        overlap(queue->slots, queue->capacity * sizeof(*queue->slots),
                reference, sizeof(*reference)) ||
        reference->clock_domain != queue->clock_domain ||
        reference->epoch != queue->epoch || reference->generation == 0U ||
        reference->parameters_revision == 0U ||
        !isfinite(reference->current.id_a) ||
        !isfinite(reference->current.iq_a) ||
        reference->received_at == NX_DEADLINE_NEVER ||
        reference->current.valid_until_us == NX_DEADLINE_NEVER ||
        reference->current.valid_until_us <= reference->received_at) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&queue->inhibited) != NX_SUCCESS ||
        reference->generation <= queue->last_generation) {
        return NX_ERROR_STATE;
    }
    const uint32_t produced = nx_atomic_u32_load_relaxed(&queue->produced);
    const uint32_t consumed = nx_atomic_u32_load_acquire(&queue->consumed);
    if ((uint32_t)(produced - consumed) >= queue->capacity) {
        return NX_ERROR_BUSY;
    }
    queue->slots[produced & (queue->capacity - 1U)] = *reference;
    queue->last_generation = reference->generation;
    nx_atomic_u32_store_release(&queue->produced, produced + 1U);
    return NX_SUCCESS;
}

nx_result_t nx_drive_inhibit(nx_drive_references_t* queue, nx_result_t reason) {
    if (!references_valid(queue) || reason <= NX_SUCCESS ||
        reason > NX_ERROR_CONTEXT) {
        return NX_ERROR_INVALID;
    }
    (void)latched(&queue->inhibited, reason);
    return NX_SUCCESS;
}

nx_result_t nx_drive_initialize(nx_drive_t* drive,
                                const nx_motor_config_t* motor,
                                nx_drive_references_t* references,
                                uint32_t parameters_revision) {
    if (!span(drive, sizeof(*drive), _Alignof(nx_drive_t)) ||
        !span(motor, sizeof(*motor), _Alignof(nx_motor_config_t)) ||
        !references_valid(references) || parameters_revision == 0U ||
        overlap(drive, sizeof(*drive), motor, sizeof(*motor)) ||
        overlap(drive, sizeof(*drive), references, sizeof(*references)) ||
        overlap(drive, sizeof(*drive), references->slots,
                references->capacity * sizeof(*references->slots)) ||
        motor->clock_domain != references->clock_domain ||
        motor->clock_epoch != references->epoch) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&references->inhibited) != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    nx_drive_t next = {.references = references,
                       .parameters_revision = parameters_revision};
    nx_result_t result = nx_motor_initialize(&next.motor, motor);
    if (result != NX_SUCCESS) {
        return result;
    }
    next.initialized = true;
    *drive = next;
    return NX_SUCCESS;
}

static nx_result_t drive_fault(nx_drive_t* drive, nx_result_t reason) {
    if (drive->fault == NX_SUCCESS) {
        drive->fault = reason;
    }
    if (drive->motor.initialized) {
        (void)nx_motor_latch_fault(&drive->motor, drive->fault);
    }
    return drive->fault;
}

nx_result_t nx_drive_step(nx_drive_t* drive, const nx_motor_sample_t* sample,
                          nx_drive_output_t* out) {
    if (!span(drive, sizeof(*drive), _Alignof(nx_drive_t)) ||
        !span(sample, sizeof(*sample), _Alignof(nx_motor_sample_t)) ||
        !span(out, sizeof(*out), _Alignof(nx_drive_output_t)) ||
        overlap(drive, sizeof(*drive), sample, sizeof(*sample)) ||
        overlap(drive, sizeof(*drive), out, sizeof(*out)) ||
        overlap(sample, sizeof(*sample), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    if (!drive->initialized || !references_valid(drive->references)) {
        return NX_ERROR_STATE;
    }
    if (drive->fault != NX_SUCCESS) {
        return drive->fault;
    }
    nx_drive_references_t* queue = drive->references;
    nx_result_t inhibited =
        (nx_result_t)nx_atomic_u32_load_acquire(&queue->inhibited);
    if (inhibited != NX_SUCCESS) {
        return drive_fault(drive, inhibited);
    }
    const uint32_t consumed = nx_atomic_u32_load_relaxed(&queue->consumed);
    const uint32_t produced = nx_atomic_u32_load_acquire(&queue->produced);
    if ((uint32_t)(produced - consumed) > queue->capacity) {
        return drive_fault(drive, NX_ERROR_STATE);
    }
    if (consumed != produced) {
        const nx_drive_reference_t next =
            queue->slots[consumed & (queue->capacity - 1U)];
        nx_atomic_u32_store_release(&queue->consumed, consumed + 1U);
        if (next.clock_domain != sample->clock_domain ||
            next.epoch != sample->clock_epoch ||
            next.parameters_revision != drive->parameters_revision ||
            next.received_at > sample->observed_at_us ||
            (drive->have_reference &&
             next.generation <= drive->active.generation)) {
            return drive_fault(drive, NX_ERROR_STATE);
        }
        if (next.current.valid_until_us <= sample->apply_time_us) {
            return drive_fault(drive, NX_ERROR_TIMEOUT);
        }
        drive->active = next;
        drive->adopted_cycle = sample->sequence;
        drive->adopted_at = sample->observed_at_us;
        drive->have_reference = true;
    }
    if (!drive->have_reference) {
        return NX_ERROR_EMPTY;
    }
    nx_drive_output_t decision = {
        .reference_generation = drive->active.generation,
        .command_sequence = drive->active.command_sequence,
        .adopted_cycle = drive->adopted_cycle,
        .adopted_at = drive->adopted_at,
        .accepted_id_a = drive->active.current.id_a,
        .accepted_iq_a = drive->active.current.iq_a};
    nx_result_t result = nx_motor_voltage_step(
        &drive->motor, sample, &drive->active.current, &decision.voltage);
    if (result == NX_SUCCESS) {
        result =
            nx_motor_inverse_park(decision.voltage.planned_voltage,
                                  decision.voltage.output_electrical_angle_rad,
                                  &decision.stationary_voltage);
    }
    inhibited = (nx_result_t)nx_atomic_u32_load_acquire(&queue->inhibited);
    if (result != NX_SUCCESS || inhibited != NX_SUCCESS) {
        return drive_fault(drive, result != NX_SUCCESS ? result : inhibited);
    }
    *out = decision;
    return NX_SUCCESS;
}

nx_result_t nx_drive_track_voltage(nx_drive_t* drive, uint64_t cycle,
                                   nx_motor_dq_t voltage) {
    if (!span(drive, sizeof(*drive), _Alignof(nx_drive_t))) {
        return NX_ERROR_INVALID;
    }
    if (!drive->initialized || drive->fault != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    return nx_motor_track_voltage(&drive->motor, cycle, voltage);
}

nx_result_t nx_drive_feedback_sampler_initialize(
    nx_drive_feedback_sampler_t* sampler, uint32_t clock_domain, uint64_t epoch,
    uint32_t division, uint32_t maximum_gap_us) {
    if (!span(sampler, sizeof(*sampler),
              _Alignof(nx_drive_feedback_sampler_t)) ||
        clock_domain == 0U || epoch == 0U || division == 0U ||
        maximum_gap_us == 0U) {
        return NX_ERROR_INVALID;
    }
    const nx_drive_feedback_sampler_t next = {.clock_domain = clock_domain,
                                              .epoch = epoch,
                                              .division = division,
                                              .maximum_gap_us = maximum_gap_us,
                                              .initialized = true};
    *sampler = next;
    return NX_SUCCESS;
}

static nx_result_t sampler_fault(nx_drive_feedback_sampler_t* sampler,
                                 nx_result_t reason) {
    if (sampler->fault == NX_SUCCESS) {
        sampler->fault = reason;
    }
    return sampler->fault;
}

nx_result_t nx_drive_feedback_sample(nx_drive_feedback_sampler_t* sampler,
                                     const nx_motor_sample_t* sample,
                                     float position_rad,
                                     const nx_drive_output_t* adopted,
                                     nx_drive_feedback_t* out) {
    if (!span(sampler, sizeof(*sampler),
              _Alignof(nx_drive_feedback_sampler_t)) ||
        !span(sample, sizeof(*sample), _Alignof(nx_motor_sample_t)) ||
        !span(adopted, sizeof(*adopted), _Alignof(nx_drive_output_t)) ||
        !span(out, sizeof(*out), _Alignof(nx_drive_feedback_t)) ||
        overlap(sampler, sizeof(*sampler), out, sizeof(*out)) ||
        overlap(sample, sizeof(*sample), out, sizeof(*out)) ||
        overlap(adopted, sizeof(*adopted), out, sizeof(*out))) {
        return NX_ERROR_INVALID;
    }
    if (!sampler->initialized) {
        return NX_ERROR_STATE;
    }
    if (sampler->fault != NX_SUCCESS) {
        return sampler->fault;
    }
    if (!sample->valid || !isfinite(position_rad) ||
        !isfinite(adopted->accepted_iq_a) ||
        adopted->reference_generation == 0U ||
        adopted->voltage.sequence != sample->sequence ||
        adopted->adopted_cycle > sample->sequence ||
        adopted->adopted_at > sample->observed_at_us ||
        sample->clock_domain != sampler->clock_domain ||
        sample->clock_epoch != sampler->epoch ||
        sample->sample_time_us == NX_DEADLINE_NEVER ||
        sample->sample_time_us > sample->observed_at_us) {
        return sampler_fault(sampler, NX_ERROR_INVALID);
    }
    if (!sampler->anchored) {
        sampler->first_cycle = sampler->last_cycle = sample->sequence;
        sampler->interval_start = sampler->last_at = sample->sample_time_us;
        sampler->first_position_rad = position_rad;
        sampler->anchored = true;
        return NX_ERROR_BUSY;
    }
    if (sampler->last_cycle == UINT64_MAX ||
        sample->sequence != sampler->last_cycle + 1U ||
        sample->sample_time_us <= sampler->last_at) {
        return sampler_fault(sampler, NX_ERROR_STATE);
    }
    if (sample->sample_time_us - sampler->last_at > sampler->maximum_gap_us) {
        return sampler_fault(sampler, NX_ERROR_TIMEOUT);
    }
    sampler->last_cycle = sample->sequence;
    sampler->last_at = sample->sample_time_us;
    ++sampler->count;
    if (sampler->count < sampler->division) {
        return NX_ERROR_BUSY;
    }
    if (sampler->sequence == UINT32_MAX) {
        return sampler_fault(sampler, NX_ERROR_EXHAUSTED);
    }
    const float dt =
        (float)(sample->sample_time_us - sampler->interval_start) * 0.000001f;
    const float velocity = (position_rad - sampler->first_position_rad) / dt;
    if (!isfinite(dt) || dt <= 0 || !isfinite(velocity)) {
        return sampler_fault(sampler, NX_ERROR_EXHAUSTED);
    }
    const nx_drive_feedback_t next = {
        .mechanical = {.clock_domain = sampler->clock_domain,
                       .epoch = sampler->epoch,
                       .sequence = sampler->sequence + 1U,
                       .interval_start = sampler->interval_start,
                       .sampled_at = sample->sample_time_us,
                       .position_rad = position_rad,
                       .velocity_rad_s = velocity,
                       .valid = true},
        .fast_first_cycle = sampler->first_cycle,
        .fast_last_cycle = sample->sequence,
        .reference_generation = adopted->reference_generation,
        .command_sequence = adopted->command_sequence,
        .accepted_iq_a = adopted->accepted_iq_a};
    sampler->sequence = next.mechanical.sequence;
    sampler->first_cycle = sample->sequence;
    sampler->interval_start = sample->sample_time_us;
    sampler->first_position_rad = position_rad;
    sampler->count = 0U;
    *out = next;
    return NX_SUCCESS;
}

static bool feedback_queue_valid(const nx_drive_feedback_queue_t* queue) {
    return span(queue, sizeof(*queue), _Alignof(nx_drive_feedback_queue_t)) &&
           queue->initialized &&
           capacity_valid(queue->capacity, sizeof(*queue->slots)) &&
           span(queue->slots, queue->capacity * sizeof(*queue->slots),
                _Alignof(nx_drive_feedback_t));
}

nx_result_t nx_drive_feedback_initialize(nx_drive_feedback_queue_t* queue,
                                         nx_drive_feedback_t* slots,
                                         uint32_t capacity,
                                         uint32_t clock_domain,
                                         uint64_t epoch) {
    if (!span(queue, sizeof(*queue), _Alignof(nx_drive_feedback_queue_t)) ||
        !capacity_valid(capacity, sizeof(*slots)) ||
        !span(slots, capacity * sizeof(*slots),
              _Alignof(nx_drive_feedback_t)) ||
        overlap(queue, sizeof(*queue), slots, capacity * sizeof(*slots)) ||
        clock_domain == 0U || epoch == 0U) {
        return NX_ERROR_INVALID;
    }
    const nx_drive_feedback_queue_t next = {.slots = slots,
                                            .capacity = capacity,
                                            .clock_domain = clock_domain,
                                            .epoch = epoch,
                                            .initialized = true};
    *queue = next;
    return NX_SUCCESS;
}

nx_result_t nx_drive_feedback_publish(nx_drive_feedback_queue_t* queue,
                                      const nx_drive_feedback_t* feedback) {
    if (!feedback_queue_valid(queue) ||
        !span(feedback, sizeof(*feedback), _Alignof(nx_drive_feedback_t)) ||
        overlap(queue, sizeof(*queue), feedback, sizeof(*feedback)) ||
        overlap(queue->slots, queue->capacity * sizeof(*queue->slots), feedback,
                sizeof(*feedback)) ||
        !feedback->mechanical.valid ||
        feedback->mechanical.clock_domain != queue->clock_domain ||
        feedback->mechanical.epoch != queue->epoch ||
        feedback->mechanical.sampled_at == NX_DEADLINE_NEVER ||
        feedback->mechanical.interval_start >=
            feedback->mechanical.sampled_at ||
        feedback->fast_first_cycle >= feedback->fast_last_cycle ||
        !isfinite(feedback->mechanical.position_rad) ||
        !isfinite(feedback->mechanical.velocity_rad_s) ||
        !isfinite(feedback->accepted_iq_a)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t fault = (nx_result_t)nx_atomic_u32_load_acquire(&queue->fault);
    if (fault != NX_SUCCESS) {
        return fault;
    }
    if (queue->last_sequence == UINT32_MAX ||
        feedback->mechanical.sequence != queue->last_sequence + 1U ||
        (queue->last_sequence != 0U &&
         (feedback->fast_first_cycle != queue->last_fast_cycle ||
          feedback->mechanical.interval_start != queue->last_at))) {
        return latched(&queue->fault, NX_ERROR_STATE);
    }
    const uint32_t produced = nx_atomic_u32_load_relaxed(&queue->produced);
    const uint32_t consumed = nx_atomic_u32_load_acquire(&queue->consumed);
    if ((uint32_t)(produced - consumed) >= queue->capacity) {
        return latched(&queue->fault, NX_ERROR_OVERFLOW);
    }
    queue->slots[produced & (queue->capacity - 1U)] = *feedback;
    queue->last_sequence = feedback->mechanical.sequence;
    queue->last_fast_cycle = feedback->fast_last_cycle;
    queue->last_at = feedback->mechanical.sampled_at;
    nx_atomic_u32_store_release(&queue->produced, produced + 1U);
    return NX_SUCCESS;
}

nx_result_t nx_drive_feedback_take(nx_drive_feedback_queue_t* queue,
                                   nx_drive_feedback_t* feedback) {
    if (!feedback_queue_valid(queue) ||
        !span(feedback, sizeof(*feedback), _Alignof(nx_drive_feedback_t)) ||
        overlap(queue, sizeof(*queue), feedback, sizeof(*feedback)) ||
        overlap(queue->slots, queue->capacity * sizeof(*queue->slots), feedback,
                sizeof(*feedback))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t fault = (nx_result_t)nx_atomic_u32_load_acquire(&queue->fault);
    if (fault != NX_SUCCESS) {
        return fault;
    }
    const uint32_t consumed = nx_atomic_u32_load_relaxed(&queue->consumed);
    const uint32_t produced = nx_atomic_u32_load_acquire(&queue->produced);
    if (consumed == produced) {
        return NX_ERROR_EMPTY;
    }
    if ((uint32_t)(produced - consumed) > queue->capacity) {
        return NX_ERROR_STATE;
    }
    const nx_drive_feedback_t next =
        queue->slots[consumed & (queue->capacity - 1U)];
    fault = (nx_result_t)nx_atomic_u32_load_acquire(&queue->fault);
    if (fault != NX_SUCCESS) {
        return fault;
    }
    *feedback = next;
    nx_atomic_u32_store_release(&queue->consumed, consumed + 1U);
    return NX_SUCCESS;
}

nx_result_t nx_drive_bus_limit(const nx_drive_bus_budget_t* budget,
                               const float* requested_power_w, size_t count,
                               nx_time_us_t now, float* scales) {
    if (!span(budget, sizeof(*budget), _Alignof(nx_drive_bus_budget_t)) ||
        count == 0U || count > SIZE_MAX / sizeof(float) ||
        !span(requested_power_w, count * sizeof(float), _Alignof(float)) ||
        !span(scales, count * sizeof(float), _Alignof(float)) ||
        overlap(scales, count * sizeof(float), requested_power_w,
                count * sizeof(float)) ||
        overlap(scales, count * sizeof(float), budget, sizeof(*budget)) ||
        !isfinite(budget->motoring_power_w) || budget->motoring_power_w < 0 ||
        !isfinite(budget->regeneration_power_w) ||
        budget->regeneration_power_w < 0 ||
        budget->valid_until == NX_DEADLINE_NEVER ||
        budget->sampled_at >= budget->valid_until || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (now < budget->sampled_at || now >= budget->valid_until) {
        return NX_ERROR_TIMEOUT;
    }
    float motoring = 0, regeneration = 0;
    for (size_t i = 0; i < count; ++i) {
        const float power = requested_power_w[i];
        if (!isfinite(power)) {
            return NX_ERROR_INVALID;
        }
        if (power >= 0) {
            motoring += power;
        } else {
            regeneration -= power;
        }
        if (!isfinite(motoring) || !isfinite(regeneration)) {
            return NX_ERROR_EXHAUSTED;
        }
    }
    const float consume_scale = motoring > budget->motoring_power_w
                                    ? budget->motoring_power_w / motoring
                                    : 1;
    const float regenerate_scale =
        regeneration > budget->regeneration_power_w
            ? budget->regeneration_power_w / regeneration
            : 1;
    for (size_t i = 0; i < count; ++i) {
        scales[i] = requested_power_w[i] < 0 ? regenerate_scale : consume_scale;
    }
    return NX_SUCCESS;
}
