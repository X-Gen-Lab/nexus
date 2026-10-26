/**
 * \file            g474_i2c_v2_test.cpp
 * \brief           Actual finite V2 byte protocol and abort settlement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_i2c_v2_model.h"
#include "nexus/core/fault.h"
#include "stm32g474_provider.h"
#include "terminal_watchdog.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
namespace {
nx_i2c_v2_state_t* current;
bool stalled;
bool reset_refused;
uint32_t injected;
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
    if (reg == current->config->reset) {
        if (reset_refused)
            return;
        *reg = value;
        if ((value & current->config->reset_mask) != 0U)
            *current->config->registers = {};
        return;
    }
    auto* r = current->config->registers;
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
    }
}
extern "C" void nx_test_i2c_v2_poll(nx_i2c_v2_state_t* p) {
    auto* r = p->config->registers;
    if (stalled)
        return;
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
class G474I2cV2 : public testing::Test {
  protected:
    nx_i2c_v2_registers_t registers{};
    uint32_t reset{};
    nx_i2c_v2_config_t config{};
    nx_i2c_v2_config_t alternate{};
    nx_i2c_v2_state_t state{};
    nx_i2c_v2_endpoint_state_t endpoint{&state, 0x50};
    nx_i2c_port_t face{&nx_i2c_v2_ops, &state};
    nx_i2c_endpoint_t device{&nx_i2c_v2_endpoint_ops, &endpoint};
    testing::NiceMock<FaultPort> fault_mock;
    void SetUp() override {
        nx_g474_model_reset();
        current = &state;
        state.config = &config;
        faults = &fault_mock;
        stalled = false;
        reset_refused = false;
        injected = 0;
        remaining = 0;
        controls.clear();
        bytes.clear();
        config.registers = &registers;
        config.reset = &reset;
        config.reset_mask = 1;
        config.timing = 0x30420f13;
        config.poll_limit = 8;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_initialize(&state));
    }
};
TEST_F(G474I2cV2, ForeignLiveHardwareRejectsWithoutWrites) {
    registers.cr1 = 1;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_v2_initialize(&state));
    EXPECT_EQ(0U, registers.timing);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474I2cV2, ColdBindingCannotReplaceRetainedCleanupResources) {
    const auto& first = config;
    alternate = config;
    alternate.timing = 0x20420f13U;
    alternate.poll_limit = 4U;
    const auto& second = alternate;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_bind(&state, &first));
    EXPECT_EQ(&first, state.config);
    EXPECT_TRUE(controls.empty());
    Start();
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_v2_bind(&state, &second));
    EXPECT_EQ(&first, state.config);
    reset_refused = true;
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_v2_bind(&state, &second));
    EXPECT_EQ(&first, state.config);
    reset_refused = false;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_stop(&face));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_bind(&state, &second));
    EXPECT_EQ(&second, state.config);
    EXPECT_EQ(0U, registers.cr1);
    EXPECT_EQ(0U, registers.timing);
}
TEST_F(G474I2cV2, UnboundFacesRejectWithoutDereferencingConfiguration) {
    state.config = nullptr;
    uint8_t byte = 1U;
    nx_i2c_message_t message{&byte, 1U, false};
    size_t done = 99U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_v2_initialize(&state));
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_port_stop(&face));
    EXPECT_EQ(NX_ERROR_STATE, nx_i2c_port_recover(&face));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&device, &message, 1U, 100U, &done));
    EXPECT_EQ(99U, done);
    EXPECT_TRUE(controls.empty());
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_v2_bind(&state, nullptr));
    EXPECT_EQ(nullptr, state.config);
}
TEST_F(G474I2cV2, ColdBindingRejectsConfigurationInsideHardwareStorage) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_v2_bind(
                  &state, reinterpret_cast<const nx_i2c_v2_config_t*>(&state)));
    EXPECT_EQ(&config, state.config);
    auto replacement = config;
    replacement.reset = &replacement.timing;
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_v2_bind(&state, &replacement));
    EXPECT_EQ(&config, state.config);
    replacement = config;
    replacement.registers =
        reinterpret_cast<nx_i2c_v2_registers_t*>(&replacement);
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_v2_bind(&state, &replacement));
    EXPECT_EQ(&config, state.config);
    EXPECT_TRUE(controls.empty());
    EXPECT_FALSE(state.retained);
}
TEST_F(G474I2cV2, ConfigurationCannotBeBorrowedAsDataOrProgressOutput) {
    Start();
    size_t done = 99U;
    nx_i2c_message_t message{reinterpret_cast<uint8_t*>(&config), 1U, true};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&device, &message, 1U, 100U, &done));
    EXPECT_EQ(99U, done);
    message.read = false;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&device, &message, 1U, 100U, &done));
    uint8_t byte = 1U;
    message.data = &byte;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&device, &message, 1U, 100U,
                                          reinterpret_cast<size_t*>(&config)));
    EXPECT_TRUE(controls.empty());
    EXPECT_TRUE(bytes.empty());
    EXPECT_EQ(&registers, config.registers);
    EXPECT_EQ(8U, config.poll_limit);
    EXPECT_FALSE(state.active);
}
TEST_F(G474I2cV2, ExactPadsAreReleasedOpenDrainWithoutInternalPullups) {
    g_g474_model.gpio_b.ODR = 1U;
    g_g474_model.gpio_b.MODER = 0xffffffffU;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_i2c_pins_prepare());
    EXPECT_EQ((1U << 8) | (1U << 9),
              g_g474_model.gpio_b.OTYPER & ((1U << 8) | (1U << 9)));
    EXPECT_EQ(0U, g_g474_model.gpio_b.PUPDR & (15U << 16));
    EXPECT_EQ(1U, g_g474_model.gpio_b.ODR & 1U);
    EXPECT_EQ(0x44U, g_g474_model.gpio_b.AFR[1] & 255U);
    EXPECT_EQ(10U << 16, g_g474_model.gpio_b.MODER & (15U << 16));
}
TEST_F(G474I2cV2, RefusedAlternatePinReadbackIsNotSuccess) {
    g_g474_model.ignore_write = &g_g474_model.gpio_b.AFR[1];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_i2c_pins_prepare());
}
TEST_F(G474I2cV2, WriteReadUsesRepeatedStartAndOneFinalStop) {
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
TEST_F(G474I2cV2, TwoHundredFiftySixBytesReloadWithoutRestart) {
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
TEST_F(G474I2cV2, LongWriteReadReloadsOnlyInsideItsOwnMessage) {
    Start();
    nx_i2c_long_endpoint_t extended{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    uint8_t tx[511] = {};
    uint8_t rx[257] = {};
    nx_i2c_message_t messages[] = {{tx, sizeof(tx), false},
                                   {rx, sizeof(rx), true}};
    size_t done = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_long_endpoint_transaction(
                              &extended, messages, 2U, 1000000U, &done));
    EXPECT_EQ(768U, done);
    ASSERT_EQ(5U, controls.size());
    for (size_t i = 0U; i < controls.size(); ++i) {
        EXPECT_EQ(i == 0U || i == 3U, (controls[i] & (1U << 13U)) != 0U);
        EXPECT_EQ(i < 3U ? 0U : 1U, (controls[i] >> 10U) & 1U);
        EXPECT_EQ(i == 4U, (controls[i] & (1U << 25U)) != 0U);
    }
    EXPECT_EQ(0x5aU, rx[256]);
}
TEST_F(G474I2cV2, TwoHundredFiftySixByteReceptionKeepsOneReadDirection) {
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
TEST_F(G474I2cV2, TimeoutResetsAndRequiresExplicitRecovery) {
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
TEST_F(G474I2cV2, NackAndArbitrationDoNotBecomeSuccess) {
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
TEST_F(G474I2cV2, AuditedMasterBerrQuirkClearsOnlyThatFlagAndContinues) {
    config.ignore_master_berr = true;
    Start();
    uint8_t tx = 1;
    nx_i2c_message_t message{&tx, 1, false};
    size_t done = 0;
    injected = 1U << 8;
    EXPECT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&device, &message, 1, 1000, &done));
    EXPECT_EQ(1U, done);
    EXPECT_FALSE(state.fault);
}
TEST_F(G474I2cV2, ExpiredOrAliasedAdmissionRetainsNothing) {
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
TEST_F(G474I2cV2, FailedResetCannotReturnAndReleaseTheCaller) {
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
