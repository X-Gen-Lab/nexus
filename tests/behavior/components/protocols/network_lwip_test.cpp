/**
 * \file            network_lwip_test.cpp
 * \brief           Actual lwIP frame copying, ownership and stop contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-04
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_adapter.h"
#include "nexus/network/lwip_storage.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>
extern "C" {
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
}
namespace {
using TestPacket = std::unique_ptr<pbuf, decltype(&pbuf_free)>;
using TestUdp = std::unique_ptr<udp_pcb, decltype(&udp_remove)>;
using TestTcp = std::unique_ptr<tcp_pcb, decltype(&tcp_abort)>;
uint32_t now_ms = 10U;
struct Mac {
    nx_eth_frames_tx_request_t* pending{};
    std::vector<std::vector<uint8_t>> sent;
    std::vector<uint8_t> received;
    nx_eth_frames_state_t state{};
    nx_result_t release_result{NX_SUCCESS};
    nx_result_t stop_result{NX_SUCCESS};
    nx_result_t completion_result{NX_SUCCESS};
    bool complete_on_service{};
    bool held{};
    bool immediate{true};
    size_t releases{};
    size_t stops{};
    static nx_result_t Submit(void* context,
                              nx_eth_frames_tx_request_t* request) {
        auto& self = *static_cast<Mac*>(context);
        if (self.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        if (nx_request_admit(&request->base, NX_REQUEST_ACTIVE) != NX_SUCCESS) {
            return NX_ERROR_STATE;
        }
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < request->fragment_count; ++i) {
            const auto& part = request->fragments[i];
            bytes.insert(bytes.end(), part.data, part.data + part.length);
        }
        self.sent.push_back(bytes);
        if (self.immediate) {
            nx_request_settle(
                &request->base, self.completion_result,
                self.completion_result == NX_SUCCESS ? bytes.size() : 0U);
        } else {
            self.pending = request;
        }
        return NX_SUCCESS;
    }
    static nx_result_t Acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                               nx_eth_fragment_t* parts, size_t capacity) {
        auto& self = *static_cast<Mac*>(context);
        if (self.received.empty()) {
            return NX_ERROR_EMPTY;
        }
        if (capacity < 2U) {
            return NX_ERROR_OVERFLOW;
        }
        if (self.held) {
            return NX_ERROR_BUSY;
        }
        self.held = true;
        *loan = {};
        loan->identity = &self;
        loan->epoch = 1U;
        loan->length = self.received.size();
        loan->fragment_count = 2U;
        parts[0] = {self.received.data(), 13U};
        parts[1] = {self.received.data() + 13U, self.received.size() - 13U};
        return NX_SUCCESS;
    }
    static nx_result_t Release(void* context,
                               const nx_eth_frames_rx_loan_t* loan) {
        auto& self = *static_cast<Mac*>(context);
        ++self.releases;
        if (!self.held || loan->identity != &self) {
            return NX_ERROR_STATE;
        }
        if (self.release_result != NX_SUCCESS) {
            return self.release_result;
        }
        self.held = false;
        self.received.clear();
        return NX_SUCCESS;
    }
    static nx_result_t State(void* context, nx_eth_frames_state_t* state) {
        *state = static_cast<Mac*>(context)->state;
        return NX_SUCCESS;
    }
    static nx_result_t Apply(void* context, const nx_eth_link_t* link) {
        static_cast<Mac*>(context)->state.mac.link = *link;
        return NX_SUCCESS;
    }
    static nx_result_t Stop(void* context) {
        auto& self = *static_cast<Mac*>(context);
        ++self.stops;
        return self.stop_result;
    }
    static nx_result_t Service(void* context) {
        auto& self = *static_cast<Mac*>(context);
        if (self.complete_on_service && self.pending != nullptr) {
            auto* request = self.pending;
            self.pending = nullptr;
            nx_request_settle(&request->base, self.completion_result,
                              self.completion_result == NX_SUCCESS
                                  ? request->fragments[0].length
                                  : 0U);
        }
        return NX_SUCCESS;
    }
};
const nx_eth_frames_ops_t mac_ops{Mac::Submit,  nullptr,      Mac::Service,
                                  Mac::Acquire, Mac::Release, Mac::State,
                                  nullptr,      Mac::Apply,   Mac::Stop,
                                  nullptr,      nullptr};
class NetworkLwip : public testing::Test {
  protected:
    Mac mac;
    nx_eth_frames_port_t port{&mac_ops, &mac};
    nx_lwip_adapter_t adapter{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> tx{};
    std::array<nx_eth_fragment_t, 2> views{};
    nx_lwip_config_t config{};
    static nx_time_us_t Clock(void*) {
        return static_cast<nx_time_us_t>(now_ms) * 1000U;
    }
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            lwip_init();
            initialized = true;
        }
        config.port = &port;
        config.clock = {Clock, nullptr};
        config.tx_timeout_us = 100000U;
        config.tx_buffer = tx.data();
        config.tx_capacity = tx.size();
        config.rx_views = views.data();
        config.rx_capacity = views.size();
        const uint8_t address[]{10U, 0U, 0U, 2U};
        const uint8_t netmask[]{255U, 255U, 255U, 0U};
        const uint8_t hardware[]{2U, 0U, 0U, 0U, 0U, 2U};
        std::memcpy(config.address, address, 4U);
        std::memcpy(config.netmask, netmask, 4U);
        std::memcpy(config.hardware_address, hardware, 6U);
    }
    void TearDown() override {
        mac.release_result = NX_SUCCESS;
        mac.stop_result = NX_SUCCESS;
        if (mac.pending != nullptr) {
            auto* pending = mac.pending;
            mac.pending = nullptr;
            nx_request_settle(&pending->base, NX_ERROR_CANCELLED, 0U);
        }
        (void)nx_lwip_adapter_stop(&adapter);
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_start(&adapter, &config));
        const nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_apply_link(&adapter, &link));
        mac.sent.clear();
    }
    static std::vector<uint8_t> ArpRequest() {
        std::vector<uint8_t> frame(60U);
        const uint8_t header[]{255U, 255U, 255U, 255U, 255U, 255U, 2U,
                               0U,   0U,   0U,   0U,   1U,   8U,   6U};
        const uint8_t arp[]{0U, 1U, 8U, 0U, 6U,  4U, 0U, 1U, 2U, 0U,
                            0U, 0U, 0U, 1U, 10U, 0U, 0U, 1U, 0U, 0U,
                            0U, 0U, 0U, 0U, 10U, 0U, 0U, 2U};
        std::memcpy(frame.data(), header, sizeof(header));
        std::memcpy(frame.data() + sizeof(header), arp, sizeof(arp));
        return frame;
    }
    static std::vector<uint8_t> ArpRequestFor(uint8_t address) {
        auto request = ArpRequest();
        request[41] = address;
        return request;
    }
    static err_t SendPayload() {
        auto* pcb = udp_new();
        if (pcb == nullptr) {
            return ERR_MEM;
        }
        auto* packet = pbuf_alloc(PBUF_TRANSPORT, 3U, PBUF_RAM);
        if (packet == nullptr) {
            udp_remove(pcb);
            return ERR_MEM;
        }
        const uint8_t bytes[]{3U, 5U, 7U};
        ip_addr_t peer{};
        IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
        err_t result = pbuf_take(packet, bytes, sizeof(bytes));
        if (result == ERR_OK) {
            result = udp_sendto(pcb, packet, &peer, 1234U);
        }
        pbuf_free(packet);
        udp_remove(pcb);
        return result;
    }
    static void Checksum(std::vector<uint8_t>& frame, size_t start, size_t size,
                         size_t field) {
        uint32_t sum = 0U;
        for (size_t i = 0U; i < size; i += 2U) {
            sum += static_cast<uint32_t>(frame[start + i]) << 8U;
            if (i + 1U < size) {
                sum += frame[start + i + 1U];
            }
        }
        while ((sum >> 16U) != 0U) {
            sum = (sum & 0xffffU) + (sum >> 16U);
        }
        const auto value = static_cast<uint16_t>(~sum);
        frame[field] = static_cast<uint8_t>(value >> 8U);
        frame[field + 1U] = static_cast<uint8_t>(value);
    }
    static std::vector<uint8_t> IpFrame(uint8_t protocol,
                                        const std::vector<uint8_t>& data) {
        std::vector<uint8_t> frame(34U + data.size());
        const uint8_t ethernet[]{2U, 0U, 0U, 0U, 0U, 2U, 2U,
                                 0U, 0U, 0U, 0U, 1U, 8U, 0U};
        std::memcpy(frame.data(), ethernet, sizeof(ethernet));
        frame[14] = 0x45U;
        const auto length = static_cast<uint16_t>(20U + data.size());
        frame[16] = static_cast<uint8_t>(length >> 8U);
        frame[17] = static_cast<uint8_t>(length);
        frame[22] = 64U;
        frame[23] = protocol;
        frame[26] = 10U;
        frame[29] = 1U;
        frame[30] = 10U;
        frame[33] = 2U;
        std::memcpy(frame.data() + 34U, data.data(), data.size());
        Checksum(frame, 14U, 20U, 24U);
        return frame;
    }
};
TEST_F(NetworkLwip, InvalidCapacityRejectsWithoutInterfaceOrHardwareEffect) {
    config.tx_capacity = 13U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_lwip_adapter_start(&adapter, &config));
    EXPECT_EQ(nullptr, netif_default);
    EXPECT_EQ(0U, mac.stops);
    EXPECT_TRUE(mac.sent.empty());
}
TEST_F(NetworkLwip, ZeroOrMulticastHardwareIdentityRejectsWithoutEffects) {
    std::memset(config.hardware_address, 0, 6U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_lwip_adapter_start(&adapter, &config));
    config.hardware_address[0] = 1U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_lwip_adapter_start(&adapter, &config));
    EXPECT_EQ(nullptr, netif_default);
    EXPECT_EQ(0U, mac.stops);
}
TEST_F(NetworkLwip, MissingProviderContextRejectsBeforeNetifAttachment) {
    const nx_eth_frames_port_t invalid{&mac_ops, nullptr};
    config.port = &invalid;
    EXPECT_EQ(NX_ERROR_INVALID, nx_lwip_adapter_start(&adapter, &config));
    EXPECT_EQ(nullptr, netif_default);
    EXPECT_EQ(0U, mac.stops);
}
TEST_F(NetworkLwip, ActualArpParserRepliesAfterCompleteFragmentCopyAndRelease) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(1U, mac.releases);
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(8U, mac.sent[0][12]);
    EXPECT_EQ(6U, mac.sent[0][13]);
    EXPECT_EQ(2U, mac.sent[0][21]);
    EXPECT_EQ(1U, mac.sent[0][41]);
}
TEST_F(NetworkLwip, FailedReleaseRetainsLoanAndDoesNotPublishIntoStack) {
    Start();
    mac.received = ArpRequest();
    mac.release_result = NX_ERROR_IO;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(mac.held);
    EXPECT_TRUE(mac.sent.empty());
    mac.release_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    EXPECT_FALSE(mac.held);
}
TEST_F(NetworkLwip, AsyncTransmitCopiesAndRemainsBorrowedUntilRealSettlement) {
    Start();
    mac.immediate = false;
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_NE(nullptr, mac.pending);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&mac.pending->base));
    const auto before = tx;
    auto* frame = pbuf_alloc(PBUF_RAW, 60U, PBUF_RAM);
    ASSERT_NE(nullptr, frame);
    EXPECT_EQ(ERR_MEM, adapter.interface.linkoutput(&adapter.interface, frame));
    pbuf_free(frame);
    EXPECT_EQ(before, tx);
    mac.stop_result = NX_ERROR_BUSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&mac.pending->base));
    auto* pending = mac.pending;
    mac.pending = nullptr;
    nx_request_settle(&pending->base, NX_ERROR_CANCELLED, 0U);
    mac.stop_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(nullptr, netif_default);
}
TEST_F(NetworkLwip, ActualUdpSendUsesArpAndCopiesRawStackPayload) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.sent.clear();
    TestUdp pcb(udp_new(), udp_remove);
    ASSERT_NE(nullptr, pcb);
    ip_addr_t peer{};
    IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
    TestPacket packet(pbuf_alloc(PBUF_TRANSPORT, 3U, PBUF_RAM), pbuf_free);
    ASSERT_NE(nullptr, packet);
    const uint8_t payload[]{1U, 2U, 3U};
    ASSERT_EQ(ERR_OK, pbuf_take(packet.get(), payload, sizeof(payload)));
    EXPECT_EQ(ERR_OK, udp_sendto(pcb.get(), packet.get(), &peer, 1234U));
    packet.reset();
    pcb.reset();
    ASSERT_EQ(1U, mac.sent.size());
    const auto& bytes = mac.sent.front();
    ASSERT_GE(bytes.size(), 45U);
    EXPECT_EQ(17U, bytes[23]);
    EXPECT_EQ(1U, bytes[42]);
    EXPECT_EQ(2U, bytes[43]);
    EXPECT_EQ(3U, bytes[44]);
}
TEST_F(NetworkLwip, ActualIcmpEchoReplyUsesCheckedIpAndIcmpInput) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.sent.clear();
    const std::vector<uint8_t> echo{8U, 0U, 0U, 0U, 1U, 2U,
                                    0U, 1U, 4U, 5U, 6U, 7U};
    mac.received = IpFrame(1U, echo);
    Checksum(mac.received, 34U, echo.size(), 36U);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(1U, mac.sent[0][23]);
    EXPECT_EQ(0U, mac.sent[0][34]);
    EXPECT_EQ(4U, mac.sent[0][42]);
    EXPECT_EQ(7U, mac.sent[0][45]);
}
TEST_F(NetworkLwip, ActualUdpInputDeliversCopiedPayloadAfterRelease) {
    Start();
    std::vector<uint8_t> payload;
    TestUdp pcb(udp_new(), udp_remove);
    ASSERT_NE(nullptr, pcb);
    ASSERT_EQ(ERR_OK, udp_bind(pcb.get(), IP_ANY_TYPE, 1234U));
    udp_recv(
        pcb.get(),
        [](void* context, struct udp_pcb*, struct pbuf* packet,
           const ip_addr_t*, u16_t) {
            auto& bytes = *static_cast<std::vector<uint8_t>*>(context);
            bytes.resize(packet->tot_len);
            (void)pbuf_copy_partial(packet, bytes.data(), packet->tot_len, 0U);
            pbuf_free(packet);
        },
        &payload);
    const std::vector<uint8_t> datagram{0x11U, 0x11U, 4U, 0xd2U, 0U, 11U,
                                        0U,    0U,    7U, 8U,    9U};
    mac.received = IpFrame(17U, datagram);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    pcb.reset();
    EXPECT_EQ((std::vector<uint8_t>{7U, 8U, 9U}), payload);
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(NetworkLwip, ActualTcpConnectEmitsSynWithPinnedStack) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.sent.clear();
    TestTcp pcb(tcp_new(), tcp_abort);
    ASSERT_NE(nullptr, pcb);
    ip_addr_t peer{};
    IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
    ASSERT_EQ(ERR_OK, tcp_connect(pcb.get(), &peer, 1234U, nullptr));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(6U, mac.sent[0][23]);
    EXPECT_NE(0U, mac.sent[0][47] & 2U);
    pcb.reset();
}
TEST_F(NetworkLwip, PoolExhaustionReleasesFrameAndReportsBoundedOverload) {
    Start();
    std::vector<struct pbuf*> packets;
    for (;;) {
        auto* packet = pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL);
        if (packet == nullptr) {
            break;
        }
        packets.push_back(packet);
    }
    ASSERT_FALSE(packets.empty());
    mac.received = ArpRequest();
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
    EXPECT_TRUE(mac.sent.empty());
    for (auto* packet : packets) {
        pbuf_free(packet);
    }
    mac.received = ArpRequest();
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(2U, mac.releases);
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(2U, mac.sent[0][21]);
}
TEST_F(NetworkLwip, SecondAdapterRejectsWithoutTakingOverDefaultRoute) {
    Start();
    nx_lwip_adapter_t second{};
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_start(&second, &config));
    EXPECT_EQ(&adapter.interface, netif_default);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&second));
    EXPECT_EQ(0U, mac.stops);
}
TEST_F(NetworkLwip, CorruptIpChecksumDropsWithoutReplyAndReturnsHardwareLoan) {
    Start();
    const std::vector<uint8_t> echo{8U, 0U, 0U, 0U, 1U, 2U, 0U, 1U};
    mac.received = IpFrame(1U, echo);
    mac.received[24] ^= 0xffU;
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(mac.sent.empty());
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(NetworkLwip, ArenaPreservesNativePointerAlignmentForOddPayloads) {
    std::vector<TestPacket> packets;
    for (uint16_t size = 1U; size < 33U; ++size) {
        TestPacket packet(pbuf_alloc(PBUF_RAW, size, PBUF_RAM), pbuf_free);
        ASSERT_NE(nullptr, packet);
        EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(packet.get()) %
                          alignof(struct pbuf));
        packets.push_back(std::move(packet));
    }
}
TEST_F(NetworkLwip, ReceiveOverloadDoesNotStarveDueStackTimers) {
    Start();
    bool fired = false;
    const auto callback = [](void* context) {
        *static_cast<bool*>(context) = true;
    };
    sys_timeout(1U, callback, &fired);
    std::vector<struct pbuf*> packets;
    for (;;) {
        auto* packet = pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL);
        if (packet == nullptr) {
            break;
        }
        packets.push_back(packet);
    }
    now_ms += 2U;
    mac.received = ArpRequest();
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(fired);
    sys_untimeout(callback, &fired);
    for (auto* packet : packets) {
        pbuf_free(packet);
    }
}

TEST_F(NetworkLwip, StaticAddressChangesOnlyAfterCompleteAdapterWithdrawal) {
    Start();
    config.address[3] = 9U;
    EXPECT_EQ(2U, ip4_addr4(netif_ip4_addr(&adapter.interface)));
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_start(&adapter, &config));
    EXPECT_EQ(2U, ip4_addr4(netif_ip4_addr(&adapter.interface)));
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(ERR_OK, SendPayload());
    ASSERT_GE(mac.sent.back().size(), 45U);
    EXPECT_EQ(2U, mac.sent.back()[29]);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    ASSERT_EQ(nullptr, netif_default);

    /* All raw PCBs/callbacks/loans joined before fresh storage is initialized.
     */
    adapter = {};
    Start();
    EXPECT_EQ(9U, ip4_addr4(netif_ip4_addr(&adapter.interface)));
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(mac.sent.empty());
    mac.received = ArpRequestFor(9U);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(9U, mac.sent[0][31]);
    mac.sent.clear();
    ASSERT_EQ(ERR_OK, SendPayload());
    ASSERT_EQ(1U, mac.sent.size());
    ASSERT_GE(mac.sent[0].size(), 45U);
    EXPECT_EQ(17U, mac.sent[0][23]);
    EXPECT_EQ(9U, mac.sent[0][29]);
    EXPECT_EQ(7U, mac.sent[0][44]);
}

