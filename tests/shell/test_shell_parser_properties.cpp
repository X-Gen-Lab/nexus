/**
 * \file            test_shell_parser_properties.cpp
 * \brief           Shell Parser Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell command line parser.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware, Property 3: Command Line Parsing Correctness
 * **Validates: Requirements 3.1, 3.4, 3.5**
 */

#include <gtest/gtest.h>
#include <random>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell_parser.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Shell Parser Property Test Fixture
 */
class ShellParserPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    parsed_command_t result;
    char line_buffer[512];

    void SetUp() override {
        rng.seed(std::random_device{}());
        memset(&result, 0, sizeof(result));
        memset(line_buffer, 0, sizeof(line_buffer));
    }

    /**
     * \brief           Generate a random alphanumeric string
     */
    std::string randomAlphanumeric(int minLen, int maxLen) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 61);

        int len = lenDist(rng);
        std::string str;
        str.reserve(len);

        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else if (c < 52) {
                str += static_cast<char>('A' + (c - 26));
            } else {
                str += static_cast<char>('0' + (c - 52));
            }
        }
        return str;
    }

    /**
     * \brief           Generate a random string that may contain spaces (for
     * quoted args)
     */
    std::string randomStringWithSpaces(int minLen, int maxLen) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 63);

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
                str += ' ';  // Include spaces
            }
        }
        return str;
    }

    /**
     * \brief           Build a command line from command name and arguments
     */
    std::string buildCommandLine(const std::string& cmdName,
                                 const std::vector<std::string>& args,
                                 const std::vector<bool>& quoted) {
        std::ostringstream oss;
        oss << cmdName;

        for (size_t i = 0; i < args.size(); ++i) {
            oss << " ";
            if (quoted[i]) {
                oss << "\"" << args[i] << "\"";
            } else {
                oss << args[i];
            }
        }
        return oss.str();
    }

    void copyLine(const std::string& line) {
        size_t len = line.length();
        if (len >= sizeof(line_buffer)) {
            len = sizeof(line_buffer) - 1;
        }
        memcpy(line_buffer, line.c_str(), len);
        line_buffer[len] = '\0';
    }
};

/*---------------------------------------------------------------------------*/
/* Property 3: Command Line Parsing Correctness                              */
/* *For any* command line with space-separated arguments (including quoted   */
/* strings), parsing SHALL produce the correct command name and argument     */
/* array, preserving argument order and content.                             */
/* **Validates: Requirements 3.1, 3.4, 3.5**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 3: Command Line Parsing Correctness
 *
 * *For any* command line with space-separated arguments (including quoted
 * strings), parsing SHALL produce the correct command name and argument
 * array, preserving argument order and content.
 *
 * **Validates: Requirements 3.1, 3.4, 3.5**
 */
TEST_F(ShellParserPropertyTest, Property3_CommandLineParsingCorrectness) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Generate random command name (1-10 alphanumeric chars) */
        std::string cmdName = randomAlphanumeric(1, 10);

        /* Generate random number of arguments (0 to SHELL_MAX_ARGS-1) */
        std::uniform_int_distribution<int> argCountDist(0, SHELL_MAX_ARGS - 1);
        int argCount = argCountDist(rng);

        /* Generate arguments and decide if each should be quoted */
        std::vector<std::string> args;
        std::vector<bool> quoted;
        std::uniform_int_distribution<int> quoteDist(0, 1);

        for (int i = 0; i < argCount; ++i) {
            bool useQuote = quoteDist(rng) == 1;
            quoted.push_back(useQuote);

            if (useQuote) {
                /* Quoted args can contain spaces */
                args.push_back(randomStringWithSpaces(1, 15));
            } else {
                /* Unquoted args must not contain spaces */
                args.push_back(randomAlphanumeric(1, 10));
            }
        }

        /* Build command line */
        std::string cmdLine = buildCommandLine(cmdName, args, quoted);
        copyLine(cmdLine);

        /* Parse the command line */
        shell_status_t status = parse_command_line(line_buffer, &result);

        /* Verify parsing succeeded */
        ASSERT_EQ(SHELL_OK, status)
            << "Iteration " << iter << ": parse failed for: " << cmdLine;

        /* Verify command name */
        ASSERT_NE(nullptr, result.cmd_name)
            << "Iteration " << iter << ": cmd_name is NULL for: " << cmdLine;
        EXPECT_STREQ(cmdName.c_str(), result.cmd_name)
            << "Iteration " << iter << ": command name mismatch";

        /* Verify argument count (argc includes command name) */
        EXPECT_EQ(argCount + 1, result.argc)
            << "Iteration " << iter << ": argc mismatch. Expected "
            << (argCount + 1) << ", got " << result.argc << " for: " << cmdLine;

        /* Verify argv[0] is command name */
        EXPECT_STREQ(cmdName.c_str(), result.argv[0])
            << "Iteration " << iter << ": argv[0] should be command name";

        /* Verify each argument content and order */
        for (int i = 0; i < argCount; ++i) {
            ASSERT_NE(nullptr, result.argv[i + 1])
                << "Iteration " << iter << ": argv[" << (i + 1) << "] is NULL";
            EXPECT_STREQ(args[i].c_str(), result.argv[i + 1])
                << "Iteration " << iter << ": argument " << i << " mismatch"
                << " for: " << cmdLine;
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property: Empty Input Handling
 *
 * *For any* empty or whitespace-only input, parsing SHALL succeed with
 * argc=0 and cmd_name=NULL.
 *
 * **Validates: Requirements 3.1**
 */
TEST_F(ShellParserPropertyTest, Property_EmptyInputHandling) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Generate random whitespace string (0-20 spaces/tabs) */
        std::uniform_int_distribution<int> lenDist(0, 20);
        std::uniform_int_distribution<int> charDist(0, 1);

        int len = lenDist(rng);
        std::string whitespace;
        for (int i = 0; i < len; ++i) {
            whitespace += (charDist(rng) == 0) ? ' ' : '\t';
        }

        copyLine(whitespace);

        /* Parse the whitespace-only line */
        shell_status_t status = parse_command_line(line_buffer, &result);

        /* Verify parsing succeeded */
        EXPECT_EQ(SHELL_OK, status)
            << "Iteration " << iter
            << ": parse failed for whitespace-only input";

        /* Verify empty result */
        EXPECT_EQ(0, result.argc)
            << "Iteration " << iter << ": argc should be 0 for whitespace-only";
        EXPECT_EQ(nullptr, result.cmd_name)
            << "Iteration " << iter
            << ": cmd_name should be NULL for whitespace-only";
    }
}

