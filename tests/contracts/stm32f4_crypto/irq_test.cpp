/**
 * \file            irq_test.cpp
 * \brief           Cold crypto IRQ lease effects and failed withdrawal receipts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_crypto_irq.h"
#include "stm32f4xx.h"
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
void nx_hal_test_cpu_reset(void);
void nx_hal_test_dsb_hook(void (*hook)(void));
bool nx_hal_test_isr(bool next);
}
#include <cstring>
#include <gtest/gtest.h>

namespace {
bool refuse_enable, refuse_disable, refuse_clear, refuse_priority;
unsigned writes;
constexpr uint16_t vector = HASH_RNG_IRQn;
constexpr unsigned bank = vector / 32U;
constexpr uint32_t bit = UINT32_C(1) << (vector % 32U);
void PriorityWriteback() {
    if (refuse_priority)
        NVIC->IPR[vector] = 0x30U;
}
class CryptoIrq : public testing::Test {
  protected:
    nx_stm32f4_crypto_irq_t receipt{};
    void SetUp() override {
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        nx_hal_test_cpu_reset();
        nx_hal_test_dsb_hook(PriorityWriteback);
        refuse_enable = refuse_disable = refuse_clear = refuse_priority = false;
        writes = 0U;
        NVIC->IPR[vector] = 0x70U;
    }
    void TearDown() override {
        nx_hal_test_dsb_hook(nullptr);
    }
};
TEST_F(CryptoIrq, ColdAdmissionNeverClaimsForeignVectorOrInvalidPriority) {
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 16U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, TIM2_IRQn, 5U),
              NX_ERROR_INVALID);
    NVIC->ISER[bank] = bit;
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 5U), NX_ERROR_BUSY);
    NVIC->ISER[bank] = 0U;
    NVIC->ISPR[bank] = bit;
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 5U), NX_ERROR_BUSY);
    NVIC->ISPR[bank] = 0U;
    *const_cast<volatile uint32_t*>(&NVIC->IABR[bank]) = bit;
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 5U), NX_ERROR_BUSY);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(writes, 0U);
}
TEST_F(CryptoIrq, AcquireAndReleasePreservePeerBitsAndRestoreExactPriority) {
    const uint32_t peer = UINT32_C(1) << (CRYP_IRQn % 32U);
    NVIC->ISER[bank] = peer;
    EXPECT_EQ(nx_stm32f4_crypto_irq_acquire(&receipt, vector, 5U), NX_SUCCESS);
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(NVIC->IPR[vector], 0x50U);
    EXPECT_EQ(NVIC->ISER[bank], peer | bit);
    NVIC->ISPR[bank] = peer | bit;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_SUCCESS);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(NVIC->IPR[vector], 0x70U);
    EXPECT_EQ(NVIC->ISER[bank], peer);
    EXPECT_EQ(NVIC->ISPR[bank], peer);
    unsigned before = writes;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_SUCCESS);
    EXPECT_EQ(writes, before);
}
TEST_F(CryptoIrq, FailedEnableRetainsChangedPriorityUntilExplicitRollback) {
    refuse_enable = true;
    EXPECT_EQ(nx_stm32f4_crypto_irq_acquire(&receipt, vector, 5U), NX_ERROR_IO);
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(receipt.saved_priority, 0x70U);
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_SUCCESS);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(NVIC->IPR[vector], 0x70U);
}
TEST_F(CryptoIrq, FailedDisableOrPendingClearCannotEraseTheReceipt) {
    ASSERT_EQ(nx_stm32f4_crypto_irq_acquire(&receipt, vector, 5U), NX_SUCCESS);
    refuse_disable = true;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_ERROR_BUSY);
    EXPECT_TRUE(receipt.owned);
    refuse_disable = false;
    refuse_clear = true;
    NVIC->ISPR[bank] = bit;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_ERROR_BUSY);
    EXPECT_TRUE(receipt.owned);
    refuse_clear = false;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_SUCCESS);
}
TEST_F(CryptoIrq, FailedPriorityWriteRetainsReceiptThroughItsFailedRestore) {
    refuse_priority = true;
    EXPECT_EQ(nx_stm32f4_crypto_irq_acquire(&receipt, vector, 5U), NX_ERROR_IO);
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_ERROR_BUSY);
    EXPECT_TRUE(receipt.owned);
    refuse_priority = false;
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector), NX_SUCCESS);
    EXPECT_EQ(NVIC->IPR[vector], 0x70U);
}
TEST_F(CryptoIrq, IsrCannotAcquireOrReleaseTheTaskOwnedVector) {
    nx_hal_test_isr(true);
    EXPECT_EQ(nx_stm32f4_crypto_irq_admit(&receipt, vector, 5U),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_stm32f4_crypto_irq_acquire(&receipt, vector, 5U),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_stm32f4_crypto_irq_release(&receipt, vector),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(writes, 0U);
    nx_hal_test_isr(false);
}
} /* namespace */
extern "C" void nx_crypto_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    for (unsigned i = 0U; i < 3U; ++i) {
        if (reg == &NVIC->ISER[i]) {
            if (!refuse_enable)
                NVIC->ISER[i] |= value;
            return;
        }
        if (reg == &NVIC->ICER[i]) {
            if (!refuse_disable)
                NVIC->ISER[i] &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[i]) {
            if (!refuse_clear)
                NVIC->ISPR[i] &= ~value;
            return;
        }
    }
    *reg = value;
}
