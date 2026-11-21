/**
 * \file            portable_native_process_test.cpp
 * \brief           Explicit Native bootstrap and permanent-task boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/storage.h"
#include <atomic>
#include <csignal>
#include <gtest/gtest.h>
#include <sys/resource.h>
#include <unistd.h>

namespace {
static_assert(std::atomic<unsigned>::is_always_lock_free);
std::atomic<unsigned> startup_calls;
std::atomic<unsigned> startup_same_thread;
pthread_t caller;

/** \brief Record actual callback identity without a hidden worker. */
void startup(void*) {
    startup_same_thread.store(pthread_equal(pthread_self(), caller) != 0 ? 1U
                                                                         : 0U,
                              std::memory_order_release);
    startup_calls.fetch_add(1U, std::memory_order_release);
}

/** \brief Only lock-free atomic observation and POSIX _exit run in the signal.
 */
void finish_probe(int) {
    const bool correct =
        startup_calls.load(std::memory_order_acquire) == 1U &&
        startup_same_thread.load(std::memory_order_acquire) == 1U;
    _exit(correct ? 0 : 20);
}

/** \brief Returning violates the permanent-task contract. */
void returning_permanent(void*) {
}

TEST(PortableNativeLifecycleDeathTest,
     BootstrapRunsOnceOnCallerAndDoesNotReturn) {
    EXPECT_EXIT(
        {
            caller = pthread_self();
            (void)signal(SIGALRM, finish_probe);
            (void)alarm(1);
            (void)nx_os_run(startup, nullptr);
            _exit(21);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PortableNativeLifecycleDeathTest, PermanentEntryReturnStopsTheProcess) {
    EXPECT_EXIT(
        {
            const struct rlimit no_core {};
            if (setrlimit(RLIMIT_CORE, &no_core) != 0) {
                _exit(23);
            }
            static nx_permanent_task_t task{};
            NX_OS_STACK_DEFINE(stack, 524288);
            (void)alarm(2);
            if (nx_permanent_task_start(&task, "permanent", stack, sizeof stack,
                                        0, returning_permanent,
                                        nullptr) != NX_SUCCESS) {
                _exit(22);
            }
            for (;;) {
                (void)pause();
            }
        },
        ::testing::KilledBySignal(SIGABRT), "");
}
} /* namespace */
