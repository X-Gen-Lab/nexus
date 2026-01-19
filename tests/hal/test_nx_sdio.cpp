/**
 * \file            test_nx_sdio.cpp
 * \brief           SDIO unit tests
 * \author          Nexus Team
 */

#include "hal/interface/nx_sdio.h"
#include "hal/nx_status.h"
#include "native_sdio_test.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>

/*---------------------------------------------------------------------------*/
/* Test Fixtures                                                             */
/*---------------------------------------------------------------------------*/

class SDIOTestFixture {
  public:
    SDIOTestFixture() {
        /* Reset all instances before each test */
        nx_sdio_native_reset_all();

        /* Get SDIO instance */
        sdio = nx_sdio_native_get(0);
        REQUIRE(sdio != nullptr);

        /* Set card present */
        nx_sdio_native_set_card_present(0, true);
    }

    ~SDIOTestFixture() {
        /* Reset after test */
        nx_sdio_native_reset_all();
    }

    nx_sdio_t* sdio;
};

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO lifecycle init", "[sdio][lifecycle]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle != nullptr);

    /* Check initial state */
    REQUIRE(lifecycle->get_state(lifecycle) == NX_DEV_STATE_UNINITIALIZED);

    /* Initialize */
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);
    REQUIRE(lifecycle->get_state(lifecycle) == NX_DEV_STATE_RUNNING);

    /* Verify state */
    bool initialized = false;
    bool suspended = false;
    REQUIRE(nx_sdio_native_get_state(0, &initialized, &suspended) == NX_OK);
    REQUIRE(initialized == true);
    REQUIRE(suspended == false);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO lifecycle deinit",
                 "[sdio][lifecycle]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle != nullptr);

    /* Initialize first */
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Deinitialize */
    REQUIRE(lifecycle->deinit(lifecycle) == NX_OK);
    REQUIRE(lifecycle->get_state(lifecycle) == NX_DEV_STATE_UNINITIALIZED);

    /* Verify state */
    bool initialized = false;
    REQUIRE(nx_sdio_native_get_state(0, &initialized, nullptr) == NX_OK);
    REQUIRE(initialized == false);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO lifecycle suspend/resume",
                 "[sdio][lifecycle]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle != nullptr);

    /* Initialize first */
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Suspend */
    REQUIRE(lifecycle->suspend(lifecycle) == NX_OK);
    REQUIRE(lifecycle->get_state(lifecycle) == NX_DEV_STATE_SUSPENDED);

    /* Verify state */
    bool suspended = false;
    REQUIRE(nx_sdio_native_get_state(0, nullptr, &suspended) == NX_OK);
    REQUIRE(suspended == true);

    /* Resume */
    REQUIRE(lifecycle->resume(lifecycle) == NX_OK);
    REQUIRE(lifecycle->get_state(lifecycle) == NX_DEV_STATE_RUNNING);

    /* Verify state */
    REQUIRE(nx_sdio_native_get_state(0, nullptr, &suspended) == NX_OK);
    REQUIRE(suspended == false);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO lifecycle error conditions",
                 "[sdio][lifecycle][error]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle != nullptr);

    /* Cannot deinit before init */
    REQUIRE(lifecycle->deinit(lifecycle) == NX_ERR_NOT_INIT);

    /* Cannot suspend before init */
    REQUIRE(lifecycle->suspend(lifecycle) == NX_ERR_NOT_INIT);

    /* Cannot resume before init */
    REQUIRE(lifecycle->resume(lifecycle) == NX_ERR_NOT_INIT);

    /* Initialize */
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Cannot init twice */
    REQUIRE(lifecycle->init(lifecycle) == NX_ERR_ALREADY_INIT);

    /* Cannot resume when not suspended */
    REQUIRE(lifecycle->resume(lifecycle) == NX_ERR_INVALID_STATE);

    /* Suspend */
    REQUIRE(lifecycle->suspend(lifecycle) == NX_OK);

    /* Cannot suspend twice */
    REQUIRE(lifecycle->suspend(lifecycle) == NX_ERR_INVALID_STATE);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO power management", "[sdio][power]") {
    nx_power_t* power = sdio->get_power(sdio);
    REQUIRE(power != nullptr);

    /* Power is always enabled in simulation */
    REQUIRE(power->is_enabled(power) == true);

    /* Enable/disable operations succeed but don't change state */
    REQUIRE(power->enable(power) == NX_OK);
    REQUIRE(power->is_enabled(power) == true);

    REQUIRE(power->disable(power) == NX_OK);
    REQUIRE(power->is_enabled(power) == true);
}

/*---------------------------------------------------------------------------*/
/* Block Read/Write Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO read/write single block",
                 "[sdio][read][write]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Prepare test data */
    uint8_t write_data[512];
    uint8_t read_data[512];
    for (size_t i = 0; i < 512; i++) {
        write_data[i] = (uint8_t)(i & 0xFF);
    }

    /* Write block */
    REQUIRE(sdio->write(sdio, 0, write_data, 1) == NX_OK);

    /* Read block */
    REQUIRE(sdio->read(sdio, 0, read_data, 1) == NX_OK);

    /* Verify data */
    REQUIRE(memcmp(write_data, read_data, 512) == 0);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO read/write multiple blocks",
                 "[sdio][read][write]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Prepare test data (4 blocks) */
    uint8_t write_data[2048];
    uint8_t read_data[2048];
    for (size_t i = 0; i < 2048; i++) {
        write_data[i] = (uint8_t)(i & 0xFF);
    }

    /* Write multiple blocks */
    REQUIRE(sdio->write(sdio, 10, write_data, 4) == NX_OK);

    /* Read multiple blocks */
    REQUIRE(sdio->read(sdio, 10, read_data, 4) == NX_OK);

    /* Verify data */
    REQUIRE(memcmp(write_data, read_data, 2048) == 0);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO erase blocks", "[sdio][erase]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Write some data first */
    uint8_t write_data[512];
    memset(write_data, 0xAA, 512);
    REQUIRE(sdio->write(sdio, 5, write_data, 1) == NX_OK);

    /* Erase the block */
    REQUIRE(sdio->erase(sdio, 5, 1) == NX_OK);

    /* Read back and verify erased (0xFF) */
    uint8_t read_data[512];
    REQUIRE(sdio->read(sdio, 5, read_data, 1) == NX_OK);

    for (size_t i = 0; i < 512; i++) {
        REQUIRE(read_data[i] == 0xFF);
    }
}

