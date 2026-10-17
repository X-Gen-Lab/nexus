/**
 * \file            ethernet_frames_mac_test.cpp
 * \brief           Enhanced MAC contracts with an external MMIO oracle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Exercises actual provider code against modeled registers.
 */
#include "eth_frames_mac.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
namespace {
constexpr uint32_t own = UINT32_C(0x80000000);
constexpr uint32_t fs = UINT32_C(0x10000000);
constexpr uint32_t ls = UINT32_C(0x20000000);
constexpr uint32_t ttse = UINT32_C(0x02000000);
constexpr uint32_t ttss = UINT32_C(0x00020000);
constexpr unsigned bus = 0x1000U, status = 0x1014U, control = 0x1018U;
constexpr unsigned debug = 0x34U, ptp = 0x700U;
constexpr uintptr_t base = UINT32_C(0x40028000);
constexpr uintptr_t memory = UINT32_C(0x20000000);
constexpr size_t count = 8U;
uint64_t now;
class Hardware {
  public:
    MOCK_METHOD(void, Write, (unsigned, uint32_t));
    MOCK_METHOD(void, Publish, (size_t, uint32_t));
    std::array<uint32_t, 0x1060U / 4U> latched{};
    std::array<uint64_t, 0x1060U / 4U> last_write_ns{};
    uint64_t nanoseconds = 0U;
    unsigned mac_writes = 0U;
    unsigned mac_reads = 0U;
    bool hold_reset = false;
    bool hold_engine = false;
    bool hold_flush = false;
    bool hold_ptp = false;
    bool timestamp_rollover_on_read = false;
    bool illegal_flush = false;
    bool enforce_spacing = false;
    bool first_published_early = false;
    bool irq_enabled = false;
    bool irq_pending = false;
    size_t first = 0U;
    size_t segments = 0U;
};
Hardware* hardware;
uint32_t* Regs() {
    return reinterpret_cast<uint32_t*>(base);
}
nx_eth_frames_descriptor_t* Tx() {
    return reinterpret_cast<nx_eth_frames_descriptor_t*>(memory);
}
class EthernetFramesMac : public testing::Test {
  protected:
    testing::NiceMock<Hardware> oracle;
    void* mmio = nullptr;
    void* ram = nullptr;
    nx_eth_frames_descriptor_t* tx = Tx();
    nx_eth_frames_descriptor_t* rx =
        reinterpret_cast<nx_eth_frames_descriptor_t*>(memory + 0x100U);
    nx_eth_frames_tx_slot_t txslots[count]{};
    nx_eth_frames_rx_slot_t rxslots[count]{};
    nx_dma_memory_region_t region{memory, 0x20000U, 3U};
    nx_eth_frames_mac_t mac{};
    nx_eth_frames_tx_request_t request{}, second{};
    nx_eth_fragment_t pieces[3]{};
    nx_eth_fragment_t second_piece{};
    nx_eth_frames_port_t port{&nx_eth_frames_mac_ops, &mac};
    nx_eth_frames_rx_loan_t borrowed[count]{};
    size_t borrowed_count = 0U;
    struct Capture {
        std::mutex mutex;
        std::condition_variable changed;
        bool entered = false;
        bool released = false;
    } capture;
    const nx_irq_wake_t wake{&capture,
                             [](void* context) -> nx_result_t {
                                 auto* sink = static_cast<Capture*>(context);
                                 std::unique_lock<std::mutex> lock(sink->mutex);
                                 sink->entered = true;
                                 sink->changed.notify_all();
                                 sink->changed.wait(
                                     lock, [&] { return sink->released; });
                                 return NX_ERROR_IO;
                             },
                             false};
    uint8_t* first = reinterpret_cast<uint8_t*>(memory + 0x4000U);
    uint8_t* rest = reinterpret_cast<uint8_t*>(memory + 0x4800U);
    uint8_t* other = reinterpret_cast<uint8_t*>(memory + 0x5000U);
    void SetUp() override {
        hardware = &oracle;
        now = 100U;
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x40000000)), mmio);
        ram = mmap(reinterpret_cast<void*>(memory), 0x20000U,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(memory), ram);
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023830)) =
            UINT32_C(0x1e000000); /* MAC, TX, RX and separate PTP gate. */
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40013804)) =
            UINT32_C(0x00800000); /* RMII, no guessed PHY link. */
        /* Direct provider-unit precondition: an already established clock tree.
         * The normal generated-factory target must execute real System startup
         * against these same registers, rather than inherit this assignment. */
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023800)) = UINT32_C(
            0x03030003); /* HSE/HXTAL + PLL ready, HSI/IRC16 retained. */
