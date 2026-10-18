/**
 * \file            h743_runtime_clock_test.cpp
 * \brief           Real SPI lifecycle lease and monotonic runtime pause
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "h743_runtime_clock_model.h"
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_spi_dma.h"
#include <array>
#include <cstdlib>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
extern "C" {
void DMA1_Stream2_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);
void SPI4_IRQHandler(void);
}
class H743RuntimeClock : public testing::Test {
  protected:
    uint8_t* memory = nullptr;
    nx_spi_request_t request{};
    const nx_spi_port_t* Port() {
        return nx_factory_spi(NX_SPI_ID_BUS4);
    }
    const nx_spi_endpoint_t* Device() {
        return nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE4);
    }
    nx_stm32h743_spi_dma_state_t* Owner() {
        return static_cast<nx_stm32h743_spi_dma_state_t*>(Port()->context);
    }
    nx_clock_domain_t* Domain() {
        return nx_platform_runtime_domain();
    }
    void SetUp() override {
        nx_h743_spi_dma_factory_reset();
        nx_runtime_watch = false;
        nx_runtime_hold_resume_cleanup = false;
        nx_runtime_unleased_writes = 0U;
        void* raw =
            mmap(reinterpret_cast<void*>(0x24000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, raw);
        memory = static_cast<uint8_t*>(raw);
        new (memory) std::array<uint8_t, 0x80000U>{};
        nx_request_initialize(&request.base);
        for (unsigned n = 0; n < 32U; ++n)
            memory[n] = static_cast<uint8_t>(n + 3U);
        g_h743_model.spi_watch_pins = true;
    }
    void Start() {
        auto r = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, r.primary);
        ASSERT_EQ(NX_SUCCESS, r.cleanup);
        ASSERT_EQ(0U, r.remaining_effects);
        ASSERT_EQ(1U, Domain()->count);
    }
    void Tick() {
        nx_h743_spi_dma_factory_step();
        if (!nx_arch_irq_is_masked()) {
            for (int irq : {13, 14, 84}) {
                if (!nx_h743_spi_dma_factory_level(irq) ||
                    !g_h743_model.irq_enabled[irq])
                    continue;
                g_h743_model.irq_active[irq] = true;
                g_h743_spi_dma_model.exception =
                    static_cast<unsigned>(irq + 16);
                if (irq == 13)
                    DMA1_Stream2_IRQHandler();
                else if (irq == 14)
                    DMA1_Stream3_IRQHandler();
                else
                    SPI4_IRQHandler();
                g_h743_spi_dma_model.exception = 0U;
                g_h743_model.irq_active[irq] = false;
            }
        }
        if (g_nx_stm32h743_system.started)
            nx_spi_port_service(Port());
    }
    void Pump(unsigned count = 1200U) {
        for (unsigned n = 0U; n < count; ++n)
            Tick();
    }
    void Submit(nx_time_us_t deadline = 1000000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_spi_request_prepare(&request, memory, memory + 0x1000U, 8U,
                                         deadline));
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(Device(), &request));
    }
    nx_result_t Pause() {
        nx_result_t r = NX_ERROR_BUSY;
        for (unsigned n = 0U; n < 500U && r == NX_ERROR_BUSY; ++n) {
            Tick();
            r = nx_platform_runtime_pause();
        }
        return r;
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        nx_runtime_watch = false;
        if (Owner()->active != nullptr) {
            EXPECT_EQ(NX_SUCCESS, nx_spi_port_cancel(Port(), Owner()->active));
        }
        nx_result_t r = NX_ERROR_BUSY;
        for (unsigned n = 0U; n < 2000U && r == NX_ERROR_BUSY; ++n) {
            Tick();
            r = nx_platform_stop();
        }
        EXPECT_EQ(NX_SUCCESS, r);
        if (r != NX_SUCCESS || Owner()->active != nullptr ||
            Domain()->count != 0U)
            std::abort();
        EXPECT_EQ(0U, g_h743_spi_dma_model.gated_accesses);
        EXPECT_EQ(0U, g_h743_model.spi_reset_order_errors);
        EXPECT_EQ(0U, g_h743_spi_dma_model.ordering_errors);
        EXPECT_EQ(0, munmap(memory, 0x80000U));
    }
};
TEST_F(H743RuntimeClock, ColdGettersHaveNoHardwareOrLeaseEffects) {
    const unsigned writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
    EXPECT_EQ(Domain(), nx_platform_runtime_domain());
    EXPECT_EQ(0U, Domain()->count);
    EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
}
TEST_F(H743RuntimeClock, LeasePrecedesEveryStartupResourceWrite) {
    nx_runtime_watch = true;
    Start();
    EXPECT_EQ(0U, nx_runtime_unleased_writes);
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_EQ(0U, nx_runtime_unleased_writes);
    nx_runtime_watch = false;
    EXPECT_EQ(0U, Domain()->count);
}
TEST_F(H743RuntimeClock, RequestSettledStillRetainsProviderLifecycleLease) {
    Start();
    Submit();
    Pump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_TRUE(Owner()->initialized);
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(Domain(), &barrier));
    EXPECT_EQ(NX_ERROR_BUSY, nx_clock_domain_quiescent(barrier));
    EXPECT_EQ(1U, Domain()->count);
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_EQ(NX_SUCCESS, nx_clock_domain_quiescent(barrier));
}
TEST_F(H743RuntimeClock, ActivePauseClosesWithoutRevokingRequestOrTimebase) {
    Start();
    Submit();
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_runtime_pause());
    EXPECT_EQ(&request, Owner()->active);
    EXPECT_EQ(1U, Domain()->count);
    EXPECT_TRUE(Domain()->closing);
    EXPECT_NE(0U, TIM2->CR1 & TIM_CR1_CEN);
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(Port(), &request));
    Pump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, Pause());
}
TEST_F(H743RuntimeClock, PendingRxCommitCannotAuthorizeLeaseRelease) {
    Start();
    Submit();
    g_h743_spi_dma_model.hold_rx_commit = true;
    Pump(100U);
    ASSERT_TRUE(g_h743_spi_dma_model.rx_pending);
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(Port(), &request));
    Pump(100U);
    EXPECT_NE(0U, DMA1_Stream2->CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_runtime_pause());
    EXPECT_EQ(1U, Domain()->count);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    g_h743_spi_dma_model.hold_rx_commit = false;
    Pump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, Pause());
}
TEST_F(H743RuntimeClock, RefusedGateWithdrawalKeepsLeaseUntilRealRetry) {
    Start();
    g_h743_model.ignore_write = &RCC->AHB1ENR;
    EXPECT_EQ(NX_ERROR_IO, Pause());
    EXPECT_EQ(1U, Domain()->count);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    g_h743_model.ignore_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_EQ(0U, Domain()->count);
}
TEST_F(H743RuntimeClock, PauseResumeKeepsTimerAndMonotonicDeadlineDomain) {
    Start();
    const auto psc = TIM2->PSC;
    const auto overflow = g_nx_stm32h743_system.overflow_us;
    const auto before = nx_time_now_us();
    ASSERT_EQ(NX_SUCCESS, Pause());
    Pump(25U);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, TIM2->CR1 & TIM_CR1_CEN);
    EXPECT_EQ(psc, TIM2->PSC);
    EXPECT_EQ(overflow, g_nx_stm32h743_system.overflow_us);
    EXPECT_GT(nx_time_now_us(), before);
    const auto epoch = Domain()->epoch;
    auto r = nx_platform_runtime_resume();
    ASSERT_EQ(NX_SUCCESS, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
    EXPECT_EQ(epoch + 1U, Domain()->epoch);
    EXPECT_EQ(1U, Domain()->count);
    Submit();
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(H743RuntimeClock, RetainedTimerCountsActualPendingWrapDuringPause) {
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    TIM2->CNT = UINT32_MAX - 1U;
    const auto before = nx_time_now_us();
    TIM2->CNT = 3U;
    TIM2->SR |= TIM_SR_UIF;
    const auto after = nx_time_now_us();
    EXPECT_EQ(before + 5U, after);
    EXPECT_EQ(NX_SUCCESS, nx_platform_runtime_resume().primary);
    EXPECT_GE(nx_time_now_us(), after);
}
TEST_F(H743RuntimeClock, FailedResumePreservesPrimaryAndCleanupResponsibility) {
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    g_h743_spi_dma_factory_model.trim_after_pins = true;
    nx_runtime_hold_resume_cleanup = true;
    auto r = nx_platform_runtime_resume();
    EXPECT_EQ(NX_ERROR_STATE, r.primary);
    EXPECT_NE(NX_SUCCESS, r.cleanup);
    EXPECT_EQ(1U, r.remaining_effects);
    EXPECT_EQ(1U, Domain()->count);
    nx_runtime_hold_resume_cleanup = false;
    RCC->HSICFGR = 64U << RCC_HSICFGR_HSITRIM_Pos;
    g_h743_model.ignore_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_EQ(NX_SUCCESS, nx_platform_runtime_resume().primary);
}
TEST_F(H743RuntimeClock, ContextRejectionsHaveZeroEffectsAndPreserveMask) {
    Start();
    auto saved = nx_arch_irq_save();
    const unsigned writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_runtime_pause());
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_runtime_resume().primary);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
    EXPECT_EQ(1U, Domain()->count);
}
TEST_F(H743RuntimeClock, StaleBarrierCannotApproveResumedProvider) {
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    nx_clock_barrier_t barrier{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(Domain(), &barrier));
    ASSERT_EQ(NX_SUCCESS, nx_platform_runtime_resume().primary);
    EXPECT_EQ(NX_ERROR_STATE, nx_clock_domain_quiescent(barrier));
}
TEST_F(H743RuntimeClock, PreexistingNeighborGateIsNeverWithdrawnByPause) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    DMA1_Stream0->CR = DMA_SxCR_EN;
    DMA1_Stream0->M0AR = 0x24007000U;
    DMAMUX1_Channel0->CCR = 99U;
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(DMA_SxCR_EN, DMA1_Stream0->CR);
    EXPECT_EQ(0x24007000U, DMA1_Stream0->M0AR);
    EXPECT_EQ(99U, DMAMUX1_Channel0->CCR);
}

TEST_F(H743RuntimeClock, RepeatedPauseAfterReleaseHasNoHardwareEffects) {
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    const auto writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
    ASSERT_EQ(0U, Domain()->count);
    ASSERT_EQ(NX_SUCCESS, nx_platform_runtime_pause());
    EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
}
TEST_F(H743RuntimeClock, OldDeadlineExpiresInTheSameLiveDomainAcrossPause) {
    Start();
    const auto deadline = nx_deadline_after(nx_time_now_us(), 10U);
    ASSERT_EQ(NX_SUCCESS, Pause());
    Pump(30U);
    ASSERT_GT(nx_time_now_us(), deadline);
    ASSERT_EQ(NX_SUCCESS, nx_platform_runtime_resume().primary);
    const auto starts = g_h743_spi_dma_model.starts;
    ASSERT_EQ(NX_SUCCESS,
              nx_spi_request_prepare(&request, memory, memory + 0x1000U, 8U,
                                     deadline));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_spi_endpoint_submit(Device(), &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, Owner()->active);
    EXPECT_EQ(starts, g_h743_spi_dma_model.starts);
    EXPECT_EQ(1U, Domain()->count);
}
TEST_F(H743RuntimeClock, RefusedPadRestoreKeepsLeaseAfterDriverStops) {
    Start();
    g_h743_model.ignore_write = &GPIOE->MODER;
    EXPECT_EQ(NX_ERROR_IO, Pause());
    EXPECT_FALSE(Owner()->retained);
    EXPECT_EQ(1U, Domain()->count);
    EXPECT_NE(0U, TIM2->CR1 & TIM_CR1_CEN);
    g_h743_model.ignore_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, Pause());
    EXPECT_EQ(0U, Domain()->count);
}
TEST_F(H743RuntimeClock, ForeignLeaseRefusesResumeBeforeAnyHardwareEffect) {
    Start();
    ASSERT_EQ(NX_SUCCESS, Pause());
    nx_clock_barrier_t barrier{};
    nx_clock_lease_t foreign{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(Domain(), &barrier));
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_acquire(Domain(), &foreign));
    const auto writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
    auto r = nx_platform_runtime_resume();
    EXPECT_EQ(NX_ERROR_BUSY, r.primary);
    EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
    EXPECT_EQ(Domain(), foreign.owner);
    EXPECT_EQ(1U, Domain()->count);
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_release(Domain(), &foreign));
    EXPECT_EQ(NX_SUCCESS, nx_platform_runtime_resume().primary);
}
TEST_F(H743RuntimeClock, LostSharedMuxKeepsQuarantinedLoanAndLifecycleLease) {
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        Start();
        g_h743_spi_dma_model.hold_wire = true;
        Submit();
        Pump(25U);
        if (g_h743_spi_dma_model.rx_pending)
            _exit(3);
        nx_h743_spi_dma_model_assert_reset(RCC_AHB1RSTR_DMA2RST);
        Tick();
        const bool retained =
            nx_request_state(&request.base) == NX_REQUEST_QUARANTINED &&
            Owner()->active == &request && Domain()->count == 1U &&
            nx_platform_runtime_pause() == NX_ERROR_BUSY &&
            g_nx_stm32h743_system.started;
        _exit(retained ? 0 : 4);
    }
    int status = 0;
    pid_t joined = 0;
    for (unsigned n = 0U; n < 100U && joined == 0; ++n) {
        joined = waitpid(child, &status, WNOHANG);
        if (joined == 0)
            usleep(1000U);
    }
    if (joined == 0) {
        kill(child, SIGKILL);
        joined = waitpid(child, &status, 0);
    }
    ASSERT_EQ(child, joined);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(0, WEXITSTATUS(status));
    /* The child's address space is withdrawn; no device cleanup is claimed. */
}
