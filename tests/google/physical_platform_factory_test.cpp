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
#include "stm32f407_provider.h"
nx_platform_start_result_t nx_empty_platform_start(void);
nx_result_t nx_empty_platform_stop(void);
nx_platform_start_result_t
nx_empty_platform_start_diagnosed(nx_platform_diagnostic_t* diagnostic);
nx_result_t
nx_empty_platform_stop_diagnosed(nx_platform_diagnostic_t* diagnostic);
nx_result_t nx_platform_suspend(void) __attribute__((weak));
nx_platform_start_result_t nx_platform_resume(void) __attribute__((weak));
nx_result_t nx_empty_platform_suspend(void) __attribute__((weak));
nx_platform_start_result_t nx_empty_platform_resume(void) __attribute__((weak));
/** \brief Both SDK fixtures use the same host-only NVIC register storage. */
NVIC_Type g_f4_model_nvic;
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
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
};
static PhysicalSystemPorts* s_system;
static bool s_privileged;
static unsigned s_starts;
static unsigned s_stops;
static unsigned s_identities;
static unsigned s_writes;
static bool s_refuse_selected_gpio_mode;
static constexpr size_t MMIO_BYTES = 0x80000U;
static constexpr uintptr_t MMIO_BASE = UINT32_C(0x40000000);
static constexpr unsigned SELECTED_GPIO = 4U;
static constexpr unsigned SELECTED_PIN = 3U;
/** \brief Inspect the same physical GPIO layout through both official SDKs. */
static volatile uint32_t& selected_gpio(unsigned offset) {
    return *reinterpret_cast<volatile uint32_t*>(
        UINT32_C(0x40020000) + SELECTED_GPIO * 0x400U + offset);
}
/** \brief GPIO gate is shared; its prior owners survive every phase. */
static volatile uint32_t& selected_gpio_gate(void) {
    return RCC->AHB1ENR;
}

extern "C" bool nx_physical_factory_is_privileged(void) {
    return s_privileged;
}
extern "C" void nx_physical_factory_write(volatile uint32_t* reg,
                                          uint32_t value) {
    ++s_writes;
    if (!s_refuse_selected_gpio_mode || reg != &selected_gpio(0U))
        *reg = value;
}
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    ++s_identities;
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return s_system->Preflight();
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
    nx_platform_start_result_t
    start_diagnosed(nx_platform_diagnostic_t* diagnostic) {
        return GetParam() ? nx_platform_start_diagnosed(diagnostic)
                          : nx_empty_platform_start_diagnosed(diagnostic);
    }
    nx_result_t stop_diagnosed(nx_platform_diagnostic_t* diagnostic) {
        return GetParam() ? nx_platform_stop_diagnosed(diagnostic)
                          : nx_empty_platform_stop_diagnosed(diagnostic);
    }
    void SetUp() override {
        g_f4_model_nvic = {};
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
        s_refuse_selected_gpio_mode = false;
        ON_CALL(system, Preflight()).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(system, Start()).WillByDefault(Return(0));
        ON_CALL(system, Stop()).WillByDefault(Return(0));
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        ASSERT_FALSE(nx_arch_irq_is_masked());
        ASSERT_EQ(stop(), NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping == nullptr) {
            return;
        }
        s_privileged = true;
        s_refuse_selected_gpio_mode = false;
        ON_CALL(system, Stop()).WillByDefault(Return(0));
        EXPECT_EQ(stop(), NX_SUCCESS);
        s_system = nullptr;
        EXPECT_EQ(munmap(mapping, MMIO_BYTES), 0);
        mapping = nullptr;
    }
};

TEST_P(PhysicalPlatformFactory, SuspendAndResumeUseTheActualStaticPinOwners) {
    auto suspend =
        GetParam() ? &nx_platform_suspend : &nx_empty_platform_suspend;
    auto resume = GetParam() ? &nx_platform_resume : &nx_empty_platform_resume;
    ASSERT_NE(nullptr, suspend);
    ASSERT_NE(nullptr, resume);
    ASSERT_EQ(NX_SUCCESS, start().primary);
    g_nx_stm32_system.started = true;
    g_nx_stm32_system.core_hz = 168000000U;
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
    RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                RCC_CFGR_PPRE2_DIV2;
    unsigned starts = s_starts;
    unsigned stops = s_stops;
    ASSERT_EQ(NX_SUCCESS, suspend());
    EXPECT_EQ(stops, s_stops);
    EXPECT_TRUE(g_nx_stm32_system.started);
    if (GetParam()) {
        EXPECT_EQ(0U, selected_gpio_gate() & (1U << SELECTED_GPIO));
    }
    ASSERT_EQ(NX_SUCCESS, resume().primary);
    EXPECT_EQ(starts, s_starts);
    EXPECT_EQ(stops, s_stops);
    if (GetParam()) {
        EXPECT_NE(0U, selected_gpio_gate() & (1U << SELECTED_GPIO));
        EXPECT_EQ(1U, (selected_gpio(0U) >> (SELECTED_PIN * 2U)) & 3U);
    }
}

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

