/**
 * \file            main.c
 * \brief           Consume retained F103 backup words through the RTC factory.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/rtc_aux.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;

    const nx_rtc_port_t* backup = nx_factory_rtc_ref(NX_RTC_REF_BACKUP);
    nx_rtc_snapshot_t snapshot = {0};
    nx_rtc_request_t request = {0};
    uint32_t word = UINT32_MAX;
    const bool checked =
        nx_rtc_port_backup_read(backup, 9U, &word) == NX_SUCCESS &&
        word <= UINT16_MAX &&
        nx_rtc_port_backup_read(backup, 10U, &word) == NX_ERROR_INVALID &&
        nx_rtc_port_read(backup, &snapshot) == NX_ERROR_UNSUPPORTED &&
        nx_rtc_port_submit(backup, &request) == NX_ERROR_UNSUPPORTED &&
        nx_rtc_port_backup_write(backup, 0U, 0U) == NX_ERROR_UNSUPPORTED;
    const nx_result_t closed = nx_rtc_port_stop(backup);
    const nx_result_t stopped = nx_platform_stop();
    return checked && closed == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
