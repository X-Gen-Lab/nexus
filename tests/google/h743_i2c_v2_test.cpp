/**
 * \file            h743_i2c_v2_test.cpp
 * \brief           Actual finite V2 byte protocol and abort settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_i2c_v2_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include "terminal_watchdog.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
namespace {
nx_i2c_v2_state_t* current;
bool stalled;
bool stall_after_write;
bool reset_refused;
uint32_t injected;
size_t inject_after;
size_t remaining;
std::vector<uint32_t> controls;
std::vector<uint8_t> bytes;
class FaultPort {
  public:
    MOCK_METHOD(void, Observe, ());
};
FaultPort* faults;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    faults->Observe();
}
extern "C" void nx_test_i2c_v2_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == current->reset) {
        if (reset_refused)
            return;
        *reg = value;
        if ((value & current->reset_mask) != 0U)
            *current->registers = {};
        return;
    }
    auto* r = current->registers;
    if (reg == &r->icr) {
        r->isr &= ~value;
        return;
    }
    *reg = value;
    if (reg == &r->cr2) {
        if (value == 0U) {
            r->isr = 0U;
            return;
        }
        controls.push_back(value);
        if ((value & (1U << 14)) != 0U) {
            if (!stalled) {
                r->isr = (1U << 5);
                r->cr2 = value & ~(1U << 14);
            }
            return;
        }
        remaining = (value >> 16) & 255U;
        r->isr = (1U << 15);
    }
    if (reg == &r->txdr) {
        bytes.push_back(static_cast<uint8_t>(value));
        --remaining;
        r->isr &= ~(1U << 1);
        if (stall_after_write) {
            stalled = true;
        }
    }
}
extern "C" void nx_test_i2c_v2_poll(nx_i2c_v2_state_t* p) {
    auto* r = p->registers;
    if (stalled)
        return;
    if (bytes.size() >= inject_after) {
        r->isr |= 1U << 4U;
        inject_after = SIZE_MAX;
        return;
    }
    if (injected != 0U) {
        r->isr |= injected;
        injected = 0;
        return;
    }
    if ((r->cr2 & (1U << 13)) != 0U)
        r->cr2 &= ~(1U << 13);
    if ((r->isr & (1U << 2)) != 0U) {
        /* External RXDR read side effect, not the production algorithm. */
        r->isr &= ~(1U << 2);
        --remaining;
    }
    if (remaining != 0U) {
        r->rxdr = 0x5aU;
        r->isr |= (r->cr2 & (1U << 10)) != 0U ? (1U << 2) : (1U << 1);
    } else if ((r->isr & (1U << 15)) != 0U) {
        if ((r->cr2 & (1U << 25)) != 0U) {
            r->isr = 1U << 5;
        } else {
            r->isr |= (r->cr2 & (1U << 24)) != 0U ? 1U << 7 : 1U << 6;
        }
    }
}
class H743I2cV2 : public testing::Test {
  protected:
    nx_i2c_v2_registers_t registers{};
    uint32_t reset{};
    nx_i2c_v2_state_t state{};
    nx_i2c_v2_endpoint_state_t endpoint{&state, 0x50};
    nx_i2c_port_t face{&nx_i2c_v2_ops, &state};
    nx_i2c_endpoint_t device{&nx_i2c_v2_endpoint_ops, &endpoint};
    testing::NiceMock<FaultPort> fault_mock;
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_I2C1EN;
        g_h743_model.rcc.AHB4ENR = RCC_AHB4ENR_GPIOBEN;
        current = &state;
        faults = &fault_mock;
        stalled = false;
        stall_after_write = false;
        reset_refused = false;
        injected = 0;
        inject_after = SIZE_MAX;
        remaining = 0;
        controls.clear();
        bytes.clear();
        state.registers = &registers;
        state.reset = &reset;
        state.reset_mask = 1;
        state.timing = 0x30420f13;
        state.poll_limit = 8;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_initialize(&state));
    }
};
TEST_F(H743I2cV2, ForeignLiveHardwareRejectsWithoutWrites) {
    registers.cr1 = 1;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_v2_initialize(&state));
    EXPECT_EQ(0U, registers.timing);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743I2cV2, ForeignTimeoutModeRejectsWithoutAdoptingTheController) {
    registers.timeout = 1U << 15U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_v2_initialize(&state));
    EXPECT_EQ(0U, registers.timing);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743I2cV2, ExactPadsAreReleasedOpenDrainWithoutInternalPullups) {
    g_h743_model.gpio_b.ODR = 1U;
    g_h743_model.gpio_b.MODER = 0xffffffffU;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_i2c_pins_prepare());
    EXPECT_EQ((1U << 8) | (1U << 9),
              g_h743_model.gpio_b.OTYPER & ((1U << 8) | (1U << 9)));
    EXPECT_EQ(0U, g_h743_model.gpio_b.PUPDR & (15U << 16));
    EXPECT_EQ(1U, g_h743_model.gpio_b.ODR & 1U);
    EXPECT_EQ(0x44U, g_h743_model.gpio_b.AFR[1] & 255U);
    EXPECT_EQ(10U << 16, g_h743_model.gpio_b.MODER & (15U << 16));
}
TEST_F(H743I2cV2, RefusedAlternatePinReadbackIsNotSuccess) {
    g_h743_model.ignore_write = &g_h743_model.gpio_b.AFR[1];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_i2c_pins_prepare());
}
TEST_F(H743I2cV2, WriteReadUsesRepeatedStartAndOneFinalStop) {
    Start();
    uint8_t tx[2] = {0x12, 0x34};
    uint8_t rx[2] = {};
    nx_i2c_message_t messages[] = {{tx, 2, false}, {rx, 2, true}};
    size_t done = 0;
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&device, messages, 2, 10000, &done));
    EXPECT_EQ(4U, done);
    EXPECT_EQ((std::vector<uint8_t>{0x12, 0x34}), bytes);
    EXPECT_EQ(0x5a, rx[0]);
    EXPECT_EQ(0x5a, rx[1]);
    ASSERT_EQ(2U, controls.size());
    EXPECT_EQ(0U, controls[0] & (1U << 25));
    EXPECT_NE(0U, controls[1] & (1U << 25));
    EXPECT_EQ(0U, registers.isr & (1U << 15));
    EXPECT_FALSE(state.active);
}
TEST_F(H743I2cV2, TxdrProgressDoesNotProveWireAckWhenTheBusStalls) {
    Start();
    stall_after_write = true;
    uint8_t value = 0xA5U;
    nx_i2c_message_t message{&value, 1U, false};
    nx_i2c_long_endpoint_t long_device{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    size_t progress = 0U;
    EXPECT_EQ(nx_i2c_long_endpoint_transaction(&long_device, &message, 1U,
                                               10000U, &progress),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(progress, 1U);
    EXPECT_EQ(bytes.size(), 1U);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(registers.cr1, 0U);
}
TEST_F(H743I2cV2, TwoHundredFiftySixBytesReloadWithoutRestart) {
    Start();
    uint8_t tx[256] = {};
    nx_i2c_message_t message{tx, sizeof(tx), false};
    size_t done = 0;
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&device, &message, 1, 10000, &done));
    EXPECT_EQ(256U, done);
    ASSERT_EQ(2U, controls.size());
    EXPECT_NE(0U, controls[0] & (1U << 24));
    EXPECT_EQ(0U, controls[1] & (1U << 13));
}
TEST_F(H743I2cV2, OptionalLongMessagesReloadWithoutAdditionalAddressPhases) {
    Start();
    nx_i2c_long_endpoint_t extended{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    for (size_t length : {257U, 510U, 511U, 65535U}) {
        controls.clear();
        bytes.clear();
        std::vector<uint8_t> payload(length, 0x39U);
        nx_i2c_message_t message{payload.data(), payload.size(), false};
        size_t done = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_i2c_long_endpoint_transaction(
                                  &extended, &message, 1U, 10000000U, &done));
        EXPECT_EQ(length, done);
        EXPECT_EQ(length, bytes.size());
        ASSERT_EQ((length + 254U) / 255U, controls.size());
        for (size_t i = 0U; i < controls.size(); ++i) {
            EXPECT_EQ(i == 0U, (controls[i] & (1U << 13U)) != 0U);
            EXPECT_EQ(i + 1U != controls.size(),
                      (controls[i] & (1U << 24U)) != 0U);
            EXPECT_EQ(i + 1U == controls.size(),
                      (controls[i] & (1U << 25U)) != 0U);
            EXPECT_EQ(0xa0U, controls[i] & 0x3ffU);
        }
    }
}
TEST_F(H743I2cV2, LongReadReloadPreservesErratumOneByteRuleAndRepeatedStart) {
    state.receive_reload_one = true;
    Start();
    nx_i2c_long_endpoint_t extended{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    uint8_t address = 0x10U;
    uint8_t rx[257] = {};
    nx_i2c_message_t messages[] = {{&address, 1U, false},
                                   {rx, sizeof(rx), true}};
    size_t done = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_long_endpoint_transaction(
                              &extended, messages, 2U, 1000000U, &done));
    EXPECT_EQ(258U, done);
    ASSERT_EQ(258U, controls.size());
    for (size_t i = 1U; i < controls.size(); ++i) {
        EXPECT_EQ(1U, (controls[i] >> 16U) & 255U);
        EXPECT_EQ(i == 1U, (controls[i] & (1U << 13U)) != 0U);
        EXPECT_NE(0U, controls[i] & (1U << 10U));
        EXPECT_EQ(0x5aU, rx[i - 1U]);
    }
}
TEST_F(H743I2cV2, ExtendedBoundsNeverRelaxTheOriginalSmallApi) {
    Start();
    uint8_t payload[257] = {};
    nx_i2c_message_t message{payload, sizeof(payload), false};
    size_t done = 0U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_i2c_endpoint_transaction(
                                        &device, &message, 1U, 10000U, &done));
    EXPECT_TRUE(controls.empty());
    EXPECT_FALSE(state.active);
}
TEST_F(H743I2cV2, ExtendedAliasingAndOversizedMessagesRejectWithoutHardware) {
    Start();
    nx_i2c_long_endpoint_t extended{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    std::vector<uint8_t> payload(65536U);
    nx_i2c_message_t message{payload.data(), payload.size(), false};
    size_t done = 0U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_i2c_long_endpoint_transaction(&extended, &message, 1U, 10000U,
                                               &done));
    message = {reinterpret_cast<uint8_t*>(&message), sizeof(message), true};
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_long_endpoint_transaction(
                                    &extended, &message, 1U, 10000U, &done));
    EXPECT_TRUE(controls.empty());
}
TEST_F(H743I2cV2, NackAtReloadReturnsExactPrefixWithoutRestartOrReplay) {
    Start();
    nx_i2c_long_endpoint_t extended{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    uint8_t payload[511] = {};
    nx_i2c_message_t message{payload, sizeof(payload), false};
    inject_after = 255U;
    size_t done = 0U;
    EXPECT_EQ(NX_ERROR_NACK, nx_i2c_long_endpoint_transaction(
                                 &extended, &message, 1U, 1000000U, &done));
    EXPECT_EQ(255U, done);
    EXPECT_EQ(255U, bytes.size());
    ASSERT_EQ(1U, controls.size());
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_ERROR_STATE, nx_i2c_long_endpoint_transaction(
                                  &extended, &message, 1U, 1000000U, &done));
    EXPECT_EQ(255U, bytes.size());
}
TEST_F(H743I2cV2, ExtendedDescriptorAdmissionChecksOverflowAndMutability) {
    uint8_t payload[300] = {};
    nx_i2c_message_t messages[] = {{payload, sizeof(payload), false},
                                   {payload + 1U, 257U, true}};
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_long_messages_validate(messages, 2U));
    messages[1].read = false;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_long_messages_validate(messages, 2U));
    messages[0].data = reinterpret_cast<uint8_t*>(UINTPTR_MAX - 100U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_long_messages_validate(messages, 1U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_i2c_long_messages_validate(messages, 9U));
}
TEST_F(H743I2cV2, TwoHundredFiftySixByteReceptionKeepsOneReadDirection) {
    Start();
    uint8_t rx[256] = {};
    nx_i2c_message_t message{rx, sizeof(rx), true};
    size_t done = 0;
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&device, &message, 1, 10000, &done));
    EXPECT_EQ(256U, done);
    ASSERT_EQ(2U, controls.size());
    EXPECT_NE(0U, controls[0] & (1U << 24));
    EXPECT_EQ(0U, controls[1] & (1U << 13));
    EXPECT_NE(0U, controls[1] & (1U << 10));
    EXPECT_EQ(0x5aU, rx[0]);
    EXPECT_EQ(0x5aU, rx[255]);
}
TEST_F(H743I2cV2, AuditedH7ReceptionReloadsOneByteWithoutExtraStart) {
    state.receive_reload_one = true;
    Start();
    uint8_t rx[256] = {};
    nx_i2c_message_t message{rx, sizeof(rx), true};
    size_t done = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(&device, &message, 1U,
                                                      10000U, &done));
    ASSERT_EQ(256U, controls.size());
    EXPECT_EQ(256U, done);
    for (size_t i = 0U; i < controls.size(); ++i) {
        EXPECT_EQ(1U, (controls[i] >> 16U) & 255U);
        EXPECT_EQ(i == 0U, (controls[i] & (1U << 13U)) != 0U);
        EXPECT_EQ(i + 1U != controls.size(), (controls[i] & (1U << 24U)) != 0U);
        EXPECT_EQ(i + 1U == controls.size(), (controls[i] & (1U << 25U)) != 0U);
        EXPECT_EQ(0x5aU, rx[i]);
    }
}
TEST_F(H743I2cV2, TimeoutResetsAndRequiresExplicitRecovery) {
    Start();
    stalled = true;
    uint8_t tx = 1;
    nx_i2c_message_t message{&tx, 1, false};
    size_t done = 7;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_i2c_endpoint_transaction(&device, &message, 1, 100, &done));
    EXPECT_EQ(0U, done);
    EXPECT_TRUE(state.fault);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0U, registers.cr1);
    stalled = false;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_recover(&face));
    EXPECT_FALSE(state.fault);
}
TEST_F(H743I2cV2, NackAndArbitrationDoNotBecomeSuccess) {
    Start();
    uint8_t tx = 1;
    nx_i2c_message_t message{&tx, 1, false};
    size_t done = 0;
    injected = 1U << 4;
    EXPECT_EQ(NX_ERROR_NACK,
              nx_i2c_endpoint_transaction(&device, &message, 1, 100, &done));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_recover(&face));
    injected = 1U << 9;
    EXPECT_EQ(NX_ERROR_ARBITRATION,
              nx_i2c_endpoint_transaction(&device, &message, 1, 100, &done));
}
TEST_F(H743I2cV2, AuditedMasterBerrQuirkClearsOnlyThatFlagAndContinues) {
    Start();
    state.ignore_master_berr = true;
    uint8_t tx = 1;
    nx_i2c_message_t message{&tx, 1, false};
    size_t done = 0;
    injected = 1U << 8;
    EXPECT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&device, &message, 1, 1000, &done));
    EXPECT_EQ(1U, done);
    EXPECT_FALSE(state.fault);
}
TEST_F(H743I2cV2, ExpiredOrAliasedAdmissionRetainsNothing) {
    Start();
    uint8_t tx = 1;
    nx_i2c_message_t message{&tx, 1, false};
    size_t done = 1;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_i2c_endpoint_transaction(&device, &message, 1, 0, &done));
    EXPECT_TRUE(controls.empty());
    message.data = reinterpret_cast<uint8_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&device, &message, 1, 100, &done));
    EXPECT_TRUE(controls.empty());
}
TEST_F(H743I2cV2, FailedResetCannotReturnAndReleaseTheCaller) {
    Start();
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        stalled = true;
        reset_refused = true;
        EXPECT_CALL(fault_mock, Observe()).WillOnce([&] {
            nexus_test::RecordTerminalObservation(state.active && state.fault);
        });
        uint8_t tx = 1;
        nx_i2c_message_t message{&tx, 1, false};
        size_t done = 0;
        (void)nx_i2c_endpoint_transaction(&device, &message, 1, 100, &done);
    }));
}
