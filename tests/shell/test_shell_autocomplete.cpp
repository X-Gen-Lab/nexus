/**
 * \file            test_shell_autocomplete.cpp
 * \brief           Shell Auto-Completion Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell auto-completion functionality.
 * Requirements: 6.1, 6.2, 6.3, 6.4
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "shell/shell_autocomplete.h"
#include "shell/shell_command.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixtures                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Shell Auto-Completion Test Fixture
 */
class ShellAutocompleteTest : public ::testing::Test {
protected:
    completion_result_t result;

    void SetUp() override {
        /* Clear any previously registered commands */
        shell_clear_commands();
        memset(&result, 0, sizeof(result));
    }

    void TearDown() override {
        shell_clear_commands();
    }

    /* Helper to register a simple test command */
    static int dummy_handler(int argc, char* argv[]) {
        (void)argc;
        (void)argv;
        return 0;
    }

    void registerCommand(const char* name) {
        static shell_command_t cmds[32];
        static int cmd_index = 0;
        
        cmds[cmd_index].name = name;
        cmds[cmd_index].handler = dummy_handler;
        cmds[cmd_index].help = "Test command";
        cmds[cmd_index].usage = name;
        cmds[cmd_index].completion = nullptr;
        
        shell_register_command(&cmds[cmd_index]);
        cmd_index++;
    }
};

/*---------------------------------------------------------------------------*/
/* Command Completion Tests - Requirements 6.1, 6.4                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test completion with unique match
 * \details         Requirements 6.1, 6.4 - Complete to matching command and add space
 */
TEST_F(ShellAutocompleteTest, UniqueMatchCompletion) {
    registerCommand("help");
    registerCommand("version");
    registerCommand("clear");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("hel", &result));
    EXPECT_EQ(1, result.match_count);
    EXPECT_STREQ("help", result.matches[0]);
    EXPECT_EQ(4, result.common_prefix_len);  /* "help" length */
}

/**
 * \brief           Test completion with partial prefix
 * \details         Requirements 6.1 - Complete partial command name
 */
TEST_F(ShellAutocompleteTest, PartialPrefixCompletion) {
    registerCommand("gpio");
    registerCommand("get");
    registerCommand("set");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("g", &result));
    EXPECT_EQ(2, result.match_count);
    /* Both "gpio" and "get" start with "g" */
    EXPECT_EQ(1, result.common_prefix_len);  /* Only "g" is common */
}


/*---------------------------------------------------------------------------*/
/* Multiple Match Tests - Requirements 6.2                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test completion with multiple matches
 * \details         Requirements 6.2 - Show all matching commands
 */
TEST_F(ShellAutocompleteTest, MultipleMatchCompletion) {
    registerCommand("gpio_set");
    registerCommand("gpio_get");
    registerCommand("gpio_toggle");
    registerCommand("help");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("gpio", &result));
    EXPECT_EQ(3, result.match_count);
    /* Common prefix is "gpio_" (5 chars) */
    EXPECT_EQ(5, result.common_prefix_len);
}

/**
 * \brief           Test completion with common prefix calculation
 * \details         Requirements 6.2 - Calculate common prefix for multiple matches
 */
TEST_F(ShellAutocompleteTest, CommonPrefixCalculation) {
    registerCommand("test_alpha");
    registerCommand("test_beta");
    registerCommand("test_gamma");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("test", &result));
    EXPECT_EQ(3, result.match_count);
    /* Common prefix is "test_" (5 chars) */
    EXPECT_EQ(5, result.common_prefix_len);
    
    /* Verify we can get the common prefix */
    char prefix[32];
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    EXPECT_EQ(5, len);
    EXPECT_STREQ("test_", prefix);
}

/*---------------------------------------------------------------------------*/
/* No Match Tests - Requirements 6.3                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test completion with no matches
 * \details         Requirements 6.3 - Do nothing when no matches
 */
TEST_F(ShellAutocompleteTest, NoMatchCompletion) {
    registerCommand("help");
    registerCommand("version");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("xyz", &result));
    EXPECT_EQ(0, result.match_count);
    EXPECT_EQ(0, result.common_prefix_len);
}

/**
 * \brief           Test completion with empty command registry
 * \details         Requirements 6.3 - Handle empty registry gracefully
 */
TEST_F(ShellAutocompleteTest, EmptyRegistryCompletion) {
    /* No commands registered */
    EXPECT_EQ(SHELL_OK, autocomplete_command("help", &result));
    EXPECT_EQ(0, result.match_count);
}

/*---------------------------------------------------------------------------*/
/* Edge Case Tests                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test completion with empty partial string
 * \details         Edge case: Empty string should match all commands
 */
TEST_F(ShellAutocompleteTest, EmptyPartialCompletion) {
    registerCommand("help");
    registerCommand("version");
    registerCommand("clear");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("", &result));
    EXPECT_EQ(3, result.match_count);
}

