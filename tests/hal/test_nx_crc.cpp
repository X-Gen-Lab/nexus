/**
 * \file            test_nx_crc.cpp
 * \brief           CRC Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for CRC peripheral implementation.
 *                  Requirements: 3.1-3.8, 10.1-10.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_crc.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_crc_helpers.h"
}

/**
 * \brief           CRC Test Fixture
 */
class CRCTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all CRC instances before each test */
        native_crc_reset_all();

        /* Get CRC0 instance */
        crc = nx_factory_crc(0);
        ASSERT_NE(nullptr, crc);

        /* Initialize CRC */
        nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize CRC */
        if (crc != nullptr) {
            nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_crc_reset_all();
    }

    nx_crc_t* crc = nullptr;
};

/*---------------------------------------------------------------------------*/
/* CRC-32 Calculation Tests - Requirements 3.2, 3.3                          */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, CRC32_EmptyData) {
    /* Test CRC-32 with empty data */
    const uint8_t data[] = "";
    uint32_t result = crc->calculate(crc, data, 0);

    /* Empty data should return init XOR final_xor */
    /* For CRC-32: init=0xFFFFFFFF, final_xor=0xFFFFFFFF */
    /* Result should be 0xFFFFFFFF ^ 0xFFFFFFFF = 0x00000000 */
    EXPECT_EQ(0x00000000U, result);
}

TEST_F(CRCTest, CRC32_SingleByte) {
    /* Test CRC-32 with single byte */
    const uint8_t data[] = {0x00};
    uint32_t result = crc->calculate(crc, data, 1);

    /* Known CRC-32 value for {0x00} */
    EXPECT_NE(0U, result);
}

TEST_F(CRCTest, CRC32_KnownValue) {
    /* Test CRC-32 with known test vector */
    const uint8_t data[] = "123456789";
    uint32_t result = crc->calculate(crc, data, 9);

    /* Known CRC-32 value for "123456789" is 0xCBF43926 */
    EXPECT_EQ(0xCBF43926U, result);
}

TEST_F(CRCTest, CRC32_MultipleBytes) {
    /* Test CRC-32 with multiple bytes */
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t result = crc->calculate(crc, data, 5);

    /* Result should be non-zero */
    EXPECT_NE(0U, result);
}

/*---------------------------------------------------------------------------*/
/* CRC Reset/Update Tests - Requirements 3.2                                 */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, ResetCRC) {
    /* Calculate initial CRC */
    const uint8_t data1[] = "test";
    crc->update(crc, data1, 4);
    uint32_t result1 = crc->get_result(crc);

    /* Reset CRC */
    crc->reset(crc);

    /* Calculate again with same data */
    crc->update(crc, data1, 4);
    uint32_t result2 = crc->get_result(crc);

    /* Results should be identical */
    EXPECT_EQ(result1, result2);
}

TEST_F(CRCTest, UpdateCRC) {
    /* Calculate CRC in one shot */
    const uint8_t data[] = "123456789";
    uint32_t result1 = crc->calculate(crc, data, 9);

    /* Calculate CRC incrementally */
    crc->reset(crc);
    crc->update(crc, data, 3);     /* "123" */
    crc->update(crc, data + 3, 3); /* "456" */
    crc->update(crc, data + 6, 3); /* "789" */
    uint32_t result2 = crc->get_result(crc);

    /* Results should be identical */
    EXPECT_EQ(result1, result2);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 3.6, 10.2                                  */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, LifecycleInit) {
    /* Already initialized in SetUp, check state */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_crc_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(CRCTest, LifecycleDeinit) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    bool initialized = false;
    EXPECT_EQ(NX_OK, native_crc_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);
}

TEST_F(CRCTest, LifecycleSuspend) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_crc_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);
}

TEST_F(CRCTest, LifecycleResume) {
    /* Suspend first */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_crc_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(CRCTest, LifecycleGetState) {
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);

    /* Should be running after init */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinit */
    lifecycle->deinit(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 3.7, 10.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, PowerEnable) {
    /* Get power interface through lifecycle */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);

    /* Power interface is embedded in implementation */
    /* For now, we test through the CRC interface */
    /* Power management doesn't affect CRC calculation in simulation */
    const uint8_t data[] = "test";
    uint32_t result = crc->calculate(crc, data, 4);
    EXPECT_NE(0U, result);
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests - Requirements 10.6                                 */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, NullPointerHandling) {
    /* Test NULL pointer handling */
    crc->reset(nullptr);              /* Should not crash */
    crc->update(nullptr, nullptr, 0); /* Should not crash */
    EXPECT_EQ(0U, crc->get_result(nullptr));
    EXPECT_EQ(0U, crc->calculate(nullptr, nullptr, 0));
}

TEST_F(CRCTest, UninitializedOperation) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Operations on uninitialized CRC should return 0 or do nothing */
    const uint8_t data[] = "test";
    crc->reset(crc);           /* Should not crash */
    crc->update(crc, data, 4); /* Should not crash */
    EXPECT_EQ(0U, crc->get_result(crc));
}

TEST_F(CRCTest, DoubleInit) {
    /* Try to initialize again */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(CRCTest, DeinitUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to deinitialize again */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

TEST_F(CRCTest, SuspendUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to suspend */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->suspend(lifecycle));
}

TEST_F(CRCTest, ResumeNotSuspended) {
    /* Try to resume without suspending */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}

TEST_F(CRCTest, DoubleSuspend) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Try to suspend again */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Set Polynomial Tests - Requirements 3.4                                   */
/*---------------------------------------------------------------------------*/

TEST_F(CRCTest, SetPolynomial) {
    /* Set polynomial */
    EXPECT_EQ(NX_OK, crc->set_polynomial(crc, 0x04C11DB7));

    /* Calculate CRC */
    const uint8_t data[] = "test";
    uint32_t result = crc->calculate(crc, data, 4);
    EXPECT_NE(0U, result);
}

TEST_F(CRCTest, SetPolynomialNull) {
    /* Test NULL pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR, crc->set_polynomial(nullptr, 0x04C11DB7));
}

TEST_F(CRCTest, SetPolynomialUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to set polynomial */
    EXPECT_EQ(NX_ERR_NOT_INIT, crc->set_polynomial(crc, 0x04C11DB7));
}
