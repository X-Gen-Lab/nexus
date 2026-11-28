/**
 * \file            main.c
 * \brief           Static F107 MII IPv4 consumer through the pinned stack.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "lwip/init.h"
#include "nexus/dp83848/dp83848.h"
#include "nexus/network/lwip_storage.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "raw_frames.h"
#include <stdalign.h>

_Static_assert(NO_SYS == 1 && LWIP_IPV4 == 1 && LWIP_ICMP == 1 && LWIP_ARP == 1,
               "Use the single-owner IPv4 reference");
#if LWIP_UDP && LWIP_TCP
#include "lwip/udp.h"
#include "nexus/network/lwip_tcp_storage.h"

_Static_assert(MEM_SIZE == 16384 && PBUF_POOL_SIZE == 24 && LWIP_DHCP == 0 &&
                   LWIP_DNS == 0,
               "Use the existing copied TCP/UDP profile");
#else
_Static_assert(LWIP_UDP == 0 && LWIP_TCP == 0 && LWIP_RAW == 0 &&
                   MEM_SIZE == 4096 && PBUF_POOL_SIZE == 4,
               "Use only the bounded IPv4 profile");
#endif

static f107_raw_frames_t frames;
static nx_lwip_adapter_t network;
static alignas(4) uint8_t tx[NX_ETH_FRAME_MAX];
static nx_eth_fragment_t rx_view;

#if LWIP_UDP && LWIP_TCP
static nx_lwip_tcp_t client;
static struct udp_pcb* datagrams;
static bool client_started;
static bool byte_sent;
static volatile uint32_t fixture_seed;
static uint32_t fixture_random_state;

/* A reproducible bench fixture seed, admitted before any hardware starts. */
uint32_t nx_lwip_port_random_u32(void) {
    fixture_random_state ^= fixture_random_state << 13U;
    fixture_random_state ^= fixture_random_state >> 17U;
    fixture_random_state ^= fixture_random_state << 5U;
    return fixture_random_state;
}

static void discard_datagram(void* context, struct udp_pcb* receiver,
                             struct pbuf* packet, const ip_addr_t* peer,
                             u16_t source_port) {
    (void)context;
    (void)receiver;
    (void)peer;
    (void)source_port;
    (void)pbuf_free(packet);
}

static nx_result_t start_transports(nx_time_us_t until) {
    datagrams = udp_new_ip_type(IPADDR_TYPE_V4);
    if (datagrams == NULL)
        return NX_ERROR_EXHAUSTED;
    if (udp_bind(datagrams, IP_ADDR_ANY, 49152U) != ERR_OK)
        return NX_ERROR_IO;
    udp_recv(datagrams, discard_datagram, NULL);

    const uint8_t probe = 'N';
    struct pbuf* packet = pbuf_alloc(PBUF_TRANSPORT, 1U, PBUF_RAM);
    if (packet == NULL)
        return NX_ERROR_EXHAUSTED;
    ip_addr_t peer;
    IP_ADDR4(&peer, 192U, 0U, 2U, 3U);
    err_t sent = pbuf_take(packet, &probe, 1U);
    if (sent == ERR_OK)
        sent = udp_sendto(datagrams, packet, &peer, 49153U);
    (void)pbuf_free(packet);
    if (sent != ERR_OK)
        return NX_ERROR_IO;

    const nx_lwip_tcp_config_t config = {.network = &network,
                                         .receive_budget = 1U,
                                         .address = {192U, 0U, 2U, 3U},
                                         .port = 49153U};
    const nx_result_t result = nx_lwip_tcp_start(&client, &config, until);
    client_started = result == NX_SUCCESS;
    return result;
}

static nx_result_t service_transports(nx_time_us_t until) {
    nx_result_t result = nx_lwip_tcp_service(&client);
    if (result != NX_SUCCESS)
        return result == NX_ERROR_BUSY ? NX_SUCCESS : result;
    uint8_t byte = 'N';
    size_t count;
    if (!byte_sent) {
        result = nx_lwip_tcp_try_write(&client, &byte, 1U, &count, until);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
            return result;
        byte_sent = count == 1U;
    }
    result = nx_lwip_tcp_try_read(&client, &byte, 1U, &count, until);
    return result == NX_ERROR_BUSY || result == NX_ERROR_EMPTY ? NX_SUCCESS
                                                               : result;
}

static nx_result_t stop_transports(void) {
    if (datagrams != NULL) {
        udp_remove(datagrams);
        datagrams = NULL;
    }
    if (!client_started)
        return NX_SUCCESS;
    const nx_result_t result = nx_lwip_tcp_abort(&client);
    if (result == NX_SUCCESS)
        client_started = false;
    return result;
}
#endif