#if defined(NX_ETH_FRAMES_TEST_GD32)
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023804)) =
            25U | (400U << 6U) | UINT32_C(0x00400000) | (9U << 24U);
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023c00)) = 7U;
#else
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023804)) =
            8U | (336U << 6U) | UINT32_C(0x00400000) | (7U << 24U);
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023c00)) = 0x705U;
#endif
        *reinterpret_cast<uint32_t*>(UINT32_C(0x40023808)) = UINT32_C(
            0x0000940a); /* Actual PLL source, HPRE1, APB1/4, APB2/2. */

        mac.registers = base;
        mac.regions = &region;
        mac.region_count = 1U;
        mac.tx_descriptors = tx;
        mac.rx_descriptors = rx;
        mac.tx_slots = txslots;
        mac.rx_slots = rxslots;
        mac.tx_count = mac.rx_count = count;
        mac.hclk_hz = 168000000U;
        mac.clock_domain = 1U;
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
#if defined(NX_ETH_FRAMES_TEST_GD32)
        mac.hclk_hz = 200000000U;
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 91U, 4U, 0U};
#endif
        mac.source = {61U, 5U, 0U};
        mac.irq_control = nx_eth_frames_model_irq_control;
        mac.pend_irq = nx_eth_frames_model_pend_irq;
        mac.validate_wake = nx_eth_frames_model_validate_wake;
        mac.address[0] = 2U;
        mac.address[5] = 1U;
        mac.drain_timeout_us = 1000U;
        for (size_t i = 0U; i < count; ++i) {
            rxslots[i].data =
                reinterpret_cast<uint8_t*>(memory + 0x1000U + i * 256U);
            rxslots[i].capacity = 256U;
        }
        first[12] = 8U;
        other[12] = 8U;
        pieces[0] = {first, 13U};
        pieces[1] = {rest, 51U}; /* Type field crosses aligned buffers. */
        second_piece = {other, 64U};
        nx_request_initialize(&request.base);
        nx_request_initialize(&second.base);
    }
    nx_result_t Acquire(nx_eth_frames_rx_loan_t* loan, nx_eth_fragment_t* views,
                        size_t capacity) {
        const nx_result_t result =
            nx_eth_frames_port_acquire(&port, loan, views, capacity);
        if (result == NX_SUCCESS) {
            EXPECT_LT(borrowed_count, count);
            if (borrowed_count < count) {
                borrowed[borrowed_count++] = *loan;
            }
        }
        return result;
    }
    nx_result_t Release(const nx_eth_frames_rx_loan_t* loan) {
        const nx_result_t result = nx_eth_frames_port_release(&port, loan);
        if (result == NX_SUCCESS) {
            for (size_t i = 0U; i < borrowed_count; ++i) {
                if (borrowed[i].identity == loan->identity &&
                    borrowed[i].epoch == loan->epoch) {
                    borrowed[i] = borrowed[--borrowed_count];
                    break;
                }
            }
        }
        return result;
    }
    void TearDown() override {
        testing::Mock::VerifyAndClearExpectations(&oracle);
        oracle.hold_reset = oracle.hold_engine = oracle.hold_flush = false;
        oracle.hold_ptp = false;
        while (borrowed_count != 0U) {
            const auto loan = borrowed[borrowed_count - 1U];
            const nx_result_t result = Release(&loan);
            EXPECT_EQ(NX_SUCCESS, result);
            if (result != NX_SUCCESS) {
                break;
            }
        }
        if (mac.retained) {
            nx_result_t result = nx_eth_frames_port_stop(&port);
            if (result != NX_SUCCESS) {
                result = nx_eth_frames_port_recover(&port, now + 1000U);
                EXPECT_EQ(NX_SUCCESS, result);
                if (result == NX_SUCCESS) {
                    result = nx_eth_frames_port_stop(&port);
                }
            }
            EXPECT_EQ(NX_SUCCESS, result);
        }
        EXPECT_EQ(0U, borrowed_count);
        EXPECT_FALSE(mac.retained);
        EXPECT_EQ(0U, mac.wake_publishers);
        EXPECT_FALSE(oracle.irq_enabled);
        bool pending = false;
        for (const auto& slot : txslots) {
            EXPECT_EQ(nullptr, slot.request);
            pending = pending || slot.request != nullptr;
        }
        if (mac.retained || borrowed_count != 0U || mac.wake_publishers != 0U ||
            oracle.irq_enabled || pending) {
            ADD_FAILURE() << "Retained source or loan prevents safe unmapping";
            std::abort();
        }
        hardware = nullptr;
        if (ram != nullptr && ram != MAP_FAILED) {
            munmap(ram, 0x20000U);
        }
        if (mmio != nullptr && mmio != MAP_FAILED) {
            munmap(mmio, 0x80000U);
        }
    }
    nx_result_t Initialize(nx_time_us_t deadline = 10000U) {
#if defined(NX_ETH_FRAMES_TEST_STM32)
        return nx_stm32_eth_frames_initialize(&mac, deadline);
#elif defined(NX_ETH_FRAMES_TEST_GD32)
        return nx_gd32_eth_frames_initialize(&mac, deadline);
#else
        return nx_eth_frames_mac_initialize(&mac, deadline);
#endif
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, Initialize());
        const nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(&port, &link));
        ASSERT_EQ(NX_SUCCESS,
                  nx_eth_frames_tx_prepare(&request, pieces, 2U, true, 10000U));
    }
    void Progress() {
        for (size_t i = 0U; i < count + 2U; ++i) {
            ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_service(&port));
        }
    }
    void RxFrame(size_t first_slot, size_t length, bool tagged = false,
                 uint32_t last_flags = 0U) {
        const size_t wire = length + 4U;
        size_t remaining = wire;
        size_t index = first_slot;
        size_t ordinal = 0U;
        while (remaining != 0U) {
            const size_t bytes = remaining < 256U ? remaining : 256U;
            std::memset(rxslots[index].data, 0x22, bytes);
            if (ordinal == 0U) {
                auto* data = rxslots[index].data;
                data[12] = tagged ? 0x81U : 0x08U;
                data[13] = 0U;
                if (tagged) {
                    data[14] = 0xb0U;
                    data[15] = 0x25U;
                    data[16] = 0x08U;
                    data[17] = 0x00U;
                }
            }
            rx[index].status = ordinal == 0U ? 0x200U : 0U;
            remaining -= bytes;
            if (remaining == 0U) {
                rx[index].status |=
                    static_cast<uint32_t>(wire << 16U) | 0x120U | last_flags;
            }
            ++ordinal;
            index = (index + 1U) % count;
        }
        Regs()[status / 4U] |= 0x40U;
    }
};
} /* namespace */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t nx_eth_frames_model_read(uintptr_t address,
                                             unsigned offset) {
    auto* registers = reinterpret_cast<uint32_t*>(address);
    ++hardware->mac_reads;
#if defined(NX_ETH_FRAMES_TEST_GD32)
    hardware->nanoseconds += 5U;
#else
    hardware->nanoseconds += 6U;
#endif
    if (offset == 0x708U && hardware->timestamp_rollover_on_read) {
        hardware->timestamp_rollover_on_read = false;
        const uint32_t before = registers[offset / 4U];
        registers[0x708U / 4U] = before + 1U;
        registers[0x70cU / 4U] = 20U;
        return before;
    }
    return registers[offset / 4U];
}
extern "C" void nx_eth_frames_model_write(uintptr_t address, unsigned offset,
                                          uint32_t value) {
    auto* registers = reinterpret_cast<uint32_t*>(address);
    ++hardware->mac_writes;
#if defined(NX_ETH_FRAMES_TEST_GD32)
    hardware->nanoseconds += 5U;
#else
    hardware->nanoseconds += 6U;
#endif
    hardware->Write(offset, value);
    if (offset == status) {
        registers[offset / 4U] &= ~(value & UINT32_C(0x0001e7ff));
        return;
    }
    if (offset == control && (value & 0x100000U) != 0U &&
        (registers[debug / 4U] & 0x01770000U) != 0U) {
        hardware->illegal_flush = true;
    }
    if (offset == control) {
        if (hardware->hold_engine) {
            value |= registers[control / 4U] & UINT32_C(0x2002);
        }
        if ((value & UINT32_C(0x2000)) != 0U) {
            registers[status / 4U] |= UINT32_C(0x00600000);
        }
        if ((value & 2U) != 0U) {
            registers[status / 4U] |= UINT32_C(0x00060000);
        }
    }
    /* Erratum oracle separates visible write/readback from internal latching.
     */
    const size_t i = offset / 4U;
    const bool affected = offset == 0U || offset == 4U || offset == 0x1cU ||
                          offset == bus || offset == control || offset == ptp;
    registers[i] = value;
    if (!hardware->enforce_spacing || !affected ||
        hardware->last_write_ns[i] == 0U ||
        hardware->nanoseconds - hardware->last_write_ns[i] >= 80U) {
        hardware->latched[i] = value;
    }
    hardware->last_write_ns[i] = hardware->nanoseconds;
}
extern "C" void nx_eth_frames_model_poll(void* context) {
    auto* mac = static_cast<nx_eth_frames_mac_t*>(context);
    auto* registers = reinterpret_cast<uint32_t*>(mac->registers);
    ++now;
    hardware->nanoseconds += 1000U;
    if ((registers[bus / 4U] & 1U) != 0U && !hardware->hold_reset) {
        std::memset(registers, 0, 0x1060U);
        hardware->latched.fill(0U);
    }
    if (!hardware->hold_engine &&
        (registers[control / 4U] & UINT32_C(0x2000)) == 0U) {
        registers[status / 4U] &= ~UINT32_C(0x00700000);
    }
    if (!hardware->hold_engine && (registers[control / 4U] & 2U) == 0U) {
        registers[status / 4U] &= ~UINT32_C(0x000e0000);
    }
    if (!hardware->hold_flush) {
        registers[control / 4U] &= ~UINT32_C(0x00100000);
    }
    if (!hardware->hold_ptp) {
        if ((registers[ptp / 4U] & 4U) != 0U) {
            registers[0x708U / 4U] = registers[0x710U / 4U];
            registers[0x70cU / 4U] = registers[0x714U / 4U];
        }
        registers[ptp / 4U] &= ~UINT32_C(0x2c);
    }
}
extern "C" void nx_eth_frames_model_descriptor(void* descriptor,
                                               uint32_t value) {
    auto* d = static_cast<nx_eth_frames_descriptor_t*>(descriptor);
    const uintptr_t descriptor_address = reinterpret_cast<uintptr_t>(d);
    const size_t index = descriptor_address >= memory
                             ? (descriptor_address - memory) / sizeof(*d)
                             : count;
    if (index < count) {
        hardware->Publish(index, value);
        if (index == hardware->first && hardware->segments > 1U &&
            (value & own) != 0U) {
            for (size_t i = 1U; i < hardware->segments; ++i) {
                if ((Tx()[(index + i) % count].status & own) == 0U) {
                    hardware->first_published_early = true;
                }
            }
        }
    }
    d->status = value;
}
extern "C" nx_result_t nx_eth_frames_model_irq_control(nx_eth_frames_mac_t*,
                                                       bool enabled) {
    hardware->irq_enabled = enabled;
    if (!enabled) {
        hardware->irq_pending = false;
    }
    return NX_SUCCESS;
}
extern "C" void nx_eth_frames_model_pend_irq(nx_eth_frames_mac_t*) {
    hardware->irq_pending = true;
}
extern "C" nx_result_t
nx_eth_frames_model_validate_wake(nx_eth_frames_mac_t* mac,
                                  const nx_irq_wake_t* wake, uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling) {
        return NX_ERROR_PERMISSION;
    }
    return nx_irq_wake_validate(wake, &mac->irq_policy, &mac->source);
}
TEST_F(EthernetFramesMac,
       InitializesEightWordDescriptorsAndDigitalHardwareClock) {
    static_assert(sizeof(nx_eth_frames_descriptor_t) == 32U);
    ASSERT_EQ(NX_SUCCESS, Initialize());
    EXPECT_NE(0U, Regs()[bus / 4U] & 0x80U);
    EXPECT_EQ(20U, Regs()[0x704U / 4U]);
    EXPECT_EQ(0x303U, Regs()[ptp / 4U] & 0x303U);
#if defined(NX_ETH_FRAMES_TEST_GD32)
    EXPECT_EQ(1073741824U, Regs()[0x718U / 4U]);
#else
    EXPECT_EQ(1278264076U, Regs()[0x718U / 4U]);
#endif
    EXPECT_EQ(memory + 32U, tx[0].next);
    EXPECT_EQ(memory, tx[count - 1U].next);
    EXPECT_NE(0U, mac.clock_epoch);
}
TEST_F(EthernetFramesMac,
       PublishesWholeFrameBeforeFirstOwnAndUsesOnlyLastStatus) {
    Start();
    oracle.first = 0U;
    oracle.segments = 2U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    EXPECT_FALSE(oracle.first_published_early);
    EXPECT_EQ(fs | ttse, tx[0].status & (fs | ls | ttse));
    EXPECT_EQ(ls, tx[1].status & (fs | ls | ttse));
    tx[0].status = 0U; /* Real writeback may clear every control bit. */
    Progress();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Regs()[0x708U / 4U] = 18U;
    tx[1].timestamp_high = 17U;
    tx[1].timestamp_low = 123456780U;
    tx[1].status = ttss; /* LS is software metadata, not writeback proof. */
    Progress();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(64U, request.base.transferred);
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, request.stamp.source);
    EXPECT_EQ(17123456780U, request.stamp.ticks);
    EXPECT_EQ(1000000000U, request.stamp.ticks_per_second);
    EXPECT_FALSE(request.stamp.uncertainty_known);
}
TEST_F(EthernetFramesMac,
       MissingTimestampNeverReusesPreviousDescriptorCapture) {
    Start();
    tx[1].timestamp_high = 71U;
    tx[1].timestamp_low = 120U;
    tx[1].status = ttss;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    tx[0].status = 0U;
    tx[1].status = 0U;
    Progress();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(NX_IO_TIMESTAMP_UNKNOWN, request.stamp.source);
}
TEST_F(EthernetFramesMac,
       RingAdmissionRejectsWholeTransactionWithoutOwnOrBorrow) {
    Start();
    for (size_t i = 1U; i < count; ++i) {
        tx[i].status = own;
    }
    const auto before = request;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_submit(&port, &request));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(0U, tx[0].status & own);
}
TEST_F(EthernetFramesMac,
       InBandVlanAcrossTxFragmentsIsPreservedWithoutInsertion) {
    Start();
    first[12] = 0x81U;
    rest[0] = 0U;
    rest[1] = 0xb0U;
    rest[2] = 0x25U;
    rest[3] = 0x08U;
    rest[4] = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_eth_frames_tx_prepare(&request, pieces, 2U, true, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(first), tx[0].buffer);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(rest), tx[1].buffer);
    EXPECT_EQ(13U, tx[0].control & 0x1fffU);
    EXPECT_EQ(51U, tx[1].control & 0x1fffU);
    EXPECT_EQ(0x81U, first[12]);
    EXPECT_EQ(0xb0U, rest[1]);
}
TEST_F(EthernetFramesMac, IncompleteRxNeverExposesOneFragmentAsAFrame) {
    Start();
    rx[0].status = 0x200U;
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    EXPECT_EQ(NX_ERROR_EMPTY, Acquire(&loan, views, 2U));
    EXPECT_EQ(0U, rx[0].status & own);
}
TEST_F(EthernetFramesMac, FcsAcrossLastTwoBuffersIsTrimmedWithoutEarlyRearm) {
    Start();
    RxFrame(0U, 254U);
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, views, 2U));
    EXPECT_EQ(254U, loan.length);
    EXPECT_EQ(2U, loan.descriptor_count);
    EXPECT_EQ(1U, loan.fragment_count);
    EXPECT_EQ(254U, views[0].length);
    EXPECT_EQ(0U, rx[0].status & own);
    EXPECT_EQ(0U, rx[1].status & own);
    ASSERT_EQ(NX_SUCCESS, Release(&loan));
    EXPECT_NE(0U, rx[0].status & own);
    EXPECT_NE(0U, rx[1].status & own);
}
TEST_F(EthernetFramesMac, CapacityFailurePreservesQueuedWholeFrameAndOutputs) {
    Start();
    RxFrame(0U, 300U);
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    const auto before = loan;
    EXPECT_EQ(NX_ERROR_OVERFLOW, Acquire(&loan, views, 1U));
    EXPECT_EQ(0, std::memcmp(&before, &loan, sizeof(loan)));
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, views, 2U));
    EXPECT_EQ(300U, loan.length);
    EXPECT_EQ(2U, loan.fragment_count);
}
TEST_F(EthernetFramesMac,
       VlanAndLastDescriptorTimestampRemainExplicitHardwareFacts) {
    Start();
    RxFrame(0U, 300U, true, 0x480U); /* VLAN bit10, enhanced TSV bit7. */
    rx[0].timestamp_high = 999U; /* First segment is not the capture source. */
    Regs()[0x708U / 4U] = 24U;
    rx[1].timestamp_high = 23U;
    rx[1].timestamp_low = 500000000U;
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, views, 2U));
    EXPECT_TRUE(loan.vlan.present);
    EXPECT_EQ(0xb025U, loan.vlan.tci);
    EXPECT_EQ(0x81U, views[0].data[12]); /* No tag stripping. */
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, loan.stamp.source);
    EXPECT_EQ(23500000000U, loan.stamp.ticks);
    EXPECT_EQ(1000000000U, loan.stamp.ticks_per_second);
    EXPECT_EQ(mac.clock_domain, loan.stamp.domain);
    EXPECT_EQ(mac.clock_epoch, loan.stamp.epoch);
}
TEST_F(EthernetFramesMac,
       AllOnesCaptureIsCorruptInsteadOfNanosecondsOrAcquisitionTime) {
    Start();
    RxFrame(0U, 64U, false, 0x80U);
    rx[0].timestamp_high = UINT32_MAX;
    rx[0].timestamp_low = UINT32_MAX;
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, &view, 1U));
    EXPECT_EQ(NX_IO_TIMESTAMP_UNKNOWN, loan.stamp.source);
    EXPECT_NE(0U, loan.capture_flags & NX_ETH_FRAMES_CAPTURE_CORRUPT);
    EXPECT_EQ(now, loan.acquired_us);
}
TEST_F(EthernetFramesMac, HeldWholeFrameBlocksResetAndSurvivesStopAtRingWrap) {
    Start();
    RxFrame(0U, 300U);
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, views, 2U));
    std::array<uint8_t, 256> before{};
    std::memcpy(before.data(), views[0].data, before.size());
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_stop(&port));
    EXPECT_CALL(oracle, Write(bus, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_recover(&port, 10000U));
    EXPECT_EQ(0, std::memcmp(before.data(), views[0].data, before.size()));
    EXPECT_EQ(0U, rx[0].status & own);
    EXPECT_EQ(0U, rx[1].status & own);
    testing::Mock::VerifyAndClearExpectations(&oracle);
    ASSERT_EQ(NX_SUCCESS, Release(&loan));
}
TEST_F(EthernetFramesMac,
       AbortIsControllerWideButNeverRevokesEitherPayloadLoan) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_tx_prepare(&second, &second_piece, 1U,
                                                   false, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &second));
    oracle.hold_engine = true;
    Regs()[status / 4U] |= 0x100000U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_cancel(&port, &request));
    now += 1001U;
    Progress();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&second.base));
    EXPECT_NE(0U, tx[0].buffer);
    EXPECT_NE(0U, tx[2].buffer);
    oracle.hold_reset = true;
    EXPECT_NE(NX_SUCCESS, nx_eth_frames_port_recover(&port, now + 4U));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&second.base));
}
TEST_F(EthernetFramesMac,
       ExplicitProvenResetChangesClockEpochAndSettlesWithoutReplay) {
    Start();
    const uint64_t epoch = mac.clock_epoch;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    oracle.hold_engine = true;
    Regs()[status / 4U] |= 0x100000U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_cancel(&port, &request));
    now += 1001U;
    Progress();
    ASSERT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    oracle.hold_engine = false;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_recover(&port, now + 1000U));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
    EXPECT_GT(mac.clock_epoch, epoch);
    EXPECT_EQ(0U, tx[0].status & own);
    EXPECT_EQ(0U, tx[1].status & own);
}
TEST_F(EthernetFramesMac, CoherentCounterReadRetriesSecondsRollover) {
    Start();
    Regs()[0x708U / 4U] = 14U;
    Regs()[0x70cU / 4U] = 999999980U;
    oracle.timestamp_rollover_on_read = true;
    nx_io_timestamp_t stamp{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&port, &stamp));
    EXPECT_EQ(15000000020U, stamp.ticks);
    EXPECT_EQ(NX_IO_TIMESTAMP_SERVICE_OBSERVATION, stamp.source);
}
TEST_F(EthernetFramesMac,
       VisibleReadbackCannotReplaceRequiredInternalClockSpacing) {
    Start();
    oracle.last_write_ns[0] = 0U;
    oracle.enforce_spacing = true;
    const nx_eth_link_t down{false, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(&port, &down));
    EXPECT_EQ(0U, Regs()[0] & 0x0cU);
    EXPECT_EQ(0U, oracle.latched[0] & 0x0cU);
}
TEST_F(EthernetFramesMac, NonemptyFifoIsNeverFlushedToInventCancellationProof) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    Regs()[debug / 4U] = 0x01000000U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_cancel(&port, &request));
    Progress();
    EXPECT_FALSE(oracle.illegal_flush);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(EthernetFramesMac, ColdInitializeRejectsLiveRawOwnerBeforeReset) {
    Regs()[0x1010U / 4U] = static_cast<uint32_t>(memory + 0x6000U);
    EXPECT_EQ(NX_ERROR_BUSY, Initialize());
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
}
#if defined(NX_ETH_FRAMES_TEST_STM32) || defined(NX_ETH_FRAMES_TEST_GD32)
TEST_F(EthernetFramesMac, MissingSeparatePtpGateRejectsBeforeAnyMacAccess) {
    *reinterpret_cast<uint32_t*>(UINT32_C(0x40023830)) &= ~UINT32_C(0x10000000);
    EXPECT_EQ(NX_ERROR_STATE, Initialize());
    EXPECT_EQ(0U, oracle.mac_reads);
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
}
TEST_F(EthernetFramesMac, CcmAndTcmStorageCannotBecomeEthernetDescriptors) {
    mac.tx_descriptors =
        reinterpret_cast<nx_eth_frames_descriptor_t*>(UINT32_C(0x10000000));
    EXPECT_NE(NX_SUCCESS, Initialize());
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
}
#endif
TEST_F(EthernetFramesMac, ActiveWholeLoansProtectEveryMutableOutput) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    const auto before = request;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_frames_port_state(
                  &port, reinterpret_cast<nx_eth_frames_state_t*>(&request)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_frames_port_clock(
                  &port, reinterpret_cast<nx_io_timestamp_t*>(rest)));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_eth_frames_port_state(
                  &port, reinterpret_cast<nx_eth_frames_state_t*>(&port)));
    EXPECT_EQ(&nx_eth_frames_mac_ops, port.ops);
}
TEST_F(EthernetFramesMac,
       ReceiveCrcOrDescriptorErrorDropsAllFragmentsNotOnlyTail) {
    Start();
    RxFrame(0U, 300U, false, 0xc002U); /* ES, DE and CRC. */
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t views[2]{};
    EXPECT_EQ(NX_ERROR_EMPTY, Acquire(&loan, views, 2U));
    EXPECT_NE(0U, rx[0].status & own);
    EXPECT_NE(0U, rx[1].status & own);
    nx_eth_frames_state_t snapshot{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_state(&port, &snapshot));
    EXPECT_NE(0U, snapshot.mac.discarded);
}
TEST_F(EthernetFramesMac, EpochNeverReusesAnAlreadyReleasedReceiveIncarnation) {
    Start();
    RxFrame(0U, 64U);
    Progress();
    nx_eth_frames_rx_loan_t first_loan{}, second_loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&first_loan, &view, 1U));
    ASSERT_EQ(NX_SUCCESS, Release(&first_loan));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_stop(&port));
    ASSERT_EQ(NX_SUCCESS, Initialize());
    const nx_eth_link_t link{true, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(&port, &link));
    RxFrame(0U, 64U);
    Progress();
    ASSERT_EQ(NX_SUCCESS, Acquire(&second_loan, &view, 1U));
    EXPECT_GT(second_loan.epoch, first_loan.epoch);
    EXPECT_EQ(NX_ERROR_STATE, Release(&first_loan));
    EXPECT_EQ(0U, rx[0].status & own);
}
TEST_F(EthernetFramesMac, CapturedWakeBlocksRecoveryAndStorageReclamation) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_attach_wake(&port, &wake, 0U));
    RxFrame(0U, 64U);
    std::thread publisher([&] { nx_eth_frames_mac_irq(&mac); });
    bool captured = false;
    {
        std::unique_lock<std::mutex> lock(capture.mutex);
        captured = capture.changed.wait_for(lock, std::chrono::seconds(1),
                                            [&] { return capture.entered; });
    }
    EXPECT_TRUE(captured);
    if (captured) {
        EXPECT_EQ(NX_ERROR_BUSY, nx_eth_frames_port_stop(&port));
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_eth_frames_port_recover(&port, now + 1000U));
    }
    {
        std::lock_guard<std::mutex> lock(capture.mutex);
        capture.released = true;
        capture.changed.notify_all();
    }
    publisher.join();
    EXPECT_EQ(0U, mac.wake_publishers);
    EXPECT_EQ(NX_SUCCESS, nx_eth_frames_port_stop(&port));
}
TEST_F(EthernetFramesMac,
       RejectedStartWithdrawalCannotTurnAProcessIntoStopped) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    oracle.hold_engine = true;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_cancel(&port, &request));
    for (unsigned i = 0U; i < 4U; ++i) {
        nx_eth_frames_model_poll(&mac);
    }
    now += 1001U;
    Progress();
    EXPECT_NE(0U, Regs()[control / 4U] & UINT32_C(0x2000));
    EXPECT_NE(0U, Regs()[status / 4U] & UINT32_C(0x00700000));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
}
TEST_F(EthernetFramesMac,
       IrqCannotRetireAcceptedMetadataBeforeFirstOwnRelease) {
    Start();
    bool interrupted = false;
    EXPECT_CALL(oracle, Publish(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(oracle, Publish(1U, testing::_))
        .WillRepeatedly([&](size_t, uint32_t value) {
            if (!interrupted && (value & own) != 0U) {
                /* Variable fragment work must not hold the metadata mask. */
                EXPECT_FALSE(nx_arch_irq_is_masked());
                if (!nx_arch_irq_is_masked()) {
                    interrupted = true;
                    nx_eth_frames_mac_irq(&mac);
                    EXPECT_NE(NX_REQUEST_SETTLED,
                              nx_request_state(&request.base));
                }
            }
        });
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    EXPECT_TRUE(interrupted);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_NE(0U, tx[0].status & own);
    EXPECT_NE(0U, tx[1].status & own);
    testing::Mock::VerifyAndClearExpectations(&oracle);
}
TEST_F(EthernetFramesMac,
       HardwareZeroCaptureRemainsValidWithoutInventedUncertainty) {
    Start();
    RxFrame(0U, 64U, false, 0x80U);
    rx[0].timestamp_high = 0U;
    rx[0].timestamp_low = 0U;
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, &view, 1U));
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, loan.stamp.source);
    EXPECT_EQ(0U, loan.stamp.ticks);
    EXPECT_EQ(1000000000U, loan.stamp.ticks_per_second);
    EXPECT_NE(0U, loan.stamp.domain);
    EXPECT_NE(0U, loan.stamp.epoch);
    EXPECT_FALSE(loan.stamp.uncertainty_known);
    EXPECT_EQ(0U, loan.stamp.uncertainty_ticks);
    EXPECT_EQ(NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION, loan.acquired_source);
}
TEST_F(EthernetFramesMac,
       RealHardwareCaptureUsesTheSharedTimestampComparisonContract) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(&port, &request));
    Regs()[0x708U / 4U] = 2U;
    tx[0].status = 0U;
    tx[1].timestamp_high = 1U;
    tx[1].timestamp_low = 0U;
    tx[1].status = ttss;
    Progress();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, request.stamp.source);
    nx_io_timestamp_t observed{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&port, &observed));
    EXPECT_EQ(NX_IO_TIMESTAMP_SERVICE_OBSERVATION, observed.source);
    nx_io_timestamp_delta_t delta{};
    ASSERT_EQ(NX_SUCCESS,
              nx_io_timestamp_elapsed(&request.stamp, &observed, &delta));
    EXPECT_EQ(1000000000U, delta.ticks);
    EXPECT_FALSE(delta.uncertainty_known);
    EXPECT_EQ(0U, delta.uncertainty_ticks);
}
TEST_F(EthernetFramesMac,
       NaturalSecondsWrapExtendsTicksWithoutChangingClockEpoch) {
    Start();
    const uint32_t epoch = mac.clock_epoch;
    nx_io_timestamp_t stamp{};
    const uint32_t samples[]{UINT32_MAX / 3U, 2U * (UINT32_MAX / 3U),
                             UINT32_MAX - 1U};
    for (uint32_t seconds : samples) {
        now = 100U + static_cast<uint64_t>(seconds) * 1000000U;
        Regs()[0x708U / 4U] = seconds;
        Regs()[0x70cU / 4U] = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&port, &stamp));
    }
    now += 2000000U;
    Regs()[0x708U / 4U] = 0U;
    Regs()[0x70cU / 4U] = 20U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&port, &stamp));
    EXPECT_EQ((UINT64_C(1) << 32U) * UINT64_C(1000000000) + 20U, stamp.ticks);
    EXPECT_EQ(epoch, stamp.epoch);
}

