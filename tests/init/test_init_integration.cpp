/** Explicit registry and firmware metadata integration; no hidden main call. */
#include <gtest/gtest.h>
#include "nx_firmware_info.h"
#include "nx_init.h"

TEST(InitIntegration, RegistryStatisticsMatchCompletion) {
    nx_status_t status = nx_init_run();
    nx_init_stats_t stats{};
    ASSERT_EQ(nx_init_get_stats(&stats), NX_OK);
    EXPECT_EQ(stats.total_count, stats.success_count + stats.fail_count);
    EXPECT_EQ(status, stats.fail_count ? NX_ERR_GENERIC : NX_OK);
    EXPECT_EQ(nx_init_is_complete(), stats.fail_count == 0);
}

TEST(InitIntegration, RepeatedRunRetainsStatisticsAndResult) {
    nx_status_t first = nx_init_run();
    nx_init_stats_t before{}, after{};
    ASSERT_EQ(nx_init_get_stats(&before), NX_OK);
    EXPECT_EQ(nx_init_run(), first);
    ASSERT_EQ(nx_init_get_stats(&after), NX_OK);
    EXPECT_EQ(after.total_count, before.total_count);
    EXPECT_EQ(after.success_count, before.success_count);
    EXPECT_EQ(after.fail_count, before.fail_count);
    EXPECT_EQ(after.last_error, before.last_error);
}

TEST(InitIntegration, NullOutputIsRejected) {
    EXPECT_EQ(nx_init_get_stats(nullptr), NX_ERR_NULL_PTR);
    EXPECT_EQ(nx_get_version_string(nullptr, 32), 0u);
}

TEST(InitIntegration, ExternalFirmwareVersionEncodingRoundTrips) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);
    EXPECT_EQ(version, 0x01020304u);
    EXPECT_EQ(NX_VERSION_MAJOR(version), 1u);
    EXPECT_EQ(NX_VERSION_MINOR(version), 2u);
    EXPECT_EQ(NX_VERSION_PATCH(version), 3u);
    EXPECT_EQ(NX_VERSION_BUILD(version), 4u);
}
