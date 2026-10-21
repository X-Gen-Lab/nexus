/**
 * \file            native_mapped_storage.c
 * \brief           Independent bounded Native NOR mutation and source detach
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/native_mapped_storage.h"

/** \brief Check representable nonempty spans before comparing their ends. */
static bool separate(const void* a, size_t a_size, const void* b,
                     size_t b_size) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return a != NULL && b != NULL && a_size != 0 && b_size != 0 &&
           a_size <= UINTPTR_MAX - first && b_size <= UINTPTR_MAX - second &&
           (first + a_size <= second || second + b_size <= first);
}

/** \brief Establish only a coherent Native CPU-readable aperture. */
static nx_result_t map(void* context) {
    nx_native_mapped_storage_t* model = context;
    if (model->active || model->mapped) {
        return NX_ERROR_STATE;
    }
    model->mapped = true;
    return NX_SUCCESS;
}

/** \brief All Native consumers have joined before this explicit withdrawal. */
static nx_result_t unmap(void* context) {
    nx_native_mapped_storage_t* model = context;
    if (!model->mapped) {
        return NX_ERROR_STATE;
    }
    model->mapped = false;
    return NX_SUCCESS;
}

/** \brief Retain only the selected input span after unique owner admission. */
static nx_result_t start(void* context, nx_mapped_storage_operation_t operation,
                         size_t offset, const void* data, size_t length) {
    nx_native_mapped_storage_t* model = context;
    if (model->active || model->mapped) {
        return NX_ERROR_STATE;
    }
    model->input = data;
    model->operation = operation;
    model->offset = offset;
    model->length = length;
    model->completed = 0;
    model->active = true;
    model->cancelled = false;
    return NX_SUCCESS;
}

/** \brief Mutate at most the explicit byte quantum, never replaying failures.
 */
static nx_result_t poll(void* context, size_t* completed) {
    nx_native_mapped_storage_t* model = context;
    if (!model->active) {
        return NX_ERROR_STATE;
    }
    if (model->cancelled) {
        *completed = model->completed;
        return NX_ERROR_CANCELLED;
    }
    size_t budget = model->quantum;
    while (budget != 0 && model->completed < model->length) {
        if (model->completed >= model->fault_after) {
            *completed = model->completed;
            return NX_ERROR_IO;
        }
        uint8_t* destination = &model->bytes[model->offset + model->completed];
        if (model->operation == NX_MAPPED_STORAGE_PROGRAM) {
            uint8_t value = model->input[model->completed];
            if ((*destination & value) != value) {
                *completed = model->completed;
                return NX_ERROR_IO;
            }
            *destination = value;
        } else {
            *destination = UINT8_MAX;
        }
        model->completed++;
        budget--;
    }
    *completed = model->completed;
    return model->completed == model->length ? NX_SUCCESS : NX_ERROR_BUSY;
}

/** \brief Native has no independent worker; only owner service can advance. */
static nx_result_t cancel(void* context) {
    nx_native_mapped_storage_t* model = context;
    if (!model->active) {
        return NX_ERROR_STATE;
    }
    model->cancelled = true;
    return NX_SUCCESS;
}

/** \brief Explicitly detach the retained input only after modeled drain. */
static nx_result_t drain(void* context) {
    nx_native_mapped_storage_t* model = context;
    if (model->drain_error) {
        return NX_ERROR_IO;
    }
    if (model->hold_drain) {
        return NX_ERROR_BUSY;
    }
    model->input = NULL;
    model->length = 0;
    model->offset = 0;
    model->active = false;
    model->cancelled = false;
    return NX_SUCCESS;
}

/** \brief Preserve caller bytes while binding a genuinely independent model. */
nx_result_t
nx_native_mapped_storage_init(nx_native_mapped_storage_t* model, uint8_t* bytes,
                              size_t size, size_t program_unit,
                              size_t erase_unit, size_t quantum,
                              nx_mapped_storage_backend_t* backend) {
    if (model == NULL || backend == NULL || size == 0 || program_unit == 0 ||
        erase_unit == 0 || quantum == 0 || size % erase_unit != 0 ||
        erase_unit % program_unit != 0 ||
        !separate(model, sizeof(*model), bytes, size) ||
        !separate(backend, sizeof(*backend), bytes, size) ||
        !separate(model, sizeof(*model), backend, sizeof(*backend))) {
        return NX_ERROR_INVALID;
    }
    *model = (nx_native_mapped_storage_t){.bytes = bytes,
                                          .size = size,
                                          .quantum = quantum,
                                          .fault_after = SIZE_MAX};
    *backend = (nx_mapped_storage_backend_t){
        model,
        sizeof(*model),
        {bytes, size, program_unit, erase_unit, NX_MAPPED_STORAGE_CPU_READ},
        map,
        unmap,
        start,
        poll,
        cancel,
        drain};
    return NX_SUCCESS;
}
