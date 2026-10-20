/**
 * \file            lpdma_lptim1.c
 * \brief           Exact finite LPTIM1 request preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/stm32u575_lpdma_lptim1.h"
#include "nexus/io/dma.h"
#include "stm32u575xx.h"
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)pointer;
    return pointer != NULL && bytes <= UINTPTR_MAX - begin &&
           begin % alignment == 0U &&
           !(begin < UINT32_C(0x60000000) &&
             UINT32_C(0x40000000) < begin + bytes) &&
           !(begin < UINT32_C(0xe0100000) &&
             UINT32_C(0xe0000000) < begin + bytes);
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool separate(const void* control, size_t size,
                     const nx_stm32u575_lptim1_tuple_t* tuple,
                     nx_stm32u575_lpdma_lptim1_workspace_t* workspace) {
    return !overlap(control, size, tuple, sizeof(*tuple)) &&
           !overlap(control, size, workspace, sizeof(*workspace));
}
nx_result_t nx_stm32u575_lpdma_lptim1_request_prepare(
    nx_stm32u575_lpdma_lptim1_request_t* request,
    const nx_stm32u575_retained_time_snapshot_t* domain,
    const nx_stm32u575_lptim1_tuple_t* tuple,
    nx_stm32u575_lpdma_lptim1_workspace_t* workspace, nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U)
        return NX_ERROR_CONTEXT;
    if (!span(request, sizeof(*request),
              _Alignof(nx_stm32u575_lpdma_lptim1_request_t)) ||
        !span(domain, sizeof(*domain),
              _Alignof(nx_stm32u575_retained_time_snapshot_t)) ||
        overlap(request, sizeof(*request), domain, sizeof(*domain)))
        return NX_ERROR_INVALID;
    static const nx_dma_memory_region_t region = {
        UINT32_C(0x28000000), 16384U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_result_t result = nx_dma_buffer_validate(
        &region, 1U, tuple, sizeof(*tuple), 4U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    result = nx_dma_buffer_validate(&region, 1U, workspace, sizeof(*workspace),
                                    4U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (overlap(tuple, sizeof(*tuple), workspace, sizeof(*workspace)) ||
        !separate(request, sizeof(*request), tuple, workspace) ||
        !separate(domain, sizeof(*domain), tuple, workspace) ||
        !span(domain->identity, sizeof(nx_stm32u575_retained_time_storage_t),
              _Alignof(nx_stm32u575_retained_time_storage_t)) ||
        overlap(request, sizeof(*request), domain->identity,
                sizeof(nx_stm32u575_retained_time_storage_t)) ||
        !separate(domain->identity,
                  sizeof(nx_stm32u575_retained_time_storage_t), tuple,
                  workspace) ||
        domain->generation == 0U || domain->now == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS)
        return result;
    request->identity = request;
    request->tuple = tuple;
    request->private_destination = (void*)&LPTIM1->CCR1;
    request->private_length = sizeof(*tuple);
    request->workspace = workspace;
    request->clock_identity = domain->identity;
    request->clock_generation = domain->generation;
    return NX_SUCCESS;
}
