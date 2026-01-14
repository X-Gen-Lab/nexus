/**
 * \file            test_shell_command_properties.cpp
 * \brief           Shell Command Registration Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell command registration functionality.
 * These tests verify universal properties that should hold for all valid inputs.
 * Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware
 * **Validates: Requirements 2.1, 2.5, 2.7**
 */

#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <cstring>

extern "C" {
#include "shell/shell_command.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Safe string copy helper
 */
static void safe_strcpy(char* dest, size_t dest_size, const char* src) {
    if (dest == nullptr || src == nullptr || dest_size == 0) return;
    size_t src_len = std::strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    std::memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/** Simple test command handler */
static int test_handler(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 0;
}

/** Test completion callback */
static void test_completion(const char* partial, char* completions[], int* count) {
    (void)partial;
    (void)completions;
    *count = 0;
}

/**
 * \brief           Shell Command Property Test Fixture
 */
class CommandPropertyTest : public ::testing::Test {
protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        shell_clear_commands();
    }

    void TearDown() override {
        shell_clear_commands();
    }

    /**
     * \brief           Generate a random valid command name
     *
     * Command names must be alphanumeric with underscores, 1-15 chars
     */
    std::string randomCommandName(int minLen = 1, int maxLen = 15) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 62);
        
        int len = lenDist(rng);
        std::string str;
        str.reserve(len);
        
        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else if (c < 52) {
                str += static_cast<char>('A' + (c - 26));
            } else if (c < 62) {
                str += static_cast<char>('0' + (c - 52));
            } else {
                str += '_';
            }
        }
        return str;
    }

    /**
     * \brief           Generate a list of unique random command names
     */
    std::vector<std::string> generateUniqueCommandNames(int count) {
        std::set<std::string> nameSet;
        std::vector<std::string> names;
        names.reserve(count);
        
        while (static_cast<int>(names.size()) < count) {
            std::string name = randomCommandName(3, 12);
            if (nameSet.find(name) == nameSet.end()) {
                nameSet.insert(name);
                names.push_back(name);
            }
        }
        return names;
    }

    /**
     * \brief           Generate a random help string
     */
    std::string randomHelpString() {
        std::uniform_int_distribution<int> lenDist(10, 50);
        std::uniform_int_distribution<int> charDist(0, 26);
        
        int len = lenDist(rng);
        std::string str;
        str.reserve(len);
        
        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else {
                str += ' ';
            }
        }
        return str;
    }
};


/*---------------------------------------------------------------------------*/
/* Property 2: Command Registration Round-Trip                               */
/* *For any* valid command with unique name, registering the command,        */
/* retrieving it by name, and then unregistering it SHALL all succeed,       */
/* and the retrieved command SHALL match the registered command.             */
/* **Validates: Requirements 2.1, 2.5, 2.7**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 2: Command Registration Round-Trip
 * 
 * *For any* valid command with unique name, registering the command,
 * retrieving it by name, and then unregistering it SHALL all succeed,
 * and the retrieved command SHALL match the registered command.
 * 
 * **Validates: Requirements 2.1, 2.5, 2.7**
 */
TEST_F(CommandPropertyTest, Property2_CommandRegistrationRoundTrip) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        /* Generate random command */
        std::string name = randomCommandName(3, 12);
        std::string help = randomHelpString();
        std::string usage = name + " [args]";
        
        /* Use static storage for strings that must persist */
        static char s_name[SHELL_MAX_CMD_NAME];
        static char s_help[64];
        static char s_usage[64];
        
        safe_strcpy(s_name, sizeof(s_name), name.c_str());
        safe_strcpy(s_help, sizeof(s_help), help.c_str());
        safe_strcpy(s_usage, sizeof(s_usage), usage.c_str());
        
        shell_command_t cmd = {
            .name = s_name,
            .handler = test_handler,
            .help = s_help,
            .usage = s_usage,
            .completion = test_completion
        };
        
        /* Step 1: Register command */
        shell_status_t regStatus = shell_register_command(&cmd);
        EXPECT_EQ(SHELL_OK, regStatus)
            << "Iter " << iter << ": registration failed for '" << name << "'";
        
        if (regStatus != SHELL_OK) continue;
        
        /* Step 2: Retrieve command by name */
        const shell_command_t* retrieved = shell_get_command(s_name);
        ASSERT_NE(nullptr, retrieved)
            << "Iter " << iter << ": get_command returned null for '" << name << "'";
        
        /* Verify retrieved command matches registered command */
        EXPECT_STREQ(s_name, retrieved->name)
            << "Iter " << iter << ": name mismatch";
        EXPECT_EQ(test_handler, retrieved->handler)
            << "Iter " << iter << ": handler mismatch";
        EXPECT_STREQ(s_help, retrieved->help)
            << "Iter " << iter << ": help mismatch";
        EXPECT_STREQ(s_usage, retrieved->usage)
            << "Iter " << iter << ": usage mismatch";
        EXPECT_EQ(test_completion, retrieved->completion)
            << "Iter " << iter << ": completion mismatch";
        
        /* Step 3: Unregister command */
        shell_status_t unregStatus = shell_unregister_command(s_name);
        EXPECT_EQ(SHELL_OK, unregStatus)
            << "Iter " << iter << ": unregistration failed for '" << name << "'";
        
        /* Verify command is no longer retrievable */
        EXPECT_EQ(nullptr, shell_get_command(s_name))
            << "Iter " << iter << ": command still retrievable after unregister";
        
        /* Verify count is back to 0 */
        EXPECT_EQ(0, shell_get_command_count())
            << "Iter " << iter << ": count should be 0 after unregister";
    }
}

