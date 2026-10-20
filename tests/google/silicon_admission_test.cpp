/**
 * \file            silicon_admission_test.cpp
 * \brief           Cold observed-silicon admission and readonly source rules
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/silicon.h"
#include "stm32_identity.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class SiliconSource {
  public:
    MOCK_METHOD(nx_result_t, Read, (nx_silicon_identity_t*));
    static nx_result_t read(const void* context, nx_silicon_identity_t* out) {
        return static_cast<SiliconSource*>(const_cast<void*>(context))
            ->Read(out);
    }
};

class SiliconAdmission : public testing::Test {
  protected:
    testing::StrictMock<SiliconSource> hardware;
    nx_silicon_source_t source{&hardware, SiliconSource::read};
    nx_silicon_revision_rule_t rules[2]{
        {0x1000U, NX_SILICON_REVISION_REVIEWED, 1U},
        {0x2000U, NX_SILICON_REVISION_BLOCKED, 0U}};
    nx_silicon_admission_policy_t policy{};
    nx_silicon_admission_t output{};
    nx_silicon_identity_t observed{0x413U, 0x1000U, 1048576U, true};
    void SetUp() override {
        policy.device_id = 0x413U;
        policy.flash_bytes = 1048576U;
        policy.rules = rules;
        policy.rule_count = 2U;
        policy.applied_workarounds = 1U;
        policy.review_sha256[0] = 1U;
        output.identity.device_id = 777U;
    }
    void expect_read() {
        EXPECT_CALL(hardware, Read(testing::_))
            .WillOnce(testing::DoAll(testing::SetArgPointee<0>(observed),
                                     testing::Return(NX_SUCCESS)));
    }
};

TEST_F(SiliconAdmission, ReadsExactlyOnceAndAdmitsReviewedSoftwarePolicy) {
    expect_read();
    ASSERT_EQ(nx_silicon_preflight(&source, &policy, &output), NX_SUCCESS);
    EXPECT_TRUE(output.admitted);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_REVIEWED);
    EXPECT_EQ(output.identity.device_id, 0x413U);
    EXPECT_EQ(output.identity.revision_id, 0x1000U);
}

TEST_F(SiliconAdmission, UnknownIsRejectedUnlessObservationOnlyWasExplicit) {
    observed.revision_id = 0x3000U;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(output.admitted);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_UNREVIEWED);
    policy.allow_unreviewed = true;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output), NX_SUCCESS);
    EXPECT_TRUE(output.admitted);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_UNREVIEWED);
}

TEST_F(SiliconAdmission, BlockedCannotBeOverriddenByObservationOnly) {
    observed.revision_id = 0x2000U;
    policy.allow_unreviewed = true;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_PERMISSION);
    EXPECT_FALSE(output.admitted);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_BLOCKED);
}

TEST_F(SiliconAdmission, RequiredWorkaroundCannotBeAssumedFromReviewedID) {
    policy.applied_workarounds = 0U;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_PERMISSION);
    EXPECT_FALSE(output.admitted);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_WORKAROUND_MISSING);
}

TEST_F(SiliconAdmission, DeviceAndDensityMismatchAlwaysReject) {
    policy.allow_unreviewed = true;
    observed.device_id = 0x450U;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output), NX_ERROR_IO);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_DEVICE_MISMATCH);
    observed.device_id = policy.device_id;
    observed.flash_bytes = 524288U;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output), NX_ERROR_IO);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_DENSITY_MISMATCH);
}

TEST_F(SiliconAdmission, UnobservedRevisionCannotMatchNumericalZero) {
    observed.revision_known = false;
    observed.revision_id = 0U;
    expect_read();
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(output.revision, NX_SILICON_REVISION_NOT_OBSERVED);
    EXPECT_FALSE(output.admitted);
}

TEST_F(SiliconAdmission, SourceFailurePreservesOutputWithoutPartialIdentity) {
    EXPECT_CALL(hardware, Read(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgPointee<0>(observed),
                                 testing::Return(NX_ERROR_PERMISSION)));
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(output.identity.device_id, 777U);
}

TEST_F(SiliconAdmission, MalformedPolicyRejectsBeforeHardwareRead) {
    EXPECT_EQ(nx_silicon_preflight(nullptr, &policy, &output),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_silicon_preflight(&source, nullptr, &output),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, nullptr),
              NX_ERROR_INVALID);
    policy.review_sha256[0] = 0U;
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_INVALID);
    policy.review_sha256[0] = 1U;
    rules[1].revision_id = rules[0].revision_id;
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_INVALID);
    rules[1].revision_id = 0U;
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, &output),
              NX_ERROR_INVALID);
    EXPECT_EQ(output.identity.device_id, 777U);
}

TEST_F(SiliconAdmission, AliasedOutputRejectsBeforeHardwareRead) {
    auto* alias = reinterpret_cast<nx_silicon_admission_t*>(&policy);
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, alias), NX_ERROR_INVALID);
    alias = reinterpret_cast<nx_silicon_admission_t*>(rules);
    EXPECT_EQ(nx_silicon_preflight(&source, &policy, alias), NX_ERROR_INVALID);
}

TEST(SiliconRegisters, ReadsDocumentedGroupRevisionAndDensityWithoutWrites) {
    volatile uint32_t idcode = UINT32_C(0x10000413);
    volatile uint16_t flash_kib = 1024U;
    nx_stm32_identity_registers_t registers{&idcode, &flash_kib};
    nx_silicon_identity_t output{};
    ASSERT_EQ(nx_stm32_identity_read(&registers, &output), NX_SUCCESS);
    EXPECT_EQ(output.device_id, 0x413U);
    EXPECT_EQ(output.revision_id, 0x1000U);
    EXPECT_EQ(output.flash_bytes, 1048576U);
    EXPECT_TRUE(output.revision_known);
    EXPECT_EQ(idcode, UINT32_C(0x10000413));
    EXPECT_EQ(flash_kib, 1024U);
}

TEST(SiliconRegisters, InvalidDensityAndNullRegisterStoragePreserveOutput) {
    volatile uint32_t idcode = UINT32_C(0x10000413);
    volatile uint16_t flash_kib = UINT16_MAX;
    nx_stm32_identity_registers_t registers{&idcode, &flash_kib};
    nx_silicon_identity_t output{777U, 0U, 0U, false};
    EXPECT_EQ(nx_stm32_identity_read(&registers, &output), NX_ERROR_IO);
    EXPECT_EQ(output.device_id, 777U);
    flash_kib = 0U;
    EXPECT_EQ(nx_stm32_identity_read(&registers, &output), NX_ERROR_IO);
    registers.idcode = nullptr;
    EXPECT_EQ(nx_stm32_identity_read(&registers, &output), NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32_identity_read(nullptr, &output), NX_ERROR_INVALID);
}
} /* namespace */
