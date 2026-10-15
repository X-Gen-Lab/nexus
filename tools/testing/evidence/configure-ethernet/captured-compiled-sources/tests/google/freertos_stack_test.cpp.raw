/**
 * \file            freertos_stack_test.cpp
 * \brief           Real stack snapshot verifies external kernel bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "nexus/arch/arch.h"
#include "nexus/os/freertos_stack.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class Ports {
  public:
    MOCK_METHOD(bool, isr, ());
    MOCK_METHOD(bool, privileged, ());
    MOCK_METHOD(nx_arch_irq_masks_t, masks, ());
    MOCK_METHOD(BaseType_t, scheduler, ());
    MOCK_METHOD(BaseType_t, buffers,
                (TaskHandle_t, StackType_t**, StaticTask_t**));
};
Ports* ports;

class FreeRTOSStack : public testing::Test {
  protected:
    testing::StrictMock<Ports> dependency;
    StaticTask_t control{};
    alignas(8) StackType_t stack[32]{};
    nx_freertos_stack_binding_t binding{
        reinterpret_cast<TaskHandle_t>(&control), &control, stack, 32U};
    nx_stack_observation_t out{77U, 78U, 79U};

    void SetUp() override {
        ports = &dependency;
        std::memset(stack, 0xA5, sizeof stack);
    }
    void allowed() {
        EXPECT_CALL(dependency, isr()).WillOnce(testing::Return(false));
        EXPECT_CALL(dependency, privileged()).WillOnce(testing::Return(true));
        EXPECT_CALL(dependency, masks())
            .WillOnce(testing::Return(nx_arch_irq_masks_t{0U, 0U, 0U}));
        EXPECT_CALL(dependency, scheduler())
            .WillOnce(testing::Return(taskSCHEDULER_RUNNING));
    }
    void exact_buffers() {
        EXPECT_CALL(dependency, buffers(binding.handle, testing::_, testing::_))
            .WillOnce(testing::DoAll(testing::SetArgPointee<1>(stack),
                                     testing::SetArgPointee<2>(&control),
                                     testing::Return(pdTRUE)));
    }
};

TEST_F(FreeRTOSStack, RejectsInvalidBindingsBeforeAnyKernelOrMemoryAccess) {
    EXPECT_EQ(nx_freertos_stack_observe(nullptr, &out), NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_stack_observe(&binding, nullptr), NX_ERROR_INVALID);
    binding.handle = nullptr;
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_INVALID);
    binding.handle = reinterpret_cast<TaskHandle_t>(&control);
    binding.words = SIZE_MAX;
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_INVALID);
    EXPECT_EQ(out.capacity_bytes, 77U);
}

TEST_F(FreeRTOSStack, HandlerRejectsWithoutCallingTheKernel) {
    EXPECT_CALL(dependency, isr()).WillOnce(testing::Return(true));
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_CONTEXT);
    EXPECT_EQ(out.capacity_bytes, 77U);
}

TEST_F(FreeRTOSStack, MaskedThreadRejectsWithoutCallingTheKernel) {
    EXPECT_CALL(dependency, isr()).WillOnce(testing::Return(false));
    EXPECT_CALL(dependency, privileged()).WillOnce(testing::Return(true));
    EXPECT_CALL(dependency, masks())
        .WillOnce(testing::Return(nx_arch_irq_masks_t{1U, 0U, 0U}));
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_CONTEXT);
}

TEST_F(FreeRTOSStack, StoppedSchedulerDoesNotQueryAnUnstartedTask) {
    EXPECT_CALL(dependency, isr()).WillOnce(testing::Return(false));
    EXPECT_CALL(dependency, privileged()).WillOnce(testing::Return(true));
    EXPECT_CALL(dependency, masks())
        .WillOnce(testing::Return(nx_arch_irq_masks_t{0U, 0U, 0U}));
    EXPECT_CALL(dependency, scheduler())
        .WillOnce(testing::Return(taskSCHEDULER_NOT_STARTED));
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_CONTEXT);
}

TEST_F(FreeRTOSStack, ActualKernelBufferMismatchCannotProduceASnapshot) {
    allowed();
    EXPECT_CALL(dependency, buffers(binding.handle, testing::_, testing::_))
        .WillOnce(testing::DoAll(testing::SetArgPointee<1>(stack + 1U),
                                 testing::SetArgPointee<2>(&control),
                                 testing::Return(pdTRUE)));
    EXPECT_EQ(nx_freertos_stack_observe(&binding, &out), NX_ERROR_STATE);
    EXPECT_EQ(out.capacity_bytes, 77U);
}

TEST_F(FreeRTOSStack, AllPaintedSpanTerminatesWithoutTheKernelUnboundedScan) {
    allowed();
    exact_buffers();
    ASSERT_EQ(nx_freertos_stack_observe(&binding, &out), NX_SUCCESS);
    EXPECT_EQ(out.capacity_bytes, sizeof stack);
    EXPECT_EQ(out.untouched_bytes, sizeof stack);
    EXPECT_EQ(out.touched_bytes, 0U);
}

TEST_F(FreeRTOSStack, ReadOnlyObservedPrefixUsesOriginalCapacity) {
    reinterpret_cast<unsigned char*>(stack)[13U] = 0U;
    allowed();
    exact_buffers();
    ASSERT_EQ(nx_freertos_stack_observe(&binding, &out), NX_SUCCESS);
    EXPECT_EQ(out.untouched_bytes, 13U);
    EXPECT_EQ(out.touched_bytes, sizeof stack - 13U);
    EXPECT_EQ(reinterpret_cast<unsigned char*>(stack)[13U], 0U);
}
} /* namespace */

extern "C" {
bool nx_arch_in_isr(void) {
    return ports->isr();
}
bool nx_arch_is_privileged(void) {
    return ports->privileged();
}
nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return ports->masks();
}
BaseType_t xTaskGetSchedulerState(void) {
    return ports->scheduler();
}
BaseType_t xTaskGetStaticBuffers(TaskHandle_t task, StackType_t** stack,
                                 StaticTask_t** control) {
    return ports->buffers(task, stack, control);
}
}
