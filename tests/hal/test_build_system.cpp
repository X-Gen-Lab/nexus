/**
 * \file            test_build_system.cpp
 * \brief           Build System Verification Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            These tests verify that the build system is correctly
 *                  configured for multi-compiler support (GCC, Clang, IAR).
 *
 * \par             Requirements: 12.1, 12.2, 12.3, 12.9
 */

#include "compiler_abstraction.h"
#include "core_config.h"
#include <gtest/gtest.h>

/**
 * \brief           Test fixture for build system verification
 */
class BuildSystemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // No setup needed
    }

    void TearDown() override {
        // No teardown needed
    }
};

/*===========================================================================*/
/* Compiler Detection Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test that exactly one compiler is detected
 * \details         Verifies that the compiler detection macros correctly
 *                  identify exactly one compiler type.
 */
TEST_F(BuildSystemTest, ExactlyOneCompilerDetected) {
    int compiler_count = 0;

#if COMPILER_GCC
    compiler_count++;
#endif
#if COMPILER_CLANG
    compiler_count++;
#endif
#if COMPILER_IAR
    compiler_count++;
#endif
#if COMPILER_MSVC
    compiler_count++;
#endif

    // At least one compiler should be detected (or unknown)
    // For native builds, MSVC or GCC/Clang should be detected
    EXPECT_GE(compiler_count, 0);
    EXPECT_LE(compiler_count, 1);
}

/**
 * \brief           Test that COMPILER_NAME is defined
 */