TEST_F(EthernetFramesMac,
       CounterRegressionCannotPublishAValidFactWithinOneClockEpoch) {
    Start();
    Regs()[0x708U / 4U] = 1U;
    Regs()[0x70cU / 4U] = 900U;
    nx_io_timestamp_t first_timestamp{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(&port, &first_timestamp));
    ASSERT_EQ(1000000900U, first_timestamp.ticks);
    Regs()[0x70cU / 4U] = 800U;
    nx_io_timestamp_t output = first_timestamp;
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_frames_port_clock(&port, &output));
    EXPECT_EQ(first_timestamp.ticks, output.ticks);
    EXPECT_EQ(first_timestamp.epoch, output.epoch);
    EXPECT_EQ(first_timestamp.domain, output.domain);
    EXPECT_EQ(first_timestamp.source, output.source);
    EXPECT_EQ(first_timestamp.ticks_per_second, output.ticks_per_second);
    EXPECT_EQ(first_timestamp.uncertainty_known, output.uncertainty_known);
    EXPECT_EQ(first_timestamp.uncertainty_ticks, output.uncertainty_ticks);
}

TEST_F(EthernetFramesMac, ReceiveTokenIdentityDoesNotIncludeStructPadding) {
    Start();
    RxFrame(0U, 64U, false, 0x80U);
    Progress();
    nx_eth_frames_rx_loan_t loan{};
    nx_eth_fragment_t view{};
    ASSERT_EQ(NX_SUCCESS, Acquire(&loan, &view, 1U));
    nx_eth_frames_rx_loan_t copied = loan;
    constexpr size_t padding = offsetof(nx_io_timestamp_t, uncertainty_known) +
                               sizeof(copied.stamp.uncertainty_known);
    ASSERT_LT(padding, sizeof(copied.stamp));
    auto* representation = reinterpret_cast<unsigned char*>(&copied.stamp);
    for (size_t i = padding; i < sizeof(copied.stamp); ++i) {
        representation[i] ^= 0xffU;
    }
    /* C structure copies preserve field values, not unspecified padding bytes.
     * No semantic identity, timestamp or lease field has been changed. */
    EXPECT_EQ(NX_SUCCESS, Release(&copied));
}

