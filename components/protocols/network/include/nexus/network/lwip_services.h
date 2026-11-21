/**
 * \file            lwip_services.h
 * \brief           Explicit UDP, DHCP and DNS services for one lwIP executor
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_SERVICES_H
#define NEXUS_NETWORK_LWIP_SERVICES_H
#include "nexus/network/lwip_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_lwip_udp nx_lwip_udp_t;
typedef struct nx_lwip_dhcp nx_lwip_dhcp_t;
typedef struct nx_lwip_dns nx_lwip_dns_t;
/** \brief Copied peer identity; network byte order octets, host-order port. */
typedef struct {
    uint8_t address[4];
    uint16_t port;
} nx_ipv4_peer_t;
/**
 * \brief           Open one bounded copied-datagram receiver.
 * \param[in,out]   socket: Zeroed or closed exact-profile caller storage.
 * \param[in]       network: Live adapter in the one serialized NO_SYS domain.
 * \param[in]       local_port: Nonzero explicit UDP port.
 * \param[in,out]   buffer: Disjoint caller RX storage retained through close.
 * \param[in]       capacity: Positive maximum accepted datagram bytes.
 * \return          SUCCESS, INVALID, STATE, BUSY or EXHAUSTED; no worker.
 * \note            Task-only. One pending datagram; full/oversized packets drop
 *                  and increment the explicit dropped counter. No hidden queue.
 */
nx_result_t nx_lwip_udp_open(nx_lwip_udp_t* socket, nx_lwip_adapter_t* network,
                             uint16_t local_port, uint8_t* buffer,
                             size_t capacity);
/**
 * \brief           Send one copied datagram through the selected raw stack.
 * \param[in,out]   socket: Live serialized socket, outside raw callbacks.
 * \param[in]       peer: Explicit IPv4/port, copied during this call.
 * \param[in]       data: Bytes borrowed only until return; NULL if size zero.
 * \param[in]       size: Datagram size, at most 1472 (no IP fragmentation).
 * \param[in]       deadline: Finite same-domain absolute admission deadline.
 * \return          SUCCESS means local stack admission, never peer delivery;
 *                  BUSY/EXHAUSTED, TIMEOUT or provider/stack error otherwise.
 * \note            No implicit network progress; owner calls adapter_service.
 */
nx_result_t nx_lwip_udp_send(nx_lwip_udp_t* socket, const nx_ipv4_peer_t* peer,
                             const uint8_t* data, size_t size,
                             nx_time_us_t deadline);
/**
 * \brief           Copy and consume one complete datagram without truncation.
 * \param[in,out]   socket: Live serialized socket.
 * \param[out]      peer: Disjoint source identity, unchanged on failure.
 * \param[out]      data: Disjoint output storage, not retained.
 * \param[in]       capacity: Writable bytes, must fit the complete datagram.
 * \param[out]      size: Distinct count, unchanged on failure; zero is valid.
 * \return          SUCCESS, EMPTY or EXHAUSTED (packet stays pending).
 */
nx_result_t nx_lwip_udp_receive(nx_lwip_udp_t* socket, nx_ipv4_peer_t* peer,
                                uint8_t* data, size_t capacity, size_t* size);
/**
 * \brief           Detach callbacks and release local PCB before buffer reuse.
 * \param[in,out]   socket: Serialized socket, all callers joined.
 * \return          SUCCESS, BUSY or INVALID. Does not withdraw MAC loans.
 */
nx_result_t nx_lwip_udp_close(nx_lwip_udp_t* socket);
/**
 * \brief           Start DHCP on the explicitly selected managed profile.
 * \param[in,out]   client: Zeroed caller storage retained until stop success.
 * \param[in]       network: Attached adapter; all static-address users joined.
 * \return          SUCCESS admits discovery, not a lease; INVALID/STATE/BUSY/
 *                  EXHAUSTED otherwise. Requires explicit timer/network
 * service. \note            One DHCP owner per netif; no static fallback or
 * auto worker.
 */
nx_result_t nx_lwip_dhcp_start(nx_lwip_dhcp_t* client,
                               nx_lwip_adapter_t* network);
/**
 * \brief           Observe DHCP's actual supplied-address predicate.
 * \param[in]       client: Live serialized DHCP owner.
 * \param[out]      address: Distinct four-byte output, unchanged if no lease.
 * \return          SUCCESS, BUSY or INVALID; does not advance timers.
 */
nx_result_t nx_lwip_dhcp_address(const nx_lwip_dhcp_t* client,
                                 uint8_t address[4]);
/**
 * \brief           Stop DHCP locally and detach its caller-owned state.
 * \param[in,out]   client: All DHCP/address consumers joined, network still
 * live. \return          SUCCESS or INVALID; no promise of server lease
 * withdrawal. \note            Stop before adapter withdrawal. Product owns
 * reconfiguration.
 */
nx_result_t nx_lwip_dhcp_stop(nx_lwip_dhcp_t* client);
/**
 * \brief           Start one copied-name IPv4 DNS query on managed profile.
 * \param[in,out]   query: Zeroed or fully settled exact caller storage.
 * \param[in]       network: Live adapter and stable clock/timers through join.
 * \param[in]       hostname: NUL-terminated name, copied by bounded upstream
 * DNS. \param[in]       deadline: Finite absolute deadline in network time
 * domain. \return          SUCCESS admits query; result may already be
 * available. \note            Task-only. DNS server is set explicitly by the
 * product or DHCP. Cancellation cannot detach upstream callback; keep query
 * storage until poll/cancel returns a terminal result.
 */
nx_result_t nx_lwip_dns_start(nx_lwip_dns_t* query, nx_lwip_adapter_t* network,
                              const char* hostname, nx_time_us_t deadline);
/**
 * \brief           Observe resolution or await a retained upstream callback.
 * \param[in,out]   query: Serialized admitted query, timers still serviced.
 * \param[out]      address: Distinct four-byte output, written on success only.
 * \return          SUCCESS, BUSY, IO, TIMEOUT or CANCELLED. Timeout remains
 * BUSY until callback retirement; it does not revoke its borrow.
 */
nx_result_t nx_lwip_dns_poll(nx_lwip_dns_t* query, uint8_t address[4]);
/**
 * \brief           Cancel local delivery while retaining callback
 * responsibility. \param[in,out]   query: Serialized admitted query. \return
 * BUSY while upstream retains context; CANCELLED when joined. \note Does not
 * stop the global DNS engine or another query.
 */
nx_result_t nx_lwip_dns_cancel(nx_lwip_dns_t* query);
#ifdef __cplusplus
}
#endif
#endif