TEST_P(PhysicalPlatformFactory, BasePreflightRejectsBeforeAnyOuterLease) {
    const auto before = register_snapshot();
    EXPECT_CALL(system, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(system, Start()).Times(0);
    const auto result = start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(s_starts, 0U);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_EQ(register_snapshot(), before);
}

TEST_P(PhysicalPlatformFactory, PrivilegedOwnerCanRestartActualSelectedGraph) {
    EXPECT_EQ(start().primary, NX_SUCCESS);
    if (GetParam()) {
        const nx_gpio_port_t* port = nx_factory_gpio(NX_GPIO_ID_OUTPUT);
        ASSERT_NE(port, nullptr);
        EXPECT_EQ(nx_gpio_port_write(port, 8U, 0U), NX_SUCCESS);
    }
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(start().primary, NX_SUCCESS);
    EXPECT_EQ(stop(), NX_SUCCESS);
    EXPECT_EQ(s_starts, 2U);
    EXPECT_EQ(s_stops, 2U);
}

TEST_P(PhysicalPlatformFactory, PoweredForeignOutputOrAfRejectsWithoutEffects) {
    for (uint32_t mode : {1U, 2U}) {
        selected_gpio_gate() = 1U << SELECTED_GPIO;
        selected_gpio(0U) = mode << (SELECTED_PIN * 2U);
        const auto before = register_snapshot();
        s_starts = s_writes = 0U;
        const auto result = start();
        EXPECT_EQ(GetParam() ? NX_ERROR_BUSY : NX_SUCCESS, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(GetParam() ? 0U : 1U, s_starts);
        EXPECT_EQ(0U, s_writes);
        EXPECT_EQ(before, register_snapshot());
        ASSERT_EQ(NX_SUCCESS, stop());
    }
}

TEST_P(PhysicalPlatformFactory, PoweredSelectedLockRejectsWithoutStealingPin) {
    selected_gpio_gate() = 1U << SELECTED_GPIO;
    selected_gpio(28U) = (1U << 16U) | (1U << SELECTED_PIN);
    const auto before = register_snapshot();
    const auto result = start();
    EXPECT_EQ(GetParam() ? NX_ERROR_BUSY : NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(GetParam() ? 0U : 1U, s_starts);
    EXPECT_EQ(0U, s_writes);
    EXPECT_EQ(before, register_snapshot());
}

TEST_P(PhysicalPlatformFactory, UnpoweredForeignPinOnlyRollsBackAddedGate) {
    for (bool locked : {false, true}) {
        selected_gpio_gate() = 1U << 2U;
        selected_gpio(0U) = locked ? 0U : 2U << (SELECTED_PIN * 2U);
        selected_gpio(28U) = locked ? (1U << 16U) | (1U << SELECTED_PIN) : 0U;
        const auto before = register_snapshot();
        const auto result = start();
        EXPECT_EQ(GetParam() ? NX_ERROR_BUSY : NX_SUCCESS, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(1U << 2U, selected_gpio_gate());
        EXPECT_EQ(before, register_snapshot());
        ASSERT_EQ(NX_SUCCESS, stop());
    }
}

TEST_P(PhysicalPlatformFactory,
       ResetInputAndAnalogPreserveSharedNeighborFields) {
    for (uint32_t mode : {0U, 3U}) {
        selected_gpio_gate() = (1U << SELECTED_GPIO) | (1U << 2U);
        selected_gpio(0U) = (mode << (SELECTED_PIN * 2U)) | (2U << 30U);
        selected_gpio(4U) = 1U << 15U;
        selected_gpio(8U) = 3U << 30U;
        selected_gpio(12U) = 2U << 30U;
        selected_gpio(20U) = 1U << 15U;
        selected_gpio(36U) = 7U << 28U;
        const uint32_t fields[] = {selected_gpio(0U),  selected_gpio(4U),
                                   selected_gpio(8U),  selected_gpio(12U),
                                   selected_gpio(20U), selected_gpio(36U)};
        ASSERT_EQ(NX_SUCCESS, start().primary);
        ASSERT_EQ(NX_SUCCESS, stop());
        EXPECT_EQ(fields[0], selected_gpio(0U));
        EXPECT_EQ(fields[1], selected_gpio(4U));
        EXPECT_EQ(fields[2], selected_gpio(8U));
        EXPECT_EQ(fields[3], selected_gpio(12U));
        EXPECT_EQ(fields[4], selected_gpio(20U));
        EXPECT_EQ(fields[5], selected_gpio(36U));
        EXPECT_EQ((1U << SELECTED_GPIO) | (1U << 2U), selected_gpio_gate());
    }
}
TEST_P(PhysicalPlatformFactory,
       FailedGlobalAnalogWithdrawalRetainsPinGateAndStopRetry) {
    selected_gpio(0U) = 3U << (SELECTED_PIN * 2U);
    selected_gpio_gate() = 1U << 2U;
    ASSERT_EQ(NX_SUCCESS, start().primary);
    s_refuse_selected_gpio_mode = true;
    EXPECT_EQ(GetParam() ? NX_ERROR_IO : NX_SUCCESS, stop());
    if (GetParam()) {
        EXPECT_EQ(NX_ERROR_STATE, start().primary);
        EXPECT_NE(0U, selected_gpio_gate() & (1U << SELECTED_GPIO));
    } else {
        EXPECT_EQ(1U << 2U, selected_gpio_gate());
    }
    s_refuse_selected_gpio_mode = false;
    EXPECT_EQ(NX_SUCCESS, stop());
    EXPECT_EQ(3U << (SELECTED_PIN * 2U), selected_gpio(0U));
    EXPECT_EQ(1U << 2U, selected_gpio_gate());
}

TEST_P(PhysicalPlatformFactory, DiagnosedPreflightFailureHasNoOuterEffects) {
    const auto before = register_snapshot();
    EXPECT_CALL(system, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(system, Start()).Times(0);
    nx_platform_diagnostic_t diagnostic = {};
    const auto result = start_diagnosed(&diagnostic);
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_SYSTEM);
    EXPECT_EQ(diagnostic.controller_index, SIZE_MAX);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.cleanup_controller_index, SIZE_MAX);
    EXPECT_EQ(register_snapshot(), before);
}

TEST_P(PhysicalPlatformFactory, DiagnosedSystemStopFailureRetainsSession) {
    ASSERT_EQ(start().primary, NX_SUCCESS);
    ON_CALL(system, Stop()).WillByDefault(Return(1));
    nx_platform_diagnostic_t diagnostic = {};
    EXPECT_EQ(stop_diagnosed(&diagnostic), NX_ERROR_IO);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_SYSTEM);
    EXPECT_EQ(diagnostic.controller_index, SIZE_MAX);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.cleanup_controller_index, SIZE_MAX);
    EXPECT_EQ(start().primary, NX_ERROR_STATE);
    ON_CALL(system, Stop()).WillByDefault(Return(0));
    EXPECT_EQ(stop_diagnosed(&diagnostic), NX_SUCCESS);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.controller_index, SIZE_MAX);
}

