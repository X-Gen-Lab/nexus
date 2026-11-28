/**
 * \file            main.c
 * \brief           Exercise exact physical Flash metadata and read dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Observe the full provider through public calls without writing. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_flash_port_t* port = nx_factory_flash_ref(NX_FLASH_REF_PHYSICAL);
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(port);
    uint8_t vector_byte;
    const bool valid = geometry != NULL && geometry->program_unit == 2U &&
                       geometry->size == 128U * 1024U &&
                       nx_flash_port_read(port, 0U, &vector_byte,
                                          sizeof(vector_byte)) == NX_SUCCESS;
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
