/**
 * \file            u575_secure_batch_test.cpp
 * \brief           Real copied Secure job state machine and GPIO boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_secure_batch.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using testing::Return;
using testing::StrictMock;

class U575SecureBatch : public testing::Test {
  protected:
    StrictMock<testing::MockFunction<nx_result_t(uint32_t, uint32_t)>> output;
    nx_gpio_ops_t ops{};
    nx_gpio_port_t gpio{&ops, this};
    nx_stm32u575_secure_batch_t slot{};
    nx_stm32u575_secure_gpio_command_t commands[2]{{0x80U, 0U}, {0U, 0x80U}};
    uint32_t ticket = 0U;
    void SetUp() override {
        ops.write = [](void* context, uint32_t set, uint32_t clear) {
            return static_cast<U575SecureBatch*>(context)->output.Call(set,
                                                                       clear);
        };
        slot.port = &gpio;
        slot.allowed = 0x80U;
    }
    void Submit() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_submit(&slot, commands,
                                                               2U, &ticket));
        ASSERT_NE(0U, ticket);
    }
};
TEST_F(U575SecureBatch, AdmissionCopiesWithoutOutputAndServiceIsOneCommand) {
    Submit();
    commands[0].set = 0U;
    EXPECT_CALL(output, Call(0x80U, 0U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_service(&slot, ticket));
    EXPECT_EQ(1U, slot.status.completed);
    EXPECT_EQ(NX_STM32U575_BATCH_RUNNING, slot.status.phase);
    EXPECT_CALL(output, Call(0U, 0x80U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_service(&slot, ticket));
    EXPECT_EQ(NX_STM32U575_BATCH_COMPLETE, slot.status.phase);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_secure_batch_service(&slot, ticket));
}
TEST_F(U575SecureBatch, RejectsWholeListBeforeCopyOrWrite) {
    auto before = slot;
    commands[1] = {1U, 1U};
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_stm32u575_secure_batch_submit(&slot, commands, 2U, &ticket));
    EXPECT_EQ(0, std::memcmp(&before, &slot, sizeof(slot)));
    EXPECT_EQ(0U, ticket);
}
TEST_F(U575SecureBatch, CancelSettlesFutureCommandsAndDoesNotRewindWrites) {
    Submit();
    EXPECT_CALL(output, Call(0x80U, 0U)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_service(&slot, ticket));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_cancel(&slot, ticket));
    EXPECT_EQ(NX_STM32U575_BATCH_CANCELLED, slot.status.phase);
    EXPECT_EQ(1U, slot.status.completed);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_secure_batch_service(&slot, ticket));
}
TEST_F(U575SecureBatch, RunningAndUnconsumedTerminalJobsCannotBeReplaced) {
    Submit();
    uint32_t other = 999U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_secure_batch_submit(&slot, commands, 2U, &other));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_secure_batch_consume(&slot, ticket));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_cancel(&slot, ticket));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_secure_batch_submit(&slot, commands, 2U, &other));
    EXPECT_EQ(999U, other);
}
TEST_F(U575SecureBatch, OldTicketCannotPollCancelOrExecuteReusedSlot) {
    Submit();
    const uint32_t old = ticket;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_cancel(&slot, ticket));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_secure_batch_consume(&slot, ticket));
    Submit();
    EXPECT_GT(ticket, old);
    nx_stm32u575_secure_batch_status_t status{77U, 88U, NX_ERROR_IO,
                                              NX_STM32U575_BATCH_FAILED};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_secure_batch_poll(&slot, old, &status));
    EXPECT_EQ(77U, status.epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_secure_batch_cancel(&slot, old));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_secure_batch_service(&slot, old));
}
TEST_F(U575SecureBatch, FailedOutputLatchesErrorWithoutExecutingLaterCommands) {
    Submit();
    EXPECT_CALL(output, Call(0x80U, 0U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_secure_batch_service(&slot, ticket));
    EXPECT_EQ(NX_STM32U575_BATCH_FAILED, slot.status.phase);
    EXPECT_EQ(0U, slot.status.completed);
    nx_stm32u575_secure_batch_status_t status{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_secure_batch_poll(&slot, ticket, &status));
    EXPECT_EQ(NX_ERROR_IO, status.result);
}
TEST_F(U575SecureBatch, EpochNeverWrapsAndAliasedInputOrOutputRejects) {
    slot.status.epoch = UINT32_MAX;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_secure_batch_submit(&slot, commands, 2U, &ticket));
    slot.status.epoch = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_secure_batch_submit(
                                    &slot, slot.commands, 1U, &ticket));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_secure_batch_submit(
                                    &slot, commands, 2U, &slot.count));
    Submit();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_secure_batch_poll(&slot, ticket, &slot.status));
}
TEST_F(U575SecureBatch, WireDispatchPreservesTicketAndRejectsInvalidActions) {
    nx_stm32u575_secure_batch_message_t message{};
    nx_stm32u575_secure_batch_reply_t reply{99U, 99U, 99U, 99U};
    message.action = 999U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_secure_batch_dispatch(&slot, &message, &reply));
    EXPECT_EQ(99U, reply.ticket);
    message.action = NX_STM32U575_BATCH_SUBMIT;
    message.count = 1U;
    message.commands[0] = commands[0];
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_secure_batch_dispatch(&slot, &message, &reply));
    ASSERT_NE(0U, reply.ticket);
    message.ticket = reply.ticket;
    message.action = NX_STM32U575_BATCH_POLL;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_secure_batch_dispatch(&slot, &message, &reply));
    message.count = 0U;
    EXPECT_CALL(output, Call(0x80U, 0U)).WillOnce(Return(NX_SUCCESS));
    message.action = NX_STM32U575_BATCH_SERVICE;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_secure_batch_dispatch(&slot, &message, &reply));
    EXPECT_EQ(1U, reply.completed);
    EXPECT_EQ(NX_STM32U575_BATCH_COMPLETE, reply.phase);
    message.action = NX_STM32U575_BATCH_CONSUME;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_secure_batch_dispatch(&slot, &message, &reply));
    EXPECT_EQ(NX_STM32U575_BATCH_IDLE, reply.phase);
}
TEST(U575SecureSpans, ChecksCompleteBoundsAlignmentOverflowAndAlias) {
    const uintptr_t base = 0x20040000U;
    auto* message =
        reinterpret_cast<nx_stm32u575_secure_batch_message_t*>(base);
    auto* reply =
        reinterpret_cast<nx_stm32u575_secure_batch_reply_t*>(base + 80U);
    EXPECT_TRUE(nx_stm32u575_secure_batch_ns_spans(base, 128U, message, reply));
    EXPECT_FALSE(nx_stm32u575_secure_batch_ns_spans(base, 88U, message, reply));
    EXPECT_FALSE(
        nx_stm32u575_secure_batch_ns_spans(base + 4U, 128U, message, reply));
    EXPECT_FALSE(nx_stm32u575_secure_batch_ns_spans(
        base, 128U, message,
        reinterpret_cast<nx_stm32u575_secure_batch_reply_t*>(base + 4U)));
    EXPECT_FALSE(nx_stm32u575_secure_batch_ns_spans(
        base, 128U,
        reinterpret_cast<nx_stm32u575_secure_batch_message_t*>(base + 1U),
        reply));
    EXPECT_FALSE(nx_stm32u575_secure_batch_ns_spans(UINTPTR_MAX - 7U, 128U,
                                                    message, reply));
    EXPECT_FALSE(
        nx_stm32u575_secure_batch_ns_spans(base, 128U, nullptr, reply));
}
