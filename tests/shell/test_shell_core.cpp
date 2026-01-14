/**
 * \file            test_shell_core.cpp
 * \brief           Shell Core Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell core initialization, deinitialization, and processing.
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "shell/shell.h"
#include "shell/shell_command.h"
}

/**
 * \brief           Mock backend for testing
 */
class MockBackend {
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
};

std::vector<uint8_t> MockBackend::input_buffer;
std::vector<uint8_t> MockBackend::output_buffer;
size_t MockBackend::read_pos = 0;

/** Mock backend instance */
static const shell_backend_t mock_backend = {.read = MockBackend::read,
                                             .write = MockBackend::write};

/**
 * \brief           Shell Core Test Fixture
 */
class ShellCoreTest : public ::testing::Test {
  protected:
    void SetUp() override {
        MockBackend::reset();
        shell_clear_commands();
        /* Ensure shell is deinitialized */
        if (shell_is_initialized()) {
            shell_deinit();
        }
    }

    void TearDown() override {
        if (shell_is_initialized()) {
            shell_deinit();
        }
        shell_clear_commands();
        MockBackend::reset();
    }

    shell_config_t get_default_config() {
        shell_config_t config = {.prompt = "test> ",
                                 .cmd_buffer_size = 128,
                                 .history_depth = 8,
                                 .max_commands = 32};
        return config;
    }
};

/**
 * \name            Initialization Tests - Requirements 1.1, 1.2, 1.3
 * \{
 */

TEST_F(ShellCoreTest, InitWithValidConfig) {
    shell_config_t config = get_default_config();

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithNullConfig) {
    /* Requirement 1.2: NULL config returns SHELL_ERROR_INVALID_PARAM */
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(nullptr));
    EXPECT_FALSE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitTwiceReturnsError) {
    /* Requirement 1.3: Double init returns SHELL_ERROR_ALREADY_INIT */
    shell_config_t config = get_default_config();

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_EQ(SHELL_ERROR_ALREADY_INIT, shell_init(&config));
}

TEST_F(ShellCoreTest, InitWithCustomPrompt) {
    /* Requirement 1.4: Configurable prompt up to 16 characters */
    shell_config_t config = get_default_config();
    config.prompt = "custom> ";

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithMaxPromptLength) {
    shell_config_t config = get_default_config();
    config.prompt = "1234567890123456"; /* Exactly 16 chars */

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithTooLongPrompt) {
    shell_config_t config = get_default_config();
    config.prompt = "12345678901234567"; /* 17 chars - too long */

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config));
    EXPECT_FALSE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithNullPromptUsesDefault) {
    shell_config_t config = get_default_config();
    config.prompt = nullptr;

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

/**
 * \}
 */

/**
 * \name            Buffer Size Tests - Requirement 1.5
 * \{
 */

TEST_F(ShellCoreTest, InitWithMinBufferSize) {
    shell_config_t config = get_default_config();
    config.cmd_buffer_size = SHELL_MIN_CMD_BUFFER_SIZE; /* 64 */

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithMaxBufferSize) {
    shell_config_t config = get_default_config();
    config.cmd_buffer_size = SHELL_MAX_CMD_BUFFER_SIZE; /* 256 */

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithTooSmallBufferSize) {
    shell_config_t config = get_default_config();
    config.cmd_buffer_size = SHELL_MIN_CMD_BUFFER_SIZE - 1;

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config));
    EXPECT_FALSE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithTooLargeBufferSize) {
    shell_config_t config = get_default_config();
    config.cmd_buffer_size = SHELL_MAX_CMD_BUFFER_SIZE + 1;

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config));
    EXPECT_FALSE(shell_is_initialized());
}

/**
 * \}
 */

/**
 * \name            History Depth Tests
 * \{
 */

TEST_F(ShellCoreTest, InitWithMinHistoryDepth) {
    shell_config_t config = get_default_config();
    config.history_depth = SHELL_MIN_HISTORY_DEPTH; /* 4 */

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithMaxHistoryDepth) {
    shell_config_t config = get_default_config();
    config.history_depth = SHELL_MAX_HISTORY_DEPTH; /* 32 */

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithTooSmallHistoryDepth) {
    shell_config_t config = get_default_config();
    config.history_depth = SHELL_MIN_HISTORY_DEPTH - 1;

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config));
    EXPECT_FALSE(shell_is_initialized());
}

TEST_F(ShellCoreTest, InitWithTooLargeHistoryDepth) {
    shell_config_t config = get_default_config();
    config.history_depth = SHELL_MAX_HISTORY_DEPTH + 1;

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_init(&config));
    EXPECT_FALSE(shell_is_initialized());
}

/**
 * \}
 */

/**
 * \name            Deinitialization Tests - Requirement 1.6
 * \{
 */

