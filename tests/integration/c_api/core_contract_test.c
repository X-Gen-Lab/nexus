/**
 * \file            core_contract_test.c
 *
 * \brief           Request borrow/epoch/concurrent publication and wrap
 *                  regressions.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/request.h"
#include <pthread.h>
#include <stdio.h>

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                      \
        }                                                                      \
    } while (0)

/** \brief Exercise absolute deadline arithmetic and the reserved sentinel. */
static bool deadline_boundaries(void) {
    CHECK(!nx_deadline_expired(NX_DEADLINE_NEVER, UINT64_MAX));
    CHECK(!nx_deadline_expired(11, 10));
    CHECK(nx_deadline_expired(10, 10));
    CHECK(nx_deadline_expired(9, 10));
    CHECK(nx_deadline_after(10, 5) == 15);
    CHECK(nx_deadline_after(UINT64_MAX - 5, 5) == NX_DEADLINE_NEVER);
    CHECK(nx_deadline_after(UINT64_MAX - 5, 4) == UINT64_MAX - 1);
    return true;
}

/** \brief Verify wrap extension, fractional ticks and explicit exhaustion. */
static bool counter_wrap(void) {
    nx_clock32_t clock = {0};
    nx_time_us_t output = 777;
    CHECK(nx_clock32_observe(&clock, 0, &output) == NX_ERROR_STATE);
    CHECK(output == 777);
    CHECK(nx_clock32_initialize(&clock, UINT32_MAX - 3, 1000000) == NX_SUCCESS);
    CHECK(nx_clock32_observe(&clock, 6, &output) == NX_SUCCESS);
    CHECK(output == 10);
    CHECK(nx_clock32_observe(&clock, 9, &output) == NX_SUCCESS);
    CHECK(output == 13);
    CHECK(nx_clock32_initialize(&clock, 0, 3) == NX_SUCCESS);
    CHECK(nx_clock32_observe(&clock, 1, &output) == NX_SUCCESS);
    CHECK(output == 333333);
    CHECK(nx_clock32_observe(&clock, 3, &output) == NX_SUCCESS);
    CHECK(output == 1000000);
    clock.ticks = UINT64_MAX;
    clock.frequency_hz = 1000000;
    CHECK(nx_clock32_observe(&clock, 4, &output) == NX_ERROR_EXHAUSTED);
    CHECK(clock.previous == 3);
    CHECK(nx_clock32_initialize(&clock, 0, 0) == NX_ERROR_INVALID);
    return true;
}

/** \brief Admission rejection retains no borrow; timeout is not settlement. */
static bool request_lifetime(void) {
    nx_request_t request;
    nx_request_initialize(&request);
    CHECK(nx_request_state(&request) == NX_REQUEST_READY);
    CHECK(nx_request_prepare(&request, 50) == NX_SUCCESS);
    CHECK(nx_request_admit(&request, NX_REQUEST_DRAINING) == NX_ERROR_INVALID);
    CHECK(nx_request_state(&request) == NX_REQUEST_READY);
    CHECK(nx_request_admit(&request, NX_REQUEST_QUEUED) == NX_SUCCESS);
    CHECK(nx_request_admit(&request, NX_REQUEST_ACTIVE) == NX_ERROR_STATE);
    CHECK(nx_request_prepare(&request, 80) == NX_ERROR_STATE);
    CHECK(nx_request_transition(&request, NX_REQUEST_ACTIVE) == NX_SUCCESS);
    CHECK(nx_request_transition(&request, NX_REQUEST_READY) == NX_ERROR_STATE);
    CHECK(nx_request_transition(&request, NX_REQUEST_DRAINING) == NX_SUCCESS);
    CHECK(nx_request_transition(&request, NX_REQUEST_QUARANTINED) ==
          NX_SUCCESS);
    nx_result_t result = NX_SUCCESS;
    size_t count = 999;
    CHECK(nx_request_result(&request, &result, &count) == NX_ERROR_BUSY);
    CHECK(count == 999);
    nx_request_settle(&request, NX_ERROR_TIMEOUT, 3);
    CHECK(nx_request_state(&request) == NX_REQUEST_SETTLED);
    CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && count == 3);
    nx_request_settle(&request, NX_SUCCESS, 99);
    CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && count == 3);
    CHECK(nx_request_prepare(&request, 80) == NX_SUCCESS);
    CHECK(request.deadline == 80);
    CHECK(nx_request_result(&request, &result, &count) == NX_ERROR_STATE);
    return true;
}

