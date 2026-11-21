/**
 * \file            lwip_listener.h
 * \brief           Bounded raw TCP listener over explicit caller connection
 * slots \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_LISTENER_H
#define NEXUS_NETWORK_LWIP_LISTENER_H
#include "nexus/network/lwip_tcp.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_lwip_tcp_listener nx_lwip_tcp_listener_t;
/**
 * \brief           Listen with an exact caller-selected array of idle clients.
 * \param[in,out]   listener: Zeroed or stopped exact-profile caller storage.
 * \param[in]       network: Attached network/clock retained through all
 * clients. \param[in,out]   clients: Disjoint caller slots, zeroed or fully
 * stopped. \param[in]       count: Positive exact connection capacity, no
 * hidden pool. \param[in]       port: Explicit nonzero local TCP port.
 * \param[in]       receive_budget: Nonzero per-client network progress budget.
 * \return          SUCCESS, INVALID, BUSY or EXHAUSTED; opens only local
 * listener. \note            Same serialized NO_SYS executor. Incoming
 * connections bind directly into an idle slot and use the existing TCP byte
 *                  stream. Full capacity aborts new connections; no backlog
 *                  promise. While listening, only its callback reuses idle
 * slots; application must join former face users before closing a slot.
 */
nx_result_t nx_lwip_tcp_listen(nx_lwip_tcp_listener_t* listener,
                               nx_lwip_adapter_t* network,
                               nx_lwip_tcp_t* clients, size_t count,
                               uint16_t port, size_t receive_budget);
/**
 * \brief           Stop accepting connections and detach listener callbacks.
 * \param[in,out]   listener: Serialized listener, external callers joined.
 * \return          SUCCESS, INVALID or BUSY; existing clients stay live.
 * \note            Caller separately closes/aborts and joins every client
 * before reclaiming their array, network, clock or MAC resources.
 */
nx_result_t nx_lwip_tcp_listener_stop(nx_lwip_tcp_listener_t* listener);
#ifdef __cplusplus
}
#endif
#endif
