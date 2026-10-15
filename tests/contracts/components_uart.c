/**
 * \file            components_uart.c
 * \brief           Real UART owner terminal ordering, drain and producer tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/components/uart_owner.h"
#include "nexus/io/native/model.h"
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

/** \brief All provider state stays in the single execution owner's context. */
typedef struct {
    pthread_mutex_t mutex;
    nx_bus_owner_t owner;
    nx_owner_slot_t slots[3];
    nx_uart_owner_executor_t executor;
    uint8_t rx[8];
    uint8_t transmitted[6000];
} fixture_t;

/** \brief Serialize only bounded admission and cancellation metadata. */
static uintptr_t enter(void* context) {
    CHECK(pthread_mutex_lock(context) == 0);
    return 0;
}

/** \brief Restore the precise metadata guard after each short critical path. */
static void leave(void* context, uintptr_t state) {
    (void)state;
    CHECK(pthread_mutex_unlock(context) == 0);
}

/**
 * \brief           Use the provider's absolute deadline domain without a second
 *                  clock.
 */
static uint64_t clock_read(void* context) {
    (void)context;
    return nx_time_now_us();
}

/**
 * \brief           Create only explicitly budgeted storage and actual Native
 *                  providers.
 */
static void initialize(fixture_t* fixture, bool automatic_irq) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(pthread_mutex_init(&fixture->mutex, NULL) == 0);
    CHECK(nx_native_clock_configure(true, 0) == NX_SUCCESS);
    nx_native_uart_config_t config = {NX_UART_RX_BYTES,
                                      fixture->rx,
                                      sizeof(fixture->rx),
                                      fixture->transmitted,
                                      sizeof(fixture->transmitted),
                                      automatic_irq};
    CHECK(nx_native_uart_configure(&config) == NX_SUCCESS);
    nx_owner_guard_port_t guard = {&fixture->mutex, enter, leave};
    nx_clock_t clock = {clock_read, NULL};
    CHECK(nx_bus_owner_init(
              &fixture->owner, fixture->slots, 3, guard,
              nx_uart_owner_executor_port(&fixture->executor, nx_native_uart),
              clock, NULL) == NX_SUCCESS);
}

/**
 * \brief           Release model storage only after owner, provider and callers
 *                  quiesce.
 */
static void close_fixture(fixture_t* fixture) {
    CHECK(nx_bus_owner_idle(&fixture->owner));
    nx_bus_owner_stop(&fixture->owner);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_SUCCESS);
    CHECK(pthread_mutex_destroy(&fixture->mutex) == 0);
}

/**
 * \brief           Establish one outer admission while leaving private storage
 *                  separate.
 */
static nx_owner_ticket_t submit(fixture_t* fixture, nx_request_t* request,
                                nx_uart_owner_operation_t* operation,
                                uint64_t deadline) {
    nx_request_initialize(request);
    CHECK(nx_request_prepare(request, deadline) == NX_SUCCESS);
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture->owner, request, operation, NULL,
                              &ticket) == NX_SUCCESS);
    return ticket;
}

/**
 * \brief           Verify only acquire-published final fields and detached
 *                  inner loans.
 */
static void expect_result(fixture_t* fixture, nx_request_t* request,
                          nx_result_t expected, size_t count) {
    nx_result_t result;
    size_t transferred;
    CHECK(nx_request_result(request, &result, &transferred) == NX_SUCCESS);
    CHECK(result == expected && transferred == count);
    CHECK(!fixture->executor.pending && fixture->executor.inner.data == NULL);
    CHECK(fixture->executor.inner.length == 0);
}

/** \brief A byte-loaded event is distinct from TC and outer settlement. */
static void terminal_ordering(void) {
    fixture_t fixture;
    initialize(&fixture, false);
    const uint8_t bytes[] = {0x12, 0x34};
    nx_uart_owner_operation_t operation = {bytes, sizeof(bytes)};
    nx_request_t request;
    nx_owner_ticket_t ticket = submit(&fixture, &request, &operation, 10);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(&fixture.executor.inner.base != &request);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    nx_native_uart_irq_step();
    CHECK(nx_request_state(&fixture.executor.inner.base) == NX_REQUEST_ACTIVE);
    CHECK(nx_native_clock_advance(20) == NX_SUCCESS);
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_SUCCESS, sizeof(bytes));
    CHECK(nx_native_uart_transmitted() == sizeof(bytes));
    CHECK(!memcmp(fixture.transmitted, bytes, sizeof(bytes)));
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_ERROR_STATE);
    close_fixture(&fixture);
}

