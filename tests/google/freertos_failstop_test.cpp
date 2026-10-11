/**
 * \file            freertos_failstop_test.cpp
 * \brief           Actual weak hooks observe and retain through terminal park
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include "nexus/core/request.h"
#include "nexus/os/freertos.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" void nx_freertos_assert_failed(const char*, unsigned);
extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char*);
namespace {
unsigned observed;
unsigned masked;
bool correct;
nx_fault_domain_t expected_domain;
bool exit_at_mask;
bool verify_retained;
nx_request_t retained;
nx_arch_irq_masks_t model_masks;
uint32_t expected_basepri;
int feedback_fd = -1;

/** \brief Normal child callbacks publish evidence; no signal handler is used.
 */
enum class Phase : uint32_t { observer = 1, query = 2, disabled = 3 };
struct Evidence {
    uint32_t phase;
    unsigned observed;
    unsigned masked;
    uint32_t correct;
    uint32_t loan;
    nx_arch_irq_masks_t masks;
};

/** \brief Exercise missing evidence and premature exit against the controller.
 */
enum class Probe { production, missing_evidence, premature_exit };

void publish(Phase phase) {
    if (feedback_fd < 0) {
        return;
    }
    const Evidence evidence = {
        static_cast<uint32_t>(phase),
        observed,
        masked,
        correct ? 1U : 0U,
        static_cast<uint32_t>(verify_retained ? nx_request_state(&retained)
                                              : NX_REQUEST_READY),
        model_masks};
    const auto* bytes = reinterpret_cast<const char*>(&evidence);
    size_t remaining = sizeof(evidence);
    while (remaining != 0U) {
        const ssize_t count = write(feedback_fd, bytes, remaining);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            std::_Exit(94);
        }
        const size_t written = static_cast<size_t>(count);
        bytes += written;
        remaining -= written;
    }
}

void prepare(nx_fault_domain_t domain, bool immediate) {
    model_masks = {};
    expected_basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    observed = 0;
    masked = 0;
    correct = false;
    expected_domain = domain;
    exit_at_mask = immediate;
    verify_retained = false;
}

bool same_masks(nx_arch_irq_masks_t actual, nx_arch_irq_masks_t expected) {
    return actual.primask == expected.primask &&
           actual.basepri == expected.basepri &&
           actual.faultmask == expected.faultmask;
}

/** \brief Child owns its pipe; only the parent can end an observed park. */
void run_child(int fd, nx_arch_irq_masks_t incoming, bool port_expected,
               bool loan_expected, Probe probe, int acknowledge_fd) {
    feedback_fd = fd;
    prepare(NX_FAULT_FREERTOS_ASSERT, false);
    model_masks = incoming;
    if (!port_expected) {
        expected_basepri = incoming.basepri;
    }
    if (loan_expected) {
        nx_request_initialize(&retained);
        if (nx_request_prepare(&retained, NX_DEADLINE_NEVER) != NX_SUCCESS ||
            nx_request_admit(&retained, NX_REQUEST_ACTIVE) != NX_SUCCESS) {
            std::_Exit(92);
        }
        verify_retained = true;
    }
    if (probe == Probe::missing_evidence) {
        std::_Exit(42);
    }
    if (probe == Probe::premature_exit) {
        /* Valid reports alone cannot establish the terminal park. */
        nx_fault_observe(NX_FAULT_FREERTOS_ASSERT, NX_ERROR_STATE,
                         "owned-static-source", 73U);
        (void)nx_arch_irq_masks();
        if (port_expected) {
            vPortDisableInterrupts();
        }
        /* Exit only after the parent has received all valid evidence. */
        char acknowledgement;
        ssize_t count;
        do {
            count = read(acknowledge_fd, &acknowledgement, 1);
        } while (count < 0 && errno == EINTR);
        if (count != 1) {
            std::_Exit(95);
        }
        close(acknowledge_fd);
        std::_Exit(93);
    }
    nx_freertos_assert_failed("owned-static-source", 73U);
    std::_Exit(93);
}

