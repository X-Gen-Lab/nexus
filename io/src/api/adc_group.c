/**
 * \file            adc_group.c
 * \brief           Checked finite whole-frame ADC request dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_group.h"

/** \brief Validate representable exclusive ends before inspecting metadata. */
static bool span(const void* p, size_t bytes, size_t alignment) {
    return p != NULL && bytes != 0U && (uintptr_t)p % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool valid(const nx_adc_group_port_t* p) {
    return span(p, sizeof(*p), _Alignof(nx_adc_group_port_t)) &&
           span(p->ops, sizeof(*p->ops), _Alignof(nx_adc_group_ops_t)) &&
           p->context != NULL;
}
static bool control(const nx_adc_group_port_t* p, const void* data,
                    size_t bytes) {
    return overlap(data, bytes, p, sizeof(*p)) ||
           overlap(data, bytes, p->ops, sizeof(*p->ops));
}
/** \brief Validate both capacities before preparing or borrowing either. */
static bool paired(const nx_adc_group_request_t* request,
                   const nx_adc_group_results_t* results) {
    if (!span(results, sizeof(*results), _Alignof(nx_adc_group_results_t)) ||
        overlap(results, sizeof(*results), request, sizeof(*request)))
        return false;
    const nx_adc_group_result_t* outputs[] = {&results->regular,
                                              &results->injected};
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_adc_group_result_t* output = outputs[i];
        if (!span(output->data, output->bytes, 1U) ||
            overlap(output->data, output->bytes, request, sizeof(*request)) ||
            overlap(output->data, output->bytes, results, sizeof(*results)))
            return false;
    }
    return !overlap(results->regular.data, results->regular.bytes,
                    results->injected.data, results->injected.bytes);
}
nx_result_t nx_adc_group_prepare(nx_adc_group_request_t* request, void* frames,
                                 size_t bytes, size_t count,
                                 nx_time_us_t deadline) {
    if (!span(request, sizeof(*request), _Alignof(nx_adc_group_request_t))) {
        return NX_ERROR_INVALID;
    }
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return NX_ERROR_STATE;
    }
    if (count == 0U || deadline == NX_DEADLINE_NEVER ||
        !span(frames, bytes, 1U) ||
        overlap(frames, bytes, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->output.frames = frames;
        request->bytes = bytes;
        request->frame_count = count;
        request->results_kind = NX_ADC_GROUP_RAW_RESULTS;
    }
    return result;
}
nx_result_t nx_adc_group_prepare_results(nx_adc_group_request_t* request,
                                         nx_adc_group_results_t* results,
                                         size_t count, nx_time_us_t deadline) {
    if (!span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)))
        return NX_ERROR_INVALID;
    nx_request_state_t state = nx_request_state(&request->base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED)
        return NX_ERROR_STATE;
    if (count == 0U || deadline == NX_DEADLINE_NEVER ||
        !paired(request, results))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->output.results = results;
        request->bytes = sizeof(*results);
        request->frame_count = count;
        request->results_kind = NX_ADC_GROUP_REGULAR_INJECTED_RESULTS;
    }
    return result;
}
nx_result_t nx_adc_group_submit(const nx_adc_group_port_t* p,
                                nx_adc_group_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        control(p, request, sizeof(*request)) ||
        !span(request->output.frames, request->bytes, 1U) ||
        control(p, request->output.frames, request->bytes) ||
        overlap(request, sizeof(*request), request->output.frames,
                request->bytes)) {
        return NX_ERROR_INVALID;
    }
    if (request->results_kind == NX_ADC_GROUP_REGULAR_INJECTED_RESULTS) {
        if (request->bytes != sizeof(*request->output.results) ||
            !paired(request, request->output.results) ||
            control(p, request->output.results->regular.data,
                    request->output.results->regular.bytes) ||
            control(p, request->output.results->injected.data,
                    request->output.results->injected.bytes))
            return NX_ERROR_INVALID;
    } else if (request->results_kind != NX_ADC_GROUP_RAW_RESULTS) {
        return NX_ERROR_INVALID;
    }
    return p->ops->submit == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->submit(p->context, request);
}
nx_result_t nx_adc_group_cancel(const nx_adc_group_port_t* p,
                                nx_adc_group_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        control(p, request, sizeof(*request))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->cancel == NULL ? NX_ERROR_UNSUPPORTED
                                  : p->ops->cancel(p->context, request);
}
nx_result_t nx_adc_group_service(const nx_adc_group_port_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                   : p->ops->service(p->context);
}
nx_result_t nx_adc_group_stop(const nx_adc_group_port_t* p) {
    if (!valid(p)) {
        return NX_ERROR_INVALID;
    }
    return p->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->stop(p->context);
}
nx_result_t nx_adc_group_info(const nx_adc_group_port_t* p,
                              nx_adc_group_info_t* info) {
    if (!valid(p) ||
        !span(info, sizeof(*info), _Alignof(nx_adc_group_info_t)) ||
        control(p, info, sizeof(*info))) {
        return NX_ERROR_INVALID;
    }
    return p->ops->info == NULL ? NX_ERROR_UNSUPPORTED
                                : p->ops->info(p->context, info);
}

