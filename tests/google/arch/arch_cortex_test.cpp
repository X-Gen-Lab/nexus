/**
 * \file            arch_cortex_test.cpp
 *
 * \brief           CPU primitive contracts against a mocked hardware boundary.
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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arch_cortex_model.h"
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
class Hardware {
  public:
    MOCK_METHOD(uint32_t, Read, (nx_arch_model_register_t));
    MOCK_METHOD(void, Write, (nx_arch_model_register_t, uint32_t));
    MOCK_METHOD(void, Instruction, (nx_arch_model_instruction_t));
};
Hardware* hardware;
class ArchCortex : public ::testing::Test {
  protected:
    StrictMock<Hardware> registers;
    void SetUp() override {
        hardware = &registers;
    }
    void TearDown() override {
        hardware = nullptr;
    }
    void ExpectMask(uint32_t primask, uint32_t basepri, uint32_t faultmask) {
        EXPECT_CALL(registers, Read(NX_ARCH_MODEL_PRIMASK))
            .WillOnce(Return(primask));
#if NEXUS_ARCH_TEST_PRIORITY_MASK
        EXPECT_CALL(registers, Read(NX_ARCH_MODEL_BASEPRI))
            .WillOnce(Return(basepri));
        EXPECT_CALL(registers, Read(NX_ARCH_MODEL_FAULTMASK))
            .WillOnce(Return(faultmask));
#else
        (void)basepri;
        (void)faultmask;
#endif
    }
    void ExpectPrivilegedThread() {
        EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
        EXPECT_CALL(registers, Read(NX_ARCH_MODEL_CONTROL))
            .WillOnce(Return(0U));
    }
};

TEST_F(ArchCortex, SaveRetainsIncomingMaskBeforeDisablingAndBarriers) {
    InSequence order;
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_PRIMASK)).WillOnce(Return(1U));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_CPSID_I));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_DSB));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_ISB));
    EXPECT_EQ(nx_arch_irq_save().value, 1U);
}

TEST_F(ArchCortex, RestoreWritesOnlyIncomingPrimaskBetweenBarriers) {
    InSequence order;
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_DSB));
    EXPECT_CALL(registers, Write(NX_ARCH_MODEL_PRIMASK, 1U));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_ISB));
    nx_arch_irq_restore({1U});
}

TEST_F(ArchCortex, UnmaskedContextDoesNotInventPriorityMasks) {
    ExpectMask(0U, 0U, 0U);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST_F(ArchCortex, IncomingPrimaskBlocksProgress) {
    ExpectMask(1U, 0U, 0U);
    EXPECT_TRUE(nx_arch_irq_is_masked());
}

TEST_F(ArchCortex, MaskSnapshotReportsExactRegistersWithoutMutation) {
    ExpectMask(1U, 0x50U, 1U);
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    EXPECT_EQ(masks.primask, 1U);
#if NEXUS_ARCH_TEST_PRIORITY_MASK
    EXPECT_EQ(masks.basepri, 0x50U);
    EXPECT_EQ(masks.faultmask, 1U);
#else
    EXPECT_EQ(masks.basepri, 0U);
    EXPECT_EQ(masks.faultmask, 0U);
#endif
}

#if NEXUS_ARCH_TEST_PRIORITY_MASK
TEST_F(ArchCortex, BasepriBlocksProgressWithoutMutation) {
    ExpectMask(0U, 0x50U, 0U);
    EXPECT_TRUE(nx_arch_irq_is_masked());
}

TEST_F(ArchCortex, FaultmaskBlocksProgressWithoutMutation) {
    ExpectMask(0U, 0U, 1U);
    EXPECT_TRUE(nx_arch_irq_is_masked());
}
#endif

TEST_F(ArchCortex, ExceptionNumberRetainsExactIpsrIdentity) {
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(47U));
    EXPECT_EQ(nx_arch_exception_number(), 47U);
}

TEST_F(ArchCortex, ThreadContextHasNoHardwareException) {
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
    EXPECT_FALSE(nx_arch_in_isr());
}

TEST_F(ArchCortex, HandlerContextIncludesSystemExceptions) {
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(3U));
    EXPECT_TRUE(nx_arch_in_isr());
}

TEST_F(ArchCortex, HandlerRemainsPrivilegedWithoutReadingThreadControl) {
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(16U));
    EXPECT_TRUE(nx_arch_is_privileged());
}

TEST_F(ArchCortex, ThreadPrivilegeUsesControlNprivOnly) {
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_CONTROL)).WillOnce(Return(2U));
    EXPECT_TRUE(nx_arch_is_privileged());
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_CONTROL)).WillOnce(Return(1U));
    EXPECT_FALSE(nx_arch_is_privileged());
}

TEST_F(ArchCortex, BarriersRemainDistinctInstructions) {
    InSequence order;
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_DMB));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_DSB));
    EXPECT_CALL(registers, Instruction(NX_ARCH_MODEL_ISB));
    nx_arch_dmb();
    nx_arch_dsb();
    nx_arch_isb();
}

TEST_F(ArchCortex, NullCounterOutputDoesNotReadHardware) {
    EXPECT_FALSE(nx_arch_cycle_snapshot(nullptr));
}

#if NEXUS_ARCH_HAS_DWT_CYCCNT
TEST_F(ArchCortex, UnprivilegedCounterRequestDoesNotTouchDwt) {
    uint32_t cycles = 0x12345678U;
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_IPSR)).WillOnce(Return(0U));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_CONTROL)).WillOnce(Return(1U));
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0x12345678U);
}

TEST_F(ArchCortex, DisabledTraceDoesNotReadDwtOrMutateOutput) {
    uint32_t cycles = 0x12345678U;
    ExpectPrivilegedThread();
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DEMCR)).WillOnce(Return(0U));
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0x12345678U);
}

TEST_F(ArchCortex, DisabledCounterDoesNotReadCyclesOrMutateOutput) {
    uint32_t cycles = 0x12345678U;
    ExpectPrivilegedThread();
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DEMCR))
        .WillOnce(Return(1U << 24));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DWT_CONTROL))
        .WillOnce(Return(0U));
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0x12345678U);
}

TEST_F(ArchCortex, MissingCounterDoesNotReadCyclesOrMutateOutput) {
    uint32_t cycles = 0x12345678U;
    ExpectPrivilegedThread();
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DEMCR))
        .WillOnce(Return(1U << 24));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DWT_CONTROL))
        .WillOnce(Return((1U << 25) | 1U));
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0x12345678U);
}

TEST_F(ArchCortex, EnabledCounterReadsExactlyOnceWithoutWrites) {
    uint32_t cycles = 0U;
    InSequence order;
    ExpectPrivilegedThread();
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DEMCR))
        .WillOnce(Return(1U << 24));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_DWT_CONTROL))
        .WillOnce(Return(1U));
    EXPECT_CALL(registers, Read(NX_ARCH_MODEL_CYCCNT))
        .WillOnce(Return(0xFFFFFFFFU));
    EXPECT_TRUE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0xFFFFFFFFU);
}
#else
TEST_F(ArchCortex, UnreviewedCounterProfileNeverReadsHardware) {
    uint32_t cycles = 0x12345678U;
    EXPECT_FALSE(nx_arch_cycle_snapshot(&cycles));
    EXPECT_EQ(cycles, 0x12345678U);
}
#endif
} /* namespace */

extern "C" uint32_t nx_arch_model_read(nx_arch_model_register_t reg) {
    return hardware->Read(reg);
}
extern "C" void nx_arch_model_write(nx_arch_model_register_t reg,
                                    uint32_t value) {
    hardware->Write(reg, value);
}
extern "C" void
nx_arch_model_instruction(nx_arch_model_instruction_t instruction) {
    hardware->Instruction(instruction);
}
