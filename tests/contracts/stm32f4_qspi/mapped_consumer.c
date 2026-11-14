/**
 * \file            mapped_consumer.c
 * \brief           Exact CPU-read loan and cold restart without NOR mutation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mapped_storage.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_mapped_storage_loan_t loan;
static volatile uint8_t observed;

/** \brief Only a proved release permits cold stop or a subsequent restart. */
static nx_result_t release(const nx_mapped_storage_port_t* storage) {
    for (unsigned i = 0U; i < 1000U; ++i) {
        nx_result_t result = nx_mapped_storage_release(storage, &loan);
        if (result != NX_ERROR_BUSY)
            return result;
    }
    return NX_ERROR_BUSY;
}
/** \brief Software reference only; no program/erase or hardware qualification.
 * \note Integration excludes all foreign DMA/debug/aperture accesses and any
 *       instruction execution from the window. Only the exact granted span is
 *       read; this single CPU joins its reads before release. Failed release
 *       retains static storage and forbids cold teardown or buffer reuse.
 */
int main(void) {
    for (unsigned cycle = 0U; cycle < 2U; ++cycle) {
        nx_platform_start_result_t start = nx_platform_start();
        if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS)
            return 1;
        const nx_external_bus_port_t* controller =
            nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_SERIAL);
        if (nx_external_bus_port_state(controller) != NX_EXTERNAL_BUS_PREPARED)
            return 2;
        const nx_mapped_storage_port_t* storage =
            nx_factory_mapped_storage_ref(NX_MAPPED_STORAGE_REF_STORAGE);
        const nx_mapped_storage_geometry_t* geometry =
            nx_mapped_storage_geometry(storage);
        if (geometry == NULL || geometry->size != 0x1000000U ||
            geometry->readers != NX_MAPPED_STORAGE_CPU_READ)
            return 2;
        nx_result_t result = nx_mapped_storage_acquire(
            storage, NX_MAPPED_STORAGE_CPU_READ, 0U, 16U, &loan);
        if (result != NX_SUCCESS) {
            (void)nx_platform_stop();
            return 3;
        }
        const volatile uint8_t* bytes = loan.data;
        for (size_t i = 0U; i < loan.length; ++i)
            observed ^= bytes[i];
        if (release(storage) != NX_SUCCESS)
            return 4;
        if (nx_mapped_storage_stop(storage) != NX_SUCCESS ||
            nx_platform_stop() != NX_SUCCESS)
            return 5;
    }
    return 0;
}
