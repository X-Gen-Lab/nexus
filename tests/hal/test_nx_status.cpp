/**
 * \file            test_nx_status.cpp
 * \brief           Tests for nx_status.h error handling infrastructure
 * \author          Nexus Team
 *
 * Unit tests for the unified error code system including:
 * - nx_status_to_string() conversion function
 * - Error callback mechanism
 * - Helper macros (NX_IS_OK, NX_IS_ERROR, NX_RETURN_IF_ERROR)
 *
 * **Validates: Requirements 1.1, 1.2, 1.4, 1.5, 1.6**
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_status.h"
}

/**
 * \brief           Test fixture for nx_status tests
 */
class NxStatusTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Reset error callback before each test
        nx_set_error_callback(nullptr, nullptr);
        callback_count = 0;
        last_status = NX_OK;
        last_module = nullptr;
        last_msg = nullptr;
    }

    void TearDown() override {
        nx_set_error_callback(nullptr, nullptr);
    }

    // Static callback tracking variables
    static int callback_count;
    static nx_status_t last_status;
    static const char* last_module;
    static const char* last_msg;

    // Error callback for testing
    static void test_error_callback(void* user_data, nx_status_t status,
                                    const char* module, const char* msg) {
        callback_count++;
        last_status = status;
        last_module = module;
        last_msg = msg;

        // Verify user_data if provided
        if (user_data != nullptr) {
            int* counter = static_cast<int*>(user_data);
            (*counter)++;
        }
    }
};

// Initialize static members
int NxStatusTest::callback_count = 0;
nx_status_t NxStatusTest::last_status = NX_OK;
const char* NxStatusTest::last_module = nullptr;
const char* NxStatusTest::last_msg = nullptr;

/*===========================================================================*/
/* nx_status_to_string() Tests                                                */
/*===========================================================================*/

/**
 * \brief           Test nx_status_to_string() returns "OK" for NX_OK
 */
TEST_F(NxStatusTest, StatusToString_NX_OK) {
    const char* str = nx_status_to_string(NX_OK);
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "OK");
}

/**
 * \brief           Test nx_status_to_string() for general errors
 */
TEST_F(NxStatusTest, StatusToString_GeneralErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_GENERIC), "Generic error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_INVALID_PARAM),
                 "Invalid parameter");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NULL_PTR), "Null pointer");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NOT_SUPPORTED), "Not supported");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NOT_FOUND), "Not found");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_INVALID_SIZE), "Invalid size");
}

/**
 * \brief           Test nx_status_to_string() for state errors
 */
TEST_F(NxStatusTest, StatusToString_StateErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NOT_INIT), "Not initialized");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_ALREADY_INIT),
                 "Already initialized");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_INVALID_STATE), "Invalid state");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_BUSY), "Device busy");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_SUSPENDED), "Device suspended");
}

/**
 * \brief           Test nx_status_to_string() for resource errors
 */
TEST_F(NxStatusTest, StatusToString_ResourceErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NO_MEMORY), "Out of memory");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NO_RESOURCE),
                 "Resource unavailable");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_RESOURCE_BUSY), "Resource busy");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_LOCKED), "Resource locked");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_FULL), "Buffer full");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_EMPTY), "Buffer empty");
}

/**
 * \brief           Test nx_status_to_string() for timeout errors
 */
TEST_F(NxStatusTest, StatusToString_TimeoutErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_TIMEOUT), "Timeout");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_WOULD_BLOCK), "Would block");
}

/**
 * \brief           Test nx_status_to_string() for IO errors
 */
TEST_F(NxStatusTest, StatusToString_IOErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_IO), "IO error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_OVERRUN), "Buffer overrun");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_UNDERRUN), "Buffer underrun");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_PARITY), "Parity error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_FRAMING), "Framing error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NOISE), "Noise error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NACK), "NACK received");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_BUS), "Bus error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_ARBITRATION), "Arbitration lost");
}

/**
 * \brief           Test nx_status_to_string() for DMA errors
 */
TEST_F(NxStatusTest, StatusToString_DMAErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_DMA), "DMA error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_DMA_TRANSFER),
                 "DMA transfer error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_DMA_CONFIG),
                 "DMA configuration error");
}

/**
 * \brief           Test nx_status_to_string() for data errors
 */
TEST_F(NxStatusTest, StatusToString_DataErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_NO_DATA), "No data available");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_DATA_SIZE), "Data size error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_CRC), "CRC error");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_CHECKSUM), "Checksum error");
}

