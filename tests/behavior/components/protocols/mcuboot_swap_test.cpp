/**
 * \file            mcuboot_swap_test.cpp
 * \brief           Real authenticated scratch swap, trial and revert contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mcuboot_boot_call.h"
#include "nexus/components/mcuboot_swap_control.h"
#include "nexus/mcuboot_swap_port.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <csetjmp>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

namespace {
const nx_mcuboot_swap_flash_operation_t* active = nullptr;

class SwapFlow : public testing::Test {
  protected:
    static constexpr uint32_t primary_offset = 0x20000;
    static constexpr uint32_t secondary_offset = 0x80000;
    static constexpr uint32_t scratch_offset = 0xe0000;
    static constexpr uint32_t slot_bytes = 0x60000;
    static constexpr uint32_t image_ok_offset = slot_bytes - 24;
    std::array<uint8_t, 0x100000> bytes = {};
    const std::array<nx_flash_erase_span_t, 3> spans = {
        {{0, 0x4000, 4}, {0x10000, 0x10000, 1}, {0x20000, 0x20000, 7}}};
    const nx_flash_geometry_t physical = {0x08000000,   0x100000,     4,
                                          spans.data(), spans.size(), 0};
    nx_time_us_t now = 100;
    size_t programs = 0;
    size_t erases = 0;
    uint32_t cut_program = UINT32_MAX;
    uint32_t cut_erase = UINT32_MAX;
    std::jmp_buf reset;

    static const nx_flash_geometry_t* geometry(const void* context) {
        return &static_cast<const SwapFlow*>(context)->physical;
    }
    static nx_result_t read(const void* context, uint32_t offset, void* output,
                            size_t length) {
        const auto& self = *static_cast<const SwapFlow*>(context);
        if (offset > self.bytes.size() || length > self.bytes.size() - offset) {
            return NX_ERROR_INVALID;
        }
        std::copy_n(self.bytes.data() + offset, length,
                    static_cast<uint8_t*>(output));
        return NX_SUCCESS;
    }
    static nx_result_t program(void* context, uint32_t offset,
                               const void* input, size_t length,
                               nx_time_us_t deadline) {
        auto& self = *static_cast<SwapFlow*>(context);
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        const auto* data = static_cast<const uint8_t*>(input);
        const size_t applied = offset == self.cut_program ? length / 2 : length;
        for (size_t index = 0; index < applied; ++index) {
            if ((self.bytes[offset + index] & data[index]) != data[index]) {
                return NX_ERROR_STATE;
            }
            self.bytes[offset + index] &= data[index];
        }
        ++self.programs;
        if (offset == self.cut_program) {
            self.cut_program = UINT32_MAX;
            std::longjmp(self.reset, 1);
        }
        return NX_SUCCESS;
    }
    static nx_result_t erase(void* context, uint32_t offset, size_t length,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<SwapFlow*>(context);
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        const size_t applied = offset == self.cut_erase ? length / 2 : length;
        std::fill_n(self.bytes.begin() + offset, applied, 0xff);
        ++self.erases;
        if (offset == self.cut_erase) {
            self.cut_erase = UINT32_MAX;
            std::longjmp(self.reset, 1);
        }
        return NX_SUCCESS;
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<SwapFlow*>(context)->now;
    }
    const nx_flash_ops_t ops = {geometry, read, program, erase, nullptr};
    const nx_flash_port_t flash = {&ops, this};
    nx_flash_region_t primary_region = {&flash, primary_offset, slot_bytes,
                                        true};
    nx_flash_region_t secondary_region = {&flash, secondary_offset, slot_bytes,
                                          true};
    nx_flash_region_t scratch_region = {&flash, scratch_offset, 0x20000, true};
    const struct flash_area primary = {NX_MCUBOOT_PRIMARY_AREA_ID,
                                       &primary_region};
    const struct flash_area secondary = {NX_MCUBOOT_SECONDARY_AREA_ID,
                                         &secondary_region};
    const struct flash_area scratch = {NX_MCUBOOT_SCRATCH_AREA_ID,
                                       &scratch_region};
    const nx_mcuboot_swap_flash_operation_t operation = {
        &primary, &secondary, &scratch, {clock, this}, 10000};

    std::vector<uint8_t> image(const char* name) {
        std::ifstream stream(std::string(NX_MCUBOOT_VALID_IMAGES) + "/" + name,
                             std::ios::binary);
        EXPECT_TRUE(stream.good());
        return {std::istreambuf_iterator<char>(stream), {}};
    }
    void install(uint32_t offset, const char* name) {
        const auto data = image(name);
        ASSERT_LT(data.size(), slot_bytes - 1024);
        std::copy(data.begin(), data.end(), bytes.begin() + offset);
    }
    void SetUp() override {
        bytes.fill(0xff);
        active = &operation;
        install(primary_offset, "version-1.0.0.signed.bin");
        install(secondary_offset, "version-1.1.0.signed.bin");
    }
    void TearDown() override {
        active = nullptr;
    }
};

TEST_F(SwapFlow, SignedUnrequestedCandidateLeavesPrimaryAndFlashUnchanged) {
    nx_test_mcuboot_selected_t selected = {};
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.offset, primary_offset);
    EXPECT_EQ(selected.minor, 0);
    EXPECT_EQ(erases, 0U);
    EXPECT_EQ(programs, 0U);
}

TEST_F(SwapFlow, TrialActuallySwapsThenRevertsWhenNotConfirmed) {
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    nx_test_mcuboot_selected_t selected = {};
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
    EXPECT_GT(erases, 0U);
    EXPECT_GT(programs, 0U);
    EXPECT_EQ(bytes[primary_offset + image_ok_offset], 0xff);
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_NONE;
    ASSERT_EQ(nx_mcuboot_swap_query(&action), NX_SUCCESS);
    EXPECT_EQ(action, NX_MCUBOOT_ACTION_REVERT);
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 0);
}

TEST_F(SwapFlow, ConfirmedTrialPersistsAndSecondBootDoesNotSwapAgain) {
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    nx_test_mcuboot_selected_t selected = {};
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    ASSERT_EQ(nx_mcuboot_swap_confirm(), NX_SUCCESS);
    EXPECT_EQ(bytes[primary_offset + image_ok_offset], 1);
    bool confirmed = false;
    ASSERT_EQ(nx_mcuboot_swap_confirmed(&confirmed), NX_SUCCESS);
    EXPECT_TRUE(confirmed);
    const auto previous_erases = erases;
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
    EXPECT_EQ(erases, previous_erases);
}

TEST_F(SwapFlow, PermanentRequestKeepsNewImageWithoutApplicationConfirm) {
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_PERMANENT),
              NX_SUCCESS);
    nx_test_mcuboot_selected_t selected = {};
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
}

TEST_F(SwapFlow, CorruptCandidateCannotReplaceAuthenticatedPrimary) {
    bytes[secondary_offset + 600] ^= 1;
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    nx_test_mcuboot_selected_t selected = {};
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 0);
    const auto original = image("version-1.0.0.signed.bin");
    EXPECT_TRUE(std::equal(original.begin(), original.end(),
                           bytes.begin() + primary_offset));
}

TEST_F(SwapFlow, ScratchOverlapRejectsBeforePersistentEffects) {
    scratch_region.offset = primary_offset;
    const struct flash_area* area = nullptr;
    EXPECT_EQ(flash_area_open(NX_MCUBOOT_SCRATCH_AREA_ID, &area), -EINVAL);
    EXPECT_EQ(area, nullptr);
    EXPECT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_ERROR_IO);
    EXPECT_EQ(programs, 0U);
    EXPECT_EQ(erases, 0U);
}

TEST_F(SwapFlow, ExpiredAndWithdrawnScopesNeverPublishConfirmationOrWrite) {
    now = operation.deadline;
    bool confirmed = true;
    EXPECT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_ERROR_IO);
    EXPECT_EQ(nx_mcuboot_swap_confirmed(&confirmed), NX_ERROR_IO);
    EXPECT_TRUE(confirmed);
    EXPECT_EQ(programs, 0U);
    EXPECT_EQ(erases, 0U);
    auto next_operation = operation;
    next_operation.deadline = now + 1000;
    active = &next_operation;
    bytes[primary_offset + image_ok_offset] = 0;
    EXPECT_EQ(nx_mcuboot_swap_confirmed(&confirmed), NX_ERROR_STATE);
    EXPECT_TRUE(confirmed);
    active = nullptr;
    EXPECT_EQ(nx_mcuboot_swap_confirmed(&confirmed), NX_ERROR_IO);
    EXPECT_TRUE(confirmed);
}

TEST_F(SwapFlow, PartialPrimaryProgramResumesThroughRealBootState) {
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    cut_program = primary_offset + 1024;
    nx_test_mcuboot_selected_t selected = {};
    if (setjmp(reset) == 0) {
        (void)nx_test_mcuboot_boot(&selected);
        FAIL() << "Expected simulated reset during primary copy";
    }
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
    ASSERT_EQ(nx_mcuboot_swap_confirm(), NX_SUCCESS);
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
}

TEST_F(SwapFlow, PartialSecondaryEraseResumesWithoutLosingOldOrNewImage) {
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    cut_erase = secondary_offset;
    nx_test_mcuboot_selected_t selected = {};
    if (setjmp(reset) == 0) {
        (void)nx_test_mcuboot_boot(&selected);
        FAIL() << "Expected simulated reset during secondary erase";
    }
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 1);
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 0);
}

TEST_F(SwapFlow, ThreeSectorSignedImageResumesThirdSectorPartialProgramming) {
    std::fill(bytes.begin() + secondary_offset,
              bytes.begin() + secondary_offset + slot_bytes, 0xff);
    install(secondary_offset, "version-1.2.0-large.signed.bin");
    ASSERT_EQ(nx_mcuboot_swap_request(NX_MCUBOOT_REQUEST_TEST), NX_SUCCESS);
    cut_program = primary_offset + 2 * 0x20000 + 1024;
    nx_test_mcuboot_selected_t selected = {};
    if (setjmp(reset) == 0) {
        (void)nx_test_mcuboot_boot(&selected);
        FAIL() << "Expected simulated reset during third sector copy";
    }
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 2);
    const auto candidate = image("version-1.2.0-large.signed.bin");
    EXPECT_TRUE(std::equal(candidate.begin(), candidate.end(),
                           bytes.begin() + primary_offset));
    ASSERT_EQ(nx_mcuboot_swap_confirm(), NX_SUCCESS);
    ASSERT_EQ(nx_test_mcuboot_boot(&selected), 0);
    EXPECT_EQ(selected.minor, 2);
}
} /* namespace */

extern "C" const nx_mcuboot_swap_flash_operation_t*
nx_mcuboot_swap_flash_operation(void) {
    return active;
}
extern "C" void nx_mcuboot_assert_fail(void) {
    ADD_FAILURE() << "Unexpected assertion in real MCUboot swap";
    std::abort();
}
extern "C" void nx_mcuboot_watchdog_progress(void) {
}
