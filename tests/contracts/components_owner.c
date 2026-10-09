/**
 * \file            components_owner.c
 * \brief           Owner admission, stable cancel and retained drain
 *                  regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/components/bus_owner.h"
#include "nexus/os/native.h"
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

/** \brief Provider model with real retained operation responsibility. */
typedef struct {
    pthread_mutex_t mutex;
    nx_bus_owner_t owner;
    nx_owner_slot_t slots[3];
    uint64_t now;
    void* active;
    nx_result_t start_error;
    nx_result_t service_error;
    bool abort_failure;
    bool draining;
    bool complete;
    bool auto_complete;
    unsigned starts;
    unsigned cancels;
    unsigned wakes;
    nx_request_t* completion_observed;
} fixture_t;

/** \brief Protect only bounded adapter metadata. */
static uintptr_t enter(void* context) {
    fixture_t* fixture = context;
    CHECK(pthread_mutex_lock(&fixture->mutex) == 0);
    return 0;
}

/** \brief Restore metadata guard. */
static void leave(void* context, uintptr_t state) {
    fixture_t* fixture = context;
    (void)state;
    CHECK(pthread_mutex_unlock(&fixture->mutex) == 0);
}

/** \brief Clock snapshot is stable during single-owner test execution. */
static uint64_t now(void* context) {
    return ((fixture_t*)context)->now;
}

/** \brief Start errors retain no new provider reference. */
static nx_result_t start(void* context, void* operation,
                         nx_time_us_t deadline) {
    fixture_t* fixture = context;
    CHECK(deadline >= fixture->now);
    if (fixture->start_error != NX_SUCCESS) {
        return fixture->start_error;
    }
    CHECK(fixture->active == NULL);
    fixture->active = operation;
    ++fixture->starts;
    if (fixture->auto_complete) {
        fixture->complete = true;
    }
    return NX_SUCCESS;
}

/** \brief Failed drain never gives the operation back. */
static nx_result_t service(void* context, nx_result_t* result, size_t* count) {
    fixture_t* fixture = context;
    CHECK(fixture->active != NULL);
    if (fixture->service_error != NX_SUCCESS) {
        return fixture->service_error;
    }
    if (!fixture->complete) {
        return NX_ERROR_BUSY;
    }
    *result = fixture->draining ? NX_ERROR_CANCELLED : NX_SUCCESS;
    *count = fixture->draining ? 0 : 7;
    fixture->active = NULL;
    fixture->draining = false;
    fixture->complete = false;
    return NX_SUCCESS;
}

/** \brief Abort error retains storage until later service succeeds. */
static nx_result_t cancel(void* context) {
    fixture_t* fixture = context;
    ++fixture->cancels;
    if (fixture->abort_failure) {
        return NX_ERROR_IO;
    }
    fixture->draining = true;
    fixture->complete = true;
    return NX_SUCCESS;
}

/** \brief Wake is deliberately lossy; request is already final authority. */
static nx_result_t wake(void* context) {
    fixture_t* fixture = context;
    __atomic_fetch_add(&fixture->wakes, 1, __ATOMIC_RELAXED);
    if (fixture->completion_observed != NULL) {
        CHECK(nx_request_state(fixture->completion_observed) ==
              NX_REQUEST_SETTLED);
        for (size_t i = 0; i < 3; ++i) {
            CHECK(fixture->slots[i].identity.request !=
                  fixture->completion_observed);
        }
    }
    return NX_ERROR_EXHAUSTED;
}

/** \brief Prepare unused caller storage with explicit ports. */
static void initialize(fixture_t* fixture) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(pthread_mutex_init(&fixture->mutex, NULL) == 0);
    nx_owner_guard_port_t guard = {fixture, enter, leave};
    nx_owner_executor_port_t executor = {fixture, start, service, cancel,
                                         false};
    nx_clock_t clock = {now, fixture};
    nx_wait_port_t hint = {fixture, NULL, NULL, wake};
    CHECK(nx_bus_owner_init(&fixture->owner, fixture->slots, 3, guard, executor,
                            clock, &hint) == NX_SUCCESS);
}