static nx_time_us_t clock_read(void* context) {
    (void)context;
    return nx_time_now_us();
}

uint32_t nx_lwip_port_now_ms(void) {
    return (uint32_t)(nx_time_now_us() / 1000U);
}

void nx_lwip_port_assert(const char* message) {
    (void)message;
    /* The live static buffers stay owned on failed withdrawal. */
    for (;;) {
    }
}

static nx_result_t observe_phy(const nx_mdio_port_t* port,
                               nx_eth_link_t* link) {
    nx_dp83848_t phy = {
        .mdio = port, .expected_id = NX_DP83848_NS_ID, .address = 1U};
    const nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), 2000000U);
    nx_result_t result = nx_dp83848_probe(&phy, limit);
    if (result == NX_ERROR_UNSUPPORTED) {
        phy.expected_id = NX_DP83848_TI_ID;
        result = nx_dp83848_probe(&phy, limit);
    }
    if (result == NX_SUCCESS) {
        const nx_dp83848_mode_t mode = {.advertisement = NX_DP83848_10_FULL |
                                                         NX_DP83848_100_FULL,
                                        .autonegotiation = true};
        result = nx_dp83848_configure(&phy, &mode, limit);
    }
    while (result == NX_SUCCESS &&
           !nx_deadline_expired(limit, nx_time_now_us())) {
        result = nx_dp83848_link(&phy, link, limit);
        if (result != NX_SUCCESS || link->up)
            return result;
    }
    return result == NX_SUCCESS ? NX_ERROR_TIMEOUT : result;
}

/** \brief Retry the provider's reset/readback with a finite attempt budget. */
static nx_result_t close(void) {
    const nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), 5000U);
    for (unsigned attempt = 0U; attempt < 32U; ++attempt) {
#if LWIP_UDP && LWIP_TCP
        nx_result_t result = stop_transports();
        if (result == NX_SUCCESS)
            result = nx_lwip_adapter_stop(&network);
#else
        nx_result_t result = nx_lwip_adapter_stop(&network);
#endif
        if (result == NX_SUCCESS)
            result = nx_platform_stop();
        if (result != NX_ERROR_BUSY)
            return result;
        if (nx_deadline_expired(limit, nx_time_now_us()))
            break;
    }
    return NX_ERROR_TIMEOUT;
}

static nx_result_t service_network(void) {
    const nx_eth_port_t* raw = nx_factory_eth_ref(NX_ETH_REF_LAN);
    const nx_mdio_port_t* mdio = nx_factory_mdio_ref(NX_MDIO_REF_LAN);
    if (raw == NULL || mdio == NULL)
        return NX_ERROR_STATE;
    f107_raw_frames_init(&frames, raw);
    lwip_init();
    const nx_lwip_config_t config = {
        .port = &frames.face,
        .clock = {clock_read, NULL},
        .tx_timeout_us = 5000U,
        .tx_buffer = tx,
        .tx_capacity = sizeof(tx),
        .rx_views = &rx_view,
        .rx_capacity = 1U,
        .hardware_address = {2U, 12U, 52U, 0U, 0U, 1U},
        .address = {192U, 0U, 2U, 2U},
        .netmask = {255U, 255U, 255U, 0U},
    };
    nx_result_t result = nx_lwip_adapter_start(&network, &config);
    if (result != NX_SUCCESS)
        return result;
    nx_eth_link_t link = {0};
    result = observe_phy(mdio, &link);
    if (result == NX_SUCCESS && link.up)
        result = nx_lwip_adapter_apply_link(&network, &link);
    const nx_time_us_t until = nx_deadline_after(nx_time_now_us(), 100000U);
#if LWIP_UDP && LWIP_TCP
    if (result == NX_SUCCESS)
        result = start_transports(until);
    while (result == NX_SUCCESS &&
           !nx_deadline_expired(until, nx_time_now_us()))
        result = service_transports(until);
#else
    while (result == NX_SUCCESS &&
           !nx_deadline_expired(until, nx_time_now_us()))
        result = nx_lwip_adapter_service(&network, 1U);
#endif
    return result;
}

int main(void) {
#if LWIP_UDP && LWIP_TCP
    fixture_random_state = fixture_seed;
    if (fixture_random_state == 0U)
        return 3;
#endif
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        if (close() != NX_SUCCESS)
            nx_lwip_port_assert("platform retains startup resources");
        return 1;
    }
    const nx_result_t result = service_network();
    if (close() != NX_SUCCESS)
        nx_lwip_port_assert("network or platform retains resources");
    return result == NX_SUCCESS ? 0 : 2;
}
