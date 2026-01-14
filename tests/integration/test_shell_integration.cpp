/**
 * \file            test_shell_integration.cpp
 * \brief           Shell/CLI Middleware Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for Shell/CLI middleware.
 *                  Tests complete command flow, line editing with history,
 *                  and auto-completion functionality.
 *                  Requirements: 1.1-10.5
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell.h"
#include "shell/shell_command.h"
#include "shell/shell_backend.h"
#include "shell/shell_history.h"
#include "shell/shell_autocomplete.h"
#include "shell/shell_parser.h"
#include "shell/shell_line_editor.h"
}

/**
 * \brief           Shell Integration Test Fixture
 */
class ShellIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        shell_mock_backend_init();
        shell_mock_backend_reset();
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
        shell_mock_backend_deinit();
    }

    shell_config_t get_default_config() {
        shell_config_t config = {
            .prompt = "test> ",
            .cmd_buffer_size = 128,
            .history_depth = 8,
            .max_commands = 32
        };
        return config;
    }

    void init_shell_with_backend() {
        shell_config_t config = get_default_config();
        shell_init(&config);
        shell_set_backend(&shell_mock_backend);
        shell_register_builtin_commands();
    }

    void process_input(const char* input) {
        shell_mock_backend_inject_string(input);
        /* Process enough times to handle all input */
        for (size_t i = 0; i < strlen(input) + 5; i++) {
            shell_process();
        }
    }

    std::string get_output() {
        char buf[4096];
        int len = shell_mock_backend_get_output_string(buf, sizeof(buf));
        return std::string(buf, len);
    }

    void clear_output() {
        shell_mock_backend_clear_output();
    }
};

/*---------------------------------------------------------------------------*/
/* Test Command Handlers                                                     */
/*---------------------------------------------------------------------------*/

static int g_cmd_called = 0;
static int g_cmd_argc = 0;
static std::vector<std::string> g_cmd_argv;

static int test_cmd_handler(int argc, char* argv[]) {
    g_cmd_called++;
    g_cmd_argc = argc;
    g_cmd_argv.clear();
    for (int i = 0; i < argc; i++) {
        g_cmd_argv.push_back(argv[i]);
    }
    return 0;
}

static int test_cmd_error_handler(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 42;
}

static void reset_cmd_state() {
    g_cmd_called = 0;
    g_cmd_argc = 0;
    g_cmd_argv.clear();
}

/*---------------------------------------------------------------------------*/
/* Complete Command Flow Tests - Requirements 1.1, 2.1, 3.1-3.7              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test complete command registration and execution flow
 * \details         Requirements 1.1, 2.1, 3.1, 3.2
 */
TEST_F(ShellIntegrationTest, CompleteCommandFlow) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "mycmd",
        .handler = test_cmd_handler,
        .help = "My test command",
        .usage = "mycmd [args]",
        .completion = nullptr
    };
    ASSERT_EQ(SHELL_OK, shell_register_command(&cmd));
    
    reset_cmd_state();
    clear_output();
    
    /* Execute command with arguments */
    process_input("mycmd arg1 arg2\r");
    
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ(3, g_cmd_argc);
    ASSERT_GE(g_cmd_argv.size(), 3u);
    EXPECT_EQ("mycmd", g_cmd_argv[0]);
    EXPECT_EQ("arg1", g_cmd_argv[1]);
    EXPECT_EQ("arg2", g_cmd_argv[2]);
}

/**
 * \brief           Test command with quoted string arguments
 * \details         Requirements 3.4, 3.5
 */
TEST_F(ShellIntegrationTest, CommandWithQuotedArgs) {
    init_shell_with_backend();
    
    /* Use a unique command name to avoid conflict with built-in echo */
    shell_command_t cmd = {
        .name = "quotecmd",
        .handler = test_cmd_handler,
        .help = "Quote test command",
        .usage = "quotecmd [text]",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    reset_cmd_state();
    clear_output();
    
    /* Execute command with quoted string */
    process_input("quotecmd \"hello world\"\r");
    
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ(2, g_cmd_argc);
    ASSERT_GE(g_cmd_argv.size(), 2u);
    EXPECT_EQ("quotecmd", g_cmd_argv[0]);
    EXPECT_EQ("hello world", g_cmd_argv[1]);
}

/**
 * \brief           Test unknown command handling
 * \details         Requirement 3.3
 */
TEST_F(ShellIntegrationTest, UnknownCommandHandling) {
    init_shell_with_backend();
    clear_output();
    
    process_input("nonexistent\r");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("Unknown command"));
    EXPECT_NE(std::string::npos, output.find("nonexistent"));
}

