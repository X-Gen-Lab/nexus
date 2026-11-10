/**
 * \file            i2c_target.c
 * \brief           Checked finite I2C target and SMBus dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c_target.h"

/** \brief Establish complete aligned spans before typed access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           size <= UINTPTR_MAX - (uintptr_t)pointer;
}

/** \brief Compare valid spans without computing an overflowing end. */
static bool overlap(const void* left, size_t ln, const void* right, size_t rn) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return ln != 0U && rn != 0U && (a <= b ? b - a < ln : a - b < rn);
}

/** \brief A static lookup face never acquires hardware or payload storage. */
static bool valid(const nx_i2c_target_port_t* port) {
    return span(port, sizeof(*port), _Alignof(nx_i2c_target_port_t)) &&
           span(port->ops, sizeof(*port->ops), _Alignof(nx_i2c_target_ops_t)) &&
           port->context != NULL;
}

/** \brief Request metadata must not overwrite immutable dispatch storage. */
static bool request_valid(const nx_i2c_target_port_t* port,
                          const nx_i2c_target_request_t* request) {
    return span(request, sizeof(*request), _Alignof(nx_i2c_target_request_t)) &&
           !overlap(request, sizeof(*request), port, sizeof(*port)) &&
           !overlap(request, sizeof(*request), port->ops, sizeof(*port->ops)) &&
           request != port->context;
}

/** \brief Check payload extents against the face and descriptor before use. */
static bool payload_valid(const nx_i2c_target_port_t* port,
                          const nx_i2c_target_request_t* request,
                          const void* payload, size_t bytes) {
    return bytes == 0U
               ? payload == NULL
               : span(payload, bytes, 1U) &&
                     !overlap(payload, bytes, request, sizeof(*request)) &&
                     !overlap(payload, bytes, port, sizeof(*port)) &&
                     !overlap(payload, bytes, port->ops, sizeof(*port->ops));
}

/** \brief Preparation binds one existing provider without taking a borrow. */
nx_result_t nx_i2c_target_prepare(const nx_i2c_target_port_t* port,
                                  nx_i2c_target_request_t* request, uint8_t* rx,
                                  size_t rx_capacity, const uint8_t* tx,
                                  size_t tx_length,
                                  nx_i2c_target_direction_t direction,
                                  nx_time_us_t deadline) {
    if (!valid(port) || !request_valid(port, request) ||
        !payload_valid(port, request, rx, rx_capacity) ||
        !payload_valid(port, request, tx, tx_length) ||
        overlap(rx, rx_capacity, tx, tx_length))
        return NX_ERROR_INVALID;
    return port->ops->prepare == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->prepare(port->context, request, rx, rx_capacity, tx,
                                    tx_length, direction, deadline);
}

/** \brief Only the selected provider may acknowledge and accept traffic. */
nx_result_t nx_i2c_target_submit(const nx_i2c_target_port_t* port,
                                 nx_i2c_target_request_t* request) {
    if (!valid(port) || !request_valid(port, request) ||
        !payload_valid(port, request, request->rx, request->rx_capacity) ||
        !payload_valid(port, request, request->tx, request->tx_length) ||
        overlap(request->rx, request->rx_capacity, request->tx,
                request->tx_length))
        return NX_ERROR_INVALID;
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}

/** \brief Cancellation preserves every accepted reference until settlement.
 */
nx_result_t nx_i2c_target_cancel(const nx_i2c_target_port_t* port,
                                 nx_i2c_target_request_t* request) {
    if (!valid(port) || !request_valid(port, request))
        return NX_ERROR_INVALID;
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}

/** \brief Progress has no implicit worker or application callback. */
void nx_i2c_target_service(const nx_i2c_target_port_t* port) {
    if (valid(port) && port->ops->service != NULL)
        port->ops->service(port->context);
}

/** \brief The provider proves source withdrawal before releasing its owner.
 */
nx_result_t nx_i2c_target_stop(const nx_i2c_target_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}

/** \brief Explicit recovery never replays old requests or stale deadlines. */
nx_result_t nx_i2c_target_recover(const nx_i2c_target_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->recover == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->recover(port->context);
}
