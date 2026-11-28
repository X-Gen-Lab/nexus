/**
 * \file            rtc_test.cpp
 * \brief           F103xB RTC synchronization, read and withdrawal contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/rtc/alarm.h"
#include "nexus/io/rtc_aux.h"
#include "register_model.h"
#include <gtest/gtest.h>

RCC_TypeDef nx_f1_rtc_model_rcc;
PWR_TypeDef nx_f1_rtc_model_pwr;
RTC_TypeDef nx_f1_rtc_model_rtc;
BKP_TypeDef nx_f1_rtc_model_bkp;
EXTI_TypeDef nx_f1_rtc_model_exti;

namespace {
constexpr uint32_t gates = RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
constexpr uint32_t oscillator =
    RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;
constexpr uint32_t flags = RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF;
constexpr uint32_t alarm_line = 1U << 17U;
constexpr uint32_t foreign_line = 1U << 13U;
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
uint32_t vector_priority;
bool vector_enabled;
bool vector_pending;
bool vector_active;
bool drop_exti_ack;
bool drop_vector_disable;
bool pass_target_at_route;
unsigned comparator_writes;

class RetainedRtc : public testing::Test {
  protected:
    nx_stm32f1_rtc_t state{};
    nx_rtc_port_t face{&nx_stm32f1_rtc_backup_ops, &state};

    void SetUp() override {
        *RCC = {};
        *PWR = {};
        *RTC = {};
        *BKP = {};
        *EXTI = {};
        RCC->BDCR = oscillator;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        RTC->CRL = valid_crl;
        crl_writes = crl_reads = change_high = reg_writes = 0U;
        sync_at = 2U;
        rtoff_delay = 0U;
        ticks = 0U;
        normal_thread = clock_live = true;
        gate_stop_drops = dbp_stop_drops = 0U;
        vector_priority = 2U;
        vector_enabled = vector_pending = vector_active = false;
        drop_exti_ack = drop_vector_disable = pass_target_at_route = false;
        comparator_writes = 0U;
    }
};
} /* namespace */

extern "C" uint32_t nx_f1_rtc_model_get_enable(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    return vector_enabled;
}
extern "C" uint32_t nx_f1_rtc_model_get_pending(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    return vector_pending;
}
extern "C" uint32_t nx_f1_rtc_model_get_active(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    return vector_active;
}
extern "C" uint32_t nx_f1_rtc_model_get_priority(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    return vector_priority;
}
extern "C" void nx_f1_rtc_model_set_priority(IRQn_Type irq, uint32_t priority) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    vector_priority = priority;
}
extern "C" void nx_f1_rtc_model_enable(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    vector_enabled = true;
}
extern "C" void nx_f1_rtc_model_disable(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    if (!drop_vector_disable)
        vector_enabled = false;
}
extern "C" void nx_f1_rtc_model_clear(IRQn_Type irq) {
    EXPECT_EQ(RTC_Alarm_IRQn, irq);
    vector_pending = false;
}