TEST_F(NetworkLwip, RepeatedLinkFactsCloseRoutesAndAllowExplicitFreshTraffic) {
    Start();
    const nx_eth_link_t down{false, false, 0U};
    const nx_eth_link_t up{true, true, 100U};
    constexpr unsigned cycles = 8U;
    for (unsigned cycle = 0U; cycle < cycles; ++cycle) {
        SCOPED_TRACE(cycle);
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_apply_link(&adapter, &down));
        EXPECT_FALSE(netif_is_link_up(&adapter.interface));
        EXPECT_FALSE(mac.state.mac.link.up);
        mac.sent.clear();
        EXPECT_EQ(ERR_RTE, SendPayload());
        EXPECT_TRUE(mac.sent.empty());
        EXPECT_EQ(nullptr, mac.pending);
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_apply_link(&adapter, &up));
        EXPECT_TRUE(netif_is_link_up(&adapter.interface));
        EXPECT_TRUE(mac.state.mac.link.up);
        mac.received = ArpRequest();
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
        mac.sent.clear();
        ASSERT_EQ(ERR_OK, SendPayload());
        ASSERT_EQ(1U, mac.sent.size());
        ASSERT_GE(mac.sent[0].size(), 45U);
        EXPECT_EQ(17U, mac.sent[0][23]);
        EXPECT_EQ(3U, mac.sent[0][42]);
        EXPECT_EQ(7U, mac.sent[0][44]);
    }
    RecordProperty("link_cycles", static_cast<int>(cycles));
}

