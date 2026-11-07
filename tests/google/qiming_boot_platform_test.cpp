/**
 * \file            qiming_boot_platform_test.cpp
 * \brief           Actual generated startup-memory lifetime across early exits
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "stm32f407_fsmc.h"
nx_stm32_system_t g_nx_stm32_system;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
uint32_t SystemCoreClock = 168000000U;
extern const nx_stm32_sram_boot_plan_t g_nx_stm32_sram_boot_plan;
}
#include <array>
#include <cstdio>
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
class BootSystemPorts {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(nx_result_t, Identity, (uint32_t));
    MOCK_METHOD(nx_stm32_start_result_t, Start, ());
    MOCK_METHOD(int, Stop, ());
    MOCK_METHOD(bool, ResetProof, (const nx_stm32_sram_boot_plan_t*));
};
BootSystemPorts* system_ports;
bool privileged;
unsigned clock_reads;

enum class Scenario {
    BeforeStart,
    ContextFailure,
    PreflightFailure,
    IdentityFailure,
    SystemCleanFailure,
    SystemDirtyFailure,
    AdoptionFailure,
    Started
};

std::vector<uint8_t> snapshot(uintptr_t base, size_t bytes) {
    const auto* first = reinterpret_cast<const uint8_t*>(base);
    return {first, first + bytes};
}

void exercise(Scenario scenario) {
    testing::StrictMock<BootSystemPorts> ports;
    system_ports = &ports;
    EXPECT_CALL(ports, Stop()).Times(0);
    const auto mmio = snapshot(0x40000000U, 0x80000U);
    const auto fsmc = snapshot(0x60000000U, 0x1000U);
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_EXT0);
    const nx_memory_window_port_t* window =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_RAM0);
    ASSERT_NE(nullptr, bus);
    ASSERT_NE(nullptr, window);
    EXPECT_EQ(NX_EXTERNAL_BUS_CLEANUP_PENDING, nx_external_bus_port_state(bus));
    EXPECT_EQ(NX_ERROR_BUSY, nx_external_bus_port_stop(bus));
    EXPECT_EQ(mmio, snapshot(0x40000000U, 0x80000U));
    EXPECT_EQ(fsmc, snapshot(0x60000000U, 0x1000U));
    if (scenario == Scenario::BeforeStart) {
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    } else {
        if (scenario == Scenario::ContextFailure) {
            privileged = false;
        } else {
            EXPECT_CALL(ports, Preflight())
                .WillOnce(testing::Return(scenario == Scenario::PreflightFailure
                                              ? NX_ERROR_BUSY
                                              : NX_SUCCESS));
            if (scenario != Scenario::PreflightFailure) {
                EXPECT_CALL(ports, Identity(1048576U))
                    .WillOnce(testing::Return(
                        scenario == Scenario::IdentityFailure ? NX_ERROR_INVALID
                                                              : NX_SUCCESS));
                if (scenario != Scenario::IdentityFailure) {
                    nx_stm32_start_result_t outcome{0, 0, 0U};
                    if (scenario == Scenario::SystemCleanFailure)
                        outcome = {-1, 0, 0U};
                    if (scenario == Scenario::SystemDirtyFailure)
                        outcome = {-1, -1, 0x20U};
                    EXPECT_CALL(ports, Start())
                        .WillOnce(testing::Return(outcome));
                    if (outcome.primary == 0) {
                        EXPECT_CALL(ports,
                                    ResetProof(&g_nx_stm32_sram_boot_plan))
                            .WillOnce(testing::Return(
                                scenario != Scenario::AdoptionFailure));
                    }
                }
            }
        }
        nx_platform_diagnostic_t diagnostic{};
        const auto result = nx_platform_start_diagnosed(&diagnostic);
        privileged = true;
        if (scenario == Scenario::Started) {
            EXPECT_EQ(NX_SUCCESS, result.primary);
            EXPECT_EQ(NX_EXTERNAL_BUS_PREPARED,
                      nx_external_bus_port_state(bus));
            nx_memory_window_geometry_t geometry{};
            ASSERT_EQ(NX_SUCCESS,
                      nx_memory_window_port_geometry(window, &geometry));
            EXPECT_TRUE(geometry.startup_backing);
            nx_memory_window_lease_t lease{};
            EXPECT_EQ(NX_ERROR_UNSUPPORTED,
                      nx_memory_window_port_acquire(window, &lease));
        } else {
            EXPECT_NE(NX_SUCCESS, result.primary);
            EXPECT_NE(0U, result.remaining_effects);
            if (scenario == Scenario::SystemDirtyFailure) {
                EXPECT_EQ(0x20U, result.remaining_effects & 0x20U);
            }
            if (scenario == Scenario::AdoptionFailure) {
                EXPECT_EQ(NX_ERROR_BUSY, result.cleanup);
            }
        }
        EXPECT_EQ(NX_ERROR_BUSY, nx_external_bus_port_stop(bus));
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop_diagnosed(&diagnostic));
    }
    EXPECT_EQ(0U, clock_reads);
    EXPECT_EQ(mmio, snapshot(0x40000000U, 0x80000U));
    EXPECT_EQ(fsmc, snapshot(0x60000000U, 0x1000U));
    system_ports = nullptr;
}

class QimingBootPlatform : public testing::TestWithParam<Scenario> {
  protected:
    std::array<void*, 2> mappings{MAP_FAILED, MAP_FAILED};
    void SetUp() override {
        const std::array<uintptr_t, 2> bases{0x40000000U, 0x60000000U};
        const std::array<size_t, 2> sizes{0x80000U, 0x1000U};
        for (size_t i = 0U; i < 2U; ++i) {
            mappings[i] =
                mmap(reinterpret_cast<void*>(bases[i]), sizes[i],
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(reinterpret_cast<void*>(bases[i]), mappings[i]);
        }
        privileged = true;
        clock_reads = 0U;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN |
                       RCC_AHB1ENR_GPIOFEN | RCC_AHB1ENR_GPIOGEN;
        RCC->AHB3ENR = RCC_AHB3ENR_FSMCEN;
        FSMC_Bank1->BTCR[4] = FSMC_BCR1_MBKEN | FSMC_BCR1_WREN |
                              FSMC_BCR1_MWID_0 | FSMC_BCR1_EXTMOD;
        const std::array<uint16_t, 4> masks{0xFF33U, 0xFF83U, 0xF03FU, 0x043FU};
        for (unsigned port = 0U; port < 4U; ++port) {
            auto* gpio = reinterpret_cast<GPIO_TypeDef*>(GPIOA_BASE +
                                                         (port + 3U) * 0x400U);
            for (unsigned pin = 0U; pin < 16U; ++pin) {
                if ((masks[port] & (1U << pin)) == 0U)
                    continue;
                gpio->MODER |= 2U << (pin * 2U);
                gpio->OSPEEDR |= 3U << (pin * 2U);
                gpio->AFR[pin / 8U] |= 12U << ((pin % 8U) * 4U);
            }
        }
    }
    void TearDown() override {
        if (mappings[0] != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mappings[0], 0x80000U));
        }
        if (mappings[1] != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mappings[1], 0x1000U));
        }
    }
};
} /* namespace */

