/**
 * \file            lwip_tcp.h
 *
 * \brief           Bounded copied TCP client for one NO_SYS stack
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_TCP_H
#define NEXUS_NETWORK_LWIP_TCP_H
#include "nexus/network/byte_stream.h"
#include "nexus/network/lwip_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_lwip_tcp nx_lwip_tcp_t;
typedef enum {
    NX_LWIP_TCP_IDLE = 0,
    NX_LWIP_TCP_CONNECTING,
    NX_LWIP_TCP_ESTABLISHED,
    NX_LWIP_TCP_CLOSING,
    NX_LWIP_TCP_CLOSED,
    NX_LWIP_TCP_FAILED
} nx_lwip_tcp_state_t;
/**
 * \brief One copied IPv4 peer and an explicit network progress budget.
 *
 * \note Network storage and its clock epoch outlive every client.
 *       They also outlive detached FIN/TIME_WAIT state in the stack.
 *
 * \note One serialized NO_SYS executor owns raw APIs and all clients.
 *       Call facade data/control outside raw callbacks. Nested mutation
 *       across facade instances returns BUSY without recursive progress.
 *
 * \note Nonzero I/O may service at most receive_budget Ethernet frames.
 *       No worker, DNS, reconnect policy or instance registry is added.
 */
typedef struct {
    nx_lwip_adapter_t* network;
    size_t receive_budget;
    uint8_t address[4];
    uint16_t port;
} nx_lwip_tcp_config_t;
/** \brief Owner snapshot; it grants no permission to mutate a connection. */
typedef struct {
    nx_lwip_tcp_state_t state;
    nx_result_t result;
    int stack_error;
    size_t pending_receive;
    size_t pending_transmit;
    bool peer_fin;
} nx_lwip_tcp_diagnostic_t;
/**
 * \brief Admit one asynchronous connect with a copied configuration.
 *
 * \param[in,out] client: Zeroed exact storage with all former users joined.
 *
 * \param[in] config: Live adapter, copied IPv4 peer, nonzero port/budget.
 *
 * \param[in] deadline: First finite absolute connect deadline.
 *
 * \return SUCCESS admits CONNECTING. BUSY rejects an occupied domain.
 *         INVALID rejects configuration; EXHAUSTED/IO report stack admission.
 *
 * \note Initialize lwIP once. SUCCESS never proves ESTABLISHED.
 *       Rejection retains no new PCB/pbuf/config reference for this client.
 *       An admitted client requires explicit close or abort.
 *
 * \note PCBs share the official stack pool and its reclamation policy.
 *       Allocation may progress pending closes and reclaim TIME_WAIT,
 *       LAST_ACK, CLOSING or lower-priority active PCBs. This can invoke
 *       other connections' error callbacks, even if this start later fails.
 *       Local rejection does not promise a globally unchanged stack.
 */
nx_result_t nx_lwip_tcp_start(nx_lwip_tcp_t* client,
                              const nx_lwip_tcp_config_t* config,
                              nx_time_us_t deadline);
/**
 * \brief Perform one budgeted network pass and progress this connection.
 *
 * \param[in,out] client: Live client owned by the serialized task executor.
 *
 * \return SUCCESS for progress; BUSY for connecting/congestion/reentry;
 *         TIMEOUT for the locked deadline, or an actual provider/stack error.
 *
 * \note No wait or worker. Fatal errors remain sticky until explicit abort.
 *       Expiry does not release PCBs, pbufs or any underlying MAC loan.
 */
nx_result_t nx_lwip_tcp_service(nx_lwip_tcp_t* client);
/**
 * \brief Copy an immediate partial write into bounded stack storage.
 *
 * \param[in,out] client: Live serialized task owner, outside raw callbacks.
 *
 * \param[in] data: Stable input for this call; never retained after return.
 *
 * \param[in] bytes: Requested length; zero needs no clock/provider progress.
 *
 * \param[out] accepted: Disjoint copied count; zero on valid BUSY/error.
 *
 * \param[in] deadline: Finite absolute admission deadline in adapter time.
 *
 * \return SUCCESS with a positive copied count, BUSY without admission,
 *         TIMEOUT before admission, STATE while closing, or a sticky failure.
 *
 * \note Positive acceptance is TCP queue admission, not peer delivery.
 *       Delivery/acknowledgement policy belongs to the caller.
 *       A later output failure cannot undo an already copied acceptance.
 *       Invalid aliased counts are rejected before writing that count.
 */
