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
#ifdef NEXUS_I2C_STM32_TEST
#include "stm32f407_provider.h"
#else
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"
#endif
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
uint64_t model_time;
uint32_t model_mask;
bool model_isr;
size_t model_message;
size_t model_remaining;
size_t model_received;
size_t model_starts;
bool model_address;
bool model_sb_pending;
bool model_restart_on_last_read;
size_t model_duplicate_starts;
bool model_hold_stop;
bool model_hold_master;
bool model_complete_on_last_read;
bool model_stop_completed;
uint32_t model_error;
size_t model_error_after;
size_t model_stall_after;
std::vector<nx_i2c_message_t> model_messages;
testing::MockFunction<void(size_t, bool)>* model_boundary;

#ifdef NEXUS_I2C_STM32_TEST
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
#else
using State = nx_gd32_i2c_state_t;
using EndpointState = nx_gd32_i2c_endpoint_state_t;
constexpr uint32_t ack = I2C_CTL0_ACKEN;
constexpr uint32_t pos = I2C_CTL0_POAP;
constexpr uint32_t start = I2C_CTL0_START;
constexpr uint32_t stop = I2C_CTL0_STOP;
constexpr uint32_t rbne = I2C_STAT0_RBNE;
constexpr uint32_t btc = I2C_STAT0_BTC;
constexpr uint32_t nack = I2C_STAT0_AERR;
constexpr uint32_t arbitration = I2C_STAT0_LOSTARB;
constexpr uint32_t bus_error = I2C_STAT0_BERR;
volatile uint32_t& control(State* port) {
    return I2C_CTL0(port->controller->registers);
}
volatile uint32_t& status(State* port) {
    return I2C_STAT0(port->controller->registers);
}
#endif

class I2CRegisters : public testing::Test {
  protected:
    State state = {};
    EndpointState endpoint_state = {};
    nx_i2c_endpoint_t endpoint = {};
    nx_i2c_port_t port = {};
    testing::StrictMock<testing::MockFunction<void(size_t, bool)>> boundary;
    uint8_t data[256] = {};
    size_t transferred = 0;
#ifdef NEXUS_I2C_STM32_TEST
    I2C_TypeDef registers = {};
    GPIO_TypeDef lines = {};
#else
    void* mapping = MAP_FAILED;
#endif

    void SetUp() override {
        model_time = 0;
        model_mask = 0;
        model_isr = false;
        model_message = 0;
        model_remaining = 0;
        model_received = 0;
        model_starts = 0;
        model_address = false;
        model_sb_pending = false;
        model_restart_on_last_read = false;
        model_duplicate_starts = 0;
        model_hold_stop = false;
        model_hold_master = false;
        model_complete_on_last_read = false;
        model_stop_completed = false;
        model_error = 0;
        model_error_after = 0;
        model_stall_after = SIZE_MAX;
        model_messages.clear();
        model_boundary = &boundary;
        std::memset(data, 0xCC, sizeof(data));
#ifdef NEXUS_I2C_STM32_TEST
        state.registers = &registers;
        state.line_gpio = &lines;
        state.line_mask = (1U << 6U) | (1U << 7U);
        lines.IDR = state.line_mask;
        state.peripheral_mhz = 42;
        state.rate_hz = 100000;
        ASSERT_EQ(nx_stm32_i2c_initialize(&state), NX_SUCCESS);
        endpoint_state = {&state, 0x50};
        endpoint = {&nx_stm32_i2c_endpoint_ops, &endpoint_state};
        port = {&nx_stm32_i2c_ops, &state};
#else
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        ASSERT_EQ(nx_gd32_i2c_initialize_at(
                      &state, &endpoint_state, &nx_gd32_i2c1_controller, 0x50,
                      100000, GPIOB, GPIO_PIN_10 | GPIO_PIN_11),
                  NX_SUCCESS);
        GPIO_ISTAT(GPIOB) = GPIO_PIN_10 | GPIO_PIN_11;
        endpoint = {&nx_gd32_i2c_endpoint_ops, &endpoint_state};
        port = {&nx_gd32_i2c_ops, &state};
#endif
    }

