/**
 * \file            usb_clock_test.cpp
 * \brief           Actual USB clock lease, readback and bounded rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_usb_clock.h"
#include "stm32f407xx.h"
#include "usb_device_controller.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static nx_time_us_t s_now;
static bool s_isr;
static bool s_privileged;
static bool s_refuse_gate;
static bool s_refuse_stop;
static bool s_refuse_mux;
static bool s_pll_ready;
static bool s_hold_ready;
static bool s_break_source;
static unsigned s_writes;

/** \brief Advance the single model clock domain at actual deadline reads. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return s_now++;
}
/** \brief Inject the external CPU context boundary, not clock behavior. */
extern "C" bool nx_arch_in_isr(void) {
    return s_isr;
}
/** \brief Inject raw unprivileged calls rejected before clock MMIO. */
extern "C" bool nx_arch_is_privileged(void) {
    return s_privileged;
}
/** \brief Single-thread MMIO model; does not establish physical bus timing. */
extern "C" void nx_arch_dsb(void) {
}

using ClockState = nx_stm32_usb_clock_state_t;
#define CLOCK_OPS      nx_stm32_usb_clock_ops
#define CLOCK_START    nx_stm32_usb_clock_start
#define CLOCK_STOP     nx_stm32_usb_clock_stop
#define CLOCK_GATE     RCC->AHB2ENR
#define CLOCK_GATE_BIT RCC_AHB2ENR_OTGFSEN
#define CLOCK_PLL      RCC->PLLCFGR
#define CLOCK_CONTROL  RCC->CR

/** \brief Model readiness as hardware fact rather than driver-written status.
 */
extern "C" void nx_usb_clock_model_poll(void) {
}

/** \brief Refused gates and shared muxes remain visible in real readbacks. */
extern "C" void nx_usb_clock_model_write(volatile uint32_t* address,
                                         uint32_t value) {
    ++s_writes;
    if (address == &CLOCK_GATE &&
        ((s_refuse_gate && (value & CLOCK_GATE_BIT) != 0U) ||
         (s_refuse_stop && (value & CLOCK_GATE_BIT) == 0U))) {
        return;
    }
    *address = value;
    nx_usb_clock_model_poll();
    if (s_break_source && address == &CLOCK_GATE &&
        (value & CLOCK_GATE_BIT) != 0U) {
        CLOCK_CONTROL &= ~static_cast<uint32_t>(RCC_CR_HSERDY);
    }
}

class UsbClock : public ::testing::Test {
  protected:
    ClockState state{};
    void SetUp() override {
        void* mapped = mmap(reinterpret_cast<void*>(0x40023000U), 8192U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x40023000U));
        std::memset(mapped, 0, 8192U);
        s_now = 10U;
        s_isr = false;
        s_privileged = true;
        s_refuse_gate = false;
        s_refuse_stop = false;
        s_refuse_mux = false;
        s_pll_ready = true;
        s_hold_ready = false;
        s_break_source = false;
        s_writes = 0U;
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV2;
    }
};

TEST_F(UsbClock, MissingAuthorityRejectsBeforeAnyWrite) {
    EXPECT_EQ(CLOCK_START(&state, false, 100U), NX_ERROR_PERMISSION);
    EXPECT_EQ(s_writes, 0U);
    EXPECT_EQ(CLOCK_GATE, 0U);
}

