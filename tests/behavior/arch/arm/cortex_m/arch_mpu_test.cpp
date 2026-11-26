/**
 * \file            arch_mpu_test.cpp
 *
 * \brief           MPU encoders and production programming at an MMIO boundary.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/mpu.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
constexpr uintptr_t type_address = 0xe000ed90U;
constexpr uintptr_t control_address = type_address + 4U;
constexpr uintptr_t number_address = type_address + 8U;
constexpr uintptr_t base_address = type_address + 12U;
constexpr uintptr_t attributes_address = type_address + 16U;
constexpr uintptr_t mair_address = type_address + 48U;

class Hardware {
  public:
    MOCK_METHOD(uint32_t, Read, (uintptr_t));
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
    MOCK_METHOD(void, Barrier, (char));
};
class Context {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, Masks, ());
};
Hardware* hardware;
Context* context;

nx_arch_mpu_v7_region_t V7() {
    return {0x20000000U, 12U, 3U, 1U, 0x80U, true, false, false, true};
}
nx_arch_mpu_v8_region_t V8() {
    return {0x20000000U, 0x20000fffU, 3U, 3U, 7U, true};
}

class ArchMpu : public ::testing::Test {
  protected:
    StrictMock<Hardware> registers;
    NiceMock<Context> execution;
    void SetUp() override {
        hardware = &registers;
        context = &execution;
        ON_CALL(execution, Privileged()).WillByDefault(Return(true));
        ON_CALL(execution, Exception()).WillByDefault(Return(0U));
        ON_CALL(execution, Masks())
            .WillByDefault(Return(nx_arch_irq_masks_t{1U, 0U, 0U}));
    }
    void TearDown() override {
        hardware = nullptr;
        context = nullptr;
    }
    void ExpectWritableMpu(uint32_t regions = 8U) {
        EXPECT_CALL(registers, Read(type_address))
            .WillOnce(Return(regions << 8U));
        EXPECT_CALL(registers, Read(control_address)).WillOnce(Return(6U));
    }
};

TEST_F(ArchMpu, V7EncodingRetainsFieldsAndDoesNotSelectAnAliasRegion) {
    auto region = V7();
    nx_arch_mpu_v7_encoding_t encoding{};
    ASSERT_EQ(nx_arch_mpu_v7_encode(&region, &encoding), NX_ARCH_OK);
    EXPECT_EQ(encoding.rbar, 0x20000000U);
    EXPECT_EQ(encoding.rasr, 0x130c8017U);
}

TEST_F(ArchMpu, V7FullAddressSpaceIsEncodedWithoutShiftOverflow) {
    auto region = V7();
    region.base = 0U;
    region.size_log2 = 32U;
    nx_arch_mpu_v7_encoding_t encoding{};
    ASSERT_EQ(nx_arch_mpu_v7_encode(&region, &encoding), NX_ARCH_OK);
    EXPECT_EQ(encoding.rbar, 0U);
    EXPECT_EQ(encoding.rasr & 0x3fU, 0x3fU);
}

TEST_F(ArchMpu, V7MinimumRegionUsesNoSubregionMask) {
    auto region = V7();
    region.size_log2 = 5U;
    region.subregion_disable = 0U;
    nx_arch_mpu_v7_encoding_t encoding{};
    ASSERT_EQ(nx_arch_mpu_v7_encode(&region, &encoding), NX_ARCH_OK);
    EXPECT_EQ(encoding.rasr & 0xff3fU, 9U);
}

TEST_F(ArchMpu, V7AcceptsAllDefinedAccessPermissionValues) {
    for (uint32_t access : {0U, 1U, 2U, 3U, 5U, 6U}) {
        auto region = V7();
        region.access = access;
        nx_arch_mpu_v7_encoding_t encoding{};
        ASSERT_EQ(nx_arch_mpu_v7_encode(&region, &encoding), NX_ARCH_OK);
        EXPECT_EQ((encoding.rasr >> 24U) & 7U, access);
    }
}

TEST_F(ArchMpu, V7RejectsInvalidSizeAlignmentAndFieldsWithoutChangingOutput) {
    const auto valid = V7();
    auto invalid = valid;
    nx_arch_mpu_v7_encoding_t encoding{0xaaaaU, 0xbbbbU};
    const auto reject = [&]() {
        EXPECT_EQ(nx_arch_mpu_v7_encode(&invalid, &encoding), NX_ARCH_INVALID);
        EXPECT_EQ(encoding.rbar, 0xaaaaU);
        EXPECT_EQ(encoding.rasr, 0xbbbbU);
    };
    for (uint32_t size : {0U, 4U, 33U, 0xffffffffU}) {
        invalid = valid;
        invalid.size_log2 = size;
        reject();
    }
    invalid = valid;
    invalid.base += 32U;
    reject();
    invalid = valid;
    invalid.size_log2 = 32U;
    reject();
    for (uint32_t access : {4U, 7U, 8U, 0xffffffffU}) {
        invalid = valid;
        invalid.access = access;
        reject();
    }
    invalid = valid;
    invalid.tex = 8U;
    reject();
    invalid = valid;
    invalid.subregion_disable = 256U;
    reject();
    invalid = valid;
    invalid.size_log2 = 7U;
    reject();
}

TEST_F(ArchMpu, V7RejectsReservedAndImplementationDefinedMemoryAttributes) {
    for (uint32_t tex : {1U, 2U, 3U}) {
        for (uint32_t cb = 0; cb < 4U; ++cb) {
            auto region = V7();
            region.tex = tex;
            region.cacheable = (cb & 2U) != 0;
            region.bufferable = (cb & 1U) != 0;
            nx_arch_mpu_v7_encoding_t encoding{};
            const bool valid = (tex == 1U && (cb == 0U || cb == 3U)) ||
                               (tex == 2U && cb == 0U);
            EXPECT_EQ(nx_arch_mpu_v7_encode(&region, &encoding),
                      valid ? NX_ARCH_OK : NX_ARCH_INVALID);
        }
    }
}

TEST_F(ArchMpu, V7AcceptsDefinedOuterAndInnerCacheAttributeFields) {
    for (uint32_t tex : {0U, 4U, 5U, 6U, 7U}) {
        for (uint32_t cb = 0; cb < 4U; ++cb) {
            auto region = V7();
            region.tex = tex;
            region.cacheable = (cb & 2U) != 0;
            region.bufferable = (cb & 1U) != 0;
            nx_arch_mpu_v7_encoding_t encoding{};
            EXPECT_EQ(nx_arch_mpu_v7_encode(&region, &encoding), NX_ARCH_OK);
            EXPECT_EQ((encoding.rasr >> 19U) & 7U, tex);
            EXPECT_EQ((encoding.rasr >> 16U) & 3U, cb);
        }
    }
}

TEST_F(ArchMpu, V7NullInputAndOutputAreRejectedWithoutHardwareAccess) {
    auto region = V7();
    nx_arch_mpu_v7_encoding_t encoding{1U, 2U};
    EXPECT_EQ(nx_arch_mpu_v7_encode(nullptr, &encoding), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_mpu_v7_encode(&region, nullptr), NX_ARCH_INVALID);
    EXPECT_EQ(encoding.rbar, 1U);
    EXPECT_EQ(encoding.rasr, 2U);
}

TEST_F(ArchMpu, V8EncodingRetainsInclusiveLimitAndMairIndirection) {
    auto region = V8();
    nx_arch_mpu_v8_encoding_t encoding{};
    ASSERT_EQ(nx_arch_mpu_v8_encode(&region, &encoding), NX_ARCH_OK);
    EXPECT_EQ(encoding.rbar, 0x2000001fU);
    EXPECT_EQ(encoding.rlar, 0x20000fefU);
}

TEST_F(ArchMpu, V8AcceptsFullAddressSpaceWithoutEndRoundingOverflow) {
    auto region = V8();
    region.base = 0U;
    region.limit = 0xffffffffU;
    nx_arch_mpu_v8_encoding_t encoding{};
    ASSERT_EQ(nx_arch_mpu_v8_encode(&region, &encoding), NX_ARCH_OK);
    EXPECT_EQ(encoding.rlar, 0xffffffefU);
}

TEST_F(ArchMpu, V8AcceptsEveryDefinedAccessAndShareabilityField) {
    for (uint32_t access : {0U, 1U, 2U, 3U}) {
        for (uint32_t share : {0U, 2U, 3U}) {
            auto region = V8();
            region.access = access;
            region.shareability = share;
            region.execute_never = false;
            nx_arch_mpu_v8_encoding_t encoding{};
            ASSERT_EQ(nx_arch_mpu_v8_encode(&region, &encoding), NX_ARCH_OK);
            EXPECT_EQ(encoding.rbar & 0x1fU, (share << 3U) | (access << 1U));
        }
    }
}

TEST_F(ArchMpu, V8RejectsMisalignedReversedAndInvalidFieldsPreservingOutput) {
    const auto valid = V8();
    auto invalid = valid;
    nx_arch_mpu_v8_encoding_t encoding{0xaaaaU, 0xbbbbU};
    const auto reject = [&]() {
        EXPECT_EQ(nx_arch_mpu_v8_encode(&invalid, &encoding), NX_ARCH_INVALID);
        EXPECT_EQ(encoding.rbar, 0xaaaaU);
        EXPECT_EQ(encoding.rlar, 0xbbbbU);
    };
    invalid.base += 1U;
    reject();
    invalid = valid;
    invalid.limit -= 1U;
    reject();
    invalid = valid;
    invalid.base = valid.limit + 1U;
    reject();
    for (uint32_t access : {4U, 0xffffffffU}) {
        invalid = valid;
        invalid.access = access;
        reject();
    }
    for (uint32_t share : {1U, 4U, 0xffffffffU}) {
        invalid = valid;
        invalid.shareability = share;
        reject();
    }
    invalid = valid;
    invalid.attribute_index = 8U;
    reject();
}

TEST_F(ArchMpu, V8NullInputAndOutputAreRejectedWithoutHardwareAccess) {
    auto region = V8();
    nx_arch_mpu_v8_encoding_t encoding{1U, 2U};
    EXPECT_EQ(nx_arch_mpu_v8_encode(nullptr, &encoding), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_mpu_v8_encode(&region, nullptr), NX_ARCH_INVALID);
    EXPECT_EQ(encoding.rbar, 1U);
    EXPECT_EQ(encoding.rlar, 2U);
}

#if NEXUS_ARCH_MPU_VERSION == 7 || NEXUS_ARCH_MPU_VERSION == 8
nx_arch_result_t Program(uint32_t index) {
#if NEXUS_ARCH_MPU_VERSION == 7
    const auto region = V7();
    return nx_arch_mpu_v7_program(index, &region);
#else
    const auto region = V8();
    return nx_arch_mpu_v8_program(index, &region);
#endif
}

TEST_F(ArchMpu, ProgrammingRequiresPrivilegeBeforeAnyMpuRead) {
    ON_CALL(execution, Privileged()).WillByDefault(Return(false));
    EXPECT_EQ(Program(0U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, HandlerCannotProgramMpuEvenWhenPrivilegedAndMasked) {
    ON_CALL(execution, Exception()).WillByDefault(Return(16U));
    EXPECT_EQ(Program(0U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, BasepriAndFaultmaskCannotReplacePrimaskRequirement) {
    ON_CALL(execution, Masks())
        .WillByDefault(Return(nx_arch_irq_masks_t{0U, 0x50U, 1U}));
    EXPECT_EQ(Program(0U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, RegionNumberIsValidatedAgainstActualMpuTypeBeforeWrites) {
    EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(8U << 8U));
    EXPECT_EQ(Program(8U), NX_ARCH_INVALID);
}

TEST_F(ArchMpu, AbsentActualMpuRejectsRegionBeforeControlRead) {
    EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(0U));
    EXPECT_EQ(Program(0U), NX_ARCH_UNSUPPORTED);
}

TEST_F(ArchMpu, EnabledMpuIsNeverSilentlyDisabledOrReprogrammed) {
    EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(8U << 8U));
    EXPECT_CALL(registers, Read(control_address)).WillOnce(Return(7U));
    EXPECT_EQ(Program(0U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, ProgrammingOrdersOnlySelectedRegionAndRestoresRnr) {
    InSequence order;
    ExpectWritableMpu();
    EXPECT_CALL(registers, Read(number_address)).WillOnce(Return(5U));
    EXPECT_CALL(registers, Barrier('M'));
    EXPECT_CALL(registers, Write(number_address, 2U));
    EXPECT_CALL(registers, Write(attributes_address, 0U));
#if NEXUS_ARCH_MPU_VERSION == 7
    EXPECT_CALL(registers, Write(base_address, 0x20000000U));
    EXPECT_CALL(registers, Write(attributes_address, 0x130c8017U));
#else
    EXPECT_CALL(registers, Write(base_address, 0x2000001fU));
    EXPECT_CALL(registers, Write(attributes_address, 0x20000fefU));
#endif
    EXPECT_CALL(registers, Write(number_address, 5U));
    EXPECT_CALL(registers, Barrier('D'));
    EXPECT_CALL(registers, Barrier('I'));
    EXPECT_EQ(Program(2U), NX_ARCH_OK);
}

TEST_F(ArchMpu, InvalidDescriptionIsRejectedBeforeContextAndMpuAccess) {
    EXPECT_CALL(execution, Privileged()).Times(0);
    EXPECT_CALL(execution, Exception()).Times(0);
    EXPECT_CALL(execution, Masks()).Times(0);
#if NEXUS_ARCH_MPU_VERSION == 7
    auto region = V7();
    region.base += 1U;
    EXPECT_EQ(nx_arch_mpu_v7_program(0U, &region), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_mpu_v7_program(0U, nullptr), NX_ARCH_INVALID);
#else
    auto region = V8();
    region.limit -= 1U;
    EXPECT_EQ(nx_arch_mpu_v8_program(0U, &region), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_mpu_v8_program(0U, nullptr), NX_ARCH_INVALID);
#endif
}
#endif

#if NEXUS_ARCH_MPU_VERSION != 7
TEST_F(ArchMpu, UnsupportedV7NeverQueriesContextOrMpuRegisters) {
    EXPECT_CALL(execution, Privileged()).Times(0);
    EXPECT_CALL(execution, Exception()).Times(0);
    EXPECT_CALL(execution, Masks()).Times(0);
    auto region = V7();
    EXPECT_EQ(nx_arch_mpu_v7_program(0U, &region), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_mpu_v7_program(0U, nullptr), NX_ARCH_UNSUPPORTED);
}
#endif
#if NEXUS_ARCH_MPU_VERSION != 8
TEST_F(ArchMpu, UnsupportedV8NeverQueriesContextOrMpuRegisters) {
    EXPECT_CALL(execution, Privileged()).Times(0);
    EXPECT_CALL(execution, Exception()).Times(0);
    EXPECT_CALL(execution, Masks()).Times(0);
    auto region = V8();
    EXPECT_EQ(nx_arch_mpu_v8_program(0U, &region), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_mpu_v8_program(0U, nullptr), NX_ARCH_UNSUPPORTED);
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_UNSUPPORTED);
}
#endif

#if NEXUS_ARCH_MPU_VERSION == 8
TEST_F(ArchMpu, MairUpdatePreservesOtherBytesAndNeverWritesControlOrRnr) {
    InSequence order;
    ExpectWritableMpu();
    EXPECT_CALL(registers, Read(mair_address + 4U))
        .WillOnce(Return(0x12345678U));
    EXPECT_CALL(registers, Barrier('M'));
    EXPECT_CALL(registers, Write(mair_address + 4U, 0x12ff5678U));
    EXPECT_CALL(registers, Barrier('D'));
    EXPECT_CALL(registers, Barrier('I'));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(6U, 0xffU), NX_ARCH_OK);
}

TEST_F(ArchMpu, MairAcceptsOnlyDefinedDeviceAndNormalMemoryEncodings) {
    for (uint32_t attribute : {0U, 4U, 8U, 12U, 0x11U, 0x44U, 0xffU}) {
        EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(8U << 8U));
        EXPECT_CALL(registers, Read(control_address)).WillOnce(Return(0U));
        EXPECT_CALL(registers, Read(mair_address)).WillOnce(Return(0U));
        EXPECT_CALL(registers, Barrier('M'));
        EXPECT_CALL(registers, Write(mair_address, attribute));
        EXPECT_CALL(registers, Barrier('D'));
        EXPECT_CALL(registers, Barrier('I'));
        EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, attribute), NX_ARCH_OK);
    }
}

TEST_F(ArchMpu, MairRejectsReservedOutOfRangeFieldsBeforeHardwareAccess) {
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(8U, 0x44U), NX_ARCH_INVALID);
    for (uint32_t attribute :
         {1U, 2U, 3U, 5U, 0x10U, 0x18U, 0x81U, 0x88U, 256U, 0xffffffffU}) {
        EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, attribute), NX_ARCH_INVALID);
    }
}

TEST_F(ArchMpu, MairRejectsPrivilegeExceptionAndUnmaskedContext) {
    ON_CALL(execution, Privileged()).WillByDefault(Return(false));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_CONTEXT);
    ON_CALL(execution, Privileged()).WillByDefault(Return(true));
    ON_CALL(execution, Exception()).WillByDefault(Return(3U));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_CONTEXT);
    ON_CALL(execution, Exception()).WillByDefault(Return(0U));
    ON_CALL(execution, Masks())
        .WillByDefault(Return(nx_arch_irq_masks_t{0U, 0U, 0U}));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, MairRejectsEnabledMpuWithoutTouchingAttributeRegisters) {
    EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(8U << 8U));
    EXPECT_CALL(registers, Read(control_address)).WillOnce(Return(1U));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_CONTEXT);
}

TEST_F(ArchMpu, MairRejectsAbsentActualMpuBeforeControlOrAttributeAccess) {
    EXPECT_CALL(registers, Read(type_address)).WillOnce(Return(0U));
    EXPECT_EQ(nx_arch_mpu_v8_attribute_set(0U, 0x44U), NX_ARCH_UNSUPPORTED);
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
    return context->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return context->Exception();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return context->Masks();
}
extern "C" void nx_arch_dmb(void) {
    hardware->Barrier('M');
}
extern "C" void nx_arch_dsb(void) {
    hardware->Barrier('D');
}
extern "C" void nx_arch_isb(void) {
    hardware->Barrier('I');
}
