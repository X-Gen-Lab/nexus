/**
 * \file            common_network_services_test.cpp
 * \brief           Real managed lwIP UDP, DHCP and DNS ownership contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_listener_storage.h"
#include "nexus/network/lwip_services_storage.h"
#include "nexus/network/lwip_storage.h"
#include "nexus/network/lwip_tcp_storage.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
#include "lwip/dns.h"
#include "lwip/inet_chksum.h"
#include "lwip/init.h"
#include "lwip/ip4.h"
}
namespace {
uint32_t ticks = 10;
unsigned sends = 0;
struct ManagedNetwork : testing::Test {
    nx_lwip_adapter_t network{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> tx{};
    std::array<nx_eth_fragment_t, 1> views{};
    nx_eth_frames_port_t port{};
    static nx_time_us_t now(void*) {
        return uint64_t(ticks) * 1000;
    }
    static nx_result_t submit(void*, nx_eth_frames_tx_request_t* r) {
        ++sends;
        if (nx_request_admit(&r->base, NX_REQUEST_ACTIVE) != NX_SUCCESS)
            return NX_ERROR_STATE;
        nx_request_settle(&r->base, NX_SUCCESS, r->fragments[0].length);
        return NX_SUCCESS;
    }
    static nx_result_t acquire(void*, nx_eth_frames_rx_loan_t*,
                               nx_eth_fragment_t*, size_t) {
        return NX_ERROR_EMPTY;
    }
    static nx_result_t release(void*, const nx_eth_frames_rx_loan_t*) {
        return NX_SUCCESS;
    }
    static nx_result_t service(void*) {
        return NX_SUCCESS;
    }
    static nx_result_t stop(void*) {
        return NX_SUCCESS;
    }
    static nx_result_t apply(void*, const nx_eth_link_t*) {
        return NX_SUCCESS;
    }
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            lwip_init();
            initialized = true;
        }
        static const nx_eth_frames_ops_t ops{submit,  nullptr, service, acquire,
                                             release, nullptr, nullptr, apply,
                                             stop,    nullptr, nullptr};
        port = {&ops, this};
        nx_lwip_config_t config{};
        config.port = &port;
        config.clock = {now, nullptr};
        config.tx_buffer = tx.data();
        config.tx_capacity = tx.size();
        config.rx_views = views.data();
        config.rx_capacity = views.size();
        config.tx_timeout_us = 10000;
        const uint8_t mac[]{2, 0, 0, 0, 0, 1};
        std::memcpy(config.hardware_address, mac, 6);
        const uint8_t address[]{192, 168, 1, 2};
        const uint8_t mask[]{255, 255, 255, 0};
        std::memcpy(config.address, address, 4);
        std::memcpy(config.netmask, mask, 4);
        ASSERT_EQ(nx_lwip_adapter_start(&network, &config), NX_SUCCESS);
        netif_set_link_up(&network.interface);
        netif_set_up(&network.interface);
    }
    void TearDown() override {
        EXPECT_EQ(nx_lwip_adapter_stop(&network), NX_SUCCESS);
    }
    void udp_packet(uint16_t destination, const uint8_t* bytes, size_t size) {
        std::array<uint8_t, 100> packet{};
        const size_t n = 28 + size;
        packet[0] = 0x45;
        packet[2] = uint8_t(n >> 8);
        packet[3] = uint8_t(n);
        packet[8] = 64;
        packet[9] = 17;
        packet[12] = 192;
        packet[13] = 168;
        packet[14] = 1;
        packet[15] = 3;
        packet[16] = 192;
        packet[17] = 168;
        packet[18] = 1;
        packet[19] = 2;
        uint16_t checksum = inet_chksum(packet.data(), 20);
        std::memcpy(packet.data() + 10, &checksum, 2);
        packet[20] = 0x12;
        packet[21] = 0x34;
        packet[22] = uint8_t(destination >> 8);
        packet[23] = uint8_t(destination);
        packet[24] = uint8_t((8 + size) >> 8);
        packet[25] = uint8_t(8 + size);
        std::memcpy(packet.data() + 28, bytes, size);
        auto* p = pbuf_alloc(PBUF_RAW, uint16_t(n), PBUF_RAM);
        ASSERT_NE(p, nullptr);
        ASSERT_EQ(pbuf_take(p, packet.data(), uint16_t(n)), ERR_OK);
        EXPECT_EQ(ip4_input(p, &network.interface), ERR_OK);
    }
};
TEST_F(ManagedNetwork, UdpCopiesWholePacketsAndDropsWhileOneIsPending) {
    nx_lwip_udp_t socket{};
    uint8_t buffer[8]{};
    ASSERT_EQ(nx_lwip_udp_open(&socket, &network, 1234, buffer, sizeof(buffer)),
              NX_SUCCESS);
    const uint8_t first[]{1, 2, 3};
    udp_packet(1234, first, sizeof(first));
    udp_packet(1234, first, sizeof(first));
    EXPECT_EQ(socket.dropped, 1U);
    nx_ipv4_peer_t peer{};
    size_t size = 99;
    uint8_t output[8]{};
    EXPECT_EQ(nx_lwip_udp_receive(&socket, &peer, output, 2, &size),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(size, 99U);
    ASSERT_EQ(
        nx_lwip_udp_receive(&socket, &peer, output, sizeof(output), &size),
        NX_SUCCESS);
    EXPECT_EQ(size, 3U);
    EXPECT_EQ(std::memcmp(output, first, 3), 0);
    EXPECT_EQ(peer.port, 0x1234);
    EXPECT_EQ(peer.address[3], 3);
    EXPECT_EQ(nx_lwip_udp_close(&socket), NX_SUCCESS);
}
TEST_F(ManagedNetwork, UdpDeadlineAndLengthRejectBeforeStackSend) {
    nx_lwip_udp_t socket{};
    uint8_t buffer[8]{};
    ASSERT_EQ(nx_lwip_udp_open(&socket, &network, 1234, buffer, sizeof(buffer)),
              NX_SUCCESS);
    nx_ipv4_peer_t peer{{192, 168, 1, 3}, 7};
    const uint8_t byte = 1;
    unsigned before = sends;
    EXPECT_EQ(nx_lwip_udp_send(&socket, &peer, &byte, 1, now(nullptr)),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(sends, before);
    EXPECT_EQ(nx_lwip_udp_close(&socket), NX_SUCCESS);
}
TEST_F(ManagedNetwork, DhcpOwnsOneStaticStateAndRequiresActualLease) {
    nx_lwip_dhcp_t dhcp{};
    ASSERT_EQ(nx_lwip_dhcp_start(&dhcp, &network), NX_SUCCESS);
    nx_lwip_dhcp_t another{};
    EXPECT_EQ(nx_lwip_dhcp_start(&another, &network), NX_ERROR_BUSY);
    uint8_t address[4]{};
    EXPECT_EQ(nx_lwip_dhcp_address(&dhcp, address), NX_ERROR_BUSY);
    EXPECT_EQ(nx_lwip_dhcp_stop(&dhcp), NX_SUCCESS);
    EXPECT_EQ(netif_dhcp_data(&network.interface), nullptr);
}
TEST_F(ManagedNetwork,
       DnsLiteralSettlesImmediatelyWithoutUntrustedNameFallback) {
    nx_lwip_dns_t query{};
    ASSERT_EQ(nx_lwip_dns_start(&query, &network, "192.168.1.3",
                                now(nullptr) + 10000),
              NX_SUCCESS);
    uint8_t address[4]{};
    ASSERT_EQ(nx_lwip_dns_poll(&query, address), NX_SUCCESS);
    EXPECT_EQ(address[0], 192);
    EXPECT_EQ(address[3], 3);
}
TEST_F(ManagedNetwork, ListenerBindsExistingByteClientSlotsAndBoundsAdmission) {
    nx_lwip_tcp_listener_t listener{};
    nx_lwip_tcp_t clients[1]{};
    ASSERT_EQ(nx_lwip_tcp_listen(&listener, &network, clients, 1, 502, 1),
              NX_SUCCESS);
    auto* incoming = tcp_new_ip_type(IPADDR_TYPE_V4);
    ASSERT_NE(incoming, nullptr);
    incoming->state = ESTABLISHED;
    IP_ADDR4(&incoming->remote_ip, 192, 168, 1, 3);
    incoming->remote_port = 1234;
    ASSERT_EQ(
        listener.pcb->accept(listener.pcb->callback_arg, incoming, ERR_OK),
        ERR_OK);
    ASSERT_NE(nx_lwip_tcp_stream(&clients[0]), nullptr);
    EXPECT_EQ(clients[0].diagnostic.state, NX_LWIP_TCP_ESTABLISHED);
    EXPECT_EQ(listener.accepted, 1U);
    auto* excess = tcp_new_ip_type(IPADDR_TYPE_V4);
    ASSERT_NE(excess, nullptr);
    excess->state = ESTABLISHED;
    EXPECT_EQ(listener.pcb->accept(listener.pcb->callback_arg, excess, ERR_OK),
              ERR_ABRT);
    EXPECT_EQ(listener.rejected, 1U);
    EXPECT_EQ(nx_lwip_tcp_listener_stop(&listener), NX_SUCCESS);
    EXPECT_NE(nx_lwip_tcp_stream(&clients[0]), nullptr);
    EXPECT_EQ(nx_lwip_tcp_abort(&clients[0]), NX_SUCCESS);
}
TEST_F(ManagedNetwork, DnsLateCallbackCannotTurnExpiredQueryIntoSuccessOrIo) {
    ip_addr_t server;
    IP_ADDR4(&server, 192, 168, 1, 3);
    dns_setserver(0, &server);
    nx_lwip_dns_t query{};
    ASSERT_EQ(nx_lwip_dns_start(&query, &network, "deadline.example",
                                now(nullptr) + 1),
              NX_SUCCESS);
    ASSERT_TRUE(query.pending);
    for (unsigned i = 0; i < 64 && query.pending; ++i) {
        ticks += 1000;
        dns_tmr();
    }
    ASSERT_FALSE(query.pending);
    uint8_t address[4]{};
    EXPECT_EQ(nx_lwip_dns_poll(&query, address), NX_ERROR_TIMEOUT);
}
TEST_F(ManagedNetwork, DnsCancellationDoesNotRevokeItsUpstreamCallbackBorrow) {
    ip_addr_t server;
    IP_ADDR4(&server, 192, 168, 1, 3);
    dns_setserver(0, &server);
    nx_lwip_dns_t query{};
    ASSERT_EQ(nx_lwip_dns_start(&query, &network, "cancel.example",
                                now(nullptr) + 100000000),
              NX_SUCCESS);
    ASSERT_TRUE(query.pending);
    EXPECT_EQ(nx_lwip_dns_cancel(&query), NX_ERROR_BUSY);
    EXPECT_TRUE(query.pending);
    for (unsigned i = 0; i < 64 && query.pending; ++i) {
        ticks += 1000;
        dns_tmr();
    }
    ASSERT_FALSE(query.pending);
    EXPECT_EQ(nx_lwip_dns_cancel(&query), NX_ERROR_CANCELLED);
}
} /* namespace */
extern "C" uint32_t nx_lwip_port_now_ms(void) {
    return ticks;
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    return 0x12345678U;
}
extern "C" void nx_lwip_port_assert(const char*) {
    std::abort();
}
