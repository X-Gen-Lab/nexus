/**
 * \file            i2c_register_test.cpp
 *
 * \brief           Production I2C receive windows, prefix and fault contracts
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_provider.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
uint64_t model_time;
uint32_t model_mask;
uint32_t model_ordering;
bool model_isr;
bool model_privileged;
size_t model_message;
size_t model_remaining;
size_t model_received;
size_t model_starts;
bool model_address;
uint16_t model_endpoint_address;
uint8_t model_address_bits;
bool model_low_address;
bool model_transmitter_address;
bool model_read_header;
bool model_internal_restart;
std::vector<uint8_t> model_address_bytes;
bool model_sb_pending;
bool model_restart_on_last_read;
size_t model_duplicate_starts;
bool model_hold_stop;
size_t model_stop_polls;
size_t model_release_stop_after;
bool model_hold_master;
bool model_complete_on_last_read;
bool model_stop_completed;
uint32_t model_error;
size_t model_error_after;
size_t model_stall_after;
std::vector<nx_i2c_message_t> model_messages;
testing::MockFunction<void(size_t, bool)>* model_boundary;

using State = nx_stm32_i2c_state_t;
using EndpointState = nx_stm32_i2c_endpoint_state_t;
constexpr uint32_t ack = I2C_CR1_ACK;
constexpr uint32_t pos = I2C_CR1_POS;
constexpr uint32_t start = I2C_CR1_START;
constexpr uint32_t stop = I2C_CR1_STOP;
constexpr uint32_t rbne = I2C_SR1_RXNE;
constexpr uint32_t btc = I2C_SR1_BTF;
constexpr uint32_t nack = I2C_SR1_AF;
constexpr uint32_t arbitration = I2C_SR1_ARLO;
constexpr uint32_t bus_error = I2C_SR1_BERR;
volatile uint32_t& control(State* port) {
    return port->registers->CR1;
}
volatile uint32_t& status(State* port) {
    return port->registers->SR1;
}

class I2CRegisters : public testing::Test {
  protected:
    State state = {};
    EndpointState endpoint_state = {};
    nx_i2c_endpoint_t endpoint = {};
    nx_i2c_port_t port = {};
    testing::StrictMock<testing::MockFunction<void(size_t, bool)>> boundary;
    uint8_t data[256] = {};
    size_t transferred = 0;
    I2C_TypeDef registers = {};
    GPIO_TypeDef lines = {};

    void SetUp() override {
        model_time = 0;
        model_mask = 0;
        model_isr = false;
        model_privileged = true;
        model_message = 0;
        model_remaining = 0;
        model_received = 0;
        model_starts = 0;
        model_address = false;
        model_endpoint_address = 0x50U;
        model_address_bits = 7U;
        model_low_address = false;
        model_transmitter_address = false;
        model_read_header = false;
        model_internal_restart = false;
        model_address_bytes.clear();
        model_sb_pending = false;
        model_restart_on_last_read = false;
        model_duplicate_starts = 0;
        model_hold_stop = false;
        model_stop_polls = model_release_stop_after = 0U;
        model_hold_master = false;
        model_complete_on_last_read = false;
        model_stop_completed = false;
        model_error = 0;
        model_error_after = 0;
        model_stall_after = SIZE_MAX;
        model_messages.clear();
        model_boundary = &boundary;
        std::memset(data, 0xCC, sizeof(data));
        state.registers = &registers;
        state.line_gpio = &lines;
        state.line_mask = (1U << 6U) | (1U << 7U);
        lines.IDR = state.line_mask;
        state.peripheral_mhz = 42;
        state.rate_hz = 100000;
        ASSERT_EQ(nx_stm32_i2c_initialize(&state), NX_SUCCESS);
        endpoint_state = {&state, 0x50, 7U};
        endpoint = {&nx_stm32_i2c_endpoint_ops, &endpoint_state};
        port = {&nx_stm32_i2c_ops, &state};
    }

    void TearDown() override {
        model_boundary = nullptr;
    }

    nx_result_t transact(std::vector<nx_i2c_message_t> messages,
                         uint64_t deadline = 1000) {
        model_remaining = 0;
        model_address = false;
        model_sb_pending = false;
        model_messages = std::move(messages);
        return nx_i2c_endpoint_transaction(&endpoint, model_messages.data(),
                                           model_messages.size(), deadline,
                                           &transferred);
    }
};
} /* namespace */