/**
 * \brief           Active expiry and pre-TC cancellation retain actual
 *                  transmitted bytes.
 */
static void deadline_and_cancel(void) {
    fixture_t fixture;
    initialize(&fixture, false);
    const uint8_t bytes[] = {1, 2};
    nx_uart_owner_operation_t operation = {bytes, sizeof(bytes)};
    nx_request_t request;
    (void)submit(&fixture, &request, &operation, 10);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_native_uart_irq_step();
    CHECK(nx_native_clock_advance(10) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_TIMEOUT, 1);

    CHECK(nx_request_prepare(&request, 100) == NX_SUCCESS);
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_CANCELLED, 0);

    CHECK(nx_request_prepare(&request, 10) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_TIMEOUT, 0);
    CHECK(nx_native_uart_transmitted() == 1);
    close_fixture(&fixture);
}

/**
 * \brief           Accepted controller start failure settles outer without
 *                  shared base.
 */
static void start_failure_and_busy(void) {
    fixture_t fixture;
    initialize(&fixture, false);
    const uint8_t bytes[] = {0x55};
    nx_uart_tx_request_t previous = {0};
    nx_request_initialize(&previous.base);
    CHECK(nx_uart_tx_prepare(&previous, bytes, sizeof(bytes), 100) ==
          NX_SUCCESS);
    CHECK(nx_uart_port_submit(nx_native_uart, &previous) == NX_SUCCESS);
    nx_uart_owner_operation_t operation = {bytes, sizeof(bytes)};
    nx_request_t request;
    (void)submit(&fixture, &request, &operation, 100);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_BUSY);
    CHECK(nx_request_state(&request) == NX_REQUEST_QUEUED);
    CHECK(!fixture.executor.pending && fixture.executor.inner.data == NULL);
    /* Explicitly finish previous ownership before handing the port over. */
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    nx_uart_port_service(nx_native_uart);
    CHECK(nx_request_state(&previous.base) == NX_REQUEST_SETTLED);
    nx_native_uart_fault(true, false);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(nx_request_state(&request) == NX_REQUEST_ACTIVE);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_IO, 0);
    nx_native_uart_fault(false, false);

    operation.data = NULL;
    CHECK(nx_request_prepare(&request, 100) == NX_SUCCESS);
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_INVALID);
    expect_result(&fixture, &request, NX_ERROR_INVALID, 0);
    close_fixture(&fixture);
}

/**
 * \brief           Quarantined inner/outer storage survives recovery and stale
 *                  epochs.
 */
static void quarantine_reuse_and_stop(void) {
    fixture_t fixture;
    initialize(&fixture, false);
    const uint8_t bytes[] = {1, 2};
    nx_uart_owner_operation_t operation = {bytes, sizeof(bytes)};
    nx_request_t request;
    nx_owner_ticket_t old = submit(&fixture, &request, &operation, 100);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_native_uart_fault(false, true);
    CHECK(nx_bus_owner_cancel(&fixture.owner, old) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_IO);
    CHECK(nx_request_state(&request) == NX_REQUEST_QUARANTINED);
    CHECK(nx_request_state(&fixture.executor.inner.base) ==
          NX_REQUEST_QUARANTINED);
    CHECK(fixture.executor.pending && fixture.executor.inner.data == bytes);
    CHECK(nx_request_prepare(&request, 100) == NX_ERROR_STATE);
    CHECK(!nx_bus_owner_idle(&fixture.owner));
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_IO);
    nx_native_uart_fault(false, false);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_CANCELLED, 1);

    CHECK(nx_request_prepare(&request, 100) == NX_SUCCESS);
    nx_owner_ticket_t current;
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &current) == NX_SUCCESS);
    CHECK(current.slot == old.slot && current.epoch != old.epoch);
    CHECK(nx_bus_owner_cancel(&fixture.owner, old) == NX_ERROR_STATE);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_SUCCESS, sizeof(bytes));

    CHECK(nx_request_prepare(&request, 100) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&fixture.owner, &request, &operation, NULL,
                              &current) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_native_uart_fault(false, true);
    nx_bus_owner_stop(&fixture.owner);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_ERROR_IO);
    CHECK(nx_request_state(&request) == NX_REQUEST_QUARANTINED);
    CHECK(nx_uart_port_stop(nx_native_uart) == NX_ERROR_BUSY);
    nx_request_t rejected;
    nx_request_initialize(&rejected);
    CHECK(nx_request_prepare(&rejected, 100) == NX_SUCCESS);
    CHECK(nx_bus_owner_submit(&fixture.owner, &rejected, &operation, NULL,
                              &current) == NX_ERROR_STATE);
    CHECK(nx_request_state(&rejected) == NX_REQUEST_READY);
    nx_native_uart_fault(false, false);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    expect_result(&fixture, &request, NX_ERROR_CANCELLED, 0);
    close_fixture(&fixture);
}

