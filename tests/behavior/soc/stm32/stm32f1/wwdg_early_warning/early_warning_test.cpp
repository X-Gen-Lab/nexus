/**
 * \file            early_warning_test.cpp
 * \brief           F103 WWDG EWI sticky ownership and reset-persistent witness.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/early_warning.h"
#include "drivers/watchdog/window_watchdog.h"
#include "nexus/arch/arch.h"
#include <gtest/gtest.h>

RCC_TypeDef rcc_model{};
WWDG_TypeDef watchdog_model{};
PWR_TypeDef power_model{};
BKP_TypeDef backup_model{};

namespace {
constexpr uint32_t backup_gates = RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
constexpr uint32_t watchdog_exception = 16U + WWDG_IRQn;
struct {
    uint32_t exception;
    uint32_t vector;
    uint32_t priority;
    unsigned reloads;
    unsigned records;
    unsigned acknowledgements;
    unsigned power_status_clears;
    bool masked;
    bool refuse_irq;
    volatile uint32_t* refuse;
} model;

class F103Ewi : public testing::Test {
  protected:
    nx_stm32f1_wwdg_t owner{};
    nx_watchdog_state_t observed{};
    nx_stm32f1_wwdg_ewi_t warning{};
    const nx_watchdog_port_t face{&nx_stm32f1_wwdg_ops, &owner};

    void MemoryReset() {
        model = {};
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RCC->APB1RSTR = 0U;
        RCC->CR = RCC_CR_HSIRDY;
        RCC->CFGR = RCC_CFGR_SWS_HSI;
        WWDG->CFR = WWDG_CFR_W;
        WWDG->CR = WWDG_CR_T;
        WWDG->SR = 0U;
        PWR->CR = 0U;
        warning = {};
        owner = {};
    }

    void SetUp() override {
        BKP->DR1 = 0U;
        MemoryReset();
    }

    void StartWatchdog() {
        ASSERT_EQ(nx_stm32f1_wwdg_prepare(&owner, 3U), NX_SUCCESS);
        ASSERT_EQ(nx_watchdog_port_enable(&face, 15000U, false, &observed),
                  NX_SUCCESS);
        ASSERT_TRUE(observed.enabled);
    }

    void Expire() {
        WWDG->CR = WWDG_CR_WDGA | WWDG_CR_T_6;
        WWDG->SR = WWDG_SR_EWIF;
        model.exception = watchdog_exception;
    }
};

TEST_F(F103Ewi, RealIrqMarkerSurvivesSystemResetWithoutFeeding) {
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    bool seen = true;
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_take(&warning, &seen), NX_SUCCESS);
    EXPECT_FALSE(seen);
    StartWatchdog();
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_arm(&warning, &face, 4U), NX_SUCCESS);
    EXPECT_EQ(model.priority, 4U);
    EXPECT_EQ(model.vector, 1U);
    EXPECT_FALSE(model.masked);
    const unsigned reloads = model.reloads;
    Expire();
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_irq(&warning), NX_SUCCESS);
    EXPECT_EQ(model.reloads, reloads);
    EXPECT_EQ(model.records, 1U);
    EXPECT_EQ(model.acknowledgements, 1U);
    EXPECT_EQ(WWDG->SR, 0U);
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_irq(&warning), NX_ERROR_STATE);
    MemoryReset();
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_take(&warning, &seen), NX_SUCCESS);
    EXPECT_TRUE(seen);
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_take(&warning, &seen), NX_SUCCESS);
    EXPECT_FALSE(seen);
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_close(&warning), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(PWR->CR, 0U);
}

TEST_F(F103Ewi, IrreversibleSourceRetainsBackupOnNvicFailure) {
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    StartWatchdog();
    model.refuse_irq = true;
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_arm(&warning, &face, 3U), NX_ERROR_IO);
    EXPECT_NE(WWDG->CFR & WWDG_CFR_EWI, 0U);
    EXPECT_EQ(model.reloads, 1U);
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_close(&warning), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_stm32f1_wwdg_close(&owner), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(RCC->APB1ENR & backup_gates, backup_gates);
    EXPECT_NE(PWR->CR & PWR_CR_DBP, 0U);
}

TEST_F(F103Ewi, ForeignBackupAndAlreadyPendingVectorArePreserved) {
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_ERROR_BUSY);
    EXPECT_EQ(PWR->CR, 0U);
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    BKP->DR1 = 123U;
    bool seen = true;
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_take(&warning, &seen), NX_ERROR_BUSY);
    EXPECT_EQ(BKP->DR1, 123U);
    EXPECT_TRUE(seen);
    BKP->DR1 = 0U;
    StartWatchdog();
    model.vector = 2U;
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_arm(&warning, &face, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(WWDG->CFR & WWDG_CFR_EWI, 0U);
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_close(&warning), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR & backup_gates, 0U);
}

TEST_F(F103Ewi, FailedBackupWriteAcknowledgesIrqWithoutReloadOrFalseWitness) {
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    StartWatchdog();
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_arm(&warning, &face, 4U), NX_SUCCESS);
    Expire();
    model.exception = watchdog_exception + 1U;
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_irq(&warning), NX_ERROR_CONTEXT);
    EXPECT_EQ(WWDG->SR, WWDG_SR_EWIF);
    model.exception = watchdog_exception;
    model.refuse = &BKP->DR1;
    EXPECT_EQ(nx_stm32f1_wwdg_ewi_irq(&warning), NX_ERROR_IO);
    EXPECT_EQ(model.reloads, 1U);
    EXPECT_EQ(model.acknowledgements, 1U);
    EXPECT_EQ(BKP->DR1, 0U);
}
TEST_F(F103Ewi, BackupAccessNeverWritesPowerStatusClearBits) {
    PWR->CR = PWR_CR_CWUF | PWR_CR_CSBF | PWR_CR_PVDE;
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_prepare(&warning), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_wwdg_ewi_close(&warning), NX_SUCCESS);
    EXPECT_EQ(model.power_status_clears, 0U);
    EXPECT_EQ(PWR->CR, PWR_CR_PVDE);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
}
} /* namespace */

