/**
 * \file            test_nx_flash.cpp
 * \brief           Flash Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Flash peripheral implementation.
 *                  Requirements: 4.1-4.9, 10.1-10.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_flash.h"
#include "hal/nx_factory.h"
#include "devices/native_flash_helpers.h"
#include "../../../platforms/native/src/flash/nx_flash_helpers.h"
void flash_init_lifecycle(nx_lifecycle_t*);
}

/**
 * \brief           Flash Test Fixture
 */
class FlashTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all Flash instances before each test */
        native_flash_reset_all();

        /* Get Flash0 instance */
        flash = nx_factory_flash(0);
        ASSERT_NE(nullptr, flash);

        /* Initialize Flash */
        nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Unlock flash for testing */
        ASSERT_EQ(NX_OK, flash->unlock(flash));
    }

    void TearDown() override {
        /* Deinitialize Flash */
        if (flash != nullptr) {
            nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_flash_reset_all();
    }

    nx_internal_flash_t* flash = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Flash Erase Tests - Requirements 4.2                                      */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Erase_SingleSector) {
    /* Erase a single sector */
    uint32_t addr = 0;
    size_t size = flash->get_page_size(flash);

    EXPECT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(size)));

    /* Verify sector is erased */
    EXPECT_TRUE(native_flash_is_erased(0, addr, static_cast<uint32_t>(size)));
}

TEST_F(FlashTest, Erase_MultipleSectors) {
    /* Erase multiple sectors */
    uint32_t addr = 0;
    size_t size = flash->get_page_size(flash) * 3;

    EXPECT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(size)));

    /* Verify all sectors are erased */
    EXPECT_TRUE(native_flash_is_erased(0, addr, static_cast<uint32_t>(size)));
}

TEST_F(FlashTest, Erase_PartialSector) {
    /* Erase partial sector (should round up) */
    uint32_t addr = 0;
    size_t size = flash->get_page_size(flash) / 2;

    EXPECT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(size)));

    /* Verify entire sector is erased */
    EXPECT_TRUE(native_flash_is_erased(
        0, 0, static_cast<uint32_t>(flash->get_page_size(flash))));
}

TEST_F(FlashTest, Erase_WhenLocked) {
    /* Lock flash */
    EXPECT_EQ(NX_OK, flash->lock(flash));

    /* Attempt to erase */
    uint32_t addr = 0;
    size_t size = flash->get_page_size(flash);

    EXPECT_EQ(NX_ERR_PERMISSION, flash->erase(flash, addr, size));
}

/*---------------------------------------------------------------------------*/
/* Flash Write Tests - Requirements 4.3, 4.5                                 */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Write_AfterErase) {
    /* Erase sector first */
    uint32_t addr = 0;
    size_t size = flash->get_page_size(flash);
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, size));

    /* Write data */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    EXPECT_EQ(NX_OK, flash->write(flash, addr, write_data, sizeof(write_data)));
}

TEST_F(FlashTest, Write_WithoutErase) {
    /* Attempt to write without erasing first */
    uint32_t addr = 0;
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    /* First erase to ensure clean state */
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, flash->get_page_size(flash)));

    /* Write once */
    EXPECT_EQ(NX_OK, flash->write(flash, addr, write_data, sizeof(write_data)));

    /* Attempt to write again without erase - should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE,
              flash->write(flash, addr, write_data, sizeof(write_data)));
}

TEST_F(FlashTest, Write_Alignment) {
    /* Test write alignment requirements */
    uint32_t addr = 0;
    size_t write_unit = flash->get_write_unit(flash);

    /* Erase sector */
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, flash->get_page_size(flash)));

    /* Aligned write should succeed */
    uint8_t aligned_data[16] = {0};
    EXPECT_EQ(NX_OK, flash->write(flash, addr, aligned_data, write_unit * 4));

    /* Unaligned address should fail */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              flash->write(flash, addr + 1, aligned_data, write_unit));

    /* Unaligned length should fail */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              flash->write(flash, addr + static_cast<uint32_t>(write_unit) * 4,
                           aligned_data,
                           static_cast<uint32_t>(write_unit) + 1));
}

TEST_F(FlashTest, Write_WhenLocked) {
    /* Erase sector */
    uint32_t addr = 0;
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, flash->get_page_size(flash)));

    /* Lock flash */
    EXPECT_EQ(NX_OK, flash->lock(flash));

    /* Attempt to write */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04};

    EXPECT_EQ(NX_ERR_PERMISSION, flash->write(flash, addr, write_data, 4));
}

