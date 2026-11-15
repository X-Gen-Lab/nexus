/**
 * \file            realtime_metadata_test.cpp
 *
 * \brief           Metadata progress does not inspect unrelated live slots.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/bus_owner.h"
#include "nexus/io/stream.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

/** \brief Observe saved incoming masks without substituting stream logic. */
uint32_t irq_depth;
size_t irq_saves;
size_t irq_restores;

/** \brief Supply explicit executor dependencies for the real owner. */
class Executor {
  public:
    MOCK_METHOD(nx_result_t, start, (void*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, service, (nx_result_t*, size_t*));
    MOCK_METHOD(nx_result_t, cancel, ());
};

/** \brief Bind exact storage and check dependency calls leave exclusion. */
class OwnerMetadata : public ::testing::Test {
  protected:
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[3]{};
    StrictMock<Executor> executor;
    uintptr_t depth = 0U;

    /** \brief Capture the incoming nonrecursive exclusion state. */
    static uintptr_t enter(void* context) {
        auto* fixture = static_cast<OwnerMetadata*>(context);
        EXPECT_EQ(fixture->depth, 0U);
        fixture->depth = 1U;
        return 0U;
    }

    /** \brief Restore the token rather than unconditionally enabling access. */
    static void leave(void* context, uintptr_t saved) {
        auto* fixture = static_cast<OwnerMetadata*>(context);
        EXPECT_EQ(fixture->depth, 1U);
        fixture->depth = saved;
    }

    /** \brief Model an executor start outside the metadata guard. */
    static nx_result_t start(void* context, void* item, nx_time_us_t deadline) {
        auto* fixture = static_cast<OwnerMetadata*>(context);
        EXPECT_EQ(fixture->depth, 0U);
        return fixture->executor.start(item, deadline);
    }

    /** \brief Model a drain result outside the metadata guard. */
    static nx_result_t service(void* context, nx_result_t* result,
                               size_t* transferred) {
        auto* fixture = static_cast<OwnerMetadata*>(context);
        EXPECT_EQ(fixture->depth, 0U);
        return fixture->executor.service(result, transferred);
    }

    /** \brief Request cancellation without releasing the operation. */
    static nx_result_t cancel(void* context) {
        auto* fixture = static_cast<OwnerMetadata*>(context);
        EXPECT_EQ(fixture->depth, 0U);
        return fixture->executor.cancel();
    }

    /** \brief Keep all test deadlines in one nonexpired domain. */
    static nx_time_us_t now(void*) {
        return 0U;
    }

    /** \brief Initialize unused exact owner storage. */
    void initialize(nx_owner_slot_t* storage, size_t capacity) {
        const nx_owner_guard_port_t guard = {this, enter, leave};
        const nx_owner_executor_port_t port = {this, start, service, cancel,
                                               false};
        const nx_clock_t clock = {now, this};
        ASSERT_EQ(nx_bus_owner_init(&owner, storage, capacity, guard, port,
                                    clock, nullptr),
                  NX_SUCCESS);
    }

    /** \brief Admit a fresh caller-owned operation. */
    void submit(nx_request_t& request, int& operation,
                nx_owner_ticket_t& ticket) {
        nx_request_initialize(&request);
        ASSERT_EQ(nx_request_prepare(&request, 1000U), NX_SUCCESS);
        ASSERT_EQ(
            nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
            NX_SUCCESS);
    }

