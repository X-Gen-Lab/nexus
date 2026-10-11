/**
 * \file            ethernet_mac_test.cpp
 * \brief           Production descriptor ownership and MMIO fault contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "eth_mac.h"
#if defined(NX_ETH_TEST_STM32)
#include "stm32f407_eth.h"
#elif defined(NX_ETH_TEST_GD32)
extern "C" {
#include "gd32f470_eth.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
NVIC_Type g_gd32_model_nvic;
}
#endif
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
#include <sys/mman.h>
#endif
#include <array>
#include <condition_variable>
#include <cstring>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
namespace {
constexpr uint32_t own = UINT32_C(0x80000000);
constexpr unsigned ctl = 0x1018, dma_status = 0x1014;
uint64_t now;
bool hold_reset, hold_engine, hold_flush, hold_own, hold_mdio;
bool hold_table_clear;
uint16_t phy[32];
unsigned mdio_writes;
class EthernetMac : public testing::Test {
  protected:
    uint32_t regstore[0x1060 / 4]{};
    uint32_t* regs = regstore;
    void* registers_mapping = nullptr;
    void* memory_mapping = nullptr;
    nx_eth_descriptor_t txstore[2]{}, rxstore[2]{};
    nx_eth_descriptor_t* tx = txstore;
    nx_eth_descriptor_t* rx = rxstore;
    nx_eth_tx_slot_t txslots[2]{};
    nx_eth_rx_slot_t rxslots[2]{};
    alignas(4) uint8_t bufferstore[2][1520]{};
    uint8_t (*buffers)[1520] = bufferstore;
    alignas(4) uint8_t outputstore[64]{};
    uint8_t* output = outputstore;
    nx_dma_memory_region_t regions[1]{{1U, UINTPTR_MAX - 1U, 3U}};
    nx_eth_mac_t mac{};
    nx_eth_tx_request_t request{};
    const nx_eth_port_t port{&nx_eth_mac_ops, &mac};
    const nx_mdio_port_t mdio{&nx_eth_mdio_ops, &mac};
    void SetUp() override {
        now = 100;
        hold_reset = hold_engine = hold_flush = hold_own = hold_mdio = false;
        hold_table_clear = false;
        mdio_writes = 0;
        std::memset(phy, 0, sizeof(phy));
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
        registers_mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x40000000)),
                  registers_mapping);
        memory_mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x20000000)), 0x20000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x20000000)),
                  memory_mapping);
        regs = reinterpret_cast<uint32_t*>(UINT32_C(0x40028000));
        tx = reinterpret_cast<nx_eth_descriptor_t*>(UINT32_C(0x20000000));
        rx = reinterpret_cast<nx_eth_descriptor_t*>(UINT32_C(0x20000040));
        buffers = reinterpret_cast<uint8_t(*)[1520]>(UINT32_C(0x20001000));
        output = reinterpret_cast<uint8_t*>(UINT32_C(0x20003000));
        regions[0] = {UINT32_C(0x20000000), 0x20000, 3};
#if defined(NX_ETH_TEST_STM32)
        RCC->AHB1ENR |= UINT32_C(0x0e000000);
        SYSCFG->PMC |= UINT32_C(0x00800000);
#else
        RCU_AHB1EN |= UINT32_C(0x0e000000);
        SYSCFG_CFG1 |= UINT32_C(0x00800000);
#endif
#endif
        mac.registers = reinterpret_cast<uintptr_t>(regs);
        mac.regions = regions;
        mac.region_count = 1;
        mac.tx_descriptors = tx;
        mac.rx_descriptors = rx;
        mac.tx_slots = txslots;
        mac.rx_slots = rxslots;
        mac.tx_count = mac.rx_count = 2;
        mac.drain_timeout_us = 1000;
#if defined(NX_ETH_TEST_GD32)
        mac.hclk_hz = 200000000;
#else
        mac.hclk_hz = 168000000;
#endif
        mac.address[0] = 2;
        mac.address[5] = 1;
#if defined(NX_ETH_TEST_GD32)
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 91, 4, 0};
#else
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 82, 4, 0};
#endif
        mac.source = {61, 5, 0};
        rxslots[0].data = buffers[0];
        rxslots[1].data = buffers[1];
        rxslots[0].capacity = rxslots[1].capacity = 1520;
        output[12] = 0x08;
        nx_request_initialize(&request.base);
    }
    nx_result_t Initialize(nx_time_us_t deadline = 10000) {
#if defined(NX_ETH_TEST_STM32)
        return nx_stm32_eth_initialize(&mac, deadline);
#elif defined(NX_ETH_TEST_GD32)
        return nx_gd32_eth_initialize(&mac, deadline);
#else
        return nx_eth_mac_initialize(&mac, deadline);
#endif
    }
    void TearDown() override {
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
        if (memory_mapping != nullptr && memory_mapping != MAP_FAILED)
            munmap(memory_mapping, 0x20000);
        if (registers_mapping != nullptr && registers_mapping != MAP_FAILED)
            munmap(registers_mapping, 0x80000);
#endif
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, Initialize());
        nx_eth_link_t link{true, true, 100};
        ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &link));
        ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&request, output, 64, 10000));
    }
    void Receive(unsigned index, unsigned length = 68, uint32_t flags = 0x300) {
        std::memset(buffers[index], 0x22, 64);
        buffers[index][12] = 8;
        buffers[index][13] = 0;
        rx[index].status = (length << 16U) | flags;
        regs[dma_status / 4] |= 0x40;
        nx_eth_mac_irq(&mac);
    }
};
} /* namespace */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t nx_eth_model_read(uintptr_t base, unsigned offset) {
    return reinterpret_cast<uint32_t*>(base)[offset / 4];
}
extern "C" void nx_eth_model_write(uintptr_t base, unsigned offset,
                                   uint32_t value) {
    auto* regs = reinterpret_cast<uint32_t*>(base);
    if (hold_table_clear && (offset == 0x100c || offset == 0x1010) &&
        value == 0U)
        return;
    if (offset == dma_status) {
        regs[offset / 4] &= ~value;
        return;
    }
    if (offset == ctl && hold_engine) {
        value |= regs[offset / 4] & 0x2002;
    }
    regs[offset / 4] = value;
}
extern "C" void nx_eth_model_descriptor(void* descriptor, uint32_t value) {
    auto* d = static_cast<nx_eth_descriptor_t*>(descriptor);
    if (hold_own && (d->status & own) != 0 && (value & own) == 0)
        return;
    d->status = value;
}
extern "C" void nx_eth_model_poll(void* context) {
    auto* mac = static_cast<nx_eth_mac_t*>(context);
    auto* regs = reinterpret_cast<uint32_t*>(mac->registers);
    ++now;
    if (!hold_reset)
        regs[0x1000 / 4] &= ~1U;
    if (!hold_engine)
        regs[dma_status / 4] &= ~UINT32_C(0x7e0000);
    if (!hold_flush)
        regs[ctl / 4] &= ~UINT32_C(0x100000);
    if (!hold_mdio && (regs[0x10 / 4] & 1U)) {
        unsigned reg = (regs[0x10 / 4] >> 6U) & 31U;
        if (regs[0x10 / 4] & 2U) {
            phy[reg] = static_cast<uint16_t>(regs[0x14 / 4]);
            ++mdio_writes;
        } else
            regs[0x14 / 4] = phy[reg];
        regs[0x10 / 4] &= ~1U;
    }
}
TEST_F(EthernetMac, OwnHandoffRetainsBorrowUntilRealTransmitStatus) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    EXPECT_EQ(own, tx[0].status & own);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(output) & UINT32_MAX, tx[0].buffer);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    regs[dma_status / 4] |= 1;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    tx[0].status &= ~own;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, txslots[0].request);
    EXPECT_EQ(64U, request.base.transferred);
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(EthernetMac, HeldReceiveLoanIsNeverOverwrittenAtRingWrap) {
    Start();
    Receive(0);
    nx_eth_rx_frame_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &first));
    EXPECT_EQ(64U, first.length);
    Receive(1);
    EXPECT_EQ(0U, rx[0].status & own);
    EXPECT_EQ(0x22, first.data[0]);
    nx_eth_rx_frame_t second{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &second));
    EXPECT_EQ(NX_ERROR_EMPTY, nx_eth_port_acquire(&port, &second));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &first));
    EXPECT_EQ(own, rx[0].status & own);
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_release(&port, &first));
}
TEST_F(EthernetMac, CancellationWaitsForActualEngineAndFifoDrain) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    hold_engine = true;
    regs[dma_status / 4] |= 1U << 20U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_cancel(&port, &request));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    now += 1001;
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, txslots[0].request);
    hold_engine = false;
    regs[ctl / 4] &= ~0x2000U;
    regs[dma_status / 4] &= ~(7U << 20U);
    nx_eth_model_poll(&mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    nx_eth_model_poll(&mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(EthernetMac, InvalidDomainRejectsBeforeResetOrOwnPublication) {
    regions[0].permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(NX_ERROR_PERMISSION, Initialize());
    EXPECT_EQ(0U, regs[0x1000 / 4]);
    EXPECT_EQ(0U, rx[0].status);
}
TEST_F(EthernetMac, MdioTimeoutDoesNotInventReadValueOrClearBusy) {
    Start();
    hold_mdio = true;
    uint16_t value = 42;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_mdio_read(&mdio, 1, 2, &value, now + 5));
    EXPECT_EQ(42, value);
    EXPECT_EQ(1U, regs[0x10 / 4] & 1U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_mdio_write(&mdio, 1, 0, 0, now + 5));
    EXPECT_EQ(0U, mdio_writes);
}
TEST_F(EthernetMac, ReceiveOutputCannotAliasDmaBufferOrOtherLoans) {
    Start();
    Receive(0);
    auto* alias = reinterpret_cast<nx_eth_rx_frame_t*>(buffers[1]);
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_acquire(&port, alias));
    nx_eth_rx_frame_t frame{};
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &frame));
    EXPECT_EQ(buffers[0], frame.data);
}
TEST_F(EthernetMac, StateAndMdioOutputCannotOverwriteProviderStorage) {
    Start();
    auto* state_alias = reinterpret_cast<nx_eth_state_t*>(buffers[1]);
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_state(&port, state_alias));
    phy[2] = 0x2000;
    auto* mdio_alias = reinterpret_cast<uint16_t*>(buffers[1]);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_mdio_read(&mdio, 1, 2, mdio_alias, now + 10));
    EXPECT_EQ(0U, regs[0x10 / 4] & 1U);
}
TEST_F(EthernetMac, LateFatalIrqClosesAdmissionBeforeNewOwnPublication) {
    Start();
    regs[dma_status / 4] |= 0x2000;
    nx_eth_mac_irq(&mac);
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, tx[0].status & own);
}
TEST_F(EthernetMac, InitializationCannotResetForeignDescriptorEngine) {
    regs[0x100c / 4] = 0x20000000;
    EXPECT_EQ(NX_ERROR_BUSY, Initialize());
    EXPECT_EQ(0U, regs[0x1000 / 4]);
    EXPECT_EQ(0U, rx[0].status);
}
TEST_F(EthernetMac, StopDoesNotReclaimStuckOwnOrHeldLoan) {
    Start();
    Receive(0);
    nx_eth_rx_frame_t frame{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &frame));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_eth_model_poll(&mac);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    EXPECT_EQ(0x22, frame.data[0]);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &frame));
    hold_own = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    EXPECT_TRUE(mac.retained);
    hold_own = false;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
    EXPECT_EQ(0U, regs[0x100c / 4]);
    EXPECT_EQ(0U, regs[0x1010 / 4]);
}
TEST_F(EthernetMac, CapturedWakeRemainsBorrowedDuringItsCallback) {
    Start();
    struct Hint {
        const nx_eth_port_t* port;
        nx_result_t detached = NX_SUCCESS;
        nx_result_t stopped = NX_SUCCESS;
    } hint{&port};
    const nx_irq_wake_t wake{&hint,
                             [](void* p) {
                                 auto* h = static_cast<Hint*>(p);
                                 h->detached = nx_eth_port_attach_wake(
                                     h->port, nullptr, 0);
                                 h->stopped = nx_eth_port_stop(h->port);
                                 return NX_SUCCESS;
                             },
                             false};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_attach_wake(&port, &wake, 0));
    nx_eth_mac_irq(&mac);
    EXPECT_EQ(NX_ERROR_BUSY, hint.detached);
    EXPECT_EQ(NX_ERROR_BUSY, hint.stopped);
    EXPECT_EQ(&wake, mac.wake);
    nx_eth_model_poll(&mac);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
    EXPECT_EQ(nullptr, mac.wake);
}

