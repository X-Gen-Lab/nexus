/**
 * \file            stm32_family_irq_banks_test.cpp
 * \brief           Actual F446 selected-vector admission through all 97 slots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_platform_cold.h"
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
unsigned writes;
class F446IrqBanks : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    nx_stm32_platform_config_t config{};
    static constexpr size_t bank = static_cast<unsigned>(FMPI2C1_ER_IRQn) / 32U;
    static constexpr uint32_t bit =
        1U << (static_cast<unsigned>(FMPI2C1_ER_IRQn) % 32U);
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(0x40000000U));
        g_f4_model_nvic = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        writes = 0U;
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(munmap(mmio, 0x80000U), 0);
        }
    }
    template <size_t Words> bool Select(uint32_t (&mask)[Words]) {
        if constexpr (bank < Words) {
            mask[bank] = bit;
            return true;
        }
        return false;
    }
};
} /* namespace */

TEST_F(F446IrqBanks, StaticMaskStorageCoversExactlyTheSelectedIrqSpan) {
    EXPECT_EQ(sizeof(config.irq_masks) / sizeof(config.irq_masks[0]), 4U);
    EXPECT_EQ(static_cast<unsigned>(FMPI2C1_ER_IRQn), 96U);
}

TEST_F(F446IrqBanks,
       EveryForeignSelectedStateInFourthBankRejectsWithoutWrites) {
    ASSERT_TRUE(Select(config.irq_masks));
    for (auto* status : {NVIC->ISER, NVIC->ISPR, NVIC->IABR}) {
        status[bank] = bit;
        EXPECT_EQ(nx_stm32_platform_selected_preflight(&config), NX_ERROR_BUSY);
        EXPECT_EQ(writes, 0U);
        status[bank] = 0U;
    }
    EXPECT_EQ(nx_stm32_platform_selected_preflight(&config), NX_SUCCESS);
}

TEST_F(F446IrqBanks, UnselectedFourthBankStateDoesNotBecomeAnOwnedVector) {
    NVIC->ISPR[bank] = bit;
    EXPECT_EQ(nx_stm32_platform_selected_preflight(&config), NX_SUCCESS);
    EXPECT_EQ(writes, 0U);
}

TEST_F(F446IrqBanks, NeighboringFourthBankBitsRemainIndependent) {
    ASSERT_TRUE(Select(config.irq_masks));
    NVIC->IABR[bank] = bit << 1U;
    EXPECT_EQ(nx_stm32_platform_selected_preflight(&config), NX_SUCCESS);
    EXPECT_EQ(writes, 0U);
}

extern "C" void nx_family_platform_write(volatile uint32_t* reg,
                                         uint32_t value) {
    ++writes;
    *reg = value;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return {0U};
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t) {
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef*, uint8_t) {
    ADD_FAILURE() << "unselected pad accessed";
    return 0U;
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef*, uint8_t, uint16_t) {
    ADD_FAILURE() << "unselected pad restored";
}