TEST_F(ShellCoreTest, DeinitReleasesResources) {
    shell_config_t config = get_default_config();

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());

    EXPECT_EQ(SHELL_OK, shell_deinit());
    EXPECT_FALSE(shell_is_initialized());
}

TEST_F(ShellCoreTest, DeinitWithoutInitReturnsError) {
    EXPECT_EQ(SHELL_ERROR_NOT_INIT, shell_deinit());
}

TEST_F(ShellCoreTest, ReinitAfterDeinit) {
    shell_config_t config = get_default_config();

    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_EQ(SHELL_OK, shell_deinit());

    /* Should be able to reinitialize */
    EXPECT_EQ(SHELL_OK, shell_init(&config));
    EXPECT_TRUE(shell_is_initialized());
}

/**
 * \}
 */

/**
 * \name            Process Tests - Requirements 9.1, 9.2, 9.3
 * \{
 */

TEST_F(ShellCoreTest, ProcessWithoutInitReturnsError) {
    EXPECT_EQ(SHELL_ERROR_NOT_INIT, shell_process());
}

TEST_F(ShellCoreTest, ProcessWithoutBackendReturnsError) {
    shell_config_t config = get_default_config();
    shell_init(&config);

    /* No backend set */
    EXPECT_EQ(SHELL_ERROR_NO_BACKEND, shell_process());
}

TEST_F(ShellCoreTest, ProcessWithBackendNoInput) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    /* No input available */
    EXPECT_EQ(SHELL_OK, shell_process());
}

TEST_F(ShellCoreTest, ProcessPrintableCharacter) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    MockBackend::set_input("a");
    EXPECT_EQ(SHELL_OK, shell_process());

    /* Character should be echoed */
    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("a"));
}

/**
 * \}
 */

/**
 * \name            Version and Error Tests
 * \{
 */

TEST_F(ShellCoreTest, GetVersionReturnsString) {
    const char* version = shell_get_version();
    ASSERT_NE(nullptr, version);
    EXPECT_GT(strlen(version), 0u);
}

TEST_F(ShellCoreTest, GetLastErrorAfterInit) {
    shell_config_t config = get_default_config();
    shell_init(&config);

    EXPECT_EQ(SHELL_OK, shell_get_last_error());
}

TEST_F(ShellCoreTest, GetLastErrorAfterFailedInit) {
    /* Try to init with invalid config */
    shell_config_t config = get_default_config();
    config.cmd_buffer_size = 0; /* Invalid */

    shell_init(&config);
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_get_last_error());
}

/**
 * \}
 */

/**
 * \name            Command Execution Tests - Requirements 3.2, 3.3, 3.7
 * \{
 */

static int g_test_cmd_called = 0;
static int g_test_cmd_argc = 0;

static int test_cmd_handler(int argc, char* argv[]) {
    (void)argv;
    g_test_cmd_called++;
    g_test_cmd_argc = argc;
    return 0;
}

static int test_cmd_error_handler(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 42; /* Return error code */
}

TEST_F(ShellCoreTest, ExecuteRegisteredCommand) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_command_t cmd = {.name = "testcmd",
                           .handler = test_cmd_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};
    shell_register_command(&cmd);

    g_test_cmd_called = 0;
    g_test_cmd_argc = 0;

    /* Send command followed by Enter */
    MockBackend::set_input("testcmd\r");

    /* Process all input */
    for (int i = 0; i < 10; i++) {
        shell_process();
    }

    EXPECT_EQ(1, g_test_cmd_called);
    EXPECT_EQ(1, g_test_cmd_argc);
}

TEST_F(ShellCoreTest, ExecuteUnknownCommand) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    /* Send unknown command */
    MockBackend::set_input("unknowncmd\r");

    for (int i = 0; i < 15; i++) {
        shell_process();
    }

    /* Should print "Unknown command" message */
    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Unknown command"));
}

TEST_F(ShellCoreTest, ExecuteCommandWithArgs) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_command_t cmd = {.name = "testcmd",
                           .handler = test_cmd_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};
    shell_register_command(&cmd);

    g_test_cmd_called = 0;
    g_test_cmd_argc = 0;

    /* Send command with arguments */
    MockBackend::set_input("testcmd arg1 arg2\r");

    for (int i = 0; i < 20; i++) {
        shell_process();
    }

    EXPECT_EQ(1, g_test_cmd_called);
    EXPECT_EQ(3, g_test_cmd_argc); /* cmd + 2 args */
}

TEST_F(ShellCoreTest, ExecuteCommandReturningError) {
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_command_t cmd = {.name = "errorcmd",
                           .handler = test_cmd_error_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};
    shell_register_command(&cmd);

    MockBackend::set_input("errorcmd\r");

    for (int i = 0; i < 15; i++) {
        shell_process();
    }

    /* Should print error message with return code */
    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
    EXPECT_NE(std::string::npos, output.find("42"));
}

/**
 * \}
 */

