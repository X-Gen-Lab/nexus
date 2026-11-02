/**
 * \file            tls_defaults_test.cpp
 * \brief           Fresh-process backend defaults reject unowned allocation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mbedtls/platform.h"
#include <gtest/gtest.h>
TEST(TlsPlatformDefaults, NoAllocationBeforeExplicitArenaOwnership) {
    void* memory = mbedtls_calloc(1U, 16U);
    EXPECT_EQ(nullptr, memory);
    if (memory != nullptr) {
        mbedtls_free(memory);
    }
}