#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
extern "C" nx_result_t nx_eth_model_irq_control(nx_eth_mac_t*, bool) {
    return NX_SUCCESS;
}
TEST_F(EthernetMac, SocRejectsUnreviewedCcmAndWrongClockBeforeReset) {
    regions[0].begin = UINT32_C(0x10000000);
    EXPECT_EQ(NX_ERROR_PERMISSION, Initialize());
    EXPECT_EQ(0U, regs[0x1000 / 4]);
    regions[0].begin = UINT32_C(0x20000000);
    mac.hclk_hz++;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, Initialize());
    EXPECT_EQ(0U, regs[0x1000 / 4]);
}
#endif
TEST_F(EthernetMac, CrcLengthFragmentAndVlanAreDiscardedWithoutBorrow) {
    Start();
    Receive(0, 68, 0x8302);
    EXPECT_EQ(own, rx[0].status & own);
    Receive(1, 1519, 0x300);
    Receive(0, 68, 0x100);
    Receive(1, 68, 0x700);
    nx_eth_rx_frame_t frame{};
    EXPECT_EQ(NX_ERROR_EMPTY, nx_eth_port_acquire(&port, &frame));
    nx_eth_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_state(&port, &state));
    EXPECT_EQ(4U, state.discarded);
    EXPECT_EQ(NX_ETH_RX_CRC | NX_ETH_RX_LENGTH | NX_ETH_RX_FRAGMENT |
                  NX_ETH_RX_UNSUPPORTED,
              state.receive_flags);
}
TEST_F(EthernetMac, RingFullRejectsWithoutChangingRejectedRequest) {
    Start();
    nx_eth_tx_request_t second{}, third{};
    nx_request_initialize(&second.base);
    nx_request_initialize(&third.base);
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&second, output, 64, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&third, output, 64, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &second));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_submit(&port, &third));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&third.base));
    EXPECT_EQ(2U, mac.snapshot.pending_tx);
}
TEST_F(EthernetMac, UnalignedPayloadRejectsWithoutDescriptorEffects) {
    Start();
    output[13] = 8;
    output[14] = 0;
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&request, output + 1, 63, 10000));
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, tx[0].buffer);
}
TEST_F(EthernetMac, FatalBusErrorCannotTurnMissingStatusIntoSuccess) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    regs[dma_status / 4] |= 0x2000;
    nx_eth_mac_irq(&mac);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_eth_model_poll(&mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_TRUE(mac.snapshot.fatal_bus_error);
}
TEST_F(EthernetMac, UnderflowIsLatchedAndDrainedWithoutReplay) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    regs[dma_status / 4] |= 0x20;
    nx_eth_mac_irq(&mac);
    nx_eth_model_poll(&mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, tx[0].buffer);
}
TEST_F(EthernetMac, CancelPreservesAlreadyReturnedWireSuccess) {
    Start();
    nx_eth_tx_request_t second{};
    nx_request_initialize(&second.base);
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&second, output, 64, 10000));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &second));
    tx[1].status &= ~own;
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_cancel(&port, &request));
    nx_eth_model_poll(&mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&second.base));
    EXPECT_EQ(NX_SUCCESS, second.base.result);
}
TEST_F(EthernetMac, FrameMetadataMustRemainValidForEveryOutstandingBorrow) {
    Start();
    alignas(4) uint8_t alternate[64]{};
    alternate[12] = 8;
    auto* overlapping = reinterpret_cast<nx_eth_tx_request_t*>(output);
    nx_request_initialize(&overlapping->base);
    overlapping->data = alternate;
    overlapping->length = 64;
    overlapping->base.deadline = 10000;
    /* Create both objects before the first borrow. Admitting the second */
    /* request would mutate bytes that the first descriptor can still read. */
    output[12] = 8;
    output[13] = 0;
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_submit(&port, overlapping));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&overlapping->base));
}
TEST_F(EthernetMac, CoalescedCompletionsRependAfterConstantIrqBudget) {
    Start();
    unsigned pending = 0;
    static unsigned* captured = nullptr;
    captured = &pending;
    mac.pend_irq = [](nx_eth_mac_t*) { ++*captured; };
    for (unsigned i = 0; i < 2; ++i) {
        buffers[i][12] = 8;
        rx[i].status = (68U << 16U) | 0x300U;
    }
    nx_eth_mac_irq(&mac);
    EXPECT_EQ(1U, pending);
    EXPECT_EQ(NX_ETH_RX_READY, rxslots[0].state);
    EXPECT_EQ(NX_ETH_RX_DMA, rxslots[1].state);
    nx_eth_mac_irq(&mac);
    EXPECT_EQ(1U, pending);
    EXPECT_EQ(NX_ETH_RX_READY, rxslots[1].state);
}
TEST_F(EthernetMac, WakeBindingUsesActualPriorityAfterExternalChange) {
    mac.irq_policy.kernel = NX_IRQ_KERNEL_BASEPRI;
    mac.irq_policy.syscall_ceiling = 5;
    mac.source.priority_group = 3;
    Start();
    mac.validate_wake = [](nx_eth_mac_t* m, const nx_irq_wake_t* w, uint8_t) {
        const nx_irq_source_t actual{61, 2, 3};
        return nx_irq_wake_validate(w, &m->irq_policy, &actual);
    };
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, true};
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_eth_port_attach_wake(&port, &wake, 5));
    EXPECT_EQ(nullptr, mac.wake);
}

