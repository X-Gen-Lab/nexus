/**
 * \file            ethernet.c
 * \brief           Exact-context Ethernet dispatch and frame preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../private/eth_mac.h"
#include <stdint.h>

bool nx_eth_frame_valid(const void* data, size_t length) {
    if (data == NULL || length < NX_ETH_FRAME_MIN ||
        length > NX_ETH_FRAME_MAX || (uintptr_t)data > UINTPTR_MAX - length) {
        return false;
    }
    const uint8_t* bytes = data;
    uint16_t type = (uint16_t)((uint16_t)bytes[12] << 8U) | bytes[13];
    return type >= 0x0600U && type != 0x8100U && type != 0x88A8U;
}

nx_result_t nx_eth_tx_prepare(nx_eth_tx_request_t* request, const void* data,
                              size_t length, nx_time_us_t deadline) {
    if (request == NULL || !nx_eth_frame_valid(data, length) ||
        (uintptr_t)request > UINTPTR_MAX - sizeof(*request) ||
        ((uintptr_t)data < (uintptr_t)request + sizeof(*request) &&
         (uintptr_t)request < (uintptr_t)data + length)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->data = data;
        request->length = length;
    }
    return result;
}

#define NX_ETH_DISPATCH(port, method, ...)                                     \
    ((port) == NULL || (port)->ops == NULL                                     \
         ? NX_ERROR_INVALID                                                    \
         : ((port)->ops->method == NULL                                        \
                ? NX_ERROR_UNSUPPORTED                                         \
                : (port)->ops->method((port)->context, __VA_ARGS__)))
#define NX_ETH_DISPATCH_ZERO(port, method)                                     \
    ((port) == NULL || (port)->ops == NULL                                     \
         ? NX_ERROR_INVALID                                                    \
         : ((port)->ops->method == NULL                                        \
                ? NX_ERROR_UNSUPPORTED                                         \
                : (port)->ops->method((port)->context)))

nx_result_t nx_eth_port_submit(const nx_eth_port_t* port,
                               nx_eth_tx_request_t* request) {
    return request == NULL ? NX_ERROR_INVALID
                           : NX_ETH_DISPATCH(port, submit, request);
}
nx_result_t nx_eth_port_cancel(const nx_eth_port_t* port,
                               nx_eth_tx_request_t* request) {
    return request == NULL ? NX_ERROR_INVALID
                           : NX_ETH_DISPATCH(port, cancel, request);
}
nx_result_t nx_eth_port_service(const nx_eth_port_t* port) {
    return NX_ETH_DISPATCH_ZERO(port, service);
}
nx_result_t nx_eth_port_acquire(const nx_eth_port_t* port,
                                nx_eth_rx_frame_t* frame) {
    return frame == NULL ? NX_ERROR_INVALID
                         : NX_ETH_DISPATCH(port, acquire, frame);
}
nx_result_t nx_eth_port_release(const nx_eth_port_t* port,
                                const nx_eth_rx_frame_t* frame) {
    return frame == NULL ? NX_ERROR_INVALID
                         : NX_ETH_DISPATCH(port, release, frame);
}
nx_result_t nx_eth_port_state(const nx_eth_port_t* port,
                              nx_eth_state_t* state) {
    return state == NULL ? NX_ERROR_INVALID
                         : NX_ETH_DISPATCH(port, state, state);
}
nx_result_t nx_eth_port_apply_link(const nx_eth_port_t* port,
                                   const nx_eth_link_t* link) {
    return link == NULL ? NX_ERROR_INVALID
                        : NX_ETH_DISPATCH(port, apply_link, link);
}
nx_result_t nx_eth_port_stop(const nx_eth_port_t* port) {
    return NX_ETH_DISPATCH_ZERO(port, stop);
}
nx_result_t nx_eth_port_attach_wake(const nx_eth_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    return NX_ETH_DISPATCH(port, attach_wake, wake, syscall_ceiling);
}