extern "C" void ewi_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == model.refuse) {
        return;
    }
    if (reg == &WWDG->CR) {
        ++model.reloads;
    } else if (reg == &WWDG->CFR) {
        value |= *reg & static_cast<uint32_t>(WWDG_CFR_EWI);
    } else if (reg == &WWDG->SR) {
        ++model.acknowledgements;
        value &= *reg;
    } else if (reg == &PWR->CR) {
        const uint32_t status = PWR_CR_CWUF | PWR_CR_CSBF;
        if ((value & status) != 0U)
            ++model.power_status_clears;
        value &= ~status;
    } else if (reg == &BKP->DR1) {
        EXPECT_EQ(RCC->APB1ENR & backup_gates, backup_gates);
        EXPECT_NE(PWR->CR & PWR_CR_DBP, 0U);
        ++model.records;
    }
    *reg = value;
}

extern "C" void ewi_enable_irq(IRQn_Type irq) {
    EXPECT_EQ(irq, WWDG_IRQn);
    if (!model.refuse_irq) {
        model.vector |= 1U;
    }
}

extern "C" void ewi_priority(IRQn_Type irq, uint32_t priority) {
    EXPECT_EQ(irq, WWDG_IRQn);
    model.priority = priority;
}

extern "C" uint32_t ewi_get_priority(IRQn_Type irq) {
    EXPECT_EQ(irq, WWDG_IRQn);
    return model.priority;
}

extern "C" uint32_t ewi_vector(IRQn_Type irq) {
    EXPECT_EQ(irq, WWDG_IRQn);
    return model.vector;
}

extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}

extern "C" bool nx_arch_in_isr(void) {
    return model.exception != 0U;
}

extern "C" uint32_t nx_arch_exception_number(void) {
    return model.exception;
}

extern "C" bool nx_arch_irq_is_masked(void) {
    return model.masked;
}

extern "C" void nx_arch_dsb(void) {
}

extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{model.masked ? 1U : 0U};
    model.masked = true;
    return previous;
}

extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    model.masked = previous.value != 0U;
}
