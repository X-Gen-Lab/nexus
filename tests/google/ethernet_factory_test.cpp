/**
 * \file            ethernet_factory_test.cpp
 * \brief           Generated RMII storage, paired faces and retained cold
 * effects \author          Nexus Team \version         1.0.0 \date 2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "eth_mac.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_ETH_FACTORY_STM32
#include "stm32f407_eth.h"
#include "stm32f407_provider.h"
#else
#include "gd32f470_eth.h"
#include "gd32f470_provider.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#endif
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>
using testing::_;
using testing::NiceMock;
using testing::Return;
class EthernetAssemblyPorts {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
    MOCK_METHOD(nx_result_t, Clock, (unsigned));
    MOCK_METHOD(nx_result_t, Configure, (unsigned, unsigned));
    MOCK_METHOD(void, Restore, (unsigned, unsigned, uint16_t));
    MOCK_METHOD(nx_result_t, Interrupt, (bool));
    MOCK_METHOD(nx_result_t, Memory,
                (const nx_dma_memory_region_t*, size_t, const void*, size_t,
                 size_t, nx_dma_direction_t));
};
static EthernetAssemblyPorts* s_ports;
static std::array<std::array<uint16_t, 16>, 9> s_pins;
static nx_eth_mac_t* s_mac;
static nx_time_us_t s_now;
static bool s_hold_reset, s_hold_engine, s_hold_restore, s_hold_clock;
static bool s_hold_mux;
static uint16_t s_phy[32];
static uint32_t* registers() {
    return reinterpret_cast<uint32_t*>(s_mac->registers);
}
static volatile uint32_t& ahb() {
#ifdef NX_ETH_FACTORY_STM32
    return RCC->AHB1ENR;
#else
    return RCU_AHB1EN;
#endif
}
static volatile uint32_t& mux() {
#ifdef NX_ETH_FACTORY_STM32
    return SYSCFG->PMC;
#else
    return SYSCFG_CFG1;
#endif
}
extern "C" void nx_eth_factory_register_write(volatile uint32_t* address,
                                              uint32_t value) {
    if (s_hold_clock && address == &ahb() && (value & 0x0e000000U) == 0U)
        return;
    if (s_hold_mux && address == &mux() && (value & 0x00800000U) == 0U)
        return;
    *address = value;
}
#ifdef NX_ETH_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_nx_eth_factory_nvic;
}
/** \brief A separately tested system supplies this cold admission result. */
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return s_ports->Preflight();
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {s_ports->Start(), 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return s_ports->Stop();
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned index) {
    nx_result_t result = s_ports->Clock(index);
    if (result == NX_SUCCESS)
        ahb() |= 1U << index;
    return result;
}
static unsigned port_index(const GPIO_TypeDef* port) {
    return static_cast<unsigned>(
        (reinterpret_cast<uintptr_t>(port) - GPIOA_BASE) / 0x400U);
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* port,
                                         uint8_t pin) {
    return s_pins[port_index(port)][pin];
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* port, uint8_t pin,
                                     uint16_t state) {
    s_ports->Restore(port_index(port), pin, state);
    if (!s_hold_restore)
        s_pins[port_index(port)][pin] = state;
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* port, uint8_t pin,
                                              uint8_t mode, uint8_t af,
                                              uint8_t pull, bool drain) {
    if (mode != 2U || af != 11U || pull != 0U || drain)
        return NX_ERROR_INVALID;
    nx_result_t result = s_ports->Configure(port_index(port), pin);
    if (result == NX_SUCCESS)
        s_pins[port_index(port)][pin] = 0x456U;
    return result;
}
#else
extern "C" {
NVIC_Type g_gd32_model_nvic;
}
/** \brief Model only the external SoC's read-only preflight port. */
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    return s_ports->Preflight();
}
extern "C" int nx_gd32_soc_start(void) {
    return s_ports->Start();
}
extern "C" int nx_gd32_soc_stop(void) {
    return s_ports->Stop();
}
extern "C" nx_result_t nx_gd32_gpio_clock_enable(unsigned index) {
    nx_result_t result = s_ports->Clock(index);
    if (result == NX_SUCCESS)
        ahb() |= 1U << index;
    return result;
}
static unsigned port_index(uint32_t port) {
    return (port - GPIOA) / 0x400U;
}
extern "C" uint16_t nx_gd32_pin_capture(uint32_t port, unsigned pin) {
    return s_pins[port_index(port)][pin];
}
extern "C" void nx_gd32_pin_restore(uint32_t port, unsigned pin,
                                    uint16_t state) {
    s_ports->Restore(port_index(port), pin, state);
    if (!s_hold_restore)
        s_pins[port_index(port)][pin] = state;
}
extern "C" nx_result_t nx_gd32_pin_configure(uint32_t port, unsigned pin,
                                             uint8_t mode, uint8_t af,
                                             uint8_t pull, bool drain) {
    if (mode != 2U || af != 11U || pull != 0U || drain)
        return NX_ERROR_INVALID;
    nx_result_t result = s_ports->Configure(port_index(port), pin);
    if (result == NX_SUCCESS)
        s_pins[port_index(port)][pin] = 0x456U;
    return result;
}
#endif
extern "C" nx_time_us_t nx_time_now_us(void) {
    return s_now;
}
extern "C" nx_result_t
nx_dma_buffer_validate(const nx_dma_memory_region_t* regions, size_t count,
                       const void* data, size_t length, size_t alignment,
                       nx_dma_direction_t direction) {
    return s_ports->Memory(regions, count, data, length, alignment, direction);
}
extern "C" uint32_t nx_eth_model_read(uintptr_t base, unsigned offset) {
    return reinterpret_cast<uint32_t*>(base)[offset / 4U];
}
extern "C" void nx_eth_model_write(uintptr_t base, unsigned offset,
                                   uint32_t value) {
    auto* regs = reinterpret_cast<uint32_t*>(base);
    if (offset == 0x1014U)
        regs[offset / 4U] &= ~value;
    else {
        if (s_hold_engine && offset == 0x1018U)
            value |= regs[offset / 4U] & 0x2002U;
        regs[offset / 4U] = value;
    }
}
extern "C" void nx_eth_model_poll(void* context) {
    auto* mac = static_cast<nx_eth_mac_t*>(context);
    auto* regs = reinterpret_cast<uint32_t*>(mac->registers);
    ++s_now;
    if (!s_hold_reset)
        regs[0x1000U / 4U] &= ~1U;
    if (!s_hold_engine)
        regs[0x1014U / 4U] &= ~0x007e0000U;
    regs[0x1018U / 4U] &= ~0x00100000U;
    if (regs[0x10U / 4U] & 1U) {
        unsigned index = (regs[0x10U / 4U] >> 6U) & 31U;
        if (regs[0x10U / 4U] & 2U)
            s_phy[index] = static_cast<uint16_t>(regs[0x14U / 4U]);
        else
            regs[0x14U / 4U] = s_phy[index];
        regs[0x10U / 4U] &= ~1U;
    }
}
extern "C" void nx_eth_model_descriptor(void* descriptor, uint32_t value) {
    static_cast<nx_eth_descriptor_t*>(descriptor)->status = value;
}
extern "C" nx_result_t nx_eth_model_irq_control(nx_eth_mac_t*, bool enabled) {
    return s_ports->Interrupt(enabled);
}
extern "C" nx_result_t nx_eth_model_validate_wake(nx_eth_mac_t* mac,
                                                  const nx_irq_wake_t* wake,
                                                  uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    return nx_irq_wake_validate(wake, &mac->irq_policy, &mac->source);
}
extern "C" void nx_eth_model_pend_irq(nx_eth_mac_t*) {
}
class EthernetFactory : public testing::Test {
  protected:
    NiceMock<EthernetAssemblyPorts> ports;
    void* mapping = nullptr;
    const nx_eth_port_t* face = nullptr;
    alignas(4) uint8_t output[64]{};
    nx_eth_tx_request_t request{};
    static bool span(const void* data, size_t length, const void* begin,
                     size_t size) {
        uintptr_t pointer = reinterpret_cast<uintptr_t>(data);
        uintptr_t first = reinterpret_cast<uintptr_t>(begin);
        return pointer >= first && length <= size &&
               pointer - first <= size - length;
    }
    nx_result_t Memory(const nx_dma_memory_region_t* regions, size_t count,
                       const void* data, size_t length, size_t alignment,
                       nx_dma_direction_t direction) {
        const size_t reviewed =
#ifdef NX_ETH_FACTORY_STM32
            131072U;
#else
            196608U;
#endif
        if (count != 1U || regions[0].begin != 0x20000000U ||
            regions[0].size != reviewed || regions[0].permissions != 3U ||
            alignment != 4U || length == 0U ||
            reinterpret_cast<uintptr_t>(data) % alignment != 0U ||
            (direction != NX_DMA_TO_DEVICE && direction != NX_DMA_FROM_DEVICE))
            return NX_ERROR_PERMISSION;
        bool valid = span(data, length, s_mac->tx_descriptors,
                          s_mac->tx_count * sizeof(*s_mac->tx_descriptors)) ||
                     span(data, length, s_mac->rx_descriptors,
                          s_mac->rx_count * sizeof(*s_mac->rx_descriptors));
        for (size_t i = 0; i < s_mac->rx_count; ++i)
            valid = valid || span(data, length, s_mac->rx_slots[i].data,
                                  s_mac->rx_slots[i].capacity);
        valid = valid || (direction == NX_DMA_TO_DEVICE &&
                          span(data, length, output, sizeof(output)));
        return valid ? NX_SUCCESS : NX_ERROR_PERMISSION;
    }
    void SetUp() override {
        s_ports = &ports;
        ON_CALL(ports, Preflight()).WillByDefault(Return(NX_SUCCESS));
        std::memset(NVIC, 0, sizeof(*NVIC));
        s_now = 100U;
        s_hold_reset = s_hold_engine = s_hold_restore = false;
        s_hold_clock = s_hold_mux = false;
        for (auto& port : s_pins)
            port.fill(0x123U);
        std::memset(s_phy, 0, sizeof(s_phy));
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x40000000)), mapping);
        ON_CALL(ports, Start()).WillByDefault(Return(0));
        ON_CALL(ports, Stop()).WillByDefault(Return(0));
        ON_CALL(ports, Clock(_)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Configure(_, _)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Interrupt(_)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Memory(_, _, _, _, _, _))
            .WillByDefault([this](const nx_dma_memory_region_t* regions,
                                  size_t count, const void* data, size_t length,
                                  size_t alignment,
                                  nx_dma_direction_t direction) {
                return Memory(regions, count, data, length, alignment,
                              direction);
            });
        face = nx_factory_eth(NX_ETH_ID_MAC);
        ASSERT_NE(nullptr, face);
        s_mac = static_cast<nx_eth_mac_t*>(face->context);
        output[12] = 8U;
        nx_request_initialize(&request.base);
    }
    nx_result_t Stop() {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned attempt = 0; attempt < 4U; ++attempt) {
            result = nx_platform_stop();
            if (result != NX_ERROR_BUSY)
                break;
            if (s_mac->registers != 0U)
                nx_eth_model_poll(s_mac);
        }
        return result;
    }
    void TearDown() override {
        s_hold_reset = s_hold_engine = s_hold_restore = false;
        s_hold_clock = s_hold_mux = false;
        if (s_mac->registers != 0U)
            nx_eth_model_poll(s_mac);
        (void)Stop();
        if (mapping != nullptr && mapping != MAP_FAILED)
            munmap(mapping, 0x80000);
        s_ports = nullptr;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    }
    void Receive(nx_eth_rx_frame_t* frame) {
        nx_eth_link_t link{true, true, 100U};
        ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(face, &link));
        std::memset(s_mac->rx_slots[0].data, 0x22, s_mac->rx_slots[0].capacity);
        s_mac->rx_slots[0].data[12] = 8U;
        s_mac->rx_slots[0].data[13] = 0U;
        s_mac->rx_descriptors[0].status = (68U << 16U) | 0x300U;
        registers()[0x1014U / 4U] |= 0x40U;
        nx_eth_mac_irq(s_mac);
        ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(face, frame));
    }
};
TEST_F(EthernetFactory, PairedTypedGettersArePureAndShareOneMacIdentity) {
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    const nx_mdio_port_t* mdio = nx_factory_mdio(NX_MDIO_ID_MAC);
    ASSERT_NE(nullptr, mdio);
    EXPECT_EQ(face->context, mdio->context);
    EXPECT_EQ(&nx_eth_mac_ops, face->ops);
    EXPECT_EQ(&nx_eth_mdio_ops, mdio->ops);
    EXPECT_EQ(nullptr, nx_factory_eth(static_cast<nx_eth_id_t>(255)));
    EXPECT_EQ(nullptr, nx_factory_mdio(static_cast<nx_mdio_id_t>(255)));
}
TEST_F(EthernetFactory, ExactChosenStorageReviewedPadsAndNoGuessedPhyLink) {
    EXPECT_CALL(ports, Configure(0U, 1U));
    EXPECT_CALL(ports, Configure(0U, 2U));
    EXPECT_CALL(ports, Configure(0U, 7U));
    EXPECT_CALL(ports, Configure(2U, 1U));
    EXPECT_CALL(ports, Configure(2U, 4U));
    EXPECT_CALL(ports, Configure(2U, 5U));
    EXPECT_CALL(ports, Configure(6U, 11U));
    EXPECT_CALL(ports, Configure(6U, 13U));
    EXPECT_CALL(ports, Configure(6U, 14U));
    Start();
    EXPECT_EQ(2U, s_mac->tx_count);
    EXPECT_EQ(2U, s_mac->rx_count);
    EXPECT_EQ(1536U, s_mac->rx_slots[0].capacity);
    EXPECT_NE(s_mac->rx_slots[0].data, s_mac->rx_slots[1].data);
    EXPECT_FALSE(s_mac->snapshot.link.up);
    EXPECT_EQ(NX_IRQ_KERNEL_NONE, s_mac->irq_policy.kernel);
    EXPECT_EQ(61, s_mac->source.irq_number);
    EXPECT_EQ(0x0e000000U, ahb() & 0x0e000000U);
}
TEST_F(EthernetFactory, PairedCapabilitiesDescribeBusMasterAndPollingMdio) {
    const nx_factory_io_capability_t* eth =
        nx_factory_eth_capability(NX_ETH_ID_MAC);
    const nx_factory_io_capability_t* mdio =
        nx_factory_mdio_capability(NX_MDIO_ID_MAC);
    ASSERT_NE(nullptr, eth);
    ASSERT_NE(nullptr, mdio);
    EXPECT_EQ(NX_FACTORY_CAP_IRQ | NX_FACTORY_CAP_BUS_MASTER_DMA,
              eth->features);
    EXPECT_EQ(NX_ETH_FRAME_MAX, eth->max_transfer_units);
    EXPECT_EQ(4U, eth->buffer_alignment);
    EXPECT_EQ(NX_FACTORY_CAP_POLLING, mdio->features);
}
TEST_F(EthernetFactory,
       BasePreflightRejectsBeforeClockPinAndControllerEffects) {
    const auto* bytes = static_cast<const uint8_t*>(mapping);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Clock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_CALL(ports, Interrupt(_)).Times(0);
    EXPECT_CALL(ports, Memory(_, _, _, _, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(EthernetFactory, RefusedMacClockWithdrawalRetainsPlatformUntilReadback) {
    Start();
    s_hold_clock = true;
    EXPECT_EQ(NX_ERROR_IO, Stop());
    EXPECT_EQ(0x0e000000U, ahb() & 0x0e000000U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    s_hold_clock = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, ahb() & 0x0e000000U);
    EXPECT_EQ(NX_SUCCESS, nx_platform_start().primary);
}
TEST_F(EthernetFactory,
       RefusedRmiiSelectorRestoreRetainsLeaseAndBlocksRestart) {
    Start();
    s_hold_mux = true;
    EXPECT_EQ(NX_ERROR_IO, Stop());
    EXPECT_EQ(0x00800000U, mux() & 0x00800000U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    s_hold_mux = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, mux() & 0x00800000U);
}
TEST_F(EthernetFactory, ForeignMacClockRejectsWithoutAnyEffects) {
    ahb() |= 0x02000000U;
    mux() |= 0x00800000U;
    const auto* bytes = static_cast<const uint8_t*>(mapping);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, Clock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_CALL(ports, Interrupt(_)).Times(0);
    EXPECT_CALL(ports, Memory(_, _, _, _, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    EXPECT_EQ(0x02000000U, ahb() & 0x0e000000U);
    EXPECT_EQ(0x00800000U, mux() & 0x00800000U);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(EthernetFactory, ExistingRmiiSelectionSurvivesOwnedShutdown) {
    mux() |= 0x00800000U;
    Start();
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, ahb() & 0x0e000000U);
    EXPECT_EQ(0x00800000U, mux() & 0x00800000U);
}
TEST_F(EthernetFactory, FailedPartialPinConfigurationRestoresAcquiredPads) {
    EXPECT_CALL(ports, Configure(_, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Configure(2U, 4U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(ports, Restore(2U, 4U, 0x123U));
    EXPECT_CALL(ports, Restore(2U, 1U, 0x123U));
    EXPECT_CALL(ports, Restore(0U, 7U, 0x123U));
    EXPECT_CALL(ports, Restore(0U, 2U, 0x123U));
    EXPECT_CALL(ports, Restore(0U, 1U, 0x123U));
    EXPECT_EQ(NX_ERROR_IO, nx_platform_start().primary);
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(EthernetFactory, FailedPinRestoreKeepsClocksAndStorageUntilRetry) {
    Start();
    s_hold_restore = true;
    EXPECT_EQ(NX_ERROR_IO, Stop());
    EXPECT_EQ(0x0e000000U, ahb() & 0x0e000000U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    s_hold_restore = false;
    EXPECT_EQ(NX_SUCCESS, Stop());
}
TEST_F(EthernetFactory, ActualResetFailureIsCountedBeforeColdCallAndDrained) {
    s_hold_reset = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_platform_start().primary);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    s_hold_reset = false;
    nx_eth_model_poll(s_mac);
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(NX_SUCCESS, nx_platform_start().primary);
}
TEST_F(EthernetFactory,
       HeldReceiveLoanPreventsDestructiveStopAndStaleRestartToken) {
    Start();
    nx_eth_rx_frame_t first{};
    Receive(&first);
    EXPECT_EQ(NX_ERROR_BUSY, Stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(face, &first));
    EXPECT_EQ(NX_SUCCESS, Stop());
    Start();
    nx_eth_rx_frame_t second{};
    Receive(&second);
    EXPECT_GT(second.epoch, first.epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_release(face, &first));
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(face, &second));
}
TEST_F(EthernetFactory,
       FiniteMdioUsesPairedFaceWithoutInventingPhyNegotiation) {
    Start();
    s_phy[2] = 0x2000U;
    uint16_t value = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_mdio_read(nx_factory_mdio(NX_MDIO_ID_MAC), 1U, 2U,
                                       &value, s_now + 100U));
    EXPECT_EQ(0x2000U, value);
    EXPECT_FALSE(s_mac->snapshot.link.up);
}
TEST_F(EthernetFactory, IrqWithdrawalFailureRetainsPinsClocksAndRestartGate) {
    Start();
    EXPECT_CALL(ports, Interrupt(false)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, Stop());
    EXPECT_EQ(0x0e000000U, ahb() & 0x0e000000U);
    EXPECT_EQ(0x456U, s_pins[6][14]);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    testing::Mock::VerifyAndClearExpectations(&ports);
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0x123U, s_pins[6][14]);
}
TEST_F(EthernetFactory, RejectedDmaDomainDoesNotResetMacOrRetainPayload) {
    EXPECT_CALL(ports, Memory(_, _, _, _, _, _))
        .WillRepeatedly(Return(NX_ERROR_PERMISSION));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_platform_start().primary);
    EXPECT_EQ(0U, registers()[0x1000U / 4U]);
    EXPECT_FALSE(s_mac->retained);
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, ahb() & 0x0e000000U);
}
TEST_F(EthernetFactory, ExhaustedReceiveEpochCannotBeResetByReconstruction) {
    Start();
    EXPECT_EQ(NX_SUCCESS, Stop());
    uint64_t prior = s_mac->epoch;
    s_mac->epoch = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_INVALID, nx_platform_start().primary);
    EXPECT_EQ(UINT64_MAX, s_mac->epoch);
    EXPECT_EQ(NX_SUCCESS, Stop());
    s_mac->epoch = prior;
}
TEST_F(EthernetFactory, StuckTxEngineRetainsBorrowedStorageUntilRealDrain) {
    Start();
    nx_eth_link_t link{true, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(face, &link));
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&request, output, sizeof(output),
                                            s_now + 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(face, &request));
    s_hold_engine = true;
    registers()[0x1014U / 4U] |= 1U << 20U;
    EXPECT_EQ(NX_ERROR_BUSY, Stop());
    s_now += 1001U;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_service(face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, s_mac->tx_slots[0].request);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    s_hold_engine = false;
    registers()[0x1018U / 4U] &= ~0x2002U;
    nx_eth_model_poll(s_mac);
    EXPECT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, s_mac->tx_slots[0].request);
}
