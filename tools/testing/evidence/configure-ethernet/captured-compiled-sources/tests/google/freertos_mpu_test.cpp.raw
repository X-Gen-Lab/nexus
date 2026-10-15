/**
 * \file            freertos_mpu_test.cpp
 * \brief           Real restricted-task adapter rejects unsafe mappings cold
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
extern "C" {
#include "nexus/arch/arch.h"
#include "private.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

/** \brief Explicit user code placed outside ordinary privileged code. */
static void user_entry(void*) NX_FREERTOS_USER_CODE;
static void user_entry(void*) {
}

class MpuKernel {
  public:
    MOCK_METHOD(bool, privileged, ());
    MOCK_METHOD(bool, isr, ());
    MOCK_METHOD(nx_arch_irq_masks_t, masks, ());
    MOCK_METHOD(BaseType_t, scheduler, ());
    MOCK_METHOD(nx_result_t, layout, (nx_freertos_mpu_layout_t*));
    MOCK_METHOD(BaseType_t, create, (const TaskParameters_t*, TaskHandle_t*));
    MOCK_METHOD(TaskHandle_t, current, ());
    MOCK_METHOD(void, remove, (TaskHandle_t));
};

static MpuKernel* s_kernel;
extern "C" bool nx_arch_is_privileged(void) {
    return s_kernel->privileged();
}
extern "C" bool nx_arch_in_isr(void) {
    return s_kernel->isr();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return s_kernel->masks();
}
extern "C" BaseType_t xTaskGetSchedulerState(void) {
    return s_kernel->scheduler();
}
extern "C" nx_result_t
nx_freertos_mpu_layout(nx_freertos_mpu_layout_t* layout) {
    return s_kernel->layout(layout);
}
extern "C" BaseType_t
xTaskCreateRestrictedStatic(const TaskParameters_t* const config,
                            TaskHandle_t* task) {
    return s_kernel->create(config, task);
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return s_kernel->current();
}
extern "C" void vTaskDelete(TaskHandle_t task) {
    s_kernel->remove(task);
}

class FreeRtosMpu : public testing::Test {
  protected:
    testing::NiceMock<MpuKernel> kernel;
    alignas(4096) uint8_t protected_ram[4096] = {};
    alignas(4096) uint8_t user_ram[4096] = {};
    nx_freertos_mpu_task_t* task =
        reinterpret_cast<nx_freertos_mpu_task_t*>(protected_ram);
    nx_freertos_mpu_region_t region[2] = {};
    nx_freertos_mpu_config_t config = {};
    nx_freertos_mpu_layout_t memory = {};
    TaskHandle_t handle = reinterpret_cast<TaskHandle_t>(uintptr_t{0x1234U});

    void SetUp() override {
        s_kernel = &kernel;
        static_assert(sizeof(nx_freertos_mpu_task_t) < sizeof(protected_ram));
        const uintptr_t code = reinterpret_cast<uintptr_t>(user_entry);
        memory = {{0x1000U, 0x2000U},
                  {reinterpret_cast<uintptr_t>(protected_ram),
                   reinterpret_cast<uintptr_t>(protected_ram) +
                       sizeof(protected_ram)},
                  {code & ~static_cast<uintptr_t>(4095U),
                   (code & ~static_cast<uintptr_t>(4095U)) + 4096U},
                  {reinterpret_cast<uintptr_t>(user_ram),
                   reinterpret_cast<uintptr_t>(user_ram) + sizeof(user_ram)},
                  {0x3000U, 0x4000U},
                  NEXUS_ARCH_MPU_VERSION,
                  8U};
        char* name = reinterpret_cast<char*>(protected_ram + 3072U);
        std::memcpy(name, "isolated", 9U);
        region[0] = {user_ram + 1024U, 256U, NX_FREERTOS_MPU_READ_WRITE};
        config = {user_entry,
                  user_ram + 1024U,
                  16U,
                  name,
                  reinterpret_cast<StackType_t*>(user_ram),
                  256U,
                  2U,
                  region,
                  1U};
        ON_CALL(kernel, privileged()).WillByDefault(testing::Return(true));
        ON_CALL(kernel, isr()).WillByDefault(testing::Return(false));
        ON_CALL(kernel, masks())
            .WillByDefault(testing::Return(nx_arch_irq_masks_t{0U, 0U, 0U}));
        ON_CALL(kernel, scheduler())
            .WillByDefault(testing::Return(taskSCHEDULER_RUNNING));
        ON_CALL(kernel, layout(testing::_))
            .WillByDefault(testing::Invoke([this](auto* output) {
                *output = memory;
                return NX_SUCCESS;
            }));
        ON_CALL(kernel, current())
            .WillByDefault(testing::Return(
                reinterpret_cast<TaskHandle_t>(uintptr_t{0x5678U})));
    }
    void TearDown() override {
        s_kernel = nullptr;
    }
    void expect_unchanged() {
        EXPECT_EQ(task->handle, nullptr);
        const StaticTask_t empty = {};
        EXPECT_EQ(std::memcmp(&task->control, &empty, sizeof(empty)), 0);
    }
};

