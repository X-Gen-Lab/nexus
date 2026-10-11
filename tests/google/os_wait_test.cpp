/**
 * \file            os_wait_test.cpp
 * \brief           Actual wait deadlines and cooperative Native lifetimes
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus/os/baremetal.h"
#include "nexus/os/native.h"
#include <atomic>
#include <condition_variable>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

namespace {

/** \brief Observe actual POSIX wait admission without timing sleeps. */
struct WaitBoundary {
    std::mutex mutex;
    std::condition_variable entered;
    pthread_cond_t* condition = nullptr;
    unsigned timed = 0;
    unsigned infinite = 0;

    void observe(pthread_cond_t* observed, bool finite) {
        std::lock_guard<std::mutex> lock(mutex);
        if (observed == condition) {
            finite ? ++timed : ++infinite;
            entered.notify_all();
        }
    }

    void select(pthread_cond_t* observed) {
        std::lock_guard<std::mutex> lock(mutex);
        condition = observed;
        timed = infinite = 0;
    }

    void await() {
        std::unique_lock<std::mutex> lock(mutex);
        entered.wait(lock, [this] { return timed + infinite != 0; });
    }
};

WaitBoundary boundary;

/** \brief Inject only the external monotonic clock for bounded wake storms. */
class ClockBoundary {
  public:
    MOCK_METHOD(uint64_t, read, ());
};

thread_local ClockBoundary* selectedClock = nullptr;

/** \brief Only external wait operations are mocked, never the helper. */
class PortModel {
  public:
    MOCK_METHOD(uint32_t, arm, ());
    MOCK_METHOD(nx_result_t, wait, (uint32_t, uint64_t));
    MOCK_METHOD(bool, ready, ());

    nx_wait_port_t port() {
        return {this, armCallback, waitCallback, nullptr};
    }
    static uint32_t armCallback(void* context) {
        return static_cast<PortModel*>(context)->arm();
    }
    static nx_result_t waitCallback(void* context, uint32_t sequence,
                                    uint64_t deadline) {
        return static_cast<PortModel*>(context)->wait(sequence, deadline);
    }
    static bool readyCallback(void* context) {
        return static_cast<PortModel*>(context)->ready();
    }
};

/** \brief Bounded storm also terminates the unfixed production helper. */
struct WakeStorm {
    nx_wait_port_t port;
    unsigned checks = 0;

    static bool ready(void* context) {
        auto* storm = static_cast<WakeStorm*>(context);
        ++storm->checks;
        storm->port.wake(storm->port.context);
        return storm->checks > 64;
    }
};

uint64_t clockRead(void* context) {
    return *static_cast<uint64_t*>(context);
}

/** \brief Explicit caller-owned stack, kept through all joins. */
struct NativeStack {
    /* Host sanitizers need additional stack; this is not an MCU budget. */
    alignas(4096) unsigned char bytes[262144];
};

} /* namespace */

extern "C" int __real_pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);
extern "C" int __real_pthread_cond_timedwait(pthread_cond_t*, pthread_mutex_t*,
                                             const struct timespec*);
extern "C" int __real_clock_gettime(clockid_t, struct timespec*);

/** \brief Every ordinary test retains the actual monotonic host clock. */
extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec* value) {
    if (clock == CLOCK_MONOTONIC && selectedClock != nullptr) {
        uint64_t now = selectedClock->read();
        value->tv_sec = static_cast<time_t>(now / 1000000);
        value->tv_nsec = static_cast<long>((now % 1000000) * 1000);
        return 0;
    }
    return __real_clock_gettime(clock, value);
}

/** \brief Preserve the real blocking operation and observe only entry. */
extern "C" int __wrap_pthread_cond_wait(pthread_cond_t* condition,
                                        pthread_mutex_t* mutex) {
    boundary.observe(condition, false);
    return __real_pthread_cond_wait(condition, mutex);
}

/** \brief Preserve the real timed blocking operation and observe only entry. */
extern "C" int __wrap_pthread_cond_timedwait(pthread_cond_t* condition,
                                             pthread_mutex_t* mutex,
                                             const struct timespec* deadline) {
    boundary.observe(condition, true);
    return __real_pthread_cond_timedwait(condition, mutex, deadline);
}

TEST(OsWaitContract, ArmRecheckCompletionNeverSleeps) {
    testing::StrictMock<PortModel> model;
    auto port = model.port();
    testing::InSequence order;
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, arm()).WillOnce(testing::Return(7));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(true));
    EXPECT_EQ(nx_wait_until(&port, PortModel::readyCallback, &model, 42),
              NX_SUCCESS);
}

