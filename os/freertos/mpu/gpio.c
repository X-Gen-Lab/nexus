/**
 * \file            gpio.c
 * \brief           Protected one-task GPIO admission and lifetime
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/arch/arch.h"
#include "nexus/os/freertos_mpu_gpio.h"
#include "private.h"
#include "stm32u575_gpio_mpu.h"

#if NEXUS_ARCH_MPU_VERSION != 8 || configTOTAL_MPU_REGIONS != 8
#error                                                                         \
    "The scalar GPIO authority requires the selected MPU-v8 eight-region port"
#endif

/** \brief Validate before reading any linked control or readonly capability. */
static bool contains(nx_freertos_mpu_span_t span, uintptr_t address,
                     size_t bytes) {
    return span.begin < span.end && bytes != 0U && address >= span.begin &&
           address < span.end && bytes <= span.end - address;
}

/** \brief All five ranges are exact, pairwise disjoint hardware reservations.
 */
static bool layout_valid(const nx_freertos_mpu_layout_t* layout) {
    const nx_freertos_mpu_span_t spans[] = {
        layout->privileged_flash, layout->privileged_ram, layout->user_flash,
        layout->user_ram, layout->syscall_flash};
    if (layout->version != 8U || layout->regions != 8U) {
        return false;
    }
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (spans[i].begin >= spans[i].end ||
            ((spans[i].begin | spans[i].end) & 31U) != 0U) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (spans[i].begin < spans[j].end &&
                spans[j].begin < spans[i].end) {
                return false;
            }
        }
    }
    return true;
}

/** \brief The one linked binding is authority, never a user-supplied pointer.
 */
static bool binding_valid(const nx_gpio_port_t** validated_port) {
    nx_freertos_mpu_layout_t layout;
    const nx_freertos_mpu_gpio_binding_t* binding =
        &nx_freertos_mpu_gpio_binding;
    if (nx_freertos_mpu_layout(&layout) != NX_SUCCESS ||
        !layout_valid(&layout) ||
        !contains(layout.privileged_flash, (uintptr_t)binding,
                  sizeof(*binding))) {
        return false;
    }
    uintptr_t control = (uintptr_t)binding->control;
    uintptr_t task = (uintptr_t)binding->task;
    if (control % _Alignof(nx_freertos_mpu_gpio_t) != 0U ||
        task % _Alignof(nx_freertos_mpu_task_t) != 0U ||
        !contains(layout.privileged_ram, control, sizeof(*binding->control)) ||
        !contains(layout.privileged_ram, task, sizeof(*binding->task)) ||
        (control < task + sizeof(*binding->task) &&
         task < control + sizeof(*binding->control)) ||
        (uintptr_t)binding->grant % _Alignof(nx_freertos_mpu_gpio_grant_t) !=
            0U ||
        !contains(layout.privileged_flash, (uintptr_t)binding->grant,
                  sizeof(*binding->grant))) {
        return false;
    }
    const nx_freertos_mpu_gpio_grant_t* grant = binding->grant;
    if (grant->mask == 0U || (grant->mask & ~UINT32_C(0xffff)) != 0U ||
        (uintptr_t)grant->port_slot % _Alignof(const nx_gpio_port_t*) != 0U ||
        !contains(layout.privileged_flash, (uintptr_t)grant->port_slot,
                  sizeof(const nx_gpio_port_t*))) {
        return false;
    }
    const nx_gpio_port_t* port = *grant->port_slot;
    if ((uintptr_t)port % _Alignof(nx_gpio_port_t) != 0U ||
        !contains(layout.privileged_flash, (uintptr_t)port, sizeof(*port))) {
        return false;
    }
    if ((uintptr_t)port->ops % _Alignof(nx_gpio_ops_t) != 0U ||
        !contains(layout.privileged_flash, (uintptr_t)port->ops,
                  sizeof(*port->ops))) {
        return false;
    }
    nx_stm32u575_gpio_mpu_bounds_t bounds = {layout.privileged_ram.begin,
                                             layout.privileged_ram.end,
                                             task,
                                             task + sizeof(*binding->task),
                                             control,
                                             control +
                                                 sizeof(*binding->control)};
    if (port->ops->write == NULL ||
        !contains(layout.privileged_flash,
                  (uintptr_t)port->ops->write & ~(uintptr_t)1U, 2U) ||
        !nx_stm32u575_gpio_mpu_validate(port, grant->mask, &bounds)) {
        return false;
    }
    *validated_port = port;
    return true;
}

