/**
 * \file            stm32_memory_boot_test.cpp
 * \brief           Actual reset-only CCM initialization and terminal rejection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_config.h"
#include "private/stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system;
}

#include <array>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/* Production boot consumes these actual linker addresses, not stub plans. */
asm(".globl __nx_flash_start\n.set __nx_flash_start, 0x08000000\n"
    ".globl __nx_flash_end\n.set __nx_flash_end, 0x08100000\n"
    ".globl __nx_ccm_region_start\n.set __nx_ccm_region_start, 0x10000000\n"
    ".globl __nx_ccm_region_end\n.set __nx_ccm_region_end, 0x10010000\n"
    ".globl __nx_ccm_data_load\n.set __nx_ccm_data_load, 0x08010000\n"
    ".globl __nx_ccm_data_start\n.set __nx_ccm_data_start, 0x10000004\n"
#if defined(NX_STM32_MEMORY_INVALID_PLAN)
    ".globl __nx_ccm_data_end\n.set __nx_ccm_data_end, 0x10010004\n"
#else
    ".globl __nx_ccm_data_end\n.set __nx_ccm_data_end, 0x1000000c\n"
#endif
    ".globl __nx_ccm_bss_start\n.set __nx_ccm_bss_start, 0x10000010\n"
    ".globl __nx_ccm_bss_end\n.set __nx_ccm_bss_end, 0x10000018\n"
    ".globl __nx_ccm_noinit_start\n.set __nx_ccm_noinit_start, 0x1000001c\n"
    ".globl __nx_ccm_noinit_end\n.set __nx_ccm_noinit_end, 0x10000028\n");

namespace {
constexpr uintptr_t ccm = 0x10000000U;
constexpr size_t ccm_size = 0x10000U;
bool refuse_clock;
unsigned barrier_calls;

class CcmBootFixture : public testing::Test {
  protected:
    struct Mapping {
        uintptr_t address;
        size_t size;
        bool owned;
    };
    Mapping mappings[3] = {{0x40000000U, 0x40000U, false},
                           {0x08000000U, 0x100000U, false},
                           {ccm, ccm_size, false}};

    void SetUp() override {
        for (auto& mapping : mappings) {
            void* value =
                mmap(reinterpret_cast<void*>(mapping.address), mapping.size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (value != reinterpret_cast<void*>(mapping.address) &&
                value != MAP_FAILED) {
                munmap(value, mapping.size);
            }
            ASSERT_EQ(value, reinterpret_cast<void*>(mapping.address));
            mapping.owned = true;
        }
        std::memset(reinterpret_cast<void*>(ccm), 0xA5, ccm_size);
        auto* flash = reinterpret_cast<uint32_t*>(0x08010000U);
        flash[0] = 11U;
        flash[1] = 12U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
        /* Poison main provider state: reset must not depend on its contents. */
        std::memset(&g_nx_stm32_system, 0x5A, sizeof(g_nx_stm32_system));
        refuse_clock = false;
        barrier_calls = 0U;
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

    static uint32_t word(size_t index) {
        return reinterpret_cast<const uint32_t*>(ccm)[index];
    }

    void terminal_failure(unsigned expected) {
        pid_t child = fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            SystemInit();
            _exit(
                90); /* A rejected reset must never reach constructors/main. */
        }
        int status = 0;
        ASSERT_EQ(waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), static_cast<int>(expected));
    }
};

TEST_F(CcmBootFixture, InitializesOnlyExplicitlySelectedCcmAtReset) {
#if defined(NX_STM32_MEMORY_INVALID_PLAN)
    terminal_failure(2U);
#else
    std::array<unsigned char, sizeof(g_nx_stm32_system)> original{};
    std::memcpy(original.data(), &g_nx_stm32_system, original.size());
    SystemInit();
#if NEXUS_MEMORY_CCM == 1
    EXPECT_EQ(word(1U), 11U);
    EXPECT_EQ(word(2U), 12U);
    EXPECT_EQ(word(4U), 0U);
    EXPECT_EQ(word(5U), 0U);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN, 0U);
    EXPECT_GT(barrier_calls, 0U);
#else
    for (size_t index : {1U, 2U, 4U, 5U}) {
        EXPECT_EQ(word(index), 0xA5A5A5A5U);
    }
    EXPECT_EQ(RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN, 0U);
    EXPECT_EQ(barrier_calls, 0U);
#endif
    EXPECT_EQ(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, RCC_AHB1ENR_GPIOAEN);
    for (size_t index : {0U, 3U, 6U, 7U, 8U, 9U, 10U, 11U}) {
        EXPECT_EQ(word(index), 0xA5A5A5A5U);
    }
    EXPECT_EQ(std::memcmp(&g_nx_stm32_system, original.data(), original.size()),
              0);
#endif
}

TEST_F(CcmBootFixture, RefusedClockCannotReturnIntoMainOrModifyCcm) {
#if NEXUS_MEMORY_CCM == 1
    refuse_clock = true;
    terminal_failure(1U);
#else
    SystemInit();
    EXPECT_EQ(barrier_calls, 0U);
    EXPECT_EQ(word(1U), 0xA5A5A5A5U);
#endif
}

TEST_F(CcmBootFixture, IncomingCcmClockIsPreserved) {
    RCC->AHB1ENR |= RCC_AHB1ENR_CCMDATARAMEN;
#if defined(NX_STM32_MEMORY_INVALID_PLAN)
    terminal_failure(2U);
#else
    SystemInit();
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN, 0U);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
#endif
}
} /* namespace */

/** \brief Only the test boundary can emulate a gate that rejects a write. */
extern "C" void nx_stm32_memory_model_barrier(void) {
    ++barrier_calls;
    if (refuse_clock) {
        RCC->AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_CCMDATARAMEN;
    }
}

/** \brief Prove rejection precedes all writes and never resumes reset. */
extern "C" void nx_stm32_memory_model_failure(unsigned code) {
    const auto* words = reinterpret_cast<const uint32_t*>(ccm);
    for (size_t index = 0U; index < ccm_size / sizeof(uint32_t); ++index) {
        if (words[index] != 0xA5A5A5A5U) {
            _exit(91);
        }
    }
    _exit(static_cast<int>(code));
}
