/**
 * \file            os_secure_context_test.cpp
 * \brief           Actual Secure lease mechanism with mocked CPU and ASM ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/os/secure_context.h"
extern "C" {
#include "secure_context.h"
#include "secure_context_model.h"
#include "secure_init.h"
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
namespace {
struct Write {
    nx_secure_register_t reg;
    uintptr_t value;
};
class Cpu {
  public:
    MOCK_METHOD(uintptr_t, Read, (nx_secure_register_t));
    MOCK_METHOD(void, WriteReg, (nx_secure_register_t, uintptr_t));
    MOCK_METHOD(bool, NonsecureCaller, ());
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(bool, Writable, (void*, size_t));
    MOCK_METHOD(void, Dsb, ());
    MOCK_METHOD(void, Isb, ());
    MOCK_METHOD(void, Load, (SecureContext_t*));
    MOCK_METHOD(void, Save, (SecureContext_t*));
    MOCK_METHOD(nx_freertos_secure_context_t*, Resolve, (void*));
};
Cpu* cpu;
class SecureLease : public ::testing::Test {
  protected:
    NiceMock<Cpu> hardware;
    std::array<uintptr_t, 8> regs = {};
    std::vector<Write> writes;
    nx_freertos_secure_context_t first = {};
    nx_freertos_secure_context_t second = {};
    alignas(8) std::array<uint8_t, 256> first_stack = {};
    alignas(8) std::array<uint8_t, 256> second_stack = {};
    void* first_task = reinterpret_cast<void*>(0x20000020U);
    void* second_task = reinterpret_cast<void*>(0x20000040U);
    bool nonsecure = false;
    bool privileged = true;
    uint32_t exception = 0;
    void SetUp() override {
        cpu = &hardware;
        nx_secure_model_reset();
        ON_CALL(hardware, Read(_))
            .WillByDefault([this](nx_secure_register_t r) {
                return regs[static_cast<size_t>(r)];
            });
        ON_CALL(hardware, WriteReg(_, _))
            .WillByDefault([this](nx_secure_register_t r, uintptr_t value) {
                regs[static_cast<size_t>(r)] = value;
                writes.push_back({r, value});
            });
        ON_CALL(hardware, NonsecureCaller()).WillByDefault([this]() {
            return nonsecure;
        });
        ON_CALL(hardware, Privileged()).WillByDefault([this]() {
            return privileged;
        });
        ON_CALL(hardware, Exception()).WillByDefault([this]() {
            return exception;
        });
        ON_CALL(hardware, Writable(_, _)).WillByDefault(Return(true));
        ON_CALL(hardware, Resolve(_)).WillByDefault([this](void* task) {
            return task == first_task    ? &first
                   : task == second_task ? &second
                                         : nullptr;
        });
        ON_CALL(hardware, Load(_)).WillByDefault([this](SecureContext_t* c) {
            regs[NX_SECURE_REG_PSP] =
                reinterpret_cast<uintptr_t>(c->pucCurrentStackPointer);
            regs[NX_SECURE_REG_PSPLIM] =
                reinterpret_cast<uintptr_t>(c->pucStackLimit);
        });
        ON_CALL(hardware, Save(_)).WillByDefault([this](SecureContext_t* c) {
            c->pucCurrentStackPointer =
                reinterpret_cast<uint8_t*>(regs[NX_SECURE_REG_PSP]);
            regs[NX_SECURE_REG_PSP] = 0;
            regs[NX_SECURE_REG_PSPLIM] = 0;
        });
    }
    void TearDown() override {
        cpu = nullptr;
    }
    void Prepare() {
        ASSERT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(),
                                                     first_stack.size(),
                                                     first_task),
                  NX_SUCCESS);
        ASSERT_EQ(
            nx_freertos_secure_context_prepare(
                &second, second_stack.data(), second_stack.size(), second_task),
            NX_SUCCESS);
    }
    void Kernel() {
        nonsecure = true;
        exception = 11;
    }
    void Initialize() {
        Prepare();
        Kernel();
        SecureContext_Init();
        writes.clear();
    }
    uint32_t Allocate(void* task = nullptr) {
        return SecureContext_AllocateContext(128, task == nullptr ? first_task
                                                                  : task);
    }
    size_t NonmaskWrites() const {
        size_t count = 0;
        for (const auto& write : writes) {
            if (write.reg != NX_SECURE_REG_PRIMASK &&
                write.reg != NX_SECURE_REG_PRIMASK_NS) {
                ++count;
            }
        }
        return count;
    }
};
TEST_F(SecureLease, RejectsMissingOrTooSmallStorageBeforeCpuEffects) {
    EXPECT_EQ(nx_freertos_secure_context_prepare(nullptr, first_stack.data(),
                                                 256, first_task),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_freertos_secure_context_prepare(&first, nullptr, 256, first_task),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(), 64,
                                                 first_task),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data() + 1,
                                                 248, first_task),
              NX_ERROR_INVALID);
    EXPECT_FALSE(first.prepared);
    EXPECT_TRUE(writes.empty());
}
TEST_F(SecureLease, RejectsNonsecureWritableStorageBeforePreparing) {
    EXPECT_CALL(hardware, Writable(&first, sizeof(first)))
        .WillOnce(Return(false));
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(),
                                                 256, first_task),
              NX_ERROR_INVALID);
    EXPECT_FALSE(first.prepared);
}
TEST_F(SecureLease, PrepareRejectsUnprivilegedAndHandlerContexts) {
    privileged = false;
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(),
                                                 256, first_task),
              NX_ERROR_CONTEXT);
    privileged = true;
    exception = 11;
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(),
                                                 256, first_task),
              NX_ERROR_CONTEXT);
    EXPECT_FALSE(first.prepared);
}
TEST_F(SecureLease, PreparingTwiceDoesNotResetEpoch) {
    Prepare();
    first.epoch = 9;
    EXPECT_EQ(nx_freertos_secure_context_prepare(&first, first_stack.data(),
                                                 256, first_task),
              NX_ERROR_STATE);
    EXPECT_EQ(first.epoch, 9U);
}
TEST_F(SecureLease, InitRejectsNonsecureThreadAndSecureDirectCalls) {
    nonsecure = true;
    SecureContext_Init();
    exception = 11;
    nonsecure = false;
    SecureContext_Init();
    EXPECT_TRUE(writes.empty());
}
TEST_F(SecureLease, InitOwnsOnlySecureStackRegistersAndRestoresBothMasks) {
    Prepare();
    Kernel();
    regs[NX_SECURE_REG_PRIMASK] = 1;
    regs[NX_SECURE_REG_PRIMASK_NS] = 0;
    SecureContext_Init();
    EXPECT_EQ(regs[NX_SECURE_REG_PSP], 0U);
    EXPECT_EQ(regs[NX_SECURE_REG_PSPLIM], 0U);
    EXPECT_EQ(regs[NX_SECURE_REG_CONTROL], 2U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], 1U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], 0U);
}
TEST_F(SecureLease, RepeatedInitDoesNotClearAnActiveSecureStack) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_CALL(hardware, Load(_)).Times(1);
    SecureContext_LoadContext(handle, first_task);
    auto psp = regs[NX_SECURE_REG_PSP];
    writes.clear();
    SecureContext_Init();
    EXPECT_EQ(regs[NX_SECURE_REG_PSP], psp);
    EXPECT_EQ(NonmaskWrites(), 0U);
}
TEST_F(SecureLease, AllocateRejectsBeforeSchedulerContextInit) {
    Prepare();
    Kernel();
    EXPECT_EQ(Allocate(), 0U);
    EXPECT_FALSE(first.leased);
}
TEST_F(SecureLease, AllocateRejectsThreadAndUnexpectedHandlerOrigins) {
    Initialize();
    exception = 0;
    EXPECT_EQ(Allocate(), 0U);
    exception = 16;
    EXPECT_EQ(Allocate(), 0U);
    exception = 11;
    nonsecure = false;
    EXPECT_EQ(Allocate(), 0U);
    EXPECT_FALSE(first.leased);
}
TEST_F(SecureLease, AllocateRejectsUnknownTaskAndCapacityMismatch) {
    Initialize();
    EXPECT_EQ(Allocate(reinterpret_cast<void*>(0x20000900U)), 0U);
    EXPECT_EQ(SecureContext_AllocateContext(256, first_task), 0U);
    EXPECT_EQ(SecureContext_AllocateContext(65, first_task), 0U);
    EXPECT_EQ(SecureContext_AllocateContext(0, first_task), 0U);
    EXPECT_FALSE(first.leased);
}
TEST_F(SecureLease, AllocationBorrowsExactSuppliedStackAndWritesSeal) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_TRUE(first.leased);
    EXPECT_EQ(first.top, first_stack.data() + 248);
    EXPECT_EQ(first.limit, first.top - 128);
    EXPECT_EQ(first.saved_sp, first.top);
    uint32_t seal[2];
    std::memcpy(seal, first.top, sizeof(seal));
    EXPECT_EQ(seal[0], 0xfef5eda5U);
    EXPECT_EQ(seal[1], 0xfef5eda5U);
}
TEST_F(SecureLease, DuplicateAllocationRetainsOriginalLease) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_EQ(Allocate(), 0U);
    EXPECT_EQ(first.epoch, handle);
    EXPECT_TRUE(first.leased);
}
TEST_F(SecureLease, EpochExhaustionRejectsWithoutWrappingToOldHandle) {
    Initialize();
    first.epoch = UINT32_MAX;
    EXPECT_EQ(Allocate(), 0U);
    EXPECT_EQ(first.epoch, UINT32_MAX);
    EXPECT_FALSE(first.leased);
}
TEST_F(SecureLease, AllIncomingSecureAndNonsecureMasksAreRestored) {
    Initialize();
    for (uint32_t s = 0; s <= 1; s++)
        for (uint32_t ns = 0; ns <= 1; ns++) {
            regs[NX_SECURE_REG_PRIMASK] = s;
            regs[NX_SECURE_REG_PRIMASK_NS] = ns;
            auto handle = Allocate();
            ASSERT_NE(handle, 0U);
            SecureContext_FreeContext(handle, first_task);
            EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], s);
            EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], ns);
        }
}
TEST_F(SecureLease, ForgedOrCrossTaskHandleDoesNotLoadAsm) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_CALL(hardware, Load(_)).Times(0);
    SecureContext_LoadContext(handle + 1, first_task);
    SecureContext_LoadContext(handle, second_task);
    EXPECT_EQ(regs[NX_SECURE_REG_PSP], 0U);
}
TEST_F(SecureLease, LoadedContextCannotBeFreedOrReplacedByAnother) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_CALL(hardware, Load(_)).Times(1);
    SecureContext_LoadContext(handle, first_task);
    SecureContext_FreeContext(handle, first_task);
    EXPECT_TRUE(first.leased);
    EXPECT_EQ(Allocate(second_task), 0U);
}
TEST_F(SecureLease, SaveAndReloadUseTheActualAsmBoundaryAndStackPosition) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    EXPECT_CALL(hardware, Load(_)).Times(2);
    SecureContext_LoadContext(handle, first_task);
    regs[NX_SECURE_REG_PSP] -= 32;
    auto psp = regs[NX_SECURE_REG_PSP];
    exception = 14;
    EXPECT_CALL(hardware, Save(_)).Times(1);
    SecureContext_SaveContext(handle, first_task);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(first.saved_sp), psp);
    EXPECT_EQ(regs[NX_SECURE_REG_PSP], 0U);
    EXPECT_EQ(regs[NX_SECURE_REG_PSPLIM], 0U);
    SecureContext_LoadContext(handle, first_task);
    EXPECT_EQ(regs[NX_SECURE_REG_PSP], psp);
}
TEST_F(SecureLease, SaveRejectsOtherTasksAndStaleLeaseBeforeAsm) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    SecureContext_LoadContext(handle, first_task);
    exception = 14;
    EXPECT_CALL(hardware, Save(_)).Times(0);
    SecureContext_SaveContext(handle, second_task);
    SecureContext_SaveContext(handle + 1, first_task);
    EXPECT_NE(regs[NX_SECURE_REG_PSP], 0U);
}
TEST_F(SecureLease, SavingDamagedSealFailsStopAndRetainsLease) {
    Initialize();
    auto handle = Allocate();
    SecureContext_LoadContext(handle, first_task);
    std::memset(first.top + 4, 0, 4);
    exception = 14;
    EXPECT_CALL(hardware, Save(_)).Times(0);
    EXPECT_THROW(SecureContext_SaveContext(handle, first_task),
                 std::runtime_error);
    EXPECT_TRUE(first.leased);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], 1U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], 1U);
}
TEST_F(SecureLease, SavingOutOfBoundsSecureStackFailsStopBeforeAsm) {
    Initialize();
    auto handle = Allocate();
    SecureContext_LoadContext(handle, first_task);
    regs[NX_SECURE_REG_PSP] = reinterpret_cast<uintptr_t>(first.limit) - 8;
    exception = 14;
    EXPECT_CALL(hardware, Save(_)).Times(0);
    EXPECT_THROW(SecureContext_SaveContext(handle, first_task),
                 std::runtime_error);
    EXPECT_TRUE(first.leased);
}
TEST_F(SecureLease, RebindingRetiredContextPreservesEpochAndRejectsOldLease) {
    Initialize();
    auto old = Allocate();
    SecureContext_FreeContext(old, first_task);
    nonsecure = false;
    exception = 0;
    ASSERT_EQ(nx_freertos_secure_context_rebind(&first, first_task),
              NX_SUCCESS);
    Kernel();
    auto current = Allocate();
    EXPECT_GT(current, old);
    EXPECT_CALL(hardware, Load(_)).Times(0);
    SecureContext_LoadContext(old, first_task);
}
TEST_F(SecureLease, RebindingLeasedContextNeverReleasesItsStorage) {
    Initialize();
    auto handle = Allocate();
    ASSERT_NE(handle, 0U);
    nonsecure = false;
    exception = 0;
    EXPECT_EQ(nx_freertos_secure_context_rebind(&first, second_task),
              NX_ERROR_BUSY);
    EXPECT_EQ(first.task, first_task);
    EXPECT_TRUE(first.leased);
}
TEST_F(SecureLease, RebindValidatesSecureMetadataBeforeDereferencing) {
    Prepare();
    writes.clear();
    EXPECT_CALL(hardware, Writable(&first, sizeof(first)))
        .WillOnce(Return(false));
    EXPECT_EQ(nx_freertos_secure_context_rebind(&first, second_task),
              NX_ERROR_INVALID);
    EXPECT_EQ(first.task, first_task);
    EXPECT_TRUE(writes.empty());
}
TEST_F(SecureLease, SaveRejectsMisalignedAsmResultWithoutRetiringLease) {
    Initialize();
    auto handle = Allocate();
    SecureContext_LoadContext(handle, first_task);
    exception = 14;
    EXPECT_CALL(hardware, Save(_)).WillOnce([this](SecureContext_t* context) {
        context->pucCurrentStackPointer = first.top - 1;
    });
    EXPECT_THROW(SecureContext_SaveContext(handle, first_task),
                 std::runtime_error);
    EXPECT_TRUE(first.leased);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], 1U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], 1U);
}
TEST_F(SecureLease, SecureInitPreservesUnrelatedAircrFieldsAndBothMasks) {
    Initialize();
    regs[NX_SECURE_REG_AIRCR] = 0xfa051123U;
    regs[NX_SECURE_REG_PRIMASK] = 1;
    regs[NX_SECURE_REG_PRIMASK_NS] = 0;
    SecureInit_DePrioritizeNSExceptions();
    EXPECT_EQ(regs[NX_SECURE_REG_AIRCR], 0x05fa5123U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], 1U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], 0U);
}
#if configENABLE_FPU || configENABLE_MVE
TEST_F(SecureLease, FloatingPointSetupPreservesOtherFieldsAndIncomingMasks) {
    Initialize();
    regs[NX_SECURE_REG_NSACR] = 0x1000;
    regs[NX_SECURE_REG_FPCCR] = (1U << 29) | 0x1234;
    regs[NX_SECURE_REG_PRIMASK] = 0;
    regs[NX_SECURE_REG_PRIMASK_NS] = 1;
    SecureInit_EnableNSFPUAccess();
    EXPECT_EQ(regs[NX_SECURE_REG_NSACR], 0x1c00U);
    EXPECT_EQ(regs[NX_SECURE_REG_FPCCR], (1U << 26) | 0x1234U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK], 0U);
    EXPECT_EQ(regs[NX_SECURE_REG_PRIMASK_NS], 1U);
}
#else
TEST_F(SecureLease, NoFloatingPointCapabilityDoesNotTouchFpRegisters) {
    Initialize();
    EXPECT_CALL(hardware, Read(NX_SECURE_REG_NSACR)).Times(0);
    EXPECT_CALL(hardware, Read(NX_SECURE_REG_FPCCR)).Times(0);
    SecureInit_EnableNSFPUAccess();
}
#endif
} /* namespace */
extern "C" bool nx_arch_is_privileged(void) {
    return cpu->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return cpu->Exception();
}
extern "C" void nx_arch_dsb(void) {
    cpu->Dsb();
}
extern "C" void nx_arch_isb(void) {
    cpu->Isb();
}
extern "C" bool nx_secure_model_nonsecure_caller(void) {
    return cpu->NonsecureCaller();
}
extern "C" uintptr_t nx_secure_model_read(nx_secure_register_t reg) {
    return cpu->Read(reg);
}
extern "C" void nx_secure_model_write(nx_secure_register_t reg,
                                      uintptr_t value) {
    cpu->WriteReg(reg, value);
}
extern "C" bool nx_secure_model_secure_writable(void* p, size_t n) {
    return cpu->Writable(p, n);
}
extern "C" nx_freertos_secure_context_t*
nx_freertos_secure_context_for_task(void* task) {
    return cpu->Resolve(task);
}
extern "C" void SecureContext_LoadContextAsm(SecureContext_t* context) {
    cpu->Load(context);
}
extern "C" void SecureContext_SaveContextAsm(SecureContext_t* context) {
    cpu->Save(context);
}
extern "C" void nx_freertos_secure_fault(nx_freertos_secure_fault_t) {
    throw std::runtime_error("secure stack integrity failure");
}