/**
 * Feature: shell-cli-middleware, Property 2a: Multiple Commands Round-Trip
 * 
 * *For any* set of valid commands with unique names, registering all commands,
 * retrieving each by name, and unregistering all SHALL succeed.
 * 
 * **Validates: Requirements 2.1, 2.5, 2.7**
 */
TEST_F(CommandPropertyTest, Property2a_MultipleCommandsRoundTrip) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        /* Generate random number of commands (1 to half of max) */
        std::uniform_int_distribution<int> countDist(1, SHELL_MAX_COMMANDS / 2);
        int cmdCount = countDist(rng);
        
        std::vector<std::string> names = generateUniqueCommandNames(cmdCount);
        std::vector<shell_command_t> cmds(cmdCount);
        
        /* Use static storage for command data */
        static char s_names[SHELL_MAX_COMMANDS][SHELL_MAX_CMD_NAME];
        
        /* Step 1: Register all commands */
        for (int i = 0; i < cmdCount; ++i) {
            safe_strcpy(s_names[i], SHELL_MAX_CMD_NAME, names[i].c_str());
            
            cmds[i].name = s_names[i];
            cmds[i].handler = test_handler;
            cmds[i].help = nullptr;
            cmds[i].usage = nullptr;
            cmds[i].completion = nullptr;
            
            EXPECT_EQ(SHELL_OK, shell_register_command(&cmds[i]))
                << "Iter " << iter << ": failed to register command " << i;
        }
        
        EXPECT_EQ(cmdCount, shell_get_command_count())
            << "Iter " << iter << ": count mismatch after registration";
        
        /* Step 2: Verify all commands are retrievable */
        for (int i = 0; i < cmdCount; ++i) {
            const shell_command_t* retrieved = shell_get_command(s_names[i]);
            ASSERT_NE(nullptr, retrieved)
                << "Iter " << iter << ": command " << i << " not found";
            EXPECT_STREQ(s_names[i], retrieved->name)
                << "Iter " << iter << ": name mismatch for command " << i;
        }
        
        /* Step 3: Unregister all commands */
        for (int i = 0; i < cmdCount; ++i) {
            EXPECT_EQ(SHELL_OK, shell_unregister_command(s_names[i]))
                << "Iter " << iter << ": failed to unregister command " << i;
        }
        
        EXPECT_EQ(0, shell_get_command_count())
            << "Iter " << iter << ": count should be 0 after unregistering all";
    }
}

/**
 * Feature: shell-cli-middleware, Property 2b: Duplicate Registration Rejection
 * 
 * *For any* registered command, attempting to register another command
 * with the same name SHALL fail with SHELL_ERROR_ALREADY_EXISTS.
 * 
 * **Validates: Requirements 2.3**
 */
TEST_F(CommandPropertyTest, Property2b_DuplicateRegistrationRejection) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        std::string name = randomCommandName(3, 12);
        
        static char s_name[SHELL_MAX_CMD_NAME];
        safe_strcpy(s_name, sizeof(s_name), name.c_str());
        
        shell_command_t cmd1 = {
            .name = s_name,
            .handler = test_handler,
            .help = "First command",
            .usage = nullptr,
            .completion = nullptr
        };
        
        shell_command_t cmd2 = {
            .name = s_name,
            .handler = test_handler,
            .help = "Second command",
            .usage = nullptr,
            .completion = nullptr
        };
        
        /* Register first command */
        EXPECT_EQ(SHELL_OK, shell_register_command(&cmd1))
            << "Iter " << iter << ": first registration should succeed";
        
        /* Try to register duplicate */
        EXPECT_EQ(SHELL_ERROR_ALREADY_EXISTS, shell_register_command(&cmd2))
            << "Iter " << iter << ": duplicate registration should fail";
        
        /* Count should still be 1 */
        EXPECT_EQ(1, shell_get_command_count())
            << "Iter " << iter << ": count should remain 1";
    }
}

/**
 * Feature: shell-cli-middleware, Property 2c: Unregister Non-Existent Fails
 * 
 * *For any* command name that is not registered, unregistering it
 * SHALL fail with SHELL_ERROR_NOT_FOUND.
 * 
 * **Validates: Requirements 2.6**
 */