TEST(OsWaitContract, NotificationOnlyRechecksAuthoritativePredicate) {
    testing::StrictMock<PortModel> model;
    auto port = model.port();
    testing::InSequence order;
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, arm()).WillOnce(testing::Return(7));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, wait(7, 42)).WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, arm()).WillOnce(testing::Return(8));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, wait(8, 42)).WillOnce(testing::Return(NX_ERROR_TIMEOUT));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_EQ(nx_wait_until(&port, PortModel::readyCallback, &model, 42),
              NX_ERROR_TIMEOUT);
}

class OsWaitFinalObservation : public testing::TestWithParam<nx_result_t> {};

TEST_P(OsWaitFinalObservation, CompletionWinsFinalErrorObservation) {
    testing::StrictMock<PortModel> model;
    auto port = model.port();
    testing::InSequence order;
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, arm()).WillOnce(testing::Return(7));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(false));
    EXPECT_CALL(model, wait(7, 42)).WillOnce(testing::Return(GetParam()));
    EXPECT_CALL(model, ready()).WillOnce(testing::Return(true));
    EXPECT_EQ(nx_wait_until(&port, PortModel::readyCallback, &model, 42),
              NX_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(BackendErrors, OsWaitFinalObservation,
                         testing::Values(NX_ERROR_TIMEOUT, NX_ERROR_BUSY,
                                         NX_ERROR_CONTEXT, NX_ERROR_IO));

TEST(OsWaitContract, InvalidPortDoesNotCallPredicate) {
    testing::StrictMock<PortModel> model;
    auto port = model.port();
    EXPECT_EQ(nx_wait_until(nullptr, PortModel::readyCallback, &model, 0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_wait_until(&port, nullptr, &model, 0), NX_ERROR_INVALID);
    port.wait = nullptr;
    EXPECT_EQ(nx_wait_until(&port, PortModel::readyCallback, &model, 0),
              NX_ERROR_INVALID);
}

TEST(BaremetalWait, ChangedSequenceCannotBypassExpiredDeadline) {
    uint64_t now = 100;
    nx_baremetal_notify_t notification;
    ASSERT_EQ(nx_baremetal_notify_init(&notification, clockRead, &now),
              NX_SUCCESS);
    auto port = nx_baremetal_notify_port(&notification);
    auto sequence = port.arm(port.context);
    ASSERT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, now), NX_ERROR_TIMEOUT);
}

TEST(BaremetalWait, ContinuousHintsCannotExtendAbsoluteDeadline) {
    uint64_t now = 100;
    nx_baremetal_notify_t notification;
    ASSERT_EQ(nx_baremetal_notify_init(&notification, clockRead, &now),
              NX_SUCCESS);
    WakeStorm storm{nx_baremetal_notify_port(&notification)};
    EXPECT_EQ(nx_wait_until(&storm.port, WakeStorm::ready, &storm, now),
              NX_ERROR_TIMEOUT);
    EXPECT_LT(storm.checks, 64U);
}

TEST(BaremetalWait, NeverAndFiniteDeadlinesDoNotWrap) {
    uint64_t now = UINT64_MAX - 1;
    nx_baremetal_notify_t notification;
    ASSERT_EQ(nx_baremetal_notify_init(&notification, clockRead, &now),
              NX_SUCCESS);
    auto port = nx_baremetal_notify_port(&notification);
    auto sequence = port.arm(port.context);
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER),
              NX_ERROR_BUSY);
    EXPECT_EQ(port.wait(port.context, sequence, UINT64_MAX - 2),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_deadline_after(now, 2), NX_DEADLINE_NEVER);
    EXPECT_FALSE(nx_deadline_expired(NX_DEADLINE_NEVER, now));
}

TEST(BaremetalWait, RepeatedHintsDoNotRestartAFutureDeadline) {
    struct State {
        uint64_t now = 100;
        nx_wait_port_t port{};
        unsigned checks = 0;
    } fixture;
    nx_baremetal_notify_t notification;
    ASSERT_EQ(nx_baremetal_notify_init(&notification, clockRead, &fixture.now),
              NX_SUCCESS);
    fixture.port = nx_baremetal_notify_port(&notification);
    auto ready = [](void* context) {
        auto* state = static_cast<State*>(context);
        ++state->now;
        ++state->checks;
        state->port.wake(state->port.context);
        return state->checks > 64;
    };
    EXPECT_EQ(nx_wait_until(&fixture.port, ready, &fixture, 108),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(fixture.now, 109U);
    EXPECT_EQ(fixture.checks, 9U);
}

TEST(BaremetalWait, SequenceWrapIsAnObservedHint) {
    uint64_t now = 100;
    nx_baremetal_notify_t notification;
    ASSERT_EQ(nx_baremetal_notify_init(&notification, clockRead, &now),
              NX_SUCCESS);
    notification.sequence = UINT32_MAX;
    auto port = nx_baremetal_notify_port(&notification);
    auto sequence = port.arm(port.context);
    ASSERT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, 200), NX_SUCCESS);
    EXPECT_EQ(port.arm(port.context), 0U);
}

