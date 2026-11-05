/**
 * \file            h747_amp_hardware_test.cpp
 * \brief           Real H747 CMSIS/LL identities and MPU/HSEM readback
 * admission \author          Nexus Team \version         1.0.0 \date 2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h747_amp_hardware_model.h"
#include "hardware.h"
#include "stm32h7xx.h"
#include "stm32h7xx_ll_hsem.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
uint32_t bases[16], attributes[16];
uint32_t selector, mpu_control, mpu_type, cache_control;
bool refuse_selector, refuse_release;
unsigned writes;
constexpr uint32_t mapping = MPU_RASR_XN_Msk | (3U << MPU_RASR_AP_Pos) |
                             (1U << MPU_RASR_TEX_Pos) | MPU_RASR_S_Msk |
                             (11U << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
class H747Hardware : public ::testing::Test {
  protected:
    void SetUp() override {
        for (uintptr_t address : {UINT32_C(0x58024000), UINT32_C(0x58026000)}) {
            void* mapped = mmap(reinterpret_cast<void*>(address), 4096U,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            ASSERT_EQ(mapped, reinterpret_cast<void*>(address));
            std::memset(mapped, 0, 4096U);
        }
        std::memset(bases, 0, sizeof(bases));
        std::memset(attributes, 0, sizeof(attributes));
        refuse_selector = refuse_release = false;
        writes = 0U;
        RCC->AHB4ENR = RCC_AHB4ENR_HSEMEN;
        mpu_control = MPU_CTRL_ENABLE_Msk;
        mpu_type = 8U << MPU_TYPE_DREGION_Pos;
        selector = 2U;
        cache_control = 0U;
        bases[5] = 0x38000000U;
        attributes[5] = mapping;
    }
    bool ready() {
        return nx_h747_amp_hw_ready(reinterpret_cast<void*>(0x38000000U), 4096U,
                                    5U);
    }
};
TEST_F(H747Hardware, ExactSharedNoncacheRegionAndOriginalSelectorAreObserved) {
    EXPECT_TRUE(ready());
    EXPECT_EQ(selector, 2U);
}
TEST_F(H747Hardware,
       DefaultSramAttributesOrMissingClockCannotAuthorizeSharing) {
    mpu_control = 0U;
    EXPECT_FALSE(ready());
    mpu_control = MPU_CTRL_ENABLE_Msk;
    attributes[5] |= MPU_RASR_C_Msk | MPU_RASR_B_Msk;
    EXPECT_FALSE(ready());
    attributes[5] = mapping;
    RCC->AHB4ENR = 0U;
    EXPECT_FALSE(ready());
}
TEST_F(H747Hardware, HigherPriorityOverlapRejectsEvenIfOnlyAPartOverlaps) {
    bases[6] = 0x38000800U;
    attributes[6] = MPU_RASR_ENABLE_Msk | (10U << MPU_RASR_SIZE_Pos);
    EXPECT_FALSE(ready());
    EXPECT_EQ(selector, 2U);
    bases[6] = 0x30000000U;
    EXPECT_TRUE(ready());
}
TEST_F(H747Hardware, PartialOrWrongWindowAndDisabledSubregionsReject) {
    EXPECT_FALSE(
        nx_h747_amp_hw_ready(reinterpret_cast<void*>(0x38000000U), 2048U, 5U));
    EXPECT_FALSE(
        nx_h747_amp_hw_ready(reinterpret_cast<void*>(0x30000000U), 4096U, 5U));
    attributes[5] |= 1U << MPU_RASR_SRD_Pos;
    EXPECT_FALSE(ready());
}
TEST_F(H747Hardware, RefusedMpuSelectorCannotValidateAnotherRegion) {
    refuse_selector = true;
    EXPECT_FALSE(ready());
    EXPECT_EQ(selector, 2U);
}
TEST_F(H747Hardware, MalformedHigherPriorityRegionCannotAuthorizeTheWindow) {
    bases[6] = 0x3800FFC0U;
    attributes[6] = MPU_RASR_ENABLE_Msk | (15U << MPU_RASR_SIZE_Pos);
    EXPECT_FALSE(ready());
    bases[6] = 0x30000000U;
    attributes[6] = MPU_RASR_ENABLE_Msk;
    EXPECT_FALSE(ready());
}
#ifdef CORE_CM7
TEST_F(H747Hardware, EnabledDataCacheWithoutTrustedColdReceiptRejects) {
    cache_control |= SCB_CCR_DC_Msk;
    EXPECT_FALSE(ready());
}
#endif
TEST_F(H747Hardware, OfficialLlUsesExactCoreIdentityAndReadsBackTheClaim) {
    ASSERT_TRUE(nx_h747_amp_hw_take(7U, 9U));
#ifdef CORE_CM7
    EXPECT_EQ(HSEM->R[7], HSEM_R_LOCK | (3U << 8U) | 9U);
#else
    EXPECT_EQ(HSEM->R[7], HSEM_R_LOCK | (1U << 8U) | 9U);
#endif
    unsigned before = writes;
    EXPECT_FALSE(nx_h747_amp_hw_take(7U, 9U));
    EXPECT_EQ(writes, before);
    EXPECT_TRUE(nx_h747_amp_hw_release(7U, 9U));
    EXPECT_EQ(HSEM->R[7] & HSEM_R_LOCK, 0U);
}
TEST_F(H747Hardware, RefusedReleaseRetainsTheOldOwnerUntilActualReadback) {
    ASSERT_TRUE(nx_h747_amp_hw_take(2U, 1U));
    refuse_release = true;
    EXPECT_FALSE(nx_h747_amp_hw_release(2U, 1U));
    EXPECT_NE(HSEM->R[2] & HSEM_R_LOCK, 0U);
    refuse_release = false;
    EXPECT_TRUE(nx_h747_amp_hw_release(2U, 1U));
}
TEST_F(H747Hardware, InvalidSemaphoreAndProcessRejectBeforeAnyWrite) {
    EXPECT_FALSE(nx_h747_amp_hw_take(32U, 1U));
    EXPECT_FALSE(nx_h747_amp_hw_release(32U, 1U));
    EXPECT_FALSE(nx_h747_amp_hw_take(0U, 0U));
    EXPECT_EQ(writes, 0U);
}
} /* namespace */
extern "C" uint32_t nx_h747_amp_model_read(volatile const uint32_t* address) {
    if (address == &MPU->RBAR)
        return bases[selector];
    if (address == &MPU->RASR)
        return attributes[selector];
    if (address == &MPU->RNR)
        return selector;
    if (address == &MPU->CTRL)
        return mpu_control;
    if (address == &MPU->TYPE)
        return mpu_type;
    if (address == &SCB->CCR)
        return cache_control;
    return *address;
}
extern "C" void nx_h747_amp_model_write(volatile uint32_t* address,
                                        uint32_t value) {
    ++writes;
    if (address == &MPU->RNR) {
        if (!refuse_selector)
            selector = value;
        return;
    }
    uintptr_t offset = reinterpret_cast<uintptr_t>(address) -
                       reinterpret_cast<uintptr_t>(&HSEM->R[0]);
    if (offset < 32U * sizeof(uint32_t)) {
        uint32_t old = *address;
        if ((value & HSEM_R_LOCK) != 0U) {
            if ((old & HSEM_R_LOCK) == 0U)
                *address = value;
        } else if (!refuse_release &&
                   old == (value | static_cast<uint32_t>(HSEM_R_LOCK))) {
            *address = 0U;
        }
        return;
    }
    *address = value;
}
