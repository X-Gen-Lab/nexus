/**
 * \file            stm32f407_handoff_test.cpp
 * \brief           Real F407 handoff admission against register/instruction
 * mocks \author          Nexus Team \version         1.0.0 \date 2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/components/stm32f407_handoff.h"
#include "stm32f407_handoff_model.h"
#include "stm32f407_system.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <tuple>
#include <vector>
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
namespace {
constexpr uint32_t kFlash = 0x08000000U;
constexpr uint32_t kVector = kFlash + 0x4000U;
constexpr uint32_t kFlashBytes = NX_F407_HANDOFF_FLASH_BYTES;
constexpr uint32_t kStack = 0x20020000U;
constexpr uint32_t kPc = kVector + 0x201U;
constexpr uintptr_t kRcc = 0x40023800U;
constexpr uintptr_t kFlashRegisters = 0x40023c00U;
constexpr uintptr_t kCpuid = 0xe000ed00U;
constexpr uintptr_t kIcsr = 0xe000ed04U;
constexpr uintptr_t kScr = 0xe000ed10U;
constexpr uintptr_t kCcr = 0xe000ed14U;
constexpr uintptr_t kCpacr = 0xe000ed88U;
constexpr uintptr_t kMpu = 0xe000ed94U;
constexpr uintptr_t kFpccr = 0xe000ef34U;
constexpr uintptr_t kSystick = 0xe000e010U;
constexpr uintptr_t kDebugControl = 0xe000edf0U;
constexpr uintptr_t kDemcr = 0xe000edfcU;
constexpr uintptr_t kDebugIdentity = 0xe0042000U;
constexpr uintptr_t kFlashDensity = 0x1fff7a22U;
struct Transfer {};
class Hardware {
  public:
    MOCK_METHOD(uint32_t, Read, (uintptr_t));
    MOCK_METHOD(uint32_t, Read16, (uintptr_t));
    MOCK_METHOD(uint32_t, Control, ());
    MOCK_METHOD(void, Tail, (uint32_t, uint32_t, uint32_t));
};
Hardware* hardware;
uint32_t exception_number;
bool privileged;
nx_arch_irq_masks_t masks;
class F407Handoff : public ::testing::Test {
  protected:
    NiceMock<Hardware> cpu;
    std::map<uintptr_t, uint32_t> registers;
    std::vector<uintptr_t> reads;
    nx_clock_barrier_t barrier{};
    uint32_t control = 0U;
    void SetUp() override {
        hardware = &cpu;
        exception_number = 0U;
        privileged = true;
        masks = {1U, 0U, 0U};
        g_nx_stm32_system = {};
        static bool initialized = false;
        if (!initialized) {
            ASSERT_EQ(nx_stm32f407_handoff_initialize(3U), NX_SUCCESS);
            initialized = true;
        }
        ASSERT_EQ(
            nx_clock_domain_close(nx_stm32f407_handoff_domain(), &barrier),
            NX_SUCCESS);
        registers = {{kCpuid, 0x410fc241U},
                     {kRcc, 0x00000083U},
                     {kFlashRegisters + 0x10U, 0x80000000U},
                     {kDebugIdentity, 0x10000413U},
                     {kVector, kStack},
                     {kVector + 4U, kPc}};
        ON_CALL(cpu, Read(_)).WillByDefault(Invoke([this](uintptr_t address) {
            reads.push_back(address);
            uint32_t value = registers[address];
            if (address == kSystick) {
                registers[address] &= ~(1U << 16U);
            }
            return value;
        }));
        ON_CALL(cpu, Read16(kFlashDensity))
            .WillByDefault(Return(kFlashBytes / 1024U));
        ON_CALL(cpu, Control()).WillByDefault(Invoke([this] {
            return control;
        }));
        ON_CALL(cpu, Tail(_, _, _))
            .WillByDefault(
                Invoke([](uint32_t, uint32_t, uint32_t) { throw Transfer{}; }));
    }
    void TearDown() override {
        EXPECT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
        EXPECT_EQ(masks.primask, 1U);
    }
    nx_result_t Run(uint32_t vector = kVector) {
        return nx_stm32f407_handoff(barrier, vector);
    }
    void Rejected(nx_result_t result) {
        EXPECT_CALL(cpu, Tail(_, _, _)).Times(0);
        EXPECT_EQ(Run(), result);
    }
};

TEST_F(F407Handoff, TransfersExactVectorStackAndThumbPcWithoutReturning) {
    EXPECT_CALL(cpu, Tail(kVector, kStack, kPc)).Times(1);
    EXPECT_THROW(Run(), Transfer);
}
TEST_F(F407Handoff, OrdinarySystemInitFpuPermissionWithNoFpOwnerIsSupported) {
    registers[kCpacr] = 0x00f00000U;
    registers[kFpccr] = (1U << 30U) | (1U << 31U);
    EXPECT_THROW(Run(), Transfer);
}
TEST_F(F407Handoff, InitializationNeverRecyclesEpochAndGetterIsStable) {
    auto* domain = nx_stm32f407_handoff_domain();
    EXPECT_EQ(domain, nx_stm32f407_handoff_domain());
    EXPECT_EQ(nx_stm32f407_handoff_initialize(3U), NX_ERROR_STATE);
    EXPECT_EQ(nx_clock_domain_quiescent(barrier), NX_SUCCESS);
}
TEST_F(F407Handoff, RejectsOtherOrStaleDomainWithoutHardwareReads) {
    EXPECT_EQ(nx_stm32f407_handoff({}, kVector), NX_ERROR_INVALID);
    nx_clock_domain_t unrelated{};
    nx_clock_barrier_t unrelated_barrier{};
    ASSERT_EQ(nx_clock_domain_construct(&unrelated, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&unrelated, &unrelated_barrier),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32f407_handoff(unrelated_barrier, kVector),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_clock_domain_reopen(unrelated_barrier), NX_SUCCESS);
    auto stale = barrier;
    --stale.epoch;
    EXPECT_EQ(nx_stm32f407_handoff(stale, kVector), NX_ERROR_STATE);
    EXPECT_TRUE(reads.empty());
}
TEST_F(F407Handoff, RetainedExactOwnerBlocksTransferUntilActualRelease) {
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    nx_clock_lease_t lease{};
    ASSERT_EQ(nx_clock_domain_acquire(nx_stm32f407_handoff_domain(), &lease),
              NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(nx_stm32f407_handoff_domain(), &barrier),
              NX_SUCCESS);
    EXPECT_EQ(Run(), NX_ERROR_BUSY);
    EXPECT_TRUE(reads.empty());
    ASSERT_EQ(nx_clock_domain_release(nx_stm32f407_handoff_domain(), &lease),
              NX_SUCCESS);
    EXPECT_THROW(Run(), Transfer);
}
TEST_F(F407Handoff, WrongContextAndMasksArePreservedBeforeHardwareReads) {
    exception_number = 15U;
    EXPECT_EQ(Run(), NX_ERROR_CONTEXT);
    exception_number = 0U;
    privileged = false;
    EXPECT_EQ(Run(), NX_ERROR_CONTEXT);
    privileged = true;
    for (auto invalid :
         {nx_arch_irq_masks_t{0U, 0U, 0U}, nx_arch_irq_masks_t{1U, 16U, 0U},
          nx_arch_irq_masks_t{1U, 0U, 1U}}) {
        masks = invalid;
        EXPECT_EQ(Run(), NX_ERROR_CONTEXT);
        EXPECT_EQ(masks.primask, invalid.primask);
        EXPECT_EQ(masks.basepri, invalid.basepri);
        EXPECT_EQ(masks.faultmask, invalid.faultmask);
    }
    masks = {1U, 0U, 0U};
    EXPECT_TRUE(reads.empty());
}
TEST_F(F407Handoff, RejectsThreadPspPrivilegeAndFpOwnership) {
    for (uint32_t invalid : {1U, 2U, 4U}) {
        control = invalid;
        EXPECT_EQ(Run(), NX_ERROR_CONTEXT);
    }
}
TEST_F(F407Handoff, LiveSystemAndRetainedFailureCannotBeAbandoned) {
    g_nx_stm32_system.started = true;
    EXPECT_EQ(Run(), NX_ERROR_BUSY);
    g_nx_stm32_system.started = false;
    g_nx_stm32_system.remaining_effects = 1U;
    EXPECT_EQ(Run(), NX_ERROR_BUSY);
    g_nx_stm32_system.remaining_effects = 0U;
    g_nx_stm32_system.timer_error = NX_ERROR_IO;
    EXPECT_EQ(Run(), NX_ERROR_STATE);
    EXPECT_TRUE(reads.empty());
}
TEST_F(F407Handoff, RejectsWrongCpuAndPhysicalDensityOrDeviceGroup) {
    registers[kCpuid] = 0x410fc271U;
    Rejected(NX_ERROR_UNSUPPORTED);
    registers[kCpuid] = 0x410fc241U;
    registers[kDebugIdentity] = 0x10000450U;
    EXPECT_EQ(Run(), NX_ERROR_IO);
    registers[kDebugIdentity] = 0x10000413U;
    EXPECT_CALL(cpu, Read16(kFlashDensity)).WillOnce(Return(2048U));
    EXPECT_EQ(Run(), NX_ERROR_IO);
}
TEST_F(F407Handoff, RejectsVectorAlignmentExtentAndAddressOverflow) {
    for (uint32_t invalid : {0U, kFlash - 512U, kVector + 4U,
                             kFlash + kFlashBytes, UINT32_MAX - 511U}) {
        EXPECT_EQ(Run(invalid), NX_ERROR_INVALID);
    }
    EXPECT_TRUE(reads.empty());
}
TEST_F(F407Handoff, RejectsBadMspAndResetPcBeforeTail) {
    for (uint32_t invalid :
         {0x10010000U, 0x20000000U, kStack + 8U, kStack - 4U}) {
        registers[kVector] = invalid;
        EXPECT_EQ(Run(), NX_ERROR_INVALID);
    }
    registers[kVector] = kStack;
    for (uint32_t invalid :
         {0U, kPc & ~1U, kFlash - 1U, kFlash + kFlashBytes + 1U, 0x20000001U}) {
        registers[kVector + 4U] = invalid;
        EXPECT_EQ(Run(), NX_ERROR_INVALID);
    }
}
TEST_F(F407Handoff, AcceptsFinalAlignedVectorAndFinalExecutableHalfword) {
    uint32_t vector = kFlash + kFlashBytes - 512U;
    registers[vector] = 0x20000008U;
    registers[vector + 4U] = kFlash + kFlashBytes - 1U;
    EXPECT_CALL(cpu, Tail(vector, 0x20000008U, kFlash + kFlashBytes - 1U));
    EXPECT_THROW(Run(vector), Transfer);
}
TEST_F(F407Handoff, RejectsUnsupportedFpuMpuCachesSleepAndDebugState) {
    for (auto invalid : {std::pair<uintptr_t, uint32_t>{kCpacr, 0x00500000U},
                         {kFpccr, 1U},
                         {kMpu, 1U},
                         {kCcr, 1U << 16U},
                         {kScr, 4U},
                         {kDebugControl, 2U},
                         {kDemcr, 1U << 24U},
                         {kFlashRegisters, 1U << 9U}}) {
        registers[invalid.first] = invalid.second;
        EXPECT_EQ(Run(), NX_ERROR_UNSUPPORTED);
        registers[invalid.first] = 0U;
    }
}
TEST_F(F407Handoff, RejectsEnabledPendingAndActiveInterruptsInAllBanks) {
    for (uintptr_t base : {0xe000e100U, 0xe000e200U, 0xe000e300U}) {
        for (uintptr_t word = 0U; word < 3U; ++word) {
            registers[base + 4U * word] = 1U;
            EXPECT_EQ(Run(), NX_ERROR_BUSY);
            registers[base + 4U * word] = 0U;
        }
    }
    for (uint32_t pending :
         {1U << 31U, 1U << 28U, 1U << 26U, 1U << 22U, 1U << 12U, 1U}) {
        registers[kIcsr] = pending;
        EXPECT_EQ(Run(), NX_ERROR_BUSY);
    }
}
TEST_F(F407Handoff, SysTickEnableAndInterruptRejectAndCountflagIsConsumed) {
    for (uint32_t state : {1U, 2U}) {
        registers[kSystick] = state | (1U << 16U);
        EXPECT_EQ(Run(), NX_ERROR_BUSY);
        EXPECT_EQ(registers[kSystick], state);
    }
    registers[kSystick] = 1U << 16U;
    EXPECT_THROW(Run(), Transfer);
    EXPECT_EQ(registers[kSystick], 0U);
}
TEST_F(F407Handoff, SysTickReloadCurrentAndSystemFaultOwnersMustBeClear) {
    for (uintptr_t address : {kSystick + 4U, kSystick + 8U, 0xe000ed24UL}) {
        registers[address] = 1U;
        EXPECT_EQ(Run(), NX_ERROR_BUSY);
        registers[address] = 0U;
    }
}
TEST_F(F407Handoff, RejectsClockSourceDivisorsGatesAndResetOwners) {
    for (auto invalid : {std::pair<uintptr_t, uint32_t>{kRcc, 0x01000083U},
                         {kRcc + 8U, 2U},
                         {kRcc + 0x0cU, 1U << 8U},
                         {kRcc + 0x80U, 1U << 31U},
                         {kRcc + 0x10U, 1U},
                         {kRcc + 0x14U, 1U},
                         {kRcc + 0x18U, 1U},
                         {kRcc + 0x20U, 1U},
                         {kRcc + 0x24U, 1U},
                         {kRcc + 0x30U, 1U << 21U},
                         {kRcc + 0x34U, 1U},
                         {kRcc + 0x38U, 1U},
                         {kRcc + 0x40U, 1U},
                         {kRcc + 0x44U, 1U}}) {
        uint32_t previous = registers[invalid.first];
        registers[invalid.first] = invalid.second;
        EXPECT_EQ(Run(), NX_ERROR_BUSY);
        registers[invalid.first] = previous;
    }
}
TEST_F(F407Handoff, BusyOrUnlockedFlashAndArmedWatchdogReject) {
    registers[kFlashRegisters + 0x0cU] = 1U << 16U;
    EXPECT_EQ(Run(), NX_ERROR_BUSY);
    registers[kFlashRegisters + 0x0cU] = 0U;
    registers[kFlashRegisters + 0x10U] = 0U;
    EXPECT_EQ(Run(), NX_ERROR_BUSY);
    registers[kFlashRegisters + 0x10U] = 0x80000000U;
    registers[kRcc + 0x74U] = 1U;
    EXPECT_EQ(Run(), NX_ERROR_UNSUPPORTED);
}
} /* namespace */

extern "C" uint32_t nx_f407_handoff_model_read(uintptr_t address) {
    return hardware->Read(address);
}
extern "C" uint32_t nx_f407_handoff_model_read16(uintptr_t address) {
    return hardware->Read16(address);
}
extern "C" uint32_t nx_f407_handoff_model_control(void) {
    return hardware->Control();
}
extern "C" __attribute__((noreturn)) void
nx_f407_handoff_model_tail(uint32_t vector, uint32_t stack, uint32_t pc) {
    hardware->Tail(vector, stack, pc);
    std::abort();
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return exception_number;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return masks;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{masks.primask};
    masks.primask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    masks.primask = previous.value;
}
