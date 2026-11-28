/**
 * \file            f107_lwip_ipv4_test.cpp
 * \brief           Pinned lwIP ARP and exact raw-MII loans in the F107
 * reference. \author          Nexus Team \version         1.0.0 \date
 * 2026-11-28 \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_storage.h"
#include "raw_frames.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
#include "lwip/init.h"
#include "lwip/pbuf.h"
}

namespace {
constexpr std::array<uint8_t, 42> arp_request{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 192,  0,    2,    1,    0,
    0,    0,    0,    0,    0,    192,  0,    2,    2,
};

struct Mac {
    nx_eth_tx_request_t* pending{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> last{};
    size_t sent{};
    size_t releases{};
    bool offered{};
    bool held{};
    bool deferred{};
    nx_result_t stop_result{NX_SUCCESS};
    nx_result_t release_result{NX_SUCCESS};

    static nx_result_t Submit(void* context, nx_eth_tx_request_t* request) {
        auto& self = *static_cast<Mac*>(context);
        if (self.pending != nullptr)
            return NX_ERROR_BUSY;
        nx_result_t result =
            nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            std::memcpy(self.last.data(), request->data, request->length);
            ++self.sent;
            if (self.deferred)
                self.pending = request;
            else
                nx_request_settle(&request->base, NX_SUCCESS, request->length);
        }
        return result;
    }

    static nx_result_t Acquire(void* context, nx_eth_rx_frame_t* frame) {
        auto& self = *static_cast<Mac*>(context);
        if (!self.offered)
            return NX_ERROR_EMPTY;
        self.offered = false;
        self.held = true;
        *frame = {arp_request.data(), arp_request.size(), 7U, &self, 19U, 100U};
        return NX_SUCCESS;
    }

    static nx_result_t Release(void* context, const nx_eth_rx_frame_t* frame) {
        auto& self = *static_cast<Mac*>(context);
        EXPECT_TRUE(self.held);
        EXPECT_EQ(self.last.size(), NX_ETH_FRAME_MAX);
        EXPECT_EQ(&self, frame->identity);
        EXPECT_EQ(7U, frame->slot);
        EXPECT_EQ(19U, frame->epoch);
        EXPECT_EQ(100U, frame->timestamp_us);
        EXPECT_EQ(arp_request.data(), frame->data);
        EXPECT_EQ(arp_request.size(), frame->length);
        if (self.release_result == NX_SUCCESS) {
            ++self.releases;
            self.held = false;
        }
        return self.release_result;
    }

    static nx_result_t Stop(void* context) {
        auto& self = *static_cast<Mac*>(context);
        if (self.held || self.stop_result != NX_SUCCESS)
            return NX_ERROR_BUSY;
        nx_eth_tx_request_t* retired = self.pending;
        self.pending = nullptr;
        if (retired != nullptr)
            nx_request_settle(&retired->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }

    static constexpr nx_eth_ops_t ops{
        Submit,
        nullptr,
        [](void*) { return NX_SUCCESS; },
        Acquire,
        Release,
        nullptr,
        [](void*, const nx_eth_link_t*) { return NX_SUCCESS; },
        Stop,
        nullptr,
    };
};

class F107LwipIPv4 : public testing::Test {
  protected:
    Mac mac{};
    const nx_eth_port_t raw{&Mac::ops, &mac};
    f107_raw_frames_t bridge{};
    nx_lwip_adapter_t network{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> tx{};
    nx_eth_fragment_t view{};

    static void SetUpTestSuite() {
        lwip_init();
    }

    void SetUp() override {
        f107_raw_frames_init(&bridge, &raw);
        const nx_lwip_config_t config{
            &bridge.face,
            {[](void*) -> nx_time_us_t { return 10000U; }, nullptr},
            5000U,
            tx.data(),
            tx.size(),
            &view,
            1U,
            {0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
            {192, 0, 2, 2},
            {255, 255, 255, 0},
            {0, 0, 0, 0},
        };
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_start(&network, &config));
        const nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_apply_link(&network, &link));
    }

    void TearDown() override {
        mac.stop_result = mac.release_result = NX_SUCCESS;
        EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&network));
    }
};

TEST_F(F107LwipIPv4, PinnedStackAnswersArpAndReturnsOriginalRawToken) {
    mac.offered = true;
    const size_t sent = mac.sent;
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&network, 1U));
    EXPECT_EQ(1U, mac.releases);
    EXPECT_FALSE(mac.held);
    ASSERT_EQ(sent + 1U, mac.sent);
    EXPECT_EQ(0x08U, mac.last[12]);
    EXPECT_EQ(0x06U, mac.last[13]);
    EXPECT_EQ(0x00U, mac.last[20]);
    EXPECT_EQ(0x02U, mac.last[21]);
}

TEST_F(F107LwipIPv4, FailedRawStopKeepsTxBorrowUntilResetCompletion) {
    mac.deferred = true;
    mac.offered = true;
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&network, 1U));
    ASSERT_NE(nullptr, mac.pending);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&network.transmit.base));
    mac.stop_result = NX_ERROR_BUSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_stop(&network));
    EXPECT_TRUE(network.attached);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&network.transmit.base));
    mac.stop_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&network));
    EXPECT_FALSE(network.attached);
    nx_result_t terminal{};
    size_t bytes{};
    ASSERT_EQ(NX_SUCCESS,
              nx_request_result(&network.transmit.base, &terminal, &bytes));
    EXPECT_EQ(NX_ERROR_CANCELLED, terminal);
    EXPECT_EQ(0U, bytes);
}

TEST_F(F107LwipIPv4, FailedRxReleaseRetriesExactTokenWithoutFeedingStack) {
    mac.release_result = NX_ERROR_IO;
    mac.offered = true;
    const size_t sent = mac.sent;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&network, 1U));
    EXPECT_TRUE(mac.held);
    EXPECT_EQ(sent, mac.sent);
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&network));
    EXPECT_TRUE(network.attached);
    mac.release_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&network));
    EXPECT_FALSE(mac.held);
    EXPECT_EQ(1U, mac.releases);
    EXPECT_EQ(sent, mac.sent);
}
} /* namespace */

extern "C" {
uint32_t nx_lwip_port_now_ms(void) {
    return 10U;
}
void nx_lwip_port_assert(const char* message) {
    ADD_FAILURE() << message;
    std::abort();
}
}
