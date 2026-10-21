/**
 * \file            freertos_mpu_gpio_test.cpp
 * \brief           Real protected authority and U575 BSRR lifecycle contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
extern "C" {
#include "nexus/os/freertos_mpu_gpio.h"
#include "private.h"
#include "stm32u575_provider.h"
}
#include <algorithm>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>

alignas(65536) static unsigned char s_privileged[32768];
alignas(4096) static unsigned char s_user[4096];
static auto* const s_task =
    reinterpret_cast<nx_freertos_mpu_task_t*>(s_privileged);
static auto* const s_control =
    reinterpret_cast<nx_freertos_mpu_gpio_t*>(s_privileged + 8192U);
static auto* const s_provider =
    reinterpret_cast<nx_stm32u575_gpio_state_t*>(s_privileged + 16408U);
static const nx_gpio_port_t s_port = {&nx_stm32u575_gpio_ops, s_provider};
static const nx_gpio_port_t* const s_port_slot = &s_port;
static const nx_freertos_mpu_gpio_grant_t s_grant = {&s_port_slot, 3U, 0x80U};
extern "C" const nx_freertos_mpu_gpio_binding_t nx_freertos_mpu_gpio_binding = {
    s_control, s_task, &s_grant};

class GpioKernel {
  public:
    MOCK_METHOD(bool, privileged, ());
    MOCK_METHOD(bool, isr, ());
    MOCK_METHOD(BaseType_t, scheduler, ());
    MOCK_METHOD(TaskHandle_t, current, ());
    MOCK_METHOD(BaseType_t, create, (const TaskParameters_t*, TaskHandle_t*));
    MOCK_METHOD(void, remove, (TaskHandle_t));
};
static GpioKernel* s_kernel;
static nx_freertos_mpu_layout_t s_layout;
static nx_arch_irq_masks_t s_masks;
GPIO_TypeDef g_nx_test_mpu_gpio_registers;
static unsigned s_writes;
static unsigned s_guard_depth;
static bool s_layout_failure;

extern "C" bool nx_arch_is_privileged(void) {
    return s_kernel->privileged();
}
extern "C" bool nx_arch_in_isr(void) {
    return s_kernel->isr();
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return s_masks.primask != 0U || s_masks.basepri != 0U ||
           s_masks.faultmask != 0U;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return s_masks;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved = {s_masks.primask};
    s_masks.primask = 1U;
    ++s_guard_depth;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    EXPECT_GT(s_guard_depth, 0U);
    --s_guard_depth;
    s_masks.primask = saved.value;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" BaseType_t xTaskGetSchedulerState(void) {
    EXPECT_EQ(s_guard_depth, 0U);
    return s_kernel->scheduler();
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    EXPECT_EQ(s_guard_depth, 0U);
    return s_kernel->current();
}
extern "C" BaseType_t
xTaskCreateRestrictedStatic(const TaskParameters_t* const input,
                            TaskHandle_t* output) {
    EXPECT_EQ(s_guard_depth, 0U);
    return s_kernel->create(input, output);
}
extern "C" void vTaskDelete(TaskHandle_t handle) {
    EXPECT_EQ(s_guard_depth, 0U);
    s_kernel->remove(handle);
}
extern "C" nx_result_t
nx_freertos_mpu_layout(nx_freertos_mpu_layout_t* output) {
    if (s_layout_failure) {
        return NX_ERROR_STATE;
    }
    *output = s_layout;
    return NX_SUCCESS;
}
extern "C" uint32_t nx_test_mpu_gpio_read(volatile uint32_t* reg) {
    return *reg;
}
extern "C" void nx_test_mpu_gpio_write(volatile uint32_t* reg, uint32_t value) {
    EXPECT_EQ(reg, &g_nx_test_mpu_gpio_registers.BSRR);
    EXPECT_EQ(s_guard_depth, 1U);
    EXPECT_EQ(s_masks.primask, 1U);
    ++s_writes;
    *reg = value;
}

class MpuGpio : public testing::Test {
  protected:
    testing::NiceMock<GpioKernel> kernel;
    void SetUp() override {
        s_kernel = &kernel;
        std::memset(s_privileged, 0, sizeof(s_privileged));
        new (s_privileged) nx_freertos_mpu_task_t{};
        new (s_privileged + 8192U) nx_freertos_mpu_gpio_t{};
        new (s_privileged + 16408U) nx_stm32u575_gpio_state_t{};
        std::memset(&g_nx_test_mpu_gpio_registers, 0,
                    sizeof(g_nx_test_mpu_gpio_registers));
        s_masks = {};
        s_writes = 0U;
        s_guard_depth = 0U;
        s_layout_failure = false;
        s_task->handle = reinterpret_cast<TaskHandle_t>(&s_task->control);
        s_task->epoch = 1U;
        *s_provider = {&g_nx_test_mpu_gpio_registers, 0x80U, true, true, true};
        uintptr_t low = std::min(
            {reinterpret_cast<uintptr_t>(&nx_freertos_mpu_gpio_open),
             reinterpret_cast<uintptr_t>(&nx_gpio_port_write),
             reinterpret_cast<uintptr_t>(nx_stm32u575_gpio_ops.write),
             reinterpret_cast<uintptr_t>(&nx_freertos_mpu_gpio_binding)});
        uintptr_t high = std::max(
            {reinterpret_cast<uintptr_t>(&nx_freertos_mpu_gpio_binding) +
                 sizeof(nx_freertos_mpu_gpio_binding),
             reinterpret_cast<uintptr_t>(&s_grant) + sizeof(s_grant),
             reinterpret_cast<uintptr_t>(&s_port) + sizeof(s_port),
             reinterpret_cast<uintptr_t>(&s_port_slot) + sizeof(s_port_slot),
             reinterpret_cast<uintptr_t>(&nx_stm32u575_gpio_ops) +
                 sizeof(nx_stm32u575_gpio_ops)});
        uintptr_t user = 0x40000000U;
        s_layout = {
            {low & ~uintptr_t{31U}, (high + 31U) & ~uintptr_t{31U}},
            {reinterpret_cast<uintptr_t>(s_privileged),
             reinterpret_cast<uintptr_t>(s_privileged) + sizeof(s_privileged)},
            {user, user + 32U},
            {reinterpret_cast<uintptr_t>(s_user),
             reinterpret_cast<uintptr_t>(s_user) + sizeof(s_user)},
            {0x1000U, 0x1020U},
            8U,
            8U};
        ON_CALL(kernel, privileged()).WillByDefault(testing::Return(true));
        ON_CALL(kernel, isr()).WillByDefault(testing::Return(false));
        ON_CALL(kernel, scheduler())
            .WillByDefault(testing::Return(taskSCHEDULER_RUNNING));
        ON_CALL(kernel, current())
            .WillByDefault(testing::Return(s_task->handle));
    }
    void TearDown() override {
        EXPECT_EQ(s_guard_depth, 0U);
        s_kernel = nullptr;
    }
    void open() {
        ASSERT_EQ(nx_freertos_mpu_gpio_open(), NX_SUCCESS);
    }
};

TEST_F(MpuGpio, WritesOneRealBsrrForTheExactCurrentTaskAndGrant) {
    open();
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_SUCCESS);
    EXPECT_EQ(g_nx_test_mpu_gpio_registers.BSRR, 0x80U);
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0U, 0x80U), NX_SUCCESS);
    EXPECT_EQ(g_nx_test_mpu_gpio_registers.BSRR, 0x800000U);
    EXPECT_EQ(s_writes, 2U);
}
TEST_F(MpuGpio, RejectsClosedAuthorityAndWrongTaskWithoutIo) {
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_STATE);
    open();
    ON_CALL(kernel, current()).WillByDefault(testing::Return(nullptr));
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_PERMISSION);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, RejectsUnrecognizedDeviceOverlapZeroAndUnauthorizedMasks) {
    open();
    EXPECT_EQ(nx_freertos_mpu_gpio_write(4U, 0x80U, 0U), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0x80U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x100U, 0U), NX_ERROR_PERMISSION);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, PreservesBothIncomingPrimaskValues) {
    open();
    for (uint32_t incoming : {0U, 1U}) {
        s_masks.primask = incoming;
        EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_SUCCESS);
        EXPECT_EQ(s_masks.primask, incoming);
    }
}
TEST_F(MpuGpio, CloseFencesEffectsAndJoinReleasesTheExactTaskPin) {
    open();
    EXPECT_EQ(s_task->gpio_pinned, 1U);
    EXPECT_EQ(nx_freertos_mpu_gpio_join(), NX_ERROR_STATE);
    EXPECT_EQ(nx_freertos_mpu_gpio_close(), NX_SUCCESS);
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_STATE);
    EXPECT_EQ(s_task->gpio_pinned, 1U);
    EXPECT_EQ(nx_freertos_mpu_gpio_join(), NX_SUCCESS);
    EXPECT_EQ(s_task->gpio_pinned, 0U);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, JoinedAuthorityReopensForTheSameLiveTaskAndWrites) {
    open();
    ASSERT_EQ(nx_freertos_mpu_gpio_close(), NX_SUCCESS);
    ASSERT_EQ(nx_freertos_mpu_gpio_join(), NX_SUCCESS);
    open();
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_SUCCESS);
    EXPECT_EQ(g_nx_test_mpu_gpio_registers.BSRR, 0x80U);
    EXPECT_EQ(s_writes, 1U);
}
TEST_F(MpuGpio, RejectsTaskReuseAndPreservesPinOnFailedJoin) {
    open();
    ++s_task->epoch;
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_STATE);
    EXPECT_EQ(nx_freertos_mpu_gpio_close(), NX_SUCCESS);
    EXPECT_EQ(nx_freertos_mpu_gpio_join(), NX_ERROR_STATE);
    EXPECT_EQ(s_task->gpio_pinned, 1U);
}
TEST_F(MpuGpio, RejectsForgedHandleBeforeOpeningAuthority) {
    s_task->handle = reinterpret_cast<TaskHandle_t>(uintptr_t{0x1234U});
    EXPECT_EQ(nx_freertos_mpu_gpio_open(), NX_ERROR_STATE);
    EXPECT_EQ(s_task->gpio_pinned, 0U);
}
TEST_F(MpuGpio, ValidatesReadonlyBindingBeforeReadingControlOrGrant) {
    s_layout.privileged_flash = {0x1000U, 0x2000U};
    EXPECT_EQ(nx_freertos_mpu_gpio_open(), NX_ERROR_INVALID);
    EXPECT_EQ(s_task->gpio_pinned, 0U);
}
TEST_F(MpuGpio, RejectsControlOrTaskOutsideProtectedRam) {
    s_layout.privileged_ram.end = s_layout.privileged_ram.begin + 32U;
    EXPECT_EQ(nx_freertos_mpu_gpio_open(), NX_ERROR_INVALID);
    EXPECT_EQ(s_task->gpio_pinned, 0U);
}
TEST_F(MpuGpio, RejectsProviderWhoseLastFieldsEscapePrivilegedRam) {
    s_layout.privileged_ram.end =
        (reinterpret_cast<uintptr_t>(s_provider) + 31U) & ~uintptr_t{31U};
    EXPECT_EQ(nx_freertos_mpu_gpio_open(), NX_ERROR_INVALID);
    EXPECT_EQ(s_task->gpio_pinned, 0U);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, RejectsLayoutDriftBeforeAnEffectAndRetainsThePin) {
    open();
    s_layout_failure = true;
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_freertos_mpu_gpio_join(), NX_ERROR_INVALID);
    EXPECT_EQ(s_task->gpio_pinned, 1U);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, RejectsIsrUnprivilegedSuspendedAndFaultMasks) {
    open();
    ON_CALL(kernel, isr()).WillByDefault(testing::Return(true));
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_CONTEXT);
    ON_CALL(kernel, isr()).WillByDefault(testing::Return(false));
    ON_CALL(kernel, privileged()).WillByDefault(testing::Return(false));
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_CONTEXT);
    ON_CALL(kernel, privileged()).WillByDefault(testing::Return(true));
    ON_CALL(kernel, scheduler())
        .WillByDefault(testing::Return(taskSCHEDULER_SUSPENDED));
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_CONTEXT);
    ON_CALL(kernel, scheduler())
        .WillByDefault(testing::Return(taskSCHEDULER_RUNNING));
    s_masks.faultmask = 1U;
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_CONTEXT);
    s_masks = {0U, configMAX_SYSCALL_INTERRUPT_PRIORITY, 0U};
    EXPECT_EQ(nx_freertos_mpu_gpio_write(3U, 0x80U, 0U), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(MpuGpio, TaskDeletionCannotBypassTheActiveOrClosedGrantPin) {
    open();
    ON_CALL(kernel, current()).WillByDefault(testing::Return(nullptr));
    EXPECT_CALL(kernel, remove(testing::_)).Times(0);
    EXPECT_EQ(nx_freertos_mpu_task_delete(s_task), NX_ERROR_BUSY);
    ASSERT_EQ(nx_freertos_mpu_gpio_close(), NX_SUCCESS);
    EXPECT_EQ(nx_freertos_mpu_task_delete(s_task), NX_ERROR_BUSY);
}
TEST_F(MpuGpio, JoinedTaskDeletionDetachesKernelBeforeClearingHandle) {
    open();
    ASSERT_EQ(nx_freertos_mpu_gpio_close(), NX_SUCCESS);
    ASSERT_EQ(nx_freertos_mpu_gpio_join(), NX_SUCCESS);
    TaskHandle_t handle = s_task->handle;
    ON_CALL(kernel, current()).WillByDefault(testing::Return(nullptr));
    EXPECT_CALL(kernel, remove(handle)).WillOnce(testing::Invoke([&](auto) {
        EXPECT_EQ(s_task->handle, handle);
        EXPECT_EQ(nx_freertos_mpu_gpio_open(), NX_ERROR_STATE);
    }));
    EXPECT_EQ(nx_freertos_mpu_task_delete(s_task), NX_SUCCESS);
    EXPECT_EQ(s_task->handle, nullptr);
    EXPECT_EQ(s_task->epoch, 1U);
}
TEST_F(MpuGpio, SuccessfulStaticTaskRecreationAdvancesItsOwnProtectedEpoch) {
    s_task->handle = nullptr;
    s_task->epoch = 0U;
    char* name = reinterpret_cast<char*>(s_privileged + 24576U);
    std::memcpy(name, "gpio", 5U);
    const nx_freertos_mpu_config_t config = {
        reinterpret_cast<TaskFunction_t>(uintptr_t{0x40000001U}),
        nullptr,
        0U,
        name,
        reinterpret_cast<StackType_t*>(s_user),
        256U,
        1U,
        nullptr,
        0U};
    EXPECT_CALL(kernel, create(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(
            testing::Invoke([](const auto* parameters, auto* output) {
                EXPECT_EQ(parameters->pxTaskBuffer, &s_task->control);
                *output = reinterpret_cast<TaskHandle_t>(&s_task->control);
                return pdPASS;
            }));
    ASSERT_EQ(nx_freertos_mpu_task_start(s_task, &config), NX_SUCCESS);
    EXPECT_EQ(s_task->epoch, 1U);
    ON_CALL(kernel, current()).WillByDefault(testing::Return(nullptr));
    EXPECT_CALL(kernel, remove(s_task->handle)).Times(1);
    ASSERT_EQ(nx_freertos_mpu_task_delete(s_task), NX_SUCCESS);
    ASSERT_EQ(nx_freertos_mpu_task_start(s_task, &config), NX_SUCCESS);
    EXPECT_EQ(s_task->epoch, 2U);
    open();
    EXPECT_EQ(s_control->task_epoch, 2U);
}
TEST_F(MpuGpio, ExhaustedTaskEpochRejectsBeforeKernelCreation) {
    s_task->handle = nullptr;
    s_task->epoch = UINT64_MAX;
    nx_freertos_mpu_config_t config = {};
    EXPECT_CALL(kernel, create(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_freertos_mpu_task_start(s_task, &config), NX_ERROR_OVERFLOW);
    EXPECT_EQ(s_task->epoch, UINT64_MAX);
    EXPECT_EQ(s_task->handle, nullptr);
}