TEST_F(EthernetFramesMac,
       FragmentMetadataCannotReadAProtectedMacControlWindow) {
    Start();
    request.fragments = reinterpret_cast<const nx_eth_fragment_t*>(base);
    request.fragment_count = 1U;
    EXPECT_EXIT(
        {
            if (mprotect(reinterpret_cast<void*>(base), 0x2000U, PROT_NONE) !=
                0) {
                _exit(51);
            }
            const nx_result_t result =
                nx_eth_frames_port_submit(&port, &request);
            if (mprotect(reinterpret_cast<void*>(base), 0x2000U,
                         PROT_READ | PROT_WRITE) != 0) {
                _exit(52);
            }
            _exit(result == NX_ERROR_INVALID ? 0 : 53);
        },
        testing::ExitedWithCode(0), "");
}

#if defined(NX_ETH_FRAMES_TEST_STM32) || defined(NX_ETH_FRAMES_TEST_GD32)
TEST_F(EthernetFramesMac,
       SixteenMegahertzFallbackCannotBeAuthorizedByConfiguredFrequency) {
    *reinterpret_cast<uint32_t*>(UINT32_C(0x40023800)) = 3U;
    *reinterpret_cast<uint32_t*>(UINT32_C(0x40023808)) = 0U;
    EXPECT_EQ(NX_ERROR_STATE, Initialize());
    EXPECT_EQ(0U, oracle.mac_reads);
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
    for (size_t i = 0U; i < count; ++i) {
        EXPECT_EQ(0U, tx[i].status);
        EXPECT_EQ(0U, rx[i].status);
    }
}
TEST_F(EthernetFramesMac, PllReadyCannotAuthorizeAnIncorrectActualAhbDivider) {
    *reinterpret_cast<uint32_t*>(UINT32_C(0x40023808)) |= 0x80U; /* HPRE2. */
    EXPECT_EQ(NX_ERROR_STATE, Initialize());
    EXPECT_EQ(0U, oracle.mac_reads);
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
}
TEST_F(EthernetFramesMac,
       RequestedPllSourceCannotSubstituteForActualSourceStatus) {
    *reinterpret_cast<uint32_t*>(UINT32_C(0x40023808)) &= ~UINT32_C(0x0c);
    EXPECT_EQ(NX_ERROR_STATE, Initialize());
    EXPECT_EQ(0U, oracle.mac_reads);
    EXPECT_EQ(0U, oracle.mac_writes);
    EXPECT_FALSE(mac.retained);
}
#endif
