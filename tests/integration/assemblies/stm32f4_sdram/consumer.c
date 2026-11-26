/**
 * \file            consumer.c
 * \brief           CPU SDRAM window factory and linked lifecycle reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief This reference links the public contract without probing RAM. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS)
        return 1;
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_MEMORY);
    nx_external_bus_state_t state = nx_external_bus_port_state(bus);
    const nx_memory_window_port_t* window =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_RAM);
    nx_memory_window_geometry_t geometry;
    nx_result_t queried = nx_memory_window_port_geometry(window, &geometry);
    nx_memory_window_lease_t lease = {0};
    nx_result_t acquired = nx_memory_window_port_acquire(window, &lease);
    nx_result_t released =
        acquired == NX_SUCCESS
            ? nx_memory_window_port_release(window, &lease, lease.epoch)
            : acquired;
    nx_result_t stopped = nx_platform_stop();
    return state == NX_EXTERNAL_BUS_PREPARED && queried == NX_SUCCESS &&
                   released == NX_SUCCESS && stopped == NX_SUCCESS
               ? 0
               : 2;
}
