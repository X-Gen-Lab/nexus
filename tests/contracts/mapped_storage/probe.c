/**
 * \file            probe.c
 * \brief           Link-only common mapped-storage ABI and state cost fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mapped_storage_owner.h"
#include "nexus_factory.h"

_Static_assert(sizeof(nx_mapped_storage_port_t) == 8,
               "ARM face must remain exactly two pointers");

/* These explicit test doubles establish no physical serial-memory provider. */
static uint8_t aperture[64];
static uint32_t backend_state;
static nx_mapped_storage_owner_t owner;
static nx_mapped_storage_slot_t slots[2];
static nx_mapped_storage_request_t request;
static nx_mapped_storage_loan_t loan;
static nx_mapped_storage_ticket_t ticket;
static const nx_mapped_storage_port_t s_nx_face_mapped_storage = {
    &nx_mapped_storage_owner_ops, &owner};
volatile nx_result_t mapped_storage_probe_result;

/** \brief Fixed link-fixture clock; no timebase qualification. */
static nx_time_us_t now(void* context) {
    (void)context;
    return 1;
}

/** \brief Test-local aperture entry and source withdrawal. */
static nx_result_t idle(void* context) {
    (void)context;
    return NX_SUCCESS;
}

/** \brief No physical operation is implemented by this link-only fixture. */
static nx_result_t start(void* context, nx_mapped_storage_operation_t operation,
                         size_t offset, const void* data, size_t length) {
    (void)context;
    (void)operation;
    (void)offset;
    (void)data;
    (void)length;
    return NX_ERROR_UNSUPPORTED;
}

/** \brief Retain the real bounded poll ABI without claiming hardware. */
static nx_result_t poll(void* context, size_t* completed) {
    (void)context;
    *completed = 0;
    return NX_ERROR_UNSUPPORTED;
}

/** \brief Retain public construction, face, slot and full owner code paths. */
int main(void) {
    /* Retain the generated startup hooks without starting any hardware. */
    if (nx_factory_gpio(NX_GPIO_ID_COUNT) != NULL) {
        return 2;
    }
    const nx_mapped_storage_backend_t backend = {
        &backend_state,
        sizeof(backend_state),
        {aperture, sizeof(aperture), 4, 16, NX_MAPPED_STORAGE_CPU_READ},
        idle,
        idle,
        start,
        poll,
        idle,
        idle};
    mapped_storage_probe_result = nx_mapped_storage_owner_init(
        &owner, slots, 2, backend, (nx_clock_t){now, NULL});
    if (mapped_storage_probe_result != NX_SUCCESS) {
        return 1;
    }
    mapped_storage_probe_result = nx_mapped_storage_acquire(
        &s_nx_face_mapped_storage, NX_MAPPED_STORAGE_CPU_READ, 0, 4, &loan);
    mapped_storage_probe_result =
        nx_mapped_storage_release(&s_nx_face_mapped_storage, &loan);
    nx_request_initialize(&request.base);
    mapped_storage_probe_result = nx_mapped_storage_prepare(
        &request, NX_MAPPED_STORAGE_ERASE, 0, NULL, 16, NX_DEADLINE_NEVER);
    mapped_storage_probe_result =
        nx_mapped_storage_submit(&s_nx_face_mapped_storage, &request, &ticket);
    nx_mapped_storage_service(&s_nx_face_mapped_storage);
    mapped_storage_probe_result =
        nx_mapped_storage_stop(&s_nx_face_mapped_storage);
    return 0;
}
