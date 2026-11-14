/**
 * \file            stm32_flash_bank_test.cpp
 * \brief           Exact F479 dual-bank geometry and physical sector pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_basic_cold.h"
#include "stm32f407_provider.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
nx_time_us_t now;
unsigned pulses;
uint32_t encoded_sector;
bool missing_eop, refuse_relock;
bool refuse_cache_disable, refuse_cache_restore;
unsigned cache_resets;
const nx_flash_erase_span_t spans[] = {
    {0U, 0x4000U, 4U},         {0x10000U, 0x10000U, 1U},
    {0x20000U, 0x20000U, 7U},  {0x100000U, 0x4000U, 4U},
    {0x110000U, 0x10000U, 1U}, {0x120000U, 0x20000U, 7U}};
const nx_flash_geometry_t geometry{FLASH_BASE, 0x200000U, 4U, spans, 6U, 0U};
nx_stm32_flash_state_t* model;

void Complete() {
    ++pulses;
    FLASH->SR = missing_eop ? 0U : FLASH_SR_EOP;
}
} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}
extern "C" bool nx_flash_program_model_privileged(void) {
    return true;
}
extern "C" bool nx_flash_program_model_irq_idle(void) {
    return true;
}
extern "C" bool nx_flash_program_model_irq_clear(void) {
    return true;
}
extern "C" bool nx_flash_program_model_irq_active(void) {
    return false;
}
extern "C" bool nx_flash_program_model_irq_enabled(void) {
    return false;
}
extern "C" uint32_t nx_flash_program_model_hpre(void) {
    return 0U;
}
extern "C" void nx_flash_program_model_poll(unsigned kind, void* context) {
    (void)kind;
    (void)context;
}
extern "C" uint32_t nx_flash_program_model_read(const volatile uint32_t* reg) {
    return *reg;
}
extern "C" void nx_flash_program_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg == &FLASH->SR) {
        *reg &= ~value;
        return;
    }
    if (reg == &FLASH->CR && refuse_relock && (value & FLASH_CR_LOCK) != 0U)
        return;
    if (reg == &FLASH->ACR) {
        if ((refuse_cache_disable && (value & FLASH_ACR_DCEN) == 0U) ||
            (refuse_cache_restore && (value & FLASH_ACR_DCEN) != 0U))
            return;
        if ((value & FLASH_ACR_DCRST) != 0U) {
            EXPECT_EQ(0U, FLASH->ACR & FLASH_ACR_DCEN);
            ++cache_resets;
        }
    }
    *reg = value;
    if (reg == &FLASH->KEYR && value == 0xcdef89abU)
        FLASH->CR &= ~static_cast<uint32_t>(FLASH_CR_LOCK);
    if (reg == &FLASH->CR && (value & FLASH_CR_STRT) != 0U) {
        EXPECT_EQ(0U, FLASH->ACR & FLASH_ACR_DCEN);
        encoded_sector = (value & (uint32_t)FLASH_CR_SNB) >> FLASH_CR_SNB_Pos;
        FLASH->CR &= ~static_cast<uint32_t>(FLASH_CR_STRT);
        const uint32_t ordinal =
            encoded_sector >= 16U ? encoded_sector - 4U : encoded_sector;
        if (ordinal >= 24U || (encoded_sector >= 12U && encoded_sector < 16U)) {
            ADD_FAILURE() << "Invalid physical bank sector encoding";
            FLASH->SR = FLASH_SR_OPERR;
            return;
        }
        /* Independent physical bank address decoding, not geometry lookup. */
        const uint32_t local = ordinal % 12U;
        const uint32_t offset = local < 4U    ? local * 0x4000U
                                : local == 4U ? 0x10000U
                                              : (local - 4U) * 0x20000U;
        const uint32_t bytes = local < 4U    ? 0x4000U
                               : local == 4U ? 0x10000U
                                             : 0x20000U;
        std::memset(const_cast<uint8_t*>(model->memory) +
                        (ordinal / 12U) * 0x100000U + offset,
                    0xff, bytes);
        Complete();
    }
}
extern "C" void nx_flash_program_model_store(volatile uint32_t* address,
                                             uint32_t value) {
    EXPECT_EQ(0U, FLASH->ACR & FLASH_ACR_DCEN);
    *address &= value;
    Complete();
}
extern "C" void nx_flash_program_model_store_byte(volatile uint8_t* address,
                                                  uint8_t value) {
    *address &= value;
    Complete();
}

/** \brief Run the production pulse owner with independent bank observations. */
class FlashBanks : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32_flash_state_t owner{};
    const nx_flash_port_t face{&nx_stm32_flash_ops, &owner};
    std::array<uint8_t, 0x200000U> memory{};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(0x40000000U), mapping);
        now = 0U;
        pulses = encoded_sector = 0U;
        missing_eop = refuse_relock = false;
        refuse_cache_disable = refuse_cache_restore = false;
        cache_resets = 0U;
        RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
        FLASH->CR = FLASH_CR_LOCK;
        FLASH->OPTCR = FLASH_OPTCR_OPTLOCK | FLASH_OPTCR_RDP;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_flash_prepare(&owner, &geometry, 3300U));
        owner.memory = memory.data();
        model = &owner;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};

TEST_F(FlashBanks, ExactBankTwoSectorsPreserveBankOneAndAdjacentSector) {
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(16U, encoded_sector);
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(0U, memory[0U]);
    EXPECT_EQ(0U, memory[0xfffffU]);
    EXPECT_EQ(0xffU, memory[0x100000U]);
    EXPECT_EQ(0xffU, memory[0x103fffU]);
    EXPECT_EQ(0U, memory[0x104000U]);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 0x1e0000U, 0x20000U, 10000U));
    EXPECT_EQ(27U, encoded_sector);
    EXPECT_EQ(0xffU, memory.back());
    EXPECT_FALSE(owner.active);
}