/**
 * \name            Error Handling Tests - Requirements 10.1-10.5
 * \{
 */

TEST_F(ShellCoreTest, GetErrorMessageForAllStatusCodes) {
    /* Requirement 10.1: Clear error codes for all failure conditions */
    const char* msg;

    msg = shell_get_error_message(SHELL_OK);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_INVALID_PARAM);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_NOT_INIT);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_ALREADY_INIT);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_NO_MEMORY);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_NOT_FOUND);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_ALREADY_EXISTS);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_NO_BACKEND);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);

    msg = shell_get_error_message(SHELL_ERROR_BUFFER_FULL);
    ASSERT_NE(nullptr, msg);
    EXPECT_STRNE("Unknown error", msg);
}

TEST_F(ShellCoreTest, GetErrorMessageForUnknownCode) {
    /* Unknown error codes should return "Unknown error" */
    const char* msg = shell_get_error_message((shell_status_t)999);
    ASSERT_NE(nullptr, msg);
    EXPECT_STREQ("Unknown error", msg);
}

TEST_F(ShellCoreTest, PrintErrorOutputsMessage) {
    /* Requirement 10.2: Print descriptive error message */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_print_error(SHELL_ERROR_INVALID_PARAM);

    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
    EXPECT_NE(std::string::npos, output.find("Invalid parameter"));
}

TEST_F(ShellCoreTest, PrintErrorContextOutputsMessageWithContext) {
    /* Requirement 10.2: Print descriptive error message with context */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_print_error_context(SHELL_ERROR_NOT_FOUND, "command 'foo'");

    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
    EXPECT_NE(std::string::npos, output.find("not found"));
    EXPECT_NE(std::string::npos, output.find("command 'foo'"));
}

TEST_F(ShellCoreTest, PrintErrorContextWithNullContext) {
    /* Should work like shell_print_error when context is NULL */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_print_error_context(SHELL_ERROR_NO_BACKEND, nullptr);

    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
    EXPECT_NE(std::string::npos, output.find("backend"));
}

TEST_F(ShellCoreTest, PrintErrorContextWithEmptyContext) {
    /* Should work like shell_print_error when context is empty */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_print_error_context(SHELL_ERROR_NO_BACKEND, "");

    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("Error"));
}

TEST_F(ShellCoreTest, GetLastErrorTracksErrors) {
    /* Requirement 10.3: shell_get_last_error retrieves last error */
    shell_config_t config = get_default_config();

    /* Initially no error */
    shell_init(&config);
    EXPECT_EQ(SHELL_OK, shell_get_last_error());

    /* Process without backend should set error */
    shell_process();
    EXPECT_EQ(SHELL_ERROR_NO_BACKEND, shell_get_last_error());
}

TEST_F(ShellCoreTest, RecoverResetsState) {
    /* Requirement 10.5: Recover from error and show new prompt */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    /* Type some input */
    MockBackend::set_input("partial");
    for (int i = 0; i < 10; i++) {
        shell_process();
    }

    MockBackend::output_buffer.clear();

    /* Recover should reset state and show prompt */
    EXPECT_EQ(SHELL_OK, shell_recover());

    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("test>"));
}

TEST_F(ShellCoreTest, RecoverWithoutInitReturnsError) {
    /* Cannot recover if not initialized */
    EXPECT_EQ(SHELL_ERROR_NOT_INIT, shell_recover());
}

TEST_F(ShellCoreTest, RecoverClearsLastError) {
    /* Recovery should clear the last error */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    /* Cause an error by processing without backend first */
    shell_set_backend(nullptr);
    shell_process();
    EXPECT_EQ(SHELL_ERROR_NO_BACKEND, shell_get_last_error());

    /* Set backend and recover */
    shell_set_backend(&mock_backend);
    shell_recover();
    EXPECT_EQ(SHELL_OK, shell_get_last_error());
}

TEST_F(ShellCoreTest, ErrorRecoveryAfterCtrlC) {
    /* Ctrl+C should recover from partial input */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    /* Type partial input then Ctrl+C */
    MockBackend::set_input("partial\x03"); /* \x03 is Ctrl+C */
    for (int i = 0; i < 15; i++) {
        shell_process();
    }

    /* Should show ^C and new prompt */
    std::string output = MockBackend::get_output();
    EXPECT_NE(std::string::npos, output.find("^C"));
    EXPECT_NE(std::string::npos, output.find("test>"));
}

TEST_F(ShellCoreTest, ErrorMessageContainsCode) {
    /* Error messages should include the error code */
    shell_config_t config = get_default_config();
    shell_init(&config);
    shell_set_backend(&mock_backend);

    shell_print_error(SHELL_ERROR_INVALID_PARAM);

    std::string output = MockBackend::get_output();
    /* Should contain "code 2" for SHELL_ERROR_INVALID_PARAM */
    EXPECT_NE(std::string::npos, output.find("2"));
}

/**
 * \}
 */