/** \brief Model monotonic elapsed time without manufacturing protocol flags. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++model_time;
}

/** \brief Save the incoming mask used by protected hardware receive windows. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t incoming = {model_mask};
    model_mask = 1;
    return incoming;
}
/** \brief Restore exactly the incoming interrupt mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t incoming) {
    model_mask = incoming.value;
}
/** \brief Report an injected exception context. */
extern "C" bool nx_arch_in_isr(void) {
    return model_isr;
}
/** \brief Report an injected incoming interrupt mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return model_mask != 0;
}
/** \brief Host ordering does not qualify physical peripheral timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief Both MCU models share one explicit CPU interrupt-mask domain. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {model_mask, 0U, 0U};
}
/** \brief The register fixture injects one external exception identity. */
extern "C" uint32_t nx_arch_exception_number(void) {
    return model_isr ? 16U : 0U;
}
/** \brief Unprivileged CPU faults are exercised by the dedicated Arch model. */
extern "C" bool nx_arch_is_privileged(void) {
    return model_privileged;
}
/** \brief Preserve host ordering without claiming hardware timing. */
extern "C" void nx_arch_dmb(void) {
    (void)__atomic_fetch_add(&model_ordering, 0U, __ATOMIC_SEQ_CST);
}
/** \brief The host register fixture has no physical instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}
/** \brief No physical DWT counter exists in this transaction fixture. */
extern "C" bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}

/** \brief Drive SB, ADDR and receive facts in response to real register writes.
 */
extern "C" void nx_i2c_model_poll(void* raw) {
    auto* state = static_cast<State*>(raw);
    if (model_stop_completed) {
        EXPECT_EQ(control(state) & stop, 0U);
    }
    if ((control(state) & stop) != 0 &&
        (model_messages.empty() || model_remaining == 0 ||
         (model_error != 0 && model_received >= model_error_after) ||
         model_received >= model_stall_after)) {
        ++model_stop_polls;
        if (!model_hold_stop ||
            (model_release_stop_after != 0U &&
             model_stop_polls >= model_release_stop_after)) {
            control(state) &= ~stop;
            if (!model_hold_master) {
                state->registers->SR2 = 0;
            }
        }
        return;
    }
    if (model_error != 0 && model_received >= model_error_after) {
        if (model_error == arbitration) {
            state->registers->SR2 &= ~static_cast<uint32_t>(I2C_SR2_MSL);
        }
        status(state) = model_error;
        return;
    }
    if (model_received >= model_stall_after) {
        status(state) = 0;
        return;
    }
    if (model_sb_pending) {
        if ((control(state) & start) != 0) {
            ++model_duplicate_starts;
            ADD_FAILURE() << "START was requested again after hardware set SB";
            control(state) &= ~start;
        }
        status(state) = I2C_SR1_SB;
        return;
    }
    if ((control(state) & start) != 0 && model_remaining == 0) {
        if (model_starts != 0 && !model_internal_restart) {
            ++model_message;
        }
        model_internal_restart = false;
        ++model_starts;
        control(state) &= ~start;
        model_sb_pending = true;
        status(state) = I2C_SR1_SB;
        return;
    }
    if (model_address) {
        status(state) = I2C_SR1_ADDR;
        state->registers->SR2 = I2C_SR2_MSL | I2C_SR2_BUSY;
        return;
    }
    if (model_low_address) {
        status(state) = I2C_SR1_ADD10;
        return;
    }
    status(state) = rbne | btc;
    status(state) |= I2C_SR1_TXE;
}