/**
 * \brief           Completion observer may reclaim every outer byte before wake
 *                  returns.
 */
typedef struct {
    fixture_t* fixture;
    nx_request_t* request;
    nx_uart_owner_operation_t* operation;
    unsigned calls;
} reclaim_t;

/**
 * \brief           Expose post-publication use-after-free under the sanitizer
 *                  runtime.
 */
static nx_result_t reclaim(void* context) {
    reclaim_t* observer = context;
    expect_result(observer->fixture, observer->request, NX_SUCCESS, 1);
    CHECK(pthread_mutex_trylock(&observer->fixture->mutex) == 0);
    CHECK(pthread_mutex_unlock(&observer->fixture->mutex) == 0);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(observer->fixture->slots[i].identity.request == NULL);
    }
    free((void*)observer->operation->data);
    free(observer->operation);
    free(observer->request);
    observer->operation = NULL;
    observer->request = NULL;
    ++observer->calls;
    return NX_SUCCESS;
}

/** \brief Neither provider nor owner accesses the loan after terminal wake. */
static void publication_reclaims_outer(void) {
    fixture_t fixture;
    initialize(&fixture, false);
    reclaim_t observer = {&fixture, malloc(sizeof(nx_request_t)),
                          malloc(sizeof(nx_uart_owner_operation_t)), 0};
    uint8_t* byte = malloc(1);
    CHECK(observer.request != NULL && observer.operation != NULL &&
          byte != NULL);
    *byte = 0xab;
    *observer.operation = (nx_uart_owner_operation_t){byte, 1};
    nx_request_initialize(observer.request);
    CHECK(nx_request_prepare(observer.request, 100) == NX_SUCCESS);
    nx_wait_port_t completion = {&observer, NULL, NULL, reclaim};
    nx_owner_ticket_t ticket;
    CHECK(nx_bus_owner_submit(&fixture.owner, observer.request,
                              observer.operation, &completion,
                              &ticket) == NX_SUCCESS);
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    nx_native_uart_irq_step();
    nx_native_uart_irq_step();
    CHECK(nx_bus_owner_service(&fixture.owner) == NX_SUCCESS);
    CHECK(observer.calls == 1 && observer.request == NULL);
    CHECK(nx_bus_owner_cancel(&fixture.owner, ticket) == NX_ERROR_STATE);
    close_fixture(&fixture);
}

/** \brief Producers own independent descriptors; a separate task owns UART. */
typedef struct {
    fixture_t* fixture;
    uint32_t done;
} concurrent_t;

/**
 * \brief           One producer's protocol marker identifies complete TX frame
 *                  ordering.
 */
typedef struct {
    concurrent_t* shared;
    uint8_t identity;
} producer_t;

