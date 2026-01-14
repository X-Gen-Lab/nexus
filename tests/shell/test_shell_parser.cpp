/**
 * \file            test_shell_parser.cpp
 * \brief           Shell Parser Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell command line parser functionality.
 * Requirements: 3.1, 3.4, 3.5
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "shell/shell_parser.h"
}

/**
 * \brief           Shell Parser Test Fixture
 */
class ShellParserTest : public ::testing::Test {
  protected:
    parsed_command_t result;
    char line_buffer[256];

    void SetUp() override {
        memset(&result, 0, sizeof(result));
        memset(line_buffer, 0, sizeof(line_buffer));
    }

    void copyLine(const char* line) {
        size_t len = strlen(line);
        if (len >= sizeof(line_buffer)) {
            len = sizeof(line_buffer) - 1;
        }
        memcpy(line_buffer, line, len);
        line_buffer[len] = '\0';
    }
};

/*---------------------------------------------------------------------------*/
/* Basic Command Parsing Tests - Requirements 3.1, 3.4                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test parsing simple command with no arguments
 * \details         Requirements 3.1 - Parse command name
 */
TEST_F(ShellParserTest, ParseSimpleCommand) {
    copyLine("help");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("help", result.cmd_name);
    EXPECT_EQ(1, result.argc);
    EXPECT_STREQ("help", result.argv[0]);
}

/**
 * \brief           Test parsing command with single argument
 * \details         Requirements 3.1, 3.4 - Parse command and space-separated
 * argument
 */
TEST_F(ShellParserTest, ParseCommandWithOneArg) {
    copyLine("help version");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("help", result.cmd_name);
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("help", result.argv[0]);
    EXPECT_STREQ("version", result.argv[1]);
}

/**
 * \brief           Test parsing command with multiple arguments
 * \details         Requirements 3.1, 3.4 - Parse command with multiple
 * space-separated arguments
 */
TEST_F(ShellParserTest, ParseCommandWithMultipleArgs) {
    copyLine("gpio set 13 high");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("gpio", result.cmd_name);
    EXPECT_EQ(4, result.argc);
    EXPECT_STREQ("gpio", result.argv[0]);
    EXPECT_STREQ("set", result.argv[1]);
    EXPECT_STREQ("13", result.argv[2]);
    EXPECT_STREQ("high", result.argv[3]);
}

/**
 * \brief           Test parsing command with leading whitespace
 * \details         Requirements 3.1 - Handle leading whitespace
 */
TEST_F(ShellParserTest, ParseWithLeadingWhitespace) {
    copyLine("   help");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("help", result.cmd_name);
    EXPECT_EQ(1, result.argc);
}

/**
 * \brief           Test parsing command with trailing whitespace
 * \details         Requirements 3.1 - Handle trailing whitespace
 */
TEST_F(ShellParserTest, ParseWithTrailingWhitespace) {
    copyLine("help   ");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("help", result.cmd_name);
    EXPECT_EQ(1, result.argc);
}

/**
 * \brief           Test parsing command with multiple spaces between arguments
 * \details         Requirements 3.4 - Handle multiple spaces between arguments
 */
TEST_F(ShellParserTest, ParseWithMultipleSpaces) {
    copyLine("gpio   set    13");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("gpio", result.cmd_name);
    EXPECT_EQ(3, result.argc);
    EXPECT_STREQ("gpio", result.argv[0]);
    EXPECT_STREQ("set", result.argv[1]);
    EXPECT_STREQ("13", result.argv[2]);
}

/*---------------------------------------------------------------------------*/
/* Quoted String Tests - Requirements 3.5                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test parsing double-quoted string argument
 * \details         Requirements 3.5 - Quoted strings as single arguments
 */
TEST_F(ShellParserTest, ParseDoubleQuotedString) {
    copyLine("echo \"hello world\"");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("echo", result.cmd_name);
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("echo", result.argv[0]);
    EXPECT_STREQ("hello world", result.argv[1]);
}

/**
 * \brief           Test parsing single-quoted string argument
 * \details         Requirements 3.5 - Single quotes also work
 */
TEST_F(ShellParserTest, ParseSingleQuotedString) {
    copyLine("echo 'hello world'");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("echo", result.cmd_name);
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("echo", result.argv[0]);
    EXPECT_STREQ("hello world", result.argv[1]);
}

