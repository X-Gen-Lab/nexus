/**
 * \file            host_stack_main.c
 * \brief           Explicit private middleware integration, link evidence only
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_usb_host.h"
#include "usb_host_stack.h"

typedef struct {
    nx_stm32_usb_host_t* state;
    const nx_stm32_usb_host_config_t* selected;
} fixture_transport_t;
static fixture_transport_t s_nx_transport;
static nx_st_usb_host_stack_t s_nx_stack;
static nx_st_usb_host_slot_t s_nx_slots[2];
_Alignas(4) static uint8_t s_nx_staging[2][1024];

/** \brief The software fixture has no qualified VBUS source switch. */
static nx_result_t source_power(void* context, bool enabled) {
    (void)context;
    return enabled ? NX_ERROR_PERMISSION : NX_SUCCESS;
}

/** \brief Reuse the exact selected controller after its complete HCD drain. */
static nx_result_t restart(void* context, nx_time_us_t deadline) {
    fixture_transport_t* transport = context;
    return nx_stm32_usb_host_initialize(transport->state, transport->selected,
                                        deadline);
}

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return (int)started.primary;
    const nx_usb_host_port_t* host = nx_factory_usb_host(NX_USB_HOST_ID_LINK);
    if (host == NULL)
        return (int)NX_ERROR_INVALID;
    s_nx_transport.state = host->context;
    s_nx_transport.selected = s_nx_transport.state->config;
    for (size_t i = 0U; i < 2U; ++i) {
        s_nx_slots[i].storage = s_nx_staging[i];
        s_nx_slots[i].capacity = sizeof(s_nx_staging[i]);
    }
    const nx_st_usb_host_stack_config_t config = {
        host,     s_nx_slots, 2U,      source_power,    &s_nx_transport,
        1000000U, 200000U,    restart, &s_nx_transport, NULL};
    nx_result_t result =
        nx_st_usb_host_stack_initialize(&s_nx_stack, &config, NULL, NULL);
    if (result == NX_SUCCESS)
        result = nx_st_usb_host_stack_step(&s_nx_stack);
    nx_result_t closed = nx_st_usb_host_stack_close(&s_nx_stack);
    if (closed != NX_SUCCESS)
        return (int)closed;
    closed = nx_platform_stop();
    return (int)(closed == NX_SUCCESS ? result : closed);
}
