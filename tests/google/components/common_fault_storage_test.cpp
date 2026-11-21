/**
 * \file            common_fault_storage_test.cpp
 * \brief           Crash retention is acknowledged only after durable save
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "common_storage_model.h"
#include "nexus/components/fault_storage.h"
#include "nexus/components/flash_storage.h"
#include <array>
#include <gtest/gtest.h>

/** \brief External clock boundary for the deterministic Flash model only. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 10U;
}

namespace {
TEST(CommonFaultStorage, SuccessfulNormalContextCommitAcknowledgesRetention) {
    common_storage::Flash flash;
    nx_flash_storage_adapter_t adapter{};
    nx_storage_port_t port{};
    ASSERT_EQ(nx_flash_storage_bind(&adapter, flash.region(), 1000, &port),
              NX_SUCCESS);
    std::array<uint8_t, 32> scratch{};
    nx_storage_t storage{};
    ASSERT_EQ(nx_storage_open(&storage, &port, 0, 4096, scratch.data(),
                              scratch.size()),
              NX_STORAGE_OK);
    nx_fault_record_t retained{};
    nx_fault_capture_t crash{};
    crash.sequence = 45;
    crash.core[6] = 0x08001234;
    ASSERT_EQ(nx_fault_record_capture(&retained, &crash), NX_SUCCESS);
    ASSERT_EQ(nx_fault_record_persist(&retained, &storage), NX_STORAGE_OK);
    nx_fault_capture_t decoded{};
    EXPECT_EQ(nx_fault_record_read(&retained, &decoded), NX_ERROR_EMPTY);
    nx_fault_record_t durable{};
    size_t count = sizeof(durable);
    ASSERT_EQ(nx_storage_load(&storage, &durable, &count), NX_STORAGE_OK);
    EXPECT_EQ(count, NX_FAULT_RECORD_BYTES);
    ASSERT_EQ(nx_fault_record_read(&durable, &decoded), NX_SUCCESS);
    EXPECT_EQ(decoded.sequence, 45U);
    EXPECT_EQ(decoded.core[6], crash.core[6]);
}

TEST(CommonFaultStorage, MutationFailureKeepsOriginalCaptureAndRequiresReopen) {
    common_storage::Flash flash;
    nx_flash_storage_adapter_t adapter{};
    nx_storage_port_t port{};
    ASSERT_EQ(nx_flash_storage_bind(&adapter, flash.region(), 1000, &port),
              NX_SUCCESS);
    std::array<uint8_t, 32> scratch{};
    nx_storage_t storage{};
    ASSERT_EQ(nx_storage_open(&storage, &port, 0, 4096, scratch.data(),
                              scratch.size()),
              NX_STORAGE_OK);
    nx_fault_record_t retained{};
    nx_fault_capture_t crash{};
    crash.sequence = 46;
    ASSERT_EQ(nx_fault_record_capture(&retained, &crash), NX_SUCCESS);
    flash.fail_after = 0;
    EXPECT_EQ(nx_fault_record_persist(&retained, &storage), NX_STORAGE_IO);
    EXPECT_FALSE(storage.opened);
    nx_fault_capture_t decoded{};
    ASSERT_EQ(nx_fault_record_read(&retained, &decoded), NX_SUCCESS);
    EXPECT_EQ(decoded.sequence, 46U);
    flash.fail_after = SIZE_MAX;
    ASSERT_EQ(nx_storage_open(&storage, &port, 0, 4096, scratch.data(),
                              scratch.size()),
              NX_STORAGE_OK);
    EXPECT_EQ(nx_fault_record_persist(&retained, &storage), NX_STORAGE_OK);
}

TEST(CommonFaultStorage, MissingOrCorruptCaptureNeverMutatesStorage) {
    nx_fault_record_t retained{};
    nx_storage_t storage{};
    EXPECT_EQ(nx_fault_record_persist(&retained, &storage),
              NX_STORAGE_NOT_FOUND);
    nx_fault_capture_t crash{};
    crash.sequence = 47;
    ASSERT_EQ(nx_fault_record_capture(&retained, &crash), NX_SUCCESS);
    retained.bytes[48] ^= 2;
    EXPECT_EQ(nx_fault_record_persist(&retained, &storage), NX_STORAGE_CORRUPT);
    EXPECT_NE(retained.committed, 0U);
}
} /* namespace */
