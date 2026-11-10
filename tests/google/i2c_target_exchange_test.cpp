/**
 * \file            i2c_target_exchange_test.cpp
 * \brief           Finite target/SMBus exchanges at real external boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_target_exchange_model.h"
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_i2c_target.h"
}
#include <algorithm>
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" nx_stm32_system_t g_nx_stm32_system;

namespace {
using nx_i2c_target_model::hardware;

class I2cTargetExchange : public testing::Test {
  protected:
    nx_stm32_i2c_target_state_t state{};
    nx_stm32_i2c_target_cold_t cold{};
    nx_stm32_i2c_target_config_t config{};
    const nx_i2c_target_port_t port{&nx_stm32_i2c_target_ops, &state};
    nx_i2c_target_request_t request{};
    std::array<uint8_t, 8> rx{};
    const std::array<uint8_t, 3> tx{0x21U, 0x43U, 0x65U};
    uint16_t snapshots[3]{};
    nx_stm32_platform_pin_config_t pins[3]{};
    FLASH_TypeDef flash{};

    void SetUp() override {
        nx_i2c_target_model::reset();
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        flash.ACR = NEXUS_FLASH_WAIT_STATES;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.time_epoch = 1U;
        GPIOB->IDR =
            (1U << 5U) | (1U << 6U) | (1U << 7U) | (1U << 10U) | (1U << 11U);
        pins[0] = {GPIOB, &snapshots[0], 1U, 6U};
        pins[1] = {GPIOB, &snapshots[1], 1U, 7U};
        pins[2] = {GPIOB, &snapshots[2], 1U, 5U};
        config.registers = I2C1;
        config.rcc = RCC;
        config.pins = pins;
        config.pin_count = 2U;
        config.clock_hz = NEXUS_APB1_HZ;
        config.maximum_hz = 100000U;
        config.irq_latency_us = 4U;
        config.own_address = 0x52U;
        config.priority = 4U;
        rx.fill(0xCCU);
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        hardware.after_read = nullptr;
        hardware.before_write = nullptr;
        hardware.after_clock = nullptr;
        hardware.refused_write = nullptr;
        hardware.refuse_disable = false;
        hardware.refuse_clear = false;
        hardware.refuse_reset = false;
        hardware.masked = hardware.in_isr = false;
        hardware.privileged = true;
        hardware.active.fill(0U);
        if (state.active != nullptr && port.ops->cancel != nullptr) {
            (void)nx_i2c_target_cancel(&port, state.active);
            for (unsigned step = 0U; step < 8U && state.active; ++step)
                nx_i2c_target_service(&port);
        }
        if (cold.owned || state.config != nullptr) {
            EXPECT_EQ(NX_SUCCESS,
                      nx_stm32_i2c_target_close(&state, &config, &cold));
        }
    }

    void Open() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32_i2c_target_prepare(&state, &config, &cold));
        ASSERT_TRUE(cold.owned);
        ASSERT_EQ(0U, I2C1->CR1 & I2C_CR1_ACK);
    }

    void Prepare(nx_i2c_target_direction_t direction, size_t receive = 0U,
                 size_t transmit = 0U, uint64_t deadline = 1000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_i2c_target_prepare(&port, &request,
                                        receive ? rx.data() : nullptr, receive,
                                        transmit ? tx.data() : nullptr,
                                        transmit, direction, deadline));
        ASSERT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    }

    void Arm(nx_i2c_target_direction_t direction, size_t receive = 0U,
             size_t transmit = 0U, uint64_t deadline = 1000U) {
        ASSERT_NO_FATAL_FAILURE(
            Prepare(direction, receive, transmit, deadline));
        ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(&port, &request));
        ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
        ASSERT_EQ(&request, state.active);
    }

    void Irq() {
        nx_i2c_target_model::interrupt(0U, &state);
    }
    void Address(bool transmitter = false, bool second = false) {
        nx_i2c_target_model::address(0U, transmitter, second);
        Irq();
    }
    void Bytes(std::initializer_list<uint8_t> values, bool stop = false) {
        nx_i2c_target_model::bytes(0U, values, stop);
        Irq();
    }
    void Flag(uint32_t flags) {
        nx_i2c_target_model::flag(0U, flags);
        Irq();
    }
    void Service() {
        for (unsigned step = 0U;
             step < 8U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++step)
            nx_i2c_target_service(&port);
    }
    void Settled(nx_result_t result) {
        ASSERT_NO_FATAL_FAILURE(Service());
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(result, request.base.result);
        EXPECT_EQ(nullptr, state.active);
        EXPECT_EQ(0U, I2C1->CR1 & (I2C_CR1_PE | I2C_CR1_ACK));
        EXPECT_EQ(0U, hardware.enabled[I2C1_EV_IRQn]);
        EXPECT_EQ(0U, hardware.enabled[I2C1_ER_IRQn]);
        EXPECT_EQ(0U, hardware.pending[I2C1_EV_IRQn]);
        EXPECT_EQ(0U, hardware.pending[I2C1_ER_IRQn]);
    }
};
} /* namespace */

