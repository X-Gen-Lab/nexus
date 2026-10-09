#include "hal/runtime/nx_completion.h"
#include "arch/nx_arch.h"
#include <gtest/gtest.h>
#include <atomic>
#include <array>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

static thread_local bool in_isr;
extern "C" bool completion_test_in_isr(void) { return in_isr; }

namespace {
const nx_hal_completion_result_t success{NX_OK, true};
struct Observations {
    std::vector<uint64_t> sequences;
    std::vector<nx_status_t> statuses;
    bool masked = false;
};
void record(void* context, nx_hal_completion_ticket_t ticket,
            const nx_hal_completion_result_t* result) {
    auto& observations = *static_cast<Observations*>(context);
    observations.sequences.push_back(ticket.sequence);
    observations.statuses.push_back(result->status);
    observations.masked |= nx_arch_irq_is_masked();
    EXPECT_TRUE(result->settled);
    EXPECT_FALSE(in_isr);
}
class Completion : public ::testing::Test {
protected:
    nx_hal_completion_queue_t queue{};
    nx_hal_completion_slot_t slots[4]{};
    uint32_t entries[2]{};
    Observations observations;
    void SetUp() override {
        in_isr = false;
        ASSERT_EQ(nx_hal_completion_init(&queue, slots, 4, entries, 2), NX_OK);
    }
    nx_hal_completion_ticket_t arm() {
        nx_hal_completion_ticket_t ticket{};
        EXPECT_EQ(nx_hal_completion_arm(&queue, record, &observations, &ticket), NX_OK);
        return ticket;
    }
    uint32_t dispatch(uint32_t limit = 8) {
        uint32_t count = 999;
        EXPECT_EQ(nx_hal_completion_dispatch(&queue, limit, &count), NX_OK);
        return count;
    }
};

TEST_F(Completion, ExplicitPumpDeliversOnlyInTaskOutsideLock) {
    auto ticket = arm();
    in_isr = true;
    ASSERT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    EXPECT_TRUE(observations.sequences.empty());
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 1, &count), NX_ERR_CONTEXT);
    EXPECT_EQ(count, 0u);
    in_isr = false;
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(observations.sequences, std::vector<uint64_t>{ticket.sequence});
    EXPECT_FALSE(observations.masked);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

TEST_F(Completion, FullRetainsTicketAndRetryPreservesFifo) {
    auto first = arm(), second = arm(), third = arm();
    ASSERT_EQ(nx_hal_completion_post(&queue, first, success), NX_OK);
    ASSERT_EQ(nx_hal_completion_post(&queue, second, {NX_ERR_IO, true}), NX_OK);
    EXPECT_EQ(nx_hal_completion_post(&queue, third, {NX_ERR_CANCELLED, true}), NX_ERR_FULL);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_BUSY);
    EXPECT_EQ(dispatch(1), 1u);
    ASSERT_EQ(nx_hal_completion_post(&queue, third, {NX_ERR_CANCELLED, true}), NX_OK);
    EXPECT_EQ(dispatch(), 2u);
    EXPECT_EQ(observations.sequences,
        (std::vector<uint64_t>{first.sequence, second.sequence, third.sequence}));
    EXPECT_EQ(observations.statuses,
        (std::vector<nx_status_t>{NX_OK, NX_ERR_IO, NX_ERR_CANCELLED}));
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

TEST_F(Completion, DuplicateBeforeAndAfterDeliveryCannotRepeatCallback) {
    auto ticket = arm();
    ASSERT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, {NX_ERR_CANCELLED, true}), NX_ERR_INVALID_STATE);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_ERR_INVALID_STATE);
    EXPECT_EQ(dispatch(), 0u);
    EXPECT_EQ(observations.sequences.size(), 1u);
}

TEST_F(Completion, ReusedSlotRejectsOldTicket) {
    auto old = arm();
    ASSERT_EQ(nx_hal_completion_post(&queue, old, success), NX_OK);
    ASSERT_EQ(dispatch(), 1u);
    auto next = arm();
    EXPECT_EQ(next.slot, old.slot);
    EXPECT_GT(next.sequence, old.sequence);
    EXPECT_EQ(nx_hal_completion_post(&queue, old, success), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_BUSY);
    EXPECT_EQ(nx_hal_completion_post(&queue, next, success), NX_OK);
    EXPECT_EQ(dispatch(), 1u);
}