    /** \brief Establish fresh three-slot fixtures. */
    void SetUp() override {
        initialize(slots, 3U);
    }
};

TEST_F(OwnerMetadata, RejectedAdmissionRestoresSlotWithoutBorrow) {
    EXPECT_EQ(owner.free_head, 0U);
    EXPECT_EQ(slots[0].next, 1U);
    EXPECT_EQ(slots[1].next, 2U);
    EXPECT_EQ(slots[2].next, owner.capacity);
    nx_request_t request{};
    nx_request_initialize(&request);
    ASSERT_EQ(nx_request_admit(&request, NX_REQUEST_ACTIVE), NX_SUCCESS);
    nx_owner_ticket_t ticket{99U, 99U};
    int operation = 0;
    EXPECT_EQ(
        nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
        NX_ERROR_STATE);
    EXPECT_EQ(ticket.slot, 99U);
    EXPECT_EQ(ticket.epoch, 99U);
    EXPECT_EQ(owner.free_head, 0U);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    nx_request_t accepted[3]{};
    int operations[3]{};
    nx_owner_ticket_t tickets[3]{};
    for (size_t i = 0U; i < 3U; ++i) {
        submit(accepted[i], operations[i], tickets[i]);
        EXPECT_EQ(tickets[i].slot, i);
    }
    EXPECT_EQ(owner.free_head, owner.capacity);
}

TEST_F(OwnerMetadata, RecycledSlotPreservesQueueFIFOAndStaleIdentity) {
    nx_request_t requests[4]{};
    int operations[4]{};
    nx_owner_ticket_t tickets[4]{};
    for (size_t i = 0U; i < 3U; ++i) {
        submit(requests[i], operations[i], tickets[i]);
    }
    ASSERT_EQ(nx_bus_owner_cancel(&owner, tickets[0]), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&requests[0]), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.free_head, tickets[0].slot);
    EXPECT_EQ(slots[tickets[0].slot].next, owner.capacity);
    submit(requests[3], operations[3], tickets[3]);
    EXPECT_EQ(tickets[3].slot, tickets[0].slot);
    EXPECT_NE(tickets[3].epoch, tickets[0].epoch);
    EXPECT_EQ(nx_bus_owner_cancel(&owner, tickets[0]), NX_ERROR_STATE);
    for (size_t i = 1U; i < 4U; ++i) {
        EXPECT_CALL(executor, start(&operations[i], 1000U))
            .WillOnce(Return(NX_ERROR_IO));
        EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    }
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}

TEST_F(OwnerMetadata, FinalEpochIsAdmittedOnceThenRetired) {
    slots[0].identity.epoch = UINT64_MAX - 1U;
    nx_request_t requests[4]{};
    int operations[4]{};
    nx_owner_ticket_t tickets[4]{};
    submit(requests[0], operations[0], tickets[0]);
    ASSERT_EQ(tickets[0].epoch, UINT64_MAX);
    ASSERT_EQ(nx_bus_owner_cancel(&owner, tickets[0]), NX_SUCCESS);
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    submit(requests[1], operations[1], tickets[1]);
    submit(requests[2], operations[2], tickets[2]);
    EXPECT_EQ(tickets[1].slot, 1U);
    EXPECT_EQ(tickets[2].slot, 2U);
    nx_request_initialize(&requests[3]);
    EXPECT_EQ(nx_bus_owner_submit(&owner, &requests[3], &operations[3], nullptr,
                                  &tickets[3]),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&requests[3]), NX_REQUEST_READY);
    EXPECT_EQ(nx_bus_owner_cancel(&owner, tickets[0]), NX_ERROR_STATE);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].next, owner.capacity);
    EXPECT_EQ(owner.free_head, owner.capacity);
}

TEST_F(OwnerMetadata, RejectedFinalEpochRetiresWithoutLosingOtherSlots) {
    slots[0].identity.epoch = UINT64_MAX - 1U;
    nx_request_t active{};
    nx_request_initialize(&active);
    ASSERT_EQ(nx_request_admit(&active, NX_REQUEST_ACTIVE), NX_SUCCESS);
    int operation = 0;
    nx_owner_ticket_t ticket{};
    EXPECT_EQ(
        nx_bus_owner_submit(&owner, &active, &operation, nullptr, &ticket),
        NX_ERROR_STATE);
    EXPECT_EQ(slots[0].identity.request, nullptr);
    EXPECT_EQ(slots[0].identity.epoch, UINT64_MAX);
    EXPECT_EQ(owner.free_head, 1U);
    nx_request_t accepted{};
    submit(accepted, operation, ticket);
    EXPECT_EQ(ticket.slot, 1U);
}

