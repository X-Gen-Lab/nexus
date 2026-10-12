/**
 * \file            gd32_timebase_contract_test.cpp
 * \brief           Live clock epochs and cold timer ownership against real code
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f4xx.h"
#include "nexus/core/fault.h"
#include "private/system.h"
}
#include <csignal>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>

NVIC_Type g_gd32_model_nvic{};
SCB_Type g_gd32_model_scb{};
SysTick_Type g_gd32_model_systick{};
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
uint32_t SystemCoreClock = 200000000U;

namespace {
class ClockPort {
  public:
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(void, TimerWrite, (volatile uint32_t * reg, uint32_t value));
    MOCK_METHOD(void, DisableIrq, (unsigned irq));
    MOCK_METHOD(void, Observe,
                (nx_fault_domain_t domain, nx_result_t reason, const char* file,
                 unsigned line));
};
struct ParkSignal {};
volatile std::sig_atomic_t returning_fault_seen;
void AlarmAfterReturningFault(int) {
    _exit(returning_fault_seen == 1 ? 42 : 92);
}
ClockPort* clock_port;
bool hold_timer_count;
bool hold_timer_update_clear;
bool hold_timer_gate_drop;
bool update_observed;
bool hold_irq_disable;
void (*before_save)(void);

class Gd32Timebase : public testing::Test {
    void* registers = MAP_FAILED;

  protected:
    testing::NiceMock<ClockPort> clock_mock;

    void SetUp() override {
        registers =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(registers, reinterpret_cast<void*>(0x40000000U));
        std::memset(registers, 0, 0x80000U);
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        clock_port = &clock_mock;
        hold_timer_count = false;
        hold_timer_update_clear = false;
        hold_timer_gate_drop = false;
        update_observed = false;
        hold_irq_disable = false;
        before_save = nullptr;
        ON_CALL(clock_mock, Start()).WillByDefault(testing::Return(0));
        ON_CALL(clock_mock, Stop()).WillByDefault(testing::Return(0));
        ON_CALL(clock_mock, Preflight())
            .WillByDefault(testing::Return(NX_SUCCESS));
        EXPECT_CALL(clock_mock, DisableIrq(testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(clock_mock, DisableIrq(testing::_))
            .WillByDefault([](unsigned irq) {
                if (!hold_irq_disable)
                    NVIC->ISER[irq / 32U] &= ~(UINT32_C(1) << (irq % 32U));
            });
        EXPECT_CALL(clock_mock,
                    Observe(testing::_, testing::_, testing::_, testing::_))
            .Times(0);
        ON_CALL(clock_mock,
                Observe(testing::_, testing::_, testing::_, testing::_))
            .WillByDefault([](nx_fault_domain_t, nx_result_t, const char*,
                              unsigned) { throw ParkSignal{}; });
        EXPECT_CALL(clock_mock, TimerWrite(testing::_, testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(clock_mock, TimerWrite(testing::_, testing::_))
            .WillByDefault([](volatile uint32_t* reg, uint32_t value) {
                if (reg == &TIMER_INTF(TIMER1)) {
                    if (!hold_timer_update_clear || value != 0U)
                        *reg &= value;
                } else if (reg == &TIMER_CNT(TIMER1) && hold_timer_count) {
                    *reg = 37U;
                } else {
                    *reg = value;
                }
                if (reg == &TIMER_SWEVG(TIMER1) &&
                    (value & TIMER_SWEVG_UPG) != 0U) {
                    update_observed = true;
                    TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
                    TIMER_CNT(TIMER1) = hold_timer_count ? 37U : 0U;
                }
            });
    }
    void TearDown() override {
        /* Host fixture reset after observer interception is not recovery from
         * the production non-returning terminal path. */
        testing::Mock::VerifyAndClearExpectations(&clock_mock);
        hold_timer_count = false;
        hold_timer_update_clear = false;
        hold_timer_gate_drop = false;
        hold_irq_disable = false;
        before_save = nullptr;
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        if (registers == reinterpret_cast<void*>(0x40000000U)) {
            USART_CTL0(USART0) = 0U;
            USART_CTL0(USART1) = 0U;
            SPI_CTL0(SPI0) = 0U;
            SPI_CTL0(SPI4) = 0U;
            I2C_CTL0(I2C0) = 0U;
            I2C_CTL0(I2C1) = 0U;
            TIMER_CTL0(TIMER0) = 0U;
            TIMER_CTL0(TIMER2) = 0U;
            TIMER_CTL0(TIMER3) = 0U;
            ADC_CTL1(ADC0) = 0U;
            EXPECT_EQ(0, nx_gd32_soc_stop());
            EXPECT_EQ(0, munmap(registers, 0x80000U));
        } else if (registers != MAP_FAILED) {
            EXPECT_EQ(0, munmap(registers, 0x80000U));
        }
        clock_port = nullptr;
    }
};
} /* namespace */