/** \brief Hardware clears SB only after address DATA is actually written. */
extern "C" void nx_i2c_model_write(void* raw, uint32_t value) {
    auto* state = static_cast<State*>(raw);
    if (model_sb_pending) {
        ASSERT_LT(model_message, model_messages.size());
        const bool ten = model_address_bits == 10U;
        EXPECT_EQ(value,
                  ten ? (0xF0U | ((model_endpoint_address >> 7U) & 6U) |
                         (model_read_header ? 1U : 0U))
                      : (static_cast<uint32_t>(model_endpoint_address) << 1U) |
                            (model_messages[model_message].read ? 1U : 0U));
        EXPECT_EQ(control(state) & pos, 0U);
        EXPECT_NE(control(state) & ack, 0U);
        model_address_bytes.push_back(static_cast<uint8_t>(value));
        model_sb_pending = false;
        model_low_address = ten && !model_read_header;
        model_address = !model_low_address;
        model_read_header = false;
    } else if (model_low_address) {
        EXPECT_EQ(value, model_endpoint_address & 0xFFU);
        model_address_bytes.push_back(static_cast<uint8_t>(value));
        model_low_address = false;
        model_address = true;
        model_transmitter_address = model_messages[model_message].read;
    }
    state->registers->DR = value;
}

/** \brief Observe ACK/POS at the mandatory address release boundary. */
extern "C" void nx_i2c_model_address_cleared(void* raw) {
    auto* state = static_cast<State*>(raw);
    ASSERT_LT(model_message, model_messages.size());
    const auto& message = model_messages[model_message];
    model_address = false;
    if (model_transmitter_address) {
        EXPECT_EQ(control(state) & stop, 0U);
        model_transmitter_address = false;
        model_read_header = true;
        model_internal_restart = true;
        return;
    }
    model_remaining = message.read ? message.length : 0;
    if (message.read) {
        if (message.length == 1) {
            EXPECT_EQ(control(state) & (ack | pos), 0U);
            EXPECT_NE(model_mask, 0U);
        } else if (message.length == 2) {
            EXPECT_NE(control(state) & pos, 0U);
            EXPECT_NE(model_mask, 0U);
        } else {
            EXPECT_NE(control(state) & ack, 0U);
            EXPECT_EQ(control(state) & pos, 0U);
        }
    }
    model_boundary->Call(model_message, message.read);
}

TEST_F(I2CRegisters, TenBitReadAddressesTwiceBeforeExistingReceiveWindows) {
    endpoint_state.address = model_endpoint_address = 0x2ABU;
    endpoint_state.address_bits = model_address_bits = 10U;
    state.rate_hz = 80000U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_initialize(&state));
    for (size_t length : {1U, 2U, 4U}) {
        SCOPED_TRACE(length);
        model_starts = model_message = model_received = 0U;
        model_address_bytes.clear();
        EXPECT_CALL(boundary, Call(0U, true));
        EXPECT_EQ(NX_SUCCESS, transact({nx_i2c_message_read(data, length)}));
        EXPECT_EQ(length, transferred);
        EXPECT_EQ(model_address_bytes,
                  std::vector<uint8_t>({0xF4U, 0xABU, 0xF5U}));
        EXPECT_EQ(2U, model_starts);
        EXPECT_EQ(0U, model_duplicate_starts);
        EXPECT_FALSE(state.active);
    }
}

TEST_F(I2CRegisters, TenBitWriteReadPreservesMessageAndWireBoundaries) {
    endpoint_state.address = model_endpoint_address = 0x2ABU;
    endpoint_state.address_bits = model_address_bits = 10U;
    state.rate_hz = 400000U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_initialize(&state));
    EXPECT_CALL(boundary, Call(0U, false));
    EXPECT_CALL(boundary, Call(1U, true));
    EXPECT_EQ(NX_SUCCESS, transact({nx_i2c_message_write(data, 1U),
                                    nx_i2c_message_read(data + 1U, 2U)}));
    EXPECT_EQ(3U, transferred);
    EXPECT_EQ(model_address_bytes,
              std::vector<uint8_t>({0xF4U, 0xABU, 0xF4U, 0xABU, 0xF5U}));
    EXPECT_EQ(3U, model_starts);
}

