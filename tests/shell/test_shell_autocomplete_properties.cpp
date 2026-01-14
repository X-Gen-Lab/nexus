/**
 * \file            test_shell_autocomplete_properties.cpp
 * \brief           Shell Auto-Completion Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell auto-completion functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware, Property 7: Auto-Completion Prefix Match
 * **Validates: Requirements 6.1, 6.2, 6.4**
 */

#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell_autocomplete.h"
#include "shell/shell_command.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Shell Auto-Completion Property Test Fixture
 */
class ShellAutocompletePropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    completion_result_t result;

    /* Static storage for test commands */
    static constexpr int MAX_TEST_COMMANDS = 20;
    shell_command_t test_commands[MAX_TEST_COMMANDS];
    char command_names[MAX_TEST_COMMANDS][SHELL_MAX_CMD_NAME + 1];
    int registered_count;

    void SetUp() override {
        rng.seed(std::random_device{}());
        shell_clear_commands();
        memset(&result, 0, sizeof(result));
        memset(test_commands, 0, sizeof(test_commands));
        memset(command_names, 0, sizeof(command_names));
        registered_count = 0;
    }

    void TearDown() override {
        shell_clear_commands();
    }

    static int dummy_handler(int argc, char* argv[]) {
        (void)argc;
        (void)argv;
        return 0;
    }

    /**
     * \brief           Generate a random lowercase alphanumeric string
     */
    std::string randomCommandName(int minLen, int maxLen) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 35);

        int len = lenDist(rng);
        std::string str;
        str.reserve(len);

        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else {
                str += static_cast<char>('0' + (c - 26));
            }
        }
        return str;
    }

    /**
     * \brief           Register a command with the given name
     */
    bool registerCommand(const std::string& name) {
        if (registered_count >= MAX_TEST_COMMANDS) {
            return false;
        }

        /* Copy name to persistent storage */
        size_t len = name.length();
        if (len > SHELL_MAX_CMD_NAME) {
            len = SHELL_MAX_CMD_NAME;
        }
        memcpy(command_names[registered_count], name.c_str(), len);
        command_names[registered_count][len] = '\0';

        /* Set up command structure */
        test_commands[registered_count].name = command_names[registered_count];
        test_commands[registered_count].handler = dummy_handler;
        test_commands[registered_count].help = "Test command";
        test_commands[registered_count].usage = command_names[registered_count];
        test_commands[registered_count].completion = nullptr;

        shell_status_t status =
            shell_register_command(&test_commands[registered_count]);
        if (status == SHELL_OK) {
            registered_count++;
            return true;
        }
        return false;
    }

    /**
     * \brief           Check if a string starts with a given prefix
     */
    bool startsWith(const std::string& str, const std::string& prefix) {
        if (prefix.length() > str.length()) {
            return false;
        }
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    /**
     * \brief           Calculate common prefix of a vector of strings
     */
    int calculateCommonPrefix(const std::vector<std::string>& strings) {
        if (strings.empty()) {
            return 0;
        }
        if (strings.size() == 1) {
            return static_cast<int>(strings[0].length());
        }

        int minLen = static_cast<int>(strings[0].length());
        for (const auto& s : strings) {
            minLen = std::min(minLen, static_cast<int>(s.length()));
        }

        int commonLen = 0;
        for (int i = 0; i < minLen; ++i) {
            char c = strings[0][i];
            bool allMatch = true;
            for (const auto& s : strings) {
                if (s[i] != c) {
                    allMatch = false;
                    break;
                }
            }
            if (allMatch) {
                commonLen++;
            } else {
                break;
            }
        }
        return commonLen;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 7: Auto-Completion Prefix Match                                  */
/* *For any* partial command input, auto-completion SHALL return only        */
/* commands whose names start with the partial input, and the common prefix  */
/* of all matches SHALL be correctly computed.                               */
/* **Validates: Requirements 6.1, 6.2, 6.4**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 7: Auto-Completion Prefix Match
 *
 * *For any* partial command input, auto-completion SHALL return only
 * commands whose names start with the partial input, and the common prefix
 * of all matches SHALL be correctly computed.
 *
 * **Validates: Requirements 6.1, 6.2, 6.4**
 */
TEST_F(ShellAutocompletePropertyTest, Property7_AutoCompletionPrefixMatch) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Clear previous commands */
        shell_clear_commands();
        registered_count = 0;

        /* Generate random number of commands (3-10) */
        std::uniform_int_distribution<int> cmdCountDist(3, 10);
        int cmdCount = cmdCountDist(rng);

        /* Generate unique command names */
        std::vector<std::string> commandNames;
        for (int i = 0; i < cmdCount; ++i) {
            std::string name;
            bool unique;
            do {
                name = randomCommandName(3, 12);
                unique = std::find(commandNames.begin(), commandNames.end(),
                                   name) == commandNames.end();
            } while (!unique);

            commandNames.push_back(name);
            registerCommand(name);
        }

        /* Pick a random command and use part of it as prefix */
        std::uniform_int_distribution<int> cmdIdxDist(0, cmdCount - 1);
        int selectedIdx = cmdIdxDist(rng);
        std::string selectedCmd = commandNames[selectedIdx];

        /* Generate a prefix of random length (1 to full length) */
        std::uniform_int_distribution<int> prefixLenDist(
            1, static_cast<int>(selectedCmd.length()));
        int prefixLen = prefixLenDist(rng);
        std::string prefix = selectedCmd.substr(0, prefixLen);

        /* Calculate expected matches */
        std::vector<std::string> expectedMatches;
        for (const auto& cmd : commandNames) {
            if (startsWith(cmd, prefix)) {
                expectedMatches.push_back(cmd);
            }
        }

        /* Perform auto-completion */
        shell_status_t status = autocomplete_command(prefix.c_str(), &result);

        /* Verify completion succeeded */
        ASSERT_EQ(SHELL_OK, status)
            << "Iteration " << iter
            << ": autocomplete failed for prefix: " << prefix;

        /* Verify match count */
        EXPECT_EQ(static_cast<int>(expectedMatches.size()), result.match_count)
            << "Iteration " << iter
            << ": match count mismatch for prefix: " << prefix;

        /* Verify all returned matches start with the prefix */
        for (int i = 0; i < result.match_count; ++i) {
            std::string match(result.matches[i]);
            EXPECT_TRUE(startsWith(match, prefix))
                << "Iteration " << iter << ": match '" << match
                << "' does not start with prefix '" << prefix << "'";
        }

        /* Verify common prefix calculation */
        if (result.match_count > 0) {
            int expectedCommonPrefix = calculateCommonPrefix(expectedMatches);
            EXPECT_EQ(expectedCommonPrefix, result.common_prefix_len)
                << "Iteration " << iter
                << ": common prefix length mismatch for prefix: " << prefix;
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property: No False Positives
 *
 * *For any* partial input that doesn't match any command prefix,
 * auto-completion SHALL return zero matches.
 *
 * **Validates: Requirements 6.3**
 */
TEST_F(ShellAutocompletePropertyTest, Property_NoFalsePositives) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Clear previous commands */
        shell_clear_commands();
        registered_count = 0;

        /* Register commands with a specific prefix pattern */
        std::vector<std::string> prefixes = {"alpha", "beta", "gamma"};
        for (const auto& p : prefixes) {
            std::string name = p + randomCommandName(1, 5);
            registerCommand(name);
        }

        /* Generate a prefix that won't match any command */
        std::vector<std::string> nonMatchingPrefixes = {"xyz", "qqq", "zzz",
                                                        "www"};
        std::uniform_int_distribution<int> prefixDist(
            0, static_cast<int>(nonMatchingPrefixes.size()) - 1);
        std::string prefix = nonMatchingPrefixes[prefixDist(rng)];

        /* Perform auto-completion */
        shell_status_t status = autocomplete_command(prefix.c_str(), &result);

        /* Verify completion succeeded with zero matches */
        ASSERT_EQ(SHELL_OK, status)
            << "Iteration " << iter << ": autocomplete failed";
        EXPECT_EQ(0, result.match_count)
            << "Iteration " << iter
            << ": expected no matches for prefix: " << prefix;
        EXPECT_EQ(0, result.common_prefix_len)
            << "Iteration " << iter
            << ": common prefix should be 0 for no matches";
    }
}

/**
 * Feature: shell-cli-middleware, Property: Unique Match Completeness
 *
 * *For any* partial input with exactly one matching command,
 * auto-completion SHALL return that command with common_prefix_len
 * equal to the full command name length.
 *
 * **Validates: Requirements 6.4**
 */
TEST_F(ShellAutocompletePropertyTest, Property_UniqueMatchCompleteness) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Clear previous commands */
        shell_clear_commands();
        registered_count = 0;

        /* Generate commands with distinct prefixes */
        std::string uniqueCmd = "unique" + randomCommandName(3, 6);
        registerCommand(uniqueCmd);

        /* Register other commands that won't match "unique" prefix */
        registerCommand("alpha" + randomCommandName(1, 4));
        registerCommand("beta" + randomCommandName(1, 4));
        registerCommand("gamma" + randomCommandName(1, 4));

        /* Use "unique" as prefix - should match only one command */
        std::string prefix = "unique";

        /* Perform auto-completion */
        shell_status_t status = autocomplete_command(prefix.c_str(), &result);

        /* Verify exactly one match */
        ASSERT_EQ(SHELL_OK, status)
            << "Iteration " << iter << ": autocomplete failed";
        EXPECT_EQ(1, result.match_count)
            << "Iteration " << iter << ": expected exactly one match";

        /* Verify common prefix equals full command length */
        if (result.match_count == 1) {
            EXPECT_EQ(static_cast<int>(uniqueCmd.length()),
                      result.common_prefix_len)
                << "Iteration " << iter
                << ": common prefix should equal full command length";
            EXPECT_STREQ(uniqueCmd.c_str(), result.matches[0])
                << "Iteration " << iter
                << ": matched command should be the unique command";
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property: Empty Prefix Matches All
 *
 * *For any* set of registered commands, an empty prefix SHALL match
 * all registered commands.
 *
 * **Validates: Requirements 6.1**
 */
TEST_F(ShellAutocompletePropertyTest, Property_EmptyPrefixMatchesAll) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Clear previous commands */
        shell_clear_commands();
        registered_count = 0;

        /* Generate random number of commands (1-10) */
        std::uniform_int_distribution<int> cmdCountDist(1, 10);
        int cmdCount = cmdCountDist(rng);

        /* Register unique commands */
        std::vector<std::string> commandNames;
        for (int i = 0; i < cmdCount; ++i) {
            std::string name;
            bool unique;
            do {
                name = randomCommandName(3, 10);
                unique = std::find(commandNames.begin(), commandNames.end(),
                                   name) == commandNames.end();
            } while (!unique);

            commandNames.push_back(name);
            registerCommand(name);
        }

        /* Perform auto-completion with empty prefix */
        shell_status_t status = autocomplete_command("", &result);

        /* Verify all commands are matched */
        ASSERT_EQ(SHELL_OK, status)
            << "Iteration " << iter << ": autocomplete failed";
        EXPECT_EQ(cmdCount, result.match_count)
            << "Iteration " << iter << ": expected all " << cmdCount
            << " commands to match";
    }
}
