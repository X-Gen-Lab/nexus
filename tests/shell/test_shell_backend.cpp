/**
 * \file            test_shell_backend.cpp
 * \brief           Shell Backend Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell backend abstraction layer and mock backend.
 * Requirements: 8.1, 8.2
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

extern "C" {
#include "shell/shell_backend.h"
}

/**
 * \brief           Shell Backend Test Fixture
 */
class ShellBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Clear any existing backend */
        shell_set_backend(nullptr);
        /* Initialize mock backend */
        shell_mock_backend_init();
    }

    void TearDown() override {
        shell_mock_backend_deinit();
        shell_set_backend(nullptr);
    }
};

/**
 * \name            Backend Setting Tests - Requirements 8.1, 8.2
 * \{
 */

TEST_F(ShellBackendTest, SetBackendWithValidBackend) {
    /* Requirement 8.2: shell_set_backend with valid backend */
    EXPECT_EQ(SHELL_OK, shell_set_backend(&shell_mock_backend));
    EXPECT_EQ(&shell_mock_backend, shell_get_backend());
}

TEST_F(ShellBackendTest, SetBackendWithNull) {
    /* Set a backend first */
    shell_set_backend(&shell_mock_backend);
    EXPECT_EQ(&shell_mock_backend, shell_get_backend());

    /* Clear backend */
    EXPECT_EQ(SHELL_OK, shell_set_backend(nullptr));
    EXPECT_EQ(nullptr, shell_get_backend());
}

TEST_F(ShellBackendTest, GetBackendWhenNotSet) {
    shell_set_backend(nullptr);
    EXPECT_EQ(nullptr, shell_get_backend());
}

/**
 * \}
 */

/**
 * \name            Shell Printf Tests
 * \{
 */

TEST_F(ShellBackendTest, PrintfWithNoBackend) {
    shell_set_backend(nullptr);
    EXPECT_LT(shell_printf("test"), 0);
}

TEST_F(ShellBackendTest, PrintfWithBackend) {
    shell_set_backend(&shell_mock_backend);

    int result = shell_printf("Hello %s", "World");
    EXPECT_GT(result, 0);

    char output[256];
    shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_STREQ("Hello World", output);
}

TEST_F(ShellBackendTest, PrintfWithNullFormat) {
    shell_set_backend(&shell_mock_backend);
    EXPECT_LT(shell_printf(nullptr), 0);
}

TEST_F(ShellBackendTest, PrintfWithInteger) {
    shell_set_backend(&shell_mock_backend);

    shell_printf("Value: %d", 42);

    char output[256];
    shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_STREQ("Value: 42", output);
}

/**
 * \}
 */

/**
 * \name            Shell Write Tests
 * \{
 */

TEST_F(ShellBackendTest, WriteWithNoBackend) {
    shell_set_backend(nullptr);
    uint8_t data[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(0, shell_write(data, sizeof(data)));
}

TEST_F(ShellBackendTest, WriteWithBackend) {
    shell_set_backend(&shell_mock_backend);

    uint8_t data[] = {0x41, 0x42, 0x43};  /* "ABC" */
    int result = shell_write(data, sizeof(data));
    EXPECT_EQ(3, result);

    char output[256];
    shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_STREQ("ABC", output);
}

TEST_F(ShellBackendTest, WriteWithNullData) {
    shell_set_backend(&shell_mock_backend);
    EXPECT_EQ(0, shell_write(nullptr, 10));
}

TEST_F(ShellBackendTest, WriteWithZeroLength) {
    shell_set_backend(&shell_mock_backend);
    uint8_t data[] = {0x01};
    EXPECT_EQ(0, shell_write(data, 0));
}

TEST_F(ShellBackendTest, WriteWithNegativeLength) {
    shell_set_backend(&shell_mock_backend);
    uint8_t data[] = {0x01};
    EXPECT_EQ(0, shell_write(data, -1));
}

/**
 * \}
 */

/**
 * \name            Shell Putchar Tests
 * \{
 */

TEST_F(ShellBackendTest, PutcharWithNoBackend) {
    shell_set_backend(nullptr);
    EXPECT_EQ(0, shell_putchar('A'));
}

TEST_F(ShellBackendTest, PutcharWithBackend) {
    shell_set_backend(&shell_mock_backend);

    EXPECT_EQ(1, shell_putchar('A'));
    EXPECT_EQ(1, shell_putchar('B'));
    EXPECT_EQ(1, shell_putchar('C'));

    char output[256];
    shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_STREQ("ABC", output);
}

/**
 * \}
 */

/**
 * \name            Shell Puts Tests
 * \{
 */

TEST_F(ShellBackendTest, PutsWithNoBackend) {
    shell_set_backend(nullptr);
    EXPECT_EQ(0, shell_puts("test"));
}

TEST_F(ShellBackendTest, PutsWithBackend) {
    shell_set_backend(&shell_mock_backend);

    int result = shell_puts("Hello World");
    EXPECT_EQ(11, result);

    char output[256];
    shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_STREQ("Hello World", output);
}

TEST_F(ShellBackendTest, PutsWithNullString) {
    shell_set_backend(&shell_mock_backend);
    EXPECT_EQ(0, shell_puts(nullptr));
}

TEST_F(ShellBackendTest, PutsWithEmptyString) {
    shell_set_backend(&shell_mock_backend);
    EXPECT_EQ(0, shell_puts(""));
}

/**
 * \}
 */

/**
 * \brief           Mock Backend Test Fixture
 */
class MockBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        shell_mock_backend_init();
    }

    void TearDown() override {
        shell_mock_backend_deinit();
    }
};