TEST_F(I2CRegisters, TenBitStandardTimingErratumRejectsBeforeStart) {
    endpoint_state.address = 0x2ABU;
    endpoint_state.address_bits = 10U;
    registers.CCR = 210U;
    nx_i2c_message_t message = nx_i2c_message_read(data, 1U);
    const auto before = registers;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_i2c_endpoint_transaction(&endpoint, &message, 1U, 1000U,
                                          &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_EQ(0, std::memcmp(&before, &registers, sizeof(registers)));
    EXPECT_FALSE(state.active);
}

TEST_F(I2CRegisters, StandardSevenBitRestartUsesErrataSafeNominalTiming) {
    EXPECT_EQ(239U, registers.CCR);
    EXPECT_CALL(boundary, Call(0U, false));
    EXPECT_CALL(boundary, Call(1U, true));
    EXPECT_EQ(NX_SUCCESS, transact({nx_i2c_message_write(data, 1U),
                                    nx_i2c_message_read(data + 1U, 2U)}));
    EXPECT_EQ(3U, transferred);
    EXPECT_EQ(2U, model_starts);
}

TEST_F(I2CRegisters, StandardTenBitReadUsesTheSameErrataSafeTiming) {
    endpoint_state.address = 0x2ABU;
    endpoint_state.address_bits = 10U;
    model_endpoint_address = endpoint_state.address;
    model_address_bits = 10U;
    EXPECT_CALL(boundary, Call(0U, true));
    EXPECT_EQ(NX_SUCCESS, transact({nx_i2c_message_read(data, 2U)}));
    EXPECT_EQ(2U, transferred);
    EXPECT_EQ(2U, model_starts);
    EXPECT_EQ(model_address_bytes, std::vector<uint8_t>({0xF4U, 0xABU, 0xF5U}));
}

TEST_F(I2CRegisters, UnsafeSevenBitRestartRejectsBeforeAnyControllerEffect) {
    registers.CCR = 210U;
    const auto before = registers;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              transact({nx_i2c_message_write(data, 1U),
                        nx_i2c_message_read(data + 1U, 2U)}));
    EXPECT_EQ(0U, transferred);
    EXPECT_EQ(0, std::memcmp(&before, &registers, sizeof(registers)));
    EXPECT_FALSE(state.active);
}

/** \brief Consume one byte and assert the protected final-three-byte sequence.
 */
extern "C" uint8_t nx_i2c_model_read(void* raw) {
    auto* state = static_cast<State*>(raw);
    EXPECT_GT(model_remaining, 0U);
    if (model_remaining > 3) {
        EXPECT_NE(control(state) & ack, 0U);
    } else {
        EXPECT_EQ(control(state) & ack, 0U);
        if (model_remaining > 1) {
            EXPECT_NE(model_mask, 0U);
        }
    }
    if (model_remaining <= 2) {
        const bool last = model_message + 1 == model_messages.size();
        EXPECT_NE(control(state) & (last ? stop : start), 0U);
        EXPECT_EQ(control(state) & (last ? start : stop), 0U);
    }
    --model_remaining;
    if (model_remaining == 0 && model_complete_on_last_read &&
        model_message + 1 == model_messages.size()) {
        /* Hardware may complete STOP before task code runs again. */
        control(state) &= ~stop;
        state->registers->SR2 = 0;
        model_stop_completed = true;
    }
    if (model_remaining == 0 && model_restart_on_last_read &&
        model_message + 1 < model_messages.size()) {
        /* A receive-window restart can reach SB before the task resumes. */
        EXPECT_NE(control(state) & start, 0U);
        control(state) &= ~start;
        ++model_message;
        ++model_starts;
        model_sb_pending = true;
        status(state) = I2C_SR1_SB;
    }
    return static_cast<uint8_t>(++model_received);
}

TEST_F(I2CRegisters, FastModeUsesWholeClockCountsWithoutExceedingTheCeiling) {
    for (uint32_t rate : {400000U, 350000U, 100001U, 5129U}) {
        SCOPED_TRACE(rate);
        State cold{};
        cold.registers = &registers;
        cold.line_gpio = &lines;
        cold.line_mask = state.line_mask;
        cold.peripheral_mhz = 42U;
        cold.rate_hz = rate;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_initialize(&cold));
        const uint32_t divider = registers.CCR & 0xFFFU;
        const bool fast = (registers.CCR & I2C_CCR_FS) != 0U;
        ASSERT_NE(0U, divider);
        EXPECT_LE(42000000U, rate * (fast ? 3U : 2U) * divider);
        EXPECT_EQ(rate > 100000U, fast);
        EXPECT_EQ(fast ? 13U : 43U, registers.TRISE);
        if (rate == 400000U) {
            EXPECT_EQ(I2C_CCR_FS | 36U, registers.CCR);
            EXPECT_LT(42000000U, 400000U * 3U * divider);
        }
    }
}

