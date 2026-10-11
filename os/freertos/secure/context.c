/**
 * \file            context.c
 * \brief           Explicit Secure stack leases without a heap or context pool
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/secure_context.h"
#include "private/hardware.h"
#include "secure_context.h"
#include <string.h>

_Static_assert(configENABLE_MPU == 0,
               "Combined user MPU and split worlds require a reviewed port");
static bool s_initialized;
/* One CPU has exactly one loaded Secure PSP. This is an active borrow, not an
 * object pool; records and stacks are supplied by immutable app composition. */
static nx_freertos_secure_context_t* s_loaded;

void SecureContext_LoadContextAsm(SecureContext_t* context);
void SecureContext_SaveContextAsm(SecureContext_t* context);

__attribute__((weak)) nx_freertos_secure_context_t*
nx_freertos_secure_context_for_task(void* task) {
    (void)task;
    return NULL;
}

__attribute__((weak, noreturn)) void
nx_freertos_secure_fault(nx_freertos_secure_fault_t fault) {
    (void)fault;
    nx_secure_hw_set_primask(1);
    nx_secure_hw_set_primask_ns(1);
    nx_arch_dsb();
    nx_arch_isb();
    for (;;) {
    }
}

/** \brief Check an actual supplied span before adding its upper boundary. */
static bool valid_span(void* pointer, size_t bytes) {
    return pointer != NULL && bytes != 0 &&
           (uintptr_t)pointer <= UINTPTR_MAX - (bytes - 1u);
}

