/**
 * \file            ethernet_frames_factory_test.cpp
 * \brief           Normal generated frames with actual System and DMA domains
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "eth_frames_mac.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_ETH_FRAMES_FACTORY_STM32
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_nx_eth_frames_factory_nvic;
#else
#include "gd32f4xx_pmu.h"
#include "gd32f4xx_rcu.h"
#include "private/system.h"
extern uint32_t SystemCoreClock;
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
extern const uint32_t __gVectors[] = {0U};
#endif
}
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
constexpr uintptr_t mmio_base = UINT32_C(0x40000000);
constexpr uintptr_t mac_base = UINT32_C(0x40028000);
constexpr uintptr_t packet_base = UINT32_C(0x20018000);
constexpr size_t packet_bytes = 4096U;
class Hardware {
  public:
    MOCK_METHOD(void, MacWrite, (unsigned, uint32_t));
    unsigned mac_reads = 0U;
    unsigned mac_writes = 0U;
    unsigned clock_writes = 0U;
    bool hold_hse = false;
    bool hold_mac_reset = false;
    bool irq_enabled = false;
};
Hardware* hardware;
nx_eth_frames_mac_t* mac;
volatile uint32_t& Reg(uintptr_t address) {
    return *reinterpret_cast<volatile uint32_t*>(address);
}
uint32_t* Registers() {
    return reinterpret_cast<uint32_t*>(mac_base);
}
void ClockFacts() {
    uint32_t control = Reg(UINT32_C(0x40023800));
    if ((control & 1U) != 0U) {
        control |= 2U;
    }
    if ((control & UINT32_C(0x00010000)) != 0U && !hardware->hold_hse) {
        control |= UINT32_C(0x00020000);
    } else {
        control &= ~UINT32_C(0x00020000);
    }
    if ((control & UINT32_C(0x01000000)) != 0U) {
        control |= UINT32_C(0x02000000);
    } else {
        control &= ~UINT32_C(0x02000000);
    }
    Reg(UINT32_C(0x40023800)) = control;
    uint32_t configuration = Reg(UINT32_C(0x40023808));
    Reg(UINT32_C(0x40023808)) =
        (configuration & ~UINT32_C(0x0c)) | ((configuration & 3U) << 2U);
#ifndef NX_ETH_FRAMES_FACTORY_STM32
    if ((PMU_CTL & PMU_CTL_HDEN) != 0U) {
        PMU_CS |= PMU_CS_HDRF;
    } else {
        PMU_CS &= ~(uint32_t)PMU_CS_HDRF;
    }
    if ((PMU_CTL & PMU_CTL_HDS) != 0U) {
        PMU_CS |= PMU_CS_HDSRF;
    } else {
        PMU_CS &= ~(uint32_t)PMU_CS_HDSRF;
    }
#endif
}
class EthernetFramesFactory : public testing::Test {
  protected:
    testing::NiceMock<Hardware> oracle;
    void* mapping = nullptr;
    void* packets = nullptr;
    const nx_eth_frames_port_t* face = nullptr;
    nx_eth_frames_tx_request_t request{};
    nx_eth_fragment_t pieces[2]{};
    nx_eth_frames_rx_loan_t held{};
    bool loan_held = false;
    void SetUp() override {
        hardware = &oracle;
        mapping =
            mmap(reinterpret_cast<void*>(mmio_base), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(mmio_base), mapping);
        /* The generated descriptor and RX arrays live in a separate NOLOAD
         * ELF SRAM section. This mapping supplies only caller TX payload. */
        packets =
            mmap(reinterpret_cast<void*>(packet_base), packet_bytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(packet_base), packets);
        std::memset(NVIC, 0, sizeof(*NVIC));
        Reg(UINT32_C(0x40023800)) = 3U;
        Reg(UINT32_C(0x40023804)) = UINT32_C(0x24003010);
