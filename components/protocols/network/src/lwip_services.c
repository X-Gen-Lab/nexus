/**
 * \file            lwip_services.c
 * \brief           Bounded datagrams and explicit DHCP/DNS callback ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "lwip/dns.h"
#include "nexus/network/lwip_services_storage.h"
#include "nexus/network/lwip_storage.h"
#include <string.h>

/** \brief Validate buffer extents and reject aliased live objects. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0 && bn != 0 && x < y + bn && y < x + an);
}
/** \brief Admit only the already attached, nonclosing selected stack owner. */
static bool live(const nx_lwip_adapter_t* n) {
    return n != NULL && n->attached && !n->closing &&
           n->config.clock.read != NULL;
}
/** \brief No mutation may overlap known live adapter or hardware staging. */
static bool outside(const nx_lwip_adapter_t* n, const void* data, size_t size) {
    return !overlap(n, sizeof(*n), data, size) &&
           !overlap(n->config.tx_buffer, n->config.tx_capacity, data, size) &&
           !overlap(n->config.rx_views,
                    n->config.rx_capacity * sizeof(*n->config.rx_views), data,
                    size);
}
/** \brief Copy one datagram or account for a bounded, explicit drop. */
static void udp_received(void* context, struct udp_pcb* pcb,
                         struct pbuf* packet, const ip_addr_t* address,
                         u16_t port) {
    nx_lwip_udp_t* s = context;
    if (packet == NULL) {
        return;
    }
    if (s->pcb != pcb || !live(s->network) || s->pending ||
        packet->tot_len > s->capacity || !IP_IS_V4(address) ||
        pbuf_copy_partial(packet, s->buffer, packet->tot_len, 0) !=
            packet->tot_len) {
        if (s->dropped != UINT32_MAX) {
            ++s->dropped;
        }
    } else {
        const ip4_addr_t* a = ip_2_ip4(address);
        s->peer = (nx_ipv4_peer_t){
            {ip4_addr1(a), ip4_addr2(a), ip4_addr3(a), ip4_addr4(a)}, port};
        s->received = packet->tot_len;
        s->pending = true;
    }
    (void)pbuf_free(packet);
}
/** \brief Bind one exact static receive buffer and one bounded stack PCB. */
nx_result_t nx_lwip_udp_open(nx_lwip_udp_t* s, nx_lwip_adapter_t* network,
                             uint16_t local_port, uint8_t* buffer,
                             size_t capacity) {
    if (s == NULL || !live(network) || local_port == 0 || buffer == NULL ||
        capacity == 0 || capacity > 1472 ||
        overlap(s, sizeof(*s), buffer, capacity) ||
        !outside(network, s, sizeof(*s)) ||
        !outside(network, buffer, capacity)) {
        return NX_ERROR_INVALID;
    }
    if (s->pcb != NULL || s->active) {
        return NX_ERROR_BUSY;
    }
    struct udp_pcb* pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (pcb == NULL) {
        return NX_ERROR_EXHAUSTED;
    }
    err_t result = udp_bind(pcb, IP4_ADDR_ANY, local_port);
    if (result != ERR_OK) {
        udp_remove(pcb);
        return result == ERR_USE ? NX_ERROR_BUSY : NX_ERROR_IO;
    }
    memset(s, 0, sizeof(*s));
    s->network = network;
    s->pcb = pcb;
    s->buffer = buffer;
    s->capacity = capacity;
    udp_bind_netif(pcb, &network->interface);
    udp_recv(pcb, udp_received, s);
    return NX_SUCCESS;
}
/** \brief Use upstream copied pbuf ownership and leave progress to the
 * executor. */