TEST_F(Gd32Timebase, DormantDomainNeverReturnsAStaleCounter) {
    TIMER_CNT(TIMER1) = 37U;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(TIMER_INTF_UPIF, TIMER_INTF(TIMER1));
}

TEST_F(Gd32Timebase, FailedClockStartupNeverPublishesLiveTime) {
    EXPECT_CALL(clock_mock, Start()).WillOnce(testing::Return(-1));
    TIMER_CNT(TIMER1) = 41U;
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(0U, nx_gd32_now_us());
}

TEST_F(Gd32Timebase, SoftwareUpdateIsClearedBeforePublishingAZeroEpoch) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    EXPECT_TRUE(update_observed);
    EXPECT_EQ(0U, TIMER_INTF(TIMER1) & TIMER_INTF_UPIF);
    EXPECT_EQ(0U, TIMER_CNT(TIMER1));
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_NE(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
}

TEST_F(Gd32Timebase, RefusedInitialUpdateClearCannotPublishLiveTime) {
    hold_timer_update_clear = true;
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}

TEST_F(Gd32Timebase, RefusedSoftwareUpdateCannotPublishAClockEpoch) {
    EXPECT_CALL(clock_mock, TimerWrite(&TIMER_SWEVG(TIMER1), TIMER_SWEVG_UPG))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}

TEST_F(Gd32Timebase, RefusedInitialCounterZeroCannotPublishLiveTime) {
    hold_timer_count = true;
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}

TEST_F(Gd32Timebase, CounterFailureRetainsObservedCleanupResponsibility) {
    hold_timer_count = true;
    hold_timer_gate_drop = true;
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32_soc_preflight());
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_EQ(0U, nx_gd32_now_us());
    hold_timer_gate_drop = false;
    ASSERT_EQ(0, nx_gd32_soc_stop());
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_EQ(NX_SUCCESS, nx_gd32_soc_preflight());
}

TEST_F(Gd32Timebase, SuccessfulDomainEndsBeforeStaleCounterCanBeObserved) {
    EXPECT_CALL(clock_mock, Start()).Times(1);
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_CNT(TIMER1) = 53U;
    EXPECT_EQ(53U, nx_gd32_now_us());
    ASSERT_EQ(0, nx_gd32_soc_stop());
    EXPECT_EQ(0U, nx_gd32_now_us());
}

TEST_F(Gd32Timebase, PendingWrapIsNotConsumedBySnapshot) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_CNT(TIMER1) = 7U;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    g_gd32_model_mask = 1U;
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U, nx_gd32_now_us());
    EXPECT_EQ(1U, g_gd32_model_mask);
    EXPECT_EQ(TIMER_INTF_UPIF, TIMER_INTF(TIMER1));
    g_gd32_model_mask = 0U;
    TIMER1_IRQHandler();
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U, nx_gd32_now_us());
}

