/**
 * \file            test_shell_backend_properties.cpp
 * \brief           Shell Backend Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell backend I/O operations.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware
 * **Property 8: Backend I/O Consistency**
 * **Validates: Requirements 8.1, 8.4, 8.5**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell_backend.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Maximum data size for property tests
 */
static constexpr size_t MAX_TEST_DATA_SIZE = 512;

/**
 * \brief           Shell Backend Property Test Fixture
 */
class ShellBackendPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        shell_mock_backend_init();
        shell_set_backend(&shell_mock_backend);
    }

    void TearDown() override {
        shell_set_backend(nullptr);
        shell_mock_backend_deinit();
    }

    /**
     * \brief           Generate random binary data
     * \param[in]       maxLen: Maximum length
     * \return          Random byte vector
     */
    std::vector<uint8_t> randomData(size_t maxLen = MAX_TEST_DATA_SIZE) {
        std::uniform_int_distribution<size_t> lenDist(1, maxLen);
        std::uniform_int_distribution<int> byteDist(0, 255);

        size_t len = lenDist(rng);
        std::vector<uint8_t> data(len);

        for (size_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(byteDist(rng));
        }
        return data;
    }

    /**
     * \brief           Generate random printable string
     * \param[in]       maxLen: Maximum length
     * \return          Random string
     */
    std::string randomString(size_t maxLen = 256) {
        std::uniform_int_distribution<size_t> lenDist(1, maxLen);
        std::uniform_int_distribution<int> charDist(32, 126);

        size_t len = lenDist(rng);
        std::string str;
        str.reserve(len);

        for (size_t i = 0; i < len; ++i) {
            str += static_cast<char>(charDist(rng));
        }
        return str;
    }

    /**
     * \brief           Generate random format string with arguments
     * \return          Pair of format string and expected result
     */
    std::pair<std::string, std::string> randomFormatString() {
        std::uniform_int_distribution<int> typeDist(0, 3);
        std::uniform_int_distribution<int> intDist(-10000, 10000);
        std::uniform_int_distribution<int> charDist('a', 'z');

        int type = typeDist(rng);
        char buffer[256];

        switch (type) {
            case 0: {
                /* Integer format */
                int val = intDist(rng);
                snprintf(buffer, sizeof(buffer), "%d", val);
                return {"Value: %d", std::string("Value: ") + buffer};
            }
            case 1: {
                /* Hex format */
                int val = intDist(rng);
                if (val < 0)
                    val = -val;
                snprintf(buffer, sizeof(buffer), "%x", val);
                return {"Hex: %x", std::string("Hex: ") + buffer};
            }
            case 2: {
                /* Character format */
                char c = static_cast<char>(charDist(rng));
                return {std::string("Char: %c"), std::string("Char: ") + c};
            }
            default: {
                /* Simple string */
                return {"Hello World", "Hello World"};
            }
        }
    }
};

/*---------------------------------------------------------------------------*/
/* Property 8: Backend I/O Consistency                                       */
/* *For any* data written to the backend, the write operation SHALL transmit */
/* all bytes, and the read operation SHALL be non-blocking.                  */
/* **Validates: Requirements 8.1, 8.4, 8.5**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 8: Backend I/O Consistency
 *
 * *For any* data written to the backend, the write operation SHALL transmit
 * all bytes.
 *
 * **Validates: Requirements 8.1, 8.5**
 */
TEST_F(ShellBackendPropertyTest, Property8_WriteTransmitsAllBytes) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Write data */
        int written = shell_write(data.data(), static_cast<int>(data.size()));

        /* Verify all bytes were written */
        EXPECT_EQ(static_cast<int>(data.size()), written)
            << "Iter " << iter << ": write should transmit all " << data.size()
            << " bytes";

        /* Verify output matches input */
        size_t outputLen = shell_mock_backend_get_output_length();
        EXPECT_EQ(data.size(), outputLen)
            << "Iter " << iter << ": output length should match input";

        std::vector<uint8_t> output(outputLen);
        shell_mock_backend_get_output(output.data(), outputLen);

        EXPECT_EQ(data, output)
            << "Iter " << iter << ": output data should match input";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8a: Read Is Non-Blocking
 *
 * *For any* read operation, the read SHALL return immediately with available
 * data or 0 if no data is available.
 *
 * **Validates: Requirements 8.4**
 */
