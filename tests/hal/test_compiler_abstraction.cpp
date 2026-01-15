/**
 * \file            test_compiler_abstraction.cpp
 * \brief           Compiler Abstraction Layer Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \par             Requirements: 12.9
 */

#include <gtest/gtest.h>

/* Include the headers under test */
extern "C" {
/* For testing purposes, we define __CORTEX_M to simulate STM32F4 environment */
#ifndef __CORTEX_M
#define __CORTEX_M 4
#endif

#include "platforms/stm32f4/include/compiler_abstraction.h"
#include "platforms/stm32f4/include/core_config.h"
}

/**
 * \brief           Core Config Test Fixture
 */
class CoreConfigTest : public ::testing::Test {
  protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

/**
 * \brief           Compiler Abstraction Test Fixture
 */
class CompilerAbstractionTest : public ::testing::Test {
  protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

/*===========================================================================*/
/* Core Config Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test core type definitions exist
 */
TEST_F(CoreConfigTest, CoreTypeDefinitionsExist) {
    /* Verify all core type constants are defined */
    EXPECT_EQ(0, CORE_CM0);
    EXPECT_EQ(1, CORE_CM0P);
    EXPECT_EQ(3, CORE_CM3);
    EXPECT_EQ(4, CORE_CM4);
    EXPECT_EQ(7, CORE_CM7);
    EXPECT_EQ(33, CORE_CM33);
}

/**
 * \brief           Test core type detection for CM4
 */
TEST_F(CoreConfigTest, CoreTypeDetectionCM4) {
    /* With __CORTEX_M = 4, CORE_TYPE should be CORE_CM4 */
    EXPECT_EQ(CORE_CM4, CORE_TYPE);
}

/**
 * \brief           Test FPU feature detection for CM4
 */
TEST_F(CoreConfigTest, FpuFeatureDetectionCM4) {
    /* CM4 should have FPU support */
    EXPECT_EQ(1, CORE_HAS_FPU);
}

/**
 * \brief           Test DSP feature detection for CM4
 */
TEST_F(CoreConfigTest, DspFeatureDetectionCM4) {
    /* CM4 should have DSP support */
    EXPECT_EQ(1, CORE_HAS_DSP);
}

/**
 * \brief           Test MPU feature detection for CM4
 */
TEST_F(CoreConfigTest, MpuFeatureDetectionCM4) {
    /* CM4 should have MPU support (CM3+) */
    EXPECT_EQ(1, CORE_HAS_MPU);
}

/**
 * \brief           Test Cache feature detection for CM4
 */
TEST_F(CoreConfigTest, CacheFeatureDetectionCM4) {
    /* CM4 should NOT have Cache support (only CM7) */
    EXPECT_EQ(0, CORE_HAS_CACHE);
}

/**
 * \brief           Test TrustZone feature detection for CM4
 */
TEST_F(CoreConfigTest, TrustZoneFeatureDetectionCM4) {
    /* CM4 should NOT have TrustZone support (only CM33) */
    EXPECT_EQ(0, CORE_HAS_TZ);
}

/**
 * \brief           Test NVIC priority bits for CM4
 */
TEST_F(CoreConfigTest, NvicPrioBitsCM4) {
    /* CM4 should have 4 NVIC priority bits */
    EXPECT_EQ(4, CORE_NVIC_PRIO_BITS);
}

/**
 * \brief           Test NVIC priority max value
 */
TEST_F(CoreConfigTest, NvicPrioMaxValue) {
    /* With 4 bits, max priority should be 15 */
    EXPECT_EQ(15U, CORE_NVIC_PRIO_MAX);
}

/**
 * \brief           Test NVIC priority constants
 */
TEST_F(CoreConfigTest, NvicPrioConstants) {
    EXPECT_EQ(0U, CORE_NVIC_PRIO_HIGHEST);
    EXPECT_EQ(CORE_NVIC_PRIO_MAX, CORE_NVIC_PRIO_LOWEST);
}

/**
 * \brief           Test core type string
 */
TEST_F(CoreConfigTest, CoreTypeString) {
    EXPECT_STREQ("Cortex-M4", CORE_TYPE_STRING);
}

/*===========================================================================*/
/* Compiler Detection Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test compiler detection macros exist
 */
TEST_F(CompilerAbstractionTest, CompilerDetectionMacrosExist) {
    /* At least one compiler should be detected */
    int compiler_count = COMPILER_GCC + COMPILER_CLANG + COMPILER_IAR;

    /* In test environment, we expect GCC, Clang, or MSVC */
#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
    EXPECT_GE(compiler_count + COMPILER_MSVC, 1);
#else
    (void)compiler_count; /* Suppress unused variable warning */
#endif
}

/**
 * \brief           Test compiler name is defined
 */
TEST_F(CompilerAbstractionTest, CompilerNameDefined) {
    EXPECT_NE(nullptr, COMPILER_NAME);
    EXPECT_GT(strlen(COMPILER_NAME), 0U);
}

/**
 * \brief           Test compiler version is defined
 */
TEST_F(CompilerAbstractionTest, CompilerVersionDefined) {
    /* Version should be a positive number for known compilers */
#if COMPILER_GCC || COMPILER_CLANG || COMPILER_IAR
    EXPECT_GT(COMPILER_VERSION, 0);
#endif
}

/*===========================================================================*/
/* Function Attribute Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test HAL_INLINE attribute compiles
 */
HAL_INLINE int test_inline_function(int x) {
    return x * 2;
}

TEST_F(CompilerAbstractionTest, HalInlineCompiles) {
    EXPECT_EQ(10, test_inline_function(5));
}

/**
 * \brief           Test HAL_WEAK attribute compiles
 */
HAL_WEAK void test_weak_function(void) {
    /* This function can be overridden */
}

TEST_F(CompilerAbstractionTest, HalWeakCompiles) {
    /* Just verify it compiles and can be called */
    test_weak_function();
    SUCCEED();
}

/**
 * \brief           Test HAL_USED attribute compiles
 */
HAL_USED static int test_used_variable = 42;

TEST_F(CompilerAbstractionTest, HalUsedCompiles) {
    EXPECT_EQ(42, test_used_variable);
}

/*===========================================================================*/
/* Memory Barrier Tests                                                      */
/*===========================================================================*/

/**
 * \brief           Test memory barrier macros compile
 * \note            We can't easily test the actual barrier behavior in unit
 * tests, but we can verify they compile and don't crash.
 */
TEST_F(CompilerAbstractionTest, MemoryBarriersCompile) {
    volatile int x = 0;

    x = 1;
    HAL_DSB();
    EXPECT_EQ(1, x);

    x = 2;
    HAL_ISB();
    EXPECT_EQ(2, x);

    x = 3;
    HAL_DMB();
    EXPECT_EQ(3, x);

    x = 4;
    HAL_COMPILER_BARRIER();
    EXPECT_EQ(4, x);
}

/**
 * \brief           Test NOP instruction compiles
 */
TEST_F(CompilerAbstractionTest, NopCompiles) {
    HAL_NOP();
    SUCCEED();
}

/*===========================================================================*/
/* Critical Section Tests                                                    */
/*===========================================================================*/

/**
 * \brief           Test critical section functions compile
 * \note            In test environment (not on real hardware), these may not
 *                  actually disable interrupts, but they should compile.
 */
TEST_F(CompilerAbstractionTest, CriticalSectionCompiles) {
    uint32_t state = hal_enter_critical();
    /* Do something in critical section */
    volatile int x = 42;
    (void)x;
    hal_exit_critical(state);
    SUCCEED();
}

/*===========================================================================*/
/* Bit Manipulation Tests                                                    */
/*===========================================================================*/

/**
 * \brief           Test count leading zeros function
 */
TEST_F(CompilerAbstractionTest, ClzFunction) {
    EXPECT_EQ(32U, hal_clz(0));
    EXPECT_EQ(31U, hal_clz(1));
    EXPECT_EQ(0U, hal_clz(0x80000000U));
    EXPECT_EQ(24U, hal_clz(0x000000FFU));
    EXPECT_EQ(16U, hal_clz(0x0000FFFFU));
    EXPECT_EQ(8U, hal_clz(0x00FFFFFFU));
}

/**
 * \brief           Test byte reverse function (32-bit)
 */
TEST_F(CompilerAbstractionTest, RevFunction) {
    EXPECT_EQ(0x78563412U, hal_rev(0x12345678U));
    EXPECT_EQ(0x00000000U, hal_rev(0x00000000U));
    EXPECT_EQ(0xFFFFFFFFU, hal_rev(0xFFFFFFFFU));
    EXPECT_EQ(0x01000000U, hal_rev(0x00000001U));
    EXPECT_EQ(0x000000FFU, hal_rev(0xFF000000U));
}

/**
 * \brief           Test byte reverse function (16-bit)
 */
TEST_F(CompilerAbstractionTest, Rev16Function) {
    EXPECT_EQ(0x3412U, hal_rev16(0x1234U));
    EXPECT_EQ(0x0000U, hal_rev16(0x0000U));
    EXPECT_EQ(0xFFFFU, hal_rev16(0xFFFFU));
    EXPECT_EQ(0x0100U, hal_rev16(0x0001U));
    EXPECT_EQ(0x00FFU, hal_rev16(0xFF00U));
}

/*===========================================================================*/
/* PRIMASK Access Tests                                                      */
/*===========================================================================*/

/**
 * \brief           Test PRIMASK get/set functions compile
 * \note            On non-ARM platforms, these may return dummy values
 */
TEST_F(CompilerAbstractionTest, PrimaskFunctionsCompile) {
    uint32_t primask = hal_get_primask();
    hal_set_primask(primask);
    SUCCEED();
}

/*===========================================================================*/
/* Feature Consistency Tests                                                 */
/*===========================================================================*/

/**
 * \brief           Test feature detection consistency
 * \note            Verify that feature flags are consistent with core type
 */
TEST_F(CoreConfigTest, FeatureConsistency) {
    /* For CM4, verify expected feature set using compile-time checks */
#if (CORE_TYPE == CORE_CM4)
    EXPECT_EQ(1, CORE_HAS_FPU);
    EXPECT_EQ(1, CORE_HAS_DSP);
    EXPECT_EQ(1, CORE_HAS_MPU);
    EXPECT_EQ(0, CORE_HAS_CACHE);
    EXPECT_EQ(0, CORE_HAS_TZ);
#endif

    /* Cache should only be available on CM7 */
#if CORE_HAS_CACHE
    EXPECT_EQ(CORE_CM7, CORE_TYPE);
#endif

    /* TrustZone should only be available on CM33 */
#if CORE_HAS_TZ
    EXPECT_EQ(CORE_CM33, CORE_TYPE);
#endif
}

/**
 * \brief           Test NVIC priority bits are valid
 */
TEST_F(CoreConfigTest, NvicPrioBitsValid) {
    /* Priority bits should be between 2 and 8 */
    EXPECT_GE(CORE_NVIC_PRIO_BITS, 2);
    EXPECT_LE(CORE_NVIC_PRIO_BITS, 8);

    /* Max priority should be (2^bits - 1) */
    EXPECT_EQ((1U << CORE_NVIC_PRIO_BITS) - 1, CORE_NVIC_PRIO_MAX);
}
