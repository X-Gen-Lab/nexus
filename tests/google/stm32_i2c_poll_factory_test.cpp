/**
 * \file            stm32_i2c_poll_factory_test.cpp
 * \brief           Actual generated polling factory and concrete C lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
class SystemLeaf {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
};
SystemLeaf* system_leaf;
bool privileged;
uint64_t now;
volatile uint32_t* refused;
uint32_t held_bits;
unsigned writes;
bool hold_stop;
std::vector<uint8_t> transmitted;
class PollFactory : public testing::Test {
  protected:
    testing::NiceMock<SystemLeaf> system;
    const nx_i2c_port_t* port = nullptr;
    const nx_i2c_endpoint_t* endpoint = nullptr;
    nx_stm32_i2c_state_t* state = nullptr;
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        privileged = true;
        now = 100U;
        refused = nullptr;
        held_bits = 0U;
        writes = 0U;
        hold_stop = false;
        transmitted.clear();
        GPIOB->IDR = (1U << 6U) | (1U << 7U);
        system_leaf = &system;
        ON_CALL(system, Preflight()).WillByDefault(testing::Return(NX_SUCCESS));
        ON_CALL(system, Start()).WillByDefault(testing::Return(0));
        ON_CALL(system, Stop()).WillByDefault(testing::Return(0));
        port = nx_factory_i2c(NX_I2C_ID_I2C0);
        endpoint = nx_factory_i2c_device(NX_I2C_DEVICE_ID_DEVICE0);
        ASSERT_NE(nullptr, port);
        ASSERT_NE(nullptr, endpoint);
        state = static_cast<nx_stm32_i2c_state_t*>(port->context);
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            privileged = true;
            g_gd32_model_mask = 0U;
            g_gd32_model_isr = false;
            refused = nullptr;
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
        system_leaf = nullptr;
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
};
} /* namespace */

TEST_F(PollFactory, FactoryLookupIsPureAndControllerIdentityIsExact) {
    EXPECT_EQ(port, nx_factory_i2c(NX_I2C_ID_I2C0));
    EXPECT_EQ(endpoint, nx_factory_i2c_device(NX_I2C_DEVICE_ID_DEVICE0));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(endpoint, port));
    EXPECT_FALSE(state->initialized);
    EXPECT_EQ(nullptr, state->registers);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(PollFactory, PlatformStartsStopsAndRestartsWithoutAnyDmaOrIrqLease) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOCEN;
    GPIOB->MODER = 3U;
    GPIOB->AFR[0] = 0xFU;
    Start();
    EXPECT_EQ(I2C1, state->registers);
    EXPECT_TRUE(state->initialized);
    EXPECT_EQ(0xA000U, GPIOB->MODER & 0xF000U);
    EXPECT_EQ(0x44000000U, GPIOB->AFR[0] & 0xFF000000U);
    EXPECT_EQ(0U, RCC->AHB1ENR & (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN));
    EXPECT_EQ(0U, NVIC->ISER[0]);
    EXPECT_EQ(0U, NVIC->ISER[1]);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(RCC_AHB1ENR_GPIOCEN, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(3U, GPIOB->MODER);
    EXPECT_EQ(0xFU, GPIOB->AFR[0]);
    EXPECT_FALSE(state->initialized);
    Start();
}

TEST_F(PollFactory, PublicFiniteWriteUsesSelectedAddressAndDrainsStop) {
    Start();
    uint8_t payload[]{0x10U, 0x29U, 0xA5U};
    nx_i2c_message_t message{payload, sizeof(payload), false};
    size_t count = SIZE_MAX;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(endpoint, &message, 1U,
                                                      1000U, &count));
    EXPECT_EQ(sizeof(payload), count);
    EXPECT_EQ((std::vector<uint8_t>{0xA0U, 0x10U, 0x29U, 0xA5U}), transmitted);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_STOP);
    EXPECT_EQ(0U, I2C1->SR2 & I2C_SR2_MSL);
    EXPECT_FALSE(state->active);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}

TEST_F(PollFactory, ExpiredPublicRequestCreatesNoHardwareTransaction) {
    Start();
    uint8_t payload = 0x6AU;
    nx_i2c_message_t message{&payload, 1U, false};
    size_t count = SIZE_MAX;
    const uint32_t before = I2C1->CR1;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_i2c_endpoint_transaction(endpoint, &message, 1U, now, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(before, I2C1->CR1);
    EXPECT_TRUE(transmitted.empty());
    EXPECT_FALSE(state->active);
}

TEST_F(PollFactory, PoweredForeignPinRejectsBeforeSystemOrControllerEffects) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER = 2U << 12U;
    EXPECT_CALL(system, Start()).Times(0);
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state->initialized);
    EXPECT_EQ(2U << 12U, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(PollFactory, RefusedPinClockAcquireCannotStartThePeripheral) {
    refused = &RCC->AHB1ENR;
    held_bits = RCC_AHB1ENR_GPIOBEN;
    EXPECT_CALL(system, Start()).Times(0);
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_FALSE(state->initialized);
    refused = nullptr;
    (void)testing::Mock::VerifyAndClearExpectations(&system);
    Start();
}

TEST_F(PollFactory, RefusedWithdrawalRetainsTheSessionUntilRealStopRetry) {
    Start();
    refused = &GPIOB->MODER;
    held_bits = 3U << 14U;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
    Start();
}

TEST_F(PollFactory, FailedWireDrainReturnsErrorAndNeverReportsTransferSuccess) {
    Start();
    uint8_t payload = 0x72U;
    nx_i2c_message_t message{&payload, 1U, false};
    size_t count = 0U;
    hold_stop = true;
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_endpoint_transaction(endpoint, &message, 1U,
                                                       5000U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_TRUE(state->fault);
    EXPECT_FALSE(state->active);
    EXPECT_EQ(0U, I2C1->CR1 & I2C_CR1_PE);
}

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_gd32_model_mask};
    g_gd32_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_gd32_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++writes;
    if (reg == refused)
        value = (value & ~held_bits) | (*reg & held_bits);
    *reg = value;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return system_leaf->Preflight();
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {system_leaf->Start(), 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return system_leaf->Stop();
}
extern "C" void nx_i2c_model_poll(void* context) {
    auto* state = static_cast<nx_stm32_i2c_state_t*>(context);
    ++now;
    if ((state->registers->CR1 & I2C_CR1_START) != 0U) {
        state->registers->SR1 = I2C_SR1_SB;
        state->registers->SR2 = I2C_SR2_MSL | I2C_SR2_BUSY;
    }
    if ((state->registers->CR1 & I2C_CR1_STOP) != 0U && !hold_stop) {
        state->registers->CR1 &= ~(uint32_t)I2C_CR1_STOP;
        state->registers->SR2 = 0U;
    }
}
extern "C" uint8_t nx_i2c_model_read(void* context) {
    auto* state = static_cast<nx_stm32_i2c_state_t*>(context);
    return (uint8_t)state->registers->DR;
}
extern "C" void nx_i2c_model_write(void* context, uint32_t value) {
    auto* state = static_cast<nx_stm32_i2c_state_t*>(context);
    transmitted.push_back((uint8_t)value);
    state->registers->DR = value;
    state->registers->SR1 = (state->registers->SR1 & I2C_SR1_SB) != 0U
                                ? I2C_SR1_ADDR
                                : (I2C_SR1_TXE | I2C_SR1_BTF);
}
extern "C" void nx_i2c_model_address_cleared(void* context) {
    auto* state = static_cast<nx_stm32_i2c_state_t*>(context);
    state->registers->SR1 = I2C_SR1_TXE;
}
