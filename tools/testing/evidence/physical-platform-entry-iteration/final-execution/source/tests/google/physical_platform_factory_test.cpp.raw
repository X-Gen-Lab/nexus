/**
 * \file            physical_platform_factory_test.cpp
 * \brief           Physical family entry rejects before outer resource effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/arch/arch.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_PHYSICAL_FACTORY_STM32
#include "stm32f407_provider.h"
#else
#include "gd32f470_provider.h"
#include "gd32f4xx_gpio.h"
#endif
nx_platform_start_result_t nx_empty_platform_start(void);
nx_result_t nx_empty_platform_stop(void);
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

using testing::NiceMock;
using testing::Return;
class PhysicalSystemPorts {
  public:
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
};
static PhysicalSystemPorts* s_system;
static bool s_privileged;
static unsigned s_starts;
static unsigned s_stops;
static unsigned s_identities;
static unsigned s_writes;
static constexpr size_t MMIO_BYTES = 0x80000U;
static constexpr uintptr_t MMIO_BASE = UINT32_C(0x40000000);

extern "C" bool nx_physical_factory_is_privileged(void) {
    return s_privileged;
}
extern "C" void nx_physical_factory_write(volatile uint32_t* reg,
                                          uint32_t value) {
    ++s_writes;
    *reg = value;
}
#ifdef NX_PHYSICAL_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    ++s_identities;
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++s_starts;
    return {s_system->Start(), 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++s_stops;
    return s_system->Stop();
}
#else
extern "C" {
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
}
extern "C" int nx_gd32_soc_start(void) {
    ++s_starts;
    return s_system->Start();
}
extern "C" int nx_gd32_soc_stop(void) {
    ++s_stops;
    return s_system->Stop();
}
#endif

/** \brief Compare real mapped register bytes; no modeled lifecycle surrogate.
 */
static std::vector<uint8_t> register_snapshot(void) {
    const auto* first = reinterpret_cast<const uint8_t*>(MMIO_BASE);
    return {first, first + MMIO_BYTES};
}
class PhysicalPlatformFactory : public testing::TestWithParam<bool> {
  protected:
    NiceMock<PhysicalSystemPorts> system;
    void* mapping = nullptr;
    nx_platform_start_result_t start(void) {
        return GetParam() ? nx_platform_start() : nx_empty_platform_start();
    }
    nx_result_t stop(void) {
        return GetParam() ? nx_platform_stop() : nx_empty_platform_stop();
    }
    void SetUp() override {
        void* admitted =
            mmap(reinterpret_cast<void*>(MMIO_BASE), MMIO_BYTES,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (admitted == reinterpret_cast<void*>(MMIO_BASE)) {
            mapping = admitted;
        } else if (admitted != MAP_FAILED) {
            /* Older kernels may ignore NOREPLACE and admit another address.
             */
            EXPECT_EQ(munmap(admitted, MMIO_BYTES), 0);
        }
        ASSERT_NE(mapping, nullptr);
        std::memset(mapping, 0, MMIO_BYTES);
        s_system = &system;
        s_privileged = true;
        s_starts = s_stops = s_identities = s_writes = 0U;
        ON_CALL(system, Start()).WillByDefault(Return(0));
        ON_CALL(system, Stop()).WillByDefault(Return(0));
#ifdef NX_PHYSICAL_FACTORY_STM32
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
#endif
        ASSERT_FALSE(nx_arch_irq_is_masked());
        ASSERT_EQ(stop(), NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping == nullptr) {
            return;
        }
        s_privileged = true;
        ON_CALL(system, Stop()).WillByDefault(Return(0));
        EXPECT_EQ(stop(), NX_SUCCESS);
        s_system = nullptr;
        EXPECT_EQ(munmap(mapping, MMIO_BYTES), 0);
        mapping = nullptr;
    }
};

TEST_P(PhysicalPlatformFactory, UnprivilegedColdStartHasNoOuterEffects) {
    const auto before = register_snapshot();
    s_privileged = false;
    const auto result = start();
    EXPECT_EQ(result.primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(s_starts, 0U);
    EXPECT_EQ(s_identities, 0U);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_EQ(register_snapshot(), before);
}

TEST_P(PhysicalPlatformFactory, UnprivilegedUntouchedStopHasNoOuterEffects) {
    const auto before = register_snapshot();
    s_privileged = false;
    EXPECT_EQ(stop(), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_stops, 0U);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_EQ(register_snapshot(), before);
}

TEST_P(PhysicalPlatformFactory, UnprivilegedStopRetainsStartedSession) {
    ASSERT_EQ(start().primary, NX_SUCCESS);
    const auto before = register_snapshot();
    const unsigned writes = s_writes;
    s_privileged = false;
    EXPECT_EQ(stop(), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_stops, 0U);
    EXPECT_EQ(s_writes, writes);
    EXPECT_EQ(register_snapshot(), before);
    s_privileged = true;
    EXPECT_EQ(start().primary, NX_ERROR_STATE);
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(s_stops, 1U);
}

TEST_P(PhysicalPlatformFactory, UnprivilegedCleanupRetryRetainsOuterLease) {
    ASSERT_EQ(start().primary, NX_SUCCESS);
    ON_CALL(system, Stop()).WillByDefault(Return(1));
    ASSERT_EQ(stop(), NX_ERROR_IO);
    const auto before = register_snapshot();
    const unsigned writes = s_writes;
    const unsigned stops = s_stops;
    s_privileged = false;
    EXPECT_EQ(stop(), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_stops, stops);
    EXPECT_EQ(s_writes, writes);
    EXPECT_EQ(register_snapshot(), before);
    s_privileged = true;
    ON_CALL(system, Stop()).WillByDefault(Return(0));
    EXPECT_EQ(start().primary, NX_ERROR_STATE);
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(s_stops, stops + 1U);
}

TEST_P(PhysicalPlatformFactory, IncomingMaskRejectsAndRemainsSaved) {
    const auto before = register_snapshot();
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(start().primary, NX_ERROR_CONTEXT);
    EXPECT_EQ(stop(), NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_EQ(s_starts, 0U);
    EXPECT_EQ(s_stops, 0U);
    EXPECT_EQ(register_snapshot(), before);
}

TEST_P(PhysicalPlatformFactory, PrivilegedOwnerCanRestartActualSelectedGraph) {
    EXPECT_EQ(start().primary, NX_SUCCESS);
    if (GetParam()) {
        const nx_gpio_port_t* port = nx_factory_gpio(NX_GPIO_ID_OUTPUT);
        ASSERT_NE(port, nullptr);
#ifdef NX_PHYSICAL_FACTORY_STM32
        EXPECT_EQ(nx_gpio_port_write(port, 8U, 0U), NX_SUCCESS);
#else
        EXPECT_EQ(nx_gpio_port_write(port, 128U, 0U), NX_SUCCESS);
#endif
    }
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(start().primary, NX_SUCCESS);
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(s_starts, 2U);
    EXPECT_EQ(s_stops, 2U);
}

INSTANTIATE_TEST_SUITE_P(EmptyAndGPIO, PhysicalPlatformFactory,
                         testing::Bool());