TEST(NativeWait, ChangedSequenceCannotBypassExpiredDeadline) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    auto port = nx_native_notify_port(&notification);
    auto sequence = port.arm(port.context);
    ASSERT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, ContinuousHintsCannotExtendAbsoluteDeadline) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    WakeStorm storm{nx_native_notify_port(&notification)};
    EXPECT_EQ(nx_wait_until(&storm.port, WakeStorm::ready, &storm, 0),
              NX_ERROR_TIMEOUT);
    EXPECT_LT(storm.checks, 64U);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, RepeatedHintsDoNotRestartAFutureDeadline) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    testing::StrictMock<ClockBoundary> clock;
    EXPECT_CALL(clock, read())
        .WillOnce(testing::Return(100))
        .WillOnce(testing::Return(101))
        .WillOnce(testing::Return(102))
        .WillOnce(testing::Return(103));
    WakeStorm storm{nx_native_notify_port(&notification)};
    selectedClock = &clock;
    nx_result_t result =
        nx_wait_until(&storm.port, WakeStorm::ready, &storm, 103);
    selectedClock = nullptr;
    EXPECT_EQ(result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(storm.checks, 9U);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, NeverUsesUntimedWaitAndSecondWaiterIsRejected) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    auto port = nx_native_notify_port(&notification);
    auto sequence = port.arm(port.context);
    boundary.select(&notification.condition);
    std::atomic<nx_result_t> result{NX_ERROR_IO};
    std::thread waiter(
        [&] { result = port.wait(port.context, sequence, NX_DEADLINE_NEVER); });
    boundary.await();
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_ERROR_BUSY);
    EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
    waiter.join();
    EXPECT_EQ(result.load(), NX_SUCCESS);
    EXPECT_GE(boundary.infinite, 1U);
    EXPECT_EQ(boundary.timed, 0U);
    boundary.select(nullptr);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, MultiplePublishersPreserveEverySequenceIncrement) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    auto port = nx_native_notify_port(&notification);
    auto sequence = port.arm(port.context);
    std::vector<std::thread> publishers;
    std::atomic<unsigned> errors{0};
    for (unsigned i = 0; i < 4; ++i) {
        publishers.emplace_back([&] {
            for (unsigned j = 0; j < 1000; ++j) {
                if (port.wake(port.context) != NX_SUCCESS) {
                    ++errors;
                }
            }
        });
    }
    for (auto& publisher : publishers) {
        publisher.join();
    }
    EXPECT_EQ(errors.load(), 0U);
    EXPECT_EQ(port.arm(port.context), sequence + 4000U);
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, SequenceWrapRemainsObservableBeforeDeadline) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    notification.sequence = UINT32_MAX;
    auto port = nx_native_notify_port(&notification);
    auto sequence = port.arm(port.context);
    ASSERT_EQ(port.wake(port.context), NX_SUCCESS);
    EXPECT_EQ(port.wait(port.context, sequence, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_EQ(port.arm(port.context), 0U);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, ActualTimeoutPreservesFinalPredicateCompletion) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    struct Predicate {
        nx_wait_port_t port;
        unsigned observations = 0;
    } predicate{nx_native_notify_port(&notification)};
    auto ready = [](void* context) {
        auto* state = static_cast<Predicate*>(context);
        ++state->observations;
        state->port.wake(state->port.context);
        return state->observations == 3;
    };
    EXPECT_EQ(nx_wait_until(&predicate.port, ready, &predicate, 0), NX_SUCCESS);
    EXPECT_EQ(predicate.observations, 3U);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, PastDeadlineNeverEntersAPosixBlockingOperation) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    boundary.select(&notification.condition);
    auto port = nx_native_notify_port(&notification);
    EXPECT_EQ(port.wait(port.context, port.arm(port.context), 0),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(boundary.infinite, 0U);
    EXPECT_EQ(boundary.timed, 0U);
    boundary.select(nullptr);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeWait, PublisherBetweenRecheckAndSleepCannotLoseWake) {
    nx_native_notify_t notification;
    ASSERT_EQ(nx_native_notify_init(&notification), NX_SUCCESS);
    struct State {
        std::mutex mutex;
        std::condition_variable gate;
        std::atomic<bool> ready{false};
        unsigned checks = 0;
        bool rechecked = false;
        bool released = false;
    } fixture;
    auto ready = [](void* context) {
        auto* state = static_cast<State*>(context);
        bool observed = state->ready.load(std::memory_order_acquire);
        if (++state->checks == 2) {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->rechecked = true;
            state->gate.notify_one();
            state->gate.wait(lock, [&] { return state->released; });
        }
        return observed;
    };
    auto port = nx_native_notify_port(&notification);
    boundary.select(&notification.condition);
    nx_result_t result = NX_ERROR_IO;
    std::thread waiter([&] {
        result = nx_wait_until(&port, ready, &fixture,
                               nx_deadline_after(nx_native_now_us(), 1000000));
    });
    {
        std::unique_lock<std::mutex> lock(fixture.mutex);
        fixture.gate.wait(lock, [&] { return fixture.rechecked; });
        fixture.ready.store(true, std::memory_order_release);
        EXPECT_EQ(port.wake(port.context), NX_SUCCESS);
        fixture.released = true;
        fixture.gate.notify_one();
    }
    waiter.join();
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(fixture.checks, 3U);
    EXPECT_EQ(boundary.timed, 0U);
    boundary.select(nullptr);
    EXPECT_EQ(nx_native_notify_destroy(&notification), NX_SUCCESS);
}