#if !defined(_WIN32)
/** \brief Provide page boundaries for actual metadata access regressions. */
class Pages {
  public:
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void* memory = mmap(nullptr, page * 2U, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    /** \brief Restore access and release unused test-owned mappings. */
    ~Pages() {
        if (memory != MAP_FAILED) {
            (void)mprotect(memory, page * 2U, PROT_READ | PROT_WRITE);
            (void)munmap(memory, page * 2U);
        }
    }
};

TEST_F(OwnerMetadata, AdmissionDoesNotReadOccupiedSlotStorage) {
    Pages pages;
    ASSERT_NE(pages.memory, MAP_FAILED);
    auto* storage = static_cast<nx_owner_slot_t*>(pages.memory);
    const size_t unavailable =
        (pages.page + sizeof(*storage) - 1U) / sizeof(*storage) + 1U;
    initialize(storage, unavailable + 1U);
    std::vector<nx_request_t> requests(unavailable + 1U);
    std::vector<int> operations(unavailable + 1U);
    std::vector<nx_owner_ticket_t> tickets(unavailable + 1U);
    for (size_t i = 0U; i < unavailable; ++i) {
        submit(requests[i], operations[i], tickets[i]);
    }
    nx_request_initialize(&requests[unavailable]);
    ASSERT_EQ(mprotect(pages.memory, pages.page, PROT_NONE), 0);
    EXPECT_EXIT(
        {
            nx_result_t result = nx_bus_owner_submit(
                &owner, &requests[unavailable], &operations[unavailable],
                nullptr, &tickets[unavailable]);
            _exit(result == NX_SUCCESS &&
                          tickets[unavailable].slot == unavailable
                      ? 0
                      : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(StreamMetadata, StopDoesNotReadBlockArray) {
    Pages pages;
    ASSERT_NE(pages.memory, MAP_FAILED);
    auto* slots = static_cast<nx_stream_slot_t*>(pages.memory);
    const size_t count = pages.page / sizeof(*slots);
    std::vector<uint8_t> payload(count * 4U);
    for (size_t i = 0U; i < count; ++i) {
        slots[i].data = payload.data() + i * 4U;
        slots[i].capacity = 4U;
    }
    nx_stream_t stream{};
    ASSERT_EQ(nx_stream_initialize(&stream, slots, count), NX_SUCCESS);
    ASSERT_EQ(mprotect(pages.memory, pages.page, PROT_NONE), 0);
    EXPECT_EXIT({ _exit(nx_stream_stop(&stream) == NX_SUCCESS ? 0 : 1); },
                ::testing::ExitedWithCode(0), "");
}
#endif

TEST(StreamMetadata, LoansDrainDuringStopAndRestoreIncomingMask) {
    uint8_t payload[12]{};
    nx_stream_slot_t slots[3]{};
    for (size_t i = 0U; i < 3U; ++i) {
        slots[i].data = payload + i * 4U;
        slots[i].capacity = 4U;
    }
    nx_stream_t stream{};
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
    irq_depth = 7U;
    irq_saves = 0U;
    irq_restores = 0U;
    nx_stream_fill_t fill{};
    nx_stream_block_t held[2]{};
    EXPECT_EQ(stream.outstanding, 0U);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_publish(&stream, &fill, 5U, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 1U);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &held[0]), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 1U);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_stream_publish(&stream, &fill, 1U, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, false), NX_ERROR_BUSY);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_EQ(nx_stream_release(&stream, &held[0]), NX_SUCCESS);
    EXPECT_EQ(nx_stream_release(&stream, &held[0]), NX_ERROR_STATE);
    EXPECT_EQ(stream.outstanding, 2U);
    ASSERT_EQ(nx_stream_acquire(&stream, &held[1]), NX_SUCCESS);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, true), NX_SUCCESS);
    EXPECT_EQ(nx_stream_abort(&stream, &fill, true), NX_ERROR_STATE);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(nx_stream_stop(&stream), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &held[1]), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(nx_stream_stop(&stream), NX_SUCCESS);
    EXPECT_FALSE(nx_stream_can_reserve(&stream));
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_STATE);
    EXPECT_EQ(irq_depth, 7U);
    EXPECT_EQ(irq_saves, 20U);
    EXPECT_EQ(irq_restores, irq_saves);
    irq_depth = 0U;
}
} /* namespace */

/** \brief Replace the architecture dependency, not production stream code. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    ++irq_saves;
    const nx_arch_irq_state_t previous = {irq_depth};
    ++irq_depth;
    return previous;
}

/** \brief Assert matched nesting and restore the exact incoming token. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    EXPECT_EQ(irq_depth, previous.value + 1U);
    irq_depth = previous.value;
    ++irq_restores;
}

/** \brief Publication ordering is an observed dependency of stream publish. */
extern "C" void nx_arch_dsb(void) {
    EXPECT_GT(irq_depth, 0U);
}