/**
 * \name            Mock Backend Initialization Tests
 * \{
 */

TEST_F(MockBackendTest, InitAndDeinit) {
    /* Already initialized in SetUp */
    EXPECT_TRUE(shell_mock_backend_is_initialized());

    EXPECT_EQ(SHELL_OK, shell_mock_backend_deinit());
    EXPECT_FALSE(shell_mock_backend_is_initialized());

    EXPECT_EQ(SHELL_OK, shell_mock_backend_init());
    EXPECT_TRUE(shell_mock_backend_is_initialized());
}

/**
 * \}
 */

/**
 * \name            Mock Backend Input Injection Tests
 * \{
 */

TEST_F(MockBackendTest, InjectInputData) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int injected = shell_mock_backend_inject_input(data, sizeof(data));
    EXPECT_EQ(4, injected);
    EXPECT_EQ(4u, shell_mock_backend_get_remaining_input());
}

TEST_F(MockBackendTest, InjectInputString) {
    int injected = shell_mock_backend_inject_string("test input");
    EXPECT_EQ(10, injected);
    EXPECT_EQ(10u, shell_mock_backend_get_remaining_input());
}

TEST_F(MockBackendTest, InjectNullInput) {
    EXPECT_EQ(0, shell_mock_backend_inject_input(nullptr, 10));
    EXPECT_EQ(0, shell_mock_backend_inject_string(nullptr));
}

TEST_F(MockBackendTest, InjectEmptyInput) {
    uint8_t data[] = {0x01};
    EXPECT_EQ(0, shell_mock_backend_inject_input(data, 0));
}

/**
 * \}
 */

/**
 * \name            Mock Backend Read Tests
 * \{
 */

TEST_F(MockBackendTest, ReadInjectedData) {
    shell_set_backend(&shell_mock_backend);

    shell_mock_backend_inject_string("ABC");

    uint8_t buffer[10];
    int read = shell_mock_backend.read(buffer, sizeof(buffer));
    EXPECT_EQ(3, read);
    EXPECT_EQ('A', buffer[0]);
    EXPECT_EQ('B', buffer[1]);
    EXPECT_EQ('C', buffer[2]);
}

TEST_F(MockBackendTest, ReadPartialData) {
    shell_set_backend(&shell_mock_backend);

    shell_mock_backend_inject_string("ABCDEF");

    uint8_t buffer[3];
    int read1 = shell_mock_backend.read(buffer, sizeof(buffer));
    EXPECT_EQ(3, read1);
    EXPECT_EQ('A', buffer[0]);
    EXPECT_EQ('B', buffer[1]);
    EXPECT_EQ('C', buffer[2]);

    int read2 = shell_mock_backend.read(buffer, sizeof(buffer));
    EXPECT_EQ(3, read2);
    EXPECT_EQ('D', buffer[0]);
    EXPECT_EQ('E', buffer[1]);
    EXPECT_EQ('F', buffer[2]);
}

