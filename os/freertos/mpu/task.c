/**
 * \file            task.c
 * \brief           Exact MPU task validation before static kernel admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/arch/arch.h"
#include "private.h"

#ifndef NEXUS_ARCH_MPU_VERSION
#error "MPU lifecycle requires explicit architectural MPU facts"
#endif
#ifndef NEXUS_CPU_HAS_BASEPRI
#error "MPU lifecycle requires explicit interrupt-mask facts"
#endif

/** \brief Check an exact span without overflowing its end address. */
static bool contains(nx_freertos_mpu_span_t span, uintptr_t address,
                     size_t bytes) {
    return span.begin < span.end && bytes != 0 && address >= span.begin &&
           address < span.end && bytes <= span.end - address;
}

/** \brief Compare already validated half-open ranges. */
static bool overlaps(uintptr_t first, size_t first_bytes, uintptr_t second,
                     size_t second_bytes) {
    return first < second + second_bytes && second < first + first_bytes;
}

/** \brief Never allow the hardware to widen an admitted mapping. */
static bool exact_region(uintptr_t address, size_t bytes) {
#if NEXUS_ARCH_MPU_VERSION == 7
    return bytes >= 32 && (bytes & (bytes - 1)) == 0 &&
           (address & (bytes - 1)) == 0;
#else
    return bytes >= 32 && (bytes & 31U) == 0 && (address & 31U) == 0;
#endif
}

/** \brief Bootstrap permission mirrors the maintained kernel port's masks. */
static bool task_context_allowed(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return false;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    BaseType_t scheduler = xTaskGetSchedulerState();
    if (masks.faultmask != 0 || scheduler == taskSCHEDULER_SUSPENDED) {
        return false;
    }
#if NEXUS_CPU_HAS_BASEPRI
    return masks.primask == 0 &&
           (masks.basepri == 0 ||
            (masks.basepri == configMAX_SYSCALL_INTERRUPT_PRIORITY &&
             scheduler == taskSCHEDULER_NOT_STARTED));
#else
    return masks.basepri == 0 && masks.primask <= 1 &&
           (masks.primask == 0 || scheduler == taskSCHEDULER_NOT_STARTED);
#endif
}

/** \brief Independently observed hardware must match the selected real port. */
static bool layout_valid(const nx_freertos_mpu_layout_t* layout) {
    if (layout->version != NEXUS_ARCH_MPU_VERSION ||
        layout->regions != configTOTAL_MPU_REGIONS) {
        return false;
    }
    const nx_freertos_mpu_span_t spans[] = {
        layout->privileged_flash, layout->privileged_ram, layout->user_flash,
        layout->user_ram, layout->syscall_flash};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (spans[i].begin >= spans[i].end ||
            !exact_region(spans[i].begin, spans[i].end - spans[i].begin)) {
            return false;
        }
        for (size_t j = 0; j < i; ++j) {
            if (overlaps(spans[i].begin, spans[i].end - spans[i].begin,
                         spans[j].begin, spans[j].end - spans[j].begin)) {
                return false;
            }
        }
    }
    return true;
}

/** \brief Bound kernel name reads to an actual readable linker-owned range. */
static bool name_valid(const nx_freertos_mpu_layout_t* layout,
                       const char* name) {
    if (name == NULL) {
        return false;
    }
    const nx_freertos_mpu_span_t spans[] = {
        layout->privileged_flash, layout->privileged_ram, layout->user_flash,
        layout->user_ram};
    uintptr_t address = (uintptr_t)name;
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (contains(spans[i], address, configMAX_TASK_NAME_LEN)) {
            for (size_t j = 0; j < configMAX_TASK_NAME_LEN; ++j) {
                if (name[j] == '\0') {
                    return true;
                }
            }
        }
    }
    return false;
}

