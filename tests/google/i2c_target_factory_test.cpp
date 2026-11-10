/**
 * \file            i2c_target_factory_test.cpp
 * \brief           Actual typed factory, Platform prepare, retention and close
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_clock.h"
#include "stm32f407_i2c_target.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
class SystemLeaf {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
};
SystemLeaf* system_leaf;
bool privileged;
bool refuse_disable;
bool hold_reset;
volatile uint32_t* refused;
unsigned writes;
uint64_t now;
FLASH_TypeDef flash;
class TargetFactory : public testing::Test {
  protected:
    testing::NiceMock<SystemLeaf> system;
    const nx_i2c_target_port_t* port{};
    nx_stm32_i2c_target_state_t* state{};
    nx_i2c_target_request_t request{};
    uint8_t payload[2]{};
    void* mmio{MAP_FAILED};
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        flash = {};
        flash.ACR = NEXUS_FLASH_WAIT_STATES;
        g_nx_stm32_system.flash = &flash;
        privileged = true;
        refuse_disable = false;
        hold_reset = false;
        refused = nullptr;
        writes = 0U;
        now = 100U;
        system_leaf = &system;
        ON_CALL(system, Preflight()).WillByDefault(testing::Return(NX_SUCCESS));
        ON_CALL(system, Start()).WillByDefault(testing::Invoke([] {
            RCC->CR =
                RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
            RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
            RCC->CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
            g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
            g_nx_stm32_system.started = true;
            ++g_nx_stm32_system.time_epoch;
            return 0;
        }));
        ON_CALL(system, Stop()).WillByDefault(testing::Invoke([] {
            g_nx_stm32_system.started = false;
            return 0;
        }));
        port = nx_factory_i2c_target(NX_I2C_TARGET_ID_TARGET);
        ASSERT_NE(nullptr, port);
        state = static_cast<nx_stm32_i2c_target_state_t*>(port->context);
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            privileged = true;
            g_f4_model_mask = 0U;
            g_f4_model_isr = false;
            refuse_disable = false;
            hold_reset = false;
            hold_reset = false;
            refused = nullptr;
            for (auto& word : g_f4_model_nvic.IABR)
                const_cast<volatile uint32_t&>(word) = 0U;
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
        system_leaf = nullptr;
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
};
} /* namespace */

TEST_F(TargetFactory, LookupIsPureAndPlatformBindsTheOneTargetOwner) {
    EXPECT_EQ(port, nx_factory_i2c_target(NX_I2C_TARGET_ID_TARGET));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state->initialized);
    Start();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_TRUE(state->initialized);
    EXPECT_EQ(I2C1, state->config->registers);
    EXPECT_EQ(0U, I2C1->CR1 & (I2C_CR1_ACK | I2C_CR1_NOSTRETCH));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(nullptr, state->config);
    Start();
}

TEST_F(TargetFactory,
       ForeignActiveI2cRejectsBeforeSystemStartAndKeepsItsClock) {
    RCC->APB1ENR = RCC_APB1ENR_I2C1EN;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
    EXPECT_CALL(system, Start()).Times(0);
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(I2C_CR1_PE | I2C_CR1_ACK, I2C1->CR1);
    EXPECT_EQ(RCC_APB1ENR_I2C1EN, RCC->APB1ENR);
}

TEST_F(TargetFactory, FailedAddressProgrammingRetainsPlatformUntilRetryClose) {
    refused = &I2C1->OAR1;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(nullptr, state->config);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refused = nullptr;
    hold_reset = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(nullptr, state->config);
    Start();
}

TEST_F(TargetFactory, BorrowedRequestCannotEscapeARefusedPlatformDrain) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_target_prepare(port, &request, payload, sizeof(payload),
                                    nullptr, 0U, NX_I2C_TARGET_RECEIVE, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_target_submit(port, &request));
    refuse_disable = true;
    EXPECT_NE(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(nullptr, state->active);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    refuse_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, state->active);
}

/** \brief External System startup succeeds only after the readonly preflight.
 */
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return system_leaf->Preflight();
}
/** \brief Mock the external System owner, not the target's hardware policy. */
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {system_leaf->Start(), 0, 0U};
}
/** \brief A failing peripheral drain must prevent System withdrawal. */
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return system_leaf->Stop();
}
/** \brief This fixture models the identity-probe boundary only. */
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
/** \brief Preserve the single privileged Thread execution contract. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Exception entry is controlled only by the hardware model. */
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
/** \brief Return the actual incoming modeled mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
/** \brief Synchronous host writes do not have an Arm bus pipeline. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief Core release/acquire publication remains exercised by its C code. */
extern "C" void nx_arch_dmb(void) {
}
/** \brief IRQ critical sections save rather than invent the incoming mask. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return saved;
}
/** \brief Restore exactly the saved modeled mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    g_f4_model_mask = state.value;
}
/** \brief One explicit Core monotonic clock supplies finite deadlines. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
/** \brief Actual reads are observable without replacing provider decisions. */
extern "C" uint32_t nx_i2c_target_factory_read(volatile uint32_t* address) {
    return *address;
}
/** \brief Reset has hardware side effects; refused writes remain refused. */
extern "C" void nx_platform_cold_model_write(volatile uint32_t* address,
                                             uint32_t value) {
    ++writes;
    if (address == refused && value != 0U)
        hold_reset = true;
    if (address != refused && !(hold_reset && address == &RCC->APB1RSTR))
        *address = value;
    if (address == &RCC->APB1RSTR && !hold_reset &&
        (value & RCC_APB1RSTR_I2C1RST) != 0U) {
        std::memset(I2C1, 0, sizeof(*I2C1));
        I2C1->TRISE = 2U;
    }
}
/** \brief NVIC enable/disable and pending clear are command operations. */
extern "C" uint32_t nx_i2c_target_factory_irq(unsigned command, IRQn_Type irq,
                                              uint32_t value) {
    unsigned index = static_cast<unsigned>(irq);
    unsigned bank = index / 32U;
    uint32_t bit = 1U << (index % 32U);
    switch (command) {
        case 0U:
            g_f4_model_nvic.ISER[bank] |= bit;
            break;
        case 1U:
            if (!refuse_disable)
                g_f4_model_nvic.ISER[bank] &= ~bit;
            break;
        case 2U:
            g_f4_model_nvic.ISPR[bank] &= ~bit;
            break;
        case 3U:
            g_f4_model_nvic.IPR[index] = static_cast<uint8_t>(value << 4U);
            break;
        case 4U:
            return g_f4_model_nvic.ISER[bank] & bit;
        case 5U:
            return g_f4_model_nvic.ISPR[bank] & bit;
        case 6U:
            return g_f4_model_nvic.IABR[bank] & bit;
        case 7U:
            return g_f4_model_nvic.IPR[index] >> 4U;
        default:
            break;
    }
    return 0U;
}

/** \brief The mocked System supplies the same finite Core time domain. */
extern "C" uint64_t nx_stm32_system_now(nx_stm32_system_t*) {
    return now;
}
