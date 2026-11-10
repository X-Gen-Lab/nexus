/**
 * \file            spi_frames.c
 * \brief           Checked explicit SPI frame request dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/spi_frames.h"

/** \brief Bind operations to exactly one immutable controller context. */
static bool valid(const nx_spi_frames_port_t* port) {
    return port != NULL && port->ops != NULL && port->context != NULL;
}

/** \brief Prepare metadata only after every caller-owned extent is valid. */
nx_result_t nx_spi_frames_prepare(const nx_spi_frames_port_t* port,
                                  nx_spi_frames_request_t* request,
                                  const void* tx, void* rx, size_t words,
                                  nx_time_us_t deadline) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->prepare != NULL
               ? port->ops->prepare(port->context, request, tx, rx, words,
                                    deadline)
               : NX_ERROR_UNSUPPORTED;
}

/** \brief Dispatch without retaining anything in the public facade. */
nx_result_t nx_spi_frames_submit(const nx_spi_frames_port_t* port,
                                 nx_spi_frames_request_t* request) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->submit != NULL ? port->ops->submit(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

/** \brief Cancellation has the same exact-instance ownership as submit. */
nx_result_t nx_spi_frames_cancel(const nx_spi_frames_port_t* port,
                                 nx_spi_frames_request_t* request) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->cancel != NULL ? port->ops->cancel(port->context, request)
                                     : NX_ERROR_UNSUPPORTED;
}

/** \brief Owner service is explicit and creates no background worker. */
void nx_spi_frames_service(const nx_spi_frames_port_t* port) {
    if (valid(port) && port->ops->service != NULL)
        port->ops->service(port->context);
}

/** \brief Stop admission while the concrete owner joins hardware sources. */
nx_result_t nx_spi_frames_stop(const nx_spi_frames_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->stop != NULL ? port->ops->stop(port->context)
                                   : NX_ERROR_UNSUPPORTED;
}

/** \brief Recovery never submits or replays caller work. */
nx_result_t nx_spi_frames_recover(const nx_spi_frames_port_t* port) {
    if (!valid(port))
        return NX_ERROR_INVALID;
    return port->ops->recover != NULL ? port->ops->recover(port->context)
                                      : NX_ERROR_UNSUPPORTED;
}
