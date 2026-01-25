/**
 * \file            test_main_common.cpp
 * \brief           Common Test Framework Main Entry Point
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Generic test entry point for non-HAL tests
 */

#include <gtest/gtest.h>

/**
 * \brief           Main entry point for tests
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument values
 * \return          Test result
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