TEST_F(Completion, UnsettledCancelTimeoutAndFaultRetainOwnership) {
    auto ticket = arm();
    for (auto status : {NX_OK, NX_ERR_CANCELLED, NX_ERR_TIMEOUT, NX_ERR_HARDWARE}) {
        EXPECT_EQ(nx_hal_completion_post(&queue, ticket, {status, false}), NX_ERR_INVALID_STATE);
        EXPECT_EQ(dispatch(), 0u);
        EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_BUSY);
    }
    EXPECT_TRUE(observations.sequences.empty());
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, {NX_ERR_CANCELLED, true}), NX_OK);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(observations.statuses, std::vector<nx_status_t>{NX_ERR_CANCELLED});
}

TEST_F(Completion, PoolExhaustionInvalidatesOutputWithoutDiscardingArmedWork) {
    std::vector<nx_hal_completion_ticket_t> tickets;
    for (int i = 0; i < 4; ++i) tickets.push_back(arm());
    nx_hal_completion_ticket_t out = tickets[0];
    EXPECT_EQ(nx_hal_completion_arm(&queue, record, &observations, &out), NX_ERR_NO_RESOURCE);
    EXPECT_EQ(out.queue, nullptr);
    EXPECT_EQ(out.sequence, 0u);
    EXPECT_EQ(out.slot, 0u);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_BUSY);
    for (auto ticket : tickets) {
        ASSERT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
        EXPECT_EQ(dispatch(), 1u);
    }
    EXPECT_EQ(observations.sequences.size(), 4u);
}

TEST_F(Completion, SequenceExhaustionIsPermanentAcrossDeinitAndReinit) {
    // Fault fixture models a queue that has issued all but its final identity.
    queue.last_sequence = std::numeric_limits<uint64_t>::max() - 1;
    auto last = arm();
    EXPECT_EQ(last.sequence, std::numeric_limits<uint64_t>::max());
    ASSERT_EQ(nx_hal_completion_post(&queue, last, success), NX_OK);
    ASSERT_EQ(dispatch(), 1u);
    ASSERT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
    ASSERT_EQ(nx_hal_completion_init(&queue, slots, 4, entries, 2), NX_OK);
    nx_hal_completion_ticket_t out{};
    EXPECT_EQ(nx_hal_completion_arm(&queue, record, &observations, &out), NX_ERR_NO_RESOURCE);
    EXPECT_EQ(nx_hal_completion_post(&queue, last, success), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

TEST_F(Completion, ReinitializationNeverRepeatsOldIdentity) {
    auto old = arm();
    ASSERT_EQ(nx_hal_completion_post(&queue, old, success), NX_OK);
    ASSERT_EQ(dispatch(), 1u);
    ASSERT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
    ASSERT_EQ(nx_hal_completion_init(&queue, slots, 4, entries, 2), NX_OK);
    auto next = arm();
    EXPECT_GT(next.sequence, old.sequence);
    EXPECT_EQ(nx_hal_completion_post(&queue, old, success), NX_ERR_INVALID_STATE);
    ASSERT_EQ(nx_hal_completion_post(&queue, next, success), NX_OK);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(observations.sequences.size(), 2u);
}

TEST_F(Completion, IdentityIncludesQueueAndBounds) {
    auto ticket = arm();
    nx_hal_completion_queue_t other{};
    nx_hal_completion_slot_t other_slots[1]{};
    uint32_t other_entries[1]{};
    ASSERT_EQ(nx_hal_completion_init(&other, other_slots, 1, other_entries, 1), NX_OK);
    EXPECT_EQ(nx_hal_completion_post(&other, ticket, success), NX_ERR_INVALID_STATE);
    auto invalid = ticket;
    invalid.sequence = 0;
    EXPECT_EQ(nx_hal_completion_post(&queue, invalid, success), NX_ERR_INVALID_STATE);
    invalid = ticket;
    invalid.slot = 5;
    EXPECT_EQ(nx_hal_completion_post(&queue, invalid, success), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(nx_hal_completion_deinit(&other), NX_OK);
}

TEST_F(Completion, EmptyAndLimitBoundCallbackCount) {
    EXPECT_EQ(dispatch(), 0u);
    auto first = arm(), second = arm();
    ASSERT_EQ(nx_hal_completion_post(&queue, first, success), NX_OK);
    ASSERT_EQ(nx_hal_completion_post(&queue, second, success), NX_OK);
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 0, &count), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(dispatch(1), 1u);
    EXPECT_EQ(observations.sequences.size(), 1u);
    EXPECT_EQ(dispatch(1), 1u);
    EXPECT_EQ(dispatch(1), 0u);
}

TEST_F(Completion, TaskOperationsRejectIsrAndMasksButPostRestoresMask) {
    auto ticket = arm();
    nx_hal_completion_ticket_t out{};
    in_isr = true;
    EXPECT_EQ(nx_hal_completion_arm(&queue, record, &observations, &out), NX_ERR_CONTEXT);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_CONTEXT);
    nx_hal_completion_queue_t extra{};
    EXPECT_EQ(nx_hal_completion_init(&extra, slots, 4, entries, 2), NX_ERR_CONTEXT);
    in_isr = false;
    auto previous = nx_arch_irq_save();
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_hal_completion_arm(&queue, record, &observations, &out), NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_INVALID_STATE);
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 1, &count), NX_ERR_INVALID_STATE);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(previous);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(dispatch(), 1u);
}