TEST_F(NetworkLwip,
       CompoundCleanupRetainsRxAndTxUntilEachOwnerReallyWithdraws) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.immediate = false;
    ASSERT_EQ(ERR_OK, SendPayload());
    ASSERT_NE(nullptr, mac.pending);
    mac.release_result = NX_ERROR_IO;
    mac.received = ArpRequest();
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(mac.held);
    EXPECT_TRUE(adapter.receive_held);
    EXPECT_TRUE(adapter.closing);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&mac.pending->base));
    const auto stops = mac.stops;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(stops, mac.stops);
    EXPECT_EQ(&adapter.interface, netif_default);
    config.address[3] = 9U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_start(&adapter, &config));
    EXPECT_EQ(2U, ip4_addr4(netif_ip4_addr(&adapter.interface)));
    EXPECT_EQ(NX_ERROR_STATE, nx_lwip_adapter_service(&adapter, 1U));

    mac.release_result = NX_SUCCESS;
    mac.stop_result = NX_ERROR_BUSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_stop(&adapter));
    EXPECT_FALSE(mac.held);
    EXPECT_FALSE(adapter.receive_held);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&mac.pending->base));
    mac.stop_result = NX_ERROR_IO;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&adapter));
    EXPECT_TRUE(adapter.attached);
    mac.stop_result = NX_SUCCESS;
    /* Even an inconsistent provider success cannot revoke the visible loan. */
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_stop(&adapter));
    ASSERT_NE(nullptr, mac.pending);
    auto* pending = mac.pending;
    mac.pending = nullptr;
    nx_request_settle(&pending->base, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(nullptr, netif_default);
    EXPECT_FALSE(adapter.attached);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));

    adapter = {};
    mac.immediate = true;
    Start();
    mac.received = ArpRequestFor(9U);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(9U, mac.sent[0][31]);
}

