/**
 * \file            stm32_crc_test.cpp
 * \brief           Exact CRC word vectors, deadlines and clock responsibility
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_crc.h"
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool privileged;
bool in_isr;
bool masked;
bool hold_clock;
bool reject_clock;
bool reject_reset;
unsigned writes;
unsigned fed;
nx_time_us_t now;
nx_time_us_t expire_at_feed;
unsigned drop_clock_at;

uint32_t calculate(uint32_t previous, uint32_t word) {
    previous ^= word;
    for (unsigned bit = 0U; bit < 32U; ++bit) {
        previous = previous & 0x80000000U ? (previous << 1U) ^ 0x04c11db7U
                                          : previous << 1U;
    }
    return previous;
}
class Stm32Crc : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    alignas(uint32_t) nx_stm32_crc_state_t state{};
    nx_crc_port_t port{&nx_stm32_crc_ops, &state};
    uint32_t result = 0xabcdef12U;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40023000U), 0x1000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40023000U));
        privileged = true;
        in_isr = masked = hold_clock = reject_clock = reject_reset = false;
        writes = fed = drop_clock_at = 0U;
        now = 100U;
        expire_at_feed = 0U;
        CRC->IDR = 0x55U;
    }
    void TearDown() override {
        privileged = true;
        in_isr = masked = hold_clock = reject_clock = reject_reset = false;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(nx_stm32_crc_close(&state), NX_SUCCESS);
            EXPECT_EQ(munmap(mapping, 0x1000U), 0);
        }
    }
    void prepare() {
        ASSERT_EQ(nx_stm32_crc_prepare(&state), NX_SUCCESS);
    }
};
} /* namespace */

extern "C" void nx_crc_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &CRC->CR) {
        if (!reject_reset && (value & CRC_CR_RESET) != 0U) {
            CRC->DR = 0xffffffffU;
            value &= ~static_cast<uint32_t>(CRC_CR_RESET);
        }
    } else if (reg == &CRC->DR) {
        value = calculate(CRC->DR, value);
        ++fed;
        if (expire_at_feed != 0U)
            now = expire_at_feed;
        if (drop_clock_at != 0U && fed >= drop_clock_at) {
            RCC->AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_CRCEN);
        }
    } else if (reg == &RCC->AHB1ENR) {
        if (hold_clock)
            value |= RCC->AHB1ENR & static_cast<uint32_t>(RCC_AHB1ENR_CRCEN);
        if (reject_clock)
            value &= ~static_cast<uint32_t>(RCC_AHB1ENR_CRCEN);
    }
    *reg = value;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{masked ? 1U : 0U};
    masked = true;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    masked = previous.value != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now++;
}

