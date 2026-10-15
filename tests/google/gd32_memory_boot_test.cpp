/**
 * \file            gd32_memory_boot_test.cpp
 * \brief           Actual SystemInit optional memory and incoming clock model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f4xx.h"
#include "nexus_config.h"
#include "private/system.h"
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
extern const uint32_t __gVectors[] = {0U};

/** \brief Fixed incoming reset clock is already stable; no hidden worker. */
void nx_gd32_model_clock_poll(void) {
    RCU_CTL |= RCU_CTL_IRC16MSTB;
    RCU_CFG0 &= ~(uint32_t)RCU_CFG0_SCSS;
}

/** \brief Unused clock-start dependencies retain exact SDK signatures. */
uint32_t rcu_clock_freq_get(rcu_clock_freq_enum clock) {
    (void)clock;
    return 16000000U;
}

/** \brief Unused clock profile dependency has no simulated initialization. */
void fmc_wscnt_set(uint32_t states) {
    (void)states;
}

/** \brief The reset-only model never starts the platform timer profile. */
void rcu_timer_clock_prescaler_config(uint32_t divider) {
    (void)divider;
}
}

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>

/* Actual absolute linker-symbol addresses, not mocked initializer functions. */
asm(".globl __nx_flash_start\n.set __nx_flash_start, 0x08000000\n"
    ".globl __nx_flash_end\n.set __nx_flash_end, 0x08100000\n"
    ".globl __nx_addsram_region_start\n"
    ".set __nx_addsram_region_start, 0x20030000\n"
    ".globl __nx_addsram_region_end\n"
    ".set __nx_addsram_region_end, 0x20070000\n"
    ".globl __nx_tcm_region_start\n"
    ".set __nx_tcm_region_start, 0x10000000\n"
    ".globl __nx_tcm_region_end\n"
    ".set __nx_tcm_region_end, 0x10010000\n"
    ".globl __nx_addsram_data_load\n"
    ".set __nx_addsram_data_load, 0x08010000\n"
    ".globl __nx_addsram_data_start\n"
    ".set __nx_addsram_data_start, 0x20030004\n"
    ".globl __nx_addsram_data_end\n"
    ".set __nx_addsram_data_end, 0x2003000c\n"
    ".globl __nx_addsram_bss_start\n"
    ".set __nx_addsram_bss_start, 0x20030010\n"
    ".globl __nx_addsram_bss_end\n"
    ".set __nx_addsram_bss_end, 0x20030018\n"
    ".globl __nx_addsram_noinit_start\n"
    ".set __nx_addsram_noinit_start, 0x2003001c\n"
    ".globl __nx_addsram_noinit_end\n"
    ".set __nx_addsram_noinit_end, 0x20030028\n"
    ".globl __nx_tcm_data_load\n.set __nx_tcm_data_load, 0x08010008\n"
    ".globl __nx_tcm_data_start\n.set __nx_tcm_data_start, 0x10000004\n"
    ".globl __nx_tcm_data_end\n.set __nx_tcm_data_end, 0x1000000c\n"
    ".globl __nx_tcm_bss_start\n.set __nx_tcm_bss_start, 0x10000010\n"
    ".globl __nx_tcm_bss_end\n.set __nx_tcm_bss_end, 0x10000018\n"
    ".globl __nx_tcm_noinit_start\n.set __nx_tcm_noinit_start, 0x1000001c\n"
    ".globl __nx_tcm_noinit_end\n.set __nx_tcm_noinit_end, 0x10000028\n");

namespace {
class MemoryBootFixture : public testing::Test {
  protected:
    static constexpr uintptr_t flash = 0x08000000U;
    static constexpr uintptr_t addsram = 0x20030000U;
    static constexpr uintptr_t tcm = 0x10000000U;

    struct Mapping {
        uintptr_t address;
        size_t size;
        bool owned;
    };
    Mapping mappings[4] = {{0x40000000U, 0x40000U, false},
                           {flash, 0x100000U, false},
                           {addsram, 0x40000U, false},
                           {tcm, 0x10000U, false}};

    static bool reserve_exact(uintptr_t address, size_t size) {
        void* mapping =
            mmap(reinterpret_cast<void*>(address), size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapping == reinterpret_cast<void*>(address)) {
            return true;
        }
        /* Kernels without NOREPLACE may ignore it and choose another address.
         */
        if (mapping != MAP_FAILED) {
            munmap(mapping, size);
        }
        return false;
    }

