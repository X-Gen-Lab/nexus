/**
 * \file            arch_security_test.cpp
 *
 * \brief           Local security mechanisms reject cross-domain access.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_mechanism_model.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/security.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
constexpr uintptr_t control = 0xE000EDD0U;
constexpr uintptr_t type = control + 4U;
constexpr uintptr_t selector = control + 8U;
constexpr uintptr_t base = control + 12U;
constexpr uintptr_t limit = control + 16U;
class Hardware {
  public:
    MOCK_METHOD(uint32_t, Read, (uintptr_t));
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, Masks, ());
    MOCK_METHOD(void, Dmb, ());
    MOCK_METHOD(void, Dsb, ());
    MOCK_METHOD(void, Isb, ());
};
Hardware* hardware;
class ArchSecurity : public ::testing::Test {
  protected:
    StrictMock<Hardware> registers;
    const nx_arch_sau_region_t region = {0x20000000U, 0x2000003FU, true};
    void SetUp() override {
        hardware = &registers;
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void Context() {
        EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
        EXPECT_CALL(registers, Exception()).WillOnce(Return(0U));
        EXPECT_CALL(registers, Masks())
            .WillOnce(Return(nx_arch_irq_masks_t{1U, 0U, 0U}));
    }
};

TEST_F(ArchSecurity, FeatureFactsDoNotDiscoverOrTouchHardware) {
    const nx_arch_features_t features = nx_arch_features();
    EXPECT_EQ(features.security_state, NEXUS_ARCH_SECURITY_STATE);
    EXPECT_EQ(features.sau, NEXUS_ARCH_HAS_SAU != 0);
    EXPECT_EQ(features.dcache_line_bytes, NEXUS_ARCH_DCACHE_LINE_BYTES);
    EXPECT_EQ(features.icache_line_bytes, NEXUS_ARCH_ICACHE_LINE_BYTES);
    EXPECT_EQ(features.mpu_version, NEXUS_ARCH_MPU_VERSION);
    EXPECT_EQ(features.dwt_cycle_counter, NEXUS_ARCH_HAS_DWT_CYCCNT != 0);
}

TEST_F(ArchSecurity, CurrentSecurityStateIsCompiledAndReadOnly) {
    EXPECT_EQ(nx_arch_security_state(), NEXUS_ARCH_SECURITY_STATE);
}

TEST_F(ArchSecurity, EncodesOnlyRequestedNonsecureCallableRegion) {
    nx_arch_sau_words_t words = {};
    ASSERT_EQ(nx_arch_sau_encode(&region, &words), NX_ARCH_OK);
    EXPECT_EQ(words.rbar, 0x20000000U);
    EXPECT_EQ(words.rlar, 0x20000023U);
    auto ordinary = region;
    ordinary.nonsecure_callable = false;
    ASSERT_EQ(nx_arch_sau_encode(&ordinary, &words), NX_ARCH_OK);
    EXPECT_EQ(words.rlar, 0x20000021U);
}

TEST_F(ArchSecurity, RejectsPartialBoundaryAndPreservesEncodingOutput) {
    nx_arch_sau_words_t words = {0x1234U, 0x5678U};
    auto invalid = region;
    invalid.base++;
    EXPECT_EQ(nx_arch_sau_encode(&invalid, &words), NX_ARCH_INVALID);
    invalid = region;
    invalid.limit--;
    EXPECT_EQ(nx_arch_sau_encode(&invalid, &words), NX_ARCH_INVALID);
    EXPECT_EQ(words.rbar, 0x1234U);
    EXPECT_EQ(words.rlar, 0x5678U);
}

TEST_F(ArchSecurity, RejectsReversedRegionAndNullEncodingArguments) {
    nx_arch_sau_words_t words = {};
    auto invalid = region;
    invalid.base = 0x20000040U;
    EXPECT_EQ(nx_arch_sau_encode(&invalid, &words), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_sau_encode(nullptr, &words), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_sau_encode(&region, nullptr), NX_ARCH_INVALID);
}

TEST_F(ArchSecurity, EncodesFinalAddressLineWithoutOverflow) {
    const nx_arch_sau_region_t last = {0xFFFFFFE0U, 0xFFFFFFFFU, false};
    nx_arch_sau_words_t words = {};
    EXPECT_EQ(nx_arch_sau_encode(&last, &words), NX_ARCH_OK);
    EXPECT_EQ(words.rbar, 0xFFFFFFE0U);
    EXPECT_EQ(words.rlar, 0xFFFFFFE1U);
}

#if NEXUS_ARCH_HAS_SAU
TEST_F(ArchSecurity, RejectsInvalidRegionBeforeCpuOrMmioAccess) {
    auto invalid = region;
    invalid.limit--;
    EXPECT_EQ(nx_arch_sau_write(0U, &invalid), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_sau_write(0U, nullptr), NX_ARCH_INVALID);
}

TEST_F(ArchSecurity, RejectsUnprivilegedCallBeforeSecureMmioAccess) {
    EXPECT_CALL(registers, Privileged()).WillOnce(Return(false));
    EXPECT_EQ(nx_arch_sau_write(0U, &region), NX_ARCH_CONTEXT);
}

TEST_F(ArchSecurity, RejectsExceptionBeforeSecureMmioAccess) {
    EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(registers, Exception()).WillOnce(Return(3U));
    EXPECT_EQ(nx_arch_sau_write(0U, &region), NX_ARCH_CONTEXT);
}

TEST_F(ArchSecurity, BasepriAloneDoesNotProtectRegionProgramming) {
    EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(registers, Exception()).WillOnce(Return(0U));
    EXPECT_CALL(registers, Masks())
        .WillOnce(Return(nx_arch_irq_masks_t{0U, 0x50U, 0U}));
    EXPECT_EQ(nx_arch_sau_write(0U, &region), NX_ARCH_CONTEXT);
}

TEST_F(ArchSecurity, RejectsActualRegionCountBeforeWrites) {
    Context();
    EXPECT_CALL(registers, Read(type)).WillOnce(Return(4U));
    EXPECT_EQ(nx_arch_sau_write(4U, &region), NX_ARCH_INVALID);
}

TEST_F(ArchSecurity, RejectsEnabledSauBeforeRegionMutation) {
    Context();
    EXPECT_CALL(registers, Read(type)).WillOnce(Return(4U));
    EXPECT_CALL(registers, Read(control)).WillOnce(Return(1U));
    EXPECT_EQ(nx_arch_sau_write(0U, &region), NX_ARCH_CONTEXT);
}

TEST_F(ArchSecurity, ProgramsOneRegionWithoutEnablingOrChangingPolicy) {
    InSequence order;
    Context();
    EXPECT_CALL(registers, Read(type)).WillOnce(Return(4U));
    EXPECT_CALL(registers, Read(control)).WillOnce(Return(2U));
    EXPECT_CALL(registers, Read(selector)).WillOnce(Return(3U));
    EXPECT_CALL(registers, Dmb());
    EXPECT_CALL(registers, Write(selector, 1U));
    EXPECT_CALL(registers, Write(limit, 0U));
    EXPECT_CALL(registers, Write(base, 0x20000000U));
    EXPECT_CALL(registers, Write(limit, 0x20000023U));
    EXPECT_CALL(registers, Write(selector, 3U));
    EXPECT_CALL(registers, Dsb());
    EXPECT_CALL(registers, Isb());
    EXPECT_EQ(nx_arch_sau_write(1U, &region), NX_ARCH_OK);
}

TEST_F(ArchSecurity, ClearsOnlyRequestedRegionAndRestoresSelector) {
    InSequence order;
    Context();
    EXPECT_CALL(registers, Read(type)).WillOnce(Return(4U));
    EXPECT_CALL(registers, Read(control)).WillOnce(Return(0U));
    EXPECT_CALL(registers, Read(selector)).WillOnce(Return(3U));
    EXPECT_CALL(registers, Dmb());
    EXPECT_CALL(registers, Write(selector, 1U));
    EXPECT_CALL(registers, Write(limit, 0U));
    EXPECT_CALL(registers, Write(selector, 3U));
    EXPECT_CALL(registers, Dsb());
    EXPECT_CALL(registers, Isb());
    EXPECT_EQ(nx_arch_sau_clear(1U), NX_ARCH_OK);
}
#else
TEST_F(ArchSecurity, UnsupportedImageCannotReadSecureRegisters) {
    EXPECT_EQ(nx_arch_sau_write(0U, &region), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_sau_clear(0U), NX_ARCH_UNSUPPORTED);
}
#endif
} /* namespace */
extern "C" uint32_t nx_arch_model_mmio_read(uintptr_t address) {
    return hardware->Read(address);
}
extern "C" void nx_arch_model_mmio_write(uintptr_t address, uint32_t value) {
    hardware->Write(address, value);
}
extern "C" bool nx_arch_is_privileged(void) {
    return hardware->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return hardware->Exception();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return hardware->Masks();
}
extern "C" void nx_arch_dmb(void) {
    hardware->Dmb();
}
extern "C" void nx_arch_dsb(void) {
    hardware->Dsb();
}
extern "C" void nx_arch_isb(void) {
    hardware->Isb();
}
