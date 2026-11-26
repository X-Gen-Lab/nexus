/**
 * \file            arch_cache_test.cpp
 *
 * \brief           Owned cache-line contracts at mocked MMIO/CPU boundaries.
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
#include "nexus/arch/cache.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arch_mechanism_model.h"
#include <limits>
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
constexpr uintptr_t kCcr = 0xe000ed14U;
constexpr uintptr_t kInstructionInvalidate = 0xe000ef58U;
constexpr uintptr_t kDataInvalidate = 0xe000ef5cU;
constexpr uintptr_t kDataClean = 0xe000ef68U;
constexpr uintptr_t kDataCleanInvalidate = 0xe000ef70U;
constexpr uint32_t kDataEnabled = 1U << 16;
constexpr uint32_t kInstructionEnabled = 1U << 17;
constexpr bool kHasData = NEXUS_ARCH_DCACHE_LINE_BYTES != 0;
constexpr bool kHasInstruction = NEXUS_ARCH_ICACHE_LINE_BYTES != 0;
constexpr uint32_t kConfiguredEnabled =
    (kHasData ? kDataEnabled : 0U) |
    (kHasInstruction ? kInstructionEnabled : 0U);
using Operation = nx_arch_result_t (*)(uintptr_t, size_t);
constexpr Operation kOperations[] = {
    nx_arch_dcache_clean, nx_arch_dcache_invalidate,
    nx_arch_dcache_clean_invalidate, nx_arch_instruction_sync};

class Hardware {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Read, (uintptr_t));
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
    MOCK_METHOD(void, Dsb, ());
    MOCK_METHOD(void, Isb, ());
};
Hardware* hardware;
class ArchCache : public ::testing::Test {
  protected:
    StrictMock<Hardware> registers;
    void SetUp() override {
        hardware = &registers;
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void ExpectEnabled(uint32_t enabled = kConfiguredEnabled) {
        EXPECT_CALL(registers, Privileged()).WillOnce(Return(true));
        EXPECT_CALL(registers, Read(kCcr)).WillOnce(Return(enabled));
    }
    void ExpectLines(uintptr_t operation, uintptr_t address, size_t bytes) {
        EXPECT_CALL(registers, Dsb());
        for (size_t offset = 0; offset < bytes; offset += 32U) {
            EXPECT_CALL(registers, Write(operation, static_cast<uint32_t>(
                                                        address + offset)));
        }
        EXPECT_CALL(registers, Dsb());
        EXPECT_CALL(registers, Isb());
    }
    void ExpectInstructionLines(uintptr_t address, size_t bytes) {
        EXPECT_CALL(registers, Dsb());
        if (kHasData) {
            for (size_t offset = 0; offset < bytes; offset += 32U) {
                EXPECT_CALL(
                    registers,
                    Write(kDataClean, static_cast<uint32_t>(address + offset)));
            }
            if (kHasInstruction) {
                EXPECT_CALL(registers, Dsb());
            }
        }
        if (kHasInstruction) {
            for (size_t offset = 0; offset < bytes; offset += 32U) {
                EXPECT_CALL(registers,
                            Write(kInstructionInvalidate,
                                  static_cast<uint32_t>(address + offset)));
            }
        }
        EXPECT_CALL(registers, Dsb());
        EXPECT_CALL(registers, Isb());
    }
};

TEST_F(ArchCache, EmptyRangeRejectsBeforeReadingHardware) {
    for (Operation operation : kOperations) {
        EXPECT_EQ(operation(0x20000000U, 0U), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, UnalignedStartNeverRoundsIntoNeighborLines) {
    for (Operation operation : kOperations) {
        EXPECT_EQ(operation(0x20000001U, 32U), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, PartialLineLengthRejectsWithoutEffects) {
    for (Operation operation : kOperations) {
        EXPECT_EQ(operation(0x20000000U, 33U), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, WrappingRangeRejectsBeforeHardware) {
    for (Operation operation : kOperations) {
        EXPECT_EQ(operation(0xffffffe0U, 64U), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, HostAddressBeyondCpuSpaceRejects) {
    for (Operation operation : kOperations) {
        const uintptr_t address = static_cast<uintptr_t>(UINT32_MAX) + 1U;
        EXPECT_EQ(operation(address, 32U), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, OversizedHostLengthRejectsBeforeArithmeticOverflow) {
    for (Operation operation : kOperations) {
        const size_t bytes = std::numeric_limits<size_t>::max() - 31U;
        EXPECT_EQ(operation(0x20000000U, bytes), NX_ARCH_INVALID);
    }
}

TEST_F(ArchCache, AbsentDataCachePerformsNoContextOrMmioReads) {
    if (!kHasData) {
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
        EXPECT_EQ(nx_arch_dcache_invalidate(0x20000000U, 32U),
                  NX_ARCH_UNSUPPORTED);
        EXPECT_EQ(nx_arch_dcache_clean_invalidate(0x20000000U, 32U),
                  NX_ARCH_UNSUPPORTED);
    } else {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataClean, 0x20000000U, 32U);
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 32U), NX_ARCH_OK);
    }
}

TEST_F(ArchCache, UnprivilegedDataMaintenanceRejectsBeforeMmio) {
    for (size_t index = 0; index < 3U; ++index) {
        if (kHasData) {
            EXPECT_CALL(registers, Privileged()).WillOnce(Return(false));
            EXPECT_EQ(kOperations[index](0x20000000U, 32U), NX_ARCH_CONTEXT);
        } else {
            EXPECT_EQ(kOperations[index](0x20000000U, 32U),
                      NX_ARCH_UNSUPPORTED);
        }
    }
}

TEST_F(ArchCache, DisabledDataCacheIsNeverEnabledOrMaintainedImplicitly) {
    InSequence order;
    for (size_t index = 0; index < 3U; ++index) {
        if (kHasData) {
            ExpectEnabled(kInstructionEnabled);
        }
        EXPECT_EQ(kOperations[index](0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, CleanPublishesExactlyTwoOwnedLinesInOrder) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataClean, 0x20000000U, 64U);
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 64U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 64U), NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, InvalidateDiscardsWithoutAnyCleanRegisterWrite) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataInvalidate, 0x20000000U, 64U);
        EXPECT_EQ(nx_arch_dcache_invalidate(0x20000000U, 64U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_invalidate(0x20000000U, 64U),
                  NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, CleanInvalidateUsesCombinedMaintenanceRegister) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataCleanInvalidate, 0x20000000U, 64U);
        EXPECT_EQ(nx_arch_dcache_clean_invalidate(0x20000000U, 64U),
                  NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_clean_invalidate(0x20000000U, 64U),
                  NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, FinalCpuLineDoesNotAccessWrappedAddressZero) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataClean, 0xffffffe0U, 32U);
        EXPECT_EQ(nx_arch_dcache_clean(0xffffffe0U, 32U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_clean(0xffffffe0U, 32U), NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, AddressZeroCanRepresentAValidMappedCpuLine) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled();
        ExpectLines(kDataClean, 0U, 32U);
        EXPECT_EQ(nx_arch_dcache_clean(0U, 32U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_clean(0U, 32U), NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, DataMaintenanceDoesNotRequireEnabledInstructionCache) {
    if (kHasData) {
        InSequence order;
        ExpectEnabled(kDataEnabled);
        ExpectLines(kDataClean, 0x20000000U, 32U);
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 32U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_dcache_clean(0x20000000U, 32U), NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, InstructionSyncCleansDataBeforeInstructionInvalidate) {
    if (kHasData || kHasInstruction) {
        InSequence order;
        ExpectEnabled();
        ExpectInstructionLines(0x08000000U, 64U);
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 64U), NX_ARCH_OK);
    } else {
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 64U),
                  NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, InstructionSyncChecksEveryPresentCacheBeforeAnyWrites) {
    if (kHasData || kHasInstruction) {
        InSequence order;
        ExpectEnabled(kHasData ? kInstructionEnabled : kDataEnabled);
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 64U),
                  NX_ARCH_UNSUPPORTED);
        if (kHasData && kHasInstruction) {
            ExpectEnabled(kDataEnabled);
            EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 64U),
                      NX_ARCH_UNSUPPORTED);
        }
    } else {
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 64U),
                  NX_ARCH_UNSUPPORTED);
    }
}

TEST_F(ArchCache, UnprivilegedInstructionSyncDoesNotAccessCaches) {
    if (kHasData || kHasInstruction) {
        EXPECT_CALL(registers, Privileged()).WillOnce(Return(false));
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 32U), NX_ARCH_CONTEXT);
    } else {
        EXPECT_EQ(nx_arch_instruction_sync(0x08000000U, 32U),
                  NX_ARCH_UNSUPPORTED);
    }
}
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return hardware->Privileged();
}
extern "C" void nx_arch_dsb(void) {
    hardware->Dsb();
}
extern "C" void nx_arch_isb(void) {
    hardware->Isb();
}
extern "C" uint32_t nx_arch_model_mmio_read(uintptr_t address) {
    return hardware->Read(address);
}
extern "C" void nx_arch_model_mmio_write(uintptr_t address, uint32_t value) {
    hardware->Write(address, value);
}
