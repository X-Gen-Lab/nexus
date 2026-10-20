/**
 * \file            u575_lptim_test.cpp
 * \brief           Real U575 RUN counter synchronization and loan withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_lptim.h"
#include "u575_lptim_model.h"
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
static nx_stm32u575_lptim_state_t* s_irq_owner;
class U575Lptim : public testing::Test {
  protected:
    nx_stm32u575_lptim_state_t state{};
    const nx_lptim_port_t port{&nx_stm32u575_lptim_ops, &state};
    nx_lptim_request_t request{};
    void SetUp() override {
        nx_u575_lptim_model_reset();
        state.registers = &g_u575_lptim_model.registers;
        nx_request_initialize(&request.base);
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lptim_initialize(&state));
    }
    void Prepare(uint32_t ticks = 10U,
                 nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        nx_lptim_sample_t sample{};
        ASSERT_EQ(NX_SUCCESS, nx_lptim_port_sample(&port, &sample));
        ASSERT_EQ(NX_SUCCESS, nx_lptim_request_prepare(&request, &sample.domain,
                                                       ticks, deadline));
    }
    void Pump(unsigned count = 200U) {
        for (unsigned n = 0U; n < count && state.active != nullptr; ++n) {
            nx_u575_lptim_model_step(1U);
            (void)nx_lptim_port_service(&port);
        }
    }
    void Launch() {
        for (unsigned n = 0U; n < 80U && g_u575_lptim_model.starts == 0U; ++n) {
            nx_u575_lptim_model_step(1U);
            (void)nx_lptim_port_service(&port);
        }
        ASSERT_EQ(1U, g_u575_lptim_model.starts);
    }
    void TearDown() override {
        g_u575_lptim_model.hold_arr = g_u575_lptim_model.hold_cmp = false;
        g_u575_lptim_model.hold_dier = g_u575_lptim_model.hold_disable = false;
        g_u575_lptim_model.hold_reset = g_u575_lptim_model.hold_release = false;
        g_u575_lptim_model.hold_pending = g_u575_lptim_model.hold_gate = false;
        if (state.active != nullptr) {
            (void)nx_lptim_port_cancel(&port, &request);
            Pump();
        }
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_lptim_port_stop(&port));
        }
        EXPECT_EQ(0U, g_u575_lptim_model.bad_access);
    }
};
TEST_F(U575Lptim, ColdOpenAndIdleSnapshotUseDistinctLiveDomain) {
    Open();
    nx_lptim_sample_t sample{};
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_sample(&port, &sample));
    EXPECT_EQ(&state, sample.domain.identity);
    EXPECT_NE(0U, sample.domain.generation);
    EXPECT_EQ(1000000U, sample.tick_hz);
    EXPECT_EQ(0U, sample.counter);
    EXPECT_FALSE(sample.running);
}
TEST_F(U575Lptim, OneTickRejectsWithoutBorrowOrRegisterWrite) {
    Open();
    Prepare(1U);
    auto writes = g_u575_lptim_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_lptim_port_submit(&port, &request));
    EXPECT_EQ(writes, g_u575_lptim_model.writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(U575Lptim, MinimumShotNeedsOverflowNotEarlierArrMatch) {
    Open();
    Prepare(2U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    nx_u575_lptim_model_step(1U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(U575Lptim, FullSixteenBitShotWrapsToZeroButReports65536Ticks) {
    Open();
    Prepare(65536U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    nx_u575_lptim_model_step(65535U);
    EXPECT_EQ(65535U, g_u575_lptim_model.registers.CNT);
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    nx_u575_lptim_model_step(1U);
    EXPECT_EQ(0U, g_u575_lptim_model.registers.CNT);
    g_u575_lptim_model.exception = 84U;
    nx_stm32u575_lptim_irq(&state);
    g_u575_lptim_model.exception = 0U;
    EXPECT_TRUE(state.event);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Pump();
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(65536U, request.base.transferred);
    EXPECT_FALSE(g_u575_model.irq_enabled[68]);
    EXPECT_FALSE(g_u575_model.irq_pending[68]);
}
TEST_F(U575Lptim, ExpiredAdmissionHasNoSourceEffects) {
    Open();
    Prepare(10U, 1U);
    nx_u575_lptim_model_step(2U);
    auto writes = g_u575_lptim_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_lptim_port_submit(&port, &request));
    EXPECT_EQ(writes, g_u575_lptim_model.writes);
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(U575Lptim, CancelBeforeStartDoesNotLaunchOrReleaseBeforeReset) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    g_u575_lptim_model.hold_reset = true;
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_cancel(&port, &request));
    Pump();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(0U, g_u575_lptim_model.starts);
    g_u575_lptim_model.hold_reset = false;
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(U575Lptim, HeldDisableRetainsLoanAndFinalProgressKeepsAdvancing) {
    Open();
    Prepare(1000U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    nx_u575_lptim_model_step(7U);
    g_u575_lptim_model.hold_disable = true;
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_cancel(&port, &request));
    Pump(5U);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_u575_lptim_model.hold_disable = false;
    Pump();
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_GE(request.base.transferred, 12U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(U575Lptim, RefusedSynchronizationDrainsInsteadOfStarting) {
    for (unsigned which = 0U; which < 3U; ++which) {
        Open();
        Prepare();
        g_u575_lptim_model.hold_arr = which == 0U;
        g_u575_lptim_model.hold_cmp = which == 1U;
        g_u575_lptim_model.hold_dier = which == 2U;
        ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
        Pump();
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_ERROR_IO, request.base.result);
        EXPECT_EQ(0U, g_u575_lptim_model.starts);
        ASSERT_EQ(NX_SUCCESS, nx_lptim_port_stop(&port));
        g_u575_lptim_model.hold_arr = g_u575_lptim_model.hold_cmp = false;
        g_u575_lptim_model.hold_dier = false;
    }
}
TEST_F(U575Lptim, PendingWithdrawalFailureKeepsRequestUntilRetry) {
    Open();
    Prepare(2U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    g_u575_lptim_model.hold_pending = true;
    Pump();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    g_u575_lptim_model.hold_pending = false;
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(U575Lptim, StaleDomainAndExhaustedGenerationNeverBorrow) {
    Open();
    Prepare();
    ++state.generation;
    EXPECT_EQ(NX_ERROR_STATE, nx_lptim_port_submit(&port, &request));
    state.generation = UINT64_MAX;
    Prepare();
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_lptim_port_submit(&port, &request));
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(U575Lptim, ForeignVectorAndKernelSelectorRejectBeforeClockEffects) {
    g_u575_model.irq_pending[68] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lptim_initialize(&state));
    EXPECT_EQ(0U, g_u575_lptim_model.writes);
    g_u575_model.irq_pending[68] = false;
    g_u575_model.rcc.CCIPR1 |= RCC_CCIPR1_LPTIM2SEL_0;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lptim_initialize(&state));
    EXPECT_EQ(0U, g_u575_lptim_model.writes);
}
TEST_F(U575Lptim, ClosingJoinsRequestAndPreservesNeighborAndPreexistingGate) {
    g_u575_model.rcc.APB1ENR2 = RCC_APB1ENR2_LPTIM2EN | RCC_APB1ENR2_I2C4EN;
    g_u575_model.priority[68] = 9U;
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_stop(&port));
    Pump();
    EXPECT_EQ(NX_SUCCESS, nx_lptim_port_stop(&port));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(RCC_APB1ENR2_LPTIM2EN | RCC_APB1ENR2_I2C4EN,
              g_u575_model.rcc.APB1ENR2);
    EXPECT_EQ(9U, g_u575_model.priority[68]);
    EXPECT_EQ(NX_SUCCESS, nx_lptim_port_stop(&port));
}
TEST_F(U575Lptim, WrongVectorCannotPublishAnEventOrMaskTheOwnedVector) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    g_u575_lptim_model.registers.ISR |= LPTIM_ISR_UE;
    g_u575_lptim_model.exception = 83U;
    nx_stm32u575_lptim_irq(&state);
    g_u575_lptim_model.exception = 0U;
    EXPECT_FALSE(state.event);
    EXPECT_TRUE(g_u575_model.irq_enabled[68]);
}
TEST_F(U575Lptim, ResetReleaseFailureDoesNotPermitPeripheralSampling) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_cancel(&port, &request));
    g_u575_lptim_model.hold_release = true;
    Pump();
    EXPECT_TRUE(state.reset_asserted);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    nx_lptim_sample_t sample{};
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_sample(&port, &sample));
    g_u575_lptim_model.hold_release = false;
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(U575Lptim,
       ClockLossObservedBySampleCannotBeRepairedByRestoringSelector) {
    for (unsigned source = 0U; source < 4U; ++source) {
        EXPECT_EXIT(
            {
                if (nx_stm32u575_lptim_initialize(&state) != NX_SUCCESS)
                    _exit(1);
                nx_lptim_sample_t sample{};
                if (nx_lptim_port_sample(&port, &sample) != NX_SUCCESS ||
                    nx_lptim_request_prepare(&request, &sample.domain, 10U,
                                             NX_DEADLINE_NEVER) != NX_SUCCESS ||
                    nx_lptim_port_submit(&port, &request) != NX_SUCCESS)
                    _exit(2);
                uint32_t cr = g_u575_model.rcc.CR;
                uint32_t csr = g_u575_model.rcc.CSR;
                if (source == 0U)
                    g_u575_model.rcc.CCIPR1 |= RCC_CCIPR1_LPTIM2SEL_0;
                else if (source == 1U)
                    g_u575_model.rcc.CFGR1 = RCC_CFGR1_SW_0;
                else if (source == 2U)
                    g_u575_model.rcc.CR &=
                        ~static_cast<uint32_t>(RCC_CR_MSISRDY);
                else
                    g_u575_model.rcc.CSR = 5U << RCC_CSR_MSISSRANGE_Pos;
                if (nx_lptim_port_sample(&port, &sample) != NX_ERROR_STATE)
                    _exit(3);
                g_u575_model.rcc.CCIPR1 = 0U;
                g_u575_model.rcc.CFGR1 = 0U;
                g_u575_model.rcc.CR = cr;
                g_u575_model.rcc.CSR = csr;
                Pump();
                _exit(state.tainted && state.active == &request &&
                              nx_request_state(&request.base) ==
                                  NX_REQUEST_QUARANTINED
                          ? 0
                          : 4);
            },
            testing::ExitedWithCode(0), "");
    }
}
TEST_F(U575Lptim, UnstableCounterRejectsBoundedlyWithoutChangingSnapshot) {
    Open();
    Prepare(1000U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    nx_lptim_sample_t sample{};
    sample.tick_hz = 123U;
    g_u575_lptim_model.unstable_counter = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_sample(&port, &sample));
    EXPECT_EQ(123U, sample.tick_hz);
    g_u575_lptim_model.unstable_counter = false;
}
TEST_F(U575Lptim, SettlementIsFinalAccessToCallerRequestPage) {
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, page);
    auto* borrowed = static_cast<nx_lptim_request_t*>(page);
    nx_request_initialize(&borrowed->base);
    Open();
    nx_lptim_sample_t sample{};
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_sample(&port, &sample));
    ASSERT_EQ(NX_SUCCESS, nx_lptim_request_prepare(borrowed, &sample.domain, 2U,
                                                   NX_DEADLINE_NEVER));
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, borrowed));
    Pump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&borrowed->base));
    ASSERT_EQ(0, mprotect(page, 4096U, PROT_NONE));
    EXPECT_EQ(NX_SUCCESS, nx_lptim_port_service(&port));
    EXPECT_EQ(NX_ERROR_STATE, nx_lptim_port_cancel(&port, borrowed));
    EXPECT_EQ(NX_SUCCESS, nx_lptim_port_stop(&port));
    EXPECT_EQ(0, munmap(page, 4096U));
}
TEST_F(U575Lptim, ReservedTimebaseCannotBecomeWritableRequestOrSnapshot) {
    Open();
    auto* storage = &g_u575_model.timer;
    auto* alias_request = reinterpret_cast<nx_lptim_request_t*>(storage);
    auto* alias_sample = reinterpret_cast<nx_lptim_sample_t*>(storage);
    EXPECT_EQ(NX_ERROR_INVALID, nx_lptim_port_submit(&port, alias_request));
    EXPECT_EQ(NX_ERROR_INVALID, nx_lptim_port_sample(&port, alias_sample));
    EXPECT_EQ(0U, storage->CR1 | storage->PSC | storage->CNT);
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(U575Lptim,
       ExpiryWinsBeforeUnobservedCompletionAndPreservesFinalProgress) {
    Open();
    Prepare(2U, 100000U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    nx_u575_lptim_model_step(2U);
    g_u575_lptim_model.now = 100000U;
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
    EXPECT_EQ(2U, request.base.transferred);
}
TEST_F(U575Lptim, EnableGuardIncludesUnknownCoreSnapshotPhase) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    /* A Core quantum boundary can precede the first full counter cycle.
     * Advance the sampled clock once without granting a hardware cycle. */
    ++g_u575_lptim_model.now;
    nx_u575_lptim_model_step(1U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    EXPECT_EQ(1U, g_u575_lptim_model.registers.ARR);
    EXPECT_EQ(0U, g_u575_lptim_model.bad_access);
    nx_u575_lptim_model_step(1U);
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(U575Lptim, EnableGuardStartsAfterActualWriteDespiteEarlierCpuWork) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    g_u575_lptim_model.before_enable_us = 4U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    EXPECT_EQ(4U, g_u575_lptim_model.now);
    for (unsigned n = 0U; n < 8U; ++n)
        (void)nx_lptim_port_service(&port);
    EXPECT_EQ(1U, g_u575_lptim_model.registers.ARR);
    EXPECT_EQ(0U, g_u575_lptim_model.bad_access);
    Pump();
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(U575Lptim, WithdrawalGuardStartsAfterDisableReadback) {
    Open();
    Prepare(1000U);
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    Launch();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_cancel(&port, &request));
    g_u575_lptim_model.before_disable_us = 4U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    auto reads = g_u575_lptim_model.counter_reads;
    for (unsigned n = 0U; n < 8U; ++n)
        (void)nx_lptim_port_service(&port);
    EXPECT_EQ(reads, g_u575_lptim_model.counter_reads);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_GE(request.base.transferred, 4U);
}
TEST_F(U575Lptim, LaunchSupervisionStartsAfterActualStartCommand) {
    Open();
    Prepare();
    ASSERT_EQ(NX_SUCCESS, nx_lptim_port_submit(&port, &request));
    g_u575_lptim_model.before_start_us = 40U;
    Launch();
    EXPECT_EQ(NX_ERROR_BUSY, nx_lptim_port_service(&port));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(10U, request.base.transferred);
}
TEST_F(U575Lptim, IrqFactsDeferRequestPublicationAndWinBeforeTaskCancellation) {
    for (unsigned boundary = 0U; boundary < 2U; ++boundary) {
        EXPECT_EXIT(
            {
                if (nx_stm32u575_lptim_initialize(&state) != NX_SUCCESS)
                    _exit(1);
                nx_lptim_sample_t sample{};
                if (nx_lptim_port_sample(&port, &sample) != NX_SUCCESS ||
                    nx_lptim_request_prepare(&request, &sample.domain, 1000U,
                                             NX_DEADLINE_NEVER) != NX_SUCCESS ||
                    nx_lptim_port_submit(&port, &request) != NX_SUCCESS)
                    _exit(2);
                Launch();
                s_irq_owner = &state;
                auto interrupt = []() {
                    g_u575_model.before_irq_save = nullptr;
                    g_u575_model.rcc.CCIPR1 |= RCC_CCIPR1_LPTIM2SEL_0;
                    g_u575_lptim_model.exception = 84U;
                    nx_stm32u575_lptim_irq(s_irq_owner);
                    g_u575_lptim_model.exception = 0U;
                    g_u575_model.rcc.CCIPR1 = 0U;
                };
                if (boundary == 0U) {
                    interrupt();
                    if (nx_request_state(&request.base) != NX_REQUEST_ACTIVE)
                        _exit(3);
                } else {
                    g_u575_model.before_irq_save = interrupt;
                }
                if (nx_lptim_port_cancel(&port, &request) != NX_SUCCESS ||
                    nx_arch_irq_is_masked())
                    _exit(4);
                (void)nx_lptim_port_service(&port);
                _exit(state.tainted && state.active == &request &&
                              state.terminal == NX_ERROR_STATE &&
                              nx_request_state(&request.base) ==
                                  NX_REQUEST_QUARANTINED
                          ? 0
                          : 5);
            },
            testing::ExitedWithCode(0), "");
    }
}