#ifdef NX_ETH_FRAMES_FACTORY_STM32
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = FLASH;
        g_nx_stm32_system.power = PWR;
        g_nx_stm32_system.timer = TIM2;
#else
        SystemCoreClock = 16000000U;
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        NVIC->priority_group = 3U;
        std::memset(SCB, 0, sizeof(*SCB));
        std::memset(SysTick, 0, sizeof(*SysTick));
#endif
        face = nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC);
        ASSERT_NE(nullptr, face);
        mac = static_cast<nx_eth_frames_mac_t*>(face->context);
        auto* head = static_cast<uint8_t*>(packets);
        auto* tail = head + 256U;
        head[12] = 0x81U;
        tail[1] = 0xb0U;
        tail[2] = 0x25U;
        tail[3] = 8U;
        pieces[0] = {head, 13U};
        pieces[1] = {tail, 51U};
        nx_request_initialize(&request.base);
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
        ASSERT_TRUE(mac->snapshot.mac.started);
    }
    nx_result_t Stop() {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < 16U && result == NX_ERROR_BUSY; ++i) {
            result = nx_platform_stop();
            if (mac->registers != 0U) {
                nx_eth_frames_model_poll(mac);
            }
        }
        return result;
    }
    void TearDown() override {
        oracle.hold_hse = oracle.hold_mac_reset = false;
        if (loan_held) {
            EXPECT_EQ(NX_SUCCESS, nx_eth_frames_port_release(face, &held));
            loan_held = false;
        }
        nx_result_t result = Stop();
        if (result != NX_SUCCESS && mac->retained) {
            const nx_result_t recovered = nx_eth_frames_port_recover(
                face, nx_deadline_after(nx_time_now_us(), 1000U));
            EXPECT_EQ(NX_SUCCESS, recovered);
            result = Stop();
        }
        EXPECT_EQ(NX_SUCCESS, result);
        EXPECT_FALSE(mac->retained);
        EXPECT_FALSE(oracle.irq_enabled);
        bool pending = false;
        for (size_t i = 0U; i < mac->tx_count; ++i) {
            pending = pending || mac->tx_slots[i].request != nullptr;
        }
        if (result != NX_SUCCESS || mac->retained || pending ||
            mac->snapshot.mac.held_rx != 0U || mac->wake_publishers != 0U ||
            oracle.irq_enabled) {
            ADD_FAILURE()
                << "Owner still retains shared clock or caller storage";
            std::abort();
        }
        hardware = nullptr;
        if (packets != nullptr && packets != MAP_FAILED) {
            munmap(packets, packet_bytes);
        }
        if (mapping != nullptr && mapping != MAP_FAILED) {
            munmap(mapping, 0x80000U);
        }
    }
};
} /* namespace */
extern "C" void nx_eth_frames_factory_register_write(volatile uint32_t* reg,
                                                     uint32_t value) {
    ++hardware->clock_writes;
    *reg = value;
}
#ifdef NX_ETH_FRAMES_FACTORY_STM32
extern "C" uint16_t nx_eth_frames_factory_flash_kib(void) {
    return 1024U;
}
extern "C" uint32_t nx_eth_frames_factory_device_id(void) {
    return 0x413U;
}
extern "C" void nx_stm32_system_model_poll(void*) {
    ClockFacts();
}
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    ++hardware->clock_writes;
    if (reg == &TIM2->SR) {
        *reg &= value;
    } else {
        *reg = value;
    }
    if (reg == &TIM2->EGR && (value & TIM_EGR_UG) != 0U) {
        TIM2->SR |= TIM_SR_UIF;
        TIM2->CNT = 0U;
    }
}
extern "C" bool nx_stm32_system_model_irq_available(int irq) {
    const unsigned index = static_cast<unsigned>(irq);
    const uint32_t bit = 1U << (index % 32U);
    return ((NVIC->ISER[index / 32U] | NVIC->ISPR[index / 32U] |
             NVIC->IABR[index / 32U]) &
            bit) == 0U;
}
extern "C" int nx_stm32_system_model_irq_control(int irq, bool enabled) {
    const unsigned index = static_cast<unsigned>(irq);
    const uint32_t bit = 1U << (index % 32U);
    if (enabled) {
        NVIC->ISER[index / 32U] |= bit;
    } else {
        NVIC->ISER[index / 32U] &= ~bit;
    }
    NVIC->ISPR[index / 32U] &= ~bit;
    return 0;
}
#else
extern "C" void nx_gd32_model_clock_poll(void) {
    ClockFacts();
}
extern "C" void nx_gd32_system_model_write(volatile uint32_t* reg,
                                           uint32_t value) {
    ++hardware->clock_writes;
    *reg = value;
}
extern "C" void nx_gd32_timebase_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++hardware->clock_writes;
    if (reg == &TIMER_INTF(TIMER1)) {
        *reg &= value;
    } else {
        *reg = value;
    }
    if (reg == &TIMER_SWEVG(TIMER1) && (value & TIMER_SWEVG_UPG) != 0U) {
        TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
        TIMER_CNT(TIMER1) = 0U;
    }
}
extern "C" void nx_gd32_timebase_model_disable_irq(unsigned irq) {
    const uint32_t bit = 1U << (irq % 32U);
    NVIC->ISER[irq / 32U] &= ~bit;
}
#endif
extern "C" uint32_t nx_eth_frames_model_read(uintptr_t base, unsigned offset) {
    ++hardware->mac_reads;
    return reinterpret_cast<uint32_t*>(base)[offset / 4U];
}
extern "C" void nx_eth_frames_model_write(uintptr_t base, unsigned offset,
                                          uint32_t value) {
    ++hardware->mac_writes;
    hardware->MacWrite(offset, value);
    auto* regs = reinterpret_cast<uint32_t*>(base);
    if (offset == 0x1014U) {
        regs[offset / 4U] &= ~(value & UINT32_C(0x0001e7ff));
    } else {
        regs[offset / 4U] = value;
        if (offset == 0x1018U) {
            if ((value & 0x2000U) != 0U) {
                regs[0x1014U / 4U] |= UINT32_C(0x00600000);
            }
            if ((value & 2U) != 0U) {
                regs[0x1014U / 4U] |= UINT32_C(0x00060000);
            }
        }
    }
}
extern "C" void nx_eth_frames_model_poll(void* context) {
    auto* selected = static_cast<nx_eth_frames_mac_t*>(context);
    auto* regs = reinterpret_cast<uint32_t*>(selected->registers);
#ifdef NX_ETH_FRAMES_FACTORY_STM32
    if ((TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        ++TIM2->CNT;
    }
#else
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0U) {
        ++TIMER_CNT(TIMER1);
    }
#endif
    if ((regs[0x1000U / 4U] & 1U) != 0U && !hardware->hold_mac_reset) {
        std::memset(regs, 0, 0x1060U);
    }
    if ((regs[0x1018U / 4U] & 0x2000U) == 0U) {
        regs[0x1014U / 4U] &= ~UINT32_C(0x00700000);
    }
    if ((regs[0x1018U / 4U] & 2U) == 0U) {
        regs[0x1014U / 4U] &= ~UINT32_C(0x000e0000);
    }
    if ((regs[0x1014U / 4U] & UINT32_C(0x00700000)) == 0U &&
        (regs[0x34U / 4U] & UINT32_C(0x01770000)) == 0U) {
        regs[0x1018U / 4U] &= ~UINT32_C(0x00100000);
    }
    if ((regs[0x700U / 4U] & 4U) != 0U) {
        regs[0x708U / 4U] = regs[0x710U / 4U];
        regs[0x70cU / 4U] = regs[0x714U / 4U];
    }
    regs[0x700U / 4U] &= ~UINT32_C(0x2c);
}
extern "C" void nx_eth_frames_model_descriptor(void* descriptor,
                                               uint32_t value) {
    static_cast<nx_eth_frames_descriptor_t*>(descriptor)->status = value;
}
extern "C" nx_result_t nx_eth_frames_model_irq_control(nx_eth_frames_mac_t*,
                                                       bool enabled) {
    hardware->irq_enabled = enabled;
    const uint32_t bit = UINT32_C(1) << (61U % 32U);
    if (enabled) {
        NVIC->ISER[61U / 32U] |= bit;
    } else {
        NVIC->ISER[61U / 32U] &= ~bit;
    }
    NVIC->ISPR[61U / 32U] &= ~bit;
    return NX_SUCCESS;
}
extern "C" void nx_eth_frames_model_pend_irq(nx_eth_frames_mac_t*) {
    NVIC->ISPR[61U / 32U] |= UINT32_C(1) << (61U % 32U);
}
extern "C" nx_result_t
nx_eth_frames_model_validate_wake(nx_eth_frames_mac_t* selected,
                                  const nx_irq_wake_t* wake, uint8_t ceiling) {
    return ceiling != selected->irq_policy.syscall_ceiling
               ? NX_ERROR_PERMISSION
               : nx_irq_wake_validate(wake, &selected->irq_policy,
                                      &selected->source);
}
TEST_F(EthernetFramesFactory,
       TypedLookupHasNoHardwareEffectsAndSharesMdioOwner) {
    const unsigned reads = oracle.mac_reads, writes = oracle.mac_writes;
    const unsigned clock_writes = oracle.clock_writes;
    EXPECT_EQ(face, nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC));
    const nx_mdio_port_t* mdio = nx_factory_mdio(NX_MDIO_ID_MAC);
    ASSERT_NE(nullptr, mdio);
    EXPECT_EQ(face->context, mdio->context);
    EXPECT_EQ(&nx_eth_frames_mac_ops, face->ops);
    EXPECT_EQ(&nx_eth_frames_mdio_ops, mdio->ops);
    EXPECT_EQ(reads, oracle.mac_reads);
    EXPECT_EQ(writes, oracle.mac_writes);
    EXPECT_EQ(clock_writes, oracle.clock_writes);
}
TEST_F(EthernetFramesFactory,
       RealSystemEstablishesClockBeforeActualDmaDomainUse) {
    Start();
    EXPECT_EQ(0x0aU, Reg(UINT32_C(0x40023808)) & 0x0fU);
    EXPECT_EQ(0x1e000000U, Reg(UINT32_C(0x40023830)) & 0x1e000000U);
    EXPECT_EQ(20U, Registers()[0x704U / 4U]);
    EXPECT_FALSE(mac->snapshot.mac.link.up);
    EXPECT_NE(0U, mac->clock_domain);
    EXPECT_GE(reinterpret_cast<uintptr_t>(mac->tx_descriptors), 0x20000000U);
    EXPECT_LT(reinterpret_cast<uintptr_t>(mac->rx_descriptors), 0x20020000U);
    EXPECT_GE(reinterpret_cast<uintptr_t>(mac->rx_slots[0].data), 0x20000000U);
    EXPECT_LT(reinterpret_cast<uintptr_t>(mac->rx_slots[7].data) +
                  mac->rx_slots[7].capacity,
              0x20020000U);
#ifdef NX_ETH_FRAMES_FACTORY_STM32
    EXPECT_EQ(168000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(83U, TIM2->PSC);
#else
    EXPECT_EQ(200000000U, SystemCoreClock);
    EXPECT_EQ(99U, TIMER_PSC(TIMER1));
#endif
}
TEST_F(EthernetFramesFactory,
       GeneratedWholeFramePreservesVlanAndHardwareCapture) {
    Start();
    const nx_eth_link_t link{true, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(face, &link));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_tx_prepare(
                              &request, pieces, 2U, true,
                              nx_deadline_after(nx_time_now_us(), 10000U)));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_submit(face, &request));
    Registers()[0x708U / 4U] = 18U;
    mac->tx_descriptors[0].status = 0U;
    mac->tx_descriptors[1].timestamp_high = 17U;
    mac->tx_descriptors[1].timestamp_low = 123456780U;
    mac->tx_descriptors[1].status = 0x20000U;
    for (unsigned i = 0U; i < 3U; ++i) {
        ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_service(face));
    }
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(64U, request.base.transferred);
    EXPECT_EQ(0x81U, pieces[0].data[12]);
    EXPECT_EQ(0xb0U, pieces[1].data[1]);
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, request.stamp.source);
    EXPECT_EQ(17123456780U, request.stamp.ticks);
}
TEST_F(EthernetFramesFactory,
       HeldFrameRetainsRealClockAndPadsUntilExactRelease) {
    Start();
    const nx_eth_link_t link{true, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_apply_link(face, &link));
    std::memset(mac->rx_slots[0].data, 0, mac->rx_slots[0].capacity);
    std::memset(mac->rx_slots[1].data, 0, mac->rx_slots[1].capacity);
    mac->rx_slots[0].data[12] = 0x81U;
    mac->rx_slots[0].data[14] = 0xb0U;
    mac->rx_slots[0].data[15] = 0x25U;
    mac->rx_slots[0].data[16] = 8U;
    mac->rx_descriptors[0].status = 0x200U;
    mac->rx_descriptors[1].status = (304U << 16U) | 0x5a0U;
    mac->rx_descriptors[1].timestamp_high = 23U;
    mac->rx_descriptors[1].timestamp_low = 500000000U;
    Registers()[0x708U / 4U] = 24U;
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_service(face));
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_service(face));
    nx_eth_fragment_t views[2]{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_acquire(face, &held, views, 2U));
    loan_held = true;
    EXPECT_EQ(300U, held.length);
    EXPECT_EQ(2U, held.descriptor_count);
    EXPECT_EQ(2U, held.fragment_count);
    EXPECT_TRUE(held.vlan.present);
    EXPECT_EQ(0xb025U, held.vlan.tci);
    EXPECT_EQ(NX_IO_TIMESTAMP_HARDWARE_EVENT, held.stamp.source);
    EXPECT_EQ(23500000000U, held.stamp.ticks);
    EXPECT_EQ(NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION, held.acquired_source);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_EQ(8U, Reg(UINT32_C(0x40023808)) & 0x0cU);
    EXPECT_NE(0U, Reg(UINT32_C(0x40023830)) & 0x1e000000U);
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_release(face, &held));
    loan_held = false;
    ASSERT_EQ(NX_SUCCESS, Stop());
    EXPECT_EQ(0U, Reg(UINT32_C(0x40023830)) & 0x1e000000U);
    EXPECT_EQ(0U, Reg(UINT32_C(0x40023808)) & 0x0cU);
}
TEST_F(EthernetFramesFactory,
       StopStartPreservesOwnerIdentityAndAdvancesClockEpoch) {
    Start();
    nx_io_timestamp_t first{}, second{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(face, &first));
    ASSERT_EQ(NX_SUCCESS, Stop());
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_frames_port_clock(face, &second));
    EXPECT_EQ(face, nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC));
    EXPECT_EQ(first.domain, second.domain);
    EXPECT_GT(second.epoch, first.epoch);
}
TEST_F(EthernetFramesFactory, FailedRealHseAcknowledgementCannotReachMacReset) {
    oracle.hold_hse = true;
    EXPECT_CALL(oracle, MacWrite(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(oracle, MacWrite(0x1000U, 1U)).Times(0);
    EXPECT_NE(NX_SUCCESS, nx_platform_start().primary);
    EXPECT_FALSE(mac->snapshot.mac.started);
    EXPECT_EQ(0U, Reg(UINT32_C(0x40023830)) & 0x1e000000U);
}