TEST_F(I2CRegisters, UnrepresentableClockCeilingRejectsBeforeRegisterStores) {
    const I2C_TypeDef before = registers;
    for (uint32_t rate : {0U, 5128U, 400001U, UINT32_MAX}) {
        State cold{};
        cold.registers = &registers;
        cold.line_gpio = &lines;
        cold.line_mask = state.line_mask;
        cold.peripheral_mhz = 42U;
        cold.rate_hz = rate;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_i2c_initialize(&cold));
        EXPECT_EQ(0, std::memcmp(&before, &registers, sizeof(before)));
    }
}

TEST_F(I2CRegisters, ReceivesThreeFourAndMaximumLengthWithExactPrefix) {
    for (size_t length : {1U, 2U, 3U, 4U, 256U}) {
        SCOPED_TRACE(length);
        model_time = 0;
        model_message = 0;
        model_received = 0;
        model_starts = 0;
        EXPECT_CALL(boundary, Call(0, true));
        ASSERT_EQ(transact({nx_i2c_message_read(data, length)}), NX_SUCCESS);
        EXPECT_EQ(transferred, length);
        for (size_t i = 0; i < length; ++i) {
            EXPECT_EQ(data[i], static_cast<uint8_t>(i + 1));
        }
        EXPECT_EQ(control(&state) & (ack | pos | stop | start), 0U);
        EXPECT_EQ(model_mask, 0U);
    }
}

TEST_F(I2CRegisters, MixedReadsUseRepeatedStartWithoutIntermediateStop) {
    uint8_t command = 0x10;
    uint8_t second[2] = {};
    EXPECT_CALL(boundary, Call(0, false));
    EXPECT_CALL(boundary, Call(1, true));
    EXPECT_CALL(boundary, Call(2, true));
    EXPECT_CALL(boundary, Call(3, false));
    ASSERT_EQ(
        transact({nx_i2c_message_write(&command, 1),
                  nx_i2c_message_read(data, 4), nx_i2c_message_read(second, 2),
                  nx_i2c_message_write(&command, 1)}),
        NX_SUCCESS);
    EXPECT_EQ(model_starts, 4U);
    EXPECT_EQ(transferred, 8U);
    EXPECT_EQ(second[0], 5U);
    EXPECT_EQ(second[1], 6U);
}

TEST_F(I2CRegisters, NackArbitrationAndBusErrorPreserveCompletedReadPrefix) {
    for (uint32_t error : {nack, arbitration, bus_error}) {
        SCOPED_TRACE(error);
        model_time = 0;
        model_message = 0;
        model_received = 0;
        model_starts = 0;
        model_error = error;
        model_error_after = 2;
        EXPECT_CALL(boundary, Call(0, true));
        EXPECT_EQ(transact({nx_i2c_message_read(data, 8)}),
                  error == nack          ? NX_ERROR_NACK
                  : error == arbitration ? NX_ERROR_ARBITRATION
                                         : NX_ERROR_IO);
        EXPECT_EQ(transferred, 2U);
        EXPECT_EQ(data[0], 1U);
        EXPECT_EQ(data[1], 2U);
        EXPECT_EQ(data[2], 0xCCU);
        EXPECT_FALSE(state.active);
        model_error = 0;
        control(&state) &= ~(stop | start);
        state.registers->SR2 = 0;
        ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
        std::memset(data, 0xCC, sizeof(data));
    }
}

