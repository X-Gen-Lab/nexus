/**
 * \file            arch_cache_control_test.cpp
 * \brief           Cold cache ownership and failure contracts on real C code
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_cache_control_model.h"
#include "arch_mechanism_model.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/cache_control.h"
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace {
constexpr uintptr_t kCpuid = 0xe000ed00U;
constexpr uintptr_t kCcr = 0xe000ed14U;
constexpr uintptr_t kClidr = 0xe000ed78U;
constexpr uintptr_t kCcsidr = 0xe000ed80U;
constexpr uintptr_t kSelector = 0xe000ed84U;
constexpr uintptr_t kInstructionInvalidate = 0xe000ef50U;
constexpr uintptr_t kDataInvalidate = 0xe000ef60U;
constexpr uint32_t kData = 1U << 16;
constexpr uint32_t kInstruction = 1U << 17;
constexpr uint32_t kOtherCcr = (1U << 9) | (1U << 4);
constexpr uint32_t kBoth = NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION;
constexpr uint32_t kDataGeometry = (31U << 13) | (3U << 3) | 1U;
constexpr uint32_t kInstructionGeometry = (63U << 13) | (1U << 3) | 1U;
constexpr bool kSupported = NEXUS_ARCH_CACHE_CONTROL_M7 != 0;
using Write = std::pair<uintptr_t, uint32_t>;
using Event = std::tuple<char, uintptr_t, uint32_t>;
class Hardware {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, Masks, ());
    MOCK_METHOD(uint32_t, Read, (uintptr_t));
    MOCK_METHOD(void, Write, (uintptr_t, uint32_t));
    MOCK_METHOD(void, Dsb, ());
    MOCK_METHOD(void, Isb, ());
    MOCK_METHOD(uint32_t, CleanDisable, (uint32_t, uint32_t));
};
Hardware* hardware;
class ArchCacheControl : public ::testing::Test {
  protected:
    NiceMock<Hardware> cpu;
    nx_arch_cache_control_t control{};
    std::map<uintptr_t, uint32_t> registers;
    std::vector<::Write> writes;
    std::vector<uintptr_t> reads;
    std::vector<Event> events;
    std::vector<uint32_t> leaf_sets;
    uint32_t data_geometry = kDataGeometry;
    uint32_t instruction_geometry = kInstructionGeometry;
    uintptr_t blocked_register = 0U;
    uint32_t blocked_value = 0U;
    bool block_leaf = false;
    void SetUp() override {
        hardware = &cpu;
        registers = {{kCpuid, 0x410fc271U},
                     {kClidr, 0x09000003U},
                     {kCcr, kOtherCcr},
                     {kSelector, 1U}};
        ON_CALL(cpu, Privileged()).WillByDefault(Return(true));
        ON_CALL(cpu, Exception()).WillByDefault(Return(0U));
        ON_CALL(cpu, Masks())
            .WillByDefault(Return(nx_arch_irq_masks_t{1U, 0U, 0U}));
        ON_CALL(cpu, Read(_)).WillByDefault(Invoke([this](uintptr_t address) {
            reads.push_back(address);
            events.emplace_back('r', address, 0U);
            if (address == kCcsidr) {
                return registers[kSelector] == 0U ? data_geometry
                                                  : instruction_geometry;
            }
            return registers[address];
        }));
        ON_CALL(cpu, Write(_, _))
            .WillByDefault(Invoke([this](uintptr_t address, uint32_t value) {
                writes.emplace_back(address, value);
                events.emplace_back('w', address, value);
                if (address != blocked_register || value != blocked_value) {
                    registers[address] = value;
                }
            }));
        ON_CALL(cpu, Dsb()).WillByDefault(Invoke([this] {
            events.emplace_back('d', 0U, 0U);
        }));
        ON_CALL(cpu, Isb()).WillByDefault(Invoke([this] {
            events.emplace_back('i', 0U, 0U);
        }));
        ON_CALL(cpu, CleanDisable(_, _))
            .WillByDefault(Invoke([this](uint32_t last_set, uint32_t ccr) {
                leaf_sets.push_back(last_set);
                events.emplace_back('l', last_set, ccr);
                if (!block_leaf) {
                    registers[kCcr] = ccr;
                }
                return registers[kCcr];
            }));
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void ExpectEnable(uint32_t mask = kBoth) {
        EXPECT_EQ(nx_arch_cache_cold_enable(&control, mask),
                  kSupported ? NX_ARCH_CACHE_OK : NX_ARCH_CACHE_UNSUPPORTED);
    }
    void ExpectDisable() {
        EXPECT_EQ(nx_arch_cache_cold_disable(&control),
                  kSupported ? NX_ARCH_CACHE_OK : NX_ARCH_CACHE_UNSUPPORTED);
    }
    void ExpectNoHardware() {
        EXPECT_CALL(cpu, Read(_)).Times(0);
        EXPECT_CALL(cpu, Write(_, _)).Times(0);
        EXPECT_CALL(cpu, CleanDisable(_, _)).Times(0);
    }
    void ExpectRejectedGeometry() {
        EXPECT_CALL(cpu, CleanDisable(_, _)).Times(0);
        EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
                  NX_ARCH_CACHE_UNSUPPORTED);
        EXPECT_EQ(control.owned_mask, 0U);
        EXPECT_FALSE(control.selector_pending);
        EXPECT_EQ(registers[kSelector], 1U);
        for (const auto& write : writes)
            EXPECT_EQ(write.first, kSelector);
    }
};

TEST_F(ArchCacheControl, NullControlRejectsWithoutHardware) {
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(nullptr, kBoth), NX_ARCH_CACHE_INVALID);
    EXPECT_EQ(nx_arch_cache_cold_disable(nullptr), NX_ARCH_CACHE_INVALID);
}

TEST_F(ArchCacheControl, EmptyAndUnknownMasksRejectBeforeHardware) {
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, 0U), NX_ARCH_CACHE_INVALID);
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, 4U), NX_ARCH_CACHE_INVALID);
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, 7U), NX_ARCH_CACHE_INVALID);
}

TEST_F(ArchCacheControl, UnreviewedProfileNeverProbesHardware) {
    if (!kSupported) {
        ExpectNoHardware();
        EXPECT_CALL(cpu, Privileged()).Times(0);
    }
    ExpectEnable();
}

TEST_F(ArchCacheControl, LiveControlNeverReentersColdAdmission) {
    control.owned_mask = NX_ARCH_CACHE_DATA;
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_STATE : NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_EQ(control.owned_mask, NX_ARCH_CACHE_DATA);
}

TEST_F(ArchCacheControl, PendingSelectorCannotBeOverwrittenByNewAdmission) {
    control.selector_pending = true;
    control.saved_selector = 7U;
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_STATE : NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_EQ(control.saved_selector, 7U);
}

TEST_F(ArchCacheControl, InvalidOwnedBitsAreNotInterpretedAsHardwareMasks) {
    control.owned_mask = 0x10000U;
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_INVALID);
}

TEST_F(ArchCacheControl, UnprivilegedCallerCannotReadCacheRegisters) {
    ON_CALL(cpu, Privileged()).WillByDefault(Return(false));
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_CONTEXT : NX_ARCH_CACHE_UNSUPPORTED);
}

TEST_F(ArchCacheControl, HandlerCallerCannotChangeGlobalCacheState) {
    ON_CALL(cpu, Exception()).WillByDefault(Return(16U));
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_CONTEXT : NX_ARCH_CACHE_UNSUPPORTED);
}

TEST_F(ArchCacheControl, EveryUnsafeMaskRejectsWithoutRegisterAccess) {
    for (nx_arch_irq_masks_t masks :
         {nx_arch_irq_masks_t{0U, 0U, 0U}, {1U, 16U, 0U}, {1U, 0U, 1U}}) {
        ON_CALL(cpu, Masks()).WillByDefault(Return(masks));
        EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
                  kSupported ? NX_ARCH_CACHE_CONTEXT
                             : NX_ARCH_CACHE_UNSUPPORTED);
    }
    EXPECT_TRUE(reads.empty());
    EXPECT_TRUE(writes.empty());
}

TEST_F(ArchCacheControl, EnabledForeignCacheIsNeverAdoptedOrMaintained) {
    registers[kCcr] |= kInstruction;
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, NX_ARCH_CACHE_DATA),
              kSupported ? NX_ARCH_CACHE_STATE : NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(control.owned_mask, 0U);
}

TEST_F(ArchCacheControl, OtherCpuAndMultiLevelCacheRejectBeforeMaintenance) {
    registers[kCpuid] = 0x410fd213U;
    ExpectRejectedGeometry();
    registers[kCpuid] = 0x410fc271U;
    registers[kClidr] |= 1U << 3;
    ExpectRejectedGeometry();
}

TEST_F(ArchCacheControl, BadDataGeometryNeverTruncatesIntoSetWayFields) {
    for (uint32_t geometry :
         {kDataGeometry ^ 3U, kDataGeometry ^ (1U << 3),
          (32U << 13) | (3U << 3) | 1U, (15U << 13) | (3U << 3) | 1U,
          (1023U << 13) | (3U << 3) | 1U}) {
        data_geometry = geometry;
        ExpectRejectedGeometry();
    }
}

TEST_F(ArchCacheControl, BadInstructionGeometryRejectsBeforeDataEnable) {
    for (uint32_t geometry :
         {kInstructionGeometry ^ 3U, kInstructionGeometry ^ (2U << 3),
          (64U << 13) | (1U << 3) | 1U, (31U << 13) | (1U << 3) | 1U,
          (2047U << 13) | (1U << 3) | 1U}) {
        instruction_geometry = geometry;
        ExpectRejectedGeometry();
    }
}

TEST_F(ArchCacheControl, EnableUsesEverySetAndWayAndPreservesOtherCcrBits) {
    ExpectEnable();
    if (!kSupported)
        return;
    std::vector<::Write> expected = {
        {kSelector, 0U}, {kSelector, 1U}, {kSelector, 1U}};
    for (uint32_t set = 32U; set-- != 0U;) {
        for (uint32_t way = 4U; way-- != 0U;) {
            expected.emplace_back(kDataInvalidate, (set << 5) | (way << 30));
        }
    }
    expected.emplace_back(kCcr, kOtherCcr | kData);
    expected.emplace_back(kInstructionInvalidate, 0U);
    expected.emplace_back(kCcr, kOtherCcr | kData | kInstruction);
    EXPECT_EQ(writes, expected);
    EXPECT_EQ(registers[kSelector], 1U);
    EXPECT_EQ(registers[kCcr], kOtherCcr | kData | kInstruction);
    EXPECT_EQ(control.owned_mask, kBoth);
    EXPECT_FALSE(control.selector_pending);
    EXPECT_TRUE(leaf_sets.empty());
}

TEST_F(ArchCacheControl, InstructionOnlyNeverMaintainsDataCache) {
    ExpectEnable(NX_ARCH_CACHE_INSTRUCTION);
    if (kSupported) {
        EXPECT_EQ(registers[kCcr], kOtherCcr | kInstruction);
        EXPECT_EQ(control.owned_mask, NX_ARCH_CACHE_INSTRUCTION);
        ExpectDisable();
        EXPECT_EQ(registers[kCcr], kOtherCcr);
        EXPECT_TRUE(leaf_sets.empty());
    }
    for (const auto& write : writes)
        EXPECT_NE(write.first, kDataInvalidate);
}

TEST_F(ArchCacheControl, DataOnlyNeverInvalidatesInstructionCache) {
    ExpectEnable(NX_ARCH_CACHE_DATA);
    if (kSupported) {
        EXPECT_EQ(registers[kCcr], kOtherCcr | kData);
        ExpectDisable();
        EXPECT_EQ(leaf_sets, std::vector<uint32_t>{31U << 5});
    }
    for (const auto& write : writes)
        EXPECT_NE(write.first, kInstructionInvalidate);
}

TEST_F(ArchCacheControl, LargestReviewedGeometryIsBoundedAndAccepted) {
    data_geometry = (511U << 13) | (3U << 3) | 1U;
    instruction_geometry = (1023U << 13) | (1U << 3) | 1U;
    ExpectEnable();
    if (kSupported) {
        size_t data_writes = 0U;
        for (const auto& write : writes)
            data_writes += write.first == kDataInvalidate ? 1U : 0U;
        EXPECT_EQ(data_writes, 2048U);
        ExpectDisable();
        EXPECT_EQ(leaf_sets, std::vector<uint32_t>{511U << 5});
    }
}

TEST_F(ArchCacheControl, EnableReadbackFailureRetainsResponsibilityForRetry) {
    blocked_register = kCcr;
    blocked_value = kOtherCcr | kData | kInstruction;
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_IO : NX_ARCH_CACHE_UNSUPPORTED);
    if (kSupported) {
        EXPECT_EQ(registers[kCcr], kOtherCcr | kData);
        EXPECT_EQ(control.owned_mask, kBoth);
        blocked_register = 0U;
        ExpectDisable();
        EXPECT_EQ(control.owned_mask, 0U);
        EXPECT_EQ(registers[kCcr], kOtherCcr);
    }
}

TEST_F(ArchCacheControl, DataEnableFailureDoesNotEnableInstructionCache) {
    blocked_register = kCcr;
    blocked_value = kOtherCcr | kData;
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, kBoth),
              kSupported ? NX_ARCH_CACHE_IO : NX_ARCH_CACHE_UNSUPPORTED);
    if (kSupported) {
        EXPECT_EQ(control.owned_mask, kBoth);
        blocked_register = 0U;
        ExpectDisable();
        EXPECT_EQ(control.owned_mask, 0U);
        EXPECT_TRUE(leaf_sets.empty());
    }
    for (const auto& write : writes)
        EXPECT_NE(write.first, kInstructionInvalidate);
}

TEST_F(ArchCacheControl, FailedSelectorRestoreKeepsOriginalForRetry) {
    blocked_register = kSelector;
    blocked_value = 1U;
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, NX_ARCH_CACHE_DATA),
              kSupported ? NX_ARCH_CACHE_IO : NX_ARCH_CACHE_UNSUPPORTED);
    if (kSupported) {
        EXPECT_EQ(control.owned_mask, NX_ARCH_CACHE_DATA);
        EXPECT_TRUE(control.selector_pending);
        EXPECT_EQ(control.saved_selector, 1U);
        EXPECT_EQ(registers[kSelector], 0U);
        EXPECT_EQ(registers[kCcr], kOtherCcr);
        blocked_register = 0U;
        ExpectDisable();
        EXPECT_EQ(registers[kSelector], 1U);
        EXPECT_EQ(control.owned_mask, 0U);
        EXPECT_FALSE(control.selector_pending);
    }
}

TEST_F(ArchCacheControl, FailedSelectionNeverUsesStaleGeometry) {
    blocked_register = kSelector;
    blocked_value = 0U;
    EXPECT_EQ(nx_arch_cache_cold_enable(&control, NX_ARCH_CACHE_DATA),
              kSupported ? NX_ARCH_CACHE_IO : NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_EQ(control.owned_mask, 0U);
    EXPECT_FALSE(control.selector_pending);
    EXPECT_EQ(std::count(reads.begin(), reads.end(), kCcsidr), 0);
}

TEST_F(ArchCacheControl, DisableUsesStackFreeLeafAndClearsOnlyOwnedCaches) {
    ExpectEnable();
    if (!kSupported)
        return;
    writes.clear();
    EXPECT_CALL(cpu, CleanDisable(31U << 5, kOtherCcr)).Times(1);
    ExpectDisable();
    EXPECT_EQ(control.owned_mask, 0U);
    EXPECT_FALSE(control.selector_pending);
    EXPECT_EQ(registers[kCcr], kOtherCcr);
    EXPECT_EQ(registers[kSelector], 1U);
}

TEST_F(ArchCacheControl, DisableFailureDoesNotForgetOwnedCache) {
    ExpectEnable();
    if (!kSupported)
        return;
    block_leaf = true;
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_IO);
    EXPECT_EQ(control.owned_mask, kBoth);
    EXPECT_EQ(registers[kCcr], kOtherCcr | kData | kInstruction);
    block_leaf = false;
    ExpectDisable();
    EXPECT_EQ(control.owned_mask, 0U);
}

TEST_F(ArchCacheControl, ForeignEnabledBitPreventsAnyDisableWrites) {
    ExpectEnable(NX_ARCH_CACHE_DATA);
    if (!kSupported)
        return;
    registers[kCcr] |= kInstruction;
    writes.clear();
    EXPECT_CALL(cpu, CleanDisable(_, _)).Times(0);
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_STATE);
    EXPECT_EQ(control.owned_mask, NX_ARCH_CACHE_DATA);
    EXPECT_TRUE(writes.empty());
}

TEST_F(ArchCacheControl, ChangedGeometryDoesNotDiscardOwnedDirtyLines) {
    ExpectEnable();
    if (!kSupported)
        return;
    data_geometry = (1023U << 13) | (3U << 3) | 1U;
    EXPECT_CALL(cpu, CleanDisable(_, _)).Times(0);
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_EQ(control.owned_mask, kBoth);
    EXPECT_EQ(registers[kCcr], kOtherCcr | kData | kInstruction);
}

TEST_F(ArchCacheControl, EmptyDisableIsIdempotentWithoutMmio) {
    ExpectNoHardware();
    ExpectDisable();
    ExpectDisable();
}

TEST_F(ArchCacheControl, DisableContextFailureRetainsLiveState) {
    control.owned_mask = kBoth;
    ON_CALL(cpu, Privileged()).WillByDefault(Return(false));
    ExpectNoHardware();
    EXPECT_EQ(nx_arch_cache_cold_disable(&control),
              kSupported ? NX_ARCH_CACHE_CONTEXT : NX_ARCH_CACHE_UNSUPPORTED);
    EXPECT_EQ(control.owned_mask, kBoth);
}

TEST_F(ArchCacheControl, SecondControlCannotAdoptFirstControlsCache) {
    ExpectEnable();
    if (!kSupported)
        return;
    nx_arch_cache_control_t another{};
    writes.clear();
    EXPECT_EQ(nx_arch_cache_cold_enable(&another, kBoth), NX_ARCH_CACHE_STATE);
    EXPECT_EQ(another.owned_mask, 0U);
    EXPECT_EQ(control.owned_mask, kBoth);
    EXPECT_TRUE(writes.empty());
}

TEST_F(ArchCacheControl, CacheEnableBarriersOrderMaintenanceAndReadback) {
    ExpectEnable();
    if (!kSupported)
        return;
    for (uint32_t value :
         {kOtherCcr | kData, kOtherCcr | kData | kInstruction}) {
        auto position =
            std::find(events.begin(), events.end(), Event{'w', kCcr, value});
        ASSERT_NE(position, events.end());
        ASSERT_GE(events.end() - position, 4);
        EXPECT_EQ(position[1], (Event{'d', 0U, 0U}));
        EXPECT_EQ(position[2], (Event{'i', 0U, 0U}));
        EXPECT_EQ(position[3], (Event{'r', kCcr, 0U}));
    }
    auto data = std::find(events.begin(), events.end(),
                          Event{'w', kCcr, kOtherCcr | kData});
    ASSERT_NE(data, events.begin());
    EXPECT_EQ(data[-1], (Event{'d', 0U, 0U}));
    auto instruction = std::find(events.begin(), events.end(),
                                 Event{'w', kInstructionInvalidate, 0U});
    ASSERT_NE(instruction, events.end());
    ASSERT_GE(instruction - events.begin(), 2);
    ASSERT_GE(events.end() - instruction, 3);
    EXPECT_EQ(instruction[-2], (Event{'d', 0U, 0U}));
    EXPECT_EQ(instruction[-1], (Event{'i', 0U, 0U}));
    EXPECT_EQ(instruction[1], (Event{'d', 0U, 0U}));
    EXPECT_EQ(instruction[2], (Event{'i', 0U, 0U}));
}

TEST_F(ArchCacheControl, DisableRestoresItsOwnIncomingSelector) {
    ExpectEnable();
    if (!kSupported)
        return;
    registers[kSelector] = 0U;
    ExpectDisable();
    EXPECT_EQ(registers[kSelector], 0U);
    EXPECT_EQ(control.saved_selector, 0U);
}

TEST_F(ArchCacheControl, DisableSelectorFailureRetainsOriginalAndRetries) {
    ExpectEnable();
    if (!kSupported)
        return;
    registers[kSelector] = 0U;
    blocked_register = kSelector;
    blocked_value = 0U;
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_IO);
    EXPECT_EQ(control.owned_mask, kBoth);
    EXPECT_TRUE(control.selector_pending);
    EXPECT_EQ(control.saved_selector, 0U);
    EXPECT_EQ(registers[kCcr], kOtherCcr | kData | kInstruction);
    EXPECT_TRUE(leaf_sets.empty());
    blocked_register = 0U;
    ExpectDisable();
    EXPECT_EQ(registers[kSelector], 0U);
    EXPECT_EQ(control.owned_mask, 0U);
    EXPECT_FALSE(control.selector_pending);
}

TEST_F(ArchCacheControl, InstructionDisableFailureCannotDiscardOwnership) {
    ExpectEnable(NX_ARCH_CACHE_INSTRUCTION);
    if (!kSupported)
        return;
    writes.clear();
    blocked_register = kCcr;
    blocked_value = kOtherCcr;
    EXPECT_EQ(nx_arch_cache_cold_disable(&control), NX_ARCH_CACHE_IO);
    EXPECT_EQ(control.owned_mask, NX_ARCH_CACHE_INSTRUCTION);
    EXPECT_EQ(registers[kCcr], kOtherCcr | kInstruction);
    for (const auto& write : writes)
        EXPECT_NE(write.first, kInstructionInvalidate);
    blocked_register = 0U;
    ExpectDisable();
    EXPECT_EQ(registers[kCcr], kOtherCcr);
    EXPECT_EQ(control.owned_mask, 0U);
}
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return hardware->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return hardware->Exception();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return hardware->Masks();
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
extern "C" uint32_t nx_arch_cache_control_model_clean_disable(uint32_t last_set,
                                                              uint32_t ccr) {
    return hardware->CleanDisable(last_set, ccr);
}
