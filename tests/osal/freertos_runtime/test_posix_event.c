/* Regression checks for the Nexus host-only FreeRTOS POSIX event helper. */
#include "wait_for_event.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    struct event* ev;
    atomic_bool entered;
    bool timed;
    bool result;
} waiter_t;

static void* wait_thread(void* argument) {
    waiter_t* waiter = argument;
    atomic_store(&waiter->entered, true);
    waiter->result = waiter->timed ? event_wait_timed(waiter->ev, 10000) :
                                    event_wait(waiter->ev);
    return NULL;
}

static uint64_t milliseconds(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static void cancellation_preserves_event(bool timed) {
    for (unsigned i = 0; i < 100; ++i) {
        struct event* ev = event_create();
        assert(ev);
        waiter_t waiter = {.ev = ev, .timed = timed};
        atomic_init(&waiter.entered, false);
        pthread_t thread;
        assert(pthread_create(&thread, NULL, wait_thread, &waiter) == 0);
        while (!atomic_load(&waiter.entered)) sched_yield();
        assert(pthread_cancel(thread) == 0);
        void* result;
        assert(pthread_join(thread, &result) == 0 && result == PTHREAD_CANCELED);
        /* Cancellation must release the mutex: upstream blocks here. */
        event_signal(ev);
        assert(event_wait_timed(ev, 0));
        assert(!event_wait_timed(ev, 0));
        event_delete(ev);
    }
}

int main(void) {
    cancellation_preserves_event(false);
    puts("POSIX event untimed cancellation and reuse passed");
    cancellation_preserves_event(true);
    puts("POSIX event timed cancellation and reuse passed");

    struct event* ev = event_create();
    assert(ev);
    event_signal(ev);
    event_signal(ev);
    assert(event_wait_timed(ev, 0));
    assert(!event_wait_timed(ev, 0));
    assert(!event_wait_timed(ev, -1));
    assert(!event_wait(NULL));
    assert(!event_wait_timed(NULL, 1));
    event_delete(NULL);
    event_signal(NULL);
    puts("POSIX event coalescing, zero timeout and invalid input passed");

    /* Force the fractional millisecond addition across a seconds boundary. */
    struct timespec now, pause = {.tv_nsec = 1000000L};
    do {
        assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
        if (now.tv_nsec < 100000000L) assert(nanosleep(&pause, NULL) == 0);
    } while (now.tv_nsec < 100000000L);
    uint64_t before = milliseconds();
    assert(!event_wait_timed(ev, 900));
    assert(milliseconds() - before >= 850);
    /* A timed-out wait also releases its mutex and consumes no event. */
    event_signal(ev);
    assert(event_wait_timed(ev, 0));
    puts("POSIX event normalized monotonic deadline and timeout cleanup passed");

    waiter_t waiter = {.ev = ev, .timed = true};
    atomic_init(&waiter.entered, false);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, wait_thread, &waiter) == 0);
    while (!atomic_load(&waiter.entered)) sched_yield();
    event_signal(ev);
    assert(pthread_join(thread, NULL) == 0 && waiter.result);
    assert(!event_wait_timed(ev, 0));
    event_delete(ev);
    puts("POSIX event signalled timed wait and consumption passed");
    puts("5 POSIX host event contract groups passed");
    return 0;
}