TEST_F(I2cTargetExchange, ForeignDeviceRegionsAreNeverCallerByteStorage) {
    ASSERT_NO_FATAL_FAILURE(Open());
    unsigned writes = hardware.writes;
    auto* device_rx = reinterpret_cast<uint8_t*>(UINT32_C(0x40003800));
    auto* device_tx = reinterpret_cast<const uint8_t*>(UINT32_C(0xe000e010));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_target_prepare(&port, &request, device_rx, 1U, nullptr, 0U,
                                    NX_I2C_TARGET_RECEIVE, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_target_prepare(&port, &request, nullptr, 0U, device_tx, 1U,
                                    NX_I2C_TARGET_TRANSMIT, 1000U));
    EXPECT_EQ(writes, hardware.writes);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(I2cTargetExchange, ReadyPayloadCannotBeChangedToOverwriteItsFace) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Prepare(NX_I2C_TARGET_RECEIVE, 2U));
    request.rx =
        reinterpret_cast<uint8_t*>(const_cast<nx_i2c_target_port_t*>(&port));
    unsigned writes = hardware.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_i2c_target_submit(&port, &request));
    EXPECT_EQ(writes, hardware.writes);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(I2cTargetExchange, AcceptancePrecedesAddressAckAndPreparationHasNoMmio) {
    ASSERT_NO_FATAL_FAILURE(Open());
    unsigned writes = hardware.writes;
    ASSERT_NO_FATAL_FAILURE(Prepare(NX_I2C_TARGET_RECEIVE, 4U));
    EXPECT_EQ(writes, hardware.writes);
    testing::StrictMock<testing::MockFunction<void()>> acceptance;
    EXPECT_CALL(acceptance, Call())
        .Times(testing::AtLeast(1))
        .WillRepeatedly([&] {
            EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
            EXPECT_EQ(&request, state.active);
        });
    hardware.before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (reg == &I2C1->CR1 && (value & I2C_CR1_ACK) != 0U)
            acceptance.Call();
    };
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(&port, &request));
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ACK);
    EXPECT_EQ(0U,
              I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP | I2C_CR1_NOSTRETCH));
    hardware.before_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange, StopDrainsBothPendingRxBytesBeforeClearingStopf) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 4U));
    Address();
    hardware.accesses.clear();
    Bytes({0x31U, 0x42U}, true);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    auto last_data = hardware.accesses.end();
    auto stop_read = hardware.accesses.end();
    auto stop_clear = hardware.accesses.end();
    for (auto it = hardware.accesses.begin(); it != hardware.accesses.end();
         ++it) {
        if (!it->write && it->address == &I2C1->DR)
            last_data = it;
        if (!it->write && it->address == &I2C1->SR1 &&
            (it->value & I2C_SR1_STOPF) != 0U)
            stop_read = it;
        if (it->write && it->address == &I2C1->CR1)
            stop_clear = it;
    }
    ASSERT_NE(hardware.accesses.end(), last_data);
    ASSERT_NE(hardware.accesses.end(), stop_read);
    ASSERT_NE(hardware.accesses.end(), stop_clear);
    EXPECT_LT(last_data, stop_read);
    EXPECT_LT(stop_read, stop_clear);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(2U, request.rx_received);
    EXPECT_EQ(0U, request.tx_completed);
    EXPECT_EQ(0x52U, request.matched_address);
    EXPECT_EQ(1U, request.address_phases);
    EXPECT_EQ(0x31U, rx[0]);
    EXPECT_EQ(0x42U, rx[1]);
    EXPECT_EQ(0xCCU, rx[2]);
    EXPECT_EQ(0U, hardware.unsafe_sr2_reads);
}