nx_result_t
nx_freertos_secure_context_prepare(nx_freertos_secure_context_t* context,
                                   void* stack, size_t bytes, void* task) {
    if (!valid_span(context, sizeof(*context)) || !valid_span(stack, bytes) ||
        task == NULL || bytes < NX_FREERTOS_SECURE_MIN_STACK_BYTES + 8u ||
        bytes % 8u != 0 || (uintptr_t)stack % 8u != 0 ||
        (uintptr_t)context % _Alignof(nx_freertos_secure_context_t) != 0) {
        return NX_ERROR_INVALID;
    }
    uintptr_t context_first = (uintptr_t)context;
    uintptr_t context_last = context_first + sizeof(*context) - 1u;
    uintptr_t stack_first = (uintptr_t)stack;
    uintptr_t stack_last = stack_first + bytes - 1u;
    if (context_first <= stack_last && stack_first <= context_last) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_secure_hw_writable(context, sizeof(*context)) ||
        !nx_secure_hw_writable(stack, bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    nx_result_t result = NX_ERROR_STATE;
    if (!context->prepared) {
        nx_freertos_secure_context_t prepared = {stack, bytes, task, NULL, NULL,
                                                 NULL,  0,     true, false};
        *context = prepared;
        result = NX_SUCCESS;
    }
    nx_secure_masks_restore(saved);
    return result;
}

nx_result_t
nx_freertos_secure_context_rebind(nx_freertos_secure_context_t* context,
                                  void* task) {
    if (!valid_span(context, sizeof(*context)) || task == NULL ||
        (uintptr_t)context % _Alignof(nx_freertos_secure_context_t) != 0) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_secure_hw_writable(context, sizeof(*context))) {
        return NX_ERROR_INVALID;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    nx_result_t result = NX_ERROR_STATE;
    if (context->prepared) {
        result = NX_ERROR_BUSY;
        if (!context->leased && s_loaded != context) {
            context->task = task;
            result = NX_SUCCESS;
        }
    }
    nx_secure_masks_restore(saved);
    return result;
}

/** \brief Resolve only trusted metadata matching this compound identity. */
static nx_freertos_secure_context_t* resolve_lease(uint32_t handle,
                                                   void* task) {
    nx_freertos_secure_context_t* context =
        nx_freertos_secure_context_for_task(task);
    if (context == NULL || !context->prepared || !context->leased ||
        context->task != task || handle == 0 || context->epoch != handle) {
        return NULL;
    }
    return context;
}

/** \brief Hold both masks when a live borrowed stack is damaged. */
static void check_integrity(const nx_freertos_secure_context_t* context) {
    uint32_t seal[2];
    memcpy(seal, __builtin_assume_aligned(context->top, 8), sizeof(seal));
    if (seal[0] != 0xfef5eda5u || seal[1] != 0xfef5eda5u) {
        nx_freertos_secure_fault(NX_FREERTOS_SECURE_SEAL_FAILURE);
    }
}

NX_SECURE_GATEWAY void SecureContext_Init(void) {
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 11u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    if (!s_initialized) {
        nx_secure_hw_set_psplim(0);
        nx_secure_hw_set_psp(0);
        nx_secure_hw_set_control(2);
        nx_arch_isb();
        s_initialized = true;
    }
    nx_secure_masks_restore(saved);
}

NX_SECURE_GATEWAY SecureContextHandle_t
SecureContext_AllocateContext(uint32_t stack_bytes, void* task) {
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 11u ||
        stack_bytes < NX_FREERTOS_SECURE_MIN_STACK_BYTES ||
        stack_bytes % 8u != 0 || task == NULL) {
        return 0;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    uint32_t handle = 0;
    nx_freertos_secure_context_t* context =
        nx_freertos_secure_context_for_task(task);
    if (s_initialized && s_loaded == NULL && nx_secure_hw_psplim() == 0 &&
        nx_secure_hw_psp() == 0 && context != NULL && context->prepared &&
        context->task == task && !context->leased &&
        stack_bytes <= context->stack_bytes - 8u &&
        context->epoch != UINT32_MAX) {
        context->top = context->stack + context->stack_bytes - 8u;
        context->limit = context->top - stack_bytes;
        context->saved_sp = context->top;
        const uint32_t seal[2] = {0xfef5eda5u, 0xfef5eda5u};
        /* Character storage is valid caller stack storage. Copy fixed words
         * without aliasing that declared byte array as a uint32_t object. */
        memcpy(__builtin_assume_aligned(context->top, 8), seal, sizeof(seal));
        ++context->epoch;
        context->leased = true;
        handle = context->epoch;
    }
    nx_secure_masks_restore(saved);
    return handle;
}

NX_SECURE_GATEWAY void SecureContext_FreeContext(SecureContextHandle_t handle,
                                                 void* task) {
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 11u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    nx_freertos_secure_context_t* context = resolve_lease(handle, task);
    if (context != NULL && s_loaded != context) {
        check_integrity(context);
        context->leased = false;
        context->saved_sp = NULL;
        context->limit = NULL;
        context->top = NULL;
    }
    nx_secure_masks_restore(saved);
}

NX_SECURE_GATEWAY void SecureContext_LoadContext(SecureContextHandle_t handle,
                                                 void* task) {
    if (!NX_SECURE_NONSECURE_CALLER()) {
        return;
    }
    uint32_t exception = nx_arch_exception_number();
    if (exception != 11u && exception != 14u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    nx_freertos_secure_context_t* context = resolve_lease(handle, task);
    if (context != NULL && s_loaded == NULL && nx_secure_hw_psplim() == 0 &&
        nx_secure_hw_psp() == 0) {
        check_integrity(context);
        if ((uintptr_t)context->saved_sp < (uintptr_t)context->limit ||
            (uintptr_t)context->saved_sp > (uintptr_t)context->top ||
            (uintptr_t)context->saved_sp % 8u != 0) {
            nx_freertos_secure_fault(NX_FREERTOS_SECURE_STACK_FAILURE);
        }
        /* Assembly owns the PSP/PSPLIM register transfer. A local exact ABI
         * record avoids aliasing the public Nexus metadata as a vendor type. */
        SecureContext_t port = {context->saved_sp, context->limit, context->top,
                                task};
        SecureContext_LoadContextAsm(&port);
        s_loaded = context;
    }
    nx_secure_masks_restore(saved);
}

NX_SECURE_GATEWAY void SecureContext_SaveContext(SecureContextHandle_t handle,
                                                 void* task) {
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 14u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    nx_freertos_secure_context_t* context = resolve_lease(handle, task);
    if (context != NULL && s_loaded == context) {
        check_integrity(context);
        uintptr_t stack = nx_secure_hw_psp();
        if (nx_secure_hw_psplim() != (uintptr_t)context->limit ||
            stack < (uintptr_t)context->limit ||
            stack > (uintptr_t)context->top || stack % 8u != 0) {
            nx_freertos_secure_fault(NX_FREERTOS_SECURE_STACK_FAILURE);
        }
        SecureContext_t port = {context->saved_sp, context->limit, context->top,
                                task};
        SecureContext_SaveContextAsm(&port);
        if ((uintptr_t)port.pucCurrentStackPointer <
                (uintptr_t)context->limit ||
            (uintptr_t)port.pucCurrentStackPointer > (uintptr_t)context->top ||
            (uintptr_t)port.pucCurrentStackPointer % 8u != 0) {
            nx_freertos_secure_fault(NX_FREERTOS_SECURE_STACK_FAILURE);
        }
        context->saved_sp = port.pucCurrentStackPointer;
        s_loaded = NULL;
    }
    nx_secure_masks_restore(saved);
}

#ifdef NEXUS_FREERTOS_SECURE_MODEL
void nx_secure_model_reset(void) {
    s_initialized = false;
    s_loaded = NULL;
}
#endif
