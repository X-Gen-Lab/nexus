/**
 * \file            external_bus.h
 * \brief           Explicit shared external-controller lifecycle face
 * \author          Nexus Team
 */
#ifndef NEXUS_EXTERNAL_BUS_H
#define NEXUS_EXTERNAL_BUS_H

#include "nexus/core/status.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Software ownership phase; it never proves physical device fitting. */
typedef enum {
    NX_EXTERNAL_BUS_UNAVAILABLE = 0,
    NX_EXTERNAL_BUS_STOPPED,
    NX_EXTERNAL_BUS_PREPARED,
    NX_EXTERNAL_BUS_CLEANUP_PENDING
} nx_external_bus_state_t;

/** \brief One readonly method table; callbacks receive the actual bus owner. */
typedef struct {
    nx_external_bus_state_t (*state)(const void* context);
    nx_result_t (*stop)(void* context);
} nx_external_bus_ops_t;

/** \brief Immutable controller face, distinct from every child bank endpoint.
 */
struct nx_external_bus_port {
    const nx_external_bus_ops_t* ops;
    void* context;
};

/**
 * \brief           Observe the selected bus owner's software phase
 * \param[in]       port: Stable face under the same serialized task executor
 * \return          Actual ownership phase, UNAVAILABLE for a missing method
 * \note            No preparation, device discovery or bank acquisition is
 *                  implicit. PREPARED is not physical clock or timing proof.
 */
nx_external_bus_state_t
nx_external_bus_port_state(const nx_external_bus_port_t* port);

/**
 * \brief           Withdraw selected banks and only the acquired bus effects
 * \param[in]       port: Same exclusive controller owner, storage still alive
 * \return          SUCCESS after complete withdrawal, BUSY while a bank loan
 *                  remains, or exact error retaining unfinished responsibility
 * \note            All endpoint consumers and CPU users must join first.
 *                  Timeout never revokes a derived pointer. Cold Platform
 *                  release restores the shared pad lease only afterward.
 */
nx_result_t nx_external_bus_port_stop(const nx_external_bus_port_t* port);

#ifdef __cplusplus
}
#endif
#endif
