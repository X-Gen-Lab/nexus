/**
 * \file            u575_security_test.cpp
 * \brief           Cold TrustZone ownership and actual SoC register contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_security.h"
#include "stm32u575_security_hw.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <vector>
using ::testing::Return;
using ::testing::StrictMock;
namespace {
uintptr_t key(volatile const uint32_t* p) {
    return reinterpret_cast<uintptr_t>(p);
}
class Memory {
  public:
    MOCK_METHOD(bool, Clear, (uint32_t, uint32_t));
    MOCK_METHOD(void, Enter, (uint32_t, uint32_t, uint32_t));
};
struct Hardware {
    std::map<uintptr_t, uint32_t> words;
    std::vector<std::pair<uintptr_t, uint32_t>> writes;
    std::vector<bool> changed_writes;
    std::array<uint32_t, 8> sau_base{};
    std::array<uint32_t, 8> sau_limit{};
    std::array<uint32_t, 16> mpu_base{};
    std::array<uint32_t, 16> mpu_limit{};
    uint32_t msp = 0x3002FFF0U;
    uint32_t control = 0U;
    uint32_t exception = 0U;
    nx_arch_irq_masks_t masks = {1U, 0U, 0U};
    size_t refused_write = SIZE_MAX;
    size_t reads = 0U;
    bool privileged = true;
    Memory* memory = nullptr;
};
Hardware* hardware;
class U575Security : public ::testing::Test {
  protected:
    Hardware mmio;
    StrictMock<Memory> memory;
    nx_stm32u575_tz_t owner{};
    nx_stm32u575_tz_layout_t layout = {0x08100000U, 0x10000U,    0x20040000U,
                                       0x4000U,     0x0C0FE000U, 0x2000U,
                                       0x3002F000U, 0x1000U};
    void SetUp() override {
        hardware = &mmio;
        mmio.memory = &memory;
        Seed();
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void Seed() {
        mmio.words[FLASHSIZE_BASE] = 2048U;
        mmio.words[key(&FLASH_S->OPTR)] =
            FLASH_OPTR_TZEN | FLASH_OPTR_SRAM3_ECC;
        mmio.words[key(&FLASH_S->SECWM1R1)] = 0x007F0000U;
        mmio.words[key(&FLASH_S->SECWM2R1)] = 0x0000007FU;
        mmio.words[key(&FLASH_S->SECBOOTADD0R)] = 0x0C00007CU;
        mmio.words[key(&FLASH_S->SECCR)] = FLASH_SECCR_LOCK;
        mmio.words[key(&FLASH_S->NSCR)] = FLASH_NSCR_LOCK;
        mmio.words[key(&SAU->TYPE)] = 8U;
        mmio.words[key(&SCB->VTOR)] = 0x0C000000U;
        mmio.words[key(&MPU_NS->TYPE)] = 16U << 8U;
        mmio.words[key(&RCC_S->CR)] = RCC_CR_MSISON | RCC_CR_MSISRDY;
        mmio.words[key(&RCC_S->CSR)] = 4U << RCC_CSR_MSISSRANGE_Pos;
        mmio.words[key(&RCC_S->AHB1ENR)] = RCC_AHB1ENR_SRAM1EN;
        mmio.words[key(&RCC_S->AHB2ENR1)] = RCC_AHB2ENR1_SRAM3EN;
        for (size_t i = 0U; i < 32U; ++i) {
            mmio.words[key(&GTZC_MPCBB3_S->SECCFGR[i])] = UINT32_MAX;
            mmio.words[key(&GTZC_MPCBB3_S->PRIVCFGR[i])] = UINT32_MAX;
        }
        for (size_t i = 0U; i < 12U; ++i) {
            mmio.words[key(&GTZC_MPCBB1_S->SECCFGR[i])] = UINT32_MAX;
            mmio.words[key(&GTZC_MPCBB1_S->PRIVCFGR[i])] = UINT32_MAX;
        }
        mmio.words[layout.ns_flash_base] = 0x20044000U;
        mmio.words[layout.ns_flash_base + 4U] = 0x08100401U;
    }
    void Prepare() {
        EXPECT_CALL(memory, Clear(0x30040000U, 0x4000U)).WillOnce(Return(true));
        ASSERT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_SUCCESS);
    }
};
TEST_F(U575Security, ColdPreflightDoesNotUpdateOwnerOrWriteConfiguration) {
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_SUCCESS);
    EXPECT_TRUE(mmio.writes.empty());
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_COLD);
    EXPECT_FALSE(owner.effects);
}
TEST_F(U575Security, BadGeometryRejectsBeforeAnyRegisterRead) {
    layout.ns_ram_base++;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    layout.ns_ram_base--;
    layout.ns_flash_bytes = UINT32_MAX;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    layout.ns_flash_bytes = 0x10000U;
    layout.nsc_base = 0x081FE000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    EXPECT_EQ(mmio.reads, 0U);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, EntireNonsecureSramCannotTransferItsClockOwnership) {
    layout.ns_ram_bytes = 0x80000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, SecureStackMustBeAlignedAndWithinSecureSramOne) {
    layout.secure_stack_base = 0x2002F000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    layout.secure_stack_base = 0x3002F001U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_INVALID);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, WrongContextRejectsWithoutHardwareConfiguration) {
    mmio.privileged = false;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.privileged = true;
    mmio.exception = 3U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.exception = 0U;
    mmio.masks = {0U, 0U, 0U};
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.masks = {1U, 1U, 0U};
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.masks = {1U, 0U, 1U};
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, ActualSecureStackAndMainStackSelectionAreRequired) {
    mmio.msp = 0x30010000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.msp = 0x3002FFF0U;
    mmio.control = 2U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, TzenZeroSwapOrEccOptionsRejectWithoutOptionWrites) {
    const uint32_t valid = FLASH_OPTR_TZEN | FLASH_OPTR_SRAM3_ECC;
    for (uint32_t invalid :
         {uint32_t{0}, valid | uint32_t{FLASH_OPTR_SWAP_BANK},
          uint32_t{FLASH_OPTR_TZEN}}) {
        mmio.words[key(&FLASH_S->OPTR)] = invalid;
        EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout),
                  NX_ERROR_UNSUPPORTED);
        EXPECT_EQ(owner.state, NX_STM32U575_TZ_COLD);
        EXPECT_FALSE(owner.effects);
    }
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, SecureFlashWatermarkMustCoverOwnCodeAndNsc) {
    mmio.words[key(&FLASH_S->SECWM1R1)] = 0x007E0000U;
    EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, OverlappingNsWatermarkAndBlockPermissionsAreRejected) {
    mmio.words[key(&FLASH_S->SECWM2R1)] = 0x00070000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_UNSUPPORTED);
    mmio.words[key(&FLASH_S->SECWM2R1)] = 0x0000007FU;
    mmio.words[key(&FLASH_S->SECBB2R1)] = 1U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_UNSUPPORTED);
    mmio.words[key(&FLASH_S->SECBB2R1)] = 0U;
    mmio.words[key(&FLASH_S->PRIVBB2R1)] = 1U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_UNSUPPORTED);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, ActiveDmaCacheOrAttributionIsNeverAdopted) {
    for (auto item : std::vector<std::pair<uintptr_t, uint32_t>>{
             {key(&RCC_S->AHB1ENR), RCC_AHB1ENR_GPDMA1EN},
             {key(&RCC_S->AHB3ENR), RCC_AHB3ENR_LPDMA1EN},
             {key(&ICACHE_S->CR), ICACHE_CR_EN},
             {key(&DCACHE1_S->CR), DCACHE_CR_EN},
             {key(&SAU->CTRL), 1U},
             {key(&MPU_NS->CTRL), 1U},
             {key(&NVIC->ISER[0]), 1U},
             {key(&NVIC_NS->ISER[3]), 1U}}) {
        const uint32_t previous = mmio.words[item.first];
        mmio.words[item.first] |= item.second;
        EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_BUSY);
        mmio.words[item.first] = previous;
    }
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, PreparesOnlyExplicitRamAndSealsItsSecurityAndPrivilege) {
    Prepare();
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_READY);
    EXPECT_TRUE(owner.effects);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->SECCFGR[0])], 0U);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->PRIVCFGR[0])], 0U);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->CFGLOCKR1)], UINT32_MAX);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->CR)], 1U);
    for (size_t i = 1U; i < 32U; ++i) {
        EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->SECCFGR[i])], UINT32_MAX);
        EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->PRIVCFGR[i])], UINT32_MAX);
    }
}
TEST_F(U575Security, SauContainsOnlyNsFlashNsRamAndSecureAliasGateways) {
    Prepare();
    EXPECT_EQ(mmio.sau_base[0], 0x08100000U);
    EXPECT_EQ(mmio.sau_limit[0], 0x0810FFE1U);
    EXPECT_EQ(mmio.sau_base[1], 0x20040000U);
    EXPECT_EQ(mmio.sau_limit[1], 0x20043FE1U);
    EXPECT_EQ(mmio.sau_base[2], 0x0C0FE000U);
    EXPECT_EQ(mmio.sau_limit[2], 0x0C0FFFE3U);
    for (size_t i = 3U; i < 8U; ++i)
        EXPECT_EQ(mmio.sau_limit[i], 0U);
    EXPECT_EQ(mmio.words[key(&SAU->CTRL)], 1U);
}
TEST_F(U575Security, NsMpuRejectsExecutableRamAndPeripheralDefaultAccess) {
    Prepare();
    EXPECT_EQ(mmio.mpu_base[0], 0x08100006U);
    EXPECT_EQ(mmio.mpu_limit[0], 0x0810FFE1U);
    EXPECT_EQ(mmio.mpu_base[1], 0x20040003U);
    EXPECT_EQ(mmio.mpu_limit[1], 0x20043FE1U);
    for (size_t i = 2U; i < 16U; ++i)
        EXPECT_EQ(mmio.mpu_limit[i], 0U);
    EXPECT_EQ(mmio.words[key(&MPU_NS->MAIR0)], 0x44U);
    EXPECT_EQ(mmio.words[key(&MPU_NS->CTRL)], 1U);
}
TEST_F(U575Security, RamClearFailureCannotExposeStorageOrPublishReady) {
    EXPECT_CALL(memory, Clear(0x30040000U, 0x4000U)).WillOnce(Return(false));
    EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_IO);
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_FAILED);
    EXPECT_TRUE(owner.effects);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB3_S->SECCFGR[0])], UINT32_MAX);
    EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_STATE);
}
TEST_F(U575Security, EveryRefusedConfigurationWriteRetainsFailedOwnership) {
    Prepare();
    const size_t count = mmio.writes.size();
    const auto changes = mmio.changed_writes;
    for (size_t step = 0U; step < count; ++step) {
        mmio = Hardware{};
        mmio.memory = &memory;
        Seed();
        owner = {};
        mmio.refused_write = step;
        EXPECT_CALL(memory, Clear(0x30040000U, 0x4000U))
            .Times(::testing::AtMost(1))
            .WillRepeatedly(Return(true));
        const nx_result_t result = nx_stm32u575_tz_prepare(&owner, &layout);
        /* A changed write must be detected; an already-equal write is harmless.
         */
        EXPECT_EQ(result == NX_SUCCESS, !changes[step]) << step;
        if (result != NX_SUCCESS) {
            EXPECT_EQ(owner.state, NX_STM32U575_TZ_FAILED);
            EXPECT_TRUE(owner.effects);
            EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_STATE);
        }
        ::testing::Mock::VerifyAndClearExpectations(&memory);
    }
}
TEST_F(U575Security, ZeroExceptionVectorsAndExactThumbResetPermitEntry) {
    Prepare();
    uint32_t stack = 9U;
    uint32_t entry = 11U;
    EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry), NX_SUCCESS);
    EXPECT_EQ(stack, 0x20044000U);
    EXPECT_EQ(entry, 0x08100401U);
}
TEST_F(U575Security, NonzeroPrivilegedExceptionVectorRejectsAndKeepsOutputs) {
    Prepare();
    uint32_t stack = 9U;
    uint32_t entry = 11U;
    for (uint32_t vector : {2U, 3U, 11U, 14U, 15U, 141U}) {
        mmio.words[layout.ns_flash_base + vector * 4U] = 0x08100401U;
        EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry),
                  NX_ERROR_PERMISSION);
        mmio.words[layout.ns_flash_base + vector * 4U] = 0U;
    }
    EXPECT_EQ(stack, 9U);
    EXPECT_EQ(entry, 11U);
}
TEST_F(U575Security, NsStackAndEntryCannotEscapeOrDisableThumbState) {
    Prepare();
    uint32_t stack = 9U;
    uint32_t entry = 11U;
    for (uint32_t invalid : {0x20040000U, 0x20044004U, 0x3002FFF0U}) {
        mmio.words[layout.ns_flash_base] = invalid;
        EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry),
                  NX_ERROR_PERMISSION);
    }
    mmio.words[layout.ns_flash_base] = 0x20044000U;
    for (uint32_t invalid :
         {0x08100400U, 0x20040001U, 0x0C0FE001U, 0x08110001U}) {
        mmio.words[layout.ns_flash_base + 4U] = invalid;
        EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry),
                  NX_ERROR_PERMISSION);
    }
    EXPECT_EQ(stack, 9U);
    EXPECT_EQ(entry, 11U);
}
TEST_F(U575Security, ChangedProtectionCannotPassFinalEntryValidation) {
    Prepare();
    uint32_t stack = 9U;
    uint32_t entry = 11U;
    for (auto item : std::vector<std::pair<uintptr_t, uint32_t>>{
             {key(&SAU->CTRL), 0U},
             {key(&MPU_NS->CTRL), 5U},
             {key(&GTZC_MPCBB3_S->CFGLOCKR1), 0U},
             {key(&SCB_NS->VTOR), 0x08100400U}}) {
        const uint32_t old = mmio.words[item.first];
        mmio.words[item.first] = item.second;
        EXPECT_NE(nx_stm32u575_tz_entry(&owner, &stack, &entry), NX_SUCCESS);
        mmio.words[item.first] = old;
    }
}
TEST_F(U575Security, MpuRegionCountIsObservedNotGuessedFromFamilyName) {
    mmio.words[key(&MPU_NS->TYPE)] = 8U << 8U;
    Prepare();
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_READY);
}
TEST_F(U575Security, AttributionAndVectorsAreLockedUntilReset) {
    Prepare();
    EXPECT_EQ(mmio.words[key(&SYSCFG_S->CNSLCKR)], 3U);
    EXPECT_EQ(mmio.words[key(&SYSCFG_S->CSLCKR)], 5U);
    EXPECT_EQ(mmio.words[key(&SCB->AIRCR)] & SCB_AIRCR_BFHFNMINS_Msk, 0U);
}
TEST_F(U575Security, LaunchPublishesRunningBeforeActualWorldTransition) {
    Prepare();
    EXPECT_CALL(memory, Enter(0x20044000U, 0x20040000U, 0x08100401U))
        .WillOnce([&](uint32_t, uint32_t, uint32_t) {
            EXPECT_EQ(owner.state, NX_STM32U575_TZ_RUNNING);
        });
    EXPECT_EQ(nx_stm32u575_tz_launch(&owner), NX_ERROR_STATE);
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_FAILED);
}
TEST_F(U575Security, FloatingPointOwnershipAndLazyStateAreRejected) {
    mmio.control = CONTROL_FPCA_Msk;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    mmio.control = 0U;
    mmio.words[key(&SCB->CPACR)] = 0x00F00000U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_BUSY);
    mmio.words[key(&SCB->CPACR)] = 0U;
    mmio.words[key(&FPU->FPCCR)] = FPU_FPCCR_LSPACT_Msk;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_BUSY);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, SecureFloatingPointActiveBitRejectsBeforeCmseHelper) {
    mmio.control = CONTROL_SFPA_Msk;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_CONTEXT);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, OtherAutonomousBusMasterClocksMustRemainOff) {
    for (auto item : std::vector<std::pair<uintptr_t, uint32_t>>{
             {key(&RCC_S->AHB1ENR), RCC_AHB1ENR_DMA2DEN},
             {key(&RCC_S->AHB2ENR1), RCC_AHB2ENR1_SDMMC1EN},
             {key(&RCC_S->AHB2ENR1), RCC_AHB2ENR1_SDMMC2EN},
             {key(&RCC_S->AHB2ENR1), RCC_AHB2ENR1_OTGEN}}) {
        const uint32_t old = mmio.words[item.first];
        mmio.words[item.first] |= item.second;
        EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_BUSY);
        mmio.words[item.first] = old;
    }
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, SecureOwnerRamMustActuallyBeSecureAndPrivileged) {
    mmio.words[key(&GTZC_MPCBB1_S->SECCFGR[11])] = 0U;
    EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_BUSY);
    EXPECT_EQ(owner.state, NX_STM32U575_TZ_FAILED);
    EXPECT_TRUE(owner.effects);
}
TEST_F(U575Security, SecureRamAndClockAttributionAreSealed) {
    Prepare();
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB1_S->CFGLOCKR1)], 0xFFFU);
    EXPECT_EQ(mmio.words[key(&GTZC_MPCBB1_S->CR)], 1U);
}
TEST_F(U575Security, ReusedOwnerAndUnsupportedMpuCountRejectWithoutWrites) {
    mmio.words[key(&MPU_NS->TYPE)] = 4U << 8U;
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, &layout), NX_ERROR_UNSUPPORTED);
    mmio.words[key(&MPU_NS->TYPE)] = 8U << 8U;
    owner.effects = true;
    EXPECT_EQ(nx_stm32u575_tz_prepare(&owner, &layout), NX_ERROR_STATE);
    EXPECT_TRUE(mmio.writes.empty());
}
TEST_F(U575Security, EmptyAndAliasedOutputsAreRejected) {
    EXPECT_EQ(nx_stm32u575_tz_preflight(nullptr, &layout), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32u575_tz_preflight(&owner, nullptr), NX_ERROR_INVALID);
    Prepare();
    uint32_t word = 17U;
    EXPECT_EQ(nx_stm32u575_tz_entry(&owner, nullptr, &word), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &word, &word), NX_ERROR_INVALID);
    EXPECT_EQ(word, 17U);
}
TEST_F(U575Security, NscAndMpuMetadataTamperRejectsFinalTransition) {
    Prepare();
    uint32_t stack = 9U;
    uint32_t entry = 11U;
    for (auto item : std::vector<std::pair<uintptr_t, uint32_t>>{
             {key(&MPU_NS->MAIR0), 0U},
             {key(&MPU_NS->MAIR1), 0x44U},
             {key(&MPU_NS->RNR), 1U},
             {key(&SYSCFG_S->CNSLCKR), 0U},
             {key(&SYSCFG_S->CSLCKR), 0U}}) {
        const uint32_t old = mmio.words[item.first];
        mmio.words[item.first] = item.second;
        EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry),
                  NX_ERROR_PERMISSION);
        mmio.words[item.first] = old;
    }
    mmio.mpu_base[1] &= ~1U;
    EXPECT_EQ(nx_stm32u575_tz_entry(&owner, &stack, &entry),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(stack, 9U);
    EXPECT_EQ(entry, 11U);
}
TEST_F(U575Security, BusMasterAcquisitionBetweenPrepareAndLaunchIsRejected) {
    Prepare();
    mmio.words[key(&RCC_S->AHB1ENR)] |= RCC_AHB1ENR_DMA2DEN;
    EXPECT_EQ(nx_stm32u575_tz_launch(&owner), NX_ERROR_PERMISSION);
}
TEST_F(U575Security, LaunchRejectsBadVectorsBeforeCrossWorldCall) {
    Prepare();
    mmio.words[layout.ns_flash_base + 44U] = 0x08100801U;
    EXPECT_EQ(nx_stm32u575_tz_launch(&owner), NX_ERROR_PERMISSION);
    EXPECT_NE(owner.state, NX_STM32U575_TZ_RUNNING);
}
} /* namespace */
extern "C" uint32_t nx_u575_tz_model_read(uintptr_t address) {
    ++hardware->reads;
    if (address == key(&SAU->RBAR))
        return hardware->sau_base[hardware->words[key(&SAU->RNR)]];
    if (address == key(&SAU->RLAR))
        return hardware->sau_limit[hardware->words[key(&SAU->RNR)]];
    if (address == key(&MPU_NS->RBAR))
        return hardware->mpu_base[hardware->words[key(&MPU_NS->RNR)]];
    if (address == key(&MPU_NS->RLAR))
        return hardware->mpu_limit[hardware->words[key(&MPU_NS->RNR)]];
    if (address == key(&MPU_NS->RBAR_A1))
        return hardware
            ->mpu_base[(hardware->words[key(&MPU_NS->RNR)] & ~3U) + 1U];
    if (address == key(&MPU_NS->RLAR_A1))
        return hardware
            ->mpu_limit[(hardware->words[key(&MPU_NS->RNR)] & ~3U) + 1U];
    return hardware->words[address];
}
extern "C" void nx_u575_tz_model_write(uintptr_t address, uint32_t value) {
    hardware->changed_writes.push_back(nx_u575_tz_model_read(address) != value);
    hardware->writes.emplace_back(address, value);
    if (hardware->writes.size() - 1U == hardware->refused_write)
        return;
    hardware->words[address] = value;
    if (address == key(&SAU->RBAR))
        hardware->sau_base[hardware->words[key(&SAU->RNR)]] = value;
    if (address == key(&SAU->RLAR))
        hardware->sau_limit[hardware->words[key(&SAU->RNR)]] = value;
    if (address == key(&MPU_NS->RBAR))
        hardware->mpu_base[hardware->words[key(&MPU_NS->RNR)]] = value;
    if (address == key(&MPU_NS->RLAR))
        hardware->mpu_limit[hardware->words[key(&MPU_NS->RNR)]] = value;
}
extern "C" uint32_t nx_u575_tz_model_msp(void) {
    return hardware->msp;
}
extern "C" uint32_t nx_u575_tz_model_control(void) {
    return hardware->control;
}
extern "C" bool nx_u575_tz_model_clear(uint32_t address, uint32_t bytes) {
    return hardware->memory->Clear(address, bytes);
}
extern "C" void nx_u575_tz_model_enter(uint32_t stack, uint32_t limit,
                                       uint32_t entry) {
    hardware->memory->Enter(stack, limit, entry);
}
extern "C" bool nx_arch_is_privileged(void) {
    return hardware->privileged;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return hardware->exception;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return hardware->masks;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