/**
 * Feature: shell-cli-middleware, Property: Argument Count Limit
 *
 * *For any* command line with more than SHELL_MAX_ARGS arguments,
 * parsing SHALL return SHELL_ERROR_BUFFER_FULL.
 *
 * **Validates: Requirements 3.6**
 */
TEST_F(ShellParserPropertyTest, Property_ArgumentCountLimit) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Generate command with exactly SHELL_MAX_ARGS + random extra args */
        std::uniform_int_distribution<int> extraDist(1, 5);
        int totalArgs = SHELL_MAX_ARGS + extraDist(rng);

        std::ostringstream oss;
        oss << "cmd"; /* Command name */

        for (int i = 1; i < totalArgs; ++i) {
            oss << " arg" << i;
        }

        std::string cmdLine = oss.str();
        copyLine(cmdLine);

        /* Parse the command line */
        shell_status_t status = parse_command_line(line_buffer, &result);

        /* Verify parsing returns buffer full error */
        EXPECT_EQ(SHELL_ERROR_BUFFER_FULL, status)
            << "Iteration " << iter << ": expected BUFFER_FULL for "
            << totalArgs << " args";
    }
}

/**
 * Feature: shell-cli-middleware, Property: Quoted String Preservation
 *
 * *For any* quoted string argument, the content inside quotes SHALL be
 * preserved exactly, including internal spaces.
 *
 * **Validates: Requirements 3.5**
 */
TEST_F(ShellParserPropertyTest, Property_QuotedStringPreservation) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Generate a string with guaranteed spaces */
        std::string quotedContent = randomAlphanumeric(1, 5) + " " +
                                    randomAlphanumeric(1, 5) + " " +
                                    randomAlphanumeric(1, 5);

        /* Build command line with quoted argument */
        std::string cmdLine = "echo \"" + quotedContent + "\"";
        copyLine(cmdLine);

        /* Parse the command line */
        shell_status_t status = parse_command_line(line_buffer, &result);

        /* Verify parsing succeeded */
        ASSERT_EQ(SHELL_OK, status) << "Iteration " << iter << ": parse failed";

        /* Verify the quoted content is preserved exactly */
        ASSERT_EQ(2, result.argc)
            << "Iteration " << iter << ": expected 2 args (cmd + quoted)";
        EXPECT_STREQ(quotedContent.c_str(), result.argv[1])
            << "Iteration " << iter << ": quoted content not preserved";
    }
}

/**
 * Feature: shell-cli-middleware, Property: Argument Order Preservation
 *
 * *For any* sequence of arguments, the order SHALL be preserved in argv.
 *
 * **Validates: Requirements 3.1, 3.4**
 */
TEST_F(ShellParserPropertyTest, Property_ArgumentOrderPreservation) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Generate 3-7 unique arguments */
        std::uniform_int_distribution<int> countDist(3, 7);
        int argCount = countDist(rng);

        std::vector<std::string> args;
        std::ostringstream oss;
        oss << "cmd";

        for (int i = 0; i < argCount; ++i) {
            /* Use index-based names to ensure uniqueness */
            std::string arg =
                "arg" + std::to_string(i) + "_" + randomAlphanumeric(2, 4);
            args.push_back(arg);
            oss << " " << arg;
        }

        std::string cmdLine = oss.str();
        copyLine(cmdLine);

        /* Parse the command line */
        shell_status_t status = parse_command_line(line_buffer, &result);

        /* Verify parsing succeeded */
        ASSERT_EQ(SHELL_OK, status) << "Iteration " << iter << ": parse failed";

        /* Verify argument order */
        ASSERT_EQ(argCount + 1, result.argc)
            << "Iteration " << iter << ": argc mismatch";

        for (int i = 0; i < argCount; ++i) {
            EXPECT_STREQ(args[i].c_str(), result.argv[i + 1])
                << "Iteration " << iter << ": argument " << i
                << " order violated";
        }
    }
}
