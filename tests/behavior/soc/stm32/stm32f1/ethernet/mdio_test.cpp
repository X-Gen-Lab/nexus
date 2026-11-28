/**
 * \file            mdio_test.cpp
 * \brief           F107 Clause22 commands, deadline and bounded pad teardown.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/ethernet/mdio.h"
#include "mdio_model.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <utility>
#include <vector>

namespace {
constexpr uintptr_t peripheral = 0x40000000U;
constexpr size_t peripheral_size = 0x2A000U;
constexpr uint32_t core_clock_gates = RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN;
constexpr uint32_t mac_clock = RCC_AHBENR_ETHMACEN;
constexpr uint32_t default_low = 0x44444444U;
constexpr uint32_t old_mdio = 0x44444844U;
constexpr uint32_t old_mdc = 0x44444484U;
constexpr uint32_t active_mdio = 0x44444B44U;
constexpr uint32_t active_mdc = 0x444444B4U;
constexpr uint16_t phy_value = 0x739AU;

using Write = std::pair<volatile uint32_t*, uint32_t>;
std::vector<Write> writes;
nx_time_us_t now;
nx_time_us_t complete_at;
bool task;
bool masked;
bool system_live;
bool divide_clock_on_completion;
bool refuse_gate_off;
volatile uint32_t* reject_pad;

class F107Mdio : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_mdio_t state{};
    const nx_mdio_port_t port{&nx_stm32f1_mdio_ops, &state};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral), peripheral_size,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral));
        task = system_live = true;
        masked = refuse_gate_off = divide_clock_on_completion = false;
        reject_pad = nullptr;
        writes.clear();
        now = 10U;
        complete_at = NX_DEADLINE_NEVER;
        RCC->AHBENR = core_clock_gates;
        RCC->APB2ENR = RCC_APB2ENR_IOPAEN;
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY |
                  RCC_CR_PLL2ON | RCC_CR_PLL2RDY;
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PLLSRC |
                    RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2;
        RCC->CFGR2 = RCC_CFGR2_PREDIV1_DIV5 | RCC_CFGR2_PREDIV2_DIV5 |
                     RCC_CFGR2_PLL2MUL8 | RCC_CFGR2_PREDIV1SRC_PLL2;
        GPIOA->CRL = old_mdio;
        GPIOA->ODR = 4U;
        GPIOC->CRL = old_mdc;
        GPIOC->ODR = 2U;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_size), 0);
        }
    }

    void prepare() {
        ASSERT_EQ(nx_stm32f1_mdio_prepare(&state), NX_SUCCESS);
        EXPECT_EQ(RCC->AHBENR, core_clock_gates | mac_clock);
        EXPECT_EQ(GPIOA->CRL, active_mdio);
        EXPECT_EQ(GPIOC->CRL, active_mdc);
        writes.clear();
    }

    void expect_released() {
        EXPECT_EQ(RCC->AHBENR, core_clock_gates);
        EXPECT_EQ(RCC->APB2ENR, RCC_APB2ENR_IOPAEN);
        EXPECT_EQ(GPIOA->CRL, old_mdio);
        EXPECT_EQ(GPIOC->CRL, old_mdc);
        EXPECT_EQ(GPIOA->ODR, 4U);
        EXPECT_EQ(GPIOC->ODR, 2U);
    }
};

TEST_F(F107Mdio, EnforcesClockContextAndExclusiveIdlePeripheral) {
    system_live = false;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_STATE);
    system_live = true;
    RCC->CFGR ^= RCC_CFGR_SWS_PLL;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_STATE);
    RCC->CFGR ^= RCC_CFGR_SWS_PLL;
    masked = true;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_CONTEXT);
    masked = false;
    RCC->AHBENR |= RCC_AHBENR_ETHMACRXEN;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_BUSY);
    EXPECT_TRUE(writes.empty());
}

TEST_F(F107Mdio, BusyOnFirstGateReadbackCannotReleaseTheClock) {
    ETH->MACMIIAR = ETH_MACMIIAR_MB;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_BUSY);
    EXPECT_EQ(GPIOA->CRL, old_mdio);
    EXPECT_EQ(GPIOC->CRL, old_mdc);
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->AHBENR & mac_clock, mac_clock);
    ETH->MACMIIAR = 0U;
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_SUCCESS);
    expect_released();
}

TEST_F(F107Mdio, WritesDataBeforeExactCommandAndReadsOnlyOnCompletion) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    complete_at = 13U;
    uint16_t value = 0xAAAAU;
    ASSERT_EQ(nx_mdio_read(&port, 31U, 30U, &value, 20U), NX_SUCCESS);
    EXPECT_EQ(value, phy_value);
    ASSERT_EQ(writes.size(), 1U);
    EXPECT_EQ(writes[0],
              Write(&ETH->MACMIIAR, (31U << 11U) | (30U << 6U) | 1U));
    writes.clear();
    complete_at = now + 3U;
    ASSERT_EQ(nx_mdio_write(&port, 1U, 2U, 0xBEEFU, now + 10U), NX_SUCCESS);
    ASSERT_EQ(writes.size(), 2U);
    EXPECT_EQ(writes[0], Write(&ETH->MACMIIDR, 0xBEEFU));
    EXPECT_EQ(writes[1], Write(&ETH->MACMIIAR, (1U << 11U) | (2U << 6U) | 3U));
    ASSERT_EQ(nx_stm32f1_mdio_close(&state), NX_SUCCESS);
    expect_released();
}

TEST_F(F107Mdio, RejectsInvalidExpiredAndForeignBusyRequestsWithoutEffects) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    uint16_t value = 0xAAAAU;
    EXPECT_EQ(nx_mdio_read(&port, 32U, 0U, &value, 20U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &value, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &value, now), NX_ERROR_TIMEOUT);
    ETH->MACMIIAR = ETH_MACMIIAR_MB;
    EXPECT_EQ(nx_mdio_write(&port, 1U, 1U, 42U, 20U), NX_ERROR_BUSY);
    task = false;
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &value, 20U), NX_ERROR_CONTEXT);
    EXPECT_EQ(value, 0xAAAAU);
    EXPECT_TRUE(writes.empty());
}

TEST_F(F107Mdio, TimeoutLeavesValueUntouchedAndCloseRetainsUntilBusyClears) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    uint16_t value = 0xAAAAU;
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &value, 14U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(value, 0xAAAAU);
    ASSERT_EQ(writes.size(), 1U);
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(RCC->AHBENR & mac_clock, mac_clock);
    EXPECT_EQ(GPIOA->CRL, active_mdio);
    EXPECT_EQ(GPIOC->CRL, active_mdc);
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &value, 30U), NX_ERROR_STATE);
    EXPECT_EQ(writes.size(), 1U);
    RCC->AHBENR &= ~mac_clock;
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_ERROR_BUSY);
    RCC->AHBENR |= mac_clock;
    ETH->MACMIIAR = 0U;
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_SUCCESS);
    expect_released();
    ASSERT_NO_FATAL_FAILURE(prepare());
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_SUCCESS);
}

TEST_F(F107Mdio, RejectsCompletionAtDeadlineAndNeverPublishesReadData) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    uint16_t output = 0xAAAAU;
    complete_at = 13U;
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &output, 13U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, 0xAAAAU);
}

TEST_F(F107Mdio, DoesNotPublishAResultAfterTheReviewedClockChanges) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    uint16_t output = 0xAAAAU;
    complete_at = 13U;
    divide_clock_on_completion = true;
    EXPECT_EQ(nx_mdio_read(&port, 1U, 1U, &output, 20U), NX_ERROR_STATE);
    EXPECT_EQ(output, 0xAAAAU);
}

TEST_F(F107Mdio, FailedPadAndClockReadbacksRetainReceiptsForRetry) {
    reject_pad = &GPIOA->CRL;
    EXPECT_EQ(nx_stm32f1_mdio_prepare(&state), NX_ERROR_IO);
    EXPECT_NE(RCC->AHBENR & mac_clock, 0U);
    reject_pad = nullptr;
    refuse_gate_off = true;
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_ERROR_IO);
    EXPECT_EQ(GPIOA->CRL, old_mdio);
    EXPECT_EQ(GPIOC->CRL, old_mdc);
    refuse_gate_off = false;
    EXPECT_EQ(nx_stm32f1_mdio_close(&state), NX_SUCCESS);
    expect_released();
}
} /* namespace */

extern "C" void nx_test_f107_write(volatile uint32_t* reg, uint32_t value) {
    writes.emplace_back(reg, value);
    if (reg != reject_pad && !(reg == &RCC->AHBENR && refuse_gate_off &&
                               (value & mac_clock) == 0U)) {
        *reg = value;
    }
}

extern "C" void nx_test_f107_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & 0xFFFFU)) & ~(value >> 16U);
}

extern "C" bool nx_arch_is_privileged(void) {
    return task;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    if (now >= complete_at && (ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0U) {
        const uint32_t command = ETH->MACMIIAR;
        if ((command & ETH_MACMIIAR_MW) == 0U) {
            ETH->MACMIIDR = phy_value;
        }
        ETH->MACMIIAR = command & ~static_cast<uint32_t>(ETH_MACMIIAR_MB);
        if (divide_clock_on_completion) {
            RCC->CFGR |= RCC_CFGR_HPRE_DIV2;
        }
    }
    return now++;
}