TEST_F(Gd32Timebase, LateIrqCannotResurrectAStoppedEpoch) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    ASSERT_EQ(0, nx_gd32_soc_stop());
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    TIMER_CNT(TIMER1) = 11U;
    TIMER1_IRQHandler();
    EXPECT_EQ(0U, nx_gd32_now_us());
}

TEST_F(Gd32Timebase, RestartCreatesAnIndependentZeroEpoch) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    TIMER1_IRQHandler();
    ASSERT_EQ(0, nx_gd32_soc_stop());
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_CNT(TIMER1) = 3U;
    EXPECT_EQ(3U, nx_gd32_now_us());
}

TEST_F(Gd32Timebase, IsolatedAckFailureStillObservesTerminalOwnership) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    EXPECT_EQ(NX_SUCCESS, nx_gd32_timebase_error());
    TIMER_CNT(TIMER1) = 59U;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    EXPECT_CALL(clock_mock,
                TimerWrite(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(clock_mock, TimerWrite(&TIMER_DMAINTEN(TIMER1), 0U)).Times(1);
    EXPECT_CALL(clock_mock, TimerWrite(&TIMER_CTL0(TIMER1), 0U)).Times(1);
    EXPECT_CALL(clock_mock, DisableIrq((unsigned)TIMER1_IRQn)).Times(1);
    g_gd32_model_isr = true;
    EXPECT_CALL(clock_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .Times(1);
    EXPECT_THROW(TIMER1_IRQHandler(), ParkSignal);
    g_gd32_model_isr = false;
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_EQ(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32U);
    EXPECT_EQ(0U, NVIC->ISER[(uint32_t)TIMER1_IRQn / 32U] & bit);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_EQ(1U, g_gd32_model_mask);
}

TEST_F(Gd32Timebase, UnisolatedAckFailureObservesFaultWithMasksAndOwnersHeld) {
    for (bool refuse_irq : {false, true}) {
        SCOPED_TRACE(refuse_irq ? "NVIC disable refused" : "DIER zero refused");
        ASSERT_EQ(0, nx_gd32_soc_start());
        TIMER_CNT(TIMER1) = 61U;
        TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
        EXPECT_CALL(clock_mock,
                    TimerWrite(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF))
            .WillOnce([](volatile uint32_t*, uint32_t) {});
        hold_irq_disable = refuse_irq;
        if (!refuse_irq) {
            EXPECT_CALL(clock_mock, TimerWrite(&TIMER_DMAINTEN(TIMER1), 0U))
                .WillOnce([](volatile uint32_t*, uint32_t) {});
        } else {
            EXPECT_CALL(clock_mock, TimerWrite(&TIMER_DMAINTEN(TIMER1), 0U))
                .Times(1);
        }
        EXPECT_CALL(clock_mock, TimerWrite(&TIMER_CTL0(TIMER1), 0U)).Times(1);
        EXPECT_CALL(clock_mock, DisableIrq((unsigned)TIMER1_IRQn)).Times(1);
        EXPECT_CALL(clock_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                        testing::_, testing::_))
            .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                         unsigned line) {
                EXPECT_EQ(1U, g_gd32_model_mask);
                EXPECT_TRUE(g_gd32_model_isr);
                EXPECT_NE(nullptr, file);
                EXPECT_GT(line, 0U);
                EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
                EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
                throw ParkSignal{};
            });
        g_gd32_model_isr = true;
        EXPECT_THROW(TIMER1_IRQHandler(), ParkSignal);
        EXPECT_EQ(1U, g_gd32_model_mask);
        EXPECT_EQ(0U, nx_gd32_now_us());
        EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
        EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&clock_mock));
        /* Reinitialize the intercepted host fixture for the next variant;
         * a real terminal observer cannot return to perform this cleanup. */
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        hold_irq_disable = false;
        EXPECT_EQ(NX_ERROR_BUSY, nx_gd32_soc_preflight());
        ASSERT_EQ(0, nx_gd32_soc_stop());
        EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
        EXPECT_EQ(NX_SUCCESS, nx_gd32_soc_preflight());
    }
}