/** \brief Old epoch commands cannot inspect reused/freed request storage. */
static bool cancellation_identity(void) {
    nx_request_slot_t slot = {0};
    nx_request_t request;
    nx_request_initialize(&request);
    uint64_t first;
    CHECK(nx_request_slot_bind(&slot, &request, &first) == NX_SUCCESS);
    CHECK(first == 1);
    CHECK(nx_request_slot_lookup(&slot, 0) == NULL);
    CHECK(nx_request_slot_lookup(&slot, first + 1) == NULL);
    CHECK(nx_request_slot_lookup(&slot, first) == &request);
    CHECK(nx_request_admit(&request, NX_REQUEST_ACTIVE) == NX_SUCCESS);
    CHECK(nx_request_slot_release(&slot, first) == NX_ERROR_BUSY);
    nx_request_settle(&request, NX_ERROR_CANCELLED, 0);
    CHECK(nx_request_slot_release(&slot, first) == NX_SUCCESS);
    CHECK(nx_request_slot_lookup(&slot, first) == NULL);
    uint64_t second;
    CHECK(nx_request_prepare(&request, NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_request_slot_bind(&slot, &request, &second) == NX_SUCCESS);
    CHECK(second == 2 && nx_request_slot_lookup(&slot, first) == NULL);
    CHECK(nx_request_slot_release(&slot, first) == NX_ERROR_STATE);
    CHECK(nx_request_slot_release(&slot, second) == NX_SUCCESS);
    slot.epoch = UINT64_MAX;
    CHECK(nx_request_slot_bind(&slot, &request, &second) == NX_ERROR_EXHAUSTED);
    CHECK(slot.request == NULL && slot.epoch == UINT64_MAX);
    return true;
}

/** \brief A producer publishes result before the final release state store. */
static void* publish_result(void* context) {
    nx_request_t* request = context;
    nx_request_settle(request, NX_ERROR_IO, 1234);
    return NULL;
}

/** \brief An acquire observer sees complete result despite no wake
 * notification. */
static bool concurrent_publication(void) {
    for (unsigned iteration = 0; iteration < 200; iteration++) {
        nx_request_t request;
        nx_request_initialize(&request);
        CHECK(nx_request_admit(&request, NX_REQUEST_ACTIVE) == NX_SUCCESS);
        pthread_t producer;
        CHECK(pthread_create(&producer, NULL, publish_result, &request) == 0);
        unsigned spins = 0;
        while (nx_request_state(&request) != NX_REQUEST_SETTLED) {
            CHECK(++spins < 100000000);
        }
        nx_result_t result;
        size_t count;
        CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
        CHECK(result == NX_ERROR_IO && count == 1234);
        CHECK(pthread_join(producer, NULL) == 0);
    }
    return true;
}

/** \brief Verify nested saved state and honest Native cycle unavailability. */
static bool architecture_contract(void) {
    CHECK(!nx_arch_irq_is_masked());
    nx_arch_irq_state_t outer = nx_arch_irq_save();
    CHECK(nx_arch_irq_is_masked());
    nx_arch_irq_state_t inner = nx_arch_irq_save();
    CHECK(inner.value == outer.value + 1);
    nx_arch_dmb();
    nx_arch_dsb();
    nx_arch_isb();
    nx_arch_irq_restore(inner);
    CHECK(nx_arch_irq_is_masked());
    nx_arch_irq_restore(outer);
    CHECK(!nx_arch_irq_is_masked());
    CHECK(!nx_arch_in_isr());
    uint32_t cycles = 42;
    CHECK(!nx_arch_cycle_snapshot(&cycles));
    CHECK(cycles == 42);
    return true;
}

/** \brief Run every meaningful core regression and reject a failed case. */
int main(void) {
    const struct {
        const char* name;
        bool (*run)(void);
    } cases[] = {{"deadline boundaries", deadline_boundaries},
                 {"counter wrap", counter_wrap},
                 {"request lifetime", request_lifetime},
                 {"cancellation identity", cancellation_identity},
                 {"concurrent publication", concurrent_publication},
                 {"architecture contract", architecture_contract}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].run()) {
            fprintf(stderr, "FAIL %s\n", cases[i].name);
            return 1;
        }
        printf("PASS %s\n", cases[i].name);
    }
    return 0;
}