/*---------------------------------------------------------------------------*/
/* Flash Read Tests - Requirements 4.4                                       */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Read_ErasedData) {
    /* Erase sector */
    uint32_t addr = 0;
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, flash->get_page_size(flash)));

    /* Read erased data */
    uint8_t read_data[16] = {0};
    EXPECT_EQ(NX_OK, flash->read(flash, addr, read_data, sizeof(read_data)));

    /* Verify all bytes are 0xFF (erased) */
    for (size_t i = 0; i < sizeof(read_data); i++) {
        EXPECT_EQ(0xFF, read_data[i]);
    }
}

TEST_F(FlashTest, Read_WrittenData) {
    /* Erase sector */
    uint32_t addr = 0;
    EXPECT_EQ(NX_OK, flash->erase(flash, addr, flash->get_page_size(flash)));

    /* Write data */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    EXPECT_EQ(NX_OK, flash->write(flash, addr, write_data, sizeof(write_data)));

    /* Read data back */
    uint8_t read_data[16] = {0};
    EXPECT_EQ(NX_OK, flash->read(flash, addr, read_data, sizeof(read_data)));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(write_data, read_data, sizeof(write_data)));
}

TEST_F(FlashTest, Read_CrossSectorBoundary) {
    /* Read across sector boundary */
    uint32_t page_size = static_cast<uint32_t>(flash->get_page_size(flash));
    uint32_t addr = page_size - 8;

    /* Erase sectors */
    EXPECT_EQ(NX_OK, flash->erase(flash, 0, page_size * 2));

    /* Write data across boundary */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    EXPECT_EQ(NX_OK, flash->write(flash, addr, write_data, sizeof(write_data)));

    /* Read data back */
    uint8_t read_data[16] = {0};
    EXPECT_EQ(NX_OK, flash->read(flash, addr, read_data, sizeof(read_data)));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(write_data, read_data, sizeof(write_data)));
}

/*---------------------------------------------------------------------------*/
/* Flash Lock/Unlock Tests - Requirements 4.5                                */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Lock_Unlock) {
    /* Flash should start unlocked (from SetUp) */
    bool locked = false;
    EXPECT_EQ(NX_OK, native_flash_get_lock_status(0, &locked));
    EXPECT_FALSE(locked);

    /* Lock flash */
    EXPECT_EQ(NX_OK, flash->lock(flash));
    EXPECT_EQ(NX_OK, native_flash_get_lock_status(0, &locked));
    EXPECT_TRUE(locked);

    /* Unlock flash */
    EXPECT_EQ(NX_OK, flash->unlock(flash));
    EXPECT_EQ(NX_OK, native_flash_get_lock_status(0, &locked));
    EXPECT_FALSE(locked);
}