TEST_F(I2CRegisters, ExpiredDeadlineRejectsWithoutAddressOrRegisterChanges) {
    const uint32_t before = control(&state);
    EXPECT_EQ(transact({nx_i2c_message_read(data, 256)}, 1), NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(model_starts, 0U);
}

TEST_F(I2CRegisters, ContextAndOversizedMessagesRejectBeforeHardwareAccess) {
    const uint32_t before = control(&state);
    model_isr = true;
    EXPECT_EQ(transact({nx_i2c_message_read(data, 3)}), NX_ERROR_CONTEXT);
    model_isr = false;
    model_mask = 1;
    EXPECT_EQ(transact({nx_i2c_message_read(data, 3)}), NX_ERROR_CONTEXT);
    EXPECT_EQ(model_mask, 1U);
    model_mask = 0;
    EXPECT_NE(transact({nx_i2c_message_read(data, 257)}), NX_SUCCESS);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(model_starts, 0U);
}

TEST_F(I2CRegisters, NeverDeadlineRejectsBeforeHardwareAccess) {
    const uint32_t before = control(&state);
    EXPECT_EQ(transact({nx_i2c_message_read(data, 3)}, NX_DEADLINE_NEVER),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(model_starts, 0U);
}

TEST_F(I2CRegisters, DeadlineDuringReadReturnsOnlyTheCompletedPrefix) {
    for (size_t length : {3U, 8U}) {
        SCOPED_TRACE(length);
        model_time = 0;
        model_message = 0;
        model_received = 0;
        model_starts = 0;
        model_stall_after = length == 3 ? 1 : 2;
        EXPECT_CALL(boundary, Call(0, true));
        EXPECT_EQ(transact({nx_i2c_message_read(data, length)}, 40),
                  NX_ERROR_TIMEOUT);
        EXPECT_EQ(transferred, model_stall_after);
        for (size_t i = 0; i < model_stall_after; ++i) {
            EXPECT_EQ(data[i], static_cast<uint8_t>(i + 1));
        }
        EXPECT_EQ(data[model_stall_after], 0xCCU);
        EXPECT_FALSE(state.active);
        model_stall_after = SIZE_MAX;
        ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
        std::memset(data, 0xCC, sizeof(data));
    }
}

TEST_F(I2CRegisters, ExhaustedStopBudgetDoesNotWaitForLaterBusRelease) {
    model_time = UINT64_MAX - 100U;
    model_hold_stop = true;
    model_release_stop_after = 4U;
    EXPECT_CALL(boundary, Call(0U, true));
    EXPECT_EQ(transact({nx_i2c_message_read(data, 4U)}, UINT64_MAX - 1U),
              NX_ERROR_IO);
    EXPECT_EQ(transferred, 4U);
    EXPECT_TRUE(state.fault);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(model_stop_polls, 1U);
}

TEST_F(I2CRegisters, StuckStopCannotReportSuccessAndRequiresRecovery) {
    model_hold_stop = true;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_NE(transact({nx_i2c_message_read(data, 4)}, 40), NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.fault);
    model_hold_stop = false;
    /* Controller reset does not establish released external lines. */
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_IO);
    state.registers->SR2 = 0;
    ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
    model_time = 0;
    model_message = 0;
    model_received = 0;
    model_starts = 0;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_EQ(transact({nx_i2c_message_read(data, 4)}), NX_SUCCESS);
}

TEST_F(I2CRegisters, EightMessageBoundIsValidatedBeforeTheFirstStart) {
    std::vector<nx_i2c_message_t> messages(9, nx_i2c_message_read(data, 1));
    const uint32_t before = control(&state);
    EXPECT_EQ(transact(messages), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(transferred, 0U);
    messages.resize(8);
    for (size_t i = 0; i < messages.size(); ++i) {
        EXPECT_CALL(boundary, Call(i, true));
    }
    ASSERT_EQ(transact(messages), NX_SUCCESS);
    EXPECT_EQ(transferred, 8U);
    EXPECT_EQ(model_starts, 8U);
}

TEST_F(I2CRegisters, CompletedStopDoesNotIssueAnotherStopAfterMasterRelease) {
    model_complete_on_last_read = true;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_EQ(transact({nx_i2c_message_read(data, 4)}), NX_SUCCESS);
    EXPECT_TRUE(model_stop_completed);
    EXPECT_EQ(control(&state) & stop, 0U);
    EXPECT_EQ(transferred, 4U);
}

TEST_F(I2CRegisters, StopBitClearWithoutMasterReleaseCannotReportSuccess) {
    model_hold_master = true;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_NE(transact({nx_i2c_message_read(data, 4)}, 40), NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_FALSE(state.active);
}

TEST_F(I2CRegisters,
       ReceiveRestartIsRequestedOnlyOnceEvenWhenHardwareClearsIt) {
    for (bool immediate : {false, true}) {
        for (size_t length : {1U, 2U, 3U, 256U}) {
            for (bool next_read : {false, true}) {
                SCOPED_TRACE(immediate);
                SCOPED_TRACE(length);
                SCOPED_TRACE(next_read);
                model_time = 0;
                model_message = 0;
                model_received = 0;
                model_starts = 0;
                model_duplicate_starts = 0;
                model_restart_on_last_read = immediate;
                uint8_t next[1] = {0x10};
                EXPECT_CALL(boundary, Call(0, true));
                EXPECT_CALL(boundary, Call(1, next_read));
                ASSERT_EQ(transact({nx_i2c_message_read(data, length),
                                    {next, sizeof(next), next_read}}),
                          NX_SUCCESS);
                EXPECT_EQ(transferred, length + 1);
                EXPECT_EQ(model_starts, 2U);
                EXPECT_EQ(model_duplicate_starts, 0U);
                EXPECT_EQ(model_mask, 0U);
            }
        }
    }
}

TEST_F(I2CRegisters, RecoveryRequiresBothReleasedLinesWhenBusyFlagIsClear) {
    for (unsigned line = 0; line < 2; ++line) {
        state.registers->SR2 = 0;
        lines.IDR = state.line_mask & ~(1U << (6U + line));
        EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_IO);
        EXPECT_TRUE(state.fault);
        lines.IDR = state.line_mask;
        ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
    }
}

TEST_F(I2CRegisters, MissingLineFactsRejectInitializationAndRecovery) {
    state.line_gpio = nullptr;
    const uint32_t before = control(&state);
    EXPECT_EQ(nx_stm32_i2c_initialize(&state), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_INVALID);
    EXPECT_EQ(control(&state), before);
    state.line_gpio = &lines;
    state.line_mask = 0;
    EXPECT_EQ(nx_stm32_i2c_initialize(&state), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_INVALID);
    EXPECT_EQ(control(&state), before);
}

TEST(I2CPollingIdentity, ColdChildrenMatchExactControllerWithoutPortAccess) {
    State first = {};
    State second = {};
    EndpointState child = {&first, 0x50, 7U};
    EndpointState other_address = {&first, 0x51, 7U};
    const nx_i2c_ops_t* operations = &nx_stm32_i2c_ops;
    const nx_i2c_endpoint_ops_t* endpoint_operations =
        &nx_stm32_i2c_endpoint_ops;
    const nx_i2c_port_t first_face = {operations, &first};
    const nx_i2c_port_t same_context = {operations, &first};
    const nx_i2c_port_t second_face = {operations, &second};
    const nx_i2c_endpoint_t face = {endpoint_operations, &child};
    const nx_i2c_endpoint_t addressed = {endpoint_operations, &other_address};
    uint8_t first_before[sizeof(first)];
    uint8_t second_before[sizeof(second)];
    uint8_t child_before[sizeof(child)];
    std::memcpy(first_before, &first, sizeof(first));
    std::memcpy(second_before, &second, sizeof(second));
    std::memcpy(child_before, &child, sizeof(child));
    model_time = 123U;
    model_mask = 1U;
    model_isr = true;
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&face, &first_face));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&face, &same_context));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&addressed, &first_face));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &second_face));
    EXPECT_EQ(std::memcmp(&first, first_before, sizeof(first)), 0);
    EXPECT_EQ(std::memcmp(&second, second_before, sizeof(second)), 0);
    EXPECT_EQ(std::memcmp(&child, child_before, sizeof(child)), 0);
    EXPECT_EQ(model_time, 123U);
    EXPECT_EQ(model_mask, 1U);
    EXPECT_TRUE(model_isr);
}

