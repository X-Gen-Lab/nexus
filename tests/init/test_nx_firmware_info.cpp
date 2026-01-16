/**
 * \file            test_nx_firmware_info.cpp
 * \brief           Tests for nx_firmware_info.h firmware information
 * \author          Nexus Team
 *
 * Unit tests for the firmware information system including:
 * - Version encoding and decoding
 * - Version string formatting
 * - Firmware info structure
 *
 * **Validates: Requirements 8.1-8.5**
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "nx_firmware_info.h"

/* Test helper for MSVC to set firmware info */
#if defined(_MSC_VER)
void _nx_set_firmware_info_test(const nx_firmware_info_t* info);
#endif
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for nx_firmware_info tests
 */
class NxFirmwareInfoTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Nothing to set up */
    }

    void TearDown() override {
        /* Nothing to tear down */
    }
};

/*---------------------------------------------------------------------------*/
/* Version Encoding Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test NX_VERSION_ENCODE with typical values
 */
TEST_F(NxFirmwareInfoTest, VersionEncode_TypicalValues) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

    /* Expected: 0x01020304 */
    EXPECT_EQ(version, 0x01020304u);
}

/**
 * \brief           Test NX_VERSION_ENCODE with zero values
 */
TEST_F(NxFirmwareInfoTest, VersionEncode_ZeroValues) {
    uint32_t version = NX_VERSION_ENCODE(0, 0, 0, 0);

    EXPECT_EQ(version, 0x00000000u);
}

/**
 * \brief           Test NX_VERSION_ENCODE with maximum values
 */
TEST_F(NxFirmwareInfoTest, VersionEncode_MaxValues) {
    uint32_t version = NX_VERSION_ENCODE(255, 255, 255, 255);

    EXPECT_EQ(version, 0xFFFFFFFFu);
}

/**
 * \brief           Test NX_VERSION_ENCODE with mixed values
 */
TEST_F(NxFirmwareInfoTest, VersionEncode_MixedValues) {
    /* Version 10.20.30.40 */
    uint32_t version = NX_VERSION_ENCODE(10, 20, 30, 40);

    /* Expected: 0x0A141E28 */
    EXPECT_EQ(version, 0x0A141E28u);
}

/*---------------------------------------------------------------------------*/
/* Version Decoding Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test NX_VERSION_MAJOR extraction
 */
TEST_F(NxFirmwareInfoTest, VersionMajor_Extraction) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

    EXPECT_EQ(NX_VERSION_MAJOR(version), 1u);
}

/**
 * \brief           Test NX_VERSION_MINOR extraction
 */
TEST_F(NxFirmwareInfoTest, VersionMinor_Extraction) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

    EXPECT_EQ(NX_VERSION_MINOR(version), 2u);
}

/**
 * \brief           Test NX_VERSION_PATCH extraction
 */
TEST_F(NxFirmwareInfoTest, VersionPatch_Extraction) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

    EXPECT_EQ(NX_VERSION_PATCH(version), 3u);
}

/**
 * \brief           Test NX_VERSION_BUILD extraction
 */
TEST_F(NxFirmwareInfoTest, VersionBuild_Extraction) {
    uint32_t version = NX_VERSION_ENCODE(1, 2, 3, 4);

    EXPECT_EQ(NX_VERSION_BUILD(version), 4u);
}

/**
 * \brief           Test version round-trip encoding/decoding
 *
 * Property: For any valid version components, encoding then decoding
 * should return the original values.
 */