/** \brief Prepare one independent caller incarnation. */
static void prepare(nx_request_t* request, uint64_t deadline) {
    nx_request_initialize(request);
    CHECK(nx_request_prepare(request, deadline) == NX_SUCCESS);
}

/**
 * \brief           Full rejection establishes no borrow; accepted start errors
 *                  settle.
 */
static void admission_and_epoch(void) {
    fixture_t fixture;
    initialize(&fixture);
    nx_request_t request[4];
    nx_owner_ticket_t ticket[4];
    int operation[4] = {0};
    for (size_t i = 0; i < 4; ++i) {
        prepare(&request[i], 100);
        nx_result_t result = nx_bus_owner_submit(
            &fixture.owner, &request[i], &operation[i], NULL, &ticket[i]);
        CHECK(result == (i < 3 ? NX_SUCCESS : NX_ERROR_EXHAUSTED));
    }
    CHECK(nx_request_state(&request[3]) == NX_REQUEST_READY);
    fixture.start_error = NX_ERROR_BUSY;
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&request[0]) == NX_REQUEST_QUEUED);
    fixture.start_error = NX_ERROR_IO;
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_IO);
    nx_result_t result;
    size_t count;
    CHECK(nx_request_result(&request[0], &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_IO && count == 0);
    nx_owner_ticket_t old = ticket[0];
    CHECK(nx_request_prepare(&request[0], 100) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&fixture.owner, &request[0], &operation[0], NULL,
                              &ticket[0]) == NX_SUCCESS);
    CHECK(old.slot == ticket[0].slot && old.epoch != ticket[0].epoch);
    CHECK(nx_bus_owner_cancel(&fixture.owner, old) == NX_ERROR_STATE);
    CHECK(!fixture.slots[ticket[0].slot].cancel_requested);
    nx_bus_owner_stop(&fixture.owner);
    while (!nx_bus_owner_idle(&fixture.owner)) {
        CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    }
    CHECK(nx_bus_owner_submit(&fixture.owner, &request[3], &operation[3], NULL,
                              &ticket[3]) == NX_ERROR_STATE);
    CHECK(pthread_mutex_destroy(&fixture.mutex) == 0);
}

/** \brief Stop preserves progress; failed abort does not return the loan. */
static void stop_and_failed_abort(void) {
    fixture_t fixture;
    initialize(&fixture);
    nx_request_t request;
    prepare(&request, 100);
    int operation = 0;
    nx_owner_ticket_t ticket;
    nx_wait_port_t completion = {&fixture, NULL, NULL, wake};
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, &completion,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    fixture.abort_failure = true;
    nx_bus_owner_stop(&fixture.owner);
    CHECK(!nx_bus_owner_idle(&fixture.owner));
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&request) == NX_REQUEST_QUARANTINED);
    CHECK(fixture.active == &operation);
    fixture.abort_failure = false;
    fixture.completion_observed = &request;
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_request_state(&request) == NX_REQUEST_SETTLED);
    CHECK(nx_bus_owner_idle(&fixture.owner));
    CHECK(fixture.active == NULL && fixture.cancels == 2);
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_ERROR_STATE);
    CHECK(pthread_mutex_destroy(&fixture.mutex) == 0);
}

/** \brief Queued deadline/cancel never reaches the physical start. */
static void queued_deadline(void) {
    fixture_t fixture;
    initialize(&fixture);
    nx_request_t request;
    prepare(&request, 10);
    int operation = 0;
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    fixture.now = 10;
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_result_t result;
    size_t count;
    CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && fixture.starts == 0);
    CHECK(nx_request_prepare(&request, 100) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_CANCELLED && fixture.starts == 0);
    CHECK(pthread_mutex_destroy(&fixture.mutex) == 0);
}

/** \brief Active deadline remains TIMEOUT through cooperative abort. */
static void active_deadline(void) {
    fixture_t fixture;
    initialize(&fixture);
    nx_request_t request;
    prepare(&request, 10);
    int operation = 0;
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    fixture.now = 10;
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_result_t result;
    size_t count;
    CHECK(nx_request_result(&request, &result, &count) == NX_SUCCESS);
    CHECK(result == NX_ERROR_TIMEOUT && count == 0);
    CHECK(fixture.cancels == 1 && nx_bus_owner_idle(&fixture.owner));
    CHECK(pthread_mutex_destroy(&fixture.mutex) == 0);
}

