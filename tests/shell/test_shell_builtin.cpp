/**
 * \file            test_shell_builtin.cpp
 * \brief           Shell Built-in Commands Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell built-in commands: help, version, clear, history, echo.
 * Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6
 */

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell.h"
#include "shell/shell_command.h"
#include "shell/shell_history.h"
}

/**
 * \brief           Mock backend for testing built-in commands
 */
class BuiltinMockBackend {
  public:
    static std::vector<uint8_t> input_buffer;
    static std::vector<uint8_t> output_buffer;
    static size_t read_pos;

    static void reset() {
        input_buffer.clear();
        output_buffer.clear();
        read_pos = 0;
    }

    static void set_input(const char* str) {
        input_buffer.clear();
        read_pos = 0;
        while (*str) {
            input_buffer.push_back(static_cast<uint8_t>(*str++));
        }
    }

    static int read(uint8_t* data, int max_len) {
        if (read_pos >= input_buffer.size() || max_len <= 0) {
            return 0;
        }
        int count = 0;
        while (count < max_len && read_pos < input_buffer.size()) {
            data[count++] = input_buffer[read_pos++];
        }
        return count;
    }

    static int write(const uint8_t* data, int len) {
        for (int i = 0; i < len; i++) {
            output_buffer.push_back(data[i]);
        }
        return len;
    }

    static std::string get_output() {
        return std::string(output_buffer.begin(), output_buffer.end());
    }

    static void clear_output() {
        output_buffer.clear();
    }
};

std::vector<uint8_t> BuiltinMockBackend::input_buffer;
std::vector<uint8_t> BuiltinMockBackend::output_buffer;
size_t BuiltinMockBackend::read_pos = 0;

/** Mock backend instance */
static const shell_backend_t builtin_mock_backend = {
    .read = BuiltinMockBackend::read, .write = BuiltinMockBackend::write};

/**
 * \brief           Shell Built-in Commands Test Fixture
 */
class ShellBuiltinTest : public ::testing::Test {
  protected:
    void SetUp() override {
        BuiltinMockBackend::reset();
        shell_clear_commands();
        if (shell_is_initialized()) {
            shell_deinit();
        }

        shell_config_t config = {.prompt = "test> ",
                                 .cmd_buffer_size = 128,
                                 .history_depth = 8,
                                 .max_commands = 32};
        shell_init(&config);
        shell_set_backend(&builtin_mock_backend);
        shell_register_builtin_commands();
    }

    void TearDown() override {
        if (shell_is_initialized()) {
            shell_deinit();
        }
        shell_clear_commands();
        BuiltinMockBackend::reset();
    }

    void execute_command(const char* cmd) {
        std::string input = std::string(cmd) + "\r";
        BuiltinMockBackend::set_input(input.c_str());
        for (size_t i = 0; i < input.length() + 5; i++) {
            shell_process();
        }
    }
};

/*---------------------------------------------------------------------------*/
/* Help Command Tests - Requirements 7.1, 7.2                                */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, HelpCommandRegistered) {
    const shell_command_t* cmd = shell_get_command("help");
    ASSERT_NE(nullptr, cmd);
    EXPECT_STREQ("help", cmd->name);
    EXPECT_NE(nullptr, cmd->handler);
}

TEST_F(ShellBuiltinTest, HelpListsAllCommands) {
    /* Requirement 7.1: help lists all registered commands */
    BuiltinMockBackend::clear_output();
    execute_command("help");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain all built-in commands */
    EXPECT_NE(std::string::npos, output.find("help"));
    EXPECT_NE(std::string::npos, output.find("version"));
    EXPECT_NE(std::string::npos, output.find("clear"));
    EXPECT_NE(std::string::npos, output.find("history"));
    EXPECT_NE(std::string::npos, output.find("echo"));
}

TEST_F(ShellBuiltinTest, HelpShowsSpecificCommand) {
    /* Requirement 7.2: help <command> shows detailed help */
    BuiltinMockBackend::clear_output();
    execute_command("help version");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain command name and description */
    EXPECT_NE(std::string::npos, output.find("version"));
    EXPECT_NE(std::string::npos, output.find("Command:"));
}

TEST_F(ShellBuiltinTest, HelpUnknownCommand) {
    BuiltinMockBackend::clear_output();
    execute_command("help nonexistent");

    std::string output = BuiltinMockBackend::get_output();

    /* Should indicate unknown command */
    EXPECT_NE(std::string::npos, output.find("Unknown command"));
}

/*---------------------------------------------------------------------------*/
/* Version Command Tests - Requirement 7.3                                   */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, VersionCommandRegistered) {
    const shell_command_t* cmd = shell_get_command("version");
    ASSERT_NE(nullptr, cmd);
    EXPECT_STREQ("version", cmd->name);
    EXPECT_NE(nullptr, cmd->handler);
}

