#define _POSIX_C_SOURCE 200809L
#include "arch/nx_arch.h"
#include "hal/system/nx_mutex.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __linux__
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

enum { THREADS = 4, ITERATIONS = 20000 };
static atomic_bool s_start;
static unsigned s_inside, s_value;
static nx_atomic_t s_hal_atomic;

static void* increment(void* argument) {
    uintptr_t number = (uintptr_t)argument;
    while (!atomic_load_explicit(&s_start, memory_order_acquire)) sched_yield();
    assert(!nx_arch_in_isr() && !nx_arch_irq_is_masked());
    for (unsigned i = 0; i < ITERATIONS; ++i) {
        /* Both the public Arch entry and the existing HAL entry must serialize
         * against the same lock, including recursive atomic access. */
        uint32_t state = number % 2 ? nx_critical_enter() : nx_arch_irq_save().value;
        assert(nx_arch_irq_is_masked());
        assert(++s_inside == 1);
        nx_arch_irq_state_t nested = nx_arch_irq_save();
        unsigned previous = s_value;
        if (i % 1000 == 0) sched_yield();
        s_value = previous + 1;
        nx_arch_irq_restore(nested);
        assert(nx_arch_irq_is_masked());
        nx_atomic_fetch_add(&s_hal_atomic, 1);
        assert(--s_inside == 0);
        if (number % 2) nx_critical_exit(state);
        else nx_arch_irq_restore((nx_arch_irq_state_t){state});
        assert(!nx_arch_irq_is_masked());
    }
    return NULL;
}

#ifdef __linux__
static void* restore_foreign(void* arg) {
    nx_arch_irq_restore(*(nx_arch_irq_state_t*)arg);
    return NULL;
}
static void require_abort(unsigned misuse) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        struct rlimit no_core = {0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        nx_arch_irq_state_t outer = nx_arch_irq_save();
        if (misuse == 0) {
            (void)nx_arch_irq_save();
            nx_arch_irq_restore(outer); /* Out of order. */
        } else if (misuse == 1) {
            nx_arch_irq_restore(outer);
            nx_arch_irq_restore(outer); /* Twice. */
        } else {
            pthread_t foreign;
            assert(pthread_create(&foreign, NULL, restore_foreign, &outer) == 0);
            assert(pthread_join(foreign, NULL) == 0);
        }
        _exit(1);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
#endif

int main(void) {
    assert(!nx_arch_in_isr() && !nx_arch_irq_is_masked());
    /* No previous save: exercise simultaneous first initialization as well. */
    pthread_t threads[THREADS];
    for (uintptr_t i = 0; i < THREADS; ++i)
        assert(pthread_create(&threads[i], NULL, increment, (void*)i) == 0);
    atomic_store_explicit(&s_start, true, memory_order_release);
    for (unsigned i = 0; i < THREADS; ++i)
        assert(pthread_join(threads[i], NULL) == 0);
    assert(s_value == THREADS * ITERATIONS && s_inside == 0);
    assert(nx_atomic_load(&s_hal_atomic) == THREADS * ITERATIONS);
    puts("Native concurrent first use and 80000 nested Arch/HAL updates passed");

    nx_arch_irq_state_t outer = nx_arch_irq_save();
    nx_arch_irq_state_t inner = nx_arch_irq_save();
    nx_arch_irq_restore(inner);
    assert(nx_arch_irq_is_masked());
    nx_arch_dmb(); nx_arch_dsb(); nx_arch_isb();
    nx_arch_irq_restore(outer);
    assert(!nx_arch_irq_is_masked());
    puts("Native nested saved-state and task context passed");

    uint32_t expected = THREADS * ITERATIONS;
    assert(nx_atomic_compare_exchange(&s_hal_atomic, &expected, 7));
    expected = 3;
    assert(!nx_atomic_compare_exchange(&s_hal_atomic, &expected, 9));
    assert(expected == 7);
    nx_atomic_store(&s_hal_atomic, UINT32_MAX);
    assert(nx_atomic_fetch_add(&s_hal_atomic, 1) == UINT32_MAX);
    assert(nx_atomic_load(&s_hal_atomic) == 0);
    puts("HAL compare-exchange and unsigned wrap passed");
#ifdef __linux__
    for (unsigned misuse = 0; misuse < 3; ++misuse) require_abort(misuse);
    puts("Native invalid restore tokens fail closed passed");
#endif
    return 0;
}