/**
 * \brief           Independently owned producers retain their requests until
 *                  settlement.
 */
typedef struct {
    fixture_t* fixture;
    uint32_t done;
} concurrent_t;

/**
 * \brief           Exercise cross-task epochs while another producer reuses
 *                  stable slots.
 */
static void producer_task(void* context) {
    concurrent_t* concurrent = context;
    fixture_t* fixture = concurrent->fixture;
    nx_request_t request;
    nx_request_initialize(&request);
    nx_owner_ticket_t old = {0, 0};
    int operation = 1;
    uint64_t bound = nx_native_now_us() + 5000000u;
    for (unsigned i = 0; i < 1000; ++i) {
        CHECK(nx_request_prepare(&request, NX_DEADLINE_NEVER) == NX_SUCCESS);
        nx_owner_ticket_t ticket;
        nx_result_t admitted;
        do {
            admitted = nx_bus_owner_submit(&fixture->owner, &request,
                                           &operation, NULL, &ticket);
            CHECK(admitted == NX_SUCCESS || admitted == NX_ERROR_EXHAUSTED);
            CHECK(nx_native_now_us() < bound);
            if (admitted != NX_SUCCESS) {
                sched_yield();
            }
        } while (admitted != NX_SUCCESS);
        if (old.epoch != 0) {
            CHECK(nx_bus_owner_cancel(&fixture->owner, old) == NX_ERROR_STATE);
        }
        while (nx_request_state(&request) != NX_REQUEST_SETTLED) {
            CHECK(nx_native_now_us() < bound);
            sched_yield();
        }
        nx_result_t result;
        size_t transferred;
        CHECK(nx_request_result(&request, &result, &transferred) == NX_SUCCESS);
        CHECK(result == NX_SUCCESS && transferred == 7);
        old = ticket;
    }
    __atomic_fetch_add(&concurrent->done, 1, __ATOMIC_RELEASE);
}

/** \brief The only executor keeps running while callers wait and join. */
static void executor_task(void* context) {
    concurrent_t* concurrent = context;
    uint64_t bound = nx_native_now_us() + 5000000u;
    for (;;) {
        CHECK(nx_native_now_us() < bound);
        nx_result_t result = nx_bus_owner_service(&concurrent->fixture->owner);
        CHECK(result == NX_SUCCESS || result == NX_ERROR_BUSY);
        if (__atomic_load_n(&concurrent->done, __ATOMIC_ACQUIRE) == 3 &&
            nx_bus_owner_idle(&concurrent->fixture->owner)) {
            nx_bus_owner_stop(&concurrent->fixture->owner);
            return;
        }
        sched_yield();
    }
}

/** \brief Run actual producers on exact caller-owned Native task stacks. */
static void multi_producer(void) {
    fixture_t fixture;
    initialize(&fixture);
    fixture.auto_complete = true;
    concurrent_t concurrent = {&fixture, 0};
    nx_native_task_t tasks[4] = {0};
    void* stacks[4];
    for (size_t i = 0; i < 4; ++i) {
        CHECK(posix_memalign(&stacks[i], 4096, 65536) == 0);
        CHECK(nx_native_task_start(&tasks[i], stacks[i], 65536,
                                   i == 0 ? executor_task : producer_task,
                                   &concurrent) == NX_SUCCESS);
    }
    /* Joining producers never stops their unique execution owner first. */
    for (size_t i = 1; i < 4; ++i) {
        CHECK(nx_native_task_join(&tasks[i]) == NX_SUCCESS);
        free(stacks[i]);
    }
    CHECK(nx_native_task_join(&tasks[0]) == NX_SUCCESS);
    free(stacks[0]);
    CHECK(fixture.starts == 3000 && nx_bus_owner_idle(&fixture.owner));
    CHECK(pthread_mutex_destroy(&fixture.mutex) == 0);
}

/** \brief Run observable borrow, drain and stale-command regressions. */
int main(void) {
    admission_and_epoch();
    stop_and_failed_abort();
    queued_deadline();
    active_deadline();
    multi_producer();
    puts("Owner unique admission, stale cancel, full queue and stop drain "
         "passed");
    return 0;
}