nx_result_t nx_lwip_tcp_try_write(nx_lwip_tcp_t* client, const uint8_t* data,
                                  size_t bytes, size_t* accepted,
                                  nx_time_us_t deadline);
/**
 * \brief Copy partial RX data and credit only bytes actually consumed.
 *
 * \param[in,out] client: Serialized task owner; reads may drain while closing.
 *
 * \param[out] data: Unretained output disjoint from all known live storage.
 *
 * \param[in] bytes: Capacity; zero needs no clock/provider progress.
 *
 * \param[out] received: Disjoint count; zero on valid BUSY/error or EOF.
 *
 * \param[in] deadline: Finite absolute attempt deadline in adapter time.
 *
 * \return SUCCESS with data, SUCCESS/count zero for orderly TCP EOF,
 *         BUSY without data, TIMEOUT, STATE when detached, or sticky failure.
 *
 * \note One accepted pbuf chain is held. Refused chains remain lwIP-owned.
 *       Buffered bytes precede FIN; peer half-close still permits writes.
 *       TLS independently authenticates close-notify. Invalid aliased counts
 *       are rejected before writing that count.
 */
nx_result_t nx_lwip_tcp_try_read(nx_lwip_tcp_t* client, uint8_t* data,
                                 size_t bytes, size_t* received,
                                 nx_time_us_t deadline);
/**
 * \brief Close admission and detach after explicit local drain.
 *
 * \param[in,out] client: Producers joined; reads may drain retained RX.
 *
 * \param[in] deadline: First finite close deadline; never implicitly extended.
 *
 * \return SUCCESS detaches this context. BUSY retains held/refused RX,
 *         unacknowledged TX or a failed close. TIMEOUT retains all ownership.
 *
 * \note CONNECTING may close before establishment. Callbacks detach before
 *       successful tcp_close; its PCB must never be accessed afterwards.
 *       The pinned stack can defer FIN allocation while returning success.
 *       FIN/TIME_WAIT belong to the still-live stack/network. This operation
 *       does not prove global or PHY drain. Timeout requires explicit abort.
 */
nx_result_t nx_lwip_tcp_close(nx_lwip_tcp_t* client, nx_time_us_t deadline);
/**
 * \brief Explicitly discard data, detach callbacks and abort an owned PCB.
 *
 * \param[in,out] client: Exclusive storage after facade users have joined.
 *
 * \return SUCCESS after local cleanup, including a cold client; BUSY while
 *         any facade operation/callback executes; INVALID for a null client.
 *
 * \note BUSY releases nothing. The error callback already freed its PCB.
 *       Product code separately drains the MAC/netif after all clients join.
 */
nx_result_t nx_lwip_tcp_abort(nx_lwip_tcp_t* client);
/**
 * \brief Return the existing readonly byte face without progressing it.
 *
 * \param[in] client: Storage retained through every facade user.
 *
 * \return Stable face while initialized, including CONNECTING/FAILED;
 *         NULL before admission or after successful close/abort.
 *
 * \note Readonly query in the same serialized task domain. A face is not
 *       connection readiness; its data methods return the actual state.
 */
const nx_byte_stream_t* nx_lwip_tcp_stream(const nx_lwip_tcp_t* client);
/**
 * \brief Copy a readonly owner snapshot without progressing the stack.
 *
 * \param[in] client: Live or locally detached storage, still allocated.
 *
 * \param[out] diagnostic: Output disjoint from known live control/data.
 *
 * \return SUCCESS for the snapshot; INVALID for null or aliased output.
 *
 * \note Use the serialized executor. A snapshot neither admits I/O nor
 *       authorizes release. Detached queries never follow old borrowed state.
 */
nx_result_t nx_lwip_tcp_diagnostic(const nx_lwip_tcp_t* client,
                                   nx_lwip_tcp_diagnostic_t* diagnostic);
#ifdef __cplusplus
}
#endif
#endif