/** \brief Exercise exact queue saturation and stale tickets through reuse. */
static void producer_task(void* context) {
    producer_t* producer = context;
    concurrent_t* concurrent = producer->shared;
    nx_request_t request;
    nx_request_initialize(&request);
    nx_owner_ticket_t old = {0, 0};
    uint8_t bytes[2] = {producer->identity, 0};
    nx_uart_owner_operation_t operation = {bytes, sizeof(bytes)};
    uint64_t bound = nx_native_now_us() + UINT64_C(5000000);
    for (unsigned i = 0; i < 1000; ++i) {
        bytes[1] = (uint8_t)i;
        CHECK(nx_request_prepare(&request, NX_DEADLINE_NEVER) == NX_SUCCESS);
        nx_owner_ticket_t ticket;
        nx_result_t admitted;
        do {
            admitted = nx_bus_owner_submit(&concurrent->fixture->owner,
                                           &request, &operation, NULL, &ticket);
            CHECK(admitted == NX_SUCCESS || admitted == NX_ERROR_EXHAUSTED);
            CHECK(nx_native_now_us() < bound);
            if (admitted != NX_SUCCESS) {
                CHECK(nx_request_state(&request) == NX_REQUEST_READY);
                sched_yield();
            }
        } while (admitted != NX_SUCCESS);
        if (old.epoch != 0) {
            CHECK(nx_bus_owner_cancel(&concurrent->fixture->owner, old) ==
                  NX_ERROR_STATE);
        }
        while (nx_request_state(&request) != NX_REQUEST_SETTLED) {
            CHECK(nx_native_now_us() < bound);
            sched_yield();
        }
        nx_result_t result;
        size_t transferred;
        CHECK(nx_request_result(&request, &result, &transferred) == NX_SUCCESS);
        CHECK(result == NX_SUCCESS && transferred == sizeof(bytes));
        old = ticket;
    }
    __atomic_fetch_add(&concurrent->done, 1, __ATOMIC_RELEASE);
}

/** \brief Continue controller progress while producer tasks wait and join. */
static void executor_task(void* context) {
    concurrent_t* concurrent = context;
    uint64_t bound = nx_native_now_us() + UINT64_C(5000000);
    for (;;) {
        CHECK(nx_native_now_us() < bound);
        nx_result_t result = nx_bus_owner_service(&concurrent->fixture->owner);
        CHECK(result == NX_SUCCESS || result == NX_ERROR_BUSY);
        if (__atomic_load_n(&concurrent->done, __ATOMIC_ACQUIRE) == 3 &&
            nx_bus_owner_idle(&concurrent->fixture->owner)) {
            return;
        }
        sched_yield();
    }
}

/** \brief Real Native tasks send 3000 uninterleaved frames through one UART. */
static void multi_producer(void) {
    fixture_t fixture;
    initialize(&fixture, true);
    concurrent_t concurrent = {&fixture, 0};
    producer_t producers[3] = {
        {&concurrent, 1}, {&concurrent, 2}, {&concurrent, 3}};
    nx_native_task_t tasks[4] = {0};
    void* stacks[4];
    for (size_t i = 0; i < 4; ++i) {
        /* Host instrumentation storage, not an MCU stack budget. */
        CHECK(posix_memalign(&stacks[i], 4096, 256U * 1024U) == 0);
        CHECK(nx_native_task_start(&tasks[i], stacks[i], 256U * 1024U,
                                   i == 0 ? executor_task : producer_task,
                                   i == 0 ? (void*)&concurrent
                                          : (void*)&producers[i - 1]) ==
              NX_SUCCESS);
    }
    for (size_t i = 1; i < 4; ++i) {
        CHECK(nx_native_task_join(&tasks[i]) == NX_SUCCESS);
        free(stacks[i]);
    }
    CHECK(nx_native_task_join(&tasks[0]) == NX_SUCCESS);
    free(stacks[0]);
    CHECK(nx_native_uart_transmitted() == sizeof(fixture.transmitted));
    unsigned sequence[3] = {0};
    for (size_t i = 0; i < sizeof(fixture.transmitted); i += 2) {
        uint8_t identity = fixture.transmitted[i];
        CHECK(identity >= 1 && identity <= 3);
        CHECK(fixture.transmitted[i + 1] == (uint8_t)sequence[identity - 1]);
        ++sequence[identity - 1];
    }
    CHECK(sequence[0] == 1000 && sequence[1] == 1000 && sequence[2] == 1000);
    close_fixture(&fixture);
}

/**
 * \brief           Execute actual UART provider contracts without fake wire
 *                  qualification.
 */
int main(void) {
    terminal_ordering();
    deadline_and_cancel();
    start_failure_and_busy();
    quarantine_reuse_and_stop();
    publication_reclaims_outer();
    multi_producer();
    puts("UART owner private settlement, TC ordering, quarantine and 3000 "
         "multi-producer frames passed");
    return 0;
}