TEST_F(NxFirmwareInfoTest, VersionRoundTrip) {
    /* Test multiple version combinations */
    struct {
        uint8_t major, minor, patch, build;
    } test_cases[] = {
        {0, 0, 0, 0},     {1, 0, 0, 0},        {1, 2, 3, 4},
        {10, 20, 30, 40}, {100, 200, 150, 50}, {255, 255, 255, 255},
    };

    for (const auto& tc : test_cases) {
        uint32_t version =
            NX_VERSION_ENCODE(tc.major, tc.minor, tc.patch, tc.build);

        EXPECT_EQ(NX_VERSION_MAJOR(version), tc.major)
            << "Major mismatch for " << (int)tc.major << "." << (int)tc.minor
            << "." << (int)tc.patch << "." << (int)tc.build;
        EXPECT_EQ(NX_VERSION_MINOR(version), tc.minor)
            << "Minor mismatch for " << (int)tc.major << "." << (int)tc.minor
            << "." << (int)tc.patch << "." << (int)tc.build;
        EXPECT_EQ(NX_VERSION_PATCH(version), tc.patch)
            << "Patch mismatch for " << (int)tc.major << "." << (int)tc.minor
            << "." << (int)tc.patch << "." << (int)tc.build;
        EXPECT_EQ(NX_VERSION_BUILD(version), tc.build)
            << "Build mismatch for " << (int)tc.major << "." << (int)tc.minor
            << "." << (int)tc.patch << "." << (int)tc.build;
    }
}

/*---------------------------------------------------------------------------*/
/* Version String Tests                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test nx_get_version_string with NULL buffer
 */
TEST_F(NxFirmwareInfoTest, VersionString_NullBuffer) {
    size_t result = nx_get_version_string(nullptr, 32);

    EXPECT_EQ(result, 0u);
}

/**
 * \brief           Test nx_get_version_string with zero size
 */
TEST_F(NxFirmwareInfoTest, VersionString_ZeroSize) {
    char buf[32];
    size_t result = nx_get_version_string(buf, 0);

    EXPECT_EQ(result, 0u);
}

/**
 * \brief           Test nx_get_version_string when no firmware info defined
 *
 * Note: Without NX_FIRMWARE_INFO_DEFINE, the function should return 0.
 */
TEST_F(NxFirmwareInfoTest, VersionString_NoFirmwareInfo) {
    char buf[32] = "unchanged";
    size_t result = nx_get_version_string(buf, sizeof(buf));

    /* Without firmware info defined, should return 0 */
    /* The buffer may or may not be modified depending on implementation */
    EXPECT_EQ(result, 0u);
}

/*---------------------------------------------------------------------------*/
/* Firmware Info API Tests                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test nx_get_firmware_info when no info defined
 *
 * Note: Without NX_FIRMWARE_INFO_DEFINE, should return NULL.
 */
TEST_F(NxFirmwareInfoTest, GetFirmwareInfo_NoInfoDefined) {
    const nx_firmware_info_t* info = nx_get_firmware_info();

    /* Without firmware info defined, should return NULL */
    EXPECT_EQ(info, nullptr);
}

/*---------------------------------------------------------------------------*/
/* Structure Size Tests                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test firmware info structure size
 *
 * Verify the structure has expected size for binary compatibility.
 */
TEST_F(NxFirmwareInfoTest, StructureSize) {
    /* Structure should be:
     * - product[32]: 32 bytes
     * - factory[16]: 16 bytes
     * - date[12]: 12 bytes
     * - time[12]: 12 bytes
     * - version: 4 bytes
     * - key: 4 bytes
     * Total: 80 bytes (may have padding)
     */
    EXPECT_GE(sizeof(nx_firmware_info_t), 80u);
}

/**
 * \brief           Test firmware info structure field offsets
 *
 * Verify fields are at expected offsets for binary compatibility.
 */
TEST_F(NxFirmwareInfoTest, StructureFieldOffsets) {
    /* Product should be at offset 0 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, product), 0u);

    /* Factory should be at offset 32 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, factory), 32u);

    /* Date should be at offset 48 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, date), 48u);

    /* Time should be at offset 60 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, time), 60u);

    /* Version should be at offset 72 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, version), 72u);

    /* Key should be at offset 76 */
    EXPECT_EQ(offsetof(nx_firmware_info_t, key), 76u);
}

/*---------------------------------------------------------------------------*/
/* Version String Formatting Tests (with test helper)                        */
/*---------------------------------------------------------------------------*/

#if defined(_MSC_VER)
/**
 * \brief           Test fixture for firmware info with test helper
 */
class NxFirmwareInfoWithHelperTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Clear any previous firmware info */
        _nx_set_firmware_info_test(nullptr);
    }

    void TearDown() override {
        /* Clear firmware info after test */
        _nx_set_firmware_info_test(nullptr);
    }
};

/**
 * \brief           Test nx_get_firmware_info with defined info
 */
TEST_F(NxFirmwareInfoWithHelperTest, GetFirmwareInfo_WithInfo) {
    nx_firmware_info_t test_info = {.product = "Test Product",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version = NX_VERSION_ENCODE(1, 2, 3, 4),
                                    .key = 0x12345678};

    _nx_set_firmware_info_test(&test_info);

    const nx_firmware_info_t* info = nx_get_firmware_info();
    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->product, "Test Product");
    EXPECT_STREQ(info->factory, "TEST");
    EXPECT_EQ(info->version, NX_VERSION_ENCODE(1, 2, 3, 4));
    EXPECT_EQ(info->key, 0x12345678u);
}

/**
 * \brief           Test nx_get_version_string with typical version
 */
TEST_F(NxFirmwareInfoWithHelperTest, VersionString_TypicalVersion) {
    nx_firmware_info_t test_info = {.product = "Test",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version = NX_VERSION_ENCODE(1, 2, 3, 4),
                                    .key = 0};

    _nx_set_firmware_info_test(&test_info);

    char buf[32];
    size_t len = nx_get_version_string(buf, sizeof(buf));

    EXPECT_GT(len, 0u);
    EXPECT_STREQ(buf, "1.2.3.4");
}

/**
 * \brief           Test nx_get_version_string with zero version
 */
TEST_F(NxFirmwareInfoWithHelperTest, VersionString_ZeroVersion) {
    nx_firmware_info_t test_info = {.product = "Test",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version = NX_VERSION_ENCODE(0, 0, 0, 0),
                                    .key = 0};

    _nx_set_firmware_info_test(&test_info);

    char buf[32];
    size_t len = nx_get_version_string(buf, sizeof(buf));

    EXPECT_GT(len, 0u);
    EXPECT_STREQ(buf, "0.0.0.0");
}

/**
 * \brief           Test nx_get_version_string with large version numbers
 */
TEST_F(NxFirmwareInfoWithHelperTest, VersionString_LargeNumbers) {
    nx_firmware_info_t test_info = {.product = "Test",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version =
                                        NX_VERSION_ENCODE(100, 200, 150, 50),
                                    .key = 0};

    _nx_set_firmware_info_test(&test_info);

    char buf[32];
    size_t len = nx_get_version_string(buf, sizeof(buf));

    EXPECT_GT(len, 0u);
    EXPECT_STREQ(buf, "100.200.150.50");
}

/**
 * \brief           Test nx_get_version_string with small buffer
 */
TEST_F(NxFirmwareInfoWithHelperTest, VersionString_SmallBuffer) {
    nx_firmware_info_t test_info = {.product = "Test",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version = NX_VERSION_ENCODE(1, 2, 3, 4),
                                    .key = 0};

    _nx_set_firmware_info_test(&test_info);

    /* Buffer too small for full version string */
    char buf[4];
    size_t len = nx_get_version_string(buf, sizeof(buf));

    /* Should truncate but not overflow */
    EXPECT_LE(len, 3u);
    EXPECT_EQ(buf[3], '\0');
}

/**
 * \brief           Test nx_get_version_string with max version
 */
TEST_F(NxFirmwareInfoWithHelperTest, VersionString_MaxVersion) {
    nx_firmware_info_t test_info = {.product = "Test",
                                    .factory = "TEST",
                                    .date = "Jan 16 2026",
                                    .time = "12:00:00",
                                    .version =
                                        NX_VERSION_ENCODE(255, 255, 255, 255),
                                    .key = 0};

    _nx_set_firmware_info_test(&test_info);

    char buf[32];
    size_t len = nx_get_version_string(buf, sizeof(buf));

    EXPECT_GT(len, 0u);
    EXPECT_STREQ(buf, "255.255.255.255");
}
#endif /* _MSC_VER */
