/**
 * \file            rtc_test.cpp
 * \brief           F103xB RTC synchronization, read and withdrawal contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/rtc/instance.h"
#include "nexus/io/rtc_aux.h"
#include "register_model.h"
#include <gtest/gtest.h>

RCC_TypeDef nx_f1_rtc_model_rcc;
PWR_TypeDef nx_f1_rtc_model_pwr;
RTC_TypeDef nx_f1_rtc_model_rtc;
BKP_TypeDef nx_f1_rtc_model_bkp;

namespace {
constexpr uint32_t gates = RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
constexpr uint32_t oscillator =
    RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;
constexpr uint32_t flags = RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF;
constexpr auto valid_crl = RTC_CRL_RTOFF | RTC_CRL_RSF | flags;
unsigned crl_writes;
unsigned sync_at;
unsigned rtoff_delay;
unsigned crl_reads;
unsigned change_high;
unsigned reg_writes;
nx_time_us_t ticks;
bool normal_thread;
bool clock_live;
unsigned gate_stop_drops;
unsigned dbp_stop_drops;

class RetainedRtc : public testing::Test {
  protected:
    nx_stm32f1_rtc_t state{};
    nx_rtc_port_t face{&nx_stm32f1_rtc_backup_ops, &state};

    void SetUp() override {
        *RCC = {};
        *PWR = {};
        *RTC = {};
        *BKP = {};
        RCC->BDCR = oscillator;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RTC->CRL = valid_crl;
        crl_writes = crl_reads = change_high = reg_writes = 0U;
        sync_at = 2U;
        rtoff_delay = 0U;
        ticks = 0U;
        normal_thread = clock_live = true;
        gate_stop_drops = dbp_stop_drops = 0U;
    }
};
} /* namespace */

extern "C" uint32_t nx_f1_rtc_model_read(volatile const uint32_t* reg) {
    if (reg == &RTC->CRL && rtoff_delay != 0U && --rtoff_delay == 0U)
        RTC->CRL |= RTC_CRL_RTOFF;
    if (reg == &RTC->CRL && crl_writes != 0U && ++crl_reads == sync_at) {
        RTC->CRL |= RTC_CRL_RSF;
    }
    if (reg == &RTC->CNTH) {
        const uint32_t value = RTC->CNTH;
        if (change_high != 0U) {
            --change_high;
            RTC->CNTH = (value + 1U) & UINT16_MAX;
            RTC->CNTL = 0U;
        }
        return value;
    }
    return *reg;
}

extern "C" void nx_f1_rtc_model_write(volatile uint32_t* reg, uint32_t value) {
    ++reg_writes;
    if (reg == &RTC->CRL) {
        ++crl_writes;
        EXPECT_EQ(flags, value);
        EXPECT_EQ(gates, RCC->APB1ENR & gates);
        EXPECT_NE(0U, PWR->CR & PWR_CR_DBP);
        EXPECT_NE(0U, RTC->CRL & RTC_CRL_RTOFF);
        *reg = (*reg & RTC_CRL_RTOFF) | (*reg & value & flags);
    } else if (reg == &PWR->CR) {
        EXPECT_EQ(0U, value & (PWR_CR_CWUF | PWR_CR_CSBF));
        EXPECT_EQ(gates, RCC->APB1ENR & gates);
        if ((value & PWR_CR_DBP) == 0U && dbp_stop_drops != 0U) {
            --dbp_stop_drops;
            return;
        }
        *reg = value;
    } else if (reg == &RCC->APB1ENR) {
        if ((value & gates) == 0U && gate_stop_drops != 0U) {
            --gate_stop_drops;
            return;
        }
        *reg = value;
    } else {
        ADD_FAILURE() << "write outside the RTC adoption receipt";
    }
}

extern "C" bool nx_arch_is_privileged(void) {
    return normal_thread;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" bool nx_stm32f1_system_live(void) {
    return clock_live;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ticks++;
}