    void TearDown() override {
        model_boundary = nullptr;
#ifndef NEXUS_I2C_STM32_TEST
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
#endif
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
    return true;
}
/** \brief Preserve host ordering without claiming hardware timing. */
extern "C" void nx_arch_dmb(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
/** \brief The host register fixture has no physical instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}
/** \brief No physical DWT counter exists in this transaction fixture. */
extern "C" bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}
#ifndef NEXUS_I2C_STM32_TEST
/** \brief Model selected clock acquisition with the official register layout.
 */
extern "C" void rcu_periph_clock_enable(rcu_periph_enum clock) {
    RCU_REG_VAL(clock) |= BIT(RCU_BIT_POS(clock));
}
/** \brief Model release of only the selected controller clock. */
extern "C" void rcu_periph_clock_disable(rcu_periph_enum clock) {
    RCU_REG_VAL(clock) &= ~BIT(RCU_BIT_POS(clock));
}
/** \brief Model local reset without pretending external wires are released. */
extern "C" void i2c_deinit(uint32_t registers) {
    std::memset(reinterpret_cast<void*>(static_cast<uintptr_t>(registers)), 0,
                0x28);
}
/** \brief Unselected fixture pin setup is outside this transaction model. */
extern "C" void gpio_af_set(uint32_t, uint32_t, uint32_t) {
}
/** \brief Unselected fixture output setup is outside this transaction model. */
extern "C" void gpio_output_options_set(uint32_t, uint8_t, uint32_t, uint32_t) {
}
/** \brief Unselected fixture mode setup is outside this transaction model. */
extern "C" void gpio_mode_set(uint32_t, uint32_t, uint32_t, uint32_t) {
}
#endif

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
        if (!model_hold_stop) {
            control(state) &= ~stop;
#ifdef NEXUS_I2C_STM32_TEST
            if (!model_hold_master) {
                state->registers->SR2 = 0;
            }
#else
            if (!model_hold_master) {
                I2C_STAT1(state->controller->registers) = 0;
            }
#endif
        }
        return;
    }
    if (model_error != 0 && model_received >= model_error_after) {
        if (model_error == arbitration) {
#ifdef NEXUS_I2C_STM32_TEST
            state->registers->SR2 &= ~static_cast<uint32_t>(I2C_SR2_MSL);
#else
            I2C_STAT1(state->controller->registers) &= ~I2C_STAT1_MASTER;
#endif
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
#ifdef NEXUS_I2C_STM32_TEST
        status(state) = I2C_SR1_SB;
#else
        status(state) = I2C_STAT0_SBSEND;
#endif
        return;
    }
    if ((control(state) & start) != 0 && model_remaining == 0) {
        if (model_starts != 0) {
            ++model_message;
        }
        ++model_starts;
        control(state) &= ~start;
        model_sb_pending = true;
#ifdef NEXUS_I2C_STM32_TEST
        status(state) = I2C_SR1_SB;
#else
        status(state) = I2C_STAT0_SBSEND;
#endif
        return;
    }
    if (model_address) {
#ifdef NEXUS_I2C_STM32_TEST
        status(state) = I2C_SR1_ADDR;
        state->registers->SR2 = I2C_SR2_MSL | I2C_SR2_BUSY;
#else
        status(state) = I2C_STAT0_ADDSEND;
        I2C_STAT1(state->controller->registers) =
            I2C_STAT1_MASTER | I2C_STAT1_I2CBSY;
#endif
        return;
    }
    status(state) = rbne | btc;
#ifdef NEXUS_I2C_STM32_TEST
    status(state) |= I2C_SR1_TXE;
#else
    status(state) |= I2C_STAT0_TBE;
#endif
}

/** \brief Hardware clears SB only after address DATA is actually written. */
extern "C" void nx_i2c_model_write(void* raw, uint32_t value) {
    auto* state = static_cast<State*>(raw);
    if (model_sb_pending) {
        ASSERT_LT(model_message, model_messages.size());
        EXPECT_EQ(value, (UINT32_C(0x50) << 1U) |
                             (model_messages[model_message].read ? 1U : 0U));
        EXPECT_EQ(control(state) & pos, 0U);
        EXPECT_NE(control(state) & ack, 0U);
        model_sb_pending = false;
        model_address = true;
    }
#ifdef NEXUS_I2C_STM32_TEST
    state->registers->DR = value;
#else
    I2C_DATA(state->controller->registers) = value;
#endif
}

/** \brief Observe ACK/POS at the mandatory address release boundary. */
extern "C" void nx_i2c_model_address_cleared(void* raw) {
    auto* state = static_cast<State*>(raw);
    ASSERT_LT(model_message, model_messages.size());
    const auto& message = model_messages[model_message];
    model_address = false;
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
#ifdef NEXUS_I2C_STM32_TEST
        state->registers->SR2 = 0;
#else
        I2C_STAT1(state->controller->registers) = 0;
#endif
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
#ifdef NEXUS_I2C_STM32_TEST
        status(state) = I2C_SR1_SB;
#else
        status(state) = I2C_STAT0_SBSEND;
#endif
    }
    return static_cast<uint8_t>(++model_received);
}