TEST_F(I2cTargetExchange,
       AddrArrivingAfterStatusSnapshotCannotBeClearedBlindly) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    bool injected = false;
    hardware.after_read = [&](volatile uint32_t* reg, uint32_t value) {
        if (!injected && reg == &I2C1->SR1 && (value & I2C_SR1_ADDR) == 0U) {
            injected = true;
            nx_i2c_target_model::address(0U, false);
        }
    };
    Irq();
    EXPECT_TRUE(injected);
    EXPECT_EQ(0U, hardware.accidental_addr_clears);
    EXPECT_EQ(0U, hardware.unsafe_sr2_reads);
    hardware.after_read = nullptr;
    Irq();
    Bytes({0x55U}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.rx_received);
}

TEST_F(I2cTargetExchange, ReceiveCapacityCannotOverwriteTheFollowingByte) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    Bytes({0x11U, 0x22U}, true);
    Service();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(NX_SUCCESS, request.base.result);
    EXPECT_EQ(0xCCU, rx[1]);
}

TEST_F(I2cTargetExchange, TransmitFeedsOnlyFirstAddrThenBtfAndCompletesAtAf) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_TRANSMIT, 0U, 2U));
    Address(true);
    ASSERT_EQ((std::vector<uint8_t>{0x21U}), hardware.loaded[0]);
    Flag(I2C_SR1_TXE);
    EXPECT_EQ(1U, hardware.loaded[0].size());
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    ASSERT_EQ((std::vector<uint8_t>{0x21U, 0x43U}), hardware.loaded[0]);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Flag(I2C_SR1_AF);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(2U, request.tx_completed);
    EXPECT_EQ(0U, request.rx_received);
}

TEST_F(I2cTargetExchange, EarlyMasterNackCannotInventTheRestOfAResponse) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_TRANSMIT, 0U, 3U));
    Address(true);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_LT(request.tx_completed, 3U);
    EXPECT_EQ(1U, hardware.loaded[0].size());
}

TEST_F(I2cTargetExchange, RepeatedStartUsesOnePrecomputedWriteReadExchange) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_WRITE_READ, 4U, 2U));
    Address();
    Bytes({0x07U});
    Address(true);
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.rx_received);
    EXPECT_EQ(2U, request.tx_completed);
    EXPECT_EQ(2U, request.address_phases);
    EXPECT_EQ(0x07U, rx[0]);
    EXPECT_EQ((std::vector<uint8_t>{0x21U, 0x43U}), hardware.loaded[0]);
}

TEST_F(I2cTargetExchange, RepeatedStartDrainsOldBtfBeforeClearingNewAddress) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_WRITE_READ, 4U, 2U));
    Address();
    nx_i2c_target_model::bytes(0U, {0x07U, 0x08U});
    nx_i2c_target_model::address(0U, true);
    hardware.accesses.clear();
    Irq();
    auto last_rx = hardware.accesses.end();
    auto fresh_sr1 = hardware.accesses.end();
    auto address_clear = hardware.accesses.end();
    auto first_tx = hardware.accesses.end();
    for (auto it = hardware.accesses.begin(); it != hardware.accesses.end();
         ++it) {
        if (!it->write && it->address == &I2C1->DR)
            last_rx = it;
        if (!it->write && it->address == &I2C1->SR1 &&
            (it->value & I2C_SR1_ADDR) != 0U)
            fresh_sr1 = it;
        if (!it->write && it->address == &I2C1->SR2 &&
            address_clear == hardware.accesses.end())
            address_clear = it;
        if (it->write && it->address == &I2C1->DR &&
            first_tx == hardware.accesses.end())
            first_tx = it;
    }
    EXPECT_EQ(0x07U, rx[0]);
    EXPECT_EQ(0x08U, rx[1]);
    ASSERT_NE(hardware.accesses.end(), last_rx);
    ASSERT_NE(hardware.accesses.end(), fresh_sr1);
    ASSERT_NE(hardware.accesses.end(), address_clear);
    ASSERT_NE(hardware.accesses.end(), first_tx);
    EXPECT_LT(last_rx, fresh_sr1);
    EXPECT_LT(fresh_sr1, address_clear);
    EXPECT_LT(address_clear, first_tx);
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(2U, request.rx_received);
    EXPECT_EQ(2U, request.tx_completed);
}