TEST_F(RetainedRtc, SynchronizesBeforeReadsAndPreservesForeignState) {
    PWR->CR = PWR_CR_PLS_LEV2;
    BKP->DR1 = 0xFFFFB311U;
    BKP->DR10 = 0xBBCCDDEEU;
    RTC->CNTH = 0x1234U;
    RTC->CNTL = 0xAABBU;

    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(1U, crl_writes);
    EXPECT_EQ(valid_crl, RTC->CRL);
    EXPECT_EQ(PWR_CR_PLS_LEV2, PWR->CR);
    EXPECT_EQ(gates | RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    uint32_t value = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_ticks(&state, &value));
    EXPECT_EQ(0x1234AABBU, value);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_backup_read(&face, 0U, &value));
    EXPECT_EQ(0xB311U, value);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_backup_read(&face, 9U, &value));
    EXPECT_EQ(0xDDEEU, value);
    EXPECT_EQ(NX_ERROR_INVALID, nx_rtc_port_backup_read(&face, 10U, &value));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_rtc_port_backup_read(&face, 0U, const_cast<uint32_t*>(&BKP->DR1)));
    nx_rtc_snapshot_t calendar{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_rtc_port_read(&face, &calendar));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_rtc_port_backup_write(&face, 0U, 1U));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&face));
    EXPECT_EQ(oscillator, RCC->BDCR);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_rtc_ticks(&state, &value));
    EXPECT_EQ(0xDDEEU, value);
}

TEST_F(RetainedRtc, ObservesFullWrapOrKeepsOutputOnUnstableRead) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_prepare(&state));
    RTC->CNTH = UINT16_MAX;
    RTC->CNTL = UINT16_MAX;
    change_high = 1U;
    uint32_t value = 0x12345678U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_ticks(&state, &value));
    EXPECT_EQ(0U, value);
    value = 0xDEADBEEFU;
    change_high = 100U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_rtc_ticks(&state, &value));
    EXPECT_EQ(0xDEADBEEFU, value);
    RTC->CRL &= ~static_cast<uint32_t>(RTC_CRL_RSF);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_rtc_ticks(&state, &value));
    EXPECT_EQ(0xDEADBEEFU, value);
}

TEST_F(RetainedRtc, RefusesPvdOwnerAndUnknownOscillatorWithoutWrites) {
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_prepare(&state));
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_PWREN);
    RCC->BDCR ^= RCC_BDCR_LSEBYP;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_rtc_prepare(&state));
    RCC->BDCR = oscillator;
    normal_thread = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32f1_rtc_prepare(&state));
    normal_thread = true;
    clock_live = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(0U, reg_writes);
}

TEST_F(RetainedRtc, TimeoutRestoresProtectionAndCanRetryFailedStop) {
    sync_at = 0U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(0U, PWR->CR & PWR_CR_DBP);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(flags, RTC->CRL & flags);
    RTC->CRL |= RTC_CRL_RSF;
    sync_at = 2U;
    crl_reads = crl_writes = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_prepare(&state));
    gate_stop_drops = 1U;
    EXPECT_EQ(NX_ERROR_IO, nx_rtc_port_stop(&face));
    uint32_t value = 23U;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_read(&face, 0U, &value));
    EXPECT_EQ(23U, value);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&face));
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}

TEST_F(RetainedRtc, RefusesForeignConfigurationAndAssertedBackupReset) {
    RTC->CRL |= RTC_CRL_CNF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(0U, crl_writes);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    RTC->CRL = valid_crl;
    PWR->CR = PWR_CR_PVDE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(0U, crl_writes);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    PWR->CR = 0U;
    RCC->APB1RSTR = RCC_APB1RSTR_BKPRST;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(0U, crl_writes);
}

TEST_F(RetainedRtc, WaitsForRtoffBeforeTouchingApbMirror) {
    RTC->CRL = flags;
    rtoff_delay = 3U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(1U, crl_writes);
    EXPECT_EQ(0U, PWR->CR & PWR_CR_DBP);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_close(&state));
}

TEST_F(RetainedRtc, KeepsGateReceiptUntilWriteProtectionRestores) {
    dbp_stop_drops = 2U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_rtc_prepare(&state));
    EXPECT_EQ(gates, RCC->APB1ENR & gates);
    EXPECT_NE(0U, PWR->CR & PWR_CR_DBP);
    uint32_t value = 23U;
    EXPECT_EQ(NX_ERROR_STATE, nx_rtc_port_backup_read(&face, 0U, &value));
    EXPECT_EQ(23U, value);
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&face));
    EXPECT_EQ(0U, PWR->CR & PWR_CR_DBP);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}