TEST(I2CPollingIdentity, ForeignControllerMethodsAndMissingBindingsReject) {
    State state = {};
    EndpointState child = {&state, 0x50, 7U};
    const nx_i2c_ops_t* operations = &nx_stm32_i2c_ops;
    const nx_i2c_endpoint_ops_t* endpoint_operations =
        &nx_stm32_i2c_endpoint_ops;
    const nx_i2c_ops_t foreign_operations = *operations;
    const nx_i2c_port_t matching = {operations, &state};
    const nx_i2c_port_t foreign = {&foreign_operations, &state};
    const nx_i2c_port_t no_context = {operations, nullptr};
    const nx_i2c_port_t no_methods = {nullptr, &state};
    const nx_i2c_endpoint_t face = {endpoint_operations, &child};
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&face, &matching));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &foreign));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &no_context));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &no_methods));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, nullptr));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(nullptr, &matching));
    ASSERT_NE(endpoint_operations->on_port, nullptr);
    EXPECT_FALSE(endpoint_operations->on_port(nullptr, &matching));
    EXPECT_FALSE(endpoint_operations->on_port(&child, nullptr));
    child.port = nullptr;
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &matching));
}

TEST(I2CPollingIdentity, FixedNonreservedAddressIsValidatedWithoutStartup) {
    State state = {};
    EndpointState child = {&state, 8U, 7U};
    const nx_i2c_port_t port = {&nx_stm32_i2c_ops, &state};
    const nx_i2c_endpoint_t face = {&nx_stm32_i2c_endpoint_ops, &child};
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&face, &port));
    child.address = 119U;
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&face, &port));
    constexpr uint8_t invalid[] = {0U, 7U, 120U, 127U, 255U};
    for (uint8_t address : invalid) {
        child.address = address;
        EXPECT_FALSE(nx_i2c_endpoint_on_port(&face, &port));
        EXPECT_EQ(child.address, address);
        EXPECT_EQ(child.port, &state);
    }
}