/** \brief Capture fixed records without trusting a child-reported exit code. */
struct Capture {
    std::array<Evidence, 4> records{};
    size_t count = 0;
    Evidence partial{};
    size_t used = 0;
    bool malformed = false;
};

void read_evidence(int fd, Capture& capture) {
    auto* bytes = reinterpret_cast<char*>(&capture.partial);
    const ssize_t count =
        read(fd, bytes + capture.used, sizeof(Evidence) - capture.used);
    if (count < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (count <= 0) {
        capture.malformed = true;
        return;
    }
    capture.used += static_cast<size_t>(count);
    if (capture.used == sizeof(Evidence)) {
        if (capture.count == capture.records.size()) {
            capture.malformed = true;
            return;
        }
        capture.records[capture.count++] = capture.partial;
        capture.partial = {};
        capture.used = 0;
    }
}

bool valid_evidence(const Capture& capture, nx_arch_irq_masks_t incoming,
                    bool port_expected, bool loan_expected) {
    const size_t expected_count = port_expected ? 3U : 2U;
    if (capture.malformed || capture.used != 0U ||
        capture.count != expected_count) {
        return false;
    }
    for (size_t index = 0; index < capture.count; ++index) {
        const Evidence& sample = capture.records[index];
        nx_arch_irq_masks_t expected = incoming;
        if (index == 2U) {
            expected.basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY;
        }
        if (sample.phase != index + 1U || sample.observed != 1U ||
            sample.correct != 1U || sample.masked != (index == 2U ? 1U : 0U) ||
            sample.loan !=
                (loan_expected ? NX_REQUEST_ACTIVE : NX_REQUEST_READY) ||
            !same_masks(sample.masks, expected)) {
            return false;
        }
    }
    return true;
}

/**
 * \brief Parent proves ordered evidence, live park and exact child reclamation.
 * \details The production loop remains untouched. A finite external observer
 * waits for actual observer/Arch/port records and a live quiet interval, then
 * kills and reaps that exact child. Missing records, early return or abnormal
 * exit fail; no callback or sanitizer signal safepoint inside the park is
 * needed. SIGKILL is process cleanup, never device-loan reclamation proof.
 */
bool observe_terminal(nx_arch_irq_masks_t incoming, bool port_expected,
                      bool loan_expected, Probe probe = Probe::production) {
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
    int acknowledgement[2]{-1, -1};
    if (probe == Probe::premature_exit && pipe(acknowledgement) != 0) {
        close(channel[0]);
        close(channel[1]);
        return false;
    }
    const pid_t child = fork();
    if (child == 0) {
        close(channel[0]);
        if (acknowledgement[1] >= 0) {
            close(acknowledgement[1]);
        }
        run_child(channel[1], incoming, port_expected, loan_expected, probe,
                  acknowledgement[0]);
        std::_Exit(95);
    }
    close(channel[1]);
    if (acknowledgement[0] >= 0) {
        close(acknowledgement[0]);
    }
    if (child < 0) {
        close(channel[0]);
        if (acknowledgement[1] >= 0) {
            close(acknowledgement[1]);
        }
        return false;
    }
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    auto quiet_until = deadline;
    Capture capture;
    int status = 0;
    bool reaped = false;
    bool parked = false;
    bool ready = false;
    while (Clock::now() < deadline) {
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            reaped = true;
            break;
        }
        if (result < 0 && errno != EINTR) {
            break;
        }
        pollfd wait{channel[0], POLLIN, 0};
        const int available = poll(&wait, 1, 10);
        if (available < 0 && errno != EINTR) {
            break;
        }
        if (available > 0) {
            read_evidence(channel[0], capture);
        }
        if (capture.malformed) {
            break;
        }
        if (!ready &&
            valid_evidence(capture, incoming, port_expected, loan_expected)) {
            ready = true;
            if (probe == Probe::premature_exit &&
                write(acknowledgement[1], "x", 1) != 1) {
                break;
            }
            quiet_until = Clock::now() + std::chrono::seconds(1);
        }
        if (ready && Clock::now() >= quiet_until) {
            const pid_t final = waitpid(child, &status, WNOHANG);
            parked = final == 0;
            reaped = final == child;
            break;
        }
    }
    bool killed = false;
    if (!reaped) {
        killed = kill(child, SIGKILL) == 0;
        pid_t result;
        do {
            result = waitpid(child, &status, 0);
        } while (result < 0 && errno == EINTR);
        reaped = result == child;
    }
    close(channel[0]);
    if (acknowledgement[1] >= 0) {
        close(acknowledgement[1]);
    }
    return ready && parked && killed && reaped && WIFSIGNALED(status) &&
           WTERMSIG(status) == SIGKILL &&
           valid_evidence(capture, incoming, port_expected, loan_expected);
}

