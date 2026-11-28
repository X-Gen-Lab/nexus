/**
 * \file            main.c
 * \brief           Exercise an F103ZE SD memory port without modifying media.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/block.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static _Alignas(uint32_t) uint8_t sector[512];

static bool write_bounds_rejected(const nx_block_port_t* media,
                                  const nx_block_geometry_t* geometry,
                                  nx_time_us_t deadline) {
    if (!geometry->writable)
        return true;
    const nx_block_io_result_t rejected = nx_block_port_write(
        media, geometry->block_count, sector, sizeof(sector), 1U, deadline);
    return rejected.result == NX_ERROR_INVALID &&
           rejected.completed_blocks == 0U;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_block_port_t* media = nx_factory_block_ref(NX_BLOCK_REF_MEDIA);
    nx_time_us_t deadline;
    bool read = false;
    if (nx_deadline_finite_after(nx_time_now_us(), 1500000U, &deadline) ==
            NX_SUCCESS &&
        nx_block_port_activate(media, deadline) == NX_SUCCESS) {
        const nx_block_geometry_t* geometry = nx_block_port_geometry(media);
        if (geometry != NULL && geometry->block_bytes == sizeof(sector) &&
            nx_block_port_state(media) == NX_BLOCK_READY &&
            nx_deadline_finite_after(nx_time_now_us(), 250000U, &deadline) ==
                NX_SUCCESS &&
            write_bounds_rejected(media, geometry, deadline)) {
            const nx_block_io_result_t transfer = nx_block_port_read(
                media, 0U, sector, sizeof(sector), 1U, deadline);
            read = transfer.result == NX_SUCCESS &&
                   transfer.completed_blocks == 1U &&
                   nx_block_port_sync(media, deadline) == NX_SUCCESS;
        }
    }
    if (nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline) !=
            NX_SUCCESS ||
        nx_block_port_stop(media, deadline) != NX_SUCCESS)
        return 2;
    return nx_platform_stop() == NX_SUCCESS && read ? 0 : 3;
}
