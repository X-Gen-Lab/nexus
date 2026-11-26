/**
 * \file            network_lwip_loaned_test.cpp
 * \brief           Real lwIP immutable-payload RX loans and final withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_loaned_rx_storage.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>
extern "C" {
#include "lwip/inet_chksum.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
}

namespace {
uint32_t now_ms = 10U;
struct Frame {
    std::vector<uint8_t> bytes;
    const uint8_t* input{};
    nx_eth_frames_rx_loan_t token{};
    bool queued{};
    bool held{};
};
struct Mac {
    std::array<Frame, 4> frames{};
    std::vector<std::vector<uint8_t>> sent;
    nx_result_t release_result{NX_SUCCESS};
    nx_result_t stop_result{NX_SUCCESS};
    nx_result_t link_result{NX_SUCCESS};
    nx_result_t completion{NX_SUCCESS};
    nx_eth_frames_tx_request_t* pending{};
    bool deferred{};
    bool complete_on_release{};
    size_t releases{};
    size_t acquisitions{};
    size_t stops{};
    size_t middle_bytes{51U};
    uint64_t epoch{};
    void CompletePending() {
        if (pending != nullptr) {
            auto* completed = pending;
            pending = nullptr;
            nx_request_settle(&completed->base, completion,
                              completion == NX_SUCCESS ? sent.back().size()
                                                       : 0U);
        }
    }
    void Queue(const std::vector<uint8_t>& bytes) {
        for (auto& frame : frames) {
            if (!frame.queued && !frame.held) {
                frame.bytes = bytes;
                frame.input = nullptr;
                frame.queued = true;
                return;
            }
        }
        FAIL() << "Test provider queue exhausted";
    }
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
        for (size_t i = 0U; i < request->fragment_count; ++i) {
            const auto& piece = request->fragments[i];
            bytes.insert(bytes.end(), piece.data, piece.data + piece.length);
        }
        self.sent.push_back(bytes);
        if (self.deferred) {
            self.pending = request;
        } else {
            nx_request_settle(&request->base, self.completion,
                              self.completion == NX_SUCCESS ? bytes.size()
                                                            : 0U);
        }
        return NX_SUCCESS;
    }
    static nx_result_t Acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                               nx_eth_fragment_t* pieces, size_t capacity) {
        auto& self = *static_cast<Mac*>(context);
        for (size_t i = 0U; i < self.frames.size(); ++i) {
            auto& frame = self.frames[i];
            if (!frame.queued || frame.held) {
                continue;
            }
            const size_t count = frame.bytes.size() > 64U ? 3U : 2U;
            if (capacity < count) {
                return NX_ERROR_OVERFLOW;
            }
            if (count == 3U && self.middle_bytes >= frame.bytes.size() - 13U) {
                return NX_ERROR_INVALID;
            }
            std::memset(loan, 0, sizeof(*loan));
            loan->identity = &self;
            loan->epoch = ++self.epoch;
            loan->first_slot = i;
            loan->descriptor_count = count;
            loan->fragment_count = count;
            loan->length = frame.bytes.size();
            loan->acquired_us = static_cast<nx_time_us_t>(now_ms) * 1000U;
            loan->acquired_source = NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION;
            const auto* input =
                frame.input != nullptr ? frame.input : frame.bytes.data();
            pieces[0] = {input, 13U};
            pieces[1] = {input + 13U, count == 3U ? self.middle_bytes
                                                  : frame.bytes.size() - 13U};
            if (count == 3U) {
                pieces[2] = {input + 13U + self.middle_bytes,
                             frame.bytes.size() - 13U - self.middle_bytes};
            }
            frame.token = *loan;
            frame.held = true;
            frame.queued = false;
            ++self.acquisitions;
            return NX_SUCCESS;
        }
        return NX_ERROR_EMPTY;
    }
    static nx_result_t Release(void* context,
                               const nx_eth_frames_rx_loan_t* loan) {
        auto& self = *static_cast<Mac*>(context);
        ++self.releases;
        if (loan->first_slot >= self.frames.size()) {
            return NX_ERROR_STATE;
        }
        auto& frame = self.frames[loan->first_slot];
        if (!frame.held ||
            std::memcmp(loan, &frame.token, sizeof(*loan)) != 0) {
            return NX_ERROR_STATE;
        }
        if (self.complete_on_release) {
            self.CompletePending();
        }
        if (self.release_result != NX_SUCCESS) {
            return self.release_result;
        }
        frame.held = false;
        return NX_SUCCESS;
    }
    static nx_result_t Service(void* context) {
        auto& self = *static_cast<Mac*>(context);
        if (!self.complete_on_release) {
            self.CompletePending();
        }
        return NX_SUCCESS;
    }
    static nx_result_t Apply(void* context, const nx_eth_link_t*) {
        return static_cast<Mac*>(context)->link_result;
    }
    static nx_result_t Stop(void* context) {
        auto& self = *static_cast<Mac*>(context);
        ++self.stops;
        if (self.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        for (const auto& frame : self.frames) {
            if (frame.held) {
                return NX_ERROR_BUSY;
            }
        }
        return self.stop_result;
    }
};
const nx_eth_frames_ops_t mac_ops{
    Mac::Submit, nullptr,    Mac::Service, Mac::Acquire, Mac::Release, nullptr,
    nullptr,     Mac::Apply, Mac::Stop,    nullptr,      nullptr};

class LoanedRX : public testing::Test {
  protected:
    Mac mac;
    nx_eth_frames_port_t port{&mac_ops, &mac};
    nx_lwip_adapter_t adapter{};
    nx_lwip_loaned_config_t config{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> tx{};
    std::array<nx_eth_fragment_t, 3> views{};
    std::array<nx_lwip_rx_slot_t, 2> slots{};
    std::array<nx_lwip_rx_piece_t, 6> pieces{};
    std::vector<struct pbuf*> retained;
    std::vector<uint8_t> payload;
    std::vector<std::pair<void*, size_t>> mappings;
    struct udp_pcb* udp{};
    bool retain{true};
    bool saw_original_tail{};
    nx_result_t callback_reentry{NX_SUCCESS};
    bool reenter{};
    static nx_time_us_t Clock(void*) {
        return static_cast<nx_time_us_t>(now_ms) * 1000U;
    }
    static void Checksum(std::vector<uint8_t>& frame, size_t first,
                         size_t length, size_t field) {
        uint32_t sum = 0U;
        for (size_t i = 0U; i < length; i += 2U) {
            sum += static_cast<uint32_t>(frame[first + i]) << 8U;
            if (i + 1U < length) {
                sum += frame[first + i + 1U];
            }
        }
        while (sum >> 16U) {
            sum = (sum & 0xffffU) + (sum >> 16U);
        }
        const auto check = static_cast<uint16_t>(~sum);
        frame[field] = static_cast<uint8_t>(check >> 8U);
        frame[field + 1U] = static_cast<uint8_t>(check);
    }
    static std::vector<uint8_t> Datagram(size_t count = 256U) {
        std::vector<uint8_t> frame(42U + count);
        const uint8_t header[]{2U, 0U, 0U, 0U, 0U, 2U, 2U,
                               0U, 0U, 0U, 0U, 1U, 8U, 0U};
        std::memcpy(frame.data(), header, sizeof(header));
        frame[14] = 0x45U;
        const auto length = static_cast<uint16_t>(28U + count);
        frame[16] = static_cast<uint8_t>(length >> 8U);
        frame[17] = static_cast<uint8_t>(length);
        frame[22] = 64U;
        frame[23] = 17U;
        frame[26] = 10U;
        frame[29] = 1U;
        frame[30] = 10U;
        frame[33] = 2U;
        frame[34] = 0x11U;
        frame[35] = 0x11U;
        frame[36] = 4U;
        frame[37] = 0xd2U;
        frame[38] = static_cast<uint8_t>((8U + count) >> 8U);
        frame[39] = static_cast<uint8_t>(8U + count);
        for (size_t i = 0U; i < count; ++i) {
            frame[42U + i] = static_cast<uint8_t>(i);
        }
        Checksum(frame, 14U, 20U, 24U);
        return frame;
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
    void SeedArp() {
        mac.Queue(ArpRequest());
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
        ASSERT_FALSE(mac.frames[0].held);
        mac.sent.clear();
        mac.releases = 0U;
        mac.acquisitions = 0U;
    }
    void QueueReadonly(const std::vector<uint8_t>& bytes) {
        const long page_size = sysconf(_SC_PAGESIZE);
        ASSERT_GT(page_size, 0);
        const auto length = static_cast<size_t>(page_size);
        ASSERT_LE(bytes.size(), length);
        void* page = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        ASSERT_NE(MAP_FAILED, page);
        mappings.emplace_back(page, length);
        std::memcpy(page, bytes.data(), bytes.size());
        ASSERT_EQ(0, mprotect(page, length, PROT_READ));
        mac.Queue(bytes);
        for (auto& frame : mac.frames) {
            if (frame.queued && !frame.held) {
                frame.input = static_cast<const uint8_t*>(page);
                return;
            }
        }
        FAIL() << "Read-only test frame was not queued";
    }
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            lwip_init();
            initialized = true;
        }
        config.base.port = &port;
        config.base.clock = {Clock, nullptr};
        config.base.tx_timeout_us = 100000U;
        config.base.tx_buffer = tx.data();
        config.base.tx_capacity = tx.size();
        config.base.rx_views = views.data();
        config.base.rx_capacity = views.size();
        const uint8_t address[]{10U, 0U, 0U, 2U};
        const uint8_t mask[]{255U, 255U, 255U, 0U};
        const uint8_t hardware[]{2U, 0U, 0U, 0U, 0U, 2U};
        std::memcpy(config.base.address, address, 4U);
        std::memcpy(config.base.netmask, mask, 4U);
        std::memcpy(config.base.hardware_address, hardware, 6U);
        config.slots = slots.data();
        config.slot_count = slots.size();
        config.pieces = pieces.data();
        config.piece_count = pieces.size();
    }
    void TearDown() override {
        for (auto* packet : retained) {
            pbuf_free(packet);
        }
        retained.clear();
        if (udp != nullptr) {
            udp_remove(udp);
        }
        mac.release_result = NX_SUCCESS;
        mac.stop_result = NX_SUCCESS;
        if (mac.pending != nullptr) {
            auto* pending = mac.pending;
            mac.pending = nullptr;
            nx_request_settle(&pending->base, NX_ERROR_CANCELLED, 0U);
        }
        const auto stopped = nx_lwip_adapter_stop(&adapter);
        EXPECT_EQ(NX_SUCCESS, stopped);
        EXPECT_EQ(nullptr, netif_default);
        if (stopped == NX_SUCCESS) {
            for (const auto& mapping : mappings) {
                EXPECT_EQ(0, munmap(mapping.first, mapping.second));
            }
        }
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_lwip_loaned_adapter_start(&adapter, &config));
        const nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_apply_link(&adapter, &link));
        mac.sent.clear();
    }
    void Listen() {
        udp = udp_new();
        ASSERT_NE(nullptr, udp);
        ASSERT_EQ(ERR_OK, udp_bind(udp, IP_ANY_TYPE, 1234U));
        udp_recv(udp, Receive, this);
    }
    static void Receive(void* context, struct udp_pcb*, struct pbuf* packet,
                        const ip_addr_t*, u16_t) {
        auto& self = *static_cast<LoanedRX*>(context);
        self.payload.resize(packet->tot_len);
        (void)pbuf_copy_partial(packet, self.payload.data(), packet->tot_len,
                                0U);
        for (auto* piece = packet; piece != nullptr; piece = piece->next) {
            for (const auto& frame : self.mac.frames) {
                const auto pointer =
                    reinterpret_cast<uintptr_t>(piece->payload);
                const auto begin =
                    reinterpret_cast<uintptr_t>(frame.bytes.data());
                if (!frame.bytes.empty() && pointer >= begin &&
                    pointer < begin + frame.bytes.size() && piece->len != 0U) {
                    self.saw_original_tail = true;
                    EXPECT_EQ(PBUF_ROM, piece->type_internal);
                }
            }
        }
        if (self.reenter) {
            self.callback_reentry = nx_lwip_adapter_service(&self.adapter, 1U);
        }
        if (self.retain) {
            self.retained.push_back(packet);
        } else {
            pbuf_free(packet);
        }
    }
    void ReleaseFirst() {
        ASSERT_FALSE(retained.empty());
        pbuf_free(retained.front());
        retained.erase(retained.begin());
    }
    err_t SendArp(uint8_t tail) {
        auto frame = ArpRequest();
        frame.back() = tail;
        auto* packet =
            pbuf_alloc(PBUF_RAW, static_cast<u16_t>(frame.size()), PBUF_RAM);
        if (packet == nullptr) {
            ADD_FAILURE() << "Test packet allocation failed";
            return ERR_MEM;
        }
        if (pbuf_take(packet, frame.data(), static_cast<u16_t>(frame.size())) !=
            ERR_OK) {
            ADD_FAILURE() << "Test packet initialization failed";
            pbuf_free(packet);
            return ERR_BUF;
        }
        const err_t result =
            adapter.interface.linkoutput(&adapter.interface, packet);
        pbuf_free(packet);
        return result;
    }
};

TEST_F(LoanedRX, SelectedStartAttachesWithoutBorrowingProviderStorage) {
    Start();
    EXPECT_EQ(&adapter.interface, netif_default);
    EXPECT_EQ(0U, mac.acquisitions);
    EXPECT_EQ(0U, mac.releases);
}
TEST_F(LoanedRX, LiveReinitializationCannotEraseOrReclassifyRetainedRxLoan) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    const auto token = mac.frames[0].token;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_loaned_adapter_start(&adapter, &config));
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(0U, mac.releases);
    EXPECT_EQ(0, std::memcmp(&token, &mac.frames[0].token, sizeof(token)));
}
TEST_F(LoanedRX, InvalidMetadataCapacityRejectsWithoutNetifOrProviderEffects) {
    config.piece_count = 5U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_lwip_loaned_adapter_start(&adapter, &config));
    EXPECT_EQ(nullptr, netif_default);
    EXPECT_EQ(0U, mac.acquisitions);
    EXPECT_EQ(0U, mac.stops);
}
TEST_F(LoanedRX, MetadataOverlapAndZeroSlotsRejectBeforeAttachment) {
    config.slot_count = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_lwip_loaned_adapter_start(&adapter, &config));
    config.slot_count = 2U;
    config.pieces = reinterpret_cast<nx_lwip_rx_piece_t*>(slots.data());
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_lwip_loaned_adapter_start(&adapter, &config));
    EXPECT_EQ(nullptr, netif_default);
}
TEST_F(LoanedRX, MacPayloadCannotAliasLiveCustomSlotOrPieceMetadata) {
    Start();
    for (const auto* input :
         {slots[0].prefix.bytes, reinterpret_cast<uint8_t*>(pieces.data())}) {
        mac.Queue(Datagram(128U));
        mac.frames[0].input = input;
        EXPECT_EQ(NX_ERROR_INVALID, nx_lwip_adapter_service(&adapter, 1U));
        EXPECT_FALSE(mac.frames[0].held);
        EXPECT_TRUE(retained.empty());
    }
    EXPECT_EQ(2U, mac.releases);
}
TEST_F(LoanedRX, ActualUdpPayloadTailReferencesUnchangedMultiFragmentMacBytes) {
    Start();
    Listen();
    const auto original = Datagram();
    mac.Queue(original);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, retained.size());
    EXPECT_TRUE(saw_original_tail);
    EXPECT_EQ(256U, payload.size());
    EXPECT_EQ(original, mac.frames[0].bytes);
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(0U, mac.releases);
}
TEST_F(LoanedRX, FinalPbufReferenceOnlyMarksReleaseReadyUntilOwnerProgress) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, retained.size());
    auto* packet = retained.front();
    pbuf_ref(packet);
    pbuf_free(packet);
    EXPECT_EQ(0U, mac.releases);
    ReleaseFirst();
    EXPECT_EQ(0U, mac.releases);
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.releases);
    EXPECT_FALSE(mac.frames[0].held);
}
TEST_F(LoanedRX,
       RetainedTailReferenceOutlivesItsCopiedHeaderWithoutEarlyRearm) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, retained.size());
    auto* tail = retained.front()->next;
    ASSERT_NE(nullptr, tail);
    pbuf_ref(tail);
    ReleaseFirst();
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(0U, mac.releases);
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(94U, *static_cast<const uint8_t*>(tail->payload));
    pbuf_free(tail);
    EXPECT_EQ(0U, mac.releases);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.releases);
    EXPECT_FALSE(mac.frames[0].held);
}
TEST_F(LoanedRX, SeparateOddPayloadFragmentsShareOneFinalHardwareRelease) {
    Start();
    Listen();
    mac.middle_bytes = 200U;
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    ASSERT_EQ(1U, retained.size());
    ASSERT_EQ(256U, payload.size());
    for (size_t i = 0U; i < payload.size(); ++i) {
        EXPECT_EQ(static_cast<uint8_t>(i), payload[i]);
    }
    auto* middle = retained.front()->next;
    ASSERT_NE(nullptr, middle);
    EXPECT_EQ(77U, middle->len);
    auto* last = middle->next;
    ASSERT_NE(nullptr, last);
    EXPECT_EQ(85U, last->len);
    pbuf_ref(last);
    ReleaseFirst();
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(0U, mac.releases);
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(171U, *static_cast<const uint8_t*>(last->payload));
    pbuf_free(last);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(LoanedRX, StopCannotWithdrawProviderWhilePbufReferencesRemain) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(0U, mac.stops);
    EXPECT_EQ(0U, mac.releases);
    EXPECT_EQ(&adapter.interface, netif_default);
    ReleaseFirst();
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(1U, mac.releases);
    EXPECT_EQ(1U, mac.stops);
}
TEST_F(LoanedRX, FailedExactReleaseRetainsTokenAndClosesFurtherAdmission) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    const auto token = mac.frames[0].token;
    ReleaseFirst();
    mac.release_result = NX_ERROR_IO;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(0, std::memcmp(&token, &mac.frames[0].token, sizeof(token)));
    mac.Queue(Datagram());
    EXPECT_EQ(NX_ERROR_STATE, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(1U, mac.acquisitions);
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(0U, mac.stops);
    mac.release_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
}
TEST_F(LoanedRX, TwoHeldLoansDoNotReuseSlotsAndCapacityPreservesQueuedFrame) {
    Start();
    Listen();
    mac.Queue(Datagram());
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 2U));
    ASSERT_EQ(2U, retained.size());
    EXPECT_EQ(2U, mac.acquisitions);
    mac.Queue(Datagram());
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(2U, mac.acquisitions);
    ReleaseFirst();
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(3U, mac.acquisitions);
    EXPECT_EQ(1U, mac.releases);
    EXPECT_EQ(2U, retained.size());
}
TEST_F(LoanedRX, DueTimersProgressBeforeReportedRxSlotExhaustion) {
    Start();
    Listen();
    mac.Queue(Datagram());
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 2U));
    bool fired = false;
    const auto callback = [](void* context) {
        *static_cast<bool*>(context) = true;
    };
    sys_timeout(1U, callback, &fired);
    now_ms += 2U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_TRUE(fired);
    sys_untimeout(callback, &fired);
}
TEST_F(LoanedRX, RxDoesNotConsumeTheFixedLwipPayloadPool) {
    Start();
    Listen();
    std::vector<struct pbuf*> full;
    for (;;) {
        auto* packet = pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL);
        if (packet == nullptr) {
            break;
        }
        full.push_back(packet);
    }
    ASSERT_FALSE(full.empty());
    EXPECT_EQ(2U, full.size());
    mac.Queue(Datagram());
    const auto result = nx_lwip_adapter_service(&adapter, 1U);
    pbuf_free(full.back());
    full.pop_back();
    auto* reused = pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL);
    EXPECT_NE(nullptr, reused);
    if (reused != nullptr) {
        pbuf_free(reused);
    }
    for (auto* packet : full) {
        pbuf_free(packet);
    }
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_EQ(1U, retained.size());
    EXPECT_TRUE(saw_original_tail);
}
TEST_F(LoanedRX, LinkFailureAndFailedProviderDrainRetainAdapterStorage) {
    Start();
    mac.link_result = NX_ERROR_BUSY;
    const nx_eth_link_t down{false, true, 100U};
    EXPECT_EQ(NX_ERROR_BUSY, nx_lwip_adapter_apply_link(&adapter, &down));
    EXPECT_TRUE(netif_is_link_up(&adapter.interface));
    mac.stop_result = NX_ERROR_IO;
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&adapter));
    EXPECT_EQ(&adapter.interface, netif_default);
    mac.stop_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
}
TEST_F(LoanedRX, RawCallbackCannotReenterAdapterProgressOrAcquireAnotherLoan) {
    Start();
    Listen();
    reenter = true;
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(NX_ERROR_BUSY, callback_reentry);
    EXPECT_EQ(1U, mac.acquisitions);
}
TEST_F(LoanedRX, MaxIpv4AndTcpOptionsAreMutableOnlyInsideCopiedHeaderPrefix) {
    Start();
    SeedArp();
    mac.middle_bytes = 200U;
    auto frame = Datagram(400U);
    frame.resize(14U + 60U + 60U + 256U, 0x23U);
    frame[14] = 0x4fU;
    frame[16] = static_cast<uint8_t>((frame.size() - 14U) >> 8U);
    frame[17] = static_cast<uint8_t>(frame.size() - 14U);
    frame[23] = 6U;
    frame[24] = frame[25] = 0U;
    std::memset(frame.data() + 34U, 0, 40U);
    constexpr size_t tcp = 74U;
    std::memset(frame.data() + tcp, 0, 60U);
    frame[tcp] = 0x12U;
    frame[tcp + 1U] = 0x34U;
    frame[tcp + 2U] = 4U;
    frame[tcp + 3U] = 0xd2U;
    frame[tcp + 4U] = 0x12U;
    frame[tcp + 5U] = 0x34U;
    frame[tcp + 6U] = 0x56U;
    frame[tcp + 7U] = 0x78U;
    frame[tcp + 12U] = 0xf0U;
    frame[tcp + 13U] = 2U;
    frame[tcp + 14U] = 0x40U;
    std::memset(frame.data() + tcp + 20U, 1, 40U);
    ip4_addr_t from{}, to{};
    IP4_ADDR(&from, 10U, 0U, 0U, 1U);
    IP4_ADDR(&to, 10U, 0U, 0U, 2U);
    const auto tcp_length = static_cast<u16_t>(frame.size() - tcp);
    auto* packet = pbuf_alloc(PBUF_RAW, tcp_length, PBUF_RAM);
    ASSERT_NE(nullptr, packet);
    ASSERT_EQ(ERR_OK, pbuf_take(packet, frame.data() + tcp, tcp_length));
    const u16_t checksum =
        inet_chksum_pseudo(packet, 6U, tcp_length, &from, &to);
    pbuf_free(packet);
    std::memcpy(frame.data() + tcp + 16U, &checksum, sizeof(checksum));
    Checksum(frame, 14U, 60U, 24U);
    QueueReadonly(frame);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(frame, mac.frames[0].bytes);
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_NE(0U, mac.sent[0][47] & 4U);
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(LoanedRX, ActualIcmpEchoCopiesForReplyWithoutMutatingMacLoanBytes) {
    Start();
    SeedArp();
    std::array<struct pbuf*, 2> pool{};
    for (auto& packet : pool) {
        packet = pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL);
        ASSERT_NE(nullptr, packet);
    }
    EXPECT_EQ(nullptr, pbuf_alloc(PBUF_RAW, 64U, PBUF_POOL));
    auto frame = Datagram();
    frame[23] = 1U;
    frame[24] = frame[25] = 0U;
    frame[34] = 8U;
    frame[35] = frame[36] = frame[37] = 0U;
    Checksum(frame, 34U, frame.size() - 34U, 36U);
    Checksum(frame, 14U, 20U, 24U);
    QueueReadonly(frame);
    const auto result = nx_lwip_adapter_service(&adapter, 1U);
    for (auto* packet : pool) {
        pbuf_free(packet);
    }
    ASSERT_EQ(NX_SUCCESS, result);
    EXPECT_EQ(frame, mac.frames[0].bytes);
    ASSERT_EQ(1U, mac.sent.size());
    EXPECT_EQ(0U, mac.sent[0][34]);
    EXPECT_EQ(1U, mac.releases);
}
TEST_F(LoanedRX, RealArpHeaderAcrossFragmentsReturnsLoanAfterImmediateFree) {
    Start();
    const auto frame = ArpRequest();
    mac.Queue(frame);
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(frame, mac.frames[0].bytes);
    EXPECT_EQ(1U, mac.sent.size());
    EXPECT_EQ(1U, mac.releases);
    EXPECT_FALSE(mac.frames[0].held);
}
TEST_F(LoanedRX, StaleEpochCannotReleaseReusedMacFrameOrCurrentCustomPbuf) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    const auto old = mac.frames[0].token;
    ReleaseFirst();
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_NE(old.epoch, mac.frames[0].token.epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_frames_port_release(&port, &old));
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(1U, retained.size());
}
TEST_F(LoanedRX, InlineTxErrorCannotBeOverwrittenBeforeServiceObservesIt) {
    Start();
    mac.completion = NX_ERROR_IO;
    /* Admission still succeeds; service reports the terminal wire outcome. */
    EXPECT_EQ(ERR_OK, SendArp(1U));
    const auto before = tx;
    EXPECT_EQ(ERR_IF, SendArp(2U));
    EXPECT_EQ(before, tx);
    EXPECT_EQ(1U, mac.sent.size());
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_FALSE(adapter.closing);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.completion = NX_SUCCESS;
    EXPECT_EQ(ERR_OK, SendArp(3U));
    EXPECT_EQ(2U, mac.sent.size());
}
TEST_F(LoanedRX, DeferredTxFailureIsReportedExactlyOnceWithoutClosingRx) {
    Start();
    mac.deferred = true;
    mac.completion = NX_ERROR_TIMEOUT;
    EXPECT_EQ(ERR_OK, SendArp(1U));
    EXPECT_NE(nullptr, mac.pending);
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(nullptr, mac.pending);
    EXPECT_FALSE(adapter.closing);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.deferred = false;
    mac.completion = NX_SUCCESS;
    EXPECT_EQ(ERR_OK, SendArp(2U));
}
TEST_F(LoanedRX, InlineTxSuccessKeepsTheImmediateReusePathAvailable) {
    Start();
    EXPECT_EQ(ERR_OK, SendArp(1U));
    EXPECT_EQ(ERR_OK, SendArp(2U));
    EXPECT_EQ(2U, mac.sent.size());
    EXPECT_EQ(2U, tx[59]);
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_FALSE(adapter.closing);
}
TEST_F(LoanedRX,
       RxReleaseFailureCannotHideATxCompletionPublishedInThatPortCall) {
    Start();
    Listen();
    mac.Queue(Datagram());
    ASSERT_EQ(NX_SUCCESS, nx_lwip_adapter_service(&adapter, 1U));
    mac.deferred = true;
    mac.complete_on_release = true;
    mac.completion = NX_ERROR_TIMEOUT;
    mac.release_result = NX_ERROR_IO;
    ASSERT_EQ(ERR_OK, SendArp(1U));
    ReleaseFirst();
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(nullptr, mac.pending);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&adapter.transmit.base));
    EXPECT_TRUE(adapter.closing);
    EXPECT_TRUE(mac.frames[0].held);
    EXPECT_EQ(0U, mac.stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_lwip_adapter_service(&adapter, 1U));
    EXPECT_EQ(NX_ERROR_IO, nx_lwip_adapter_stop(&adapter));
    mac.release_result = NX_SUCCESS;
    EXPECT_EQ(NX_SUCCESS, nx_lwip_adapter_stop(&adapter));
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