TEST_F(FreeRtosMpu, CreatesActualRestrictedStaticTaskWithProtectedCompleteTcb) {
    EXPECT_CALL(kernel, create(testing::_, testing::_))
        .WillOnce(testing::Invoke([this](const auto* input, auto* output) {
            EXPECT_EQ(input->pvTaskCode, user_entry);
            EXPECT_EQ(input->puxStackBuffer, config.stack);
            EXPECT_EQ(input->pxTaskBuffer, &task->control);
            EXPECT_EQ(input->usStackDepth, 256U);
            EXPECT_EQ(input->uxPriority, 2U);
            EXPECT_EQ(input->xRegions[0].pvBaseAddress, region[0].address);
            EXPECT_EQ(input->xRegions[0].ulLengthInBytes, 256U);
#if NEXUS_ARCH_MPU_VERSION == 7
            EXPECT_NE(input->xRegions[0].ulParameters &
                          portMPU_REGION_EXECUTE_NEVER,
                      0U);
#else
            EXPECT_NE(input->xRegions[0].ulParameters &
                          tskMPU_REGION_EXECUTE_NEVER,
                      0U);
#endif
            EXPECT_EQ(input->xRegions[1].ulLengthInBytes, 0U);
            *output = handle;
            return pdPASS;
        }));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_SUCCESS);
    EXPECT_EQ(task->handle, handle);
    EXPECT_CALL(kernel, create(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_BUSY);
}

class InvalidMpuConfig : public FreeRtosMpu,
                         public testing::WithParamInterface<int> {};

TEST_P(InvalidMpuConfig, RejectsWithoutKernelCreationOrMetadataMutation) {
    switch (GetParam()) {
        case 0:
            config.entry = nullptr;
            break;
        case 1:
            config.name = nullptr;
            break;
        case 2:
            config.context = nullptr;
            break;
        case 3:
            config.context_bytes = 0U;
            break;
        case 4:
            config.stack = nullptr;
            break;
        case 5:
            config.stack_words = 63U;
            break;
        case 6:
            config.stack_words = 255U;
            break;
        case 7:
            config.stack = reinterpret_cast<StackType_t*>(user_ram + 4U);
            break;
        case 8:
            config.priority = configMAX_PRIORITIES;
            break;
        case 9:
            config.regions = nullptr;
            break;
        case 10:
            config.region_count = portNUM_CONFIGURABLE_REGIONS + 1U;
            break;
        case 11:
            region[0].address = nullptr;
            break;
        case 12:
            region[0].bytes = 0U;
            break;
        case 13:
            region[0].bytes = 255U;
            break;
        case 14:
            region[0].address = user_ram + 1028U;
            break;
        case 15:
            region[0].address = protected_ram;
            break;
        case 16:
            region[0].address = user_ram;
            break;
        case 17:
            config.context = protected_ram;
            break;
        case 18:
            /* Supply an invalid wire representation without a C++ enum cast. */
            std::memset(&region[0].access, 0xff, sizeof(region[0].access));
            break;
        case 19:
            region[1] = region[0];
            config.region_count = 2U;
            break;
        case 20:
            config.stack_words = SIZE_MAX;
            break;
        case 21:
            config.context_bytes = SIZE_MAX;
            break;
        case 22:
            config.stack = reinterpret_cast<StackType_t*>(protected_ram);
            break;
        case 23:
            region[0].address = reinterpret_cast<void*>(uintptr_t{0x40000000U});
            break;
        default:
            FAIL();
    }
    EXPECT_CALL(kernel, create(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_INVALID);
    expect_unchanged();
}

INSTANTIATE_TEST_SUITE_P(ExactUserDomain, InvalidMpuConfig,
                         testing::Range(0, 24));

TEST_F(FreeRtosMpu, RejectsCallerContextAndSuspendedSchedulerBeforeAdmission) {
    EXPECT_CALL(kernel, create(testing::_, testing::_)).Times(0);
    ON_CALL(kernel, privileged()).WillByDefault(testing::Return(false));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_CONTEXT);
    ON_CALL(kernel, privileged()).WillByDefault(testing::Return(true));
    ON_CALL(kernel, isr()).WillByDefault(testing::Return(true));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_CONTEXT);
    ON_CALL(kernel, isr()).WillByDefault(testing::Return(false));
    ON_CALL(kernel, masks())
        .WillByDefault(testing::Return(nx_arch_irq_masks_t{1U, 0U, 0U}));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_CONTEXT);
    ON_CALL(kernel, masks())
        .WillByDefault(testing::Return(nx_arch_irq_masks_t{0U, 0U, 0U}));
    ON_CALL(kernel, scheduler())
        .WillByDefault(testing::Return(taskSCHEDULER_SUSPENDED));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_CONTEXT);
    expect_unchanged();
}

