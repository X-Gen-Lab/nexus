/**
 * \file            stm32_i2c_cold_test.cpp
 * \brief           Production I2C cold ownership and restoration contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_i2c_cold.h"
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
NVIC_Type g_nx_i2c_cold_nvic;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool in_isr;
static bool masked;
static bool privileged;
static volatile uint32_t* refused;
static int refused_irq;
static int refused_priority;
class I2CCold : public testing::Test {
  protected:
    void* peripherals = MAP_FAILED;
    uint16_t snapshots[2]{};
    nx_stm32_i2c_pin_config_t pins[2]{{&snapshots[0], 6U}, {&snapshots[1], 7U}};
    nx_stm32_i2c_config_t config{I2C1, pins, nullptr, 42000000U, 5U, false};
    nx_stm32_i2c_cold_receipt_t receipt{};
    nx_stm32_i2c_state_t state{};

    void SetUp() override {
        peripherals =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(peripherals, reinterpret_cast<void*>(0x40000000U));
        std::memset(&g_nx_i2c_cold_nvic, 0, sizeof(g_nx_i2c_cold_nvic));
        in_isr = false;
        masked = false;
        privileged = true;
        refused = nullptr;
        refused_irq = -1;
        refused_priority = -1;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        GPIOB->MODER = 0x3CF00000U;
        GPIOB->PUPDR = 0x00005000U;
        GPIOB->ODR = 0xC0U;
        GPIOB->IDR = 0xC0U;
    }
    void TearDown() override {
        if (peripherals != MAP_FAILED) {
            EXPECT_EQ(0, munmap(peripherals, 0x80000U));
        }
    }
    nx_result_t Prepare() {
        return nx_stm32_i2c_prepare(&state, &config, &receipt);
    }
    nx_result_t Close() {
        return nx_stm32_i2c_close(&state, &config, &receipt);
    }
};

TEST_F(I2CCold, StaticFactsRejectBeforeAnyHardwareEffect) {
    config.clock_hz = 41000000U;
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(nullptr, state.registers);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, ContextRejectsBeforeClockOrPinChanges) {
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, Prepare());
    privileged = true;
    in_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, Prepare());
    in_isr = false;
    masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, Prepare());
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->AHB1ENR);
}

TEST_F(I2CCold, PrepareAndCloseRestoreExactPadsAndPreexistingGates) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB1ENR = RCC_APB1ENR_I2C1EN | RCC_APB1ENR_TIM3EN;
    const auto mode = GPIOB->MODER;
    const auto pull = GPIOB->PUPDR;
    ASSERT_EQ(NX_SUCCESS, Prepare());
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(I2C1, state.registers);
    EXPECT_EQ(0xC0U, GPIOB->OTYPER & 0xC0U);
    EXPECT_EQ(0xA000U, GPIOB->MODER & 0xF000U);
    EXPECT_EQ(0x44000000U, GPIOB->AFR[0]);
    EXPECT_EQ(NX_SUCCESS, Close());
    EXPECT_EQ(mode, GPIOB->MODER);
    EXPECT_EQ(pull, GPIOB->PUPDR);
    EXPECT_EQ(RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN, RCC->AHB1ENR);
    EXPECT_EQ(RCC_APB1ENR_I2C1EN | RCC_APB1ENR_TIM3EN, RCC->APB1ENR);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(nullptr, state.registers);
}

TEST_F(I2CCold, ForeignEnabledControllerRemainsUntouched) {
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
    EXPECT_EQ(NX_ERROR_BUSY, Prepare());
    EXPECT_EQ(I2C_CR1_PE | I2C_CR1_ACK, I2C1->CR1);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, DuplicatePrepareDoesNotReplaceLiveOwner) {
    ASSERT_EQ(NX_SUCCESS, Prepare());
    EXPECT_EQ(NX_ERROR_BUSY, Prepare());
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(NX_SUCCESS, Close());
    EXPECT_EQ(NX_SUCCESS, Prepare());
    EXPECT_EQ(NX_SUCCESS, Close());
}

TEST_F(I2CCold, ActiveOperationRetainsClockAndPadsUntilJoined) {
    ASSERT_EQ(NX_SUCCESS, Prepare());
    state.active = true;
    EXPECT_EQ(NX_ERROR_BUSY, Close());
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_PE);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    state.active = false;
    EXPECT_EQ(NX_SUCCESS, Close());
}

TEST_F(I2CCold, FailedWithdrawalRetainsReceiptAndAllowsCloseRetry) {
    ASSERT_EQ(NX_SUCCESS, Prepare());
    refused = &GPIOB->MODER;
    EXPECT_EQ(NX_ERROR_IO, Close());
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, Close());
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(I2CCold, FailedPeripheralGateAcquireHasNoRetainedEffects) {
    refused = &RCC->APB1ENR;
    EXPECT_EQ(NX_ERROR_IO, Prepare());
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(nullptr, state.registers);
}

TEST_F(I2CCold, AliasedSnapshotsRejectBeforeAcquisition) {
    pins[1].snapshot = pins[0].snapshot;
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(I2CCold, AsynchronousPrepareCloseRestoresPublisherPriorities) {
    config.asynchronous = true;
    nx_i2c_v1_state_t async{};
    NVIC->IPR[I2C1_EV_IRQn] = 0x70U;
    NVIC->IPR[I2C1_ER_IRQn] = 0x90U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    EXPECT_TRUE(async.initialized);
    EXPECT_EQ(reinterpret_cast<volatile uint32_t*>(I2C1), async.registers);
    EXPECT_EQ(0xC0U, async.line_mask);
    NVIC->IPR[I2C1_EV_IRQn] = 0x50U;
    NVIC->IPR[I2C1_ER_IRQn] = 0x50U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_async_close(&async, &config, &receipt));
    EXPECT_EQ(0x70U, NVIC->IPR[I2C1_EV_IRQn]);
    EXPECT_EQ(0x90U, NVIC->IPR[I2C1_ER_IRQn]);
    EXPECT_EQ(nullptr, async.registers);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, PendingPublisherRejectsWithoutAcquiringHardware) {
    config.asynchronous = true;
    nx_i2c_v1_state_t async{};
    NVIC->ISPR[(unsigned)I2C1_EV_IRQn / 32U] =
        1U << ((unsigned)I2C1_EV_IRQn % 32U);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, ActivePublisherRejectsWithoutErasingItsIndependentFact) {
    config.asynchronous = true;
    const unsigned irq = (unsigned)I2C1_EV_IRQn;
    *const_cast<volatile uint32_t*>(&NVIC->IABR[irq / 32U]) = 1U << (irq % 32U);
    nx_i2c_v1_state_t async{};
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    EXPECT_EQ(1U, NVIC_GetActive(I2C1_EV_IRQn));
    EXPECT_EQ(0U, NVIC_GetPendingIRQ(I2C1_EV_IRQn));
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(I2C1_EV_IRQn));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_FALSE(receipt.owned);
}
TEST_F(I2CCold, RefusedPublisherEnableDoesNotReportSuccessfulPrepare) {
    config.asynchronous = true;
    nx_i2c_v1_state_t async{};
    refused_irq = (int)I2C1_ER_IRQn;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    refused_irq = -1;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_async_close(&async, &config, &receipt));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, RefusedPublisherPriorityDoesNotReportSuccessfulPrepare) {
    config.asynchronous = true;
    nx_i2c_v1_state_t async{};
    refused_priority = (int)I2C1_ER_IRQn;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    refused_priority = -1;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_async_close(&async, &config, &receipt));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, BusyBusRollsBackAsynchronousAcquisition) {
    config.asynchronous = true;
    nx_i2c_v1_state_t async{};
    I2C1->SR2 = I2C_SR2_BUSY;
    auto original = GPIOB->MODER;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32_i2c_async_prepare(&async, &config, &receipt));
    EXPECT_EQ(original, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(nullptr, async.registers);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(I2CCold, SecondControllerKeepsFirstControllerIndependent) {
    config.asynchronous = true;
    nx_i2c_v1_state_t first{};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_i2c_async_prepare(&first, &config, &receipt));
    uint16_t other_snapshots[2]{};
    nx_stm32_i2c_pin_config_t other_pins[2]{{&other_snapshots[0], 10U},
                                            {&other_snapshots[1], 11U}};
    nx_stm32_i2c_config_t other_config{I2C2,      other_pins, nullptr,
                                       42000000U, 5U,         true};
    nx_stm32_i2c_cold_receipt_t other_receipt{};
    nx_i2c_v1_state_t second{};
    GPIOB->IDR |= 0xC00U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_i2c_async_prepare(&second, &other_config,
                                                     &other_receipt));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32_i2c_async_close(&second, &other_config, &other_receipt));
    EXPECT_TRUE(first.initialized);
    EXPECT_NE(0U, I2C1->CR1 & I2C_CR1_PE);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_i2c_async_close(&first, &config, &receipt));
}

extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t incoming{masked ? 1U : 0U};
    masked = true;
    return incoming;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t incoming) {
    masked = incoming.value != 0U;
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}
/** \brief Read exactly one external enable fact from relocated MMIO. */
extern "C" uint32_t nx_i2c_cold_irq_enabled(IRQn_Type irq) {
    return (NVIC->ISER[(unsigned)irq / 32U] >> ((unsigned)irq % 32U)) & 1U;
}
/** \brief Pending is independent of enable and active publisher state. */
extern "C" uint32_t nx_i2c_cold_irq_pending(IRQn_Type irq) {
    return (NVIC->ISPR[(unsigned)irq / 32U] >> ((unsigned)irq % 32U)) & 1U;
}
/** \brief Disabling a vector does not erase an active publisher. */
extern "C" uint32_t nx_i2c_cold_irq_active(IRQn_Type irq) {
    return (NVIC->IABR[(unsigned)irq / 32U] >> ((unsigned)irq % 32U)) & 1U;
}
extern "C" void nx_i2c_cold_irq_enable(IRQn_Type irq) {
    if ((int)irq != refused_irq)
        *const_cast<volatile uint32_t*>(&NVIC->ISER[(unsigned)irq / 32U]) |=
            1U << ((unsigned)irq % 32U);
}
extern "C" void nx_i2c_cold_irq_disable(IRQn_Type irq) {
    *const_cast<volatile uint32_t*>(&NVIC->ISER[(unsigned)irq / 32U]) &=
        ~(1U << ((unsigned)irq % 32U));
}
extern "C" void nx_i2c_cold_irq_clear(IRQn_Type irq) {
    *const_cast<volatile uint32_t*>(&NVIC->ISPR[(unsigned)irq / 32U]) &=
        ~(1U << ((unsigned)irq % 32U));
}
extern "C" void nx_i2c_cold_irq_priority(IRQn_Type irq, uint32_t priority) {
    if ((int)irq != refused_priority)
        NVIC->IPR[(unsigned)irq] = (uint8_t)(priority << 4U);
}
extern "C" void nx_i2c_cold_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == refused)
        return;
    for (size_t i = 0U; i < 8U; ++i) {
        if (reg == &NVIC->ICER[i]) {
            *const_cast<volatile uint32_t*>(&NVIC->ISER[i]) &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[i]) {
            *const_cast<volatile uint32_t*>(&NVIC->ISPR[i]) &= ~value;
            return;
        }
    }
    *reg = value;
}