TEST_P(PhysicalPlatformFactory, DiagnosedResourceStopFailureRetainsPinLease) {
    selected_gpio(0U) = 3U << (SELECTED_PIN * 2U);
    selected_gpio_gate() = 1U << 2U;
    ASSERT_EQ(start().primary, NX_SUCCESS);
    s_refuse_selected_gpio_mode = true;
    nx_platform_diagnostic_t diagnostic = {};
    EXPECT_EQ(stop_diagnosed(&diagnostic),
              GetParam() ? NX_ERROR_IO : NX_SUCCESS);
    EXPECT_EQ(diagnostic.phase, GetParam() ? NX_PLATFORM_PHASE_RESOURCES
                                           : NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.controller_index, SIZE_MAX);
    EXPECT_EQ(diagnostic.cleanup_phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(diagnostic.cleanup_controller_index, SIZE_MAX);
    if (GetParam()) {
        EXPECT_NE(0U, selected_gpio_gate() & (1U << SELECTED_GPIO));
        EXPECT_EQ(start().primary, NX_ERROR_STATE);
    }
    s_refuse_selected_gpio_mode = false;
    EXPECT_EQ(stop_diagnosed(&diagnostic), NX_SUCCESS);
    EXPECT_EQ(diagnostic.phase, NX_PLATFORM_PHASE_NONE);
    EXPECT_EQ(3U << (SELECTED_PIN * 2U), selected_gpio(0U));
    EXPECT_EQ(1U << 2U, selected_gpio_gate());
}

INSTANTIATE_TEST_SUITE_P(EmptyAndGPIO, PhysicalPlatformFactory,
                         testing::Bool());
