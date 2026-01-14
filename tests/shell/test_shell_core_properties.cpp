/**
 * \file            test_shell_core_properties.cpp
 * \brief           Shell Core Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell core initialization and deinitialization.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware
 * **Validates: Requirements 1.1, 1.6**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>

extern "C" {
#include "shell/shell.h"
#include "shell/shell_command.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Shell Core Property Test Fixture
 */
class ShellCorePropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        shell_clear_commands();
        if (shell_is_initialized()) {
            shell_deinit();
        }
    }

    void TearDown() override {
        if (shell_is_initialized()) {
            shell_deinit();
        }
        shell_clear_commands();
    }

    /**
     * \brief           Generate a random valid prompt string
     * \param[in]       maxLen: Maximum length (up to 16)
     * \return          Random prompt string
     */
    std::string randomPrompt(int maxLen = SHELL_MAX_PROMPT_LEN) {
        std::uniform_int_distribution<int> lenDist(1, maxLen);
        std::uniform_int_distribution<int> charDist(32, 126);

        int len = lenDist(rng);
        std::string str;
        str.reserve(len);

        for (int i = 0; i < len; ++i) {
            str += static_cast<char>(charDist(rng));
        }
        return str;
    }

    /**
     * \brief           Generate a random valid buffer size
     * \return          Random buffer size in valid range
     */
    uint16_t randomBufferSize() {
        std::uniform_int_distribution<int> dist(SHELL_MIN_CMD_BUFFER_SIZE,
                                                SHELL_MAX_CMD_BUFFER_SIZE);
        return static_cast<uint16_t>(dist(rng));
    }

    /**
     * \brief           Generate a random valid history depth
     * \return          Random history depth in valid range
     */
    uint8_t randomHistoryDepth() {
        std::uniform_int_distribution<int> dist(SHELL_MIN_HISTORY_DEPTH,
                                                SHELL_MAX_HISTORY_DEPTH);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief           Generate a random valid configuration
     * \return          Random valid shell configuration
     */
    shell_config_t randomValidConfig() {
        static char s_prompt[SHELL_MAX_PROMPT_LEN + 1];
        std::string prompt = randomPrompt();
        std::memcpy(s_prompt, prompt.c_str(), prompt.length());
        s_prompt[prompt.length()] = '\0';

        shell_config_t config = {.prompt = s_prompt,
                                 .cmd_buffer_size = randomBufferSize(),
                                 .history_depth = randomHistoryDepth(),
                                 .max_commands = SHELL_MAX_COMMANDS};
        return config;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Init/Deinit Round-Trip                                        */
/* *For any* valid shell configuration, initializing and then deinitializing */
/* the shell SHALL return SHELL_OK for both operations, and the shell SHALL  */
/* be in uninitialized state after deinit.                                   */
/* **Validates: Requirements 1.1, 1.6**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 1: Init/Deinit Round-Trip
 *
 * *For any* valid shell configuration, initializing and then deinitializing
 * the shell SHALL return SHELL_OK for both operations, and the shell SHALL
 * be in uninitialized state after deinit.
 *
 * **Validates: Requirements 1.1, 1.6**
 */
TEST_F(ShellCorePropertyTest, Property1_InitDeinitRoundTrip) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Ensure clean state */
        if (shell_is_initialized()) {
            shell_deinit();
        }

        /* Generate random valid configuration */
        shell_config_t config = randomValidConfig();

        /* Step 1: Initialize shell */
        shell_status_t initStatus = shell_init(&config);
        EXPECT_EQ(SHELL_OK, initStatus)
            << "Iter " << iter
            << ": init failed with buffer_size=" << config.cmd_buffer_size
            << ", history_depth=" << static_cast<int>(config.history_depth);

        if (initStatus != SHELL_OK) {
            continue;
        }

        /* Verify shell is initialized */
        EXPECT_TRUE(shell_is_initialized())
            << "Iter " << iter << ": shell should be initialized after init";

        /* Step 2: Deinitialize shell */
        shell_status_t deinitStatus = shell_deinit();
        EXPECT_EQ(SHELL_OK, deinitStatus)
            << "Iter " << iter << ": deinit failed";

        /* Verify shell is uninitialized */
        EXPECT_FALSE(shell_is_initialized())
            << "Iter " << iter
            << ": shell should be uninitialized after deinit";
    }
}

/**
 * Feature: shell-cli-middleware, Property 1a: Multiple Init/Deinit Cycles
 *
 * *For any* valid shell configuration, performing multiple init/deinit cycles
 * SHALL succeed for each cycle.
 *
 * **Validates: Requirements 1.1, 1.6**
 */