/** \brief Query the kernel only outside the saved-mask effect boundary. */
static bool context_valid(bool service) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr()) {
        return false;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    BaseType_t scheduler = xTaskGetSchedulerState();
    return masks.primask <= 1U && masks.faultmask == 0U &&
           scheduler != taskSCHEDULER_SUSPENDED &&
           (!service || scheduler == taskSCHEDULER_RUNNING) &&
           (masks.basepri == 0U ||
            (!service && scheduler == taskSCHEDULER_NOT_STARTED &&
             masks.basepri == configMAX_SYSCALL_INTERRUPT_PRIORITY));
}

/** \brief Only the selected actual static TCB can own this incarnation. */
static bool task_live(const nx_freertos_mpu_task_t* task) {
    return task->handle == (TaskHandle_t)&task->control && task->epoch != 0U &&
           task->closing == 0U;
}

/** \brief Match both independently maintained protected incarnation records. */
static bool bound_live(const nx_freertos_mpu_gpio_t* control,
                       const nx_freertos_mpu_task_t* task) {
    return task_live(task) && task->gpio_pinned == 1U &&
           control->handle == task->handle &&
           control->task_epoch == task->epoch;
}

nx_result_t nx_freertos_mpu_gpio_open(void) {
#if defined(__arm__) || defined(__thumb__)
    /* Link-time ABI facts from the retained selected production path. */
    __asm__ volatile(".global __nexus_mpu_gpio_task_size\n"
                     ".equ __nexus_mpu_gpio_task_size,%c0\n"
                     ".global __nexus_mpu_gpio_control_size\n"
                     ".equ __nexus_mpu_gpio_control_size,%c1\n"
                     :
                     : "i"(sizeof(nx_freertos_mpu_task_t)),
                       "i"(sizeof(nx_freertos_mpu_gpio_t)));
#endif
    if (!context_valid(false)) {
        return NX_ERROR_CONTEXT;
    }
    const nx_gpio_port_t* port;
    if (!binding_valid(&port)) {
        return NX_ERROR_INVALID;
    }
    nx_freertos_mpu_gpio_t* control = nx_freertos_mpu_gpio_binding.control;
    nx_freertos_mpu_task_t* task = nx_freertos_mpu_gpio_binding.task;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (control->phase != 0U || task->gpio_pinned != 0U) {
        result = NX_ERROR_BUSY;
    } else if (task_live(task) && control->handle == NULL &&
               control->task_epoch == 0U) {
        control->handle = task->handle;
        control->task_epoch = task->epoch;
        task->gpio_pinned = 1U;
        control->phase = 1U;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}
nx_result_t nx_freertos_mpu_gpio_close(void) {
    if (!context_valid(false)) {
        return NX_ERROR_CONTEXT;
    }
    const nx_gpio_port_t* port;
    if (!binding_valid(&port)) {
        return NX_ERROR_INVALID;
    }
    nx_freertos_mpu_gpio_t* control = nx_freertos_mpu_gpio_binding.control;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (control->phase == 1U || control->phase == 2U) {
        control->phase = 2U;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}
nx_result_t nx_freertos_mpu_gpio_join(void) {
    if (!context_valid(false)) {
        return NX_ERROR_CONTEXT;
    }
    const nx_gpio_port_t* port;
    if (!binding_valid(&port)) {
        return NX_ERROR_INVALID;
    }
    nx_freertos_mpu_gpio_t* control = nx_freertos_mpu_gpio_binding.control;
    nx_freertos_mpu_task_t* task = nx_freertos_mpu_gpio_binding.task;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (control->phase == 2U && bound_live(control, task)) {
        task->gpio_pinned = 0U;
        control->handle = NULL;
        control->task_epoch = 0U;
        control->phase = 0U;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}
nx_result_t nx_freertos_mpu_gpio_write(uint32_t device, uint32_t set,
                                       uint32_t reset) {
    if (!context_valid(true)) {
        return NX_ERROR_CONTEXT;
    }
    const nx_gpio_port_t* port;
    if (!binding_valid(&port)) {
        return NX_ERROR_INVALID;
    }
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    nx_freertos_mpu_gpio_t* control = nx_freertos_mpu_gpio_binding.control;
    nx_freertos_mpu_task_t* task = nx_freertos_mpu_gpio_binding.task;
    const nx_freertos_mpu_gpio_grant_t* grant =
        nx_freertos_mpu_gpio_binding.grant;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (control->phase == 1U && bound_live(control, task)) {
        result = NX_ERROR_PERMISSION;
        if (current == control->handle && device == grant->device) {
            if ((set & reset) != 0U || (set | reset) == 0U) {
                result = NX_ERROR_INVALID;
            } else if (((set | reset) & ~grant->mask) == 0U) {
                /* The selected typed provider performs exactly one bounded
                 * BSRR write. No kernel API, wait, callback policy or lifecycle
                 * change occurs while this final live-epoch check excludes
                 * close/delete.
                 */
                result = nx_gpio_port_write(port, set, reset);
            }
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