struct Reentrant {
    nx_hal_completion_queue_t* queue;
    unsigned calls = 0;
};
void reenter(void* context, nx_hal_completion_ticket_t ticket,
             const nx_hal_completion_result_t*) {
    auto& state = *static_cast<Reentrant*>(context);
    ++state.calls;
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_hal_completion_post(state.queue, ticket, success), NX_ERR_BUSY);
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(state.queue, 1, &count), NX_ERR_BUSY);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(nx_hal_completion_deinit(state.queue), NX_ERR_BUSY);
    nx_hal_completion_ticket_t out{};
    EXPECT_EQ(nx_hal_completion_arm(state.queue, reenter, context, &out), NX_ERR_NO_RESOURCE);
}
TEST_F(Completion, CallbackReentryRetainsSlotAndPreventsSecondConsumer) {
    ASSERT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
    ASSERT_EQ(nx_hal_completion_init(&queue, slots, 1, entries, 1), NX_OK);
    Reentrant state{&queue};
    nx_hal_completion_ticket_t ticket{};
    ASSERT_EQ(nx_hal_completion_arm(&queue, reenter, &state, &ticket), NX_OK);
    ASSERT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(state.calls, 1u);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

struct BlockingCallback {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    unsigned calls = 0;
};
void block(void* context, nx_hal_completion_ticket_t,
           const nx_hal_completion_result_t*) {
    auto& state = *static_cast<BlockingCallback*>(context);
    std::unique_lock<std::mutex> lock(state.mutex);
    ++state.calls;
    state.entered = true;
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return state.release; });
}
TEST_F(Completion, RealConcurrentConsumerAndProducerKeepCallbackOwnership) {
    BlockingCallback state;
    nx_hal_completion_ticket_t ticket{};
    ASSERT_EQ(nx_hal_completion_arm(&queue, block, &state, &ticket), NX_OK);
    ASSERT_EQ(nx_hal_completion_post(&queue, ticket, success), NX_OK);
    nx_status_t worker_status = NX_ERR_GENERIC;
    uint32_t worker_count = 0;
    std::thread consumer([&] {
        worker_status = nx_hal_completion_dispatch(&queue, 1, &worker_count);
    });
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.changed.wait(lock, [&] { return state.entered; });
    }
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 1, &count), NX_ERR_BUSY);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_BUSY);
    EXPECT_EQ(nx_hal_completion_post(&queue, ticket, {NX_ERR_CANCELLED, true}), NX_ERR_BUSY);
    auto next = arm();
    EXPECT_EQ(nx_hal_completion_post(&queue, next, success), NX_OK);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.release = true;
        state.changed.notify_all();
    }
    consumer.join();
    EXPECT_EQ(worker_status, NX_OK);
    EXPECT_EQ(worker_count, 1u);
    EXPECT_EQ(state.calls, 1u);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(observations.sequences.size(), 1u);
}

TEST_F(Completion, RealRacingDuplicateProducersHaveOneWinner) {
    auto ticket = arm();
    std::mutex mutex;
    std::condition_variable changed;
    bool start = false;
    unsigned ready = 0;
    std::atomic<unsigned> accepted{0}, duplicate{0};
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 8; ++i) {
        workers.emplace_back([&, i] {
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++ready;
                changed.notify_all();
                changed.wait(lock, [&] { return start; });
            }
            nx_hal_completion_result_t terminal{
                i % 2 ? NX_ERR_CANCELLED : NX_OK, true};
            auto status = nx_hal_completion_post(&queue, ticket, terminal);
            if (status == NX_OK) ++accepted;
            else if (status == NX_ERR_INVALID_STATE) ++duplicate;
        });
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&] { return ready == 8; });
        start = true;
        changed.notify_all();
    }
    for (auto& worker : workers) worker.join();
    EXPECT_EQ(accepted.load(), 1u);
    EXPECT_EQ(duplicate.load(), 7u);
    EXPECT_EQ(dispatch(), 1u);
    EXPECT_EQ(observations.sequences.size(), 1u);
}

