/**
 * \file            test_nx_sdio.cpp
 * \brief           SDIO unit tests
 * \author          Nexus Team
 */

#include "hal/interface/nx_sdio.h"
#include "hal/nx_status.h"
#include "hal/nx_factory.h"`n#include "tests/hal/native/devices/native_sdio_helpers.h"
#include <cstring>
#include <gtest/gtest.h>

/*---------------------------------------------------------------------------*/
/* Test Fixtures                                                             */
/*---------------------------------------------------------------------------*/

class SDIOTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all instances before each test */
        native_sdio_reset_all();

        /* Get SDIO instance */
        sdio = nx_factory_sdio(0);
        ASSERT_NE(nullptr, sdio);

        /* Set card present */
        native_sdio_set_card_present(0, true);
    }

    void TearDown() override {
        /* Reset after test */
        native_sdio_reset_all();
    }

    nx_sdio_t* sdio;
};

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, Lifecycle_Init) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    ASSERT_NE(nullptr, lifecycle);

    /* Check initial state */
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Initialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify state */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_sdio_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(SDIOTest, Lifecycle_Deinit) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    ASSERT_NE(nullptr, lifecycle);

    /* Initialize first */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Verify state */
    bool initialized = false;
    EXPECT_EQ(NX_OK, native_sdio_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);
}

TEST_F(SDIOTest, Lifecycle_SuspendResume) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    ASSERT_NE(nullptr, lifecycle);

    /* Initialize first */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Verify state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, native_sdio_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Verify state */
    EXPECT_EQ(NX_OK, native_sdio_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(SDIOTest, Lifecycle_ErrorConditions) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    ASSERT_NE(nullptr, lifecycle);

    /* Cannot deinit before init */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));

    /* Cannot suspend before init */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->suspend(lifecycle));

    /* Cannot resume before init */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->resume(lifecycle));

    /* Initialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Cannot init twice */
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));

    /* Cannot resume when not suspended */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Cannot suspend twice */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, PowerManagement) {
    nx_power_t* power = sdio->get_power(sdio);
    ASSERT_NE(nullptr, power);

    /* Power is always enabled in simulation */
    EXPECT_TRUE(power->is_enabled(power));

    /* Enable/disable operations succeed but don't change state */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));

    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

/*---------------------------------------------------------------------------*/
/* Block Read/Write Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, ReadWriteSingleBlock) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Prepare test data */
    uint8_t write_data[512];
    uint8_t read_data[512];
    for (size_t i = 0; i < 512; i++) {
        write_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Write block */
    EXPECT_EQ(NX_OK, sdio->write(sdio, 0, write_data, 1));

    /* Read block */
    EXPECT_EQ(NX_OK, sdio->read(sdio, 0, read_data, 1));

    /* Verify data */
    EXPECT_EQ(0, memcmp(write_data, read_data, 512));
}

TEST_F(SDIOTest, ReadWriteMultipleBlocks) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Prepare test data (4 blocks) */
    uint8_t write_data[2048];
    uint8_t read_data[2048];
    for (size_t i = 0; i < 2048; i++) {
        write_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Write multiple blocks */
    EXPECT_EQ(NX_OK, sdio->write(sdio, 10, write_data, 4));

    /* Read multiple blocks */
    EXPECT_EQ(NX_OK, sdio->read(sdio, 10, read_data, 4));

    /* Verify data */
    EXPECT_EQ(0, memcmp(write_data, read_data, 2048));
}

TEST_F(SDIOTest, EraseBlocks) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Write some data first */
    uint8_t write_data[512];
    memset(write_data, 0xAA, 512);
    EXPECT_EQ(NX_OK, sdio->write(sdio, 5, write_data, 1));

    /* Erase the block */
    EXPECT_EQ(NX_OK, sdio->erase(sdio, 5, 1));

    /* Read back and verify erased (0xFF) */
    uint8_t read_data[512];
    EXPECT_EQ(NX_OK, sdio->read(sdio, 5, read_data, 1));

    for (size_t i = 0; i < 512; i++) {
        EXPECT_EQ(0xFF, read_data[i]);
    }
}