nx_result_t nx_adc_group_start_blocks(const nx_adc_group_port_t* p,
                                      nx_stream_t* stream) {
    if (!valid(p) || !span(stream, sizeof(*stream), _Alignof(nx_stream_t)) ||
        control(p, stream, sizeof(*stream)) || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        !span(stream->slots, stream->count * sizeof(*stream->slots),
              _Alignof(nx_stream_slot_t)) ||
        control(p, stream->slots, stream->count * sizeof(*stream->slots)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i)
        if (!span(stream->slots[i].data, stream->slots[i].capacity, 1U) ||
            control(p, stream->slots[i].data, stream->slots[i].capacity))
            return NX_ERROR_INVALID;
    return p->ops->start_blocks == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->start_blocks(p->context, stream);
}
nx_result_t nx_adc_group_attach_observer(const nx_adc_group_port_t* p,
                                         nx_adc_observer_t* observer) {
    if (!valid(p) ||
        (observer != NULL &&
         (!span(observer, sizeof(*observer), _Alignof(nx_adc_observer_t)) ||
          control(p, observer, sizeof(*observer)) || observer->count == 0U ||
          observer->count > SIZE_MAX / sizeof(*observer->records) ||
          !span(observer->records, observer->count * sizeof(*observer->records),
                _Alignof(nx_adc_observation_t)) ||
          control(p, observer->records,
                  observer->count * sizeof(*observer->records)))))
        return NX_ERROR_INVALID;
    return p->ops->attach_observer == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->attach_observer(p->context, observer);
}
nx_result_t nx_adc_group_block_observation(const nx_adc_group_port_t* p,
                                           const nx_stream_block_t* block,
                                           nx_adc_observation_t* output) {
    if (!valid(p) ||
        !span(block, sizeof(*block), _Alignof(nx_stream_block_t)) ||
        !span(output, sizeof(*output), _Alignof(nx_adc_observation_t)) ||
        control(p, output, sizeof(*output)) ||
        overlap(block, sizeof(*block), output, sizeof(*output)))
        return NX_ERROR_INVALID;
    return p->ops->block_observation == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->block_observation(p->context, block, output);
}

nx_result_t nx_adc_group_submit_injected(const nx_adc_group_port_t* p,
                                         nx_adc_group_request_t* request) {
    if (!valid(p) ||
        !span(request, sizeof(*request), _Alignof(nx_adc_group_request_t)) ||
        request->results_kind != NX_ADC_GROUP_RAW_RESULTS ||
        control(p, request, sizeof(*request)) ||
        !span(request->output.frames, request->bytes, _Alignof(uint16_t)) ||
        control(p, request->output.frames, request->bytes) ||
        overlap(request, sizeof(*request), request->output.frames,
                request->bytes))
        return NX_ERROR_INVALID;
    return p->ops->submit_injected == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->submit_injected(p->context, request);
}
nx_result_t nx_adc_group_injected_info(const nx_adc_group_port_t* p,
                                       nx_adc_group_info_t* info) {
    if (!valid(p) ||
        !span(info, sizeof(*info), _Alignof(nx_adc_group_info_t)) ||
        control(p, info, sizeof(*info)))
        return NX_ERROR_INVALID;
    return p->ops->injected_info == NULL
               ? NX_ERROR_UNSUPPORTED
               : p->ops->injected_info(p->context, info);
}