TEST(FreeRTOSFailStop, AssertObservesLocationBeforeMaskAndPark) {
    EXPECT_EXIT(
        {
            prepare(NX_FAULT_FREERTOS_ASSERT, true);
            nx_freertos_assert_failed("owned-static-source", 73U);
        },
        ::testing::ExitedWithCode(42), "");
}
TEST(FreeRTOSFailStop, StackOverflowNeverDereferencesTaskOrName) {
    EXPECT_EXIT(
        {
            prepare(NX_FAULT_FREERTOS_STACK, true);
            vApplicationStackOverflowHook(reinterpret_cast<TaskHandle_t>(1U),
                                          reinterpret_cast<char*>(1U));
        },
        ::testing::ExitedWithCode(42), "");
}
TEST(FreeRTOSFailStop, ReturningObserverCannotReclaimLoanOrContinueKernel) {
    EXPECT_FALSE(observe_terminal({}, true, true, Probe::missing_evidence));
    EXPECT_FALSE(observe_terminal({}, true, true, Probe::premature_exit));
    EXPECT_TRUE(observe_terminal({}, true, true));
}
TEST(FreeRTOSFailStop, StrongerIncomingBasepriIsNeverWeakenedByPortStore) {
    nx_arch_irq_masks_t incoming{};
    incoming.basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY - 16U;
    EXPECT_TRUE(observe_terminal(incoming, false, false));
}
TEST(FreeRTOSFailStop, EqualIncomingBasepriStaysMaskedThroughObservation) {
    nx_arch_irq_masks_t incoming{};
    incoming.basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    EXPECT_TRUE(observe_terminal(incoming, false, false));
}
TEST(FreeRTOSFailStop, PrimaskIsPreservedWithoutChangingAnotherMask) {
    nx_arch_irq_masks_t incoming{};
    incoming.primask = 1U;
    EXPECT_TRUE(observe_terminal(incoming, false, false));
}
TEST(FreeRTOSFailStop, FaultmaskIsPreservedWithoutChangingAnotherMask) {
    nx_arch_irq_masks_t incoming{};
    incoming.faultmask = 1U;
    EXPECT_TRUE(observe_terminal(incoming, false, false));
}
TEST(FreeRTOSFailStop, WeakerIncomingBasepriIsRaisedToActualKernelCeiling) {
    EXPECT_EXIT(
        {
            prepare(NX_FAULT_FREERTOS_ASSERT, true);
            model_masks.basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY + 16U;
            nx_freertos_assert_failed("owned-static-source", 73U);
        },
        ::testing::ExitedWithCode(42), "");
}
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    ++observed;
    correct =
        domain == expected_domain && reason == NX_ERROR_STATE && masked == 0 &&
        file != nullptr && line != 0U &&
        (!verify_retained || nx_request_state(&retained) == NX_REQUEST_ACTIVE);
    if (domain == NX_FAULT_FREERTOS_ASSERT) {
        correct = correct && std::strcmp(file, "owned-static-source") == 0 &&
                  line == 73U;
    }
    publish(Phase::observer);
}
extern "C" void vPortDisableInterrupts(void) {
    model_masks.basepri = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    masked = 1;
    publish(Phase::disabled);
    if (exit_at_mask) {
        std::_Exit(observed == 1 && correct == 1 &&
                           model_masks.basepri == expected_basepri
                       ? 42
                       : 90);
    }
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    publish(Phase::query);
    return model_masks;
}
