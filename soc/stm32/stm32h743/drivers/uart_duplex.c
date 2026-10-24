/**
 * \file            uart_duplex.c
 * \brief           Exact selected USART3 bidirectional DMA ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_uart_continuous_impl.h"

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static nx_stm32h743_uart_continuous_view_t
receive_view(nx_stm32h743_uart_duplex_state_t* state) {
    nx_stm32h743_uart_continuous_view_t view = {0};
    if (state != NULL) {
        view.receive = &state->receive;
        view.transmit = &state->transmit;
    }
    return view;
}
static bool joint_domain(void* context) {
    return nx_stm32h743_uart_continuous_live_parts(receive_view(context));
}
static nx_stm32h743_uart_dma_view_t
transmit_view(nx_stm32h743_uart_duplex_state_t* state) {
    nx_stm32h743_uart_dma_view_t view = {0};
    if (state != NULL) {
        view.uart = &state->receive.uart;
        view.transmit = &state->transmit;
        view.owner = state;
        view.owner_bytes = sizeof(*state);
        view.domain_check = joint_domain;
        view.domain_context = state;
    }
    return view;
}
static nx_result_t submit(void* context, nx_uart_tx_request_t* request) {
    nx_result_t result = nx_stm32h743_uart_continuous_validate_tx_parts(
        receive_view(context), request);
    return result == NX_SUCCESS ? nx_stm32h743_uart_dma_start_parts(
                                      transmit_view(context), request, false)
                                : result;
}
static nx_result_t start_admitted(void* context,
                                  nx_uart_tx_request_t* request) {
    nx_result_t result = nx_stm32h743_uart_continuous_validate_tx_parts(
        receive_view(context), request);
    return result == NX_SUCCESS ? nx_stm32h743_uart_dma_start_parts(
                                      transmit_view(context), request, true)
                                : result;
}
static nx_result_t cancel(void* context, nx_uart_tx_request_t* request) {
    return nx_stm32h743_uart_dma_cancel_parts(transmit_view(context), request);
}
static void service(void* context) {
    nx_stm32h743_uart_dma_service_parts(transmit_view(context));
    nx_stm32h743_uart_continuous_service_parts(receive_view(context));
}
static nx_result_t rx_start(void* context, nx_stream_t* stream) {
    return nx_stm32h743_uart_continuous_start_parts(receive_view(context),
                                                    stream);
}
static nx_result_t rx_stop(void* context) {
    return nx_stm32h743_uart_continuous_stop_rx_parts(receive_view(context));
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    return nx_stm32h743_uart_continuous_attach_wake_parts(receive_view(context),
                                                          wake, ceiling);
}
static nx_result_t stop(void* context) {
    nx_stm32h743_uart_duplex_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    state->receive.uart.closing = true;
    nx_result_t result = rx_stop(state);
    if (result == NX_SUCCESS)
        result = nx_stm32h743_uart_dma_stop_parts(transmit_view(state));
    if (result == NX_SUCCESS)
        result = nx_stm32h743_uart_continuous_stop_parts(receive_view(state));
    return result;
}
nx_result_t
nx_stm32h743_uart_duplex_initialize(nx_stm32h743_uart_duplex_state_t* state,
                                    uint32_t clock_hz, uint32_t poll_limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || state->transmit.dma == NULL ||
        state->transmit.tx == NULL || state->transmit.mux == NULL ||
        state->receive.regions == NULL ||
        state->transmit.regions != state->receive.regions ||
        state->transmit.region_count != state->receive.region_count)
        return NX_ERROR_INVALID;
    if (state->transmit.dma != DMA1 || state->transmit.tx != DMA1_Stream0 ||
        state->transmit.mux != DMAMUX1_Channel0)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result =
        nx_stm32h743_uart_dma_prepare_parts(transmit_view(state));
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32h743_uart_continuous_initialize_parts(
        receive_view(state), clock_hz, poll_limit);
    if (result == NX_SUCCESS)
        result = nx_stm32h743_uart_dma_initialize_parts(transmit_view(state));
    if (result != NX_SUCCESS &&
        (state->receive.retained || state->receive.uart.retained ||
         state->transmit.retained))
        (void)stop(state);
    return result;
}
void nx_stm32h743_uart_duplex_dma_irq(nx_stm32h743_uart_duplex_state_t* state) {
    nx_stm32h743_uart_dma_irq_parts(transmit_view(state));
}
void nx_stm32h743_uart_duplex_receive_irq(
    nx_stm32h743_uart_duplex_state_t* state) {
    nx_stm32h743_uart_continuous_dma_irq_parts(receive_view(state));
}
void nx_stm32h743_uart_duplex_uart_irq(
    nx_stm32h743_uart_duplex_state_t* state) {
    nx_stm32h743_uart_continuous_uart_irq_parts(receive_view(state));
    nx_stm32h743_uart_dma_irq_parts(transmit_view(state));
}
const nx_uart_ops_t nx_stm32h743_uart_duplex_ops = {
    submit, start_admitted, cancel,      service,  NULL,
    NULL,   stop,           attach_wake, rx_start, rx_stop};
