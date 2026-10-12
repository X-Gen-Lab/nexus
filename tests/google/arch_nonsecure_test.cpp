/**
 * \file            arch_nonsecure_test.cpp
 * \brief           NonSecure spans require real external CMSE authorization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_nonsecure_model.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/nonsecure.h"
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
namespace {
class Hardware {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(void*, Range, (void*, size_t, int));
};
Hardware* hardware;
class ArchNonsecure : public ::testing::Test {
  protected:
    StrictMock<Hardware> registers;
    void* const address = reinterpret_cast<void*>(uintptr_t{0x20040000U});
    void SetUp() override {
        hardware = &registers;
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void Context() {
        EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
        EXPECT_CALL(registers, Exception()).WillOnce(Return(0U));
    }
};
#if NEXUS_ARCH_SECURITY_STATE == 1
TEST_F(ArchNonsecure, NullAndEmptySpanRejectBeforeAnyHardwareQuery) {
    EXPECT_EQ(nx_arch_nonsecure_check(nullptr, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_nonsecure_check(address, 0U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
}
TEST_F(ArchNonsecure, InclusiveEndOverflowRejectsBeforeAnyHardwareQuery) {
    void* const end = reinterpret_cast<void*>(UINTPTR_MAX - 2U);
    EXPECT_EQ(nx_arch_nonsecure_check(end, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
}
TEST_F(ArchNonsecure, UnknownAccessCannotGrantReadOrWrite) {
    EXPECT_EQ(nx_arch_nonsecure_test_raw(address, 1U, 2U), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_nonsecure_test_raw(address, 1U, 255U), NX_ARCH_INVALID);
}
TEST_F(ArchNonsecure, UnprivilegedSecureCallerRejectsBeforeCmse) {
    EXPECT_CALL(registers, Privileged()).WillOnce(Return(false));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_CONTEXT);
}
TEST_F(ArchNonsecure, HandlerCallerRejectsBeforeCmse) {
    EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(registers, Exception()).WillOnce(Return(11U));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_CONTEXT);
}
TEST_F(ArchNonsecure, ReadRequiresNonsecureUnprivilegedPermission) {
    InSequence order;
    Context();
    EXPECT_CALL(registers, Range(address, 17U, 30)).WillOnce(Return(address));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 17U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_OK);
}
TEST_F(ArchNonsecure, WriteRequiresNonsecureUnprivilegedReadwritePermission) {
    InSequence order;
    Context();
    EXPECT_CALL(registers, Range(address, 8U, 23)).WillOnce(Return(address));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 8U, NX_ARCH_NONSECURE_READWRITE),
              NX_ARCH_OK);
}
TEST_F(ArchNonsecure, CmsePermissionDenialRejectsWithoutDereferencingInput) {
    Context();
    EXPECT_CALL(registers, Range(address, 4U, 30)).WillOnce(Return(nullptr));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
}
TEST_F(ArchNonsecure, ChangedReturnedAddressCannotAuthorizeDifferentStorage) {
    Context();
    EXPECT_CALL(registers, Range(address, 4U, 30))
        .WillOnce(Return(reinterpret_cast<void*>(uintptr_t{0x20040004U})));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 4U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
}
TEST_F(ArchNonsecure, FinalAddressByteDoesNotOverflowOrGetRounded) {
    void* const last = reinterpret_cast<void*>(UINTPTR_MAX);
    Context();
    EXPECT_CALL(registers, Range(last, 1U, 30)).WillOnce(Return(last));
    EXPECT_EQ(nx_arch_nonsecure_check(last, 1U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_OK);
}
TEST_F(ArchNonsecure, EveryObservationRechecksChangingHardwarePermissions) {
    InSequence order;
    Context();
    EXPECT_CALL(registers, Range(address, 3U, 30)).WillOnce(Return(address));
    Context();
    EXPECT_CALL(registers, Range(address, 3U, 30)).WillOnce(Return(nullptr));
    EXPECT_EQ(nx_arch_nonsecure_check(address, 3U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_OK);
    EXPECT_EQ(nx_arch_nonsecure_check(address, 3U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_INVALID);
}
#else
TEST_F(ArchNonsecure,
       UnsupportedDomainRejectsBeforeAnyHardwareOrPointerAccess) {
    EXPECT_EQ(nx_arch_nonsecure_check(address, 8U, NX_ARCH_NONSECURE_READ),
              NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_nonsecure_check(address, 8U, NX_ARCH_NONSECURE_READWRITE),
              NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_nonsecure_test_raw(nullptr, SIZE_MAX, 255U),
              NX_ARCH_UNSUPPORTED);
}
#endif
} /* namespace */
extern "C" bool nx_arch_is_privileged(void) {
    return hardware->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return hardware->Exception();
}
extern "C" void* nx_arch_model_nonsecure_range(void* address, size_t bytes,
                                               int flags) {
    return hardware->Range(address, bytes, flags);
}