    void SetUp() override {
        /* Admission failure never permits a fill or teardown of foreign RAM.
         */
        for (auto& mapping : mappings) {
            ASSERT_TRUE(reserve_exact(mapping.address, mapping.size));
            mapping.owned = true;
        }
        std::memset(reinterpret_cast<void*>(addsram), 0xA5, 0x40000U);
        std::memset(reinterpret_cast<void*>(tcm), 0x5A, 0x10000U);
        auto* source = reinterpret_cast<uint32_t*>(flash + 0x10000U);
        for (uint32_t index = 0U; index < 4U; ++index) {
            source[index] = 11U + index;
        }
        RCU_CTL = RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB;
        RCU_AHB1EN = RCU_AHB1EN_PAEN;
        g_gd32_model_scb = {};
    }

    void TearDown() override {
        for (auto& mapping : mappings) {
            if (mapping.owned) {
                EXPECT_EQ(munmap(reinterpret_cast<void*>(mapping.address),
                                 mapping.size),
                          0);
                mapping.owned = false;
            }
        }
    }

    static uint32_t word(uintptr_t address, size_t index) {
        return reinterpret_cast<const uint32_t*>(address)[index];
    }
};

TEST_F(MemoryBootFixture, OccupiedAddressKeepsItsOriginalOwnerAndContents) {
    constexpr size_t page = 4096U;
    void* existing = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(existing, MAP_FAILED);
    auto* value = static_cast<uint32_t*>(existing);
    *value = 0x13579BDFU;
    EXPECT_FALSE(reserve_exact(reinterpret_cast<uintptr_t>(existing), page));
    EXPECT_EQ(*value, 0x13579BDFU);
    EXPECT_EQ(munmap(existing, page), 0);
}

TEST_F(MemoryBootFixture, HostImageAndHeapStayAboveEveryHardwareWindow) {
    constexpr uintptr_t host_base = 0x60000000U;
    void* heap = sbrk(0);
    ASSERT_NE(heap, reinterpret_cast<void*>(-1));
    EXPECT_GE(reinterpret_cast<uintptr_t>(&g_gd32_model_scb), host_base);
    EXPECT_GE(reinterpret_cast<uintptr_t>(heap), host_base);
}

TEST_F(MemoryBootFixture, ResetInitializesOnlyAuthoredRegions) {
    SystemInit();
#if NEXUS_MEMORY_ADDSRAM == 1
    EXPECT_EQ(word(addsram, 1U), 11U);
    EXPECT_EQ(word(addsram, 2U), 12U);
    EXPECT_EQ(word(addsram, 4U), 0U);
    EXPECT_EQ(word(addsram, 5U), 0U);
#else
    for (size_t index : {1U, 2U, 4U, 5U}) {
        EXPECT_EQ(word(addsram, index), 0xA5A5A5A5U);
    }
#endif
#if NEXUS_MEMORY_TCM == 1
    EXPECT_EQ(word(tcm, 1U), 13U);
    EXPECT_EQ(word(tcm, 2U), 14U);
    EXPECT_EQ(word(tcm, 4U), 0U);
    EXPECT_EQ(word(tcm, 5U), 0U);
    EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_TCMSRAMEN, 0U);
#else
    for (size_t index : {1U, 2U, 4U, 5U}) {
        EXPECT_EQ(word(tcm, index), 0x5A5A5A5AU);
    }
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_TCMSRAMEN, 0U);
#endif
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_PAEN, RCU_AHB1EN_PAEN);
    for (size_t index : {0U, 3U, 6U, 7U, 8U, 9U, 10U}) {
        EXPECT_EQ(word(addsram, index), 0xA5A5A5A5U);
        EXPECT_EQ(word(tcm, index), 0x5A5A5A5AU);
    }
}

TEST_F(MemoryBootFixture, IncomingTcmClockIsNeverDisabledByReset) {
    RCU_AHB1EN |= RCU_AHB1EN_TCMSRAMEN;
    SystemInit();
    EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_TCMSRAMEN, 0U);
    EXPECT_EQ(RCU_AHB1EN & RCU_AHB1EN_PAEN, RCU_AHB1EN_PAEN);
}
} /* namespace */