/*---------------------------------------------------------------------------*/
/* Card Detection Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, CardDetection) {
    /* Card should be present initially */
    EXPECT_TRUE(sdio->is_present(sdio));

    /* Remove card */
    native_sdio_set_card_present(0, false);
    EXPECT_FALSE(sdio->is_present(sdio));

    /* Insert card */
    native_sdio_set_card_present(0, true);
    EXPECT_TRUE(sdio->is_present(sdio));
}

TEST_F(SDIOTest, OperationsWithoutCard) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);

    /* Remove card */
    native_sdio_set_card_present(0, false);

    /* Initialize should fail without card */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->init(lifecycle));

    /* Insert card and initialize */
    native_sdio_set_card_present(0, true);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Remove card after init */
    native_sdio_set_card_present(0, false);

    /* Operations should fail without card */
    uint8_t data[512];
    EXPECT_EQ(NX_ERR_INVALID_STATE, sdio->read(sdio, 0, data, 1));
    EXPECT_EQ(NX_ERR_INVALID_STATE, sdio->write(sdio, 0, data, 1));
    EXPECT_EQ(NX_ERR_INVALID_STATE, sdio->erase(sdio, 0, 1));
}

/*---------------------------------------------------------------------------*/
/* Capacity and Block Size Tests                                            */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, GetBlockSize) {
    EXPECT_EQ(512, sdio->get_block_size(sdio));
}

TEST_F(SDIOTest, GetCapacity) {
    /* 1024 blocks * 512 bytes = 524288 bytes */
    EXPECT_EQ(524288, sdio->get_capacity(sdio));
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(SDIOTest, ErrorConditions) {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    uint8_t data[512];

    /* Invalid block number */
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->read(sdio, 1024, data, 1));
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->write(sdio, 1024, data, 1));
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->erase(sdio, 1024, 1));

    /* Block count exceeds range */
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->read(sdio, 1020, data, 10));
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->write(sdio, 1020, data, 10));
    EXPECT_EQ(NX_ERR_INVALID_PARAM, sdio->erase(sdio, 1020, 10));

    /* NULL pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR, sdio->read(sdio, 0, nullptr, 1));
    EXPECT_EQ(NX_ERR_NULL_PTR, sdio->write(sdio, 0, nullptr, 1));

    /* Operations before init */
    native_sdio_reset(0);
    native_sdio_set_card_present(0, true);
    EXPECT_EQ(NX_ERR_NOT_INIT, sdio->read(sdio, 0, data, 1));
    EXPECT_EQ(NX_ERR_NOT_INIT, sdio->write(sdio, 0, data, 1));
    EXPECT_EQ(NX_ERR_NOT_INIT, sdio->erase(sdio, 0, 1));
}

/*---------------------------------------------------------------------------*/
/* Test Helper Tests                                                         */
/*---------------------------------------------------------------------------*/

TEST(SDIOHelperTest, TestHelpers) {
    native_sdio_reset_all();

    /* Get instance */
    nx_sdio_t* sdio = nx_factory_sdio(0);
    ASSERT_NE(nullptr, sdio);

    /* Invalid index */
    EXPECT_EQ(nullptr, nx_factory_sdio(10));

    /* State query */
    bool initialized = true;
    bool suspended = true;
    EXPECT_EQ(NX_OK, native_sdio_get_state(0, &initialized, &suspended));
    EXPECT_FALSE(initialized);
    EXPECT_FALSE(suspended);

    /* Card present helpers */
    native_sdio_set_card_present(0, true);
    EXPECT_TRUE(native_sdio_is_card_present(0));

    native_sdio_set_card_present(0, false);
    EXPECT_FALSE(native_sdio_is_card_present(0));

    /* Block data helper */
    native_sdio_set_card_present(0, true);
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    uint8_t write_data[512];
    uint8_t read_data[512];
    memset(write_data, 0x55, 512);

    EXPECT_EQ(NX_OK, sdio->write(sdio, 0, write_data, 1));
    EXPECT_EQ(NX_OK, native_sdio_get_block_data(0, 0, read_data));
    EXPECT_EQ(0, memcmp(write_data, read_data, 512));
}

