/**
 * \file            freertos_mpu.h
 * \brief           Privileged static lifecycle for isolated MPU user tasks
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_MPU_H
#define NEXUS_OS_FREERTOS_MPU_H

#include "FreeRTOS.h"
#include "nexus/core/status.h"
#include "nexus/os/freertos_user.h"
#include "task.h"

#if configENABLE_MPU != 1 || portUSING_MPU_WRAPPERS != 1
#error "MPU task lifecycle requires an explicit maintained MPU kernel profile"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief User data is nonexecuting normal RAM with only these permissions. */
typedef enum {
    NX_FREERTOS_MPU_READ_ONLY = 0,
    NX_FREERTOS_MPU_READ_WRITE = 1
} nx_freertos_mpu_access_t;

/** \brief A caller-selected exact region, copied during task creation. */
typedef struct {
    void* address;
    size_t bytes;
    nx_freertos_mpu_access_t access;
} nx_freertos_mpu_region_t;

/**
 * \brief           Complete caller-owned protected task metadata.
 * \note            Zero-initialize in privileged RAM. The TCB includes the real
 *                  port's context and syscall stack, inaccessible to users.
 *                  One privileged lifecycle owner serializes start/delete.
 */
typedef struct {
    StaticTask_t control;
    TaskHandle_t handle;
} nx_freertos_mpu_task_t;

/**
 * \brief           Immutable creation inputs, borrowed for this call only.
 * \note            The entry must be explicit user code and must not return.
 *                  Stack and context data remain borrowed until owner deletion;
 *                  regions never include kernel RAM, code or MMIO. Context must
 *                  be contained in the user stack, user flash or mapped data.
 *                  Names terminate within configMAX_TASK_NAME_LEN in a
 *                  readable reserved linker region; names are copied cold.
 */
typedef struct {
    TaskFunction_t entry;
    void* context;
    size_t context_bytes;
    const char* name;
    StackType_t* stack;
    size_t stack_words;
    UBaseType_t priority;
    const nx_freertos_mpu_region_t* regions;
    size_t region_count;
} nx_freertos_mpu_config_t;

/**
 * \brief           Create an unprivileged task from exact caller-owned storage.
 * \param[in,out]   task: Zero/live-free metadata in protected linker RAM.
 * \param[in]       config: Validated user entry, stack and data mappings.
 * \return          Success or INVALID/CONTEXT/STATE/BUSY/IO before admission.
 * \note            Privileged Thread only, scheduler not suspended, no manual
 *                  masks. Rejection retains no references and calls no kernel
 *                  creation API. Actual linker bounds and MPU_TYPE are checked.
 *                  Minimum creation capacity is 64 words; applications budget
 *                  actual execution depth separately. No heap or object pool.
 */
nx_result_t nx_freertos_mpu_task_start(nx_freertos_mpu_task_t* task,
                                       const nx_freertos_mpu_config_t* config)
    PRIVILEGED_FUNCTION;

/**
 * \brief           Delete another isolated task after external users quiesce.
 * \param[in,out]   task: Owned live protected metadata; never the calling task.
 * \return          Success, INVALID, CONTEXT or STATE with storage retained.
 * \note            One privileged lifecycle owner stops external access first.
 *                  Single-core kernel deletion removes scheduler references
 *                  synchronously. Self deletion is rejected; no hidden reaper.
 */
nx_result_t
nx_freertos_mpu_task_delete(nx_freertos_mpu_task_t* task) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_FREERTOS_MPU_H */
