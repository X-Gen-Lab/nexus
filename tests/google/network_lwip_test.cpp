/**
 * \file            network_lwip_test.cpp
 * \brief           Actual lwIP frame copying, ownership and stop contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_adapter.h"
#include "nexus/network/lwip_storage.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>
extern "C" {
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
}
namespace {
uint32_t now_ms = 10U;
struct Mac {
    nx_eth_frames_tx_request_t* pending{};
    std::vector<std::vector<uint8_t>> sent;
    std::vector<uint8_t> received;
    nx_eth_frames_state_t state{};
    nx_result_t release_result{NX_SUCCESS};
    nx_result_t stop_result{NX_SUCCESS};
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
            nx_request_settle(&request->base, NX_SUCCESS, bytes.size());
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
    static nx_result_t Service(void*) {
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
            nx_request_settle(&mac.pending->base, NX_ERROR_CANCELLED, 0U);
            mac.pending = nullptr;
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
    nx_request_settle(&mac.pending->base, NX_ERROR_CANCELLED, 0U);
    mac.pending = nullptr;
    mac.stop_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(nullptr, netif_default);
}
TEST_F(NetworkLwip, ActualUdpSendUsesArpAndCopiesRawStackPayload) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.sent.clear();
    auto* pcb = udp_new();
    ASSERT_NE(nullptr, pcb);
    ip_addr_t peer{};
    IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
    auto* packet = pbuf_alloc(PBUF_TRANSPORT, 3U, PBUF_RAM);
    ASSERT_NE(nullptr, packet);
    const uint8_t payload[]{1U, 2U, 3U};
    ASSERT_EQ(ERR_OK, pbuf_take(packet, payload, sizeof(payload)));
    EXPECT_EQ(ERR_OK, udp_sendto(pcb, packet, &peer, 1234U));
    pbuf_free(packet);
    udp_remove(pcb);
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
    auto* pcb = udp_new();
    ASSERT_NE(nullptr, pcb);
    ASSERT_EQ(ERR_OK, udp_bind(pcb, IP_ANY_TYPE, 1234U));
    std::vector<uint8_t> payload;
    udp_recv(
        pcb,
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
    udp_remove(pcb);
    EXPECT_EQ((std::vector<uint8_t>{7U, 8U, 9U}), payload);
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(NetworkLwip, ActualTcpConnectEmitsSynWithPinnedStack) {
    Start();
    mac.received = ArpRequest();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.sent.clear();
    auto* pcb = tcp_new();
    ASSERT_NE(nullptr, pcb);
    ip_addr_t peer{};
    IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
    ASSERT_EQ(ERR_OK, tcp_connect(pcb, &peer, 1234U, nullptr));
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(6U, mac.sent[0][23]);
    EXPECT_NE(0U, mac.sent[0][47] & 2U);
    tcp_abort(pcb);
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
    std::vector<struct pbuf*> packets;
    for (uint16_t size = 1U; size < 33U; ++size) {
        auto* packet = pbuf_alloc(PBUF_RAW, size, PBUF_RAM);
        ASSERT_NE(nullptr, packet);
        packets.push_back(packet);
        EXPECT_EQ(0U,
                  reinterpret_cast<uintptr_t>(packet) % alignof(struct pbuf));
    }
    for (auto* packet : packets) {
        pbuf_free(packet);
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