TEST_F(I2CRegisters, ReceivesThreeFourAndMaximumLengthWithExactPrefix) {
    for (size_t length : {1U, 2U, 3U, 4U, 256U}) {
        SCOPED_TRACE(length);
        model_time = 0;
        model_message = 0;
        model_received = 0;
        model_starts = 0;
        EXPECT_CALL(boundary, Call(0, true));
        ASSERT_EQ(transact({{data, length, true}}), NX_SUCCESS);
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
    ASSERT_EQ(transact({{&command, 1, false},
                        {data, 4, true},
                        {second, 2, true},
                        {&command, 1, false}}),
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
        EXPECT_EQ(transact({{data, 8, true}}), error == nack ? NX_ERROR_NACK
                                               : error == arbitration
                                                   ? NX_ERROR_ARBITRATION
                                                   : NX_ERROR_IO);
        EXPECT_EQ(transferred, 2U);
        EXPECT_EQ(data[0], 1U);
        EXPECT_EQ(data[1], 2U);
        EXPECT_EQ(data[2], 0xCCU);
        EXPECT_FALSE(state.active);
        model_error = 0;
        control(&state) &= ~(stop | start);
#ifdef NEXUS_I2C_STM32_TEST
        state.registers->SR2 = 0;
#else
        I2C_STAT1(state.controller->registers) = 0;
#endif
        ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
        std::memset(data, 0xCC, sizeof(data));
    }
}

TEST_F(I2CRegisters, ExpiredDeadlineRejectsWithoutAddressOrRegisterChanges) {
    const uint32_t before = control(&state);
    EXPECT_EQ(transact({{data, 256, true}}, 1), NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(model_starts, 0U);
}

TEST_F(I2CRegisters, ContextAndOversizedMessagesRejectBeforeHardwareAccess) {
    const uint32_t before = control(&state);
    model_isr = true;
    EXPECT_EQ(transact({{data, 3, true}}), NX_ERROR_CONTEXT);
    model_isr = false;
    model_mask = 1;
    EXPECT_EQ(transact({{data, 3, true}}), NX_ERROR_CONTEXT);
    EXPECT_EQ(model_mask, 1U);
    model_mask = 0;
    EXPECT_NE(transact({{data, 257, true}}), NX_SUCCESS);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(control(&state), before);
    EXPECT_EQ(model_starts, 0U);
}

TEST_F(I2CRegisters, NeverDeadlineRejectsBeforeHardwareAccess) {
    const uint32_t before = control(&state);
    EXPECT_EQ(transact({{data, 3, true}}, NX_DEADLINE_NEVER),
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
        EXPECT_EQ(transact({{data, length, true}}, 40), NX_ERROR_TIMEOUT);
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

TEST_F(I2CRegisters, StuckStopCannotReportSuccessAndRequiresRecovery) {
    model_hold_stop = true;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_NE(transact({{data, 4, true}}, 40), NX_SUCCESS);
    EXPECT_EQ(transferred, 4U);
    EXPECT_FALSE(state.active);
#ifdef NEXUS_I2C_STM32_TEST
    EXPECT_TRUE(state.fault);
#else
    EXPECT_TRUE(state.faulted);
#endif
    model_hold_stop = false;
#ifdef NEXUS_I2C_STM32_TEST
    /* Controller reset does not establish released external lines. */
    EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_IO);
    state.registers->SR2 = 0;
#endif
    ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
    model_time = 0;
    model_message = 0;
    model_received = 0;
    model_starts = 0;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_EQ(transact({{data, 4, true}}), NX_SUCCESS);
}

TEST_F(I2CRegisters, EightMessageBoundIsValidatedBeforeTheFirstStart) {
    std::vector<nx_i2c_message_t> messages(9, {data, 1, true});
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
    EXPECT_EQ(transact({{data, 4, true}}), NX_SUCCESS);
    EXPECT_TRUE(model_stop_completed);
    EXPECT_EQ(control(&state) & stop, 0U);
    EXPECT_EQ(transferred, 4U);
}

TEST_F(I2CRegisters, StopBitClearWithoutMasterReleaseCannotReportSuccess) {
    model_hold_master = true;
    EXPECT_CALL(boundary, Call(0, true));
    EXPECT_NE(transact({{data, 4, true}}, 40), NX_SUCCESS);
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
                ASSERT_EQ(transact({{data, length, true},
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
#ifdef NEXUS_I2C_STM32_TEST
        state.registers->SR2 = 0;
        lines.IDR = state.line_mask & ~(1U << (6U + line));
#else
        I2C_STAT1(state.controller->registers) = 0;
        GPIO_ISTAT(GPIOB) = state.line_mask & ~(1U << (10U + line));
#endif
        EXPECT_EQ(nx_i2c_port_recover(&port), NX_ERROR_IO);
#ifdef NEXUS_I2C_STM32_TEST
        EXPECT_TRUE(state.fault);
        lines.IDR = state.line_mask;
#else
        EXPECT_TRUE(state.faulted);
        GPIO_ISTAT(GPIOB) = state.line_mask;
#endif
        ASSERT_EQ(nx_i2c_port_recover(&port), NX_SUCCESS);
    }
}

#ifdef NEXUS_I2C_STM32_TEST
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
#endif
