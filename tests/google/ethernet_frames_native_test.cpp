/**
 * \file            ethernet_frames_native_test.cpp
 * \brief           Native C hardware-model and provider integration tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Explicit software events do not qualify physical hardware.
 */
#include "ethernet_frames.h"
#include "nexus/io/native/model.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
namespace {
struct Assembly {
    nx_native_eth_frames_t instance{};
    nx_eth_frames_descriptor_t tx[8]{}, rx[8]{};
    nx_eth_frames_tx_slot_t txslots[8]{};
    nx_eth_frames_rx_slot_t rxslots[8]{};
    alignas(4) uint8_t storage[8][256]{};
    nx_dma_memory_region_t region{1U, UINTPTR_MAX - 1U, 3U};
    nx_eth_frames_port_t face{&nx_eth_frames_mac_ops, &instance.mac};
    nx_eth_frames_rx_loan_t borrowed[8]{};
    size_t borrowed_count = 0U;
    Assembly() {
        auto& mac = instance.mac;
        mac.regions = &region;
        mac.region_count = 1U;
        mac.tx_descriptors = tx;
        mac.rx_descriptors = rx;
        mac.tx_slots = txslots;
        mac.rx_slots = rxslots;
        mac.tx_count = mac.rx_count = 8U;
        mac.hclk_hz = 168000000U;
        mac.clock_domain = 1U;
        mac.drain_timeout_us = 1000U;
        mac.address[0] = 2U;
        mac.address[5] = 1U;
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        mac.source = {61U, 5U, 0U};
        for (size_t i = 0U; i < 8U; ++i) {
            rxslots[i].data = storage[i];
            rxslots[i].capacity = 256U;
        }
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_native_eth_frames_initialize(
                                  &instance, nx_deadline_after(nx_time_now_us(),
                                                               1000000U)));
        const nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(&face, &link));
    }
    nx_result_t Acquire(nx_eth_frames_rx_loan_t* loan, nx_eth_fragment_t* views,
                        size_t capacity) {
        const nx_result_t result =
            nx_eth_frames_port_acquire(&face, loan, views, capacity);
        if (result == NX_SUCCESS) {
            EXPECT_LT(borrowed_count, 8U);
            if (borrowed_count < 8U) {
                borrowed[borrowed_count++] = *loan;
            }
        }
        return result;
    }
    void Shutdown() {
        instance.hardware.hold_reset = false;
        instance.hardware.hold_engine = false;
        instance.hardware.hold_ptp = false;
        instance.hardware.hold_interrupt_clear = false;
        while (borrowed_count != 0U) {
            const nx_result_t result = nx_eth_frames_port_release(
                &face, &borrowed[borrowed_count - 1U]);
            EXPECT_EQ(NX_SUCCESS, result);
            if (result != NX_SUCCESS) {
                break;
            }
            --borrowed_count;
        }
        if (instance.mac.retained) {
            nx_result_t result = nx_eth_frames_port_stop(&face);
            if (result != NX_SUCCESS) {
                result = nx_eth_frames_port_recover(
                    &face, nx_deadline_after(nx_time_now_us(), 1000000U));
                EXPECT_EQ(NX_SUCCESS, result);
                if (result == NX_SUCCESS) {
                    result = nx_eth_frames_port_stop(&face);
                }
            }
            EXPECT_EQ(NX_SUCCESS, result);
        }
        EXPECT_FALSE(instance.mac.retained);
        EXPECT_FALSE(instance.hardware.irq_enabled);
        EXPECT_EQ(0U, instance.mac.wake_publishers);
        EXPECT_EQ(0U, borrowed_count);
        bool pending = false;
        for (const auto& slot : txslots) {
            EXPECT_EQ(nullptr, slot.request);
            pending = pending || slot.request != nullptr;
        }
        if (instance.mac.retained || instance.hardware.irq_enabled ||
            instance.mac.wake_publishers != 0U || borrowed_count != 0U ||
            pending) {
            ADD_FAILURE() << "Cannot destroy a retained Native source or loan";
            std::abort();
        }
    }
    void Progress() {
        for (size_t i = 0U; i < 10U; ++i) {
            ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_service(&face));
        }
    }
};
class EthernetFramesNative : public testing::Test {
  protected:
    Assembly first, other;
    alignas(4) std::array<uint8_t, 300> bytes{};
    alignas(4) std::array<uint8_t, 13> head{};
    alignas(4) std::array<uint8_t, 51> tail{};
    nx_eth_fragment_t fragments[2]{};
    nx_eth_frames_tx_request_t request{};
    void SetUp() override {
        nx_native_clock_configure(false, 0U);
        bytes[12] = 0x81U;
        bytes[14] = 0xa0U;
        bytes[15] = 0x25U;
        bytes[16] = 0x08U;
        head[12] = 0x81U;
        tail[1] = 0xa0U;
        tail[2] = 0x25U;
        tail[3] = 0x08U;
        fragments[0] = {head.data(), head.size()};
        fragments[1] = {tail.data(), tail.size()};
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        first.Shutdown();
        other.Shutdown();
        nx_native_clock_configure(false, 0U);
    }
};
Assembly* scheduled_assembly;
nx_eth_frames_tx_request_t* scheduled_request;
void* scheduled_page;
constexpr size_t page_bytes = 4096U;
} /* namespace */
extern "C" void nx_eth_frames_native_before_loan_read(void* mac,
                                                      const void* request) {
    if (scheduled_assembly == nullptr || request != scheduled_request ||
        mac != &scheduled_assembly->instance.mac || nx_arch_irq_is_masked()) {
        return;
    }
    /* An actual model IRQ may run only outside the shared exclusion. Return
     * both descriptors, then execute real provider settlement before the
     * consumer legally reclaims its now unborrowed request page. */
    scheduled_assembly->tx[0].status = 0U;
    scheduled_assembly->tx[1].status = 0U;
    scheduled_assembly->Progress();
    if (nx_request_state(&scheduled_request->base) != NX_REQUEST_SETTLED) {
        _exit(41);
    }
    if (mprotect(scheduled_page, page_bytes, PROT_NONE) != 0) {
        _exit(42);
    }
}
namespace {
TEST_F(EthernetFramesNative,
       WholeEgressCopiesFragmentsBeforeActualProviderSettlement) {
    first.Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_tx_prepare(
                              &request, fragments, 2U, true,
                              nx_deadline_after(nx_time_now_us(), 1000000U)));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&first.face, &request));
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_frames_tick(&first.instance, 17123456780U));
    uint8_t output[64]{};
    size_t length = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_frames_transmit(&first.instance, output,
                                                        sizeof(output), &length,
                                                        NX_SUCCESS, true));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(64U, length);
    EXPECT_EQ(0, std::memcmp(output, head.data(), head.size()));
    EXPECT_EQ(0, std::memcmp(output + head.size(), tail.data(), tail.size()));
    first.Progress();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, request.stamp.source);
    EXPECT_EQ(17123456780U, request.stamp.ticks);
    EXPECT_EQ(1000000000U, request.stamp.ticks_per_second);
    EXPECT_FALSE(request.stamp.uncertainty_known);
}
TEST_F(EthernetFramesNative, EgressCapacityRejectsWithoutAnyPartialOwnReturn) {
    first.Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_tx_prepare(
                              &request, fragments, 2U, true,
                              nx_deadline_after(nx_time_now_us(), 1000000U)));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&first.face, &request));
    uint8_t output[63]{};
    size_t length = 99U;
    EXPECT_EQ(NX_ERROR_OVERFLOW, nx_native_eth_frames_transmit(
                                     &first.instance, output, sizeof(output),
                                     &length, NX_SUCCESS, true));
    EXPECT_EQ(99U, length);
    EXPECT_NE(0U, first.tx[0].status & UINT32_C(0x80000000));
    EXPECT_NE(0U, first.tx[1].status & UINT32_C(0x80000000));
}
TEST_F(EthernetFramesNative,
       FullFrameIngressCannotOverwriteAnyHeldFragmentAtWrap) {
    first.Start();
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_frames_receive(&first.instance, bytes.data(),
                                           bytes.size(), true));
    first.Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    ASSERT_EQ(NX_SUCCESS, first.Acquire(&loan, views, 2U));
    std::array<uint8_t, 256> before{};
    std::memcpy(before.data(), views[0].data, before.size());
    first.instance.hardware.rx_cursor = loan.first_slot;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_native_eth_frames_receive(&first.instance, bytes.data(),
                                           bytes.size(), true));
    EXPECT_EQ(0, std::memcmp(before.data(), views[0].data, before.size()));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_stop(&first.face));
}
TEST_F(EthernetFramesNative, CoreTimeDoesNotBecomeMissingHardwareTimestamp) {
    first.Start();
    const nx_time_us_t before = nx_time_now_us();
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_frames_receive(
                              &first.instance, bytes.data(), 64U, false));
    first.Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, first.Acquire(&loan, &view, 1U));
    EXPECT_EQ(NX_IO_TIMESTAMP_UNKNOWN, loan.stamp.source);
    EXPECT_GE(loan.acquired_us, before);
    EXPECT_LE(loan.acquired_us, nx_time_now_us());
    EXPECT_EQ(NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION, loan.acquired_source);
}
TEST_F(EthernetFramesNative,
       TwoRealModelsHaveIndependentCounterAndForeignLoanIdentity) {
    other.instance.mac.clock_domain = 2U;
    first.Start();
    other.Start();
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_frames_tick(&first.instance, 23000000000U));
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_frames_tick(&other.instance, 7000000000U));
    nx_io_timestamp_t a{}, b{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&first.face, &a));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&other.face, &b));
    EXPECT_NE(a.domain, b.domain);
    EXPECT_EQ(23000000000U, a.ticks);
    EXPECT_EQ(7000000000U, b.ticks);
    EXPECT_EQ(NX_IO_TIMESTAMP_SERVICE_OBSERVATION, a.source);
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_frames_receive(
                              &first.instance, bytes.data(), 64U, true));
    first.Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, first.Acquire(&loan, &view, 1U));
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_frames_port_release(&other.face, &loan));
    EXPECT_EQ(0U, first.rx[0].status & UINT32_C(0x80000000));
}
TEST_F(EthernetFramesNative,
       StopRestartChangesClockEpochWithoutReusingRxToken) {
    first.Start();
    nx_io_timestamp_t before{}, after{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&first.face, &before));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_stop(&first.face));
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_frames_initialize(
                              &first.instance,
                              nx_deadline_after(nx_time_now_us(), 1000000U)));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&first.face, &after));
    EXPECT_EQ(before.domain, after.domain);
    EXPECT_GT(after.epoch, before.epoch);
}
TEST_F(EthernetFramesNative,
       SourceValidationCannotReadARequestAfterItsFinalProviderAccess) {
    EXPECT_EXIT(
        {
            void* page = mmap(nullptr, page_bytes, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (page == MAP_FAILED) {
                _exit(40);
            }
            auto* loan = static_cast<nx_eth_frames_tx_request_t*>(page);
            *loan = {};
            nx_request_initialize(&loan->base);
            first.Start();
            if (nx_eth_frames_tx_prepare(
                    loan, fragments, 2U, true,
                    nx_deadline_after(nx_time_now_us(), 1000000U)) !=
                    NX_SUCCESS ||
                nx_eth_frames_port_submit(&first.face, loan) != NX_SUCCESS) {
                _exit(43);
            }
            scheduled_page = page;
            scheduled_request = loan;
            scheduled_assembly = &first;
            uint8_t output[64]{};
            size_t length = 0U;
            const nx_result_t result = nx_native_eth_frames_transmit(
                &first.instance, output, sizeof(output), &length, NX_SUCCESS,
                false);
            scheduled_assembly = nullptr;
            if (result != NX_SUCCESS || length != sizeof(output)) {
                _exit(44);
            }
            first.Progress();
            if (nx_request_state(&loan->base) != NX_REQUEST_SETTLED) {
                _exit(45);
            }
            first.Shutdown();
            if (munmap(page, page_bytes) != 0) {
                _exit(46);
            }
            _exit(0);
        },
        testing::ExitedWithCode(0), "");
}
} /* namespace */