/*---------------------------------------------------------------------------*/
/* Card Detection Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO card detection", "[sdio][card]") {
    /* Card should be present initially */
    REQUIRE(sdio->is_present(sdio) == true);

    /* Remove card */
    nx_sdio_native_set_card_present(0, false);
    REQUIRE(sdio->is_present(sdio) == false);

    /* Insert card */
    nx_sdio_native_set_card_present(0, true);
    REQUIRE(sdio->is_present(sdio) == true);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO operations without card",
                 "[sdio][card][error]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);

    /* Remove card */
    nx_sdio_native_set_card_present(0, false);

    /* Initialize should fail without card */
    REQUIRE(lifecycle->init(lifecycle) == NX_ERR_INVALID_STATE);

    /* Insert card and initialize */
    nx_sdio_native_set_card_present(0, true);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    /* Remove card after init */
    nx_sdio_native_set_card_present(0, false);

    /* Operations should fail without card */
    uint8_t data[512];
    REQUIRE(sdio->read(sdio, 0, data, 1) == NX_ERR_INVALID_STATE);
    REQUIRE(sdio->write(sdio, 0, data, 1) == NX_ERR_INVALID_STATE);
    REQUIRE(sdio->erase(sdio, 0, 1) == NX_ERR_INVALID_STATE);
}

/*---------------------------------------------------------------------------*/
/* Capacity and Block Size Tests                                            */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO get block size", "[sdio][info]") {
    REQUIRE(sdio->get_block_size(sdio) == 512);
}

TEST_CASE_METHOD(SDIOTestFixture, "SDIO get capacity", "[sdio][info]") {
    /* 1024 blocks * 512 bytes = 524288 bytes */
    REQUIRE(sdio->get_capacity(sdio) == 524288);
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_CASE_METHOD(SDIOTestFixture, "SDIO error conditions", "[sdio][error]") {
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    uint8_t data[512];

    /* Invalid block number */
    REQUIRE(sdio->read(sdio, 1024, data, 1) == NX_ERR_INVALID_PARAM);
    REQUIRE(sdio->write(sdio, 1024, data, 1) == NX_ERR_INVALID_PARAM);
    REQUIRE(sdio->erase(sdio, 1024, 1) == NX_ERR_INVALID_PARAM);

    /* Block count exceeds range */
    REQUIRE(sdio->read(sdio, 1020, data, 10) == NX_ERR_INVALID_PARAM);
    REQUIRE(sdio->write(sdio, 1020, data, 10) == NX_ERR_INVALID_PARAM);
    REQUIRE(sdio->erase(sdio, 1020, 10) == NX_ERR_INVALID_PARAM);

    /* NULL pointer */
    REQUIRE(sdio->read(sdio, 0, nullptr, 1) == NX_ERR_NULL_PTR);
    REQUIRE(sdio->write(sdio, 0, nullptr, 1) == NX_ERR_NULL_PTR);

    /* Operations before init */
    nx_sdio_native_reset(0);
    nx_sdio_native_set_card_present(0, true);
    REQUIRE(sdio->read(sdio, 0, data, 1) == NX_ERR_NOT_INIT);
    REQUIRE(sdio->write(sdio, 0, data, 1) == NX_ERR_NOT_INIT);
    REQUIRE(sdio->erase(sdio, 0, 1) == NX_ERR_NOT_INIT);
}

/*---------------------------------------------------------------------------*/
/* Test Helper Tests                                                         */
/*---------------------------------------------------------------------------*/

TEST_CASE("SDIO test helpers", "[sdio][helpers]") {
    nx_sdio_native_reset_all();

    /* Get instance */
    nx_sdio_t* sdio = nx_sdio_native_get(0);
    REQUIRE(sdio != nullptr);

    /* Invalid index */
    REQUIRE(nx_sdio_native_get(10) == nullptr);

    /* State query */
    bool initialized = true;
    bool suspended = true;
    REQUIRE(nx_sdio_native_get_state(0, &initialized, &suspended) == NX_OK);
    REQUIRE(initialized == false);
    REQUIRE(suspended == false);

    /* Card present helpers */
    nx_sdio_native_set_card_present(0, true);
    REQUIRE(nx_sdio_native_is_card_present(0) == true);

    nx_sdio_native_set_card_present(0, false);
    REQUIRE(nx_sdio_native_is_card_present(0) == false);

    /* Block data helper */
    nx_sdio_native_set_card_present(0, true);
    nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
    REQUIRE(lifecycle->init(lifecycle) == NX_OK);

    uint8_t write_data[512];
    uint8_t read_data[512];
    memset(write_data, 0x55, 512);

    REQUIRE(sdio->write(sdio, 0, write_data, 1) == NX_OK);
    REQUIRE(nx_sdio_native_get_block_data(0, 0, read_data) == NX_OK);
    REQUIRE(memcmp(write_data, read_data, 512) == 0);
}
