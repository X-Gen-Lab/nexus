/**
 * \file            main.c
 * \brief           Public I3C Cortex-M33 Runtime compile/link reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/i3c_controller_storage.h"
#if __has_include("stm32h5xx.h") || __has_include("stm32h5xx_hal_i3c.h")
#error Vendor SDK leaked into this public consumer
#endif

/* This synthetic CPU-only port never owns physical pins, IRQs or a bus. */
static nx_result_t fixture_claim(void* context, const void* owner) {
    (void)context;
    (void)owner;
    return NX_SUCCESS;
}
static void fixture_release(void* context, const void* owner) {
    (void)context;
    (void)owner;
}
static nx_result_t fixture_ready(void* context) {
    (void)context;
    return NX_SUCCESS;
}
static void fixture_begin(void* context, const nx_i3c_command_t* command) {
    (void)context;
    (void)command;
}
static void fixture_pump(void* context, const uint8_t* tx, size_t tx_capacity,
                         uint8_t* rx, size_t rx_capacity,
                         nx_i3c_observation_t* observation) {
    (void)context;
    (void)tx;
    (void)tx_capacity;
    (void)rx;
    (void)rx_capacity;
    observation->complete = true;
}
static void fixture_assign(void* context, uint8_t address) {
    (void)context;
    (void)address;
}
static void fixture_withdraw(void* context) {
    (void)context;
}
static nx_result_t fixture_quiescent(void* context) {
    (void)context;
    return NX_SUCCESS;
}
static nx_result_t fixture_reset(void* context) {
    (void)context;
    return NX_ERROR_UNSUPPORTED;
}
static nx_time_us_t fixture_clock(void* context) {
    (void)context;
    return 1;
}
static const nx_i3c_port_ops_t fixture_ops = {
    fixture_claim,    fixture_release,   fixture_ready,
    fixture_begin,    fixture_pump,      fixture_assign,
    fixture_withdraw, fixture_quiescent, fixture_reset};
static const nx_i3c_target_definition_t definitions[] = {
    {0x112233445566, 0x12}};
static nx_i3c_target_state_t targets[1];
static nx_i3c_event_t events[2];
static nx_i3c_controller_t controller;
static nx_i3c_request_t request;
static volatile nx_result_t result_sink;

void _start(void);

/** \brief Retain public API bodies without claiming MCU or bus execution. */
void _start(void) {
    const nx_i3c_controller_config_t config = {
        &fixture_ops, NULL,    {fixture_clock, NULL},
        definitions,  targets, 1,
        events,       2,       8};
    nx_i3c_ticket_t ticket = {0};
    nx_i3c_endpoint_t endpoint = {0};
    nx_i3c_event_t event;
    nx_i3c_event_status_t status;
    nx_request_initialize(&request.base);
    result_sink = nx_i3c_controller_construct(&controller, &config);
    result_sink =
        nx_i3c_request_prepare(&request, NX_I3C_DISCOVER, endpoint, 0, NULL, 0,
                               false, false, NX_DEADLINE_NEVER);
    result_sink = nx_i3c_submit(&controller, &request, &ticket);
    result_sink = nx_i3c_service(&controller);
    result_sink = nx_i3c_cancel(ticket);
    result_sink = nx_i3c_endpoint(&controller, 0, &endpoint);
    result_sink = nx_i3c_event_take(&controller, &event);
    result_sink = nx_i3c_event_status(&controller, &status);
    result_sink = nx_i3c_close(&controller);
    result_sink = nx_i3c_reset(&controller);
    result_sink = nx_i3c_reopen(&controller);
    result_sink = nx_i3c_stop(&controller);
    for (;;) {
    }
}
