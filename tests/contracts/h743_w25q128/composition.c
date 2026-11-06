/**
 * \file            composition.c
 * \brief           Private static composition of the actual H743 HAL backend
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "composition.h"
#include "stm32h743_w25q128.h"

static nx_h743_w25q128_state_t controller;
static nx_mapped_storage_owner_t owner;
static nx_mapped_storage_slot_t slots[2];
static nx_mapped_storage_backend_t backend;
static const nx_mapped_storage_port_t face = {&nx_mapped_storage_owner_ops,
                                              &owner};
static bool published;

void _init(void);
void _fini(void);
/** \brief Bare C link fixture supplies the empty CRT constructor boundary. */
void _init(void) {
}
/** \brief No C++ destructor or process-exit machinery belongs to this image. */
void _fini(void) {
}

/** \brief Use the selected platform's already established monotonic domain. */
static nx_time_us_t clock_read(void* context) {
    (void)context;
    return nx_time_now_us();
}
/** \brief These constants describe a link profile, never an invented PCB. */
nx_result_t nx_h743_w25q128_reference_prepare(void) {
    const nx_h743_w25q128_mpu_config_t regions = {12U, 13U};
    return nx_h743_w25q128_prepare(&controller, (nx_clock_t){clock_read, NULL},
                                   64000000U, 16000000U, regions);
}
/** \brief Publish the generic face only after exact discovery and owner init.
 */
void nx_h743_w25q128_reference_service(void) {
    nx_h743_w25q128_service(&controller);
    if (!published && nx_h743_w25q128_status(&controller) == NX_SUCCESS &&
        nx_h743_w25q128_backend(&controller, &backend) == NX_SUCCESS &&
        nx_mapped_storage_owner_init(&owner, slots, 2U, backend,
                                     (nx_clock_t){clock_read, NULL}) ==
            NX_SUCCESS)
        published = true;
    if (published)
        nx_mapped_storage_service(&face);
}
/** \brief Product code receives no HAL handle, register or private header. */
const nx_mapped_storage_port_t* nx_h743_w25q128_reference_storage(void) {
    return published ? &face : NULL;
}
/** \brief Retain all state until owner and hardware cleanup independently pass.
 */
nx_result_t nx_h743_w25q128_reference_stop(void) {
    if (published) {
        nx_result_t result = nx_mapped_storage_stop(&face);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = nx_h743_w25q128_stop(&controller);
    if (result == NX_SUCCESS)
        published = false;
    return result;
}