TEST(NativeQueue, CloseWakesBlockedProducerAndRetainsDrain) {
    nx_native_queue_t queue;
    uint32_t storage[1];
    ASSERT_EQ(nx_native_queue_init(&queue, storage, sizeof(storage), 1,
                                   sizeof(uint32_t)),
              NX_SUCCESS);
    uint32_t first = 1;
    ASSERT_EQ(nx_native_queue_send(&queue, &first, 0), NX_SUCCESS);
    boundary.select(&queue.writable);
    std::atomic<nx_result_t> result{NX_ERROR_IO};
    std::thread producer([&] {
        uint32_t second = 2;
        result = nx_native_queue_send(&queue, &second, NX_DEADLINE_NEVER);
    });
    boundary.await();
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    producer.join();
    EXPECT_EQ(result.load(), NX_ERROR_STATE);
    EXPECT_GE(boundary.infinite, 1U);
    EXPECT_EQ(boundary.timed, 0U);
    boundary.select(nullptr);
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_ERROR_BUSY);
    uint32_t received = 0;
    EXPECT_EQ(nx_native_queue_receive(&queue, &received, 0), NX_SUCCESS);
    EXPECT_EQ(received, first);
    EXPECT_EQ(nx_native_queue_receive(&queue, &received, 0), NX_ERROR_STATE);
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_SUCCESS);
}

TEST(NativeQueue, CloseWakesBlockedConsumerAndPreservesOutputOnRejection) {
    nx_native_queue_t queue;
    uint32_t storage[1];
    ASSERT_EQ(nx_native_queue_init(&queue, storage, sizeof(storage), 1,
                                   sizeof(uint32_t)),
              NX_SUCCESS);
    boundary.select(&queue.readable);
    std::atomic<nx_result_t> result{NX_ERROR_IO};
    uint32_t received = 99;
    std::thread consumer([&] {
        result = nx_native_queue_receive(&queue, &received, NX_DEADLINE_NEVER);
    });
    boundary.await();
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    consumer.join();
    EXPECT_EQ(result.load(), NX_ERROR_STATE);
    EXPECT_EQ(received, 99U);
    EXPECT_GE(boundary.infinite, 1U);
    EXPECT_EQ(boundary.timed, 0U);
    boundary.select(nullptr);
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_SUCCESS);
}

TEST(NativeQueue, ImmediateOperationsUseAvailableCapacityAtExpiredDeadline) {
    nx_native_queue_t queue;
    uint32_t storage[1];
    ASSERT_EQ(nx_native_queue_init(&queue, storage, sizeof(storage), 1,
                                   sizeof(uint32_t)),
              NX_SUCCESS);
    uint32_t value = 7;
    EXPECT_EQ(nx_native_queue_send(&queue, &value, 0), NX_SUCCESS);
    EXPECT_EQ(nx_native_queue_send(&queue, &value, 0), NX_ERROR_TIMEOUT);
    uint32_t received = 0;
    EXPECT_EQ(nx_native_queue_receive(&queue, &received, 0), NX_SUCCESS);
    EXPECT_EQ(received, value);
    EXPECT_EQ(nx_native_queue_receive(&queue, &received, 0), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_SUCCESS);
}