TEST_F(I2cTargetExchange, TenBitReadAcceptsTheRequiredEmptyReceiverPreamble) {
    config.ten_bit = true;
    config.own_address = 0x2ABU;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_TRANSMIT, 0U, 1U));
    Address();
    EXPECT_TRUE(hardware.loaded[0].empty());
    Address(true);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(0x2ABU, request.matched_address);
    EXPECT_EQ(2U, request.address_phases);
    EXPECT_EQ(0U, request.rx_received);
    EXPECT_EQ(1U, request.tx_completed);
}

TEST_F(I2cTargetExchange, TenBitZeroOwnAddressIsAValidFiniteReceiver) {
    config.ten_bit = true;
    config.own_address = 0U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    Bytes({0x3CU}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(0U, request.matched_address);
    EXPECT_EQ(1U, request.address_phases);
    EXPECT_EQ(1U, request.rx_received);
    EXPECT_EQ(0x3CU, rx[0]);
}

TEST_F(I2cTargetExchange, DualComparatorReportsTheActualSecondaryAddress) {
    config.second_address = 0x59U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address(false, true);
    Bytes({0x92U}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(0x59U, request.matched_address);
}

TEST_F(I2cTargetExchange, PreparedIdentityCannotCrossColdRestartOrTimeEpoch) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Prepare(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &cold));
    ASSERT_NO_FATAL_FAILURE(Open());
    unsigned writes = hardware.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_i2c_target_submit(&port, &request));
    EXPECT_EQ(writes, hardware.writes);
    ASSERT_NO_FATAL_FAILURE(Prepare(NX_I2C_TARGET_RECEIVE, 2U));
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(NX_ERROR_STATE, nx_i2c_target_submit(&port, &request));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(I2cTargetExchange,
       AliasedControlsAndInfiniteDeadlinesRejectBeforeEffects) {
    ASSERT_NO_FATAL_FAILURE(Open());
    unsigned writes = hardware.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_target_prepare(&port, &request,
                                    reinterpret_cast<uint8_t*>(&request), 1U,
                                    nullptr, 0U, NX_I2C_TARGET_RECEIVE, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_target_prepare(&port, &request,
                                    reinterpret_cast<uint8_t*>(&state), 1U,
                                    nullptr, 0U, NX_I2C_TARGET_RECEIVE, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_target_prepare(&port, &request, rx.data(), 1U, nullptr, 0U,
                                    NX_I2C_TARGET_RECEIVE, UINT64_MAX));
    EXPECT_EQ(writes, hardware.writes);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(I2cTargetExchange,
       DuplicateAdmissionNeverRearmsOrBorrowsAnotherRequest) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    unsigned writes = hardware.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_target_submit(&port, &request));
    EXPECT_EQ(writes, hardware.writes);
    EXPECT_EQ(&request, state.active);
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange, CancellationDetachesEverySourceOnlyInThreadService) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 4U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    Irq();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange,
       LateStopRemainsTimeoutWithoutATrustedCompletionStamp) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U, 0U, 20U));
    Address();
    Bytes({0x19U});
    hardware.now = 21U;
    Bytes({}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_TIMEOUT));
}

TEST_F(I2cTargetExchange, StopArrivingAfterTheEntryClockSampleRemainsTimeout) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U, 0U, 12U));
    Address();
    Bytes({0x19U});
    hardware.clock_step = 4U;
    bool injected = false;
    hardware.after_clock = [&] {
        if (!injected) {
            EXPECT_GT(hardware.now, 12U);
            nx_i2c_target_model::bytes(0U, {}, true);
            injected = true;
        }
    };
    Irq();
    hardware.after_clock = nullptr;
    hardware.clock_step = 0U;
    EXPECT_TRUE(injected);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_TIMEOUT));
}