TEST_F(ShellBackendPropertyTest, Property8a_ReadIsNonBlocking) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Read with no data should return 0 immediately */
        uint8_t buffer[256];
        int read = shell_mock_backend.read(buffer, sizeof(buffer));
        EXPECT_EQ(0, read) << "Iter " << iter
                           << ": read with no data should return 0";

        /* Inject some data */
        std::vector<uint8_t> data = randomData(100);
        shell_mock_backend_inject_input(data.data(), data.size());

        /* Read should return available data */
        read = shell_mock_backend.read(buffer, sizeof(buffer));
        EXPECT_EQ(static_cast<int>(data.size()), read)
            << "Iter " << iter << ": read should return all available data";

        /* Verify data matches */
        for (int i = 0; i < read; ++i) {
            EXPECT_EQ(data[i], buffer[i])
                << "Iter " << iter << ": byte " << i << " should match";
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property 8b: Printf Output Consistency
 *
 * *For any* printf format string and arguments, the output SHALL match
 * the expected formatted result.
 *
 * **Validates: Requirements 8.1**
 */
TEST_F(ShellBackendPropertyTest, Property8b_PrintfOutputConsistency) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate random string to print */
        std::string str = randomString(200);

        /* Print using shell_printf */
        int result = shell_printf("%s", str.c_str());
        EXPECT_GT(result, 0) << "Iter " << iter << ": printf should succeed";

        /* Get output */
        char output[512];
        shell_mock_backend_get_output_string(output, sizeof(output));

        /* Verify output matches */
        EXPECT_STREQ(str.c_str(), output)
            << "Iter " << iter << ": output should match input string";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8c: Puts Output Consistency
 *
 * *For any* string written via shell_puts, the output SHALL contain
 * exactly the input string.
 *
 * **Validates: Requirements 8.1**
 */
TEST_F(ShellBackendPropertyTest, Property8c_PutsOutputConsistency) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate random string */
        std::string str = randomString(200);

        /* Write using shell_puts */
        int written = shell_puts(str.c_str());
        EXPECT_EQ(static_cast<int>(str.length()), written)
            << "Iter " << iter << ": puts should return string length";

        /* Get output */
        char output[512];
        shell_mock_backend_get_output_string(output, sizeof(output));

        /* Verify output matches */
        EXPECT_STREQ(str.c_str(), output)
            << "Iter " << iter << ": output should match input string";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8d: Putchar Sequence Consistency
 *
 * *For any* sequence of characters written via shell_putchar, the output
 * SHALL contain exactly those characters in order.
 *
 * **Validates: Requirements 8.1**
 */
TEST_F(ShellBackendPropertyTest, Property8d_PutcharSequenceConsistency) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate random string */
        std::string str = randomString(100);

        /* Write character by character */
        for (char c : str) {
            int result = shell_putchar(c);
            EXPECT_EQ(1, result)
                << "Iter " << iter << ": putchar should return 1";
        }

        /* Get output */
        char output[256];
        shell_mock_backend_get_output_string(output, sizeof(output));

        /* Verify output matches */
        EXPECT_STREQ(str.c_str(), output)
            << "Iter " << iter << ": output should match input sequence";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8e: Read Partial Data Consistency
 *
 * *For any* injected data, reading in chunks SHALL eventually retrieve
 * all the data in the correct order.
 *
 * **Validates: Requirements 8.4**
 */
TEST_F(ShellBackendPropertyTest, Property8e_ReadPartialDataConsistency) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate random data */
        std::vector<uint8_t> data = randomData(200);
        shell_mock_backend_inject_input(data.data(), data.size());

        /* Read in random-sized chunks */
        std::vector<uint8_t> received;
        std::uniform_int_distribution<int> chunkDist(1, 50);

        while (shell_mock_backend_get_remaining_input() > 0) {
            int chunkSize = chunkDist(rng);
            uint8_t buffer[64];
            int read = shell_mock_backend.read(
                buffer, std::min(chunkSize, static_cast<int>(sizeof(buffer))));

            for (int i = 0; i < read; ++i) {
                received.push_back(buffer[i]);
            }
        }

        /* Verify all data was received in order */
        EXPECT_EQ(data.size(), received.size())
            << "Iter " << iter << ": should receive all data";

        EXPECT_EQ(data, received)
            << "Iter " << iter << ": received data should match original";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8f: Multiple Write Accumulation
 *
 * *For any* sequence of writes, the output SHALL contain all written data
 * in the order it was written.
 *
 * **Validates: Requirements 8.5**
 */
TEST_F(ShellBackendPropertyTest, Property8f_MultipleWriteAccumulation) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Generate multiple random strings */
        std::uniform_int_distribution<int> countDist(2, 10);
        int count = countDist(rng);

        std::string expected;
        for (int i = 0; i < count; ++i) {
            std::string str = randomString(50);
            shell_puts(str.c_str());
            expected += str;
        }

        /* Get accumulated output */
        std::vector<char> output(expected.length() + 1);
        shell_mock_backend_get_output_string(output.data(), output.size());

        /* Verify accumulated output */
        EXPECT_STREQ(expected.c_str(), output.data())
            << "Iter " << iter << ": accumulated output should match";
    }
}

/**
 * Feature: shell-cli-middleware, Property 8g: Backend Switch Consistency
 *
 * *For any* backend switch, subsequent I/O operations SHALL use the new
 * backend.
 *
 * **Validates: Requirements 8.2**
 */
TEST_F(ShellBackendPropertyTest, Property8g_BackendSwitchConsistency) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        shell_mock_backend_reset();

        /* Write some data */
        std::string str1 = randomString(50);
        shell_puts(str1.c_str());

        /* Verify data was written */
        size_t len1 = shell_mock_backend_get_output_length();
        EXPECT_EQ(str1.length(), len1)
            << "Iter " << iter << ": first write should succeed";

        /* Clear backend */
        shell_set_backend(nullptr);

        /* Write should fail with no backend */
        std::string str2 = randomString(50);
        int result = shell_puts(str2.c_str());
        EXPECT_EQ(0, result)
            << "Iter " << iter << ": write with no backend should return 0";

        /* Restore backend */
        shell_mock_backend_reset();
        shell_set_backend(&shell_mock_backend);

        /* Write should succeed again */
        std::string str3 = randomString(50);
        result = shell_puts(str3.c_str());
        EXPECT_EQ(static_cast<int>(str3.length()), result)
            << "Iter " << iter << ": write after restore should succeed";

        /* Verify only str3 is in output (buffer was reset) */
        char output[256];
        shell_mock_backend_get_output_string(output, sizeof(output));
        EXPECT_STREQ(str3.c_str(), output)
            << "Iter " << iter << ": output should only contain str3";
    }
}