/** \brief Caller timer remains live across netif detach; no lwIP reinit. */
struct PersistentTimer {
    unsigned fired = 0U;
    PersistentTimer() = default;
    PersistentTimer(const PersistentTimer&) = delete;
    PersistentTimer& operator=(const PersistentTimer&) = delete;
    static void Fire(void* context) {
        ++static_cast<PersistentTimer*>(context)->fired;
    }
    ~PersistentTimer() {
        sys_untimeout(Fire, this);
    }
};

TEST_F(NetworkLwip, RepeatedDetachAndRestartPreservesTheLiveGlobalTimerDomain) {
    PersistentTimer timer;
    constexpr unsigned cycles = 4U;
    for (unsigned cycle = 0U; cycle < cycles; ++cycle) {
        SCOPED_TRACE(cycle);
        config.address[3] = static_cast<uint8_t>(20U + cycle);
        Start();
        sys_timeout(1U, PersistentTimer::Fire, &timer);
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
        ASSERT_EQ(nullptr, netif_default);
        const uint32_t previous = now_ms;
        now_ms += 2U;
        adapter = {};
        Start();
        EXPECT_GT(now_ms, previous);
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
        EXPECT_EQ(cycle + 1U, timer.fired);
        EXPECT_EQ(20U + cycle, ip4_addr4(netif_ip4_addr(&adapter.interface)));
        EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    }
    RecordProperty("restart_cycles", static_cast<int>(cycles));
}
/** \brief Fixed complete raw frame; this test does not claim wire delivery. */
static TestPacket RawFrame(uint8_t marker) {
    TestPacket packet(pbuf_alloc(PBUF_RAW, 60U, PBUF_RAM), pbuf_free);
    if (packet) {
        std::array<uint8_t, 60> bytes{};
        bytes.fill(marker);
        bytes[12] = 0x88U;
        bytes[13] = 0xb5U;
        EXPECT_EQ(ERR_OK, pbuf_take(packet.get(), bytes.data(), bytes.size()));
    }
    return packet;
}