TEST_F(I2cTargetExchange, DeadlineIncludesTheFinalThreadWithdrawalProof) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U, 0U, 20U));
    Address();
    Bytes({0x19U}, true);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    hardware.now = 21U;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_TIMEOUT));
}

TEST_F(I2cTargetExchange, CancelCannotReplaceTheFirstLatchedHardwareFailure) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    Flag(I2C_SR1_BERR);
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_BUS_ERROR);
}

TEST_F(I2cTargetExchange,
       EpochLossDuringWithdrawalInvalidatesUnpublishedSuccess) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    Bytes({0x3CU}, true);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    bool injected = false;
    unsigned clock_reads = hardware.clock_reads;
    hardware.before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (!injected && reg == &RCC->APB1RSTR &&
            (value & RCC_APB1RSTR_I2C1RST) != 0U) {
            ++g_nx_stm32_system.time_epoch;
            clock_reads = hardware.clock_reads;
            injected = true;
        }
    };
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    hardware.before_write = nullptr;
    EXPECT_TRUE(injected);
    EXPECT_EQ(clock_reads, hardware.clock_reads);
}

TEST_F(I2cTargetExchange, LateRxneAfterStopCannotBeResetIntoFalseSuccess) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    Bytes({0x3CU}, true);
    bool injected = false;
    hardware.before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (!injected && reg == &I2C1->CR2 &&
            (value & (I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN)) ==
                0U) {
            nx_i2c_target_model::bytes(0U, {0xEEU});
            injected = true;
        }
    };
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    hardware.before_write = nullptr;
    EXPECT_TRUE(injected);
    EXPECT_EQ(0xCCU, rx[1]);
}

TEST_F(I2cTargetExchange, ActiveIrqRetainsTheRequestUntilTheObserverExits) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    hardware.active[I2C1_ER_IRQn] = 1U;
    Service();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_TRUE(cold.owned);
    hardware.active[I2C1_ER_IRQn] = 0U;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange, RefusedNvicDisableRetainsTheSameColdAndBufferOwner) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    hardware.refuse_disable = true;
    Service();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_TRUE(cold.owned);
    EXPECT_NE(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &cold));
    hardware.refuse_disable = false;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange, RefusedPendingClearRetainsTheBorrowUntilRetry) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    hardware.pending[I2C1_ER_IRQn] = 1U;
    hardware.refuse_clear = true;
    Service();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    hardware.refuse_clear = false;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange, RefusedHardwareResetCannotPretendTheBusOwnerExited) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    hardware.refuse_reset = true;
    Service();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    hardware.refuse_reset = false;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
}

TEST_F(I2cTargetExchange,
       EpochLossAfterDrReadForbidsItsPayloadWriteAndOldClock) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    unsigned clock_reads = 0U;
    hardware.after_read = [&](volatile uint32_t* reg, uint32_t) {
        if (reg == &I2C1->DR) {
            ++g_nx_stm32_system.time_epoch;
            clock_reads = hardware.clock_reads;
        }
    };
    Bytes({0x88U});
    hardware.after_read = nullptr;
    EXPECT_EQ(0xCCU, rx[0]);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_EQ(clock_reads, hardware.clock_reads);
}

TEST_F(I2cTargetExchange,
       ActualRccLossAfterDrReadAlsoForbidsPayloadPublication) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    hardware.after_read = [&](volatile uint32_t* reg, uint32_t) {
        if (reg == &I2C1->DR)
            RCC->PLLCFGR ^= 1U;
    };
    Bytes({0x77U});
    hardware.after_read = nullptr;
    EXPECT_EQ(0xCCU, rx[0]);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
}

TEST_F(I2cTargetExchange,
       PecReceiveExcludesCrcAndRequiresTheActualPecTransfer) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ENPEC);
    Address();
    Bytes({0x22U});
    EXPECT_GT(hardware.pec_writes[0], 0U);
    Bytes({nx_i2c_target_model::crc8({0xA4U, 0x22U})}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.rx_received);
    EXPECT_EQ(0x22U, rx[0]);
    EXPECT_EQ(0xCCU, rx[1]);
}

TEST_F(I2cTargetExchange, BadPecIsDetectedEvenWhenHardwarePecerrIsLate) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    Bytes({0x22U});
    Bytes({(uint8_t)(nx_i2c_target_model::crc8({0xA4U, 0x22U}) ^ 1U)}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_PEC_ERROR);
}