extern "C" uint32_t nx_f1_rtc_model_read(volatile const uint32_t* reg) {
    EXPECT_NE(reg, &RTC->ALRH);
    EXPECT_NE(reg, &RTC->ALRL);
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
        EXPECT_EQ(0U, value & ~static_cast<uint32_t>(flags | RTC_CRL_RSF |
                                                     RTC_CRL_CNF));
        EXPECT_EQ(gates, RCC->APB1ENR & gates);
        EXPECT_NE(0U, PWR->CR & PWR_CR_DBP);
        EXPECT_NE(0U, RTC->CRL & RTC_CRL_RTOFF);
        const bool committing =
            (*reg & RTC_CRL_CNF) != 0U && (value & RTC_CRL_CNF) == 0U;
        *reg = (*reg & RTC_CRL_RTOFF) | (*reg & value & (flags | RTC_CRL_RSF)) |
               (value & RTC_CRL_CNF);
        if (committing) {
            *reg &= ~static_cast<uint32_t>(RTC_CRL_RTOFF);
            rtoff_delay = 3U;
        }
    } else if (reg == &RTC->ALRH || reg == &RTC->ALRL) {
        EXPECT_NE(0U, RTC->CRL & RTC_CRL_CNF);
        EXPECT_NE(0U, RTC->CRL & RTC_CRL_RTOFF);
        EXPECT_NE(0U, PWR->CR & PWR_CR_DBP);
        EXPECT_LE(value, UINT16_MAX);
        ++comparator_writes;
        *reg = value;
    } else if (reg == &EXTI->PR) {
        EXPECT_EQ(alarm_line, value);
        if (!drop_exti_ack)
            *reg &= ~value;
    } else if (reg == &EXTI->IMR || reg == &EXTI->RTSR) {
        EXPECT_EQ(*reg & ~alarm_line, value & ~alarm_line);
        *reg = value;
        if (reg == &EXTI->IMR && (value & alarm_line) != 0U &&
            pass_target_at_route) {
            RTC->CNTH = RTC->ALRH;
            RTC->CNTL = RTC->ALRL;
        }
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

class RetainedAlarm : public RetainedRtc {
  protected:
    nx_stm32f1_rtc_alarm_t alarm{};

    void SetUp() override {
        RetainedRtc::SetUp();
        RTC->CRL &= ~static_cast<uint32_t>(RTC_CRL_ALRF);
        RTC->CNTH = UINT16_MAX;
        RTC->CNTL = UINT16_MAX - 5U;
        BKP->DR1 = 0x1234U;
        EXTI->IMR = EXTI->RTSR = EXTI->PR = foreign_line;
    }

    nx_result_t arm_after_counter_event() {
        EXPECT_EQ(NX_ERROR_EMPTY, nx_stm32f1_rtc_alarm_arm(&alarm, 4U));
        EXPECT_EQ(0U, comparator_writes);
        ++RTC->CNTL;
        return nx_stm32f1_rtc_alarm_arm(&alarm, 4U);
    }
};

TEST_F(RetainedAlarm, Exti17OnlyDeliversOneRawComparatorAcrossWrap) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_prepare(&alarm, 7U));
    EXPECT_EQ(7U, vector_priority);
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_stm32f1_rtc_alarm_arm(&alarm, UINT32_MAX - 4U));
    ASSERT_EQ(NX_SUCCESS, arm_after_counter_event());
    EXPECT_EQ(2U, comparator_writes);
    EXPECT_EQ(0U, RTC->ALRH);
    EXPECT_EQ(4U, RTC->ALRL);
    EXPECT_EQ(0U, RTC->CRH);
    EXPECT_EQ(0U, PWR->CR);
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stm32f1_rtc_alarm_take(&alarm));

    RTC->CRL |= RTC_CRL_ALRF;
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stm32f1_rtc_alarm_take(&alarm));
    EXTI->PR |= alarm_line;
    vector_pending = true;
    EXPECT_EQ(NX_ERROR_EMPTY, nx_stm32f1_rtc_alarm_take(&alarm));
    nx_stm32f1_rtc_alarm_irq(&alarm);
    EXPECT_FALSE(vector_enabled);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_take(&alarm));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32f1_rtc_alarm_take(&alarm));
    EXPECT_EQ(foreign_line, EXTI->IMR);
    EXPECT_EQ(foreign_line, EXTI->RTSR);
    EXPECT_EQ(foreign_line, EXTI->PR);
    EXPECT_EQ(valid_crl & ~RTC_CRL_ALRF, RTC->CRL);
    EXPECT_FALSE(vector_pending);
    EXPECT_EQ(4U, RTC->ALRL);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_close(&alarm));
    EXPECT_EQ(2U, vector_priority);
    EXPECT_EQ(0x1234U, BKP->DR1);
    EXPECT_EQ(oscillator, RCC->BDCR);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}

TEST_F(RetainedAlarm, InheritedLineOrComparatorEventIsUntouched) {
    EXTI->PR |= alarm_line;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_alarm_prepare(&alarm, 7U));
    EXPECT_EQ(0U, reg_writes);
    EXPECT_EQ(alarm_line | foreign_line, EXTI->PR);
    EXTI->PR = foreign_line;
    RTC->CRL |= RTC_CRL_ALRF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_rtc_alarm_prepare(&alarm, 7U));
    EXPECT_NE(0U, RTC->CRL & RTC_CRL_ALRF);
    EXPECT_EQ(0U, comparator_writes);
    EXPECT_EQ(2U, vector_priority);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}

TEST_F(RetainedAlarm, TargetReachingComparatorDuringArmWithdrawsTheVector) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_prepare(&alarm, 7U));
    pass_target_at_route = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, arm_after_counter_event());
    EXPECT_EQ(2U, comparator_writes);
    EXPECT_FALSE(vector_enabled);
    EXPECT_EQ(foreign_line, EXTI->IMR);
    EXPECT_EQ(foreign_line, EXTI->RTSR);
    EXPECT_EQ(foreign_line, EXTI->PR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_close(&alarm));
}

TEST_F(RetainedAlarm, ClosingRetainsOwnershipUntilInterruptIsDrained) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_prepare(&alarm, 7U));
    ASSERT_EQ(NX_SUCCESS, arm_after_counter_event());
    RTC->CRL |= RTC_CRL_ALRF;
    EXTI->PR |= alarm_line;
    vector_pending = true;
    drop_vector_disable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_rtc_alarm_close(&alarm));
    EXPECT_EQ(alarm_line | foreign_line, EXTI->IMR);
    EXPECT_EQ(gates, RCC->APB1ENR & gates);
    drop_vector_disable = false;
    drop_exti_ack = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_rtc_alarm_close(&alarm));
    EXPECT_EQ(gates, RCC->APB1ENR & gates);
    EXPECT_EQ(alarm_line | foreign_line, EXTI->PR);
    EXPECT_EQ(7U, vector_priority);
    drop_exti_ack = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_rtc_alarm_close(&alarm));
    nx_stm32f1_rtc_alarm_irq(&alarm);
    EXPECT_EQ(0U, alarm.effects);
    EXPECT_EQ(foreign_line, EXTI->PR);
    EXPECT_EQ(2U, vector_priority);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
}