TEST_F(NetworkLwip, InlineSuccessReusesStorageWithoutServiceRoundTrip) {
    Start();
    for (uint8_t marker = 1U; marker <= 4U; ++marker) {
        auto packet = RawFrame(marker);
        ASSERT_NE(nullptr, packet);
        EXPECT_EQ(ERR_OK, adapter.interface.linkoutput(&adapter.interface,
                                                       packet.get()));
        EXPECT_EQ(marker, tx[0]);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&adapter.transmit.base));
        EXPECT_EQ(nullptr, mac.pending);
    }
    EXPECT_EQ(4U, mac.sent.size());
    EXPECT_EQ(0U, mac.stops);
}

TEST_F(NetworkLwip, InlineFailureSurvivesSecondOutputUntilReportedOnce) {
    Start();
    for (auto failure : {NX_ERROR_IO, NX_ERROR_EXHAUSTED}) {
        mac.sent.clear();
        mac.completion_result = failure;
        auto first = RawFrame(0x11U);
        auto second = RawFrame(0x22U);
        ASSERT_NE(nullptr, first);
        ASSERT_NE(nullptr, second);
        ASSERT_EQ(ERR_OK, adapter.interface.linkoutput(&adapter.interface,
                                                       first.get()));
        nx_result_t completed = NX_SUCCESS;
        size_t bytes = 1U;
        ASSERT_EQ(NX_SUCCESS, nx_request_result(&adapter.transmit.base,
                                                &completed, &bytes));
        ASSERT_EQ(failure, completed);
        EXPECT_EQ(0U, bytes);
        EXPECT_EQ(nullptr, mac.pending);
        const auto preserved = tx;
        mac.completion_result = NX_SUCCESS;
        EXPECT_EQ(ERR_IF, adapter.interface.linkoutput(&adapter.interface,
                                                       second.get()));
        EXPECT_EQ(preserved, tx);
        EXPECT_EQ(1U, mac.sent.size());
        EXPECT_EQ(failure, nx_lwip_adapter_service(&adapter, 1U));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&adapter.transmit.base));
        EXPECT_TRUE(adapter.attached);
        EXPECT_FALSE(adapter.closing);
        EXPECT_EQ(0U, mac.stops);
        EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
        EXPECT_EQ(ERR_OK, adapter.interface.linkoutput(&adapter.interface,
                                                       second.get()));
        EXPECT_EQ(2U, mac.sent.size());
        EXPECT_EQ(0x22U, tx[0]);
    }
}