TEST_P(QimingBootPlatform, PersistentResetOwnerSurvivesEveryPlatformExit) {
    /* Each child inherits actual untouched C static initialization. Test code
     * never writes generated owner flags to simulate the missing initializer.
     */
    ASSERT_EXIT(
        {
            testing::TestPartResultArray results;
            testing::ScopedFakeTestPartResultReporter reporter(
                testing::ScopedFakeTestPartResultReporter::
                    INTERCEPT_ONLY_CURRENT_THREAD,
                &results);
            exercise(GetParam());
            bool failed = false;
            for (int i = 0; i < results.size(); ++i) {
                const auto& part = results.GetTestPartResult(i);
                if (part.failed()) {
                    failed = true;
                    std::fprintf(stderr, "%s:%d: %s\n",
                                 part.file_name() != nullptr ? part.file_name()
                                                             : "<mock>",
                                 part.line_number(), part.summary());
                }
            }
            std::fflush(nullptr);
            std::_Exit(failed ? 1 : 0);
        },
        testing::ExitedWithCode(0), "");
}
INSTANTIATE_TEST_SUITE_P(
    ActualGeneratedC, QimingBootPlatform,
    testing::Values(Scenario::BeforeStart, Scenario::ContextFailure,
                    Scenario::PreflightFailure, Scenario::IdentityFailure,
                    Scenario::SystemCleanFailure, Scenario::SystemDirtyFailure,
                    Scenario::AdoptionFailure, Scenario::Started));

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    g_f4_model_mask = state.value;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return system_ports->Preflight();
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t bytes) {
    return system_ports->Identity(bytes);
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return system_ports->Start();
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return system_ports->Stop();
}
extern "C" uint64_t nx_stm32_system_clock_read(void*) {
    ++clock_reads;
    return 1000U;
}
extern "C" bool
nx_stm32_sram_boot_matches(const nx_stm32_sram_boot_plan_t* plan) {
    return system_ports->ResetProof(plan);
}
extern "C" uint32_t nx_fsmc_model_read(const volatile uint32_t* reg) {
    return *reg;
}
extern "C" uint16_t nx_fsmc_model_read16(const volatile uint16_t* reg) {
    return *reg;
}
extern "C" void nx_fsmc_model_write16(volatile uint16_t* reg, uint16_t value) {
    *reg = value;
}
