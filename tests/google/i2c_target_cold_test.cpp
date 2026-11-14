/**
 * \file            i2c_target_cold_test.cpp
 * \brief           Real target admission, address programming and cold drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_i2c_target.h"
RCC_TypeDef nx_target_rcc;
I2C_TypeDef nx_target_i2c[3];
GPIO_TypeDef nx_target_gpio[9];
nx_stm32_system_t g_nx_stm32_system;
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
unsigned writes;
bool privileged;
bool masked;
bool in_isr;
bool hold_disable;
volatile uint32_t* refused;
uint32_t dropped;
std::array<uint32_t, 82> enabled;
std::array<uint32_t, 82> pending;
std::array<uint32_t, 82> active;
std::array<uint32_t, 82> priorities;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* write_observer;

class I2cTargetCold : public testing::Test {
  protected:
    nx_stm32_i2c_target_state_t state{};
    nx_stm32_i2c_target_cold_t receipt{};
    nx_stm32_i2c_target_config_t config{};
    uint16_t snapshots[3]{};
    nx_stm32_platform_pin_config_t pins[3]{};
    FLASH_TypeDef flash{};
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        observer;

    void SetUp() override {
        nx_target_rcc = {};
        std::memset(nx_target_i2c, 0, sizeof(nx_target_i2c));
        std::memset(nx_target_gpio, 0, sizeof(nx_target_gpio));
        enabled.fill(0U);
        pending.fill(0U);
        active.fill(0U);
        priorities.fill(3U);
        writes = 0U;
        privileged = true;
        masked = in_isr = hold_disable = false;
        refused = nullptr;
        dropped = 0U;
        write_observer = &observer;
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
        GPIOB->IDR = (1U << 6U) | (1U << 7U);
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
    }
    void TearDown() override {
        refused = nullptr;
        hold_disable = masked = in_isr = false;
        privileged = true;
        if (receipt.owned || state.config != nullptr) {
            EXPECT_EQ(NX_SUCCESS,
                      nx_stm32_i2c_target_close(&state, &config, &receipt));
        }
        write_observer = nullptr;
    }
    void Prepare() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    }
};
} /* namespace */

TEST_F(I2cTargetCold, SelectedAlternatePadsRestoreWithoutOriginalBankLease) {
    pins[0] = {GPIOF, &snapshots[0], 5U, 1U};
    pins[1] = {GPIOF, &snapshots[1], 5U, 0U};
    config.registers = I2C2;
    GPIOF->IDR = 3U;
    GPIOF->MODER = 0xFFFF0000U;
    const uint32_t mode = GPIOF->MODER;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_admit(&state, &config, &receipt));
    ASSERT_EQ(0U, writes);
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_EQ(RCC_AHB1ENR_GPIOFEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, I2C1->CR1);
    EXPECT_NE(0U, I2C2->CR1 & I2C_CR1_PE);
    EXPECT_EQ(4U, GPIOF->AFR[0] & 15U);
    EXPECT_EQ(4U, (GPIOF->AFR[0] >> 4U) & 15U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_EQ(mode, GPIOF->MODER);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2cTargetCold, ReadonlyAdmissionPrecedesActualAddressAndStretchSetup) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_admit(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(nullptr, state.config);
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ((1U << 14U) | (0x52U << 1U), I2C1->OAR1);
    EXPECT_EQ(0U, I2C1->OAR2);
    EXPECT_EQ(I2C_CR1_PE,
              I2C1->CR1 & (I2C_CR1_PE | I2C_CR1_ACK | I2C_CR1_NOSTRETCH));
    EXPECT_EQ(NEXUS_APB1_HZ / 1000000U, I2C1->CR2 & I2C_CR2_FREQ);
    EXPECT_EQ(0U, I2C1->CR2 & (I2C_CR2_DMAEN | I2C_CR2_ITEVTEN |
                               I2C_CR2_ITERREN | I2C_CR2_ITBUFEN));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(1U, state.time_epoch);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(1U, enabled[I2C1_EV_IRQn]);
    EXPECT_EQ(1U, enabled[I2C1_ER_IRQn]);
}

TEST_F(I2cTargetCold, TenBitOwnAddressUsesTheWholeOar1Encoding) {
    config.ten_bit = true;
    config.own_address = 0x2ABU;
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(I2C_OAR1_ADDMODE | (1U << 14U) | 0x2ABU, I2C1->OAR1);
    EXPECT_EQ(0U, I2C1->OAR2);
}

