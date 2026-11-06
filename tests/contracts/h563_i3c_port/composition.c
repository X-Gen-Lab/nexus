/**
 * \file            composition.c
 * \brief           Private H563 composition with deliberately absent boot proof
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "composition.h"
#include "stm32h563_i3c_port.h"

/* External boot/Board/target proofs are absent. Refuse ownership rather than
 * invent clocks, routing or reset. This image must never be executed. */
static nx_result_t unavailable_claim(void* context, const void* owner) {
    (void)context;
    (void)owner;
    return NX_ERROR_UNSUPPORTED;
}
static void unavailable_release(void* context, const void* owner) {
    (void)context;
    (void)owner;
}
static nx_result_t unavailable_proof(void* context) {
    (void)context;
    return NX_ERROR_UNSUPPORTED;
}
static nx_time_us_t unavailable_clock(void* context) {
    (void)context;
    return 0;
}
static const nx_h563_i3c_resources_t resources = {
    unavailable_claim, unavailable_release, unavailable_proof,
    unavailable_proof};
static nx_h563_i3c_port_t port;
static const nx_i3c_target_definition_t definition = {0x112233445566, 0x12};
static nx_i3c_target_state_t target;
static nx_i3c_event_t event;
const uint32_t nx_h563_i3c_private_storage_bytes = sizeof(port);

nx_result_t nx_h563_reference_compose(nx_i3c_controller_t* controller) {
    const nx_h563_i3c_port_config_t port_config = {I3C1, &resources, NULL};
    const nx_i3c_controller_config_t controller_config = {
        &nx_h563_i3c_port_ops,
        &port,
        {unavailable_clock, NULL},
        &definition,
        &target,
        1,
        &event,
        1,
        8};
    nx_result_t result = nx_h563_i3c_port_bind(&port, &port_config);
    return result == NX_SUCCESS
               ? nx_i3c_controller_construct(controller, &controller_config)
               : result;
}