nx_result_t nx_freertos_mpu_task_start(nx_freertos_mpu_task_t* task,
                                       const nx_freertos_mpu_config_t* config) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    nx_freertos_mpu_layout_t layout;
    if (task == NULL || config == NULL ||
        nx_freertos_mpu_layout(&layout) != NX_SUCCESS ||
        !layout_valid(&layout) ||
        ((uintptr_t)task % _Alignof(nx_freertos_mpu_task_t)) != 0 ||
        !contains(layout.privileged_ram, (uintptr_t)task, sizeof(*task))) {
        return NX_ERROR_INVALID;
    }
    if (task->handle != NULL) {
        return NX_ERROR_BUSY;
    }
    uintptr_t stack = (uintptr_t)config->stack;
    uintptr_t entry = (uintptr_t)config->entry & ~(uintptr_t)1U;
    if (config->entry == NULL || !contains(layout.user_flash, entry, 2) ||
        !name_valid(&layout, config->name) || config->stack == NULL ||
        config->stack_words < 64 ||
        config->stack_words >
            (configSTACK_DEPTH_TYPE) ~(configSTACK_DEPTH_TYPE)0 ||
        config->stack_words > SIZE_MAX / sizeof(StackType_t) ||
        config->priority >= configMAX_PRIORITIES ||
        config->region_count > portNUM_CONFIGURABLE_REGIONS ||
        (config->region_count != 0 && config->regions == NULL) ||
        (config->context == NULL) != (config->context_bytes == 0)) {
        return NX_ERROR_INVALID;
    }
    size_t stack_bytes = config->stack_words * sizeof(StackType_t);
    if (!contains(layout.user_ram, stack, stack_bytes) ||
        !exact_region(stack, stack_bytes)) {
        return NX_ERROR_INVALID;
    }
    bool context_valid =
        config->context == NULL ||
        contains((nx_freertos_mpu_span_t){stack, stack + stack_bytes},
                 (uintptr_t)config->context, config->context_bytes) ||
        contains(layout.user_flash, (uintptr_t)config->context,
                 config->context_bytes);
    TaskParameters_t parameters = {.pxTaskBuffer = &task->control};
    for (size_t i = 0; i < config->region_count; ++i) {
        const nx_freertos_mpu_region_t* region = &config->regions[i];
        uintptr_t address = (uintptr_t)region->address;
        if (!contains(layout.user_ram, address, region->bytes) ||
            !exact_region(address, region->bytes) ||
            region->bytes > UINT32_MAX ||
            (region->access != NX_FREERTOS_MPU_READ_ONLY &&
             region->access != NX_FREERTOS_MPU_READ_WRITE) ||
            overlaps(address, region->bytes, stack, stack_bytes)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0; j < i; ++j) {
            if (overlaps(address, region->bytes,
                         (uintptr_t)config->regions[j].address,
                         config->regions[j].bytes)) {
                return NX_ERROR_INVALID;
            }
        }
        context_valid |=
            contains((nx_freertos_mpu_span_t){address, address + region->bytes},
                     (uintptr_t)config->context, config->context_bytes);
        parameters.xRegions[i].pvBaseAddress = region->address;
        parameters.xRegions[i].ulLengthInBytes = (uint32_t)region->bytes;
#if NEXUS_ARCH_MPU_VERSION == 7
        parameters.xRegions[i].ulParameters =
            portMPU_REGION_EXECUTE_NEVER | portMPU_REGION_CACHEABLE_BUFFERABLE |
            (region->access == NX_FREERTOS_MPU_READ_ONLY
                 ? portMPU_REGION_READ_ONLY
                 : portMPU_REGION_READ_WRITE);
#else
        parameters.xRegions[i].ulParameters =
            tskMPU_REGION_EXECUTE_NEVER | tskMPU_REGION_NORMAL_MEMORY |
            (region->access == NX_FREERTOS_MPU_READ_ONLY
                 ? tskMPU_REGION_READ_ONLY
                 : tskMPU_REGION_READ_WRITE);
#endif
    }
    if (!context_valid) {
        return NX_ERROR_INVALID;
    }
    parameters.pvTaskCode = config->entry;
    parameters.pcName = config->name;
    parameters.usStackDepth = (configSTACK_DEPTH_TYPE)config->stack_words;
    parameters.pvParameters = config->context;
    parameters.uxPriority = config->priority;
    parameters.puxStackBuffer = config->stack;
    TaskHandle_t handle = NULL;
    if (xTaskCreateRestrictedStatic(&parameters, &handle) != pdPASS ||
        handle == NULL) {
        return NX_ERROR_IO;
    }
    task->handle = handle;
    return NX_SUCCESS;
}

nx_result_t nx_freertos_mpu_task_delete(nx_freertos_mpu_task_t* task) {
    if (!task_context_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    nx_freertos_mpu_layout_t layout;
    if (task == NULL || nx_freertos_mpu_layout(&layout) != NX_SUCCESS ||
        !layout_valid(&layout) ||
        ((uintptr_t)task % _Alignof(nx_freertos_mpu_task_t)) != 0 ||
        !contains(layout.privileged_ram, (uintptr_t)task, sizeof(*task))) {
        return NX_ERROR_INVALID;
    }
    if (task->handle == NULL || task->handle == xTaskGetCurrentTaskHandle()) {
        return NX_ERROR_STATE;
    }
    vTaskDelete(task->handle);
    task->handle = NULL;
    return NX_SUCCESS;
}
