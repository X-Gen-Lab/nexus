/**
 * \file            main.c
 * \brief           Link-only typed ADC requests, blocks and lifecycle consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_group.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_adc_group_request_t s_request;
static nx_adc_group_info_t s_info;
static nx_adc_group_info_t s_injected_info;
static nx_adc_group_results_t s_results;
static _Alignas(4) uint16_t s_data[48];
static _Alignas(4) uint16_t s_injected[12];
static _Alignas(4) uint8_t s_blocks[3][768];
static nx_stream_slot_t s_slots[3];
static nx_stream_t s_stream;
static nx_adc_observation_t s_records[3];
static nx_adc_observer_t s_observer;

/** \brief Caller-owned polling is bounded by one live absolute deadline. */
static nx_result_t wait_request(const nx_adc_group_port_t* port,
                                nx_time_us_t deadline) {
    for (;;) {
        nx_result_t result = nx_adc_group_service(port);
        if (nx_request_state(&s_request.base) == NX_REQUEST_SETTLED) {
            if (s_request.base.result != NX_SUCCESS)
                return s_request.base.result;
            return s_request.base.transferred == 1U ? NX_SUCCESS : NX_ERROR_IO;
        }
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
            return result;
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            (void)nx_adc_group_cancel(port, &s_request);
            return NX_ERROR_TIMEOUT;
        }
    }
}
/** \brief Every finite descriptor names its exact queried output layout. */
static nx_result_t finite(const nx_adc_group_port_t* port,
                          nx_time_us_t deadline, bool paired) {
    if (s_info.format.scan_stride == 0U ||
        s_info.format.scan_stride > sizeof(s_data))
        return NX_ERROR_INVALID;
    nx_result_t result;
    if (paired) {
        if (s_injected_info.format.scan_stride == 0U ||
            s_injected_info.format.scan_stride > sizeof(s_injected))
            return NX_ERROR_INVALID;
        s_results = (nx_adc_group_results_t){
            .regular = {s_data, s_info.format.scan_stride},
            .injected = {s_injected, s_injected_info.format.scan_stride}};
        result =
            nx_adc_group_prepare_results(&s_request, &s_results, 1U, deadline);
    } else {
        result = nx_adc_group_prepare(&s_request, s_data,
                                      s_info.format.scan_stride, 1U, deadline);
    }
    if (result == NX_SUCCESS)
        result = nx_adc_group_submit(port, &s_request);
    return result == NX_SUCCESS ? wait_request(port, deadline) : result;
}
/** \brief Read one real consumer loan and its matching software observation. */
static nx_result_t consume(const nx_adc_group_port_t* port,
                           nx_time_us_t deadline) {
    for (;;) {
        nx_result_t result = nx_adc_group_service(port);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
            return result;
        nx_stream_block_t block;
        result = nx_stream_acquire(&s_stream, &block);
        if (result == NX_SUCCESS) {
            nx_adc_observation_t observation;
            result = nx_adc_group_block_observation(port, &block, &observation);
            if (result == NX_SUCCESS &&
                (observation.epoch != block.epoch || block.length == 0U))
                result = NX_ERROR_IO;
            nx_result_t released = nx_stream_release(&s_stream, &block);
            return released != NX_SUCCESS ? released : result;
        }
        if (result != NX_ERROR_EMPTY)
            return result;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    }
}
/** \brief Only a pure unsupported admission can select finite fallback. */
static nx_result_t start_blocks(const nx_adc_group_port_t* port) {
    if (s_info.format.scan_stride == 0U ||
        sizeof(s_blocks[0]) % s_info.format.scan_stride != 0U)
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < 3U; ++i) {
        s_slots[i].data = s_blocks[i];
        s_slots[i].capacity = sizeof(s_blocks[i]);
    }
    nx_result_t result = nx_stream_initialize(&s_stream, s_slots, 3U);
    if (result == NX_SUCCESS)
        result = nx_adc_observer_initialize(&s_observer, s_records, 3U);
    if (result == NX_SUCCESS)
        result = nx_adc_group_attach_observer(port, &s_observer);
    if (result != NX_SUCCESS)
        return result;
    result = nx_adc_group_start_blocks(port, &s_stream);
    if (result == NX_ERROR_UNSUPPORTED) {
        nx_result_t detached = nx_adc_group_attach_observer(port, NULL);
        if (detached != NX_SUCCESS)
            return detached;
    }
    return result;
}
/** \brief Accepted blocks never fall back to a different request view. */
static nx_result_t blocks(const nx_adc_group_port_t* port,
                          nx_time_us_t deadline, bool injected) {
    nx_result_t result;
    do {
        result = nx_adc_group_service(port);
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    } while (result == NX_ERROR_BUSY);
    if (result != NX_SUCCESS)
        return result;
    if (injected) {
        if (s_injected_info.format.group != NX_ADC_INJECTED ||
            s_injected_info.format.scan_stride == 0U ||
            s_injected_info.format.scan_stride > sizeof(s_injected))
            return NX_ERROR_INVALID;
        result = nx_adc_group_prepare(&s_request, s_injected,
                                      s_injected_info.format.scan_stride, 1U,
                                      deadline);
        if (result == NX_SUCCESS)
            result = nx_adc_group_submit_injected(port, &s_request);
        if (result == NX_SUCCESS)
            result = wait_request(port, deadline);
        if (result != NX_SUCCESS)
            return result;
    }
    return consume(port, deadline);
}
/** \brief Error/timeout retains static buffers until stop actually joins. */
static nx_result_t stop(const nx_adc_group_port_t* port,
                        nx_time_us_t deadline) {
    nx_result_t result;
    do {
        result = nx_adc_group_stop(port);
        if (result != NX_ERROR_BUSY)
            return result;
    } while (!nx_deadline_expired(deadline, nx_time_now_us()));
    return NX_ERROR_TIMEOUT;
}

int main(void) {
    const nx_adc_group_port_t* port =
        nx_factory_adc_group_ref(NX_ADC_GROUP_REF_GROUP);
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || port == NULL)
        return 1;
    nx_request_initialize(&s_request.base);
    if (nx_adc_group_info(port, &s_info) != NX_SUCCESS)
        return 2;
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000000U);
    if (deadline == NX_DEADLINE_NEVER)
        return 3;
    nx_result_t inquiry = nx_adc_group_injected_info(port, &s_injected_info);
    nx_result_t result;
    if (inquiry == NX_SUCCESS &&
        s_injected_info.format.trigger == NX_ADC_TRIGGER_EVENT) {
        result = finite(port, deadline, true);
    } else if (inquiry != NX_SUCCESS && inquiry != NX_ERROR_UNSUPPORTED) {
        result = inquiry;
    } else {
        result = start_blocks(port);
        if (result == NX_SUCCESS)
            result = blocks(port, deadline, inquiry == NX_SUCCESS);
        else if (result == NX_ERROR_UNSUPPORTED)
            result = finite(port, deadline, false);
    }
    nx_time_us_t joined = nx_deadline_after(nx_time_now_us(), 10000U);
    if (joined == NX_DEADLINE_NEVER || stop(port, joined) != NX_SUCCESS)
        return 4;
    if (nx_platform_stop() != NX_SUCCESS)
        return 5;
    return result == NX_SUCCESS ? 0 : 6;
}