TEST_F(I2cTargetExchange,
       RefusedReceivePecCannotBeReplacedBySoftwareCrcSuccess) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    hardware.refused_write = &I2C1->CR1;
    hardware.refused_bits = I2C_CR1_PEC;
    Bytes({0x22U});
    Bytes({nx_i2c_target_model::crc8({0xA4U, 0x22U})}, true);
    EXPECT_EQ(0U, hardware.pec_writes[0]);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
}

TEST_F(I2cTargetExchange, RefusedTransmitPecCannotPublishSuccessFromTheLastAf) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_TRANSMIT, 0U, 1U));
    Address(true);
    hardware.refused_write = &I2C1->CR1;
    hardware.refused_bits = I2C_CR1_PEC;
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    Flag(I2C_SR1_AF);
    EXPECT_EQ(0U, hardware.pec_writes[0]);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
}

TEST_F(I2cTargetExchange, PecTransmitArmsHardwareAndDoesNotCountTheCrcByte) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_TRANSMIT, 0U, 1U));
    Address(true);
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    EXPECT_GT(hardware.pec_writes[0], 0U);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ENPEC);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.tx_completed);
    EXPECT_EQ((std::vector<uint8_t>{0x21U}), hardware.loaded[0]);
}

TEST_F(I2cTargetExchange, CombinedPecIncludesBothAddressDirectionsAndPayloads) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_WRITE_READ, 1U, 1U));
    Address();
    Bytes({0x07U});
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ACK);
    Address(true);
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    ASSERT_FALSE(hardware.pec_values[0].empty());
    EXPECT_EQ(nx_i2c_target_model::crc8({0xA4U, 0x07U, 0xA5U, 0x21U}),
              hardware.pec_values[0].back());
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.rx_received);
    EXPECT_EQ(1U, request.tx_completed);
    EXPECT_EQ(2U, request.address_phases);
}

TEST_F(I2cTargetExchange, BufferedCombinedPecFollowsWireOrderBeforeCpuDrReads) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_WRITE_READ, 1U, 1U));
    Address();
    nx_i2c_target_model::bytes(0U, {0x07U});
    nx_i2c_target_model::address(0U, true);
    Irq();
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    ASSERT_FALSE(hardware.pec_values[0].empty());
    EXPECT_EQ(nx_i2c_target_model::crc8({0xA4U, 0x07U, 0xA5U, 0x21U}),
              hardware.pec_values[0].back());
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(0x07U, rx[0]);
    EXPECT_EQ(1U, request.rx_received);
    EXPECT_EQ(1U, request.tx_completed);
}

TEST_F(I2cTargetExchange, PosDriftCannotChangeTheTargetPecPositionSilently) {
    config.smbus = config.pec = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    I2C1->CR1 |= I2C_CR1_POS;
    Address();
    Bytes({0x22U});
    Bytes({nx_i2c_target_model::crc8({0xA4U, 0x22U})}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_EQ(0xCCU, rx[0]);
}

TEST_F(I2cTargetExchange, ClockStretchDisableCannotBeSilentlyAdopted) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    I2C1->CR1 |= I2C_CR1_NOSTRETCH;
    Address();
    Bytes({0x22U}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_EQ(0xCCU, rx[0]);
}

TEST_F(I2cTargetExchange, SmbusTimeoutAndOverrunRemainHardwareDiagnostics) {
    config.smbus = true;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    Address();
    Flag(I2C_SR1_TIMEOUT | I2C_SR1_OVR | I2C_SR1_PECERR);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_CLOCK_TIMEOUT);
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_OVERRUN);
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_PEC_ERROR);
}

TEST_F(I2cTargetExchange, AlertResponseUsesOnlyItsInternalAddressByte) {
    config.smbus = config.alert = true;
    config.pin_count = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_ALERT_RESPONSE));
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ALERT);
    Flag(I2C_SR1_SMBALERT);
    EXPECT_EQ((std::vector<uint8_t>{0xA4U}), hardware.loaded[0]);
    Flag(I2C_SR1_AF);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_ALERT);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.tx_completed);
    EXPECT_EQ(0U, request.rx_received);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_ALERT);
}

