/**
 * \file            f407_sram_boot_test.cpp
 * \brief           Real pre-runtime SRAM bounds, clocks and section contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_fsmc.h"
}
#include "f407_sram_boot_model.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
nx_f407_sram_boot_model_t g_f407_sram_boot_model;
static uint32_t expected_failure_cfgr;
uint32_t nx_f407_sram_boot_read(volatile uint32_t* reg) {
    auto& m = g_f407_sram_boot_model;
    if (reg == &RCC->AHB3ENR) {
        ++m.fsmc_gate_reads;
        if (m.transient_gate_read && m.fsmc_gate_reads >= m.transient_gate_read)
            return *reg & ~uint32_t{RCC_AHB3ENR_FSMCEN};
    }
    return *reg;
}
void nx_f407_sram_boot_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_f407_sram_boot_model;
    ++m.writes;
    if (reg == &RCC->AHB1ENR && m.refuse_gpio_gate)
        return;
    if (reg == &RCC->AHB3ENR && m.refuse_fsmc_gate)
        return;
    if (reg == &FSMC_Bank1->BTCR[4] && m.refuse_bank)
        return;
    *reg = value;
    if (reg == &GPIOG->BSRR)
        GPIOG->ODR = (GPIOG->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
void nx_f407_sram_boot_barrier(void) {
    ++g_f407_sram_boot_model.barriers;
}
class SramBoot : public testing::Test {
  protected:
    struct Mapping {
        uintptr_t address;
        size_t bytes;
        bool owned;
    };
    Mapping mappings[7] = {{0x08000000U, 0x100000U, false},
                           {0x10000000U, 0x10000U, false},
                           {0x20000000U, 0x20000U, false},
                           {0x40000000U, 0x80000U, false},
                           {NX_SRAM_MODEL_FSMC_BASE, 0x1000U, false},
                           {0x68000000U, 0x100000U, false},
                           {NX_SRAM_MODEL_SCS_BASE, 0x1000U, false}};
    nx_stm32_sram_boot_plan_t* plan = nullptr;
    nx_stm32_fsmc_bank_config_t* bank = nullptr;
    void SetUp() override {
        for (auto& m : mappings) {
            auto* p =
                mmap(reinterpret_cast<void*>(m.address), m.bytes,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(reinterpret_cast<void*>(m.address), p);
            m.owned = true;
        }
        std::memset(reinterpret_cast<void*>(0x68000000U), 0xa5, 0x100000U);
        std::memset(reinterpret_cast<void*>(0x20000000U), 0x5a, 0x20000U);
        std::memset(reinterpret_cast<void*>(0x10000000U), 0x5a, 0x10000U);
        plan = reinterpret_cast<nx_stm32_sram_boot_plan_t*>(0x08040000U);
        bank = reinterpret_cast<nx_stm32_fsmc_bank_config_t*>(0x08041000U);
        *bank = {NX_STM32_FSMC_SRAM_NE3, {1U, 1U, 8U, 1U}, {1U, 1U, 8U, 1U}};
        *plan = {};
        plan->version = NX_STM32_SRAM_BOOT_VERSION;
        plan->part = NX_STM32_SRAM_IS62WV51216;
        plan->maximum_hclk_hz = 168000000U;
        plan->region_count = 2U;
        plan->flash_start = plan->vector_address = 0x08000000U;
        plan->flash_end = 0x08100000U;
        plan->msp_start = 0x2001f000U;
        plan->msp_end = 0x20020000U;
        plan->bank = bank;
        plan->regions[0] = {0x20000000U, 0x20020000U, 0x08010000U,
                            0x20000000U, 0x20000008U, 0x20000010U,
                            0x20000018U, 0x20000020U, 0x20000028U};
        plan->regions[1] = {0x68000000U, 0x68100000U, 0x08010010U,
                            0x68000004U, 0x6800000cU, 0x68000010U,
                            0x68000018U, 0x6800001cU, 0x68000028U};
        auto* source = reinterpret_cast<uint32_t*>(0x08010010U);
        source[0] = 11U;
        source[1] = 12U;
        g_f407_sram_boot_model = {};
        expected_failure_cfgr = 0U;
        g_f407_sram_boot_model.msp = 0x2001ff00U;
        g_f407_sram_boot_model.device_id = 0x10010413U;
        g_f407_sram_boot_model.flash_kib = 1024U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->PLLCFGR = 0x24003010U;
        RCC->AHB1ENR = RCC_AHB1ENR_CCMDATARAMEN;
        FSMC_Bank1->BTCR[0] = 0x30dbU;
        for (unsigned i = 0U; i < 4U; ++i) {
            if (i)
                FSMC_Bank1->BTCR[i * 2U] = 0x30d2U;
            FSMC_Bank1->BTCR[i * 2U + 1U] = 0x0fffffffU;
            FSMC_Bank1E->BWTR[i * 2U] = 0x0fffffffU;
        }
    }
    void TearDown() override {
        for (auto& m : mappings) {
            if (m.owned) {
                EXPECT_EQ(0,
                          munmap(reinterpret_cast<void*>(m.address), m.bytes));
            }
        }
    }
    uint32_t word(size_t index) {
        return reinterpret_cast<uint32_t*>(0x68000000U)[index];
    }
    void unchanged() {
        for (size_t i = 0U; i < 12U; ++i)
            EXPECT_EQ(0xa5a5a5a5U, word(i));
    }
};
TEST_F(SramBoot, PureWholePlanValidationTouchesNoHardwareOrDestination) {
    EXPECT_TRUE(nx_stm32_sram_boot_plan_valid(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, CopiesOnlySelectedExternalSectionsAndKeepsPrimaryRamPoisoned) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan))
        << " writes=" << g_f407_sram_boot_model.writes << " AHB1=" << std::hex
        << RCC->AHB1ENR << " AHB3=" << RCC->AHB3ENR
        << " BCR=" << FSMC_Bank1->BTCR[4] << " BTR=" << FSMC_Bank1->BTCR[5]
        << " BWTR=" << FSMC_Bank1E->BWTR[4];
    EXPECT_EQ(11U, word(1U));
    EXPECT_EQ(12U, word(2U));
    EXPECT_EQ(0U, word(4U));
    EXPECT_EQ(0U, word(5U));
    for (size_t i : {0U, 3U, 6U, 7U, 8U, 9U, 10U, 11U})
        EXPECT_EQ(0xa5a5a5a5U, word(i));
    EXPECT_EQ(0x5a5a5a5aU, *reinterpret_cast<uint32_t*>(0x20000000U));
    EXPECT_TRUE(nx_stm32_sram_boot_matches(plan));
}
TEST_F(SramBoot, BadLateRegionRejectsBeforeAnyGateOrDestinationWrite) {
    plan->regions[1].bss_end = 0x68100004U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, PlanAndBankConfigurationMustResideEntirelyInInternalFlash) {
    const auto copy = *plan;
    EXPECT_FALSE(nx_stm32_sram_boot_plan_valid(&copy));
    auto bank_copy = *bank;
    plan->bank = &bank_copy;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, UnknownPartAndInvalidDeclaredTimingRejectWithoutEffects) {
    for (unsigned fault = 0U; fault < 5U; ++fault) {
        auto p = *plan;
        auto b = *bank;
        if (fault == 0U)
            plan->part = 0U;
        if (fault == 1U)
            bank->read.address_hold = 0U;
        if (fault == 2U)
            bank->write.data_setup = 0U;
        if (fault == 3U)
            plan->maximum_hclk_hz = 16000000U;
        if (fault == 4U)
            bank->kind = NX_STM32_FSMC_LCD_NE4;
        EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
        EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
        *plan = p;
        *bank = b;
    }
    unchanged();
}
TEST_F(SramBoot, OffsetImageAndExternalStackRejectBeforeHardware) {
    plan->vector_address += 0x10000U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    plan->vector_address = plan->flash_start;
    plan->msp_start = 0x680f0000U;
    plan->msp_end = 0x68100000U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, WarmClockHandoffCannotBeErasedBySystemInitialization) {
    RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, IrqPublisherOrExternalMspRejectsWithoutEffects) {
    NVIC->ISER[2] = 1U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    NVIC->ISER[2] = 0U;
    g_f407_sram_boot_model.msp = 0x680fff00U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, RefusedGpioGateLeavesExternalStorageUntouched) {
    g_f407_sram_boot_model.refuse_gpio_gate = true;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    unchanged();
}
TEST_F(SramBoot, RefusedFsmcGateLeavesExternalStorageUntouched) {
    g_f407_sram_boot_model.refuse_fsmc_gate = true;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    unchanged();
}
TEST_F(SramBoot, ForeignPoweredPadOrBankNeverGetsSilentlyCorrected) {
    GPIOF->MODER = 1U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(1U, GPIOF->MODER);
    unchanged();
}
TEST_F(SramBoot, RefusedBankReadbackLeavesExternalStorageUntouched) {
    g_f407_sram_boot_model.refuse_bank = true;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    unchanged();
}
TEST_F(SramBoot, TransientClockLossBeforeCopyIsNotAUsableBank) {
    g_f407_sram_boot_model.transient_gate_read = 3U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    unchanged();
}
TEST_F(SramBoot, ReadOnlyProofRejectsMutatedTimingOrPadsWithoutRepair) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan))
        << " writes=" << g_f407_sram_boot_model.writes << " AHB1=" << std::hex
        << RCC->AHB1ENR << " AHB3=" << RCC->AHB3ENR
        << " BCR=" << FSMC_Bank1->BTCR[4] << " BTR=" << FSMC_Bank1->BTCR[5]
        << " BWTR=" << FSMC_Bank1E->BWTR[4];
    const auto writes = g_f407_sram_boot_model.writes;
    FSMC_Bank1->BTCR[5] ^= 1U;
    EXPECT_FALSE(nx_stm32_sram_boot_matches(plan));
    FSMC_Bank1->BTCR[5] ^= 1U;
    GPIOG->AFR[1] ^= 1U << 8U;
    EXPECT_FALSE(nx_stm32_sram_boot_matches(plan));
    EXPECT_EQ(writes, g_f407_sram_boot_model.writes);
}

#if defined(NX_SRAM_SYSTEM_TEST)
asm(".globl g_nx_stm32_sram_boot_plan\n"
    ".set g_nx_stm32_sram_boot_plan, 0x08040000\n");
extern "C" nx_stm32_system_t g_nx_stm32_system;
nx_stm32_system_t g_nx_stm32_system;
extern "C" void nx_f407_sram_boot_failure(uint32_t reason) {
    if (RCC->CFGR != expected_failure_cfgr)
        _exit(91);
    if (*reinterpret_cast<uint32_t*>(0x68000004U) != 0xa5a5a5a5U)
        _exit(92);
    if (g_f407_sram_boot_model.writes != 0U)
        _exit(93);
    _exit(static_cast<int>(reason));
}
TEST_F(SramBoot, ActualSystemInitBootstrapsBeforeInitializedRam) {
    std::memset(&g_nx_stm32_system, 0x5a, sizeof(g_nx_stm32_system));
    std::array<unsigned char, sizeof(g_nx_stm32_system)> before{};
    std::memcpy(before.data(), &g_nx_stm32_system, before.size());
    SystemInit();
    EXPECT_EQ(11U, word(1U));
    EXPECT_EQ(12U, word(2U));
    EXPECT_EQ(0U, word(4U));
    EXPECT_EQ(0U, word(5U));
    EXPECT_EQ(0, std::memcmp(before.data(), &g_nx_stm32_system, before.size()));
}
TEST_F(SramBoot, ActualSystemInitRejectsWarmHandoffBeforeAnyClockRewrite) {
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        expected_failure_cfgr = RCC->CFGR;
        SystemInit();
        _exit(90);
    }
    int status = 0;
    ASSERT_EQ(child, waitpid(child, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(3, WEXITSTATUS(status));
}

TEST_F(SramBoot, ActualSystemInitRejectsUnknownCutBeforePermanentSramSetup) {
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        g_f407_sram_boot_model.device_id = 0x10020413U;
        SystemInit();
        _exit(90);
    }
    int status = 0;
    ASSERT_EQ(child, waitpid(child, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(3, WEXITSTATUS(status));
}
#endif

TEST_F(SramBoot, WrongDieIdRejectsBeforeAnyGateOrDestinationWrite) {
    g_f407_sram_boot_model.device_id = 0x999U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, UnknownObservedCutRejectsBeforeAnyGateOrDestinationWrite) {
    g_f407_sram_boot_model.device_id = 0x10020413U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}

TEST_F(SramBoot, ReadonlyBootIdentityRevalidationRejectsAnUnreviewedCut) {
    ASSERT_TRUE(nx_stm32_sram_bootstrap(plan));
    const unsigned writes = g_f407_sram_boot_model.writes;
    g_f407_sram_boot_model.device_id = 0x10020413U;
    EXPECT_FALSE(nx_stm32_sram_boot_matches(plan));
    EXPECT_EQ(writes, g_f407_sram_boot_model.writes);
}
TEST_F(SramBoot, WrongFlashDensityRejectsBeforeAnyGateOrDestinationWrite) {
    g_f407_sram_boot_model.flash_kib = 512U;
    EXPECT_FALSE(nx_stm32_sram_bootstrap(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
TEST_F(SramBoot, StackReservationBelowProvedAdmissionBoundRejectsPurely) {
    plan->msp_start = plan->msp_end - 256U;
    EXPECT_FALSE(nx_stm32_sram_boot_plan_valid(plan));
    EXPECT_EQ(0U, g_f407_sram_boot_model.writes);
    unchanged();
}