/*---------------------------------------------------------------------------*/
/* Flash Lifecycle Tests - Requirements 4.6, 10.2                            */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Lifecycle_InitDeinit) {
    /* Flash is already initialized in SetUp */
    bool initialized = false;
    EXPECT_EQ(NX_OK, native_flash_get_state(0, &initialized, nullptr));
    EXPECT_TRUE(initialized);

    /* Deinitialize */
    nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    EXPECT_EQ(NX_OK, native_flash_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);

    /* Reinitialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_OK, native_flash_get_state(0, &initialized, nullptr));
    EXPECT_TRUE(initialized);
}

TEST_F(FlashTest, Lifecycle_SuspendResume) {
    bool suspended = false;

    /* Suspend */
    nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    EXPECT_EQ(NX_OK, native_flash_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    EXPECT_EQ(NX_OK, native_flash_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(FlashTest, Lifecycle_GetState) {
    nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);

    /* Should be running */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Flash Error Condition Tests - Requirements 10.6                           */
/*---------------------------------------------------------------------------*/

TEST_F(FlashTest, Error_NullPointer) {
    uint8_t data[16] = {0};

    /* Null flash pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR, flash->read(nullptr, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_NULL_PTR, flash->write(nullptr, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_NULL_PTR, flash->erase(nullptr, 0, sizeof(data)));

    /* Null data pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR, flash->read(flash, 0, nullptr, sizeof(data)));
    EXPECT_EQ(NX_ERR_NULL_PTR, flash->write(flash, 0, nullptr, sizeof(data)));
}

TEST_F(FlashTest, Error_InvalidAddress) {
    uint8_t data[16] = {0};

    /* Address beyond flash size */
    uint32_t invalid_addr = 0xFFFFFFFF;
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              flash->read(flash, invalid_addr, data, sizeof(data)));
}

TEST_F(FlashTest, Error_Uninitialized) {
    /* Deinitialize flash */
    nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Attempt operations on uninitialized flash */
    uint8_t data[16] = {0};
    EXPECT_EQ(NX_ERR_NOT_INIT, flash->read(flash, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_NOT_INIT, flash->write(flash, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_NOT_INIT, flash->erase(flash, 0, sizeof(data)));
}

TEST_F(FlashTest, Error_Suspended) {
    /* Suspend flash */
    nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Attempt operations on suspended flash */
    uint8_t data[16] = {0};
    EXPECT_EQ(NX_ERR_INVALID_STATE, flash->read(flash, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_INVALID_STATE, flash->write(flash, 0, data, sizeof(data)));
    EXPECT_EQ(NX_ERR_INVALID_STATE, flash->erase(flash, 0, sizeof(data)));
}

/* Exercise the production helpers directly: these can also be called during
 * lifecycle persistence, before a fully initialized public Flash exists. */
TEST(FlashHelperValidation, NullStateAndDataReturnWithoutDereference) {
    uint8_t data[4]={1,2,3,4};
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_save_to_file(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_load_from_file(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_erase_sector(nullptr,0));
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_read(nullptr,0,data,sizeof(data)));
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_write(nullptr,0,data,sizeof(data)));
    EXPECT_FALSE(flash_is_erased(nullptr,0,sizeof(data)));
    auto* state=new nx_flash_state_t{};
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_read(state,0,nullptr,sizeof(data)));
    EXPECT_EQ(NX_ERR_NULL_PTR,flash_write(state,0,nullptr,sizeof(data)));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,flash_save_to_file(state));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,flash_load_from_file(state));
    delete state;
}

TEST(FlashHelperValidation, AddressLengthOverflowIsRejected) {
    EXPECT_FALSE(flash_is_valid_address(4,SIZE_MAX));
    EXPECT_FALSE(flash_is_valid_address(NX_FLASH_TOTAL_SIZE,0));
    EXPECT_FALSE(flash_is_valid_address(NX_FLASH_TOTAL_SIZE-4,8));
    EXPECT_TRUE(flash_is_valid_address(NX_FLASH_TOTAL_SIZE-4,4));
}

#ifndef _WIN32
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <unistd.h>

TEST(FlashPersistenceValidation, IOFailuresPropagateAndDeinitCanBeRetried) {
    auto state=std::make_unique<nx_flash_state_t>();
    std::strcpy(state->backing_file,"/dev/full");
    EXPECT_EQ(NX_ERR_IO,flash_save_to_file(state.get()));
    nx_flash_impl_t impl{};
    impl.state=state.get();
    flash_init_lifecycle(&impl.lifecycle);
    state->initialized=true;
    EXPECT_EQ(NX_ERR_IO,impl.lifecycle.deinit(&impl.lifecycle));
    EXPECT_TRUE(state->initialized);
    char path[]="/tmp/nexus-flash-model-XXXXXX";
    int descriptor=mkstemp(path);
    ASSERT_GE(descriptor,0);
    ASSERT_EQ(0,close(descriptor));
    std::strcpy(state->backing_file,path);
    EXPECT_EQ(NX_OK,impl.lifecycle.deinit(&impl.lifecycle));
    EXPECT_FALSE(state->initialized);
    EXPECT_EQ(0,std::remove(path));
}

TEST(FlashPersistenceValidation, MissingFileIsFirstBootButShortOrUnreadableImageIsError) {
    auto state=std::make_unique<nx_flash_state_t>();
    char path[]="/tmp/nexus-flash-load-XXXXXX";
    int descriptor=mkstemp(path);
    ASSERT_GE(descriptor,0);
    const uint8_t partial[]={0x12,0x34};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(partial)),write(descriptor,partial,sizeof(partial)));
    ASSERT_EQ(0,close(descriptor));
    std::strcpy(state->backing_file,path);
    EXPECT_EQ(NX_ERR_IO,flash_load_from_file(state.get()));
    nx_flash_impl_t impl{}; impl.state=state.get();
    flash_init_lifecycle(&impl.lifecycle);
    EXPECT_EQ(NX_ERR_IO,impl.lifecycle.init(&impl.lifecycle));
    EXPECT_FALSE(state->initialized);
    EXPECT_EQ(0,std::remove(path));
    ASSERT_EQ(NX_OK,impl.lifecycle.init(&impl.lifecycle));
    EXPECT_TRUE(state->initialized);
    EXPECT_TRUE(flash_is_erased(state.get(),0,NX_FLASH_TOTAL_SIZE));
    ASSERT_EQ(NX_OK,impl.lifecycle.deinit(&impl.lifecycle));
    EXPECT_EQ(0,std::remove(path));
    std::strcpy(state->backing_file,"/tmp");
    EXPECT_EQ(NX_ERR_IO,flash_load_from_file(state.get()));
    EXPECT_EQ(NX_ERR_IO,impl.lifecycle.init(&impl.lifecycle));
    EXPECT_FALSE(state->initialized);
}
#endif