/**
 * \brief           Test completion with NULL partial string
 * \details         Edge case: NULL should be treated as empty string
 */
TEST_F(ShellAutocompleteTest, NullPartialCompletion) {
    registerCommand("help");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command(nullptr, &result));
    EXPECT_EQ(1, result.match_count);
}

/**
 * \brief           Test completion with NULL result parameter
 * \details         Edge case: NULL result should return error
 */
TEST_F(ShellAutocompleteTest, NullResultParameter) {
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, autocomplete_command("help", nullptr));
}

/**
 * \brief           Test completion with exact match
 * \details         Edge case: Exact command name should still match
 */
TEST_F(ShellAutocompleteTest, ExactMatchCompletion) {
    registerCommand("help");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("help", &result));
    EXPECT_EQ(1, result.match_count);
    EXPECT_STREQ("help", result.matches[0]);
}


/*---------------------------------------------------------------------------*/
/* Process Function Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test autocomplete_process with command at cursor
 * \details         Test the main entry point for Tab completion
 */
TEST_F(ShellAutocompleteTest, ProcessCommandCompletion) {
    registerCommand("help");
    registerCommand("history");
    
    const char* input = "hel";
    EXPECT_EQ(SHELL_OK, autocomplete_process(input, 3, 3, &result));
    EXPECT_EQ(1, result.match_count);
    EXPECT_STREQ("help", result.matches[0]);
}

/**
 * \brief           Test autocomplete_process with multiple matches
 * \details         Test process function with multiple matching commands
 */
TEST_F(ShellAutocompleteTest, ProcessMultipleMatches) {
    registerCommand("help");
    registerCommand("history");
    
    const char* input = "h";
    EXPECT_EQ(SHELL_OK, autocomplete_process(input, 1, 1, &result));
    EXPECT_EQ(2, result.match_count);
}

/**
 * \brief           Test autocomplete_process with leading whitespace
 * \details         Test that leading whitespace is handled correctly
 */
TEST_F(ShellAutocompleteTest, ProcessWithLeadingWhitespace) {
    registerCommand("help");
    
    const char* input = "  hel";
    EXPECT_EQ(SHELL_OK, autocomplete_process(input, 5, 5, &result));
    EXPECT_EQ(1, result.match_count);
    EXPECT_STREQ("help", result.matches[0]);
}

/**
 * \brief           Test autocomplete_process with NULL input
 * \details         Edge case: NULL input should return all commands
 */
TEST_F(ShellAutocompleteTest, ProcessNullInput) {
    registerCommand("help");
    
    EXPECT_EQ(SHELL_OK, autocomplete_process(nullptr, 0, 0, &result));
    EXPECT_EQ(1, result.match_count);
}

/**
 * \brief           Test autocomplete_process with NULL result
 * \details         Edge case: NULL result should return error
 */
TEST_F(ShellAutocompleteTest, ProcessNullResult) {
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, autocomplete_process("help", 4, 4, nullptr));
}

/*---------------------------------------------------------------------------*/
/* Common Prefix Extraction Tests                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test get_common_prefix with valid result
 * \details         Test extracting common prefix from completion results
 */
TEST_F(ShellAutocompleteTest, GetCommonPrefixValid) {
    registerCommand("gpio_set");
    registerCommand("gpio_get");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("gpio", &result));
    
    char prefix[32];
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    EXPECT_EQ(5, len);  /* "gpio_" */
    EXPECT_STREQ("gpio_", prefix);
}

/**
 * \brief           Test get_common_prefix with no matches
 * \details         Edge case: No matches should return empty prefix
 */
TEST_F(ShellAutocompleteTest, GetCommonPrefixNoMatches) {
    EXPECT_EQ(SHELL_OK, autocomplete_command("xyz", &result));
    
    char prefix[32];
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    EXPECT_EQ(0, len);
    EXPECT_STREQ("", prefix);
}

/**
 * \brief           Test get_common_prefix with NULL parameters
 * \details         Edge case: NULL parameters should return 0
 */
TEST_F(ShellAutocompleteTest, GetCommonPrefixNullParams) {
    char prefix[32];
    
    EXPECT_EQ(0, autocomplete_get_common_prefix(nullptr, prefix, sizeof(prefix)));
    EXPECT_EQ(0, autocomplete_get_common_prefix(&result, nullptr, sizeof(prefix)));
    EXPECT_EQ(0, autocomplete_get_common_prefix(&result, prefix, 0));
}

/**
 * \brief           Test get_common_prefix with small buffer
 * \details         Edge case: Small buffer should truncate prefix
 */
TEST_F(ShellAutocompleteTest, GetCommonPrefixSmallBuffer) {
    registerCommand("longcommandname");
    
    EXPECT_EQ(SHELL_OK, autocomplete_command("long", &result));
    
    char prefix[5];  /* Only 4 chars + null */
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    EXPECT_EQ(4, len);
    EXPECT_STREQ("long", prefix);
}
