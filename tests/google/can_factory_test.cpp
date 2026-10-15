/**
 * \file            can_factory_test.cpp
 * \brief           Actual generated CAN factory and lifecycle with SDK models
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "can.h"
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_CAN_FACTORY_STM32
#include "stm32f407_provider.h"
#else
#include "gd32f470_provider.h"
#include "gd32f4xx_can.h"
#endif
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

using testing::_;
using testing::NiceMock;
using testing::Return;

class CanAssemblyPorts {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
    MOCK_METHOD(nx_result_t, EnableClock, (unsigned));
    MOCK_METHOD(nx_result_t, Configure, (unsigned, unsigned));
    MOCK_METHOD(uint16_t, Capture, (unsigned, unsigned));
    MOCK_METHOD(void, Restore, (unsigned, unsigned, uint16_t));
};
static CanAssemblyPorts* s_ports;

#ifdef NX_CAN_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_nx_can_factory_nvic;
}
/** \brief Observe the separately tested system's read-only cold boundary. */
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
    nx_result_t result = s_ports->EnableClock(index);
    if (result == NX_SUCCESS) {
        RCC->AHB1ENR |= 1U << index;
    }
    return result;
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio,
                                         uint8_t pin) {
    unsigned index = static_cast<unsigned>(
        (reinterpret_cast<uintptr_t>(gpio) - GPIOA_BASE) / 0x400U);
    return s_ports->Capture(index, pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin,
                                     uint16_t state) {
    unsigned index = static_cast<unsigned>(
        (reinterpret_cast<uintptr_t>(gpio) - GPIOA_BASE) / 0x400U);
    s_ports->Restore(index, pin, state);
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                              uint8_t, uint8_t, uint8_t, bool) {
    unsigned index = static_cast<unsigned>(
        (reinterpret_cast<uintptr_t>(gpio) - GPIOA_BASE) / 0x400U);
    return s_ports->Configure(index, pin);
}
#else
extern "C" {
NVIC_Type g_gd32_model_nvic;
}
/** \brief Model only the external system admission result. */
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
    nx_result_t result = s_ports->EnableClock(index);
    if (result == NX_SUCCESS) {
        RCU_AHB1EN |= 1U << index;
    }
    return result;
}
extern "C" uint16_t nx_gd32_pin_capture(uint32_t gpio, unsigned pin) {
    return s_ports->Capture((gpio - GPIOA) / 0x400U, pin);
}
extern "C" void nx_gd32_pin_restore(uint32_t gpio, unsigned pin,
                                    uint16_t state) {
    s_ports->Restore((gpio - GPIOA) / 0x400U, pin, state);
}
extern "C" nx_result_t nx_gd32_pin_configure(uint32_t gpio, unsigned pin,
                                             uint8_t, uint8_t, uint8_t, bool) {
    return s_ports->Configure((gpio - GPIOA) / 0x400U, pin);
}
#endif

class CanFactory : public testing::Test {
  protected:
    NiceMock<CanAssemblyPorts> ports;
    void* mapping = MAP_FAILED;
    const nx_can_port_t* first = nullptr;
    const nx_can_port_t* second = nullptr;
    nx_can_controller_t* first_state = nullptr;
    nx_can_controller_t* second_state = nullptr;
    nx_native_can_registers_t* first_hw = nullptr;
    nx_native_can_registers_t* second_hw = nullptr;

    void SetUp() override {
        s_ports = &ports;
        ON_CALL(ports, Preflight()).WillByDefault(Return(NX_SUCCESS));
        std::memset(NVIC, 0, sizeof(*NVIC));
        ON_CALL(ports, EnableClock(_)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Configure(_, _)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Capture(_, _)).WillByDefault(Return(0x123U));
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        first = nx_factory_can(NX_CAN_ID_BUS0);
        second = nx_factory_can(NX_CAN_ID_BUS1);
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        first_state = static_cast<nx_can_controller_t*>(first->context);
        second_state = static_cast<nx_can_controller_t*>(second->context);
#ifdef NX_CAN_FACTORY_STM32
        uintptr_t base = CAN1_BASE;
#else
        uintptr_t base = CAN0;
#endif
        first_hw = reinterpret_cast<nx_native_can_registers_t*>(base);
        second_hw = reinterpret_cast<nx_native_can_registers_t*>(base + 0x400U);
        first_hw->registers[2] = 7U << 26U;
        second_hw->registers[2] = 7U << 26U;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            first_hw->hold_irq_disable = false;
            second_hw->hold_irq_disable = false;
            first_hw->hold_enter = false;
            second_hw->hold_enter = false;
            first_hw->hold_exit = false;
            second_hw->hold_exit = false;
            first_hw->hold_abort = false;
            second_hw->hold_abort = false;
            EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
        s_ports = nullptr;
    }
};