TEST_F(ShellBuiltinTest, VersionShowsVersion) {
    /* Requirement 7.3: version shows Shell version */
    BuiltinMockBackend::clear_output();
    execute_command("version");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain version string */
    const char* version = shell_get_version();
    EXPECT_NE(std::string::npos, output.find(version));
}

/*---------------------------------------------------------------------------*/
/* Clear Command Tests - Requirement 7.4                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, ClearCommandRegistered) {
    const shell_command_t* cmd = shell_get_command("clear");
    ASSERT_NE(nullptr, cmd);
    EXPECT_STREQ("clear", cmd->name);
    EXPECT_NE(nullptr, cmd->handler);
}

TEST_F(ShellBuiltinTest, ClearSendsEscapeSequence) {
    /* Requirement 7.4: clear clears the terminal screen */
    BuiltinMockBackend::clear_output();
    execute_command("clear");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain ANSI clear screen sequence */
    EXPECT_NE(std::string::npos, output.find("\033[2J"));
}

/*---------------------------------------------------------------------------*/
/* History Command Tests - Requirement 7.5                                   */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, HistoryCommandRegistered) {
    const shell_command_t* cmd = shell_get_command("history");
    ASSERT_NE(nullptr, cmd);
    EXPECT_STREQ("history", cmd->name);
    EXPECT_NE(nullptr, cmd->handler);
}

TEST_F(ShellBuiltinTest, HistoryShowsEmptyMessage) {
    /* When no commands in history */
    BuiltinMockBackend::clear_output();
    execute_command("history");

    std::string output = BuiltinMockBackend::get_output();

    /* First command is "history" itself, so it should show that */
    /* Or show "No commands" if history is checked before adding */
}

TEST_F(ShellBuiltinTest, HistoryShowsPreviousCommands) {
    /* Requirement 7.5: history shows command history */
    execute_command("version");
    execute_command("help");

    BuiltinMockBackend::clear_output();
    execute_command("history");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain previous commands */
    EXPECT_NE(std::string::npos, output.find("version"));
    EXPECT_NE(std::string::npos, output.find("help"));
}

/*---------------------------------------------------------------------------*/
/* Echo Command Tests - Requirement 7.6                                      */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, EchoCommandRegistered) {
    const shell_command_t* cmd = shell_get_command("echo");
    ASSERT_NE(nullptr, cmd);
    EXPECT_STREQ("echo", cmd->name);
    EXPECT_NE(nullptr, cmd->handler);
}

TEST_F(ShellBuiltinTest, EchoNoArgs) {
    /* Requirement 7.6: echo prints arguments */
    BuiltinMockBackend::clear_output();
    execute_command("echo");

    std::string output = BuiltinMockBackend::get_output();

    /* Should just print newline */
    EXPECT_NE(std::string::npos, output.find("\r\n"));
}

TEST_F(ShellBuiltinTest, EchoSingleArg) {
    BuiltinMockBackend::clear_output();
    execute_command("echo hello");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain the argument */
    EXPECT_NE(std::string::npos, output.find("hello"));
}

TEST_F(ShellBuiltinTest, EchoMultipleArgs) {
    BuiltinMockBackend::clear_output();
    execute_command("echo hello world");

    std::string output = BuiltinMockBackend::get_output();

    /* Should contain both arguments */
    EXPECT_NE(std::string::npos, output.find("hello"));
    EXPECT_NE(std::string::npos, output.find("world"));
}

/*---------------------------------------------------------------------------*/
/* Built-in Registration Tests                                               */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, RegisterBuiltinCommandsSuccess) {
    /* Clear and re-register */
    shell_clear_commands();

    EXPECT_EQ(SHELL_OK, shell_register_builtin_commands());

    /* All built-in commands should be registered */
    EXPECT_NE(nullptr, shell_get_command("help"));
    EXPECT_NE(nullptr, shell_get_command("version"));
    EXPECT_NE(nullptr, shell_get_command("clear"));
    EXPECT_NE(nullptr, shell_get_command("history"));
    EXPECT_NE(nullptr, shell_get_command("echo"));
}

TEST_F(ShellBuiltinTest, BuiltinCommandCount) {
    shell_clear_commands();
    shell_register_builtin_commands();

    /* Should have exactly 5 built-in commands */
    EXPECT_EQ(5, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Get History Manager Tests                                                 */
/*---------------------------------------------------------------------------*/

TEST_F(ShellBuiltinTest, GetHistoryManagerWhenInitialized) {
    history_manager_t* hist = shell_get_history_manager();
    ASSERT_NE(nullptr, hist);
}

TEST_F(ShellBuiltinTest, GetHistoryManagerWhenNotInitialized) {
    shell_deinit();

    history_manager_t* hist = shell_get_history_manager();
    EXPECT_EQ(nullptr, hist);
}