TEST_F(I2cTargetCold, DualSevenBitAddressUsesItsOwnHardwareComparator) {
    config.second_address = 0x59U;
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ((0x59U << 1U) | I2C_OAR2_ENDUAL, I2C1->OAR2);
}

TEST_F(I2cTargetCold, DualTenBitAndTenBitSmbusRejectWithoutEffects) {
    config.ten_bit = true;
    config.own_address = 0x2ABU;
    config.second_address = 0x59U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    config.second_address = 0U;
    config.smbus = true;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
}

TEST_F(I2cTargetCold, ForeignEnabledControllerAndPendingVectorRemainUntouched) {
    RCC->APB1ENR = RCC_APB1ENR_I2C1EN;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_EQ(I2C_CR1_PE | I2C_CR1_ACK, I2C1->CR1);
    I2C1->CR1 = 0U;
    pending[I2C1_ER_IRQn] = 1U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2cTargetCold, ColdContextAndActualSelectedClockAreRequired) {
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    privileged = true;
    masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    masked = false;
    in_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    in_isr = false;
    RCC->PLLCFGR ^= 1U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32_i2c_target_admit(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
}

TEST_F(I2cTargetCold, FailedOarReadbackRetainsTheExactColdCleanupReceipt) {
    refused = &I2C1->OAR1;
    dropped = 1U << 14U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(&config, state.config);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
}

TEST_F(I2cTargetCold, RefusedVectorWithdrawalKeepsPadsAndGateUntilRetry) {
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    hold_disable = true;
    EXPECT_NE(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    hold_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(3U, priorities[I2C1_EV_IRQn]);
    EXPECT_EQ(3U, priorities[I2C1_ER_IRQn]);
}

TEST_F(I2cTargetCold, ExhaustedGenerationCannotRebindAnOwner) {
    state.generation = UINT64_MAX;
    EXPECT_EQ(NX_ERROR_EXHAUSTED,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2cTargetCold, ReadonlyPreSystemAdmissionCannotRequireRunClockEffects) {
    g_nx_stm32_system.started = false;
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CFGR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_target_admit(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_EQ(nullptr, state.config);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2cTargetCold, NeighborPadLockDoesNotClaimThisTargetRoute) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
    GPIOB->LCKR = (1U << 16U) | 1U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_admit(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
}

TEST_F(I2cTargetCold, SnapshotStorageCannotAliasAnOwnedRegisterBank) {
    pins[0].snapshot =
        reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(&I2C1->CR1));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_i2c_target_prepare(&state, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2cTargetCold, CopiesOfTheExactReceiptCannotReleaseTheOwner) {
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    auto copied = receipt;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_i2c_target_close(&state, &config, &copied));
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
}

TEST_F(I2cTargetCold, I2c3KeepsItsSeparateAAndCPadsAndVectorPair) {
    config.registers = I2C3;
    pins[0] = {GPIOA, &snapshots[0], 0U, 8U};
    pins[1] = {GPIOC, &snapshots[1], 2U, 9U};
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN,
              RCC->AHB1ENR);
    EXPECT_EQ(4U, (GPIOA->AFR[1] >> 0U) & 15U);
    EXPECT_EQ(4U, (GPIOC->AFR[1] >> 4U) & 15U);
    EXPECT_EQ(1U, enabled[I2C3_EV_IRQn]);
    EXPECT_EQ(1U, enabled[I2C3_ER_IRQn]);
    EXPECT_EQ(0U, enabled[I2C1_EV_IRQn]);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_EQ(RCC_AHB1ENR_GPIOBEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(I2cTargetCold, QuietPoweredTupleAndGateReturnExactlyAfterClose) {
    RCC->APB1ENR = RCC_APB1ENR_I2C1EN;
    I2C1->CR2 = 16U;
    I2C1->OAR1 = (1U << 14U) | (0x58U << 1U);
    I2C1->OAR2 = (0x59U << 1U) | I2C_OAR2_ENDUAL;
    I2C1->CCR = 0x123U;
    I2C1->TRISE = 0x20U;
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_EQ(RCC_APB1ENR_I2C1EN, RCC->APB1ENR);
    EXPECT_EQ(0U, I2C1->CR1);
    EXPECT_EQ(16U, I2C1->CR2);
    EXPECT_EQ((1U << 14U) | (0x58U << 1U), I2C1->OAR1);
    EXPECT_EQ((0x59U << 1U) | I2C_OAR2_ENDUAL, I2C1->OAR2);
    EXPECT_EQ(0x123U, I2C1->CCR);
    EXPECT_EQ(0x20U, I2C1->TRISE);
}

TEST_F(I2cTargetCold, ExternalSlowSclCeilingIsNotAMasterDividerOverflow) {
    config.maximum_hz = 1U;
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(0U,
              I2C1->CCR & ~(uint32_t)(I2C_CCR_CCR | I2C_CCR_FS | I2C_CCR_DUTY));
    EXPECT_GE(I2C1->CCR & I2C_CCR_CCR, 4U);
}

TEST_F(I2cTargetCold, RecoveryCannotRearmAfterPartialColdRelease) {
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    refused = &RCC->APB1ENR;
    dropped = RCC_APB1ENR_I2C1EN;
    ASSERT_EQ(NX_ERROR_IO,
              nx_stm32_i2c_target_close(&state, &config, &receipt));
    EXPECT_EQ(0U, receipt.captured);
    EXPECT_TRUE(receipt.owned);
    unsigned before = writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_i2c_target_ops.recover(&state));
    EXPECT_EQ(before, writes);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_PE);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_target_close(&state, &config, &receipt));
}

/** \brief Actual reads remain observable at the external register boundary. */
extern "C" uint32_t nx_target_read(volatile uint32_t* address) {
    return *address;
}
/** \brief Refused writes and reset side effects remain real register facts. */
extern "C" void nx_target_write(volatile uint32_t* address, uint32_t value) {
    ++writes;
    if (write_observer != nullptr)
        write_observer->Call(address, value);
    *address =
        address == refused ? (value & ~dropped) | (*address & dropped) : value;
    for (unsigned index = 0U; index < 3U; ++index) {
        if (address == &nx_target_i2c[index].CCR)
            *address &= I2C_CCR_CCR | I2C_CCR_FS | I2C_CCR_DUTY;
    }
    if (address == &RCC->APB1RSTR) {
        for (unsigned index = 0U; index < 3U; ++index) {
            if ((value & (RCC_APB1RSTR_I2C1RST << index)) != 0U) {
                std::memset(&nx_target_i2c[index], 0, sizeof(I2C_TypeDef));
                nx_target_i2c[index].TRISE = 2U;
            }
        }
    }
}
/** \brief Model NVIC command acceptance without replacing provider policy. */
extern "C" uint32_t nx_target_irq(unsigned command, IRQn_Type irq,
                                  uint32_t value) {
    size_t index = static_cast<size_t>(irq);
    if (index >= enabled.size())
        return 0U;
    switch (command) {
        case 0U:
            enabled[index] = 1U;
            break;
        case 1U:
            if (!hold_disable)
                enabled[index] = 0U;
            break;
        case 2U:
            pending[index] = 0U;
            break;
        case 3U:
            priorities[index] = value;
            break;
        case 4U:
            return enabled[index];
        case 5U:
            return pending[index];
        case 6U:
            return active[index];
        case 7U:
            return priorities[index];
        default:
            break;
    }
    return 0U;
}
/** \brief Expose one modeled privileged Thread CPU boundary. */
extern "C" bool nx_target_privileged(void) {
    return privileged;
}
/** \brief Expose modeled exception context without changing hardware. */
extern "C" bool nx_target_in_isr(void) {
    return in_isr;
}
/** \brief A masked owner cannot perform cold controls. */
extern "C" bool nx_target_masked(void) {
    return masked;
}
/** \brief Synchronous host MMIO has no physical bus pipeline. */
extern "C" void nx_target_dsb(void) {
}
/** \brief Save the exact incoming modeled interrupt mask. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved{masked ? 1U : 0U};
    masked = true;
    return saved;
}
/** \brief Restore that same mask without a hidden worker. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    masked = saved.value != 0U;
}

/** \brief The cold program has no active transfer clock progression. */
extern "C" uint64_t nx_stm32_system_now(nx_stm32_system_t*) {
    return 10U;
}