TEST_F(I2CRegisters, PollingPayloadCannotBorrowMutableProviderControlBytes) {
    EXPECT_CALL(boundary, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_FALSE(state.active);
    auto* control_byte = reinterpret_cast<uint8_t*>(&state.active);
    EXPECT_EQ(NX_ERROR_INVALID,
              transact({nx_i2c_message_write(control_byte, 1U)}));
    EXPECT_EQ(0U, model_starts);
    EXPECT_EQ(0U, registers.DR);
}

TEST_F(I2CRegisters, FirstReceiveCannotRewriteALaterValidatedMessageLength) {
    EXPECT_CALL(boundary, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    uint8_t guarded[258] = {};
    guarded[256] = 0xA5U;
    model_messages.resize(2U);
    model_messages[0] = nx_i2c_message_read(
        reinterpret_cast<uint8_t*>(&model_messages[1].length), 1U);
    model_messages[1] = nx_i2c_message_read(guarded, 256U);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&endpoint, model_messages.data(),
                                          model_messages.size(), 10000U,
                                          &transferred));
    EXPECT_EQ(0U, model_starts);
    EXPECT_EQ(256U, model_messages[1].length);
    EXPECT_EQ(0xA5U, guarded[256]);
}

TEST_F(I2CRegisters, ProgressOutputCannotOverwriteDescriptorsOrImmutableTx) {
    model_messages = {nx_i2c_message_read(data, 1U)};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&endpoint, model_messages.data(),
                                          model_messages.size(), 1000U,
                                          &model_messages[0].length));
    EXPECT_EQ(1U, model_messages[0].length);
    EXPECT_EQ(0U, model_starts);
    EXPECT_CALL(boundary, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    transferred = 0x37U;
    EXPECT_EQ(NX_ERROR_INVALID,
              transact({nx_i2c_message_write(
                  reinterpret_cast<uint8_t*>(&transferred), 1U)}));
    EXPECT_EQ(0x37U, transferred);
    EXPECT_EQ(0U, model_starts);
}

TEST_F(I2CRegisters, UnprivilegedPollingCannotStartProtectedReceiveWindows) {
    EXPECT_CALL(boundary, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    const auto before = registers;
    model_privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, transact({nx_i2c_message_read(data, 2U)}));
    EXPECT_EQ(0U, model_starts);
    EXPECT_EQ(0, std::memcmp(&before, &registers, sizeof(registers)));
    EXPECT_EQ(0U, model_mask);
}

TEST_F(I2CRegisters, UnprivilegedRecoveryCannotResetOrClearTheRetainedFault) {
    state.fault = true;
    const auto before_registers = registers;
    const auto before_state = state;
    const nx_time_us_t before_time = model_time;
    model_privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_i2c_port_recover(&port));
    EXPECT_EQ(0, std::memcmp(&before_registers, &registers, sizeof(registers)));
    EXPECT_EQ(0, std::memcmp(&before_state, &state, sizeof(state)));
    EXPECT_EQ(before_time, model_time);
}
