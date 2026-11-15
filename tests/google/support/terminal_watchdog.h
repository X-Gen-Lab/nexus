/**
 * \file            terminal_watchdog.h
 * \brief           Parent observation of an actual nonreturning child path
 * \author          Nexus Team
 *
 * The child records its complete fault predicate before returning from its
 * observer. The parent requires that evidence and a full second without child
 * exit. It then kills and reaps that exact child. SIGKILL only reclaims a host
 * test process; it never proves device ownership release. No signal handler or
 * sanitizer safe point is required inside the production terminal loop.
 */
#ifndef NEXUS_TEST_TERMINAL_WATCHDOG_H
#define NEXUS_TEST_TERMINAL_WATCHDOG_H

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace nexus_test {
inline int terminal_observer_fd = -1;

/** \brief Publish exactly one validated observer record, then return normally.
 */
inline void RecordTerminalObservation(bool valid) {
    const char record = valid ? '1' : '0';
    if (terminal_observer_fd < 0 || write(terminal_observer_fd, &record,
                                          sizeof(record)) != sizeof(record)) {
        std::_Exit(94);
    }
}

/** \brief Forward child-only Google failures into the parent's evidence. */
class TerminalFailureListener : public testing::EmptyTestEventListener {
    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.failed()) {
            RecordTerminalObservation(false);
        }
    }
};

/**
 * \brief Observe a terminal child for one second after its checked record.
 * \details Missing, invalid or duplicate evidence, early return and abnormal
 * exit fail. Child Google failures invalidate evidence; callback exceptions
 * terminate that child without escaping into its inherited test runner.
 * Three seconds bound setup plus observation. Every created child is reaped;
 * every owned descriptor is closed. The callback runs only in the forked
 * child and returning from it is a failure, including after evidence.
 * The caller exclusively owns reaping this child; no SIGCHLD handler or other
 * thread may consume its wait status.
 */
template <typename Function> bool ObserveTerminal(Function execute) {
    int channel[2];
    if (pipe(channel) != 0) {
        return false;
    }
    const int flags = fcntl(channel[0], F_GETFL);
    if (flags < 0 || fcntl(channel[0], F_SETFL, flags | O_NONBLOCK) != 0) {
        close(channel[0]);
        close(channel[1]);
        return false;
    }
    const pid_t child = fork();
    if (child == 0) {
        close(channel[0]);
        terminal_observer_fd = channel[1];
        try {
            /* Google owns this host-only listener inside the disposable child.
             * Its failures must not remain invisible in fork-private state. */
            testing::UnitTest::GetInstance()->listeners().Append(
                new TerminalFailureListener);
            execute();
        } catch (...) {
            std::_Exit(93);
        }
        std::_Exit(99);
    }
    close(channel[1]);
    if (child < 0) {
        close(channel[0]);
        return false;
    }
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    auto quiet_until = deadline;
    bool ready = false;
    bool parked = false;
    bool reaped = false;
    bool child_owned = true;
    int status = 0;
    while (Clock::now() < deadline) {
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            reaped = true;
            break;
        }
        if (result < 0 && errno != EINTR) {
            child_owned = errno != ECHILD;
            break;
        }
        pollfd wait{channel[0], POLLIN, 0};
        const int available = poll(&wait, 1, 10);
        if (available < 0 && errno != EINTR) {
            break;
        }
        if (available > 0) {
            char records[2];
            const ssize_t count = read(channel[0], records, sizeof(records));
            if (count < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            if (count != 1 || ready || records[0] != '1') {
                ready = false;
                break;
            }
            ready = true;
            quiet_until = Clock::now() + std::chrono::seconds(1);
        }
        if (ready && Clock::now() >= quiet_until) {
            const pid_t final = waitpid(child, &status, WNOHANG);
            parked = final == 0;
            reaped = final == child;
            if (final < 0 && errno == ECHILD) {
                child_owned = false;
            }
            break;
        }
    }
    bool killed = false;
    if (!reaped && child_owned) {
        killed = kill(child, SIGKILL) == 0;
        pid_t result;
        do {
            result = waitpid(child, &status, 0);
        } while (result < 0 && errno == EINTR);
        reaped = result == child;
    }
    close(channel[0]);
    return ready && parked && killed && reaped && WIFSIGNALED(status) &&
           WTERMSIG(status) == SIGKILL;
}
} /* namespace nexus_test */

#endif /* NEXUS_TEST_TERMINAL_WATCHDOG_H */
