/**
 * \file            arch_sleep_test.cpp
 * \brief           Real atomic sleep mechanism with mocked CPU boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_sleep_model.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
namespace {
class Cpu {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, Masks, ());
    MOCK_METHOD(nx_arch_irq_state_t, Save, ());
    MOCK_METHOD(void, Restore, (uint32_t));
    MOCK_METHOD(uint32_t, Control, ());
    MOCK_METHOD(void, Dsb, ());
    MOCK_METHOD(void, Wfi, ());
    MOCK_METHOD(void, Isb, ());
};
Cpu* cpu;
class ArchSleep : public ::testing::Test {
  protected:
    StrictMock<Cpu> model;
    uint32_t sequence = 9;
    bool slept = true;
    void SetUp() override {
        cpu = &model;
    }
    void TearDown() override {
        cpu = nullptr;
    }
    void Thread() {
        EXPECT_CALL(model, Privileged()).WillOnce(Return(true));
        EXPECT_CALL(model, Exception()).WillOnce(Return(0));
    }
    void Enter(uint32_t changed = 9) {
        Thread();
        EXPECT_CALL(model, Masks())
            .WillOnce(Return(nx_arch_irq_masks_t{0, 0, 0}));
        EXPECT_CALL(model, Save()).WillOnce([this, changed]() {
            sequence = changed;
            return nx_arch_irq_state_t{0};
        });
    }
    void Wait() {
        Thread();
        EXPECT_CALL(model, Masks())
            .WillOnce(Return(nx_arch_irq_masks_t{1, 0, 0}));
        EXPECT_CALL(model, Control()).WillOnce(Return(0));
        EXPECT_CALL(model, Dsb());
        EXPECT_CALL(model, Wfi());
        EXPECT_CALL(model, Isb());
    }
};
TEST_F(ArchSleep, RejectsNullBeforeCpuAccess) {
    EXPECT_EQ(nx_arch_idle_if_unchanged(nullptr, 9, &slept), NX_ARCH_INVALID);
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, nullptr),
              NX_ARCH_INVALID);
}
TEST_F(ArchSleep, RejectsUnalignedWordWithoutCpuAccess) {
    alignas(4) unsigned char bytes[8] = {};
    EXPECT_EQ(nx_arch_idle_if_unchanged(reinterpret_cast<uint32_t*>(bytes + 1),
                                        9, &slept),
              NX_ARCH_INVALID);
}
TEST_F(ArchSleep, RejectsUnprivilegedWithoutMasking) {
    EXPECT_CALL(model, Privileged()).WillOnce(Return(false));
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept), NX_ARCH_CONTEXT);
    EXPECT_FALSE(slept);
}
TEST_F(ArchSleep, RejectsHandlerWithoutMasking) {
    EXPECT_CALL(model, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(model, Exception()).WillOnce(Return(16));
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept), NX_ARCH_CONTEXT);
}
TEST_F(ArchSleep, RejectsIncomingMasksWithoutChangingThem) {
    const nx_arch_irq_masks_t masks[] = {{1, 0, 0}, {0, 32, 0}, {0, 0, 1}};
    for (const auto& mask : masks) {
        Thread();
        EXPECT_CALL(model, Masks()).WillOnce(Return(mask));
        EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept),
                  NX_ARCH_CONTEXT);
    }
}
TEST_F(ArchSleep, PublicationDuringMaskingAvoidsSleep) {
    InSequence order;
    Enter(10);
    EXPECT_CALL(model, Restore(0));
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept), NX_ARCH_OK);
    EXPECT_FALSE(slept);
}
TEST_F(ArchSleep, UnchangedPublicationSleepsWithBarriersAndRestores) {
    InSequence order;
    Enter();
    Wait();
    EXPECT_CALL(model, Restore(0));
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept), NX_ARCH_OK);
    EXPECT_TRUE(slept);
}
TEST_F(ArchSleep, DeepSleepRejectsAndRestores) {
    InSequence order;
    Enter();
    Thread();
    EXPECT_CALL(model, Masks()).WillOnce(Return(nx_arch_irq_masks_t{1, 0, 0}));
    EXPECT_CALL(model, Control()).WillOnce(Return(4));
    EXPECT_CALL(model, Restore(0));
    EXPECT_EQ(nx_arch_idle_if_unchanged(&sequence, 9, &slept),
              NX_ARCH_UNSUPPORTED);
    EXPECT_FALSE(slept);
}
TEST_F(ArchSleep, DirectWfiRequiresPrimask) {
    Thread();
    EXPECT_CALL(model, Masks()).WillOnce(Return(nx_arch_irq_masks_t{0, 0, 0}));
    EXPECT_EQ(nx_arch_wait_for_interrupt(), NX_ARCH_CONTEXT);
}
TEST_F(ArchSleep, DirectWfiDoesNotChangeMask) {
    InSequence order;
    Wait();
    EXPECT_EQ(nx_arch_wait_for_interrupt(), NX_ARCH_OK);
}
} /* namespace */
extern "C" bool nx_arch_is_privileged(void) {
    return cpu->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return cpu->Exception();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return cpu->Masks();
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return cpu->Save();
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    cpu->Restore(previous.value);
}
extern "C" void nx_arch_dsb(void) {
    cpu->Dsb();
}
extern "C" void nx_arch_isb(void) {
    cpu->Isb();
}
extern "C" uint32_t nx_arch_sleep_model_control(void) {
    return cpu->Control();
}
extern "C" void nx_arch_sleep_model_wfi(void) {
    cpu->Wfi();
}