TEST_F(Stm32Crc, PreparationAcquiresOnlyTheSelectedClock) {
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN;
    prepare();
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.clock_added);
    EXPECT_EQ(nx_stm32_crc_prepare(&state), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_crc_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(CRC->IDR, 0x55U);
}
TEST_F(Stm32Crc, PreexistingClockAndIndependentDataArePreserved) {
    RCC->AHB1ENR = RCC_AHB1ENR_CRCEN;
    prepare();
    EXPECT_FALSE(state.clock_added);
    EXPECT_EQ(nx_stm32_crc_close(&state), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_CRCEN);
    EXPECT_EQ(CRC->IDR, 0x55U);
}
TEST_F(Stm32Crc, EmptyAndSingleWordVectorsUseTheDeclaredInitialState) {
    prepare();
    ASSERT_EQ(nx_crc_port_compute(&port, nullptr, 0U, 1000U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0xffffffffU);
    const uint32_t zero = 0U;
    ASSERT_EQ(nx_crc_port_compute(&port, &zero, 1U, 1000U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0xc704dd7bU);
    const uint32_t ones = 0xffffffffU;
    ASSERT_EQ(nx_crc_port_compute(&port, &ones, 1U, 1000U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0U);
}
TEST_F(Stm32Crc, WordsAreNumericMsbFirstWithoutImplicitBytePadding) {
    prepare();
    const uint32_t input[] = {0x31323334U, 0x35363738U, 0x39000000U};
    ASSERT_EQ(nx_crc_port_compute(&port, input, 3U, 1000U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0xae24e09dU);
    EXPECT_EQ(fed, 3U);
}
TEST_F(Stm32Crc, EveryCalculationResetsThePreviousPartialState) {
    prepare();
    const uint32_t input = 0U;
    ASSERT_EQ(nx_crc_port_compute(&port, &input, 1U, 1000U, &result),
              NX_SUCCESS);
    CRC->DR = 0x12345678U;
    ASSERT_EQ(nx_crc_port_compute(&port, &input, 1U, 1000U, &result),
              NX_SUCCESS);
    EXPECT_EQ(result, 0xc704dd7bU);
}
TEST_F(Stm32Crc, ExpiredAdmissionDoesNotResetOrReadInput) {
    prepare();
    const unsigned before = writes;
    const uint32_t input = 0U;
    EXPECT_EQ(nx_crc_port_compute(&port, &input, 1U, 0U, &result),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(writes, before);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Stm32Crc, ADeadlineCrossedDuringFeedingKeepsOutputUnchanged) {
    prepare();
    expire_at_feed = 1000U;
    const uint32_t input[33]{};
    EXPECT_EQ(nx_crc_port_compute(&port, input, 33U, 1000U, &result),
              NX_ERROR_TIMEOUT);
    EXPECT_LT(fed, 33U);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Stm32Crc, AClockLostDuringFeedingIsNotSuccessfulData) {
    prepare();
    drop_clock_at = 1U;
    const uint32_t input[33]{};
    EXPECT_EQ(nx_crc_port_compute(&port, input, 33U, 1000U, &result),
              NX_ERROR_IO);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Stm32Crc, RefusedResetNeverPublishesAnOldSignature) {
    prepare();
    CRC->DR = 0U;
    reject_reset = true;
    EXPECT_EQ(nx_crc_port_compute(&port, nullptr, 0U, 1000U, &result),
              NX_ERROR_IO);
    EXPECT_EQ(result, 0xabcdef12U);
}
TEST_F(Stm32Crc, RefusedClockEnableKeepsItsPartialReceipt) {
    reject_clock = true;
    EXPECT_EQ(nx_stm32_crc_prepare(&state), NX_ERROR_IO);
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.clock_added);
    reject_clock = false;
    EXPECT_EQ(nx_stm32_crc_close(&state), NX_SUCCESS);
}
TEST_F(Stm32Crc, RefusedClockWithdrawalRetainsResponsibilityUntilRetry) {
    prepare();
    hold_clock = true;
    EXPECT_EQ(nx_stm32_crc_close(&state), NX_ERROR_BUSY);
    EXPECT_TRUE(state.clock_added);
    hold_clock = false;
    EXPECT_EQ(nx_stm32_crc_close(&state), NX_SUCCESS);
    EXPECT_FALSE(state.clock_added);
}
TEST_F(Stm32Crc, MaskedAndIsrControlsRejectBeforeEffects) {
    masked = true;
    const unsigned before = writes;
    EXPECT_EQ(nx_stm32_crc_prepare(&state), NX_ERROR_CONTEXT);
    EXPECT_TRUE(masked);
    masked = false;
    in_isr = true;
    EXPECT_EQ(nx_stm32_crc_prepare(&state), NX_ERROR_CONTEXT);
    EXPECT_EQ(writes, before);
}
TEST_F(Stm32Crc, ResultCannotOverwriteItsPrivateLifecycleReceipt) {
    prepare();
    const unsigned before = writes;
    EXPECT_EQ(nx_crc_port_compute(&port, nullptr, 0U, 1000U,
                                  reinterpret_cast<uint32_t*>(&state)),
              NX_ERROR_INVALID);
    EXPECT_EQ(writes, before);
    EXPECT_TRUE(state.initialized);
}
TEST_F(Stm32Crc, ResultsCannotWriteTheCalculatorOrItsClockControl) {
    prepare();
    const uint32_t input = 0U;
    volatile uint32_t* registers[] = {
        &CRC->DR, reinterpret_cast<volatile uint32_t*>(&CRC->IDR), &CRC->CR,
        &RCC->AHB1ENR};
    for (volatile uint32_t* reg : registers) {
        const uint32_t original = *reg;
        const unsigned before = writes;
        EXPECT_EQ(nx_crc_port_compute(&port, &input, 1U, 1000U,
                                      const_cast<uint32_t*>(reg)),
                  NX_ERROR_INVALID);
        EXPECT_EQ(writes, before);
        EXPECT_EQ(*reg, original);
        *reg = original;
    }
    EXPECT_EQ(CRC->IDR, 0x55U);
}
TEST_F(Stm32Crc, InputCannotBeChangedByTheCalculatorsOwnResetOrControl) {
    prepare();
    volatile uint32_t* registers[] = {
        &CRC->DR, reinterpret_cast<volatile uint32_t*>(&CRC->IDR), &CRC->CR,
        &RCC->AHB1ENR};
    for (volatile uint32_t* reg : registers) {
        const unsigned before = writes;
        EXPECT_EQ(nx_crc_port_compute(&port, const_cast<const uint32_t*>(reg),
                                      1U, 1000U, &result),
                  NX_ERROR_INVALID);
        EXPECT_EQ(writes, before);
        EXPECT_EQ(result, 0xabcdef12U);
        result = 0xabcdef12U;
    }
    EXPECT_EQ(CRC->IDR, 0x55U);
}