nx_result_t nx_lwip_udp_send(nx_lwip_udp_t* s, const nx_ipv4_peer_t* peer,
                             const uint8_t* data, size_t size,
                             nx_time_us_t deadline) {
    if (s == NULL || peer == NULL || peer->port == 0 ||
        (data == NULL && size != 0) || size > 1472 ||
        deadline == NX_DEADLINE_NEVER ||
        overlap(s, sizeof(*s), peer, sizeof(*peer)) ||
        overlap(s, sizeof(*s), data, size) ||
        overlap(s->buffer, s->capacity, data, size)) {
        return NX_ERROR_INVALID;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (s->pcb == NULL || !live(s->network)) {
        return NX_ERROR_STATE;
    }
    if (!outside(s->network, data, size) ||
        !outside(s->network, peer, sizeof(*peer))) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now =
        s->network->config.clock.read(s->network->config.clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (now >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    s->active = true;
    struct pbuf* packet = pbuf_alloc(PBUF_TRANSPORT, (u16_t)size, PBUF_RAM);
    if (packet == NULL) {
        s->active = false;
        return NX_ERROR_EXHAUSTED;
    }
    err_t error = size != 0 ? pbuf_take(packet, data, (u16_t)size) : ERR_OK;
    if (error == ERR_OK) {
        ip_addr_t address;
        IP_ADDR4(&address, peer->address[0], peer->address[1], peer->address[2],
                 peer->address[3]);
        error = udp_sendto_if(s->pcb, packet, &address, peer->port,
                              &s->network->interface);
    }
    (void)pbuf_free(packet);
    s->active = false;
    return error == ERR_OK                        ? NX_SUCCESS
           : error == ERR_MEM || error == ERR_BUF ? NX_ERROR_BUSY
                                                  : NX_ERROR_IO;
}
/** \brief Copy a whole pending datagram without silent truncation. */
nx_result_t nx_lwip_udp_receive(nx_lwip_udp_t* s, nx_ipv4_peer_t* peer,
                                uint8_t* data, size_t capacity, size_t* size) {
    if (s == NULL || peer == NULL || size == NULL ||
        (data == NULL && capacity != 0) ||
        overlap(s, sizeof(*s), peer, sizeof(*peer)) ||
        overlap(s, sizeof(*s), size, sizeof(*size)) ||
        overlap(s, sizeof(*s), data, capacity) ||
        overlap(s->buffer, s->capacity, peer, sizeof(*peer)) ||
        overlap(s->buffer, s->capacity, size, sizeof(*size)) ||
        overlap(s->buffer, s->capacity, data, capacity) ||
        overlap(peer, sizeof(*peer), size, sizeof(*size)) ||
        overlap(peer, sizeof(*peer), data, capacity) ||
        overlap(size, sizeof(*size), data, capacity)) {
        return NX_ERROR_INVALID;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (s->pcb == NULL || !live(s->network)) {
        return NX_ERROR_STATE;
    }
    if (!outside(s->network, peer, sizeof(*peer)) ||
        !outside(s->network, size, sizeof(*size)) ||
        !outside(s->network, data, capacity)) {
        return NX_ERROR_INVALID;
    }
    if (!s->pending) {
        return NX_ERROR_EMPTY;
    }
    if (capacity < s->received) {
        return NX_ERROR_EXHAUSTED;
    }
    if (s->received != 0) {
        memcpy(data, s->buffer, s->received);
    }
    *peer = s->peer;
    *size = s->received;
    s->pending = false;
    s->received = 0;
    return NX_SUCCESS;
}
/** \brief Detach upstream references before releasing caller receive storage.
 */
nx_result_t nx_lwip_udp_close(nx_lwip_udp_t* s) {
    if (s == NULL) {
        return NX_ERROR_INVALID;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (s->pcb != NULL) {
        udp_recv(s->pcb, NULL, NULL);
        udp_remove(s->pcb);
    }
    memset(s, 0, sizeof(*s));
    return NX_SUCCESS;
}
/** \brief Supply static DHCP state rather than a second hidden allocator. */
nx_result_t nx_lwip_dhcp_start(nx_lwip_dhcp_t* c, nx_lwip_adapter_t* network) {
    if (c == NULL || !live(network) || !outside(network, c, sizeof(*c))) {
        return NX_ERROR_INVALID;
    }
    if (c->network != NULL || netif_dhcp_data(&network->interface) != NULL) {
        return NX_ERROR_BUSY;
    }
    memset(c, 0, sizeof(*c));
    c->network = network;
    dhcp_set_struct(&network->interface, &c->state);
    err_t error = dhcp_start(&network->interface);
    if (error != ERR_OK) {
        dhcp_stop(&network->interface);
        dhcp_remove_struct(&network->interface);
        c->network = NULL;
        return error == ERR_MEM ? NX_ERROR_EXHAUSTED : NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Read the real lease predicate, never infer it from nonzero metadata.
 */
nx_result_t nx_lwip_dhcp_address(const nx_lwip_dhcp_t* c, uint8_t address[4]) {
    if (c == NULL || address == NULL || c->network == NULL ||
        overlap(c, sizeof(*c), address, 4) ||
        !outside(c->network, address, 4)) {
        return NX_ERROR_INVALID;
    }
    if (!live(c->network) ||
        netif_dhcp_data(&c->network->interface) != &c->state) {
        return NX_ERROR_STATE;
    }
    if (!dhcp_supplied_address(&c->network->interface)) {
        return NX_ERROR_BUSY;
    }
    const ip4_addr_t* a = netif_ip4_addr(&c->network->interface);
    const uint8_t copy[] = {ip4_addr1(a), ip4_addr2(a), ip4_addr3(a),
                            ip4_addr4(a)};
    memcpy(address, copy, 4);
    return NX_SUCCESS;
}
/** \brief Join DHCP's netif references before allowing its state to be reused.
 */
nx_result_t nx_lwip_dhcp_stop(nx_lwip_dhcp_t* c) {
    if (c == NULL) {
        return NX_ERROR_INVALID;
    }
    if (c->network != NULL) {
        if (netif_dhcp_data(&c->network->interface) != &c->state) {
            return NX_ERROR_STATE;
        }
        dhcp_stop(&c->network->interface);
        dhcp_remove_struct(&c->network->interface);
        c->network = NULL;
    }
    return NX_SUCCESS;
}
/** \brief DNS's final callback retires the borrowed query context. */
static void dns_found(const char* name, const ip_addr_t* address,
                      void* context) {
    (void)name;
    nx_lwip_dns_t* q = context;
    if (!q->cancelled && q->result == NX_ERROR_BUSY) {
        nx_time_us_t now =
            q->network->config.clock.read(q->network->config.clock.context);
        if (now == NX_DEADLINE_NEVER || now >= q->deadline) {
            q->result = NX_ERROR_TIMEOUT;
        } else if (address != NULL && IP_IS_V4(address)) {
            const ip4_addr_t* a = ip_2_ip4(address);
            const uint8_t copy[] = {ip4_addr1(a), ip4_addr2(a), ip4_addr3(a),
                                    ip4_addr4(a)};
            memcpy(q->address, copy, 4);
            q->result = NX_SUCCESS;
        } else {
            q->result = NX_ERROR_IO;
        }
    }
    q->pending = false;
}
/** \brief Bind exactly one upstream callback loan to the explicit query. */
nx_result_t nx_lwip_dns_start(nx_lwip_dns_t* q, nx_lwip_adapter_t* network,
                              const char* hostname, nx_time_us_t deadline) {
    if (q == NULL || !live(network) || hostname == NULL ||
        deadline == NX_DEADLINE_NEVER || !outside(network, q, sizeof(*q))) {
        return NX_ERROR_INVALID;
    }
    if (q->pending) {
        return NX_ERROR_BUSY;
    }
    size_t length = 0;
    while (length < DNS_MAX_NAME_LENGTH && hostname[length] != '\0') {
        ++length;
    }
    if (length == 0 || length == DNS_MAX_NAME_LENGTH ||
        overlap(q, sizeof(*q), hostname, length + 1) ||
        !outside(network, hostname, length + 1)) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now =
        network->config.clock.read(network->config.clock.context);
    if (now == NX_DEADLINE_NEVER || now >= deadline) {
        return now == NX_DEADLINE_NEVER ? NX_ERROR_STATE : NX_ERROR_TIMEOUT;
    }
    memset(q, 0, sizeof(*q));
    q->network = network;
    q->deadline = deadline;
    q->result = NX_ERROR_BUSY;
    q->pending = true;
    ip_addr_t address;
    err_t error = dns_gethostbyname_addrtype(hostname, &address, dns_found, q,
                                             LWIP_DNS_ADDRTYPE_IPV4);
    if (error == ERR_OK) {
        dns_found(hostname, &address, q);
    } else if (error != ERR_INPROGRESS) {
        memset(q, 0, sizeof(*q));
        return error == ERR_MEM ? NX_ERROR_EXHAUSTED : NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Expiry cancels delivery but cannot revoke upstream callback storage.
 */
nx_result_t nx_lwip_dns_poll(nx_lwip_dns_t* q, uint8_t address[4]) {
    if (q == NULL || address == NULL || q->network == NULL ||
        overlap(q, sizeof(*q), address, 4) ||
        !outside(q->network, address, 4)) {
        return NX_ERROR_INVALID;
    }
    if (q->pending) {
        nx_time_us_t now =
            q->network->config.clock.read(q->network->config.clock.context);
        if (now == NX_DEADLINE_NEVER || now >= q->deadline) {
            q->result = NX_ERROR_TIMEOUT;
        }
        return NX_ERROR_BUSY;
    }
    if (q->cancelled) {
        return NX_ERROR_CANCELLED;
    }
    if (q->result == NX_SUCCESS) {
        memcpy(address, q->address, 4);
    }
    return q->result;
}
/** \brief Keep the callback context alive until upstream signals retirement. */
nx_result_t nx_lwip_dns_cancel(nx_lwip_dns_t* q) {
    if (q == NULL || q->network == NULL) {
        return NX_ERROR_INVALID;
    }
    q->cancelled = true;
    return q->pending ? NX_ERROR_BUSY : NX_ERROR_CANCELLED;
}