TEST_F(I2cTargetExchange,
       AlertArbitrationRetriesWithoutSettlingOrCallerPayload) {
    config.smbus = config.alert = true;
    config.pin_count = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_ALERT_RESPONSE));
    Flag(I2C_SR1_SMBALERT);
    Flag(I2C_SR1_ARLO);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    Flag(I2C_SR1_SMBALERT);
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_ARBITRATION);
    EXPECT_EQ(1U, request.tx_completed);
    EXPECT_EQ((std::vector<uint8_t>{0xA4U, 0xA4U}), hardware.loaded[0]);
}

TEST_F(I2cTargetExchange, AlertArbitrationNeverExtendsTheFiniteDeadline) {
    config.smbus = config.alert = true;
    config.pin_count = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_ALERT_RESPONSE, 0U, 0U, 20U));
    Flag(I2C_SR1_SMBALERT);
    Flag(I2C_SR1_ARLO);
    hardware.now = 21U;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_TIMEOUT));
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_ALERT);
}

TEST_F(I2cTargetExchange, AlertArbitrationRebuildsTheActualPecCalculator) {
    config.smbus = config.alert = config.pec = true;
    config.pin_count = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_ALERT_RESPONSE));
    Flag(I2C_SR1_SMBALERT);
    Flag(I2C_SR1_ARLO);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ALERT);
    Flag(I2C_SR1_SMBALERT);
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_ALERT);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_PEC);
    ASSERT_FALSE(hardware.pec_values[0].empty());
    EXPECT_EQ(nx_i2c_target_model::crc8({0x19U, 0xA4U}),
              hardware.pec_values[0].back());
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(1U, request.tx_completed);
    EXPECT_NE(0U, request.faults & NX_I2C_TARGET_ARBITRATION);
    EXPECT_EQ((std::vector<uint8_t>{0xA4U, 0xA4U}), hardware.loaded[0]);
}

TEST_F(I2cTargetExchange, RefusedAlertReleaseDuringPecCannotPublishSuccess) {
    config.smbus = config.alert = config.pec = true;
    config.pin_count = 3U;
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_ALERT_RESPONSE));
    Flag(I2C_SR1_SMBALERT);
    hardware.refused_write = &I2C1->CR1;
    hardware.refused_bits = I2C_CR1_ALERT;
    bool attempted_release = false;
    hardware.before_write = [&](volatile uint32_t* reg, uint32_t value) {
        if (reg == &I2C1->CR1 && (value & I2C_CR1_ALERT) == 0U)
            attempted_release = true;
    };
    Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    EXPECT_TRUE(attempted_release);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_ALERT);
    hardware.before_write = nullptr;
    Flag(I2C_SR1_AF);
    hardware.refused_write = nullptr;
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
}

TEST_F(I2cTargetExchange, TimingAndOwnAddressConfigurationRequirePeDisabled) {
    unsigned configuration_writes = 0U;
    hardware.before_write = [&](volatile uint32_t* reg, uint32_t) {
        if (reg == &I2C1->CCR || reg == &I2C1->TRISE || reg == &I2C1->OAR1 ||
            reg == &I2C1->OAR2) {
            ++configuration_writes;
            EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_PE);
        }
    };
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    Flag(I2C_SR1_BERR);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_IO));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_recover(&port));
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 1U));
    Address();
    Bytes({0x3CU}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_GT(configuration_writes, 0U);
    hardware.before_write = nullptr;
}