TEST_F(FreeRtosMpu, RejectsUnexpectedMpuHardwareAndUnprotectedMetadata) {
    EXPECT_CALL(kernel, create(testing::_, testing::_)).Times(0);
    memory.regions = 4U;
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_INVALID);
    memory.regions = 8U;
    memory.version = 0U;
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_INVALID);
    memory.version = NEXUS_ARCH_MPU_VERSION;
    auto* unprotected = reinterpret_cast<nx_freertos_mpu_task_t*>(user_ram);
    EXPECT_EQ(nx_freertos_mpu_task_start(unprotected, &config),
              NX_ERROR_INVALID);
    expect_unchanged();
}

TEST_F(FreeRtosMpu, DeletesAnotherTaskSynchronouslyAndRejectsSelfDeletion) {
    task->handle = handle;
    EXPECT_CALL(kernel, remove(handle)).Times(1);
    EXPECT_EQ(nx_freertos_mpu_task_delete(task), NX_SUCCESS);
    EXPECT_EQ(task->handle, nullptr);
    EXPECT_EQ(nx_freertos_mpu_task_delete(task), NX_ERROR_STATE);
    task->handle = handle;
    ON_CALL(kernel, current()).WillByDefault(testing::Return(handle));
    EXPECT_EQ(nx_freertos_mpu_task_delete(task), NX_ERROR_STATE);
    EXPECT_EQ(task->handle, handle);
}

TEST_F(FreeRtosMpu, AcceptsExactReadOnlyDataAndNoContextWithoutExtraMapping) {
    region[0].access = NX_FREERTOS_MPU_READ_ONLY;
    config.context = nullptr;
    config.context_bytes = 0;
    EXPECT_CALL(kernel, create(testing::_, testing::_))
        .WillOnce(testing::Invoke([this](const auto* input, auto* output) {
#if NEXUS_ARCH_MPU_VERSION == 7
            EXPECT_EQ(input->xRegions[0].ulParameters & (0x7UL << 24),
                      portMPU_REGION_READ_ONLY);
#else
            EXPECT_NE(input->xRegions[0].ulParameters & tskMPU_REGION_READ_ONLY,
                      0U);
            EXPECT_EQ(
                input->xRegions[0].ulParameters & tskMPU_REGION_READ_WRITE, 0U);
#endif
            *output = handle;
            return pdPASS;
        }));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_SUCCESS);
}

TEST_F(FreeRtosMpu, KernelAdmissionFailureRetainsNoHandle) {
    EXPECT_CALL(kernel, create(testing::_, testing::_))
        .WillOnce(testing::Return(errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_ERROR_IO);
    expect_unchanged();
}

#if NEXUS_ARCH_MPU_VERSION == 8
TEST_F(FreeRtosMpu, V8AcceptsGranularRegionWithoutPowerOfTwoWidening) {
    region[0].bytes = 96;
    EXPECT_CALL(kernel, create(testing::_, testing::_))
        .WillOnce(testing::Invoke([this](const auto* input, auto* output) {
            EXPECT_EQ(input->xRegions[0].ulLengthInBytes, 96U);
            *output = handle;
            return pdPASS;
        }));
    EXPECT_EQ(nx_freertos_mpu_task_start(task, &config), NX_SUCCESS);
}
#endif
