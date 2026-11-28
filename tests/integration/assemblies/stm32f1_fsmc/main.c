/**
 * \file            main.c
 * \brief           Consume an exclusive external SRAM CPU loan.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static volatile bool exercise_memory;

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_BANKED);
    const nx_memory_window_port_t* window =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_WORK_SRAM);
    nx_memory_window_lease_t lease = {0};
    nx_memory_window_geometry_t layout = {0};
    bool valid =
        nx_external_bus_port_state(bus) == NX_EXTERNAL_BUS_PREPARED &&
        nx_memory_window_port_geometry(window, &layout) == NX_SUCCESS &&
        nx_memory_window_port_acquire(window, &lease) == NX_SUCCESS;
    if (valid) {
        valid = lease.address == UINT32_C(0x68000000) &&
                lease.bytes == 1048576U && layout.address == lease.address &&
                layout.bytes == lease.bytes && !layout.startup_backing;
        if (valid && exercise_memory) {
            volatile uint8_t* bytes = (volatile uint8_t*)lease.address;
            volatile uint16_t* words = (volatile uint16_t*)lease.address;
            bytes[lease.bytes - 1U] = 0x5AU;
            words[0] = 0xA55AU;
            valid = bytes[lease.bytes - 1U] == 0x5AU && words[0] == 0xA55AU;
        }
        valid = nx_memory_window_port_release(window, &lease, lease.epoch) ==
                    NX_SUCCESS &&
                valid;
    }
    return nx_platform_stop() == NX_SUCCESS && valid ? 0 : 2;
}