TEST_F(I2cTargetExchange, AllIncomingCpuContextsRejectBeforeAcknowledgement) {
    ASSERT_NO_FATAL_FAILURE(Open());
    ASSERT_NO_FATAL_FAILURE(Prepare(NX_I2C_TARGET_RECEIVE, 2U));
    unsigned writes = hardware.writes;
    hardware.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_i2c_target_submit(&port, &request));
    hardware.privileged = true;
    hardware.masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_i2c_target_submit(&port, &request));
    hardware.masked = false;
    hardware.in_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_i2c_target_submit(&port, &request));
    hardware.in_isr = false;
    EXPECT_EQ(writes, hardware.writes);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_ACK);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(I2cTargetExchange, TwoControllersKeepIndependentIrqsResetAndPayload) {
    ASSERT_NO_FATAL_FAILURE(Open());
    nx_stm32_i2c_target_state_t second{};
    nx_stm32_i2c_target_cold_t second_cold{};
    nx_stm32_i2c_target_config_t second_config = config;
    uint16_t second_snapshots[2]{};
    nx_stm32_platform_pin_config_t second_pins[] = {
        {GPIOB, &second_snapshots[0], 1U, 10U},
        {GPIOB, &second_snapshots[1], 1U, 11U}};
    second_config.registers = I2C2;
    second_config.pins = second_pins;
    second_config.own_address = 0x53U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_prepare(&second, &second_config,
                                                      &second_cold));
    const nx_i2c_target_port_t second_port{&nx_stm32_i2c_target_ops, &second};
    nx_i2c_target_request_t second_request{};
    nx_request_initialize(&second_request.base);
    uint8_t second_rx = 0xCCU;
    nx_result_t prepared =
        nx_i2c_target_prepare(&second_port, &second_request, &second_rx, 1U,
                              nullptr, 0U, NX_I2C_TARGET_RECEIVE, 1000U);
    if (prepared != NX_SUCCESS) {
        EXPECT_EQ(NX_SUCCESS, prepared);
        EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&second, &second_config,
                                                        &second_cold));
        return;
    }
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(&second_port, &second_request));
    ASSERT_NO_FATAL_FAILURE(Arm(NX_I2C_TARGET_RECEIVE, 2U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_cancel(&port, &request));
    ASSERT_NO_FATAL_FAILURE(Settled(NX_ERROR_CANCELLED));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&second_request.base));
    EXPECT_NE(0U, I2C2->CR1 & I2C_CR1_ACK);
    nx_i2c_target_model::address(1U, false);
    nx_i2c_target_model::interrupt(1U, &second);
    nx_i2c_target_model::bytes(1U, {0x73U}, true);
    nx_i2c_target_model::interrupt(1U, &second);
    for (unsigned step = 0U; step < 8U && second.active; ++step)
        nx_i2c_target_service(&second_port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&second_request.base));
    EXPECT_EQ(NX_SUCCESS, second_request.base.result);
    EXPECT_EQ(0x73U, second_rx);
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32_i2c_target_close(&second, &second_config, &second_cold));
}

TEST_F(I2cTargetExchange, FullReceiveCapacityKeepsItsSizeWithoutByteWrap) {
    ASSERT_NO_FATAL_FAILURE(Open());
    std::array<uint8_t, 257> storage{};
    storage.fill(0xCCU);
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_target_prepare(&port, &request, storage.data(), 256U,
                                    nullptr, 0U, NX_I2C_TARGET_RECEIVE, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(&port, &request));
    Address();
    for (unsigned byte = 0U; byte < 256U; ++byte)
        Bytes({(uint8_t)byte});
    Bytes({}, true);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(256U, request.rx_received);
    for (unsigned byte = 0U; byte < 256U; ++byte)
        EXPECT_EQ((uint8_t)byte, storage[byte]);
    EXPECT_EQ(0xCCU, storage.back());
}

TEST_F(I2cTargetExchange, FullTransmitCapacityWaitsForEveryBtfAndFinalNack) {
    ASSERT_NO_FATAL_FAILURE(Open());
    std::array<uint8_t, 256> payload{};
    for (unsigned byte = 0U; byte < payload.size(); ++byte)
        payload[byte] = (uint8_t)byte;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_prepare(&port, &request, nullptr, 0U,
                                                payload.data(), payload.size(),
                                                NX_I2C_TARGET_TRANSMIT, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(&port, &request));
    Address(true);
    for (unsigned byte = 1U; byte < payload.size(); ++byte)
        Flag(I2C_SR1_BTF | I2C_SR1_TXE);
    EXPECT_EQ(payload.size(), hardware.loaded[0].size());
    Flag(I2C_SR1_AF);
    ASSERT_NO_FATAL_FAILURE(Settled(NX_SUCCESS));
    EXPECT_EQ(256U, request.tx_completed);
    EXPECT_TRUE(
        std::equal(payload.begin(), payload.end(), hardware.loaded[0].begin()));
}