TEST_F(CanFactory, BasePreflightRejectsBeforeClockPinAndControllerEffects) {
    const auto* bytes = static_cast<const uint8_t*>(mapping);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    EXPECT_FALSE(first_state->snapshot.started);
    EXPECT_FALSE(second_state->snapshot.started);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(CanFactory, SelectedForeignPendingIrqRejectsBeforeClockOrPinEffects) {
#ifdef NX_CAN_FACTORY_STM32
    const unsigned irq = static_cast<unsigned>(CAN1_TX_IRQn);
#else
    const unsigned irq = static_cast<unsigned>(CAN0_TX_IRQn);
#endif
    const unsigned word = irq / 32U;
    const uint32_t bit = 1U << (irq % 32U);
    NVIC->ISPR[word] = bit;
    const auto* bytes = static_cast<const uint8_t*>(mapping);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(bit, NVIC->ISPR[word]);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    EXPECT_FALSE(first_state->snapshot.started);
    EXPECT_FALSE(second_state->snapshot.started);
    NVIC->ISPR[word] = 0U;
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(CanFactory, PureLookupPreservesSeparateIdentityBeforeStartup) {
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_EQ(nx_factory_can(NX_CAN_ID_BUS0), first);
    EXPECT_EQ(nx_factory_can(NX_CAN_ID_BUS1), second);
    EXPECT_NE(first, second);
    EXPECT_EQ(nx_factory_can(NX_CAN_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_can(UINT16_MAX), nullptr);
    EXPECT_FALSE(first_state->snapshot.started);
    EXPECT_FALSE(second_state->snapshot.started);
    EXPECT_EQ(first->ops, &nx_classic_can_ops);
    EXPECT_EQ(second->ops, first->ops);
    const nx_factory_io_capability_t* capability =
        nx_factory_can_capability(NX_CAN_ID_BUS0);
    ASSERT_NE(capability, nullptr);
    EXPECT_EQ(capability->max_transfer_units, 8U);
    EXPECT_EQ(capability->max_messages, 1U);
}
TEST_F(CanFactory, BothInstancesUseOneDomainAndDistinctRingsAndMailboxes) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(first_state->domain, second_state->domain);
    EXPECT_NE(first_state->events, second_state->events);
    EXPECT_EQ(first_state->capacity, 8U);
    EXPECT_EQ(second_state->capacity, 8U);
    EXPECT_EQ(first_state->domain->active_mask, 3U);
    EXPECT_EQ(first_state->domain->filter_count, 2U);
    EXPECT_NE(first_state->registers, second_state->registers);
    EXPECT_EQ(first_state->sources[0].irq_number, 19);
    EXPECT_EQ(second_state->sources[0].irq_number, 63);
}
TEST_F(CanFactory, StuckSecondControllerRetainsPinsAndSharedDomainUntilDrain) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    second_hw->hold_irq_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_EQ(first_state->domain->active_mask, 3U);
    EXPECT_TRUE(first_state->snapshot.started);
    EXPECT_TRUE(second_state->snapshot.started);
    testing::Mock::VerifyAndClearExpectations(&ports);
    second_hw->hold_irq_disable = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(4);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(first_state->domain->active_mask, 0U);
    EXPECT_FALSE(first_state->domain->configured);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(CanFactory, FailedSecondStartupIsIncludedInReverseCleanup) {
    second_hw->enabled_irqs = 1U;
    second_hw->hold_irq_enable = true;
    second_hw->hold_irq_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_TRUE(second_state->snapshot.started);
    EXPECT_TRUE(first_state->snapshot.started);
    testing::Mock::VerifyAndClearExpectations(&ports);
    second_hw->hold_irq_disable = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(4);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_FALSE(second_state->snapshot.started);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(CanFactory, FirstControllerClockFailureReleasesUnchangedGlobalLeases) {
    const auto* bytes = static_cast<const uint8_t*>(mapping);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    {
        testing::InSequence sequence;
        /* Capture all selected pins before a controller can mutate them. */
        EXPECT_CALL(ports, Capture(0U, 11U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(0U, 12U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(1U, 12U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(1U, 13U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Start()).WillOnce(Return(0));
        EXPECT_CALL(ports, EnableClock(0U)).WillOnce(Return(NX_ERROR_IO));
        /* Unchanged global leases need comparison, without a restore write. */
        EXPECT_CALL(ports, Capture(1U, 13U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(1U, 12U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(0U, 12U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Capture(0U, 11U)).WillOnce(Return(0x123U));
        EXPECT_CALL(ports, Stop()).WillOnce(Return(0));
    }
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(first_state->domain->active_mask, 0U);
    EXPECT_FALSE(first_state->domain->configured);
    EXPECT_FALSE(first_state->snapshot.started);
    EXPECT_FALSE(second_state->snapshot.started);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
}
TEST_F(CanFactory, SecondPinFailureRestoresOnlyTheCapturedLeases) {
    EXPECT_CALL(ports, Configure(0U, 11U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Configure(0U, 12U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(ports, Restore(0U, 12U, 0x123U)).Times(1);
    EXPECT_CALL(ports, Restore(0U, 11U, 0x123U)).Times(1);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
}
