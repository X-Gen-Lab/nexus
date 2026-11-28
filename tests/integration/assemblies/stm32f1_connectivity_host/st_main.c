/**
 * \file            st_main.c
 * \brief           F107VC guarded full-speed EP0 enumeration fixture.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/usb/connectivity_host_transport.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "usb_host_stack.h"

#ifndef NX_STM32F1_USB_HOST_CORE
#error "This fixture requires the exact F107 two-view EP0 profile"
#endif

static nx_st_usb_host_stack_t s_nx_stack;
static nx_st_usb_host_slot_t s_nx_slots[2];
_Alignas(4) static uint8_t s_nx_staging[2][64];
static nx_stm32_usb_host_t* s_nx_host;
static const nx_stm32_usb_host_config_t* s_nx_selected;

static nx_result_t cold_restart(void* context, nx_time_us_t deadline) {
    (void)context;
    if (!nx_stm32f1_usb_host_power_ready())
        return NX_ERROR_STATE;
    return nx_stm32_usb_host_initialize(s_nx_host, s_nx_selected, deadline);
}

static nx_result_t source_power(void* context, bool enabled) {
    (void)context;
    if (!enabled)
        return nx_platform_stop();
    return nx_stm32f1_usb_host_power_ready() ? NX_SUCCESS : NX_ERROR_STATE;
}

static _Noreturn void retain_on_unfinished_close(void) {
    for (;;)
        nx_arch_dsb();
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.cleanup != NX_SUCCESS)
        retain_on_unfinished_close();
    if (started.primary != NX_SUCCESS)
        return (int)started.primary;
    const nx_usb_host_port_t* host =
        nx_factory_usb_host_ref(NX_USB_HOST_REF_LINK);
    if (host == NULL) {
        if (nx_platform_stop() != NX_SUCCESS)
            retain_on_unfinished_close();
        return (int)NX_ERROR_STATE;
    }
    s_nx_host = host->context;
    s_nx_selected = s_nx_host->config;
    for (size_t i = 0U; i < 2U; ++i) {
        s_nx_slots[i].storage = s_nx_staging[i];
        s_nx_slots[i].capacity = sizeof(s_nx_staging[i]);
    }
    const nx_st_usb_host_stack_config_t config = {
        host,    s_nx_slots, 2U,           source_power, &s_nx_stack,
        100000U, 200000U,    cold_restart, &s_nx_stack,  NULL};
    nx_result_t result =
        nx_st_usb_host_stack_initialize(&s_nx_stack, &config, NULL, NULL);
    nx_time_us_t deadline;
    if (result == NX_SUCCESS)
        result =
            nx_deadline_finite_after(nx_time_now_us(), 2000000U, &deadline);
    while (result == NX_SUCCESS &&
           s_nx_stack.middleware.gState != HOST_CHECK_CLASS) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        result = nx_st_usb_host_stack_step(&s_nx_stack);
        if (result == NX_ERROR_BUSY)
            result = NX_SUCCESS;
        if (s_nx_stack.middleware.gState == HOST_ABORT_STATE)
            result = NX_ERROR_IO;
    }
    for (unsigned attempt = 0U; attempt < 200000U; ++attempt) {
        const nx_result_t closed = nx_st_usb_host_stack_close(&s_nx_stack);
        if (closed == NX_SUCCESS) {
            if (nx_platform_stop() != NX_SUCCESS)
                retain_on_unfinished_close();
            return (int)result;
        }
        if (closed != NX_ERROR_BUSY)
            break;
    }
    retain_on_unfinished_close();
}