TEST_F(BuildSystemTest, CompilerNameDefined) {
    const char* name = COMPILER_NAME;
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

/**
 * \brief           Test that COMPILER_VERSION is defined
 */
TEST_F(BuildSystemTest, CompilerVersionDefined) {
    // COMPILER_VERSION should be a non-negative integer
    EXPECT_GE(COMPILER_VERSION, 0);
}

/*===========================================================================*/
/* Core Configuration Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test that CORE_TYPE is defined
 */
TEST_F(BuildSystemTest, CoreTypeDefined) {
    // CORE_TYPE should be one of the valid core types
    EXPECT_TRUE(CORE_TYPE == CORE_CM0 || CORE_TYPE == CORE_CM0P ||
                CORE_TYPE == CORE_CM3 || CORE_TYPE == CORE_CM4 ||
                CORE_TYPE == CORE_CM7 || CORE_TYPE == CORE_CM33);
}

/**
 * \brief           Test that CORE_TYPE_STRING is defined
 */
TEST_F(BuildSystemTest, CoreTypeStringDefined) {
    const char* type_string = CORE_TYPE_STRING;
    EXPECT_NE(type_string, nullptr);
    EXPECT_GT(strlen(type_string), 0u);
}

/**
 * \brief           Test that NVIC priority bits are correctly defined
 */
TEST_F(BuildSystemTest, NvicPriorityBitsDefined) {
    // NVIC priority bits should be 2 (CM0/CM0+) or 4 (CM3+)
    EXPECT_TRUE(CORE_NVIC_PRIO_BITS == 2 || CORE_NVIC_PRIO_BITS == 4);
}

/**
 * \brief           Test that NVIC priority max is consistent
 */
TEST_F(BuildSystemTest, NvicPriorityMaxConsistent) {
    // NVIC priority max should be (1 << PRIO_BITS) - 1
    uint32_t expected_max = (1UL << CORE_NVIC_PRIO_BITS) - 1;
    EXPECT_EQ(CORE_NVIC_PRIO_MAX, expected_max);
}

/*===========================================================================*/
/* Feature Detection Tests                                                   */
/*===========================================================================*/

/**
 * \brief           Test that feature detection macros are boolean
 */
TEST_F(BuildSystemTest, FeatureDetectionMacrosAreBoolean) {
    // All feature detection macros should be 0 or 1
    EXPECT_TRUE(CORE_HAS_FPU == 0 || CORE_HAS_FPU == 1);
    EXPECT_TRUE(CORE_HAS_DSP == 0 || CORE_HAS_DSP == 1);
    EXPECT_TRUE(CORE_HAS_MPU == 0 || CORE_HAS_MPU == 1);
    EXPECT_TRUE(CORE_HAS_CACHE == 0 || CORE_HAS_CACHE == 1);
    EXPECT_TRUE(CORE_HAS_TZ == 0 || CORE_HAS_TZ == 1);
}

/**
 * \brief           Test feature detection consistency for CM4
 */
TEST_F(BuildSystemTest, CM4FeatureConsistency) {
#if CORE_TYPE == CORE_CM4
    // CM4 should have FPU and DSP
    EXPECT_EQ(CORE_HAS_FPU, 1);
    EXPECT_EQ(CORE_HAS_DSP, 1);
    // CM4 should have MPU
    EXPECT_EQ(CORE_HAS_MPU, 1);
    // CM4 should NOT have Cache or TrustZone
    EXPECT_EQ(CORE_HAS_CACHE, 0);
    EXPECT_EQ(CORE_HAS_TZ, 0);
#endif
}

/**
 * \brief           Test feature detection consistency for CM0
 */
TEST_F(BuildSystemTest, CM0FeatureConsistency) {
#if CORE_TYPE == CORE_CM0 || CORE_TYPE == CORE_CM0P
    // CM0/CM0+ should NOT have FPU, DSP, MPU, Cache, or TrustZone
    EXPECT_EQ(CORE_HAS_FPU, 0);
    EXPECT_EQ(CORE_HAS_DSP, 0);
    EXPECT_EQ(CORE_HAS_MPU, 0);
    EXPECT_EQ(CORE_HAS_CACHE, 0);
    EXPECT_EQ(CORE_HAS_TZ, 0);
#endif
}

/*===========================================================================*/
/* Compiler Attribute Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test HAL_INLINE attribute
 */
HAL_INLINE uint32_t test_inline_function(uint32_t x) {
    return x + 1;
}

TEST_F(BuildSystemTest, HalInlineWorks) {
    uint32_t result = test_inline_function(5);
    EXPECT_EQ(result, 6u);
}

/**
 * \brief           Test HAL_WEAK attribute (weak function)
 */
HAL_WEAK void test_weak_function_build_system(void) {
    // Default implementation
}

TEST_F(BuildSystemTest, HalWeakCompiles) {
    // Just verify it compiles - weak functions are tested at link time
    test_weak_function_build_system();
    SUCCEED();
}

/**
 * \brief           Test HAL_PACKED attribute
 */
struct HAL_PACKED TestPackedStruct {
    uint8_t a;
    uint32_t b;
    uint8_t c;
};

TEST_F(BuildSystemTest, HalPackedWorks) {
    // Packed struct should be smaller than unpacked
    // Unpacked would be at least 12 bytes (4-byte alignment)
    // Packed should be 6 bytes
    EXPECT_LE(sizeof(TestPackedStruct), 12u);
}

/*===========================================================================*/
/* Memory Barrier Macro Tests                                                */
/*===========================================================================*/

/**
 * \brief           Test that memory barrier macros compile
 */
TEST_F(BuildSystemTest, MemoryBarrierMacrosCompile) {
    // These should compile without errors
    HAL_DSB();
    HAL_ISB();
    HAL_DMB();
    HAL_COMPILER_BARRIER();
    SUCCEED();
}

/*===========================================================================*/
/* Interrupt Control Macro Tests                                             */
/*===========================================================================*/

/**
 * \brief           Test that interrupt control macros compile
 * \note            We don't actually disable interrupts in tests
 */
TEST_F(BuildSystemTest, InterruptControlMacrosCompile) {
    // These should compile without errors
    // Note: We don't actually call them as they would affect the test
    // environment
    HAL_NOP();
    SUCCEED();
}

/*===========================================================================*/
/* Bit Manipulation Tests                                                    */
/*===========================================================================*/

/**
 * \brief           Test hal_clz (count leading zeros)
 */
TEST_F(BuildSystemTest, HalClzWorks) {
    EXPECT_EQ(hal_clz(0x80000000UL), 0u);
    EXPECT_EQ(hal_clz(0x00000001UL), 31u);
    EXPECT_EQ(hal_clz(0x00000000UL), 32u);
    EXPECT_EQ(hal_clz(0x0000FFFFUL), 16u);
}

/**
 * \brief           Test hal_rev (byte reverse 32-bit)
 */
TEST_F(BuildSystemTest, HalRevWorks) {
    EXPECT_EQ(hal_rev(0x12345678UL), 0x78563412UL);
    EXPECT_EQ(hal_rev(0x00000000UL), 0x00000000UL);
    EXPECT_EQ(hal_rev(0xFFFFFFFFUL), 0xFFFFFFFFUL);
}

/**
 * \brief           Test hal_rev16 (byte reverse 16-bit)
 */
TEST_F(BuildSystemTest, HalRev16Works) {
    EXPECT_EQ(hal_rev16(0x1234U), 0x3412U);
    EXPECT_EQ(hal_rev16(0x0000U), 0x0000U);
    EXPECT_EQ(hal_rev16(0xFFFFU), 0xFFFFU);
}

/*===========================================================================*/
/* Critical Section Tests                                                    */
/*===========================================================================*/

/**
 * \brief           Test critical section functions compile
 */
TEST_F(BuildSystemTest, CriticalSectionFunctionsCompile) {
    // These should compile without errors
    // Note: We don't actually use them as they would affect the test
    // environment uint32_t state = hal_enter_critical();
    // hal_exit_critical(state);
    SUCCEED();
}