/**
 * \brief           Test nx_status_to_string() for permission errors
 */
TEST_F(NxStatusTest, StatusToString_PermissionErrors) {
    EXPECT_STREQ(nx_status_to_string(NX_ERR_PERMISSION), "Permission denied");
    EXPECT_STREQ(nx_status_to_string(NX_ERR_READ_ONLY), "Read-only resource");
}

/**
 * \brief           Test nx_status_to_string() for unknown error codes
 */
TEST_F(NxStatusTest, StatusToString_UnknownError) {
    // Test with an invalid error code
    const char* str = nx_status_to_string(static_cast<nx_status_t>(9999));
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "Unknown error");
}

/*===========================================================================*/
/* Helper Macro Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test NX_IS_OK macro
 */
TEST_F(NxStatusTest, Macro_NX_IS_OK) {
    EXPECT_TRUE(NX_IS_OK(NX_OK));
    EXPECT_FALSE(NX_IS_OK(NX_ERR_GENERIC));
    EXPECT_FALSE(NX_IS_OK(NX_ERR_TIMEOUT));
    EXPECT_FALSE(NX_IS_OK(NX_ERR_NULL_PTR));
}

/**
 * \brief           Test NX_IS_ERROR macro
 */
TEST_F(NxStatusTest, Macro_NX_IS_ERROR) {
    EXPECT_FALSE(NX_IS_ERROR(NX_OK));
    EXPECT_TRUE(NX_IS_ERROR(NX_ERR_GENERIC));
    EXPECT_TRUE(NX_IS_ERROR(NX_ERR_TIMEOUT));
    EXPECT_TRUE(NX_IS_ERROR(NX_ERR_NULL_PTR));
}

/*===========================================================================*/
/* Error Callback Tests                                                       */
/*===========================================================================*/

/**
 * \brief           Test error callback registration and invocation
 */
TEST_F(NxStatusTest, ErrorCallback_Registration) {
    // Register callback
    nx_set_error_callback(test_error_callback, nullptr);

    // Report an error
    nx_report_error(NX_ERR_TIMEOUT, "test_module", "Test error message");

    // Verify callback was invoked
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(last_status, NX_ERR_TIMEOUT);
    EXPECT_STREQ(last_module, "test_module");
    EXPECT_STREQ(last_msg, "Test error message");
}

/**
 * \brief           Test error callback with user data
 */
TEST_F(NxStatusTest, ErrorCallback_UserData) {
    int user_counter = 0;

    // Register callback with user data
    nx_set_error_callback(test_error_callback, &user_counter);

    // Report errors
    nx_report_error(NX_ERR_GENERIC, "module1", "Error 1");
    nx_report_error(NX_ERR_BUSY, "module2", "Error 2");

    // Verify user data was passed correctly
    EXPECT_EQ(user_counter, 2);
    EXPECT_EQ(callback_count, 2);
}

/**
 * \brief           Test that NX_OK does not trigger callback
 */
TEST_F(NxStatusTest, ErrorCallback_NoCallbackForOK) {
    nx_set_error_callback(test_error_callback, nullptr);

    // Report NX_OK (should not trigger callback)
    nx_report_error(NX_OK, "test", "This should not trigger callback");

    // Verify callback was NOT invoked
    EXPECT_EQ(callback_count, 0);
}

/**
 * \brief           Test callback with NULL module and message
 */
TEST_F(NxStatusTest, ErrorCallback_NullModuleAndMessage) {
    nx_set_error_callback(test_error_callback, nullptr);

    // Report error with NULL module and message
    nx_report_error(NX_ERR_IO, nullptr, nullptr);

    // Verify callback was invoked
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(last_status, NX_ERR_IO);
    EXPECT_EQ(last_module, nullptr);
    EXPECT_EQ(last_msg, nullptr);
}

/**
 * \brief           Test disabling callback by setting to NULL
 */
TEST_F(NxStatusTest, ErrorCallback_Disable) {
    // Register callback
    nx_set_error_callback(test_error_callback, nullptr);
    nx_report_error(NX_ERR_GENERIC, "test", "First error");
    EXPECT_EQ(callback_count, 1);

    // Disable callback
    nx_set_error_callback(nullptr, nullptr);
    nx_report_error(NX_ERR_GENERIC, "test", "Second error");

    // Verify callback was NOT invoked after disabling
    EXPECT_EQ(callback_count, 1);
}