TEST(NativeQueue, MultipleProducersCloseThenDrainWithoutLosingItems) {
    nx_native_queue_t queue;
    uint32_t storage[2];
    ASSERT_EQ(nx_native_queue_init(&queue, storage, sizeof(storage), 2,
                                   sizeof(uint32_t)),
              NX_SUCCESS);
    constexpr unsigned producerCount = 4;
    constexpr unsigned itemsPerProducer = 500;
    std::vector<unsigned> received(producerCount * itemsPerProducer, 0);
    std::atomic<unsigned> errors{0};
    std::thread consumer([&] {
        for (;;) {
            uint32_t value = 0;
            nx_result_t result =
                nx_native_queue_receive(&queue, &value, NX_DEADLINE_NEVER);
            if (result == NX_ERROR_STATE) {
                break;
            }
            if (result != NX_SUCCESS || value >= received.size()) {
                ++errors;
                break;
            }
            ++received[value];
        }
    });
    std::vector<std::thread> producers;
    for (unsigned id = 0; id < producerCount; ++id) {
        producers.emplace_back([&, id] {
            for (unsigned item = 0; item < itemsPerProducer; ++item) {
                uint32_t value = id * itemsPerProducer + item;
                if (nx_native_queue_send(&queue, &value, NX_DEADLINE_NEVER) !=
                    NX_SUCCESS) {
                    ++errors;
                    break;
                }
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    consumer.join();
    EXPECT_EQ(errors.load(), 0U);
    for (unsigned count : received) {
        EXPECT_EQ(count, 1U);
    }
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_SUCCESS);
}

TEST(NativeQueue, RepeatedCloseIsIdempotentAndRejectsFutureAdmission) {
    nx_native_queue_t queue;
    uint32_t storage[1];
    ASSERT_EQ(nx_native_queue_init(&queue, storage, sizeof(storage), 1,
                                   sizeof(uint32_t)),
              NX_SUCCESS);
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    EXPECT_EQ(nx_native_queue_close(&queue), NX_SUCCESS);
    uint32_t item = 42;
    EXPECT_EQ(nx_native_queue_send(&queue, &item, NX_DEADLINE_NEVER),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_native_queue_receive(&queue, &item, NX_DEADLINE_NEVER),
              NX_ERROR_STATE);
    EXPECT_EQ(item, 42U);
    EXPECT_EQ(nx_native_queue_destroy(&queue), NX_SUCCESS);
    EXPECT_EQ(nx_native_queue_close(&queue), NX_ERROR_INVALID);
}

TEST(NativeTask, JoinRejectsSelfThenConfirmsEntryExitAndAllowsReuse) {
    NativeStack stack;
    nx_native_task_t task{};
    struct Context {
        nx_native_task_t* task = nullptr;
        std::mutex mutex;
        std::condition_variable admission;
        bool started = false;
        nx_result_t selfJoin = NX_SUCCESS;
        unsigned runs = 0;
    } context;
    context.task = &task;
    auto entry = [](void* opaque) {
        auto* state = static_cast<Context*>(opaque);
        std::unique_lock<std::mutex> lock(state->mutex);
        state->admission.wait(lock, [&] { return state->started; });
        state->selfJoin = nx_native_task_join(state->task);
        ++state->runs;
    };
    for (unsigned i = 0; i < 3; ++i) {
        context.started = false;
        ASSERT_EQ(nx_native_task_start(&task, stack.bytes, sizeof(stack.bytes),
                                       entry, &context),
                  NX_SUCCESS);
        {
            std::lock_guard<std::mutex> lock(context.mutex);
            context.started = true;
            context.admission.notify_one();
        }
        EXPECT_EQ(nx_native_task_join(&task), NX_SUCCESS);
        EXPECT_EQ(context.selfJoin, NX_ERROR_BUSY);
        EXPECT_EQ(context.runs, i + 1);
        EXPECT_FALSE(task.started);
    }
    EXPECT_EQ(nx_native_task_join(&task), NX_ERROR_INVALID);
}

TEST(NativeTask, SmallStackRejectionRetainsUnusedHandle) {
    NativeStack stack;
    nx_native_task_t task{};
    unsigned calls = 0;
    auto entry = [](void* context) { ++*static_cast<unsigned*>(context); };
    EXPECT_EQ(nx_native_task_start(&task, stack.bytes, 16, entry, &calls),
              NX_ERROR_INVALID);
    EXPECT_FALSE(task.started);
    EXPECT_EQ(task.entry, nullptr);
    EXPECT_EQ(task.context, nullptr);
    EXPECT_EQ(calls, 0U);
    EXPECT_EQ(nx_native_task_join(&task), NX_ERROR_INVALID);
}