TEST_F(FlashBanks, CrossBankEraseUsesTwoPhysicalSectorEncodings) {
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 0xe0000U, 0x24000U, 10000U));
    EXPECT_EQ(2U, pulses);
    EXPECT_EQ(16U, encoded_sector);
    EXPECT_EQ(0U, memory[0xdffffU]);
    EXPECT_EQ(0xffU, memory[0xfffffU]);
    EXPECT_EQ(0xffU, memory[0x103fffU]);
    EXPECT_EQ(0U, memory[0x104000U]);
}

TEST_F(FlashBanks, ProgramAndReadReachTheLastPhysicalBankWord) {
    memory.fill(0xffU);
    const uint32_t word = 0x12345678U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0x1ffffcU, &word,
                                                sizeof(word), 10000U));
    uint32_t output = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_read(&face, 0x1ffffcU, &output, sizeof(output)));
    EXPECT_EQ(word, output);
    EXPECT_EQ(1U, pulses);
}

TEST_F(FlashBanks, SwappedBanksRejectBeforeUnlockOrDestinationAccess) {
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_UFB_MODE;
    uint32_t output = 0xa5U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_read(&face, 0U, &output, sizeof(output)));
    EXPECT_EQ(0xa5U, output);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(0U, FLASH->KEYR);
    EXPECT_FALSE(owner.active);
}

TEST_F(FlashBanks, ForeignBootOrProtectionTopologyNeverStartsPulse) {
    for (uint32_t option :
         {uint32_t{FLASH_OPTCR_BFB2}, uint32_t{FLASH_OPTCR_SPRMOD}}) {
        FLASH->OPTCR = option;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED,
                  nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
        EXPECT_EQ(0U, FLASH->KEYR);
        EXPECT_EQ(0U, pulses);
    }
}

TEST_F(FlashBanks, MissingMappingClockDoesNotBorrowOrAcquireIt) {
    RCC->APB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, FLASH->KEYR);
    EXPECT_EQ(0U, pulses);
}

TEST_F(FlashBanks, MissingEndAndFailedRelockPreserveExistingPulseContracts) {
    missing_eop = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_FALSE(owner.active);
    missing_eop = false;
    refuse_relock = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0x104000U, 0x4000U, 10000U));
    EXPECT_TRUE(owner.active);
    refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&owner));
    EXPECT_FALSE(owner.active);
}

TEST_F(FlashBanks, RejectsOneBankGeometryAndUnreviewedAdvancedMaintenance) {
    nx_stm32_flash_state_t cold{};
    auto invalid = geometry;
    invalid.size = 0x100000U;
    invalid.erase_span_count = 3U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_flash_prepare(&cold, &invalid, 3300U));
    EXPECT_EQ(nullptr, cold.registers);
    nx_flash_maintenance_snapshot_t snapshot{};
    const nx_flash_maintenance_port_t maintenance{
        &nx_stm32_flash_maintenance_ops, &owner};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_maintenance_snapshot(&maintenance, &snapshot));
    EXPECT_EQ(0U, FLASH->OPTKEYR);
}

TEST_F(FlashBanks, ReadWhileWriteDisablesDataCacheBeforeBankTwoPulse) {
    const uint32_t acr = FLASH_ACR_ICEN | FLASH_ACR_DCEN | 5U;
    FLASH->ACR = acr;
    memory.fill(0xffU);
    const uint32_t word = 0x12345678U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0x100000U, &word,
                                                sizeof(word), 10000U));
    EXPECT_EQ(acr, FLASH->ACR);
    EXPECT_EQ(1U, cache_resets);
    EXPECT_FALSE(owner.active);
}

TEST_F(FlashBanks, CacheDisableFailureNeverStartsAndRetainsCleanup) {
    FLASH->ACR = FLASH_ACR_DCEN | 5U;
    refuse_cache_disable = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(0U, pulses);
    EXPECT_TRUE(owner.active);
    EXPECT_EQ(0U, cache_resets);
    refuse_cache_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&owner));
    EXPECT_FALSE(owner.active);
    EXPECT_EQ(FLASH_ACR_DCEN | 5U, FLASH->ACR);
}

TEST_F(FlashBanks, CacheRestoreFailureKeepsItsOriginalLeaseUntilRecovery) {
    FLASH->ACR = FLASH_ACR_DCEN | 5U;
    refuse_cache_restore = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_TRUE(owner.active);
    EXPECT_EQ(0U, FLASH->ACR & FLASH_ACR_DCEN);
    refuse_cache_restore = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&owner));
    EXPECT_EQ(FLASH_ACR_DCEN | 5U, FLASH->ACR);
    EXPECT_FALSE(owner.active);
    EXPECT_EQ(1U, pulses);
}

TEST_F(FlashBanks, AnInitiallyDisabledDataCacheIsNeverEnabledByPulses) {
    FLASH->ACR = FLASH_ACR_ICEN | 5U;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(FLASH_ACR_ICEN | 5U, FLASH->ACR);
    EXPECT_EQ(1U, cache_resets);
}

TEST_F(FlashBanks, ForeignCacheResetNeverBecomesThisPulseOwnersEffect) {
    FLASH->ACR = FLASH_ACR_DCRST | 5U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, 0x100000U, 0x4000U, 10000U));
    EXPECT_EQ(0U, FLASH->KEYR);
    EXPECT_EQ(0U, pulses);
    EXPECT_FALSE(owner.active);
    EXPECT_EQ(FLASH_ACR_DCRST | 5U, FLASH->ACR);
}
