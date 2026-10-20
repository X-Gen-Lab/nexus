/**
 * \file            g474_pin_state_test.cpp
 * \brief           Real one-port transitions, retained failures and bounded
 * MMIO
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pin_state_model.h"
#include "stm32g474_pin_state.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
class G474PinState : public testing::Test {
  protected:
    nx_pin_state_plan_t plan{};
    nx_stm32g474_pin_state_t state{};
    nx_pin_state_port_t port{&nx_stm32g474_pin_state_ops, &state};
    static constexpr uint32_t pins = (1U << 5U) | (1U << 6U);
    static inline uint32_t event_write_index;
    static inline nx_stm32g474_pin_state_t* callback_state;
    static constexpr uint32_t pairs = (3U << 10U) | (3U << 12U);
    void SetUp() override {
        nx_pin_state_model_reset();
        plan.pins = pins;
        plan.states[0] = {(1U << 10U) | (1U << 12U), 0U,
                          (2U << 10U) | (1U << 12U), 0U, 1U << 5U};
        plan.states[1] = {pairs, 0U, 0U, 0U, 0U};
        state.gpio = &g_pin_state_model.gpio;
        state.rcc = &g_pin_state_model.rcc;
        state.plan = &plan;
        state.bank_mask = RCC_AHB2ENR_GPIOAEN;
    }
    void TearDown() override {
        if (state.control.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
            if (state.control.retained)
                std::abort();
        }
        EXPECT_FALSE(nx_arch_irq_is_masked());
        EXPECT_EQ(0U, g_pin_state_model.forbidden);
        EXPECT_EQ(0U, g_pin_state_model.rcc_writes);
        EXPECT_EQ(0U, g_pin_state_model.unmasked_writes);
    }
    void Target(unsigned index) {
        g_pin_state_model.expected_output_mask = index == 0U ? pins : 0U;
        g_pin_state_model.expected_levels = plan.states[index].levels;
    }
    nx_result_t Start() {
        Target(0U);
        return nx_stm32g474_pin_state_initialize(&state);
    }
    nx_result_t Select(nx_pin_state_selection_t target) {
        Target(target == NX_PIN_STATE_DEFAULT ? 0U : 1U);
        return nx_pin_state_select(&port, target);
    }
};
TEST_F(G474PinState, ColdDefaultIsProvedBeforeCommit) {
    ASSERT_EQ(NX_SUCCESS, Start());
    EXPECT_TRUE(state.control.initialized);
    EXPECT_TRUE(state.control.retained);
    EXPECT_FALSE(state.control.dirty);
    EXPECT_EQ(0U, state.control.committed);
    EXPECT_EQ(plan.states[0].mode, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_EQ(1U << 5U, g_pin_state_model.gpio.ODR & pins);
    EXPECT_FALSE(g_pin_state_model.unsafe_drive);
    EXPECT_FALSE(g_pin_state_model.unsafe_electrical);
}
TEST_F(G474PinState, SleepWithdrawsBeforeElectricalChanges) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(pairs, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_EQ(0U, g_pin_state_model.gpio.PUPDR & pairs);
    EXPECT_EQ(1U << 5U, g_pin_state_model.gpio.ODR & pins);
    EXPECT_EQ(1U, state.control.committed);
    EXPECT_FALSE(g_pin_state_model.unsafe_electrical);
}
TEST_F(G474PinState, DefaultRestoresLatchBeforeDrive) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    g_pin_state_model.gpio.ODR &= ~pins;
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(plan.states[0].mode, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_EQ(1U << 5U, g_pin_state_model.gpio.ODR & pins);
    EXPECT_FALSE(g_pin_state_model.unsafe_drive);
}
TEST_F(G474PinState, UnselectedPadAndAlternateFieldsArePreserved) {
    auto& gpio = g_pin_state_model.gpio;
    gpio.OTYPER = 1U;
    gpio.PUPDR = 2U;
    gpio.OSPEEDR = 3U;
    gpio.AFR[0] = 0x98765432U;
    gpio.AFR[1] = 0xabcdef01U;
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(1U, gpio.MODER & 3U);
    EXPECT_EQ(1U, gpio.ODR & 1U);
    EXPECT_EQ(1U, gpio.OTYPER & 1U);
    EXPECT_EQ(2U, gpio.PUPDR & 3U);
    EXPECT_EQ(3U, gpio.OSPEEDR & 3U);
    EXPECT_EQ(0x98765432U, gpio.AFR[0]);
    EXPECT_EQ(0xabcdef01U, gpio.AFR[1]);
    EXPECT_FALSE(g_pin_state_model.unselected_change);
}
TEST_F(G474PinState, SameCleanTargetChecksImageWithoutWrites) {
    ASSERT_EQ(NX_SUCCESS, Start());
    nx_pin_state_model_counters();
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_GT(g_pin_state_model.gpio_reads, 0U);
    EXPECT_EQ(0U, g_pin_state_model.writes);
}
TEST_F(G474PinState, CleanForeignImageIsNotSilentlyAdopted) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.gpio.PUPDR ^= 1U << 10U;
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(0U, g_pin_state_model.writes);
    EXPECT_TRUE(state.control.retained);
    EXPECT_TRUE(state.control.dirty);
}
TEST_F(G474PinState, RefusedWithdrawalNeverTouchesLatchOrFields) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.refuse_write = &g_pin_state_model.gpio.MODER;
    uint32_t latch = g_pin_state_model.gpio.ODR;
    uint32_t speed = g_pin_state_model.gpio.OSPEEDR;
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_IO, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(1U, g_pin_state_model.gpio_writes);
    EXPECT_EQ(latch, g_pin_state_model.gpio.ODR);
    EXPECT_EQ(speed, g_pin_state_model.gpio.OSPEEDR);
    EXPECT_TRUE(state.control.retained);
    EXPECT_TRUE(state.control.dirty);
    EXPECT_EQ(0U, state.control.committed);
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
}
TEST_F(G474PinState, DirtyPartialImageCanRetryWithoutMatchingOldCommit) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.refuse_write = &g_pin_state_model.gpio.OSPEEDR;
    EXPECT_EQ(NX_ERROR_IO, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(pairs, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_TRUE(state.control.dirty);
    EXPECT_EQ(0U, state.control.committed);
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_FALSE(state.control.dirty);
    EXPECT_EQ(1U, state.control.committed);
}
TEST_F(G474PinState, FailedInitKeepsNoCommittedDefaultAndCanWithdraw) {
    g_pin_state_model.refuse_write = &g_pin_state_model.gpio.BSRR;
    EXPECT_EQ(NX_ERROR_IO, Start());
    EXPECT_TRUE(state.control.retained);
    EXPECT_TRUE(state.control.dirty);
    EXPECT_FALSE(state.control.initialized);
    EXPECT_EQ(255U, state.control.committed);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
    EXPECT_FALSE(state.control.retained);
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, RefusedLatchCannotEnableOutputAndRetryIsReal) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    g_pin_state_model.gpio.ODR &= ~pins;
    g_pin_state_model.refuse_write = &g_pin_state_model.gpio.BSRR;
    EXPECT_EQ(NX_ERROR_IO, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(pairs, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_TRUE(state.control.dirty);
    EXPECT_FALSE(g_pin_state_model.unsafe_drive);
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
}
TEST_F(G474PinState, ReleasedProviderCannotAutomaticallyReinitialize) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(0U, g_pin_state_model.reads);
    EXPECT_EQ(0U, g_pin_state_model.writes);
}
TEST_F(G474PinState, StopFailureKeepsOwnerForAnActualRetry) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.refuse_write = &g_pin_state_model.gpio.MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_pin_state_stop(&state));
    EXPECT_TRUE(state.control.retained);
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
}
TEST_F(G474PinState, NoGateMeansNoGpioProbeOrWrite) {
    g_pin_state_model.rcc.AHB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, Start());
    EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
    EXPECT_FALSE(state.control.retained);
}
TEST_F(G474PinState, HeldResetMeansNoGpioProbeOrWrite) {
    g_pin_state_model.rcc.AHB2RSTR = RCC_AHB2RSTR_GPIOARST;
    EXPECT_EQ(NX_ERROR_STATE, Start());
    EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
}
TEST_F(G474PinState, RuntimeGateLossRetainsWithoutBlockedAccess) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.rcc.AHB2ENR = 0U;
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_SLEEP));
    EXPECT_TRUE(state.control.retained);
    EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
    g_pin_state_model.rcc.AHB2ENR = RCC_AHB2ENR_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
}
TEST_F(G474PinState, GateLossAfterWithdrawalStopsAllFurtherGpioAccess) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.after_write = [](volatile uint32_t* reg, uint32_t) {
        if (reg == &g_pin_state_model.gpio.MODER) {
            g_pin_state_model.after_write = nullptr;
            g_pin_state_model.rcc.AHB2ENR = 0U;
        }
    };
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(1U, g_pin_state_model.gpio_writes);
    EXPECT_EQ(0U, g_pin_state_model.forbidden);
    EXPECT_TRUE(state.control.retained);
    EXPECT_TRUE(state.control.dirty);
    g_pin_state_model.rcc.AHB2ENR = RCC_AHB2ENR_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
}
TEST_F(G474PinState, LiveLockRefusesWithoutChangingFields) {
    ASSERT_EQ(NX_SUCCESS, Start());
    g_pin_state_model.gpio.LCKR = pins;
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_BUSY, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(0U, g_pin_state_model.writes);
    g_pin_state_model.gpio.LCKR = 0U;
}
TEST_F(G474PinState, ForeignColdOutputIsNotAResetLikeLease) {
    g_pin_state_model.gpio.MODER |= 1U << 10U;
    EXPECT_EQ(NX_ERROR_BUSY, Start());
    EXPECT_EQ(0U, g_pin_state_model.writes);
    EXPECT_FALSE(state.control.retained);
}
TEST_F(G474PinState, ContextRefusalsPreserveIncomingMask) {
    ASSERT_EQ(NX_SUCCESS, Start());
    nx_pin_state_model_counters();
    g_pin_state_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, Select(NX_PIN_STATE_SLEEP));
    g_pin_state_model.privileged = true;
    g_pin_state_model.isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, Select(NX_PIN_STATE_SLEEP));
    g_pin_state_model.isr = false;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT, Select(NX_PIN_STATE_SLEEP));
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_EQ(0U, g_pin_state_model.reads);
    EXPECT_EQ(0U, g_pin_state_model.writes);
}
TEST_F(G474PinState, RetainedInitializationIsBusyBeforeHardwareProbe) {
    ASSERT_EQ(NX_SUCCESS, Start());
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_BUSY, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, ZeroOrOversizeMaskIsRejectedBeforeMmio) {
    plan.pins = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    plan.pins = 0x10000U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, AlternateModeIsUnsupportedBeforeMmio) {
    plan.states[1].mode = 2U << 10U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, ReservedPullAndUnownedFieldsAreInvalid) {
    plan.states[1].pull = 3U << 10U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    plan.states[1].pull = 0U;
    plan.states[1].speed = 1U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, AnalogTargetCannotInventAnOutputLatch) {
    plan.states[1].levels = 1U << 5U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, PlanCannotAliasMutableProviderStorage) {
    state.plan = reinterpret_cast<const nx_pin_state_plan_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_pin_state_initialize(&state));
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, WrappingPlanIsRejectedWithoutDereference) {
    state.plan = reinterpret_cast<const nx_pin_state_plan_t*>(
        std::numeric_limits<uintptr_t>::max() - 3U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_pin_state_initialize(&state));
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, WrongBankMaskIsRejectedBeforeMmio) {
    state.bank_mask = 3U;
    EXPECT_EQ(NX_ERROR_INVALID, Start());
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, HealthyTransitionHasAnObservedFiniteBudget) {
    ASSERT_EQ(NX_SUCCESS, Start());
    nx_pin_state_model_counters();
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_LE(g_pin_state_model.reads, 72U);
    EXPECT_LE(g_pin_state_model.writes, 8U);
    EXPECT_GT(g_pin_state_model.gpio_reads, 0U);
    EXPECT_GT(g_pin_state_model.gpio_writes, 0U);
}

TEST_F(G474PinState, SameCleanTargetObservesFinalBankLoss) {
    ASSERT_EQ(NX_SUCCESS, Start());
    for (bool reset : {false, true}) {
        g_pin_state_model.after_read = [](volatile uint32_t* reg) {
            if (reg == &g_pin_state_model.gpio.ODR) {
                g_pin_state_model.after_read = nullptr;
                g_pin_state_model.rcc.AHB2ENR = 0U;
            }
        };
        if (reset)
            g_pin_state_model.after_read = [](volatile uint32_t* reg) {
                if (reg == &g_pin_state_model.gpio.ODR) {
                    g_pin_state_model.after_read = nullptr;
                    g_pin_state_model.rcc.AHB2RSTR = RCC_AHB2RSTR_GPIOARST;
                }
            };
        nx_pin_state_model_counters();
        EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_DEFAULT));
        EXPECT_EQ(0U, g_pin_state_model.writes);
        EXPECT_TRUE(state.control.retained);
        EXPECT_EQ(0U, g_pin_state_model.forbidden);
        EXPECT_EQ(nullptr, g_pin_state_model.after_read);
        g_pin_state_model.rcc.AHB2ENR = RCC_AHB2ENR_GPIOAEN;
        g_pin_state_model.rcc.AHB2RSTR = 0U;
        ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
    }
}

TEST_F(G474PinState,
       EachElectricalReadbackFailureCanRetryFromItsOwnPartialImage) {
    plan.states[0].output_type = 1U << 5U;
    plan.states[0].pull = 1U << 10U;
    volatile uint32_t* fields[] = {&g_pin_state_model.gpio.OTYPER,
                                   &g_pin_state_model.gpio.OSPEEDR,
                                   &g_pin_state_model.gpio.PUPDR};
    for (auto* field : fields) {
        ASSERT_EQ(NX_SUCCESS, Start());
        g_pin_state_model.refuse_write = field;
        nx_pin_state_model_counters();
        EXPECT_EQ(NX_ERROR_IO, Select(NX_PIN_STATE_SLEEP));
        EXPECT_TRUE(state.control.retained);
        EXPECT_TRUE(state.control.dirty);
        EXPECT_EQ(0U, state.control.committed);
        EXPECT_EQ(pairs, g_pin_state_model.gpio.MODER & pairs);
        EXPECT_FALSE(g_pin_state_model.unsafe_electrical);
        g_pin_state_model.refuse_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
        EXPECT_EQ(1U, state.control.committed);
        EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
    }
}
TEST_F(G474PinState,
       EachAcceptedWriteBoundaryDetectsGateLossWithoutBlockedProbe) {
    for (uint32_t write = 1U; write <= 5U; ++write) {
        ASSERT_EQ(NX_SUCCESS, Start());
        event_write_index = write;
        g_pin_state_model.after_write = [](volatile uint32_t*, uint32_t) {
            if (g_pin_state_model.gpio_writes == event_write_index) {
                g_pin_state_model.after_write = nullptr;
                g_pin_state_model.rcc.AHB2ENR = 0U;
            }
        };
        nx_pin_state_model_counters();
        /* Sleep enables no output; this field is the independent write index.
         */
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_pin_state_select(&port, NX_PIN_STATE_SLEEP));
        EXPECT_EQ(write, g_pin_state_model.gpio_writes);
        EXPECT_EQ(nullptr, g_pin_state_model.after_write);
        EXPECT_EQ(0U, g_pin_state_model.forbidden);
        EXPECT_TRUE(state.control.retained);
        EXPECT_TRUE(state.control.dirty);
        g_pin_state_model.rcc.AHB2ENR = RCC_AHB2ENR_GPIOAEN;
        EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
        EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
    }
}
TEST_F(G474PinState, ReleasedSelectRefusesBeforeEvenTheClockProbe) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_pin_state_stop(&state));
    g_pin_state_model.rcc.AHB2ENR = 0U;
    nx_pin_state_model_counters();
    EXPECT_EQ(NX_ERROR_STATE, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(0U, g_pin_state_model.reads);
    EXPECT_EQ(0U, g_pin_state_model.writes);
}
TEST_F(G474PinState, PlanCannotAliasTheSelectedMmioBank) {
    state.plan = reinterpret_cast<const nx_pin_state_plan_t*>(state.gpio);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_pin_state_initialize(&state));
    EXPECT_EQ(0U, g_pin_state_model.reads);
}
TEST_F(G474PinState, BothTransitionDirectionsHaveMeasuredFiniteMmio) {
    ASSERT_EQ(NX_SUCCESS, Start());
    nx_pin_state_model_counters();
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    RecordProperty("sleep_reads", static_cast<int>(g_pin_state_model.reads));
    RecordProperty("sleep_writes", static_cast<int>(g_pin_state_model.writes));
    EXPECT_LE(g_pin_state_model.reads, 72U);
    EXPECT_LE(g_pin_state_model.writes, 8U);
    nx_pin_state_model_counters();
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
    RecordProperty("default_reads", static_cast<int>(g_pin_state_model.reads));
    RecordProperty("default_writes",
                   static_cast<int>(g_pin_state_model.writes));
    EXPECT_LE(g_pin_state_model.reads, 72U);
    EXPECT_LE(g_pin_state_model.writes, 8U);
    EXPECT_FALSE(g_pin_state_model.unsafe_electrical);
    EXPECT_FALSE(g_pin_state_model.unsafe_drive);
}

