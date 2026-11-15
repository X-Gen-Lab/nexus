/**
 * \file            mcuboot_boot_test.cpp
 * \brief           Ordinary valid signed-image selection and overwrite copy.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mcuboot_boot_call.h"
#include "mcuboot_memory_model.h"
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using namespace nexus_mcuboot_test;

class BootFlow : public Memory {
  protected:
    std::vector<uint8_t> image(const char* name) {
        std::ifstream stream(std::string(NX_MCUBOOT_VALID_IMAGES) + "/" + name,
                             std::ios::binary);
        EXPECT_TRUE(stream.good());
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    void install(uint32_t offset, const std::vector<uint8_t>& input) {
        ASSERT_LE(input.size(), 0x20000U);
        std::copy(input.begin(), input.end(), bytes.begin() + offset);
    }
    void primary_v1() {
        install(0x20000U, image("version-1.0.0.signed.bin"));
    }
};

TEST_F(BootFlow, PublicTestKeyAuthenticatesNormalLinkedPrimary) {
    primary_v1();
    nx_test_mcuboot_selected_t selected{};
    ASSERT_EQ(0, nx_test_mcuboot_boot(&selected));
    EXPECT_EQ(0x20000U, selected.offset);
    EXPECT_EQ(512U, selected.header_size);
    EXPECT_EQ(1U, selected.major);
    EXPECT_EQ(0U, selected.minor);
}

TEST_F(BootFlow, UnrequestedValidSecondaryDoesNotSelectAnUpgrade) {
    primary_v1();
    install(0x40000U, image("version-1.1.0.signed.bin"));
    nx_test_mcuboot_selected_t selected{};
    ASSERT_EQ(0, nx_test_mcuboot_boot(&selected));
    EXPECT_EQ(0U, selected.minor);
    EXPECT_EQ(0U, programs);
}

TEST_F(BootFlow, PermanentValidCandidateCopiesAndSelectsNewPrimary) {
    primary_v1();
    const auto candidate = image("version-1.1.0.signed.bin");
    install(0x40000U, candidate);
    allow_programs();
    allow_erases();
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    nx_test_mcuboot_selected_t selected{};
    ASSERT_EQ(0, nx_test_mcuboot_boot(&selected));
    EXPECT_EQ(0x20000U, selected.offset);
    EXPECT_EQ(512U, selected.header_size);
    EXPECT_EQ(1U, selected.minor);
    EXPECT_TRUE(std::equal(candidate.begin(), candidate.end(),
                           bytes.begin() + 0x20000U));
    EXPECT_GT(erases, 0U);
}

TEST_F(BootFlow, NormalRestartKeepsNewPrimaryWithoutReapplyingCandidate) {
    primary_v1();
    install(0x40000U, image("version-1.1.0.signed.bin"));
    allow_programs();
    allow_erases();
    ASSERT_EQ(NX_SUCCESS, nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT));
    nx_test_mcuboot_selected_t selected{};
    ASSERT_EQ(0, nx_test_mcuboot_boot(&selected));
    const size_t programs_before = programs;
    const size_t erases_before = erases;
    ASSERT_EQ(0, nx_test_mcuboot_boot(&selected));
    EXPECT_EQ(1U, selected.minor);
    EXPECT_EQ(programs_before, programs);
    EXPECT_EQ(erases_before, erases);
}
} /* namespace */