/**
 * \brief           Test command returning error code
 * \details         Requirement 3.7
 */
TEST_F(ShellIntegrationTest, CommandErrorHandling) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "errorcmd",
        .handler = test_cmd_error_handler,
        .help = "Error command",
        .usage = "errorcmd",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    clear_output();
    process_input("errorcmd\r");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
    EXPECT_NE(std::string::npos, output.find("42"));
}

/*---------------------------------------------------------------------------*/
/* Line Editing and History Tests - Requirements 4.1-4.15, 5.1-5.7           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test backspace key handling
 * \details         Requirement 4.2
 */
TEST_F(ShellIntegrationTest, BackspaceEditing) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    reset_cmd_state();
    clear_output();
    
    /* Type "testt" then backspace to correct to "test" */
    process_input("testt\x7f\r");  /* \x7f is DEL/backspace */
    
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("test", g_cmd_argv[0]);
}

/**
 * \brief           Test Ctrl+C cancels input
 * \details         Requirement 4.4
 */
TEST_F(ShellIntegrationTest, CtrlCCancelsInput) {
    init_shell_with_backend();
    clear_output();
    
    /* Type partial input then Ctrl+C */
    process_input("partial\x03");  /* \x03 is Ctrl+C */
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("^C"));
    EXPECT_NE(std::string::npos, output.find("test>"));
}

/**
 * \brief           Test history navigation with Up/Down arrows
 * \details         Requirements 5.1, 5.2, 5.3
 */
TEST_F(ShellIntegrationTest, HistoryNavigation) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "cmd",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "cmd",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Execute two commands to add to history */
    process_input("cmd first\r");
    process_input("cmd second\r");
    
    reset_cmd_state();
    clear_output();
    
    /* Press Up arrow to get previous command */
    /* ESC [ A is Up arrow sequence */
    process_input("\x1b[A\r");
    
    EXPECT_EQ(1, g_cmd_called);
    /* Should execute "cmd second" (most recent) */
    ASSERT_GE(g_cmd_argv.size(), 2u);
    EXPECT_EQ("second", g_cmd_argv[1]);
}

/**
 * \brief           Test history does not add duplicates
 * \details         Requirement 5.6
 */
TEST_F(ShellIntegrationTest, HistoryNoDuplicates) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "repeat",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "repeat",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Execute same command twice */
    process_input("repeat\r");
    process_input("repeat\r");
    
    history_manager_t* hist = shell_get_history_manager();
    ASSERT_NE(nullptr, hist);
    
    /* Should only have one entry */
    EXPECT_EQ(1, history_get_count(hist));
}

/**
 * \brief           Test history does not add empty commands
 * \details         Requirement 5.7
 */
TEST_F(ShellIntegrationTest, HistoryNoEmptyCommands) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Execute a command first */
    process_input("test\r");
    
    history_manager_t* hist = shell_get_history_manager();
    uint8_t count_before = history_get_count(hist);
    
    /* Press Enter with empty input */
    process_input("\r");
    
    /* History count should not change */
    EXPECT_EQ(count_before, history_get_count(hist));
}

/*---------------------------------------------------------------------------*/
/* Auto-Completion Tests - Requirements 6.1-6.7                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test Tab completion with unique match
 * \details         Requirements 6.1, 6.4
 */
TEST_F(ShellIntegrationTest, TabCompletionUniqueMatch) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "uniquecmd",
        .handler = test_cmd_handler,
        .help = "Unique command",
        .usage = "uniquecmd",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    reset_cmd_state();
    clear_output();
    
    /* Type partial command and press Tab */
    process_input("uniq\t\r");
    
    /* Should complete to "uniquecmd" and execute */
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("uniquecmd", g_cmd_argv[0]);
}

/**
 * \brief           Test Tab completion with multiple matches
 * \details         Requirements 6.2, 6.5
 */