#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
extern "C" void nx_eth_model_pend_irq(nx_eth_mac_t*) {
}
extern "C" nx_result_t
nx_eth_model_validate_wake(nx_eth_mac_t* m, const nx_irq_wake_t* w, uint8_t) {
    return nx_irq_wake_validate(w, &m->irq_policy, &m->source);
}
#endif
TEST_F(EthernetMac, MdioCompletionAtDeadlineDoesNotPublishLateValue) {
    Start();
    phy[2] = 0x2000;
    uint16_t value = 42;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_mdio_read(&mdio, 1, 2, &value, now + 1));
    EXPECT_EQ(42, value);
    EXPECT_EQ(0U, regs[0x10 / 4] & 1U);
}
TEST_F(EthernetMac, ResetAcknowledgementAtDeadlineRetainsStartupOwner) {
    EXPECT_EQ(NX_ERROR_TIMEOUT, Initialize(now + 1));
    EXPECT_TRUE(mac.retained);
    EXPECT_EQ(0U, rx[0].status);
    EXPECT_FALSE(mac.snapshot.started);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_eth_model_poll(&mac);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
    EXPECT_FALSE(mac.retained);
}
TEST_F(EthernetMac,
       ReturnedDescriptorWithoutFrameStatusCannotInventWireSuccess) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    tx[0].status = 0x00100000;
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(EthernetMac, ModifiedReceiveTokenCannotRearmOriginalLoan) {
    Start();
    Receive(0);
    nx_eth_rx_frame_t frame{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &frame));
    auto modified = frame;
    modified.timestamp_us++;
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_release(&port, &modified));
    EXPECT_EQ(0U, rx[0].status & own);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &frame));
}
TEST_F(EthernetMac, ReceiveTokenBindsExactProviderIdentity) {
    Start();
    Receive(0);
    nx_eth_rx_frame_t frame{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &frame));
    EXPECT_EQ(&mac, frame.identity);
    auto foreign = frame;
    foreign.identity = &request;
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_release(&port, &foreign));
    EXPECT_EQ(0U, rx[0].status & own);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &frame));
}