TEST_F(Completion, BoundedConcurrentDistinctProducersRetryFullAcrossRingWraps) {
    // 32 finite rounds drive 128 distinct tickets and repeated FIFO/free-list
    // wrap. There is no consumer while the four producers race: two must see
    // FULL and retain their identity for the explicit second admission pass.
    for (unsigned round = 0; round < 32; ++round) {
        std::array<nx_hal_completion_ticket_t, 4> tickets;
        std::array<nx_status_t, 4> results{};
        for (auto& ticket : tickets) ticket = arm();
        std::mutex mutex;
        std::condition_variable changed;
        bool start = false;
        unsigned ready = 0;
        std::vector<std::thread> workers;
        for (unsigned i = 0; i < 4; ++i) {
            workers.emplace_back([&, i] {
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    ++ready;
                    changed.notify_all();
                    changed.wait(lock, [&] { return start; });
                }
                results[i] = nx_hal_completion_post(&queue, tickets[i], success);
            });
        }
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock, [&] { return ready == 4; });
            start = true;
            changed.notify_all();
        }
        for (auto& worker : workers) worker.join();
        unsigned accepted = 0, full = 0;
        for (auto result : results) {
            if (result == NX_OK) ++accepted;
            else if (result == NX_ERR_FULL) ++full;
            else ADD_FAILURE() << "unexpected producer status " << result;
        }
        EXPECT_EQ(accepted, 2u);
        EXPECT_EQ(full, 2u);
        EXPECT_EQ(dispatch(), 2u);
        for (unsigned i = 0; i < 4; ++i) {
            if (results[i] == NX_ERR_FULL) {
                ASSERT_EQ(nx_hal_completion_post(&queue, tickets[i], success), NX_OK);
            }
        }
        EXPECT_EQ(dispatch(), 2u);
    }
    EXPECT_EQ(observations.sequences.size(), 128u);
    std::set<uint64_t> unique(observations.sequences.begin(), observations.sequences.end());
    EXPECT_EQ(unique.size(), 128u);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

struct SpawnCallback {
    nx_hal_completion_queue_t* queue;
    Observations* observations;
    nx_hal_completion_ticket_t spawned{};
    unsigned calls = 0;
};
void spawn(void* context, nx_hal_completion_ticket_t,
           const nx_hal_completion_result_t*) {
    auto& state = *static_cast<SpawnCallback*>(context);
    ++state.calls;
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_hal_completion_arm(state.queue, record, state.observations,
                                   &state.spawned), NX_OK);
    EXPECT_EQ(nx_hal_completion_post(state.queue, state.spawned, success), NX_OK);
    EXPECT_TRUE(state.observations->sequences.empty());
}
TEST_F(Completion, CallbackCanArmAndPostDifferentTicketWithinDispatchLimit) {
    SpawnCallback state{&queue, &observations};
    nx_hal_completion_ticket_t first{};
    ASSERT_EQ(nx_hal_completion_arm(&queue, spawn, &state, &first), NX_OK);
    ASSERT_EQ(nx_hal_completion_post(&queue, first, success), NX_OK);
    EXPECT_EQ(dispatch(2), 2u);
    EXPECT_EQ(state.calls, 1u);
    EXPECT_NE(state.spawned.slot, first.slot);
    EXPECT_EQ(observations.sequences, std::vector<uint64_t>{state.spawned.sequence});
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
}

TEST_F(Completion, LifecycleErrorsAndInvalidArgumentsDoNotPublishTickets) {
    EXPECT_EQ(nx_hal_completion_init(&queue, slots, 4, entries, 2), NX_ERR_ALREADY_INIT);
    nx_hal_completion_queue_t extra{};
    EXPECT_EQ(nx_hal_completion_init(&extra, slots, 0, entries, 1), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_hal_completion_init(&extra, slots, 1, entries, 2), NX_ERR_INVALID_PARAM);
    EXPECT_EQ(nx_hal_completion_init(nullptr, slots, 1, entries, 1), NX_ERR_NULL_PTR);
    nx_hal_completion_ticket_t out{&queue, 9, 1};
    EXPECT_EQ(nx_hal_completion_arm(&extra, record, &observations, &out), NX_ERR_NOT_INIT);
    EXPECT_EQ(out.sequence, 0u);
    EXPECT_EQ(nx_hal_completion_arm(&queue, nullptr, nullptr, &out), NX_ERR_NULL_PTR);
    EXPECT_EQ(nx_hal_completion_arm(&queue, record, nullptr, nullptr), NX_ERR_NULL_PTR);
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 1, nullptr), NX_ERR_NULL_PTR);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_OK);
    EXPECT_EQ(nx_hal_completion_deinit(&queue), NX_ERR_NOT_INIT);
    uint32_t count = 999;
    EXPECT_EQ(nx_hal_completion_dispatch(&queue, 1, &count), NX_ERR_NOT_INIT);
    EXPECT_EQ(count, 0u);
}
} // namespace