TEST_F(CommandPropertyTest, Property2c_UnregisterNonExistentFails) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        /* Generate two different names */
        std::string registeredName = randomCommandName(3, 12);
        std::string nonExistentName = randomCommandName(3, 12);
        
        /* Ensure they're different */
        while (registeredName == nonExistentName) {
            nonExistentName = randomCommandName(3, 12);
        }
        
        static char s_regName[SHELL_MAX_CMD_NAME];
        static char s_nonExName[SHELL_MAX_CMD_NAME];
        
        safe_strcpy(s_regName, sizeof(s_regName), registeredName.c_str());
        safe_strcpy(s_nonExName, sizeof(s_nonExName), nonExistentName.c_str());
        
        shell_command_t cmd = {
            .name = s_regName,
            .handler = test_handler,
            .help = nullptr,
            .usage = nullptr,
            .completion = nullptr
        };
        
        /* Register one command */
        shell_register_command(&cmd);
        
        /* Try to unregister non-existent command */
        EXPECT_EQ(SHELL_ERROR_NOT_FOUND, shell_unregister_command(s_nonExName))
            << "Iter " << iter << ": unregister non-existent should fail";
        
        /* Original command should still be there */
        EXPECT_NE(nullptr, shell_get_command(s_regName))
            << "Iter " << iter << ": original command should still exist";
    }
}

/**
 * Feature: shell-cli-middleware, Property 2d: Capacity Limit Enforcement
 * 
 * *For any* attempt to register more than SHELL_MAX_COMMANDS commands,
 * the registration SHALL fail with SHELL_ERROR_NO_MEMORY.
 * 
 * **Validates: Requirements 2.4**
 */
TEST_F(CommandPropertyTest, Property2d_CapacityLimitEnforcement) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        /* Generate enough unique names to exceed capacity */
        std::vector<std::string> names = generateUniqueCommandNames(SHELL_MAX_COMMANDS + 1);
        
        static char s_names[SHELL_MAX_COMMANDS + 1][SHELL_MAX_CMD_NAME];
        std::vector<shell_command_t> cmds(SHELL_MAX_COMMANDS + 1);
        
        /* Register up to capacity */
        for (int i = 0; i < SHELL_MAX_COMMANDS; ++i) {
            safe_strcpy(s_names[i], SHELL_MAX_CMD_NAME, names[i].c_str());
            
            cmds[i].name = s_names[i];
            cmds[i].handler = test_handler;
            cmds[i].help = nullptr;
            cmds[i].usage = nullptr;
            cmds[i].completion = nullptr;
            
            EXPECT_EQ(SHELL_OK, shell_register_command(&cmds[i]))
                << "Iter " << iter << ": registration " << i << " should succeed";
        }
        
        EXPECT_EQ(SHELL_MAX_COMMANDS, shell_get_command_count())
            << "Iter " << iter << ": should be at capacity";
        
        /* Try to register one more */
        safe_strcpy(s_names[SHELL_MAX_COMMANDS], SHELL_MAX_CMD_NAME, 
                    names[SHELL_MAX_COMMANDS].c_str());
        
        cmds[SHELL_MAX_COMMANDS].name = s_names[SHELL_MAX_COMMANDS];
        cmds[SHELL_MAX_COMMANDS].handler = test_handler;
        cmds[SHELL_MAX_COMMANDS].help = nullptr;
        cmds[SHELL_MAX_COMMANDS].usage = nullptr;
        cmds[SHELL_MAX_COMMANDS].completion = nullptr;
        
        EXPECT_EQ(SHELL_ERROR_NO_MEMORY, shell_register_command(&cmds[SHELL_MAX_COMMANDS]))
            << "Iter " << iter << ": overflow registration should fail";
        
        /* Count should still be at capacity */
        EXPECT_EQ(SHELL_MAX_COMMANDS, shell_get_command_count())
            << "Iter " << iter << ": count should remain at capacity";
    }
}

/**
 * Feature: shell-cli-middleware, Property 2e: Re-registration After Unregister
 * 
 * *For any* command that has been unregistered, re-registering it
 * SHALL succeed.
 * 
 * **Validates: Requirements 2.1, 2.5**
 */
TEST_F(CommandPropertyTest, Property2e_ReregistrationAfterUnregister) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_clear_commands();
        
        std::string name = randomCommandName(3, 12);
        
        static char s_name[SHELL_MAX_CMD_NAME];
        safe_strcpy(s_name, sizeof(s_name), name.c_str());
        
        shell_command_t cmd = {
            .name = s_name,
            .handler = test_handler,
            .help = nullptr,
            .usage = nullptr,
            .completion = nullptr
        };
        
        /* Register, unregister, re-register cycle */
        std::uniform_int_distribution<int> cycleDist(1, 5);
        int cycles = cycleDist(rng);
        
        for (int c = 0; c < cycles; ++c) {
            EXPECT_EQ(SHELL_OK, shell_register_command(&cmd))
                << "Iter " << iter << ", cycle " << c << ": registration should succeed";
            
            EXPECT_NE(nullptr, shell_get_command(s_name))
                << "Iter " << iter << ", cycle " << c << ": command should be retrievable";
            
            EXPECT_EQ(SHELL_OK, shell_unregister_command(s_name))
                << "Iter " << iter << ", cycle " << c << ": unregistration should succeed";
            
            EXPECT_EQ(nullptr, shell_get_command(s_name))
                << "Iter " << iter << ", cycle " << c << ": command should not be retrievable";
        }
    }
}