TEST_F(EthernetMac, LinkChangeRequiresBothDirectionsQuiet) {
    Start();
    regs[dma_status / 4] = 3U << 17U;
    const auto original_control = regs[0];
    const auto original_dma = regs[ctl / 4];
    nx_eth_link_t changed{true, false, 10};
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_apply_link(&port, &changed));
    EXPECT_EQ(original_control, regs[0]);
    EXPECT_EQ(original_dma, regs[ctl / 4]);
    EXPECT_EQ(100U, mac.snapshot.link.speed_mbps);
    EXPECT_TRUE(mac.snapshot.link.full_duplex);
}
TEST_F(EthernetMac, SameObservedLinkDoesNotRewriteActiveEngine) {
    Start();
    regs[dma_status / 4] = (6U << 20U) | (3U << 17U);
    const auto original_dma = regs[ctl / 4];
    nx_eth_link_t unchanged{true, true, 100};
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &unchanged));
    EXPECT_EQ(original_dma, regs[ctl / 4]);
}
TEST_F(EthernetMac, LinkDownMustDrainRxBeforeReconfiguration) {
    Start();
    regs[dma_status / 4] = 3U << 17U;
    nx_eth_link_t down{false, false, 0};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &down));
    const auto stopped_control = regs[0];
    nx_eth_link_t changed{true, false, 10};
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_apply_link(&port, &changed));
    EXPECT_EQ(stopped_control, regs[0]);
    regs[dma_status / 4] = 0U;
    regs[0x34 / 4] = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_apply_link(&port, &changed));
    regs[0x34 / 4] = 0U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_apply_link(&port, &changed));
    nx_eth_model_poll(&mac);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_service(&port));
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &changed));
    EXPECT_EQ(10U, mac.snapshot.link.speed_mbps);
    EXPECT_FALSE(mac.snapshot.link.full_duplex);
}
TEST_F(EthernetMac, WakeTargetCannotResideInDmaReceiveStorage) {
    Start();
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    std::memcpy(buffers[0], &wake, sizeof(wake));
    auto* in_dma = reinterpret_cast<nx_irq_wake_t*>(buffers[0]);
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_attach_wake(&port, in_dma, 0));
    EXPECT_EQ(nullptr, mac.wake);
}
TEST_F(EthernetMac, KnownWakeStorageCannotBeUsedAsMutableOutput) {
    Start();
    alignas(nx_eth_state_t) uint8_t storage[sizeof(nx_eth_state_t) + 32]{};
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, false};
    std::memcpy(storage, &wake, sizeof(wake));
    auto* target = reinterpret_cast<nx_irq_wake_t*>(storage);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_attach_wake(&port, target, 0));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_mdio_read(&mdio, 1, 2, reinterpret_cast<uint16_t*>(storage),
                           now + 100));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_eth_port_state(&port, reinterpret_cast<nx_eth_state_t*>(storage)));
    EXPECT_EQ(0, std::memcmp(storage, &wake, sizeof(wake)));
}
TEST_F(EthernetMac, RequestMetadataCannotOverwriteBorrowedWakeTarget) {
    Start();
    alignas(nx_eth_tx_request_t)
        uint8_t storage[sizeof(nx_eth_tx_request_t) + sizeof(nx_irq_wake_t)]{};
    auto* aliased = reinterpret_cast<nx_eth_tx_request_t*>(storage);
    nx_request_initialize(&aliased->base);
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(aliased, output, 64, 10000));
    const nx_irq_wake_t wake{reinterpret_cast<void*>(64),
                             [](void*) { return NX_SUCCESS; }, false};
    auto* target = reinterpret_cast<nx_irq_wake_t*>(
        storage + offsetof(nx_eth_tx_request_t, length));
    std::memcpy(target, &wake, sizeof(wake));
    ASSERT_EQ(64U, aliased->length);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_attach_wake(&port, target, 0));
    EXPECT_EQ(NX_ERROR_INVALID, nx_eth_port_submit(&port, aliased));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&aliased->base));
    EXPECT_EQ(nullptr, txslots[0].request);
    EXPECT_EQ(0U, tx[0].status & own);
}
namespace {
struct EthernetPendingIrq {
    std::mutex mutex;
    std::condition_variable condition;
    std::thread thread;
    bool entered = false;
    bool done = false;
    static nx_result_t Notify(void* context) {
        auto* gate = static_cast<EthernetPendingIrq*>(context);
        std::unique_lock<std::mutex> lock(gate->mutex);
        gate->entered = true;
        gate->condition.notify_all();
        gate->condition.wait(lock, [gate] { return gate->done; });
        return NX_SUCCESS;
    }
    void Begin(nx_eth_mac_t* mac) {
        thread = std::thread([mac] { nx_eth_mac_irq(mac); });
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { return entered; });
    }
    void Join() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            done = true;
        }
        condition.notify_all();
        thread.join();
    }
};
} /* namespace */
TEST_F(EthernetMac, WakeReplacementRechecksPublisherAfterColdValidation) {
    Start();
    EthernetPendingIrq gate;
    const nx_irq_wake_t old_wake{&gate, EthernetPendingIrq::Notify, false};
    const nx_irq_wake_t next_wake{nullptr, [](void*) { return NX_SUCCESS; },
                                  false};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_attach_wake(&port, &old_wake, 0));
    mac.validate_wake = [](nx_eth_mac_t* m, const nx_irq_wake_t*, uint8_t) {
        static_cast<EthernetPendingIrq*>(m->wake->context)->Begin(m);
        return NX_SUCCESS;
    };
    const auto replaced = nx_eth_port_attach_wake(&port, &next_wake, 0);
    EXPECT_EQ(NX_ERROR_BUSY, replaced);
    EXPECT_EQ(&old_wake, mac.wake);
    gate.Join();
}
TEST_F(EthernetMac, StopRechecksCapturedPublisherAfterNvicDisableBoundary) {
    Start();
    EthernetPendingIrq gate;
    const nx_irq_wake_t wake{&gate, EthernetPendingIrq::Notify, false};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_attach_wake(&port, &wake, 0));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_eth_model_poll(&mac);
    mac.irq_control = [](nx_eth_mac_t* m, bool enabled) {
        if (!enabled)
            static_cast<EthernetPendingIrq*>(m->wake->context)->Begin(m);
        return NX_SUCCESS;
    };
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    EXPECT_TRUE(mac.retained);
    EXPECT_EQ(&wake, mac.wake);
    gate.Join();
    mac.irq_control = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
}

TEST_F(EthernetMac, StopClosesNewReceiveLoansBeforeDrainCompletes) {
    Start();
    Receive(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_eth_rx_frame_t loan{};
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_acquire(&port, &loan));
    EXPECT_EQ(0U, mac.snapshot.held_rx);
}
TEST_F(EthernetMac, FailedTableWithdrawalCannotReborrowDiscardedQueueHead) {
    Start();
    Receive(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_eth_model_poll(&mac);
    hold_table_clear = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    EXPECT_EQ(1U, mac.snapshot.discarded);
    nx_eth_rx_frame_t loan{};
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_acquire(&port, &loan));
    EXPECT_EQ(0U, mac.snapshot.held_rx);
    hold_table_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
}
