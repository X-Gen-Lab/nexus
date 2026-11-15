/**
 * \file            rtc_lptim_generated_test.cpp
 * \brief           Run the canonical public C scene once in each fresh process
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <gtest/gtest.h>
extern "C" int nx_native_time_consumer(void);
namespace {
TEST(NativeTimeGenerated, PublicCScenarioUsesRealFactoryAndPlatform) {
    EXPECT_EQ(nx_native_time_consumer(), 0);
}
} /* namespace */