TEST_F(UsbClock, ColdContextAndFiniteDeadlineAreRequired) {
    s_isr = true;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_CONTEXT);
    s_isr = false;
    s_privileged = false;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_CONTEXT);
    s_privileged = true;
    EXPECT_EQ(CLOCK_START(&state, true, NX_DEADLINE_NEVER), NX_ERROR_INVALID);
    EXPECT_EQ(CLOCK_START(&state, true, 10U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbClock, UnexpectedPllProfileRejectsWithoutReclocking) {
    CLOCK_PLL ^= 1U;
    uint32_t before = CLOCK_PLL;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(CLOCK_PLL, before);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbClock, ExistingUsbGateIsAnExternalOwner) {
    CLOCK_GATE = CLOCK_GATE_BIT;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbClock, Actual48ClockAndGateOutliveRetainedController) {
    uint32_t main_pll = CLOCK_PLL;
    ASSERT_EQ(CLOCK_START(&state, true, 100U), NX_SUCCESS);
    EXPECT_EQ(CLOCK_PLL, main_pll);
    ASSERT_EQ(CLOCK_OPS.validate(&state), NX_SUCCESS);
    ASSERT_EQ(CLOCK_OPS.retain(&state), NX_SUCCESS);
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_ERROR_BUSY);
    EXPECT_NE(CLOCK_GATE & CLOCK_GATE_BIT, 0U);
    ASSERT_EQ(CLOCK_OPS.release(&state), NX_SUCCESS);
    ASSERT_EQ(CLOCK_STOP(&state, 200U), NX_SUCCESS);
    EXPECT_EQ(CLOCK_GATE & CLOCK_GATE_BIT, 0U);
    EXPECT_EQ(CLOCK_OPS.validate(&state), NX_ERROR_STATE);
    EXPECT_EQ(CLOCK_OPS.release(&state), NX_ERROR_STATE);
}

TEST_F(UsbClock, OverflowAndDoubleReleaseDoNotMutateReferences) {
    ASSERT_EQ(CLOCK_START(&state, true, 100U), NX_SUCCESS);
    state.consumers = UINT32_MAX;
    EXPECT_EQ(CLOCK_OPS.retain(&state), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(state.consumers, UINT32_MAX);
    state.consumers = 0U;
    EXPECT_EQ(CLOCK_OPS.release(&state), NX_ERROR_STATE);
    EXPECT_EQ(state.consumers, 0U);
}

TEST_F(UsbClock, GateReadbackFailureRollsBackOnlyOwnedClockBits) {
    CLOCK_GATE = 1U;
    s_refuse_gate = true;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_IO);
    EXPECT_EQ(CLOCK_GATE, 1U);
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_SUCCESS);
}

TEST_F(UsbClock, RefusedStopRetainsCleanupResponsibility) {
    ASSERT_EQ(CLOCK_START(&state, true, 100U), NX_SUCCESS);
    s_refuse_stop = true;
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_ERROR_IO);
    EXPECT_NE(CLOCK_GATE & CLOCK_GATE_BIT, 0U);
    EXPECT_EQ(CLOCK_OPS.retain(&state), NX_ERROR_STATE);
    EXPECT_EQ(CLOCK_START(&state, true, 200U), NX_ERROR_BUSY);
    s_refuse_stop = false;
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_SUCCESS);
}

TEST_F(UsbClock, ChangedSourceFailsReadonlyValidationAndAdmission) {
    ASSERT_EQ(CLOCK_START(&state, true, 100U), NX_SUCCESS);
    CLOCK_PLL ^= 1U;
    unsigned before = s_writes;
    EXPECT_EQ(CLOCK_OPS.validate(&state), NX_ERROR_IO);
    EXPECT_EQ(CLOCK_OPS.retain(&state), NX_ERROR_IO);
    EXPECT_EQ(state.consumers, 0U);
    EXPECT_EQ(s_writes, before);
}

TEST_F(UsbClock, LostSourceAtGateBoundaryCannotPublishReadyOwner) {
    s_break_source = true;
    EXPECT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_IO);
    EXPECT_EQ(CLOCK_GATE & CLOCK_GATE_BIT, 0U);
    EXPECT_EQ(CLOCK_OPS.retain(&state), NX_ERROR_STATE);
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_SUCCESS);
}

TEST_F(UsbClock, FailedAcquisitionDrainCannotBecomeReadyAfterSourceRecovery) {
    s_break_source = true;
    s_refuse_stop = true;
    ASSERT_EQ(CLOCK_START(&state, true, 100U), NX_ERROR_IO);
    CLOCK_CONTROL |= RCC_CR_HSERDY;
    EXPECT_EQ(CLOCK_OPS.retain(&state), NX_ERROR_STATE);
    EXPECT_EQ(state.consumers, 0U);
    s_refuse_stop = false;
    EXPECT_EQ(CLOCK_STOP(&state, 200U), NX_SUCCESS);
}