/**
 * \brief           Test parsing mixed quoted and unquoted arguments
 * \details         Requirements 3.4, 3.5 - Mix of quoted and unquoted
 */
TEST_F(ShellParserTest, ParseMixedQuotedUnquoted) {
    copyLine("log info \"System started\" now");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("log", result.cmd_name);
    EXPECT_EQ(4, result.argc);
    EXPECT_STREQ("log", result.argv[0]);
    EXPECT_STREQ("info", result.argv[1]);
    EXPECT_STREQ("System started", result.argv[2]);
    EXPECT_STREQ("now", result.argv[3]);
}

/**
 * \brief           Test parsing empty quoted string
 * \details         Requirements 3.5 - Empty quoted string is valid
 */
TEST_F(ShellParserTest, ParseEmptyQuotedString) {
    copyLine("echo \"\"");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_STREQ("echo", result.cmd_name);
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("", result.argv[1]);
}

/**
 * \brief           Test parsing quoted string with special characters
 * \details         Requirements 3.5 - Quoted strings preserve special chars
 */
TEST_F(ShellParserTest, ParseQuotedWithSpecialChars) {
    copyLine("echo \"hello\tworld\"");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("hello\tworld", result.argv[1]);
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test parsing empty line
 * \details         Boundary: Empty input should return success with argc=0
 */
TEST_F(ShellParserTest, ParseEmptyLine) {
    copyLine("");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(0, result.argc);
    EXPECT_EQ(nullptr, result.cmd_name);
}

/**
 * \brief           Test parsing whitespace-only line
 * \details         Boundary: Whitespace-only should return success with argc=0
 */
TEST_F(ShellParserTest, ParseWhitespaceOnly) {
    copyLine("   \t  ");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(0, result.argc);
    EXPECT_EQ(nullptr, result.cmd_name);
}

/**
 * \brief           Test parsing with NULL line parameter
 * \details         Boundary: NULL line should return error
 */
TEST_F(ShellParserTest, ParseNullLine) {
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, parse_command_line(nullptr, &result));
}

/**
 * \brief           Test parsing with NULL result parameter
 * \details         Boundary: NULL result should return error
 */
TEST_F(ShellParserTest, ParseNullResult) {
    copyLine("help");
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM,
              parse_command_line(line_buffer, nullptr));
}

/**
 * \brief           Test parsing maximum number of arguments
 * \details         Boundary: SHELL_MAX_ARGS arguments should succeed
 */
TEST_F(ShellParserTest, ParseMaxArgs) {
    /* SHELL_MAX_ARGS is 8, so we need 8 tokens */
    copyLine("cmd a1 a2 a3 a4 a5 a6 a7");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(SHELL_MAX_ARGS, result.argc);
    EXPECT_STREQ("cmd", result.argv[0]);
    EXPECT_STREQ("a7", result.argv[7]);
}

/**
 * \brief           Test parsing too many arguments
 * \details         Boundary: More than SHELL_MAX_ARGS should return error
 */
TEST_F(ShellParserTest, ParseTooManyArgs) {
    /* SHELL_MAX_ARGS is 8, so 9 tokens should fail */
    copyLine("cmd a1 a2 a3 a4 a5 a6 a7 a8");

    EXPECT_EQ(SHELL_ERROR_BUFFER_FULL,
              parse_command_line(line_buffer, &result));
}

/**
 * \brief           Test parsing unterminated quote
 * \details         Boundary: Unterminated quote should handle gracefully
 */
TEST_F(ShellParserTest, ParseUnterminatedQuote) {
    copyLine("echo \"hello world");

    /* Should succeed, treating rest of string as argument */
    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(2, result.argc);
    EXPECT_STREQ("hello world", result.argv[1]);
}

/**
 * \brief           Test parsing with tab characters
 * \details         Boundary: Tabs should work as whitespace separators
 */
TEST_F(ShellParserTest, ParseWithTabs) {
    copyLine("gpio\tset\t13");

    EXPECT_EQ(SHELL_OK, parse_command_line(line_buffer, &result));
    EXPECT_EQ(3, result.argc);
    EXPECT_STREQ("gpio", result.argv[0]);
    EXPECT_STREQ("set", result.argv[1]);
    EXPECT_STREQ("13", result.argv[2]);
}