TEST_F(ShellIntegrationTest, TabCompletionMultipleMatches) {
    init_shell_with_backend();
    
    shell_command_t cmd1 = {
        .name = "zcmd1",
        .handler = test_cmd_handler,
        .help = "Test 1",
        .usage = "zcmd1",
        .completion = nullptr
    };
    shell_command_t cmd2 = {
        .name = "zcmd2",
        .handler = test_cmd_handler,
        .help = "Test 2",
        .usage = "zcmd2",
        .completion = nullptr
    };
    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    
    /* Test auto-completion API directly */
    completion_result_t result;
    EXPECT_EQ(SHELL_OK, autocomplete_command("zcmd", &result));
    
    /* Should find both matches */
    EXPECT_EQ(2, result.match_count);
    
    /* Verify common prefix length */
    EXPECT_EQ(4, result.common_prefix_len);  /* "zcmd" */
}

/**
 * \brief           Test Tab completion with no matches
 * \details         Requirement 6.3
 */
TEST_F(ShellIntegrationTest, TabCompletionNoMatch) {
    init_shell_with_backend();
    
    clear_output();
    
    /* Type non-matching prefix and press Tab */
    size_t len_before = shell_mock_backend_get_output_length();
    process_input("xyz\t");
    size_t len_after = shell_mock_backend_get_output_length();
    
    /* Should not add significant output (just echo) */
    EXPECT_LT(len_after - len_before, 20u);
}

/*---------------------------------------------------------------------------*/
/* Built-in Commands Tests - Requirements 7.1-7.6                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test help command lists all commands
 * \details         Requirement 7.1
 */
TEST_F(ShellIntegrationTest, HelpCommandListsAll) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "mycmd",
        .handler = test_cmd_handler,
        .help = "My custom command",
        .usage = "mycmd",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    clear_output();
    process_input("help\r");
    
    std::string output = get_output();
    /* Should list built-in commands and custom command */
    EXPECT_NE(std::string::npos, output.find("help"));
    EXPECT_NE(std::string::npos, output.find("mycmd"));
}

/**
 * \brief           Test help command for specific command
 * \details         Requirement 7.2
 */
TEST_F(ShellIntegrationTest, HelpCommandSpecific) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "mycmd",
        .handler = test_cmd_handler,
        .help = "My custom command help",
        .usage = "mycmd [options]",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    clear_output();
    process_input("help mycmd\r");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("mycmd"));
    EXPECT_NE(std::string::npos, output.find("My custom command help"));
}

/**
 * \brief           Test version command
 * \details         Requirement 7.3
 */
TEST_F(ShellIntegrationTest, VersionCommand) {
    init_shell_with_backend();
    
    clear_output();
    process_input("version\r");
    
    std::string output = get_output();
    /* Should contain version number */
    EXPECT_NE(std::string::npos, output.find("1.0"));
}

/**
 * \brief           Test history command
 * \details         Requirement 7.5
 */
TEST_F(ShellIntegrationTest, HistoryCommand) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "mycmd",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "mycmd",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Execute some commands */
    process_input("mycmd first\r");
    process_input("mycmd second\r");
    
    clear_output();
    process_input("history\r");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("mycmd first"));
    EXPECT_NE(std::string::npos, output.find("mycmd second"));
}

/**
 * \brief           Test echo command
 * \details         Requirement 7.6
 */
TEST_F(ShellIntegrationTest, EchoCommand) {
    init_shell_with_backend();
    
    clear_output();
    process_input("echo hello world\r");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("hello world"));
}

/*---------------------------------------------------------------------------*/
/* Backend Integration Tests - Requirements 8.1-8.6                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test backend read/write operations
 * \details         Requirements 8.1, 8.4, 8.5
 */
TEST_F(ShellIntegrationTest, BackendReadWrite) {
    init_shell_with_backend();
    
    /* Test write operation */
    const char* test_str = "Test output";
    int written = shell_puts(test_str);
    EXPECT_EQ((int)strlen(test_str), written);
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find(test_str));
}

/**
 * \brief           Test shell_printf function
 * \details         Requirement 8.2
 */
TEST_F(ShellIntegrationTest, ShellPrintf) {
    init_shell_with_backend();
    clear_output();
    
    shell_printf("Value: %d, String: %s\n", 42, "test");
    
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("Value: 42"));
    EXPECT_NE(std::string::npos, output.find("String: test"));
}

/**
 * \brief           Test process without backend returns error
 * \details         Requirement 8.6
 */
TEST_F(ShellIntegrationTest, ProcessWithoutBackend) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    /* Don't set backend */
    
    EXPECT_EQ(SHELL_ERROR_NO_BACKEND, shell_process());
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 10.1-10.5                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test error recovery after Ctrl+C
 * \details         Requirement 10.5
 */
