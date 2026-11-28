/**
 * \file            main.c
 * \brief           Read the selected F103RB loaded Flash options
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Observe loaded protection through the same physical Flash owner. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_flash_port_t* flash = nx_factory_flash_ref(NX_FLASH_REF_PHYSICAL);
    const nx_flash_maintenance_port_t* maintenance =
        nx_factory_flash_maintenance_ref(NX_FLASH_REF_PHYSICAL);
    nx_flash_maintenance_snapshot_t snapshot;
    const bool valid =
        flash != NULL && maintenance != NULL &&
        flash->context == maintenance->context &&
        nx_flash_maintenance_snapshot(maintenance, &snapshot) == NX_SUCCESS &&
        snapshot.sector_count == 128U &&
        snapshot.options.brownout == NX_FLASH_BROWNOUT_UNKNOWN &&
        snapshot.otp_bytes == 0U;
    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