TEST_F(Gd32Timebase, RefusedCounterIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    EXPECT_CALL(clock_mock,
                TimerWrite(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(clock_mock, TimerWrite(&TIMER_DMAINTEN(TIMER1), 0U)).Times(1);
    EXPECT_CALL(clock_mock, TimerWrite(&TIMER_CTL0(TIMER1), 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(clock_mock, DisableIrq((unsigned)TIMER1_IRQn)).Times(1);
    EXPECT_CALL(clock_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, g_gd32_model_mask);
            EXPECT_TRUE(g_gd32_model_isr);
            throw ParkSignal{};
        });
    g_gd32_model_isr = true;
    EXPECT_THROW(TIMER1_IRQHandler(), ParkSignal);
    EXPECT_EQ(1U, g_gd32_model_mask);
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_NE(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32U;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32U);
    EXPECT_EQ(0U, NVIC->ISER[bank] & bit);
    EXPECT_EQ(0U, nx_gd32_now_us());
    EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}

TEST_F(Gd32Timebase, AckFailureBeforeSnapshotMaskCannotPublishStaleTime) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    TIMER_CNT(TIMER1) = 67U;
    EXPECT_CALL(clock_mock,
                TimerWrite(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    before_save = [] {
        EXPECT_EQ(0U, g_gd32_model_mask);
        TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
        g_gd32_model_isr = true;
        TIMER1_IRQHandler();
        g_gd32_model_isr = false;
    };
    EXPECT_CALL(clock_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .Times(1);
    EXPECT_THROW(nx_gd32_now_us(), ParkSignal);
    EXPECT_EQ(nullptr, before_save);
    EXPECT_EQ(1U, g_gd32_model_mask);
    EXPECT_EQ(NX_ERROR_IO, nx_gd32_timebase_error());
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_gd32_soc_preflight());
}

TEST_F(Gd32Timebase, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_EXIT(
        {
            if (nx_gd32_soc_start() != 0)
                _exit(90);
            TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
            EXPECT_CALL(clock_mock, TimerWrite(&TIMER_INTF(TIMER1),
                                               ~(uint32_t)TIMER_INTF_UPIF))
                .WillOnce([](volatile uint32_t*, uint32_t) {});
            returning_fault_seen = 0;
            EXPECT_CALL(clock_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                            testing::_, testing::_))
                .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                             unsigned line) {
                    uint32_t bank = (uint32_t)TIMER1_IRQn / 32U;
                    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32U);
                    returning_fault_seen =
                        file != nullptr && line != 0U &&
                                g_gd32_model_mask == 1U &&
                                nx_gd32_timebase_error() == NX_ERROR_IO &&
                                (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0U &&
                                TIMER_DMAINTEN(TIMER1) == 0U &&
                                (TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) == 0U &&
                                (NVIC->ISER[bank] & bit) == 0U
                            ? 1
                            : -1;
                });
            g_gd32_model_isr = true;
            std::signal(SIGALRM, AlarmAfterReturningFault);
            alarm(1U);
            TIMER1_IRQHandler();
            _exit(99);
        },
        testing::ExitedWithCode(42), "");
}

TEST_F(Gd32Timebase, ForeignReservedTimerRejectsBeforeClockMutation) {
    TIMER_CTL0(TIMER1) = TIMER_CTL0_CEN;
    EXPECT_CALL(clock_mock, Start()).Times(0);
    EXPECT_NE(0, nx_gd32_soc_start());
    EXPECT_EQ(TIMER_CTL0_CEN, TIMER_CTL0(TIMER1));
    TIMER_CTL0(TIMER1) = 0U;
}