TEST_F(NetworkLwip, DeferredFailureReturnsOnceWithoutRevokingActiveBorrow) {
    Start();
    mac.immediate = false;
    mac.completion_result = NX_ERROR_TIMEOUT;
    auto first = RawFrame(0x31U);
    auto second = RawFrame(0x32U);
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    ASSERT_EQ(ERR_OK,
              adapter.interface.linkoutput(&adapter.interface, first.get()));
    ASSERT_NE(nullptr, mac.pending);
    const auto preserved = tx;
    EXPECT_EQ(ERR_MEM,
              adapter.interface.linkoutput(&adapter.interface, second.get()));
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&adapter.transmit.base));
    EXPECT_EQ(preserved, tx);
    mac.complete_on_service = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(nullptr, mac.pending);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&adapter.transmit.base));
    EXPECT_TRUE(adapter.attached);
    EXPECT_FALSE(adapter.closing);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.immediate = true;
    mac.completion_result = NX_SUCCESS;
    EXPECT_EQ(ERR_OK,
              adapter.interface.linkoutput(&adapter.interface, second.get()));
    EXPECT_EQ(2U, mac.sent.size());
}

struct OutputTimer {
    netif* interface;
    pbuf* packet;
    err_t result{ERR_MEM};
    OutputTimer(netif* selected, pbuf* bytes)
        : interface(selected), packet(bytes) {
    }
    OutputTimer(const OutputTimer&) = delete;
    OutputTimer& operator=(const OutputTimer&) = delete;
    static void Fire(void* context) {
        auto& timer = *static_cast<OutputTimer*>(context);
        timer.result =
            timer.interface->linkoutput(timer.interface, timer.packet);
    }
    ~OutputTimer() {
        sys_untimeout(Fire, this);
    }
};

TEST_F(NetworkLwip, TimerInlineFailureIsReportedInTheSameServiceInvocation) {
    Start();
    auto packet = RawFrame(0x41U);
    ASSERT_NE(nullptr, packet);
    OutputTimer timer(&adapter.interface, packet.get());
    mac.completion_result = NX_ERROR_IO;
    sys_timeout(1U, OutputTimer::Fire, &timer);
    now_ms += 2U;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(ERR_OK, timer.result);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&adapter.transmit.base));
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.sent.size());
}

TEST_F(NetworkLwip, RxReplyInlineFailureIsReportedAfterExactLoanRelease) {
    Start();
    mac.completion_result = NX_ERROR_EXHAUSTED;
    mac.received = ArpRequest();
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
    EXPECT_EQ(1U, mac.sent.size());
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&adapter.transmit.base));
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.sent.size());
}

} /* namespace */
extern "C" uint32_t nx_lwip_port_now_ms(void) {
    return now_ms;
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    return UINT32_C(0x12345678);
}
extern "C" void nx_lwip_port_assert(const char*) {
    std::abort();
}
