/**
 * \file            test_main.cpp
 * \brief           Test Framework Main Entry Point
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include <gtest/gtest.h>

/* Include native test helpers for MSVC device setup */
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
#include "hal/native/native_test_helpers.h"
#endif

/**
 * \brief           Main entry point for tests
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument values
 * \return          Test result
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* MSVC: Setup devices before running tests */
    native_test_setup_devices();
#endif

    int result = RUN_ALL_TESTS();

#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* MSVC: Cleanup devices after running tests */
    native_test_cleanup_devices();
#endif

    return result;
}
