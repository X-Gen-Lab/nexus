/**
 * \file            gd32_official_gpio_test.cpp
 * \brief           Real GD32 library calls preserve owned GPIO boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
NVIC_Type g_gd32_model_nvic;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

class Gd32OfficialGpio : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
};

TEST_F(Gd32OfficialGpio, ProductionInitializationUsesRealMaskedLibraryWrites) {
    nx_gd32_gpio_state_t state = {};
    GPIO_CTL(GPIOA) = UINT32_C(0xAAAAAAAA);
    GPIO_PUD(GPIOA) = UINT32_C(0x55555555);
    GPIO_OMODE(GPIOA) = UINT32_C(0xFFFF);
    GPIO_OSPD(GPIOA) = UINT32_C(0xFFFFFFFF);
    ASSERT_EQ(nx_gd32_gpio_initialize(&state, 0U, GPIO_PIN_3 | GPIO_PIN_4,
                                      GPIO_PIN_3, true),
              NX_SUCCESS);
    const uint32_t fields = (3U << 6U) | (3U << 8U);
    EXPECT_EQ(GPIO_CTL(GPIOA),
              (UINT32_C(0xAAAAAAAA) & ~fields) | (1U << 6U) | (1U << 8U));
    EXPECT_EQ(GPIO_PUD(GPIOA), UINT32_C(0x55555555) & ~fields);
    EXPECT_EQ(GPIO_OSPD(GPIOA), UINT32_C(0xFFFFFFFF) & ~fields);
    EXPECT_EQ(GPIO_OMODE(GPIOA), UINT32_C(0xFFFF) & ~(GPIO_PIN_3 | GPIO_PIN_4));
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_3 | (GPIO_PIN_4 << 16U));
    EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_PAEN);
}

TEST_F(Gd32OfficialGpio, IndependentBanksAndDisjointPinsKeepTheirOwnState) {
    nx_gd32_gpio_state_t first = {};
    nx_gd32_gpio_state_t second = {};
    nx_gd32_gpio_state_t third = {};
    ASSERT_EQ(nx_gd32_gpio_initialize(&first, 0U, GPIO_PIN_0, GPIO_PIN_0, true),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_gpio_initialize(&second, 1U, GPIO_PIN_7, 0U, false),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_gpio_initialize(&third, 0U, GPIO_PIN_4, 0U, true),
              NX_SUCCESS);
    EXPECT_EQ(GPIO_CTL(GPIOA), (1U << 0U) | (1U << 8U));
    EXPECT_EQ(GPIO_CTL(GPIOB), 0U);
    EXPECT_EQ(first.mask, GPIO_PIN_0);
    EXPECT_EQ(second.mask, GPIO_PIN_7);
    EXPECT_EQ(third.mask, GPIO_PIN_4);
    EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_PAEN | RCU_AHB1EN_PBEN);
    ASSERT_EQ(nx_gd32_gpio_stop(&third, 0U), NX_SUCCESS);
    EXPECT_EQ(first.mask, GPIO_PIN_0);
    EXPECT_EQ(second.mask, GPIO_PIN_7);
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_4 << 16U);
}

TEST_F(Gd32OfficialGpio, RejectedMaskCannotReachVendorOrClockRegisters) {
    nx_gd32_gpio_state_t state = {};
    GPIO_CTL(GPIOA) = UINT32_C(0xA5A55A5A);
    GPIO_BOP(GPIOA) = UINT32_C(0x12345678);
    const nx_gd32_gpio_state_t before = state;
    EXPECT_EQ(nx_gd32_gpio_initialize(&state, 0U, UINT32_C(0x10000), 0U, true),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_gd32_gpio_initialize(&state, 9U, GPIO_PIN_0, 0U, true),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_gd32_gpio_initialize(&state, 0U, GPIO_PIN_0, GPIO_PIN_1, true),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
    EXPECT_EQ(GPIO_CTL(GPIOA), UINT32_C(0xA5A55A5A));
    EXPECT_EQ(GPIO_BOP(GPIOA), UINT32_C(0x12345678));
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32OfficialGpio, ReadsOneMaskedSnapshotAndRejectsAliasedOutput) {
    nx_gd32_gpio_state_t state = {};
    ASSERT_EQ(
        nx_gd32_gpio_initialize(&state, 1U, GPIO_PIN_2 | GPIO_PIN_5, 0U, false),
        NX_SUCCESS);
    GPIO_ISTAT(GPIOB) = UINT32_C(0xFFFF0000) | GPIO_PIN_2 | GPIO_PIN_7;
    uint32_t value = UINT32_MAX;
    ASSERT_EQ(nx_gd32_gpio_read(&state, &value), NX_SUCCESS);
    EXPECT_EQ(value, GPIO_PIN_2);
    const nx_gd32_gpio_state_t before = state;
    EXPECT_EQ(nx_gd32_gpio_read(&state, &state.mask), NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_gd32_gpio_read(&state, const_cast<uint32_t*>(&GPIO_ISTAT(GPIOB))),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
    EXPECT_EQ(GPIO_ISTAT(GPIOB),
              UINT32_C(0xFFFF0000) | GPIO_PIN_2 | GPIO_PIN_7);
}

TEST_F(Gd32OfficialGpio, ToggleUsesOutputSnapshotAndOneCombinedLatchWrite) {
    nx_gd32_gpio_state_t state = {};
    ASSERT_EQ(
        nx_gd32_gpio_initialize(&state, 0U, GPIO_PIN_2 | GPIO_PIN_5, 0U, true),
        NX_SUCCESS);
    GPIO_OCTL(GPIOA) = UINT32_C(0xFFFF0000) | GPIO_PIN_2 | GPIO_PIN_7;
    ASSERT_EQ(nx_gd32_gpio_toggle(&state, GPIO_PIN_2 | GPIO_PIN_5), NX_SUCCESS);
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_5 | (GPIO_PIN_2 << 16U));
    const uint32_t before = GPIO_BOP(GPIOA);
    EXPECT_EQ(nx_gd32_gpio_toggle(&state, GPIO_PIN_7), NX_ERROR_PERMISSION);
    EXPECT_EQ(GPIO_BOP(GPIOA), before);
}
