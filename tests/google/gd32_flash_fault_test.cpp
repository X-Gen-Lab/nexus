/**
 * \file            gd32_flash_fault_test.cpp
 *
 * \brief           Flash relock and real NVIC model ownership regressions.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
extern bool g_gd32_model_flash_lock_works;
extern uint32_t g_gd32_model_pulses;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" void USART0_IRQHandler(void) {
}

namespace {
TEST(GD32NvicModel, QueriesKeepEnablePendingAndActiveAsIndependentFacts) {
    g_gd32_model_nvic = {};
    NVIC->ISER[0] = 1U << 4U;
    NVIC->ISPR[1] = 1U << 5U;
    NVIC->IABR[1] = 1U << 5U;
    EXPECT_EQ(1U, NVIC_GetEnableIRQ(FMC_IRQn));
    EXPECT_EQ(0U, NVIC_GetPendingIRQ(FMC_IRQn));
    EXPECT_EQ(0U, NVIC_GetActive(FMC_IRQn));
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(USART0_IRQn));
    EXPECT_EQ(1U, NVIC_GetPendingIRQ(USART0_IRQn));
    EXPECT_EQ(1U, NVIC_GetActive(USART0_IRQn));
    g_gd32_model_nvic = {};
}
TEST(GD32NvicModel, PendingClearAndDisableCannotClearAnActiveHandler) {
    g_gd32_model_nvic = {};
    NVIC->ISER[0] = (1U << 4U) | (1U << 7U);
    NVIC->ISPR[0] = (1U << 4U) | (1U << 7U);
    NVIC->IABR[0] = 1U << 4U;
    NVIC_ClearPendingIRQ(FMC_IRQn);
    EXPECT_EQ(1U << 7U, NVIC->ISPR[0]);
    EXPECT_EQ(1U, NVIC_GetEnableIRQ(FMC_IRQn));
    EXPECT_EQ(1U, NVIC_GetActive(FMC_IRQn));
    NVIC_DisableIRQ(FMC_IRQn);
    EXPECT_EQ(1U << 7U, NVIC->ISER[0]);
    EXPECT_EQ(1U, NVIC_GetActive(FMC_IRQn));
    EXPECT_EQ(1U << 7U, NVIC->ISPR[0]);
    g_gd32_model_nvic = {};
}
/** \brief Model real MMIO and Flash addresses without a firmware reset stub. */
class GD32FlashFault : public ::testing::Test {
  protected:
    void SetUp() override {
        peripheral = map(0x40000000U, 0x80000U);
        flash = map(0x08000000U, 0x100000U);
        identity = map(0x1FFF7000U, 0x2000U);
        ASSERT_NE(peripheral, MAP_FAILED);
        ASSERT_NE(flash, MAP_FAILED);
        ASSERT_NE(identity, MAP_FAILED);
        std::memset(flash, 0xFF, 0x100000U);
        *reinterpret_cast<volatile uint32_t*>(UINT32_C(0x1FFF7A20)) = 1024U
                                                                      << 16U;
        g_gd32_model_flash_lock_works = true;
        g_gd32_model_pulses = 0U;
    }
    void TearDown() override {
        if (peripheral != MAP_FAILED) {
            EXPECT_EQ(munmap(peripheral, 0x80000U), 0);
        }
        if (flash != MAP_FAILED) {
            EXPECT_EQ(munmap(flash, 0x100000U), 0);
        }
        if (identity != MAP_FAILED) {
            EXPECT_EQ(munmap(identity, 0x2000U), 0);
        }
    }
    static void* map(uintptr_t address, size_t length) {
        return mmap(reinterpret_cast<void*>(address), length,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    }
    void* peripheral = MAP_FAILED;
    void* flash = MAP_FAILED;
    void* identity = MAP_FAILED;
};

TEST_F(GD32FlashFault, RelockFailureRetainsControllerUntilStopRetryProvesLock) {
    nx_gd32_flash_state_t state = {};
    nx_gd32_flash_state_t replacement = {};
    const nx_flash_port_t port = {&nx_gd32_flash_ops, &state};
    ASSERT_EQ(nx_gd32_flash_initialize(&state), NX_SUCCESS);
    const uint16_t word = 0x1234U;
    g_gd32_model_flash_lock_works = false;
    ASSERT_EQ(nx_flash_port_program(&port, 0U, &word, sizeof(word),
                                    NX_DEADLINE_NEVER),
              NX_ERROR_IO);
    EXPECT_TRUE(state.initialized);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(FMC_CTL & FMC_CTL_LK, 0U);
    EXPECT_EQ(*static_cast<const uint16_t*>(flash), word);
    EXPECT_EQ(g_gd32_model_pulses, 1U);
    uint16_t output = 0;
    EXPECT_NE(nx_flash_port_geometry(&port), nullptr);
    EXPECT_EQ(nx_flash_port_read(&port, 0U, &output, sizeof(output)),
              NX_SUCCESS);
    EXPECT_EQ(output, word);
    EXPECT_EQ(nx_gd32_flash_stop(&state), NX_ERROR_IO);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(nx_gd32_flash_initialize(&replacement), NX_ERROR_BUSY);
    /* No replacement can acquire the retained controller. Ordinary stop must
     * prove source, lock and pending withdrawal before releasing it. */
    g_gd32_model_flash_lock_works = true;
    EXPECT_EQ(nx_gd32_flash_stop(&state), NX_SUCCESS);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(NVIC_GetPendingIRQ(FMC_IRQn), 0U);
    EXPECT_EQ(NVIC_GetActive(FMC_IRQn), 0U);
    EXPECT_EQ(nx_gd32_flash_initialize(&replacement), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_flash_stop(&replacement), NX_SUCCESS);
    EXPECT_EQ(g_gd32_model_pulses, 1U);
}
} /* namespace */
