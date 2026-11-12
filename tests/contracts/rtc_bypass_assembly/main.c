/**
 * \file            main.c
 * \brief           Explicit external LSE clock and PC13 capture ARM consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Software link fixture; no oscillator or capture execution.
 */
#include "nexus/io/rtc_aux.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_rtc_port_t* port = nx_factory_rtc_ref(NX_RTC_REF_CALENDAR);
    if (port == NULL || port->ops == NULL || port->context == NULL)
        return 1;
    nx_platform_start_result_t startup = nx_platform_start();
    if (startup.primary != NX_SUCCESS)
        return 2;
    nx_rtc_capture_config_t capture = {
        .timestamp_enabled = true, .timestamp_pin = 1U, .tamper1_pin = 1U};
    nx_result_t result = nx_rtc_port_capture_configure(port, &capture);
    if (result != NX_SUCCESS)
        return 3;
    capture.timestamp_enabled = false;
    result = nx_rtc_port_capture_configure(port, &capture);
    if (result != NX_SUCCESS)
        return 4;
    result = nx_rtc_port_stop(port);
    if (result != NX_SUCCESS)
        return 5;
    nx_rtc_backup_handoff_t receipt = {0};
    (void)nx_rtc_port_backup_handoff(port, &receipt);
    return nx_platform_stop() == NX_SUCCESS ? 0 : 6;
}