TEST_F(ShellCorePropertyTest, Property1a_MultipleInitDeinitCycles) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Ensure clean state */
        if (shell_is_initialized()) {
            shell_deinit();
        }

        /* Random number of cycles (1-5) */
        std::uniform_int_distribution<int> cycleDist(1, 5);
        int cycles = cycleDist(rng);

        for (int c = 0; c < cycles; ++c) {
            /* Generate random valid configuration for each cycle */
            shell_config_t config = randomValidConfig();

            /* Initialize */
            shell_status_t initStatus = shell_init(&config);
            EXPECT_EQ(SHELL_OK, initStatus)
                << "Iter " << iter << ", cycle " << c << ": init failed";

            if (initStatus != SHELL_OK) {
                break;
            }

            EXPECT_TRUE(shell_is_initialized())
                << "Iter " << iter << ", cycle " << c
                << ": should be initialized";

            /* Deinitialize */
            shell_status_t deinitStatus = shell_deinit();
            EXPECT_EQ(SHELL_OK, deinitStatus)
                << "Iter " << iter << ", cycle " << c << ": deinit failed";

            EXPECT_FALSE(shell_is_initialized())
                << "Iter " << iter << ", cycle " << c
                << ": should be uninitialized";
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property 1b: Init Idempotence Prevention
 *
 * *For any* initialized shell, attempting to initialize again SHALL fail
 * with SHELL_ERROR_ALREADY_INIT.
 *
 * **Validates: Requirements 1.3**
 */
TEST_F(ShellCorePropertyTest, Property1b_InitIdempotencePrevention) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Ensure clean state */
        if (shell_is_initialized()) {
            shell_deinit();
        }

        /* Generate two random configurations */
        shell_config_t config1 = randomValidConfig();
        shell_config_t config2 = randomValidConfig();

        /* First init should succeed */
        EXPECT_EQ(SHELL_OK, shell_init(&config1))
            << "Iter " << iter << ": first init should succeed";

        /* Second init should fail */
        EXPECT_EQ(SHELL_ERROR_ALREADY_INIT, shell_init(&config2))
            << "Iter " << iter << ": second init should fail";

        /* Shell should still be initialized */
        EXPECT_TRUE(shell_is_initialized())
            << "Iter " << iter << ": shell should remain initialized";

        /* Cleanup */
        shell_deinit();
    }
}

/**
 * Feature: shell-cli-middleware, Property 1c: Deinit Without Init Fails
 *
 * *For any* uninitialized shell, attempting to deinitialize SHALL fail
 * with SHELL_ERROR_NOT_INIT.
 *
 * **Validates: Requirements 1.6**
 */
TEST_F(ShellCorePropertyTest, Property1c_DeinitWithoutInitFails) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Ensure clean state */
        if (shell_is_initialized()) {
            shell_deinit();
        }

        /* Deinit without init should fail */
        EXPECT_EQ(SHELL_ERROR_NOT_INIT, shell_deinit())
            << "Iter " << iter << ": deinit without init should fail";

        /* Shell should remain uninitialized */
        EXPECT_FALSE(shell_is_initialized())
            << "Iter " << iter << ": shell should remain uninitialized";
    }
}

/**
 * Feature: shell-cli-middleware, Property 1d: Config Validation
 *
 * *For any* configuration with invalid parameters, initialization SHALL fail
 * with SHELL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 1.2, 1.4, 1.5**
 */
TEST_F(ShellCorePropertyTest, Property1d_ConfigValidation) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Ensure clean state */
        if (shell_is_initialized()) {
            shell_deinit();
        }

        /* Test invalid buffer sizes */
        std::uniform_int_distribution<int> invalidSmallDist(
            1, SHELL_MIN_CMD_BUFFER_SIZE - 1);
        std::uniform_int_distribution<int> invalidLargeDist(
            SHELL_MAX_CMD_BUFFER_SIZE + 1, SHELL_MAX_CMD_BUFFER_SIZE + 100);

        shell_config_t config = randomValidConfig();

        /* Test too small buffer */
        config.cmd_buffer_size = static_cast<uint16_t>(invalidSmallDist(rng));
        EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config))
            << "Iter " << iter << ": too small buffer should fail";
        EXPECT_FALSE(shell_is_initialized());

        /* Test too large buffer */
        config.cmd_buffer_size = static_cast<uint16_t>(invalidLargeDist(rng));
        EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config))
            << "Iter " << iter << ": too large buffer should fail";
        EXPECT_FALSE(shell_is_initialized());

        /* Test invalid history depths */
        config = randomValidConfig();

        /* Only test if min > 1 to avoid underflow */
        constexpr uint8_t minHistDepth = SHELL_MIN_HISTORY_DEPTH;
        if constexpr (minHistDepth > 1) {
            config.history_depth = minHistDepth - 1;
            EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config))
                << "Iter " << iter << ": too small history should fail";
            EXPECT_FALSE(shell_is_initialized());
        }

        config.history_depth = SHELL_MAX_HISTORY_DEPTH + 1;
        EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config))
            << "Iter " << iter << ": too large history should fail";
        EXPECT_FALSE(shell_is_initialized());
    }
}
