/**
 * \file            qspi_mapped.c
 * \brief           CPU-only mapped loans and exclusive QSPI mutation bridge
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_qspi_mapped.h"
#include <stddef.h>

_Static_assert(offsetof(nx_stm32_qspi_mapped_t, controller) == 0U,
               "One hardware owner is the composite prefix");

/** \brief Prove exact metadata spans before inspecting pointed-to fields. */
static bool extent(const void* p, size_t bytes, size_t alignment, bool write) {
    uintptr_t begin = (uintptr_t)p;
    if (p == NULL || begin % alignment != 0U || bytes == 0U ||
        bytes > UINTPTR_MAX - begin)
        return false;
    uint64_t first = begin, end = first + bytes;
    return !(first < UINT64_C(0x100000000) && UINT64_C(0x40000000) < end) &&
           (!write ||
            !(first < UINT64_C(0x10000000) && UINT64_C(0x08000000) < end));
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
nx_result_t nx_stm32_qspi_mapped_admit(const nx_stm32_qspi_t* owner,
                                       const nx_stm32_qspi_config_t* config) {
    const nx_stm32_qspi_mapped_config_t* map = config->mapping;
    if (config->dma != NULL ||
        !extent(owner, sizeof(nx_stm32_qspi_mapped_t),
                _Alignof(nx_stm32_qspi_mapped_t), true) ||
        !extent(map, sizeof(*map), _Alignof(nx_stm32_qspi_mapped_config_t),
                false) ||
        map->count == 0U || map->count > 8U ||
        !extent(map->owner, sizeof(*map->owner),
                _Alignof(nx_mapped_storage_owner_t), true) ||
        !extent(map->slots, map->count * sizeof(*map->slots),
                _Alignof(nx_mapped_storage_slot_t), true))
        return NX_ERROR_INVALID;
    const void* objects[] = {owner, config,     config->system, config->pins,
                             map,   map->owner, map->slots};
    const size_t sizes[] = {sizeof(nx_stm32_qspi_mapped_t),
                            sizeof(*config),
                            sizeof(*config->system),
                            config->pin_count * sizeof(*config->pins),
                            sizeof(*map),
                            sizeof(*map->owner),
                            map->count * sizeof(*map->slots)};
    for (size_t i = 0U; i < sizeof(objects) / sizeof(objects[0]); ++i) {
        for (size_t j = i + 1U; j < sizeof(objects) / sizeof(objects[0]); ++j)
            if (overlap(objects[i], sizes[i], objects[j], sizes[j]))
                return NX_ERROR_INVALID;
        for (size_t pin = 0U; pin < config->pin_count; ++pin)
            if (overlap(objects[i], sizes[i], config->pins[pin].snapshot,
                        sizeof(uint16_t)))
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
static nx_time_us_t clock_read(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    return nx_stm32_system_clock_read(map->controller.config->system);
}
static nx_mapped_storage_map_result_t map_read(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_mapped_storage_map_result_t result =
        nx_stm32_qspi_mapping_enter(&map->controller);
    map->mapping_active = result.cleanup_required;
    return result;
}
static nx_result_t unmap(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = nx_stm32_qspi_mapping_leave(&map->controller);
    if (result == NX_SUCCESS)
        map->mapping_active = false;
    return result;
}
static nx_result_t start(void* context, nx_mapped_storage_operation_t operation,
                         size_t offset, const void* data, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32_qspi_mapped_t* map = context;
    if (map->mapping_active || map->inner_live)
        return NX_ERROR_STATE;
    nx_result_t result = nx_flash_async_prepare(
        &map->inner,
        operation == NX_MAPPED_STORAGE_PROGRAM ? NX_FLASH_ASYNC_PROGRAM
                                               : NX_FLASH_ASYNC_ERASE,
        (uint32_t)offset, data, length, deadline);
    if (result == NX_SUCCESS)
        result = nx_stm32_qspi_async_ops.submit(&map->controller, &map->inner);
    map->inner_live = result == NX_SUCCESS;
    map->inner_result = result;
    map->inner_completed = 0U;
    return result;
}
/** \brief Only acquire-observed inner settlement permits outer publication. */
static nx_result_t poll(void* context, size_t* completed) {
    nx_stm32_qspi_mapped_t* map = context;
    if (!map->inner_live) {
        *completed = map->inner_completed;
        return map->inner_result;
    }
    (void)nx_stm32_qspi_async_ops.service(&map->controller);
    if (nx_request_state(&map->inner.base) != NX_REQUEST_SETTLED) {
        *completed = map->controller.completed;
        return nx_request_state(&map->inner.base) == NX_REQUEST_QUARANTINED
                   ? NX_ERROR_IO
                   : NX_ERROR_BUSY;
    }
    nx_result_t status = nx_request_result(&map->inner.base, &map->inner_result,
                                           &map->inner_completed);
    if (status != NX_SUCCESS)
        return status;
    map->inner_live = false;
    *completed = map->inner_completed;
    return map->inner_result;
}
static nx_result_t cancel(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    return map->inner_live
               ? nx_stm32_qspi_async_ops.cancel(&map->controller, &map->inner)
               : NX_SUCCESS;
}
static nx_result_t drain(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    size_t completed = 0U;
    if (map->inner_live) {
        nx_result_t result = poll(context, &completed);
        if (map->inner_live)
            return result == NX_ERROR_BUSY ? NX_ERROR_BUSY : NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
nx_result_t
nx_stm32_qspi_mapped_prepare(nx_stm32_qspi_t* owner,
                             const nx_stm32_qspi_mapped_config_t* config) {
    nx_stm32_qspi_mapped_t* map = (nx_stm32_qspi_mapped_t*)owner;
    if (map->constructed)
        return map->binding != config || map->inner_live || map->mapping_active
                   ? NX_ERROR_STATE
                   : nx_mapped_storage_owner_reopen(config->owner);
    nx_mapped_storage_backend_t backend = {
        map,
        sizeof(*map),
        {(const uint8_t*)(uintptr_t)0x90000000U, 0x1000000U, 1U, 4096U,
         NX_MAPPED_STORAGE_CPU_READ},
        map_read,
        unmap,
        start,
        poll,
        cancel,
        drain};
    nx_result_t result = nx_mapped_storage_owner_init(
        config->owner, config->slots, config->count, backend,
        (nx_clock_t){clock_read, map});
    if (result == NX_SUCCESS) {
        map->binding = config;
        nx_request_initialize(&map->inner.base);
        map->constructed = true;
    }
    return result;
}
static nx_result_t ready(nx_stm32_qspi_mapped_t* map) {
    /* Validate the composite prefix before forming any member address. */
    nx_result_t result =
        nx_stm32_qspi_mapping_ready((const nx_stm32_qspi_t*)map);
    return result != NX_SUCCESS ? result
           : !map->constructed  ? NX_ERROR_STATE
                                : NX_SUCCESS;
}
static const nx_mapped_storage_geometry_t* geometry(const void* context) {
    const nx_stm32_qspi_mapped_t* map = context;
    return map->constructed ? &map->binding->owner->backend.geometry : NULL;
}
static nx_result_t acquire(void* context, nx_mapped_storage_reader_t reader,
                           size_t offset, size_t length,
                           nx_mapped_storage_loan_t* loan) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_qspi_mapping_external(&map->controller, loan, sizeof(*loan),
                                        true))
        return NX_ERROR_INVALID;
    return nx_mapped_storage_owner_ops.acquire(map->binding->owner, reader,
                                               offset, length, loan);
}
static nx_result_t release(void* context,
                           const nx_mapped_storage_loan_t* loan) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_qspi_mapping_external(&map->controller, loan, sizeof(*loan),
                                        false))
        return NX_ERROR_INVALID;
    return nx_mapped_storage_owner_ops.release(map->binding->owner, loan);
}
static nx_result_t submit(void* context, nx_mapped_storage_request_t* request,
                          nx_mapped_storage_ticket_t* ticket) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32_qspi_mapping_external(&map->controller, request,
                                        sizeof(*request), true) ||
        !nx_stm32_qspi_mapping_external(&map->controller, ticket,
                                        sizeof(*ticket), true))
        return NX_ERROR_INVALID;
    if (request->base.deadline == NX_DEADLINE_NEVER ||
        (request->operation == NX_MAPPED_STORAGE_PROGRAM &&
         !nx_stm32_qspi_mapping_external(&map->controller, request->data,
                                         request->length, false)))
        return NX_ERROR_INVALID;
    return nx_mapped_storage_owner_ops.submit(map->binding->owner, request,
                                              ticket);
}
static nx_result_t cancel_outer(void* context,
                                nx_mapped_storage_ticket_t ticket) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    return result == NX_SUCCESS
               ? nx_mapped_storage_owner_ops.cancel(map->binding->owner, ticket)
               : result;
}
static void service(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    /* A failed clock cannot suspend the already accepted cleanup protocol. */
    if (result == NX_SUCCESS || (result == NX_ERROR_STATE && map->constructed &&
                                 map->controller.config != NULL))
        nx_mapped_storage_owner_ops.service(map->binding->owner);
}
static nx_result_t stop(void* context) {
    nx_stm32_qspi_mapped_t* map = context;
    nx_result_t result = ready(map);
    return result == NX_SUCCESS ? nx_stm32_qspi_mapped_stop(&map->controller)
                                : result;
}
nx_result_t nx_stm32_qspi_mapped_stop(nx_stm32_qspi_t* owner) {
    nx_stm32_qspi_mapped_t* map = (nx_stm32_qspi_mapped_t*)owner;
    if (!map->constructed)
        return NX_SUCCESS;
    nx_result_t result = nx_mapped_storage_owner_ops.stop(map->binding->owner);
    if (result == NX_ERROR_BUSY && map->inner_live) {
        nx_mapped_storage_owner_ops.service(map->binding->owner);
        result = nx_mapped_storage_owner_ops.stop(map->binding->owner);
    }
    return result;
}
const nx_mapped_storage_ops_t nx_stm32_qspi_mapped_ops = {
    geometry, acquire, release, submit, cancel_outer, service, stop};