TEST_F(MockBackendTest, ReadWhenEmpty) {
    shell_set_backend(&shell_mock_backend);

    uint8_t buffer[10];
    int read = shell_mock_backend.read(buffer, sizeof(buffer));
    EXPECT_EQ(0, read);
}

TEST_F(MockBackendTest, ReadWithNullBuffer) {
    shell_set_backend(&shell_mock_backend);
    shell_mock_backend_inject_string("test");

    int read = shell_mock_backend.read(nullptr, 10);
    EXPECT_EQ(0, read);
}

/**
 * \}
 */

/**
 * \name            Mock Backend Write/Output Tests
 * \{
 */

TEST_F(MockBackendTest, WriteAndGetOutput) {
    shell_set_backend(&shell_mock_backend);

    uint8_t data[] = {'H', 'e', 'l', 'l', 'o'};
    int written = shell_mock_backend.write(data, sizeof(data));
    EXPECT_EQ(5, written);

    EXPECT_EQ(5u, shell_mock_backend_get_output_length());

    char output[256];
    int copied = shell_mock_backend_get_output_string(output, sizeof(output));
    EXPECT_EQ(5, copied);
    EXPECT_STREQ("Hello", output);
}

TEST_F(MockBackendTest, GetOutputData) {
    shell_set_backend(&shell_mock_backend);

    uint8_t data[] = {0x01, 0x02, 0x03};
    shell_mock_backend.write(data, sizeof(data));

    uint8_t output[10];
    int copied = shell_mock_backend_get_output(output, sizeof(output));
    EXPECT_EQ(3, copied);
    EXPECT_EQ(0x01, output[0]);
    EXPECT_EQ(0x02, output[1]);
    EXPECT_EQ(0x03, output[2]);
}

TEST_F(MockBackendTest, ClearOutput) {
    shell_set_backend(&shell_mock_backend);

    shell_mock_backend.write((const uint8_t*)"test", 4);
    EXPECT_EQ(4u, shell_mock_backend_get_output_length());

    shell_mock_backend_clear_output();
    EXPECT_EQ(0u, shell_mock_backend_get_output_length());
}

TEST_F(MockBackendTest, ResetBuffers) {
    shell_set_backend(&shell_mock_backend);

    shell_mock_backend_inject_string("input");
    shell_mock_backend.write((const uint8_t*)"output", 6);

    EXPECT_GT(shell_mock_backend_get_remaining_input(), 0u);
    EXPECT_GT(shell_mock_backend_get_output_length(), 0u);

    shell_mock_backend_reset();

    EXPECT_EQ(0u, shell_mock_backend_get_remaining_input());
    EXPECT_EQ(0u, shell_mock_backend_get_output_length());
}

/**
 * \}
 */

/**
 * \name            Mock Backend Edge Cases
 * \{
 */

TEST_F(MockBackendTest, GetOutputWithNullBuffer) {
    shell_set_backend(&shell_mock_backend);
    shell_mock_backend.write((const uint8_t*)"test", 4);

    EXPECT_EQ(0, shell_mock_backend_get_output(nullptr, 10));
    EXPECT_EQ(0, shell_mock_backend_get_output_string(nullptr, 10));
}

TEST_F(MockBackendTest, GetOutputWithZeroSize) {
    shell_set_backend(&shell_mock_backend);
    shell_mock_backend.write((const uint8_t*)"test", 4);

    uint8_t buffer[10];
    char str[10];
    EXPECT_EQ(0, shell_mock_backend_get_output(buffer, 0));
    EXPECT_EQ(0, shell_mock_backend_get_output_string(str, 0));
}

TEST_F(MockBackendTest, ReadWriteWhenNotInitialized) {
    shell_mock_backend_deinit();
    shell_set_backend(&shell_mock_backend);

    uint8_t buffer[10];
    EXPECT_EQ(0, shell_mock_backend.read(buffer, sizeof(buffer)));
    EXPECT_EQ(0, shell_mock_backend.write((const uint8_t*)"test", 4));
}

/**
 * \}
 */