TEST_F(Gd32Timebase, SecondaryPollingControllersPreventClockWithdrawal) {
    ASSERT_EQ(0, nx_gd32_soc_start());
    EXPECT_CALL(clock_mock, Stop()).Times(0);
    for (volatile uint32_t* control :
         {&USART_CTL0(USART1), &SPI_CTL0(SPI0), &I2C_CTL0(I2C1),
          &TIMER_CTL0(TIMER0), &TIMER_CTL0(TIMER3)}) {
        uint32_t enable = control == &USART_CTL0(USART1) ? USART_CTL0_UEN
                          : control == &SPI_CTL0(SPI0)   ? SPI_CTL0_SPIEN
                                                         : I2C_CTL0_I2CEN;
        *control = enable;
        EXPECT_NE(0, nx_gd32_soc_stop());
        EXPECT_EQ(enable, *control);
        EXPECT_NE(0U, TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN);
        *control = 0U;
    }
}

/** \brief Save the incoming external CPU mask for a bounded snapshot. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    if (g_gd32_model_mask == 0U && before_save != nullptr) {
        void (*inject)(void) = before_save;
        before_save = nullptr;
        inject();
    }
    nx_arch_irq_state_t previous = {g_gd32_model_mask};
    g_gd32_model_mask = 1U;
    return previous;
}
/** \brief Restore exactly the mask captured by the external CPU port. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_gd32_model_mask = previous.value;
}
/** \brief Query injected exception context without changing provider state. */
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
/** \brief Supply privileged CPU context at the external register boundary. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
/** \brief Query the fixture mask at the cold admission boundary. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
/** \brief Model synchronous register writes without physical bus timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The host register model has no Cortex-M instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}
/** \brief Delegate the independent clock component to GoogleMock. */
extern "C" int nx_gd32_clock_start(void) {
    return clock_port->Start();
}
/** \brief Observe independent clock cleanup through its external port. */
extern "C" int nx_gd32_clock_stop(void) {
    return clock_port->Stop();
}
/** \brief Model the independent clock component's read-only admission. */
extern "C" nx_result_t nx_gd32_clock_preflight(void) {
    return clock_port->Preflight();
}
/** \brief Model update-event counter reset and W0C at the external MMIO port.
 */
extern "C" void nx_gd32_timebase_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    clock_port->TimerWrite(reg, value);
}
/** \brief Model refused NVIC withdrawal through the external IRQ port. */
extern "C" void nx_gd32_timebase_model_disable_irq(unsigned irq) {
    clock_port->DisableIrq(irq);
}
/** \brief Observe terminal faults without mocking the production IRQ path. */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    clock_port->Observe(domain, reason, file, line);
}
/** \brief Decode the official SDK run-clock acquisition encoding. */
extern "C" void rcu_periph_clock_enable(rcu_periph_enum peripheral) {
    REG32(RCU + ((uint32_t)peripheral >> 6U)) |=
        UINT32_C(1) << ((uint32_t)peripheral & 31U);
}
/** \brief Release only the requested modeled run-clock bit. */
extern "C" void rcu_periph_clock_disable(rcu_periph_enum peripheral) {
    if (peripheral == RCU_TIMER1 && hold_timer_gate_drop)
        return;
    REG32(RCU + ((uint32_t)peripheral >> 6U)) &=
        ~(UINT32_C(1) << ((uint32_t)peripheral & 31U));
}
extern "C" void
rcu_periph_clock_sleep_enable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + ((uint32_t)peripheral >> 6U)) |=
        UINT32_C(1) << ((uint32_t)peripheral & 31U);
}
extern "C" void
rcu_periph_clock_sleep_disable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + ((uint32_t)peripheral >> 6U)) &=
        ~(UINT32_C(1) << ((uint32_t)peripheral & 31U));
}
/** \brief Reset the external timer register bank before production setup. */
extern "C" void timer_deinit(uint32_t timer) {
    std::memset(reinterpret_cast<void*>((uintptr_t)timer), 0, 0x50U);
}
