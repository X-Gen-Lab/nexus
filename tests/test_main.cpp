/**
 * \file            test_main.cpp
 * \brief           Test Framework Main Entry Point
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/base/nx_device.h"
#include <gtest/gtest.h>

/* Include native test helpers for manual device registration */
#if NX_DEVICE_MANUAL_REGISTRATION
#include "native_test_helpers.h"
#endif

/**
 * \brief           Main entry point for tests
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument values
 * \return          Test result
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

#if NX_DEVICE_MANUAL_REGISTRATION
    /* Setup devices before running tests (manual registration) */
    native_test_setup_devices();
#endif

    int result = RUN_ALL_TESTS();

#if NX_DEVICE_MANUAL_REGISTRATION
    /* Cleanup devices after running tests */
    native_test_cleanup_devices();
#endif

    return result;
}