TEST_F(G474PinState, ExplicitInputAndAnalogMixDoesNotWriteAnOutputLatch) {
    plan.states[1].mode = 3U << 12U;
    plan.states[1].pull = 1U << 10U;
    ASSERT_EQ(NX_SUCCESS, Start());
    uint32_t latch = g_pin_state_model.gpio.ODR;
    nx_pin_state_model_counters();
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(plan.states[1].mode, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_EQ(plan.states[1].pull, g_pin_state_model.gpio.PUPDR & pairs);
    EXPECT_EQ(latch, g_pin_state_model.gpio.ODR);
    EXPECT_EQ(5U, g_pin_state_model.gpio_writes);
    EXPECT_FALSE(g_pin_state_model.unsafe_electrical);
    EXPECT_FALSE(g_pin_state_model.unsafe_drive);
}
TEST_F(G474PinState, FinalDriveRefusalKeepsWithdrawalAndRequiresRealRetry) {
    ASSERT_EQ(NX_SUCCESS, Start());
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    g_pin_state_model.after_write = [](volatile uint32_t* reg, uint32_t) {
        if (reg == &g_pin_state_model.gpio.PUPDR) {
            g_pin_state_model.after_write = nullptr;
            g_pin_state_model.refuse_write = &g_pin_state_model.gpio.MODER;
        }
    };
    EXPECT_EQ(NX_ERROR_IO, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(pairs, g_pin_state_model.gpio.MODER & pairs);
    EXPECT_TRUE(state.control.retained);
    EXPECT_TRUE(state.control.dirty);
    EXPECT_EQ(1U, state.control.committed);
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(0U, state.control.committed);
}
TEST_F(G474PinState, NestedTaskEntriesCannotBreakTheSavedMask) {
    ASSERT_EQ(NX_SUCCESS, Start());
    callback_state = &state;
    g_pin_state_model.after_write = [](volatile uint32_t*, uint32_t) {
        g_pin_state_model.after_write = nullptr;
        uint32_t reads = g_pin_state_model.reads;
        uint32_t writes = g_pin_state_model.writes;
        const nx_pin_state_port_t nested{&nx_stm32g474_pin_state_ops,
                                         callback_state};
        EXPECT_TRUE(nx_arch_irq_is_masked());
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_pin_state_select(&nested, NX_PIN_STATE_DEFAULT));
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32g474_pin_state_initialize(callback_state));
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32g474_pin_state_stop(callback_state));
        EXPECT_TRUE(nx_arch_irq_is_masked());
        EXPECT_EQ(reads, g_pin_state_model.reads);
        EXPECT_EQ(writes, g_pin_state_model.writes);
    };
    ASSERT_EQ(NX_SUCCESS, Select(NX_PIN_STATE_SLEEP));
    EXPECT_EQ(nullptr, g_pin_state_model.after_write);
    EXPECT_FALSE(nx_arch_irq_is_masked());
    callback_state = nullptr;
}