TEST_F(ShellIntegrationTest, ErrorRecoveryCtrlC) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Type partial input, cancel with Ctrl+C, then execute command */
    process_input("partial\x03");
    
    reset_cmd_state();
    clear_output();
    
    process_input("test\r");
    
    /* Should execute successfully after recovery */
    EXPECT_EQ(1, g_cmd_called);
}

/**
 * \brief           Test shell_recover function
 * \details         Requirement 10.5
 */
TEST_F(ShellIntegrationTest, ShellRecoverFunction) {
    init_shell_with_backend();
    
    /* Type partial input */
    process_input("partial");
    
    /* Call recover */
    EXPECT_EQ(SHELL_OK, shell_recover());
    
    clear_output();
    
    /* Should show new prompt */
    shell_print_prompt();
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("test>"));
}

/**
 * \brief           Test get_last_error tracking
 * \details         Requirement 10.3
 */
TEST_F(ShellIntegrationTest, GetLastErrorTracking) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    
    /* Process without backend should set error */
    shell_process();
    EXPECT_EQ(SHELL_ERROR_NO_BACKEND, shell_get_last_error());
    
    /* Set backend - error should still be NO_BACKEND until next operation */
    shell_set_backend(&shell_mock_backend);
    
    /* After recover, error should be cleared */
    shell_recover();
    EXPECT_EQ(SHELL_OK, shell_get_last_error());
}

/*---------------------------------------------------------------------------*/
/* Complex Integration Scenarios                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test complete interactive session
 * \details         Tests full workflow: init, register commands, execute,
 *                  use history, auto-complete, and cleanup
 */
TEST_F(ShellIntegrationTest, CompleteInteractiveSession) {
    init_shell_with_backend();
    
    /* Register custom commands */
    shell_command_t cmd1 = {
        .name = "greet",
        .handler = test_cmd_handler,
        .help = "Greet someone",
        .usage = "greet <name>",
        .completion = nullptr
    };
    shell_command_t cmd2 = {
        .name = "goodbye",
        .handler = test_cmd_handler,
        .help = "Say goodbye",
        .usage = "goodbye",
        .completion = nullptr
    };
    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    
    /* Execute first command */
    reset_cmd_state();
    process_input("greet Alice\r");
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("Alice", g_cmd_argv[1]);
    
    /* Execute second command */
    reset_cmd_state();
    process_input("goodbye\r");
    EXPECT_EQ(1, g_cmd_called);
    
    /* Use history to repeat first command */
    reset_cmd_state();
    process_input("\x1b[A\x1b[A\r");  /* Up, Up, Enter */
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("greet", g_cmd_argv[0]);
    
    /* Use Tab completion */
    reset_cmd_state();
    process_input("goo\t\r");  /* Should complete to "goodbye" */
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("goodbye", g_cmd_argv[0]);
    
    /* Get help */
    clear_output();
    process_input("help greet\r");
    std::string output = get_output();
    EXPECT_NE(std::string::npos, output.find("Greet someone"));
}

/**
 * \brief           Test multiple commands in sequence
 */
TEST_F(ShellIntegrationTest, MultipleCommandsSequence) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "count",
        .handler = test_cmd_handler,
        .help = "Count",
        .usage = "count",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    /* Execute multiple commands */
    for (int i = 0; i < 5; i++) {
        reset_cmd_state();
        char input[32];
        snprintf(input, sizeof(input), "count %d\r", i);
        process_input(input);
        
        EXPECT_EQ(1, g_cmd_called);
        EXPECT_EQ(std::to_string(i), g_cmd_argv[1]);
    }
}

/**
 * \brief           Test line editing with cursor movement
 * \details         Requirements 4.8, 4.9, 4.10, 4.11
 */
TEST_F(ShellIntegrationTest, LineEditingCursorMovement) {
    init_shell_with_backend();
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test",
        .completion = nullptr
    };
    shell_register_command(&cmd);
    
    reset_cmd_state();
    
    /* Type "tst", move left, insert 'e' to make "test" */
    /* Left arrow is ESC [ D */
    process_input("tst\x1b[D\x1b[De\r");
    
    EXPECT_EQ(1, g_cmd_called);
    EXPECT_EQ("test", g_cmd_argv[0]);
}
