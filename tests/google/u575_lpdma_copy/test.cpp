/**
 * \file            test.cpp
 * \brief           Actual copy admission, linked data and compound drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus/arch/arch.h"
#include "stm32u575_lpdma_copy_internal.h"
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::StrictMock;
class RetainedBoundary {
  public:
    MOCK_METHOD(nx_result_t, sample,
                (nx_stm32u575_retained_time_storage_t*,
                 nx_stm32u575_retained_time_snapshot_t*));
    MOCK_METHOD(nx_result_t, validate,
                (nx_stm32u575_retained_time_storage_t*, const void*, uint64_t));
};
static RetainedBoundary* retained;
extern "C" nx_result_t nx_stm32u575_retained_time_sample(
    nx_stm32u575_retained_time_storage_t* storage,
    nx_stm32u575_retained_time_snapshot_t* sample) {
    return retained->sample(storage, sample);
}
extern "C" nx_result_t nx_stm32u575_retained_time_validate_alarm_a_copy(
    nx_stm32u575_retained_time_storage_t* storage, const void* owner,
    uint64_t generation) {
    return retained->validate(storage, owner, generation);
}
class LpdmaCopy : public testing::Test {
  protected:
    nx_stm32u575_lpdma_copy_storage_t storage{};
    nx_stm32u575_retained_time_storage_t clock{};
    nx_stm32u575_lpdma_copy_config_t config{&clock, this};
    nx_stm32u575_lpdma_copy_t copy{};
    nx_stm32u575_lpdma_copy_request_t request{};
    nx_stm32u575_retained_time_snapshot_t domain{&clock, 3, 1000000, 256};
    StrictMock<RetainedBoundary> boundary;
    unsigned char* source =
        reinterpret_cast<unsigned char*>(SRAM4_BASE_NS + 0x100U);
    unsigned char* destination =
        reinterpret_cast<unsigned char*>(SRAM4_BASE_NS + 0x200U);
    nx_stm32u575_lpdma_copy_workspace_t* workspace =
        reinterpret_cast<nx_stm32u575_lpdma_copy_workspace_t*>(SRAM4_BASE_NS +
                                                               0x300U);
    nx_arch_irq_state_t mask{};
    bool masked{};
    bool stale{};
    static void SetUpTestSuite() {
        void* mapping =
            mmap(reinterpret_cast<void*>(SRAM4_BASE_NS), SRAM4_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(SRAM4_BASE_NS), mapping);
    }
    static void TearDownTestSuite() {
        EXPECT_EQ(0,
                  munmap(reinterpret_cast<void*>(SRAM4_BASE_NS), SRAM4_SIZE));
    }
    void SetUp() override {
        model.reset();
        std::memset(reinterpret_cast<void*>(SRAM4_BASE_NS), 0xa5, SRAM4_SIZE);
        for (unsigned n = 0; n < 32; ++n)
            source[n] = static_cast<unsigned char>(n + 1U);
        nx_request_initialize(&request.base);
        retained = &boundary;
        EXPECT_CALL(boundary, sample(&clock, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](auto*, auto* out) {
                if (stale)
                    return NX_ERROR_STATE;
                *out = domain;
                return NX_SUCCESS;
            }));
        EXPECT_CALL(boundary, validate(&clock, this, _))
            .Times(AnyNumber())
            .WillRepeatedly(
                Invoke([this](auto*, const void*, uint64_t generation) {
                    return !stale && generation == domain.generation
                               ? NX_SUCCESS
                               : NX_ERROR_STATE;
                }));
    }
    void enter_mask() {
        if (!masked) {
            mask = nx_arch_irq_save();
            masked = true;
        }
    }
    void open() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
    }
    void prepare(size_t length = 16,
                 nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                                  &request, &domain, source, destination,
                                  length, workspace, deadline));
    }
    void arm() {
        enter_mask();
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    }
    void finish() {
        model.trigger();
        model.finish_block();
        model.finish_block();
    }
    void quiet_rtc() {
        model.reg(&RTC->CR) = RTC_CR_BYPSHAD;
        model.reg(&RTC->SR) = 0U;
        model.reg(&NVIC->ISER[0]) = model.reg(&NVIC->ISPR[0]) =
            model.reg(&NVIC->IABR[0]) = 0U;
    }
    void TearDown() override {
        if (copy.ops != nullptr) {
            enter_mask();
            model.hold_suspend = model.hold_reset = model.hold_clear =
                model.hold_irq = false;
            model.link_pending = false;
            model.reg(&RCC->CFGR2) = 0U;
            quiet_rtc();
            if (nx_request_state(&request.base) != NX_REQUEST_READY &&
                nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
                (void)nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_CANCELLED,
                                                    8);
                (void)nx_stm32u575_lpdma_copy_release(&copy, &request,
                                                      NX_ERROR_CANCELLED);
            }
            (void)nx_stm32u575_lpdma_copy_stop(&copy, 8);
        }
        if (masked)
            nx_arch_irq_restore(mask);
        retained = nullptr;
    }
};
TEST_F(LpdmaCopy,
       ReadonlyConstructionBindsExactFaceAndPreparedClockedController) {
    unsigned writes = model.writes;
    open();
    EXPECT_EQ(&nx_stm32u575_lpdma_copy_ops, copy.ops);
    EXPECT_EQ(&storage, copy.context);
    EXPECT_EQ(writes, model.writes);
}
TEST_F(LpdmaCopy, GateOffIsNotIdleProofAndRejectsWithoutDmaAccess) {
    model.reg(&RCC->AHB3ENR) &= ~static_cast<uint32_t>(RCC_AHB3ENR_LPDMA1EN);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
    EXPECT_EQ(0U, model.dma_reads);
    EXPECT_EQ(0U, model.writes);
    EXPECT_EQ(nullptr, copy.ops);
}
TEST_F(LpdmaCopy,
       PreparationRejectsSram1SecureAliasWrapAndWorkspaceAliasUnchanged) {
    auto before = request;
    for (const void* bad : {reinterpret_cast<void*>(0x20000000U),
                            reinterpret_cast<void*>(0x38000100U),
                            reinterpret_cast<void*>(UINTPTR_MAX - 1U)}) {
        EXPECT_NE(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                                  &request, &domain, bad, destination, 16,
                                  workspace, NX_DEADLINE_NEVER));
        EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    }
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_stm32u575_lpdma_copy_request_prepare(
            &request, &domain, source, destination, 16,
            reinterpret_cast<nx_stm32u575_lpdma_copy_workspace_t*>(source),
            NX_DEADLINE_NEVER));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy,
       RejectsForeignChannelAndVectorWithoutAdmissionOrWorkspaceWrite) {
    open();
    prepare();
    enter_mask();
    auto before = *workspace;
    model.reg(&LPDMA1_Channel1->CCR) = DMA_CCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0, std::memcmp(&before, workspace, sizeof(before)));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy, TwoRealDataBlocksNeedOneAlarmAndOnlyFinalChannelCompletion) {
    open();
    prepare();
    arm();
    EXPECT_EQ(14U, (model.reg(&LPDMA1_Channel0->CTR2) & DMA_CTR2_TRIGSEL) >>
                       DMA_CTR2_TRIGSEL_Pos);
    EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CTR2) & (1U << 10U));
    EXPECT_EQ(DMA_CTR2_TCEM, model.reg(&LPDMA1_Channel0->CTR2) & DMA_CTR2_TCEM);
    EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CCR) & DMA_CCR_LSM);
    EXPECT_EQ(DMA_CLLR_UT1 | DMA_CLLR_UT2 | DMA_CLLR_UB1 | DMA_CLLR_USA |
                  DMA_CLLR_UDA | DMA_CLLR_ULL,
              model.reg(&LPDMA1_Channel0->CLLR) &
                  ~static_cast<uint32_t>(DMA_CLLR_LA));
    model.finish_block();
    EXPECT_EQ(0xa5U, destination[0]);
    model.trigger();
    model.finish_block();
    EXPECT_EQ(0, std::memcmp(source, destination, 8));
    EXPECT_EQ(0xa5U, destination[8]);
    EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CSR) & DMA_CSR_TCF);
    EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CTR2) & DMA_CTR2_TRIGPOL);
    EXPECT_EQ(DMA_CTR2_TCEM, model.reg(&LPDMA1_Channel0->CTR2) & DMA_CTR2_TCEM);
    EXPECT_EQ(0U, model.reg(&LPDMA1_Channel0->CLLR));
    model.finish_block();
    EXPECT_EQ(0, std::memcmp(source, destination, 16));
    EXPECT_NE(0U, model.reg(&LPDMA1_Channel0->CSR) & DMA_CSR_TCF);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(16U, request.base.transferred);
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(LpdmaCopy, HeldSuspendAndIgnoredEnClearCannotRevokeLoan) {
    open();
    prepare();
    arm();
    model.trigger();
    model.transfer(3);
    model.hold_suspend = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_cancel(&copy, &request));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_CANCELLED, 2));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(0U, model.reg(&LPDMA1_Channel0->CCR) & DMA_CCR_EN);
    EXPECT_EQ(0U, model.resets);
    model.hold_suspend = false;
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_CANCELLED, 8));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_release(&copy, &request,
                                                          NX_ERROR_CANCELLED));
    EXPECT_EQ(3U, request.base.transferred);
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(LpdmaCopy, CompletionNeverReleasesUntilRtcWutIrqAndOwnerRestoration) {
    open();
    prepare();
    arm();
    finish();
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    model.reg(&RTC->CR) |= RTC_CR_ALRAE | RTC_CR_WUTE;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_copy_service(&copy, 8));
    quiet_rtc();
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
}
TEST_F(LpdmaCopy, SourceProfileLossPerformsZeroDmaAccessAndRetainsEveryLoan) {
    open();
    prepare();
    arm();
    model.reg(&RCC->CFGR2) = 1U;
    auto reads = model.dma_reads;
    auto writes = model.dma_writes;
    EXPECT_NE(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 3));
    EXPECT_EQ(reads, model.dma_reads);
    EXPECT_EQ(writes, model.dma_writes);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    model.reg(&RCC->CFGR2) = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(LpdmaCopy,
       StaleGenerationRejectsBeforeAdmissionWithoutWorkspaceOrMmioEffects) {
    open();
    prepare();
    domain.generation++;
    auto before = *workspace;
    enter_mask();
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, model.writes);
    EXPECT_EQ(0, std::memcmp(&before, workspace, sizeof(before)));
}
TEST_F(LpdmaCopy,
       FirstObservedDeadlineWinsBeforePendingCompletionAndRemainsBorrowed) {
    open();
    prepare(16, domain.now + 10U);
    arm();
    finish();
    domain.now += 10U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_copy_service(&copy, 8));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(LpdmaCopy, ErrorAndLatePendingIrqCannotSettleUntilRealClear) {
    open();
    prepare();
    arm();
    model.reg(&LPDMA1_Channel0->CSR) |= DMA_CSR_DTEF;
    model.hold_clear = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 2));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    model.hold_clear = false;
    model.hold_irq = true;
    model.reg(&NVIC->ISPR[114U / 32U]) |= 1U << (114U % 32U);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 2));
    model.hold_irq = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(LpdmaCopy,
       ScopedGateRestorePreservesRtcAndNoRequestAccessAfterSettlement) {
    model.reg(&RCC->SRDAMR) = RCC_SRDAMR_RTCAPBAMEN;
    open();
    prepare();
    arm();
    finish();
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    EXPECT_EQ(RCC_SRDAMR_RTCAPBAMEN, model.reg(&RCC->SRDAMR));
    EXPECT_NE(0U, model.reg(&RCC->AHB3ENR) & RCC_AHB3ENR_SRAM4EN);
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    auto before = request;
    auto lli = *workspace;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_service(&copy, 8));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(0, std::memcmp(&lli, workspace, sizeof(lli)));
}
TEST_F(LpdmaCopy, WholeWorkspaceAndControlAliasRejectBeforePublication) {
    auto before = request;
    auto* tail = reinterpret_cast<nx_stm32u575_lpdma_copy_workspace_t*>(
        SRAM4_BASE_NS + SRAM4_SIZE - 4U);
    EXPECT_NE(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                              &request, &domain, source, destination, 16, tail,
                              NX_DEADLINE_NEVER));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    auto* in_buffer =
        reinterpret_cast<nx_stm32u575_lpdma_copy_request_t*>(source);
    nx_request_initialize(&in_buffer->base);
    auto saved = *in_buffer;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_lpdma_copy_request_prepare(
                                    in_buffer, &domain, source, destination, 16,
                                    workspace, NX_DEADLINE_NEVER));
    EXPECT_EQ(0, std::memcmp(&saved, in_buffer, sizeof(saved)));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy, CopiedPreparedRequestAndOwnerStorageCannotAcquireHardware) {
    open();
    prepare();
    enter_mask();
    auto copied_request = request;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_lpdma_copy_arm(&copy, &copied_request));
    auto copied_storage = storage;
    nx_stm32u575_lpdma_copy_t forged{copy.ops, &copied_storage};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lpdma_copy_arm(&forged, &request));
    EXPECT_EQ(0U, model.writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}
TEST_F(LpdmaCopy,
       ConstructionOutputAliasAndMutatedPreparedSpanRejectWithoutEffects) {
    auto before = storage;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_lpdma_copy_initialize(
                  &storage, &config,
                  reinterpret_cast<nx_stm32u575_lpdma_copy_t*>(&storage)));
    EXPECT_EQ(0, std::memcmp(&before, &storage, sizeof(storage)));
    auto saved_config = config;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_lpdma_copy_initialize(
                  &storage, &config,
                  reinterpret_cast<nx_stm32u575_lpdma_copy_t*>(&config)));
    EXPECT_EQ(0, std::memcmp(&saved_config, &config, sizeof(config)));
    open();
    prepare();
    enter_mask();
    request.destination = reinterpret_cast<void*>(0x20000000U);
    auto lli = *workspace;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_stm32u575_lpdma_copy_arm(&copy, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0, std::memcmp(&lli, workspace, sizeof(lli)));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy,
       ActiveCpuVectorAndPowerDownRejectWithoutTouchingBorrowedData) {
    open();
    prepare();
    enter_mask();
    model.reg(&NVIC->IABR[114U / 32U]) = 1U << (114U % 32U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    model.reg(&NVIC->IABR[114U / 32U]) = 0U;
    model.reg(&PWR->CR2) = PWR_CR2_SRAM4PDS;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy, BadClockProfileAndRangeOneRejectBeforeDmaAccess) {
    model.reg(&RCC->CFGR2) = 1U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
    EXPECT_EQ(0U, model.dma_reads);
    model.reg(&RCC->CFGR2) = 0U;
    model.reg(&PWR->VOSR) |= PWR_VOSR_VOS;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
    EXPECT_EQ(0U, model.dma_reads);
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy,
       KnownCompletionRejectsLateCancelButCompoundFailureRemainsError) {
    open();
    prepare();
    arm();
    finish();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_lpdma_copy_service(&copy, 8));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lpdma_copy_cancel(&copy, &request));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(LpdmaCopy,
       TaintedClockStillAllowsProvenHardwareDrainAndErrorSettlement) {
    open();
    prepare();
    arm();
    finish();
    stale = true;
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_STATE, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_STATE));
    EXPECT_EQ(NX_ERROR_STATE, request.base.result);
}
TEST_F(LpdmaCopy, SettledRequestCanBeProtectedBeforeLaterServiceAndStop) {
    open();
    enter_mask();
    void* allocation = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, allocation);
    auto* separate =
        static_cast<nx_stm32u575_lpdma_copy_request_t*>(allocation);
    nx_request_initialize(&separate->base);
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                              separate, &domain, source, destination, 16,
                              workspace, NX_DEADLINE_NEVER));
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_arm(&copy, separate));
    finish();
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, separate, NX_SUCCESS));
    ASSERT_EQ(0, mprotect(allocation, 4096, PROT_NONE));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_service(&copy, 8));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_stop(&copy, 8));
    EXPECT_EQ(0, munmap(allocation, 4096));
}
TEST_F(LpdmaCopy, ClockOverlappingConfigRejectsBeforeAnyExternalClockCall) {
    auto* overlapping =
        reinterpret_cast<nx_stm32u575_lpdma_copy_config_t*>(&clock);
    overlapping->clock = &clock;
    overlapping->wake_owner = this;
    EXPECT_CALL(boundary, sample(_, _)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_lpdma_copy_initialize(&storage, overlapping, &copy));
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy, FreshOwnerMustBeAllZeroRatherThanPartlyPlausibleBytes) {
    storage.private_bytes[sizeof(storage) - 1U] = 1U;
    EXPECT_CALL(boundary, sample(_, _)).Times(0);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
    EXPECT_EQ(0U, model.reads);
    EXPECT_EQ(0U, model.writes);
}
TEST_F(LpdmaCopy, PrepareCannotOverwriteItsKnownRetainedClockIdentity) {
    void* allocation = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, allocation);
    auto* separate =
        static_cast<nx_stm32u575_lpdma_copy_request_t*>(allocation);
    nx_request_initialize(&separate->base);
    auto identity = domain;
    identity.identity = static_cast<unsigned char*>(allocation) + 16U;
    auto before = *separate;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_lpdma_copy_request_prepare(
                                    separate, &identity, source, destination,
                                    16, workspace, NX_DEADLINE_NEVER));
    EXPECT_EQ(0, std::memcmp(&before, separate, sizeof(before)));
    EXPECT_EQ(0, munmap(allocation, 4096));
}
TEST_F(LpdmaCopy, ExclusiveRoleAndDrainedLoanFenceASecondAdmission) {
    open();
    nx_stm32u575_lpdma_copy_storage_t other{};
    nx_stm32u575_lpdma_copy_t other_face{};
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_initialize(&other, &config, &other_face));
    EXPECT_EQ(nullptr, other_face.ops);
    prepare();
    arm();
    finish();
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    nx_stm32u575_lpdma_copy_request_t second{};
    nx_request_initialize(&second.base);
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_request_prepare(
                              &second, &domain, source, destination, 16,
                              workspace, NX_DEADLINE_NEVER));
    auto writes = model.writes;
    auto lli = *workspace;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_lpdma_copy_arm(&copy, &second));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&second.base));
    EXPECT_EQ(writes, model.writes);
    EXPECT_EQ(0, std::memcmp(&lli, workspace, sizeof(lli)));
}
TEST_F(LpdmaCopy, FinalLookingTcfWithShortDestinationCannotPublishSuccess) {
    open();
    prepare();
    arm();
    finish();
    model.reg(&LPDMA1_Channel0->CDAR) =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(destination) + 15U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    EXPECT_NE(NX_SUCCESS, request.base.result);
}
TEST_F(LpdmaCopy, OutstandingLinkReadPreventsSuspendProofAndReset) {
    open();
    prepare();
    arm();
    model.link_pending = true;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_cancel(&copy, &request));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_CANCELLED, 2));
    EXPECT_EQ(0U, model.resets);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    model.link_pending = false;
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_CANCELLED, 8));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_release(&copy, &request,
                                                          NX_ERROR_CANCELLED));
}
TEST_F(LpdmaCopy,
       RejectedPrivilegeWriteAfterAcceptanceRetainsAndDrainsSetupLoan) {
    open();
    prepare();
    enter_mask();
    model.hold_privilege = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_arm(&copy, &request));
    EXPECT_NE(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_ERROR_IO, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
}
TEST_F(LpdmaCopy,
       RefusedResetOnCompletedChannelCannotHideBehindAlreadyClearEn) {
    open();
    prepare();
    arm();
    finish();
    model.hold_reset = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 2));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
    EXPECT_NE(0U, model.reg(&RCC->SRDAMR) & RCC_SRDAMR_LPDMA1AMEN);
    model.hold_reset = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, 8));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32u575_lpdma_copy_release(&copy, &request, NX_SUCCESS));
}

TEST_F(LpdmaCopy, EffectiveKernelMsiMustMatchPreparedFourMhzRange) {
    for (bool programmed : {false, true}) {
        model.reg(&RCC->ICSCR1) =
            programmed ? RCC_ICSCR1_MSIRGSEL | (4U << RCC_ICSCR1_MSISRANGE_Pos)
                       : 0U;
        model.reg(&RCC->CSR) = 4U << RCC_CSR_MSISSRANGE_Pos;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy));
        EXPECT_EQ(nullptr, copy.ops);
        EXPECT_EQ(0U, model.writes);
        EXPECT_EQ(0U, model.dma_reads);
    }
}
TEST_F(LpdmaCopy,
       ReceiverObservedExternalResetPermanentlyRetainsUncertainLoan) {
    EXPECT_EXIT(
        {
            open();
            prepare();
            arm();
            model.reg(&RCC->AHB3RSTR) = RCC_AHB3RSTR_LPDMA1RST;
            auto reads = model.dma_reads;
            auto writes = model.dma_writes;
            if (nx_stm32u575_lpdma_copy_service(&copy, 8) != NX_ERROR_BUSY)
                std::_Exit(2);
            if (reads != model.dma_reads || writes != model.dma_writes)
                std::_Exit(3);
            model.reg(&RCC->AHB3RSTR) = 0U;
            /* A reset-default IDLE is not proof the earlier bus master joined.
             */
            model.reg(&LPDMA1_Channel0->CCR) = 0U;
            model.reg(&LPDMA1_Channel0->CSR) = DMA_CSR_IDLEF;
            if (nx_stm32u575_lpdma_copy_service(&copy, 8) != NX_ERROR_BUSY)
                std::_Exit(4);
            if (nx_stm32u575_lpdma_copy_release(&copy, &request, NX_ERROR_IO) !=
                NX_ERROR_BUSY)
                std::_Exit(5);
            if (nx_request_state(&request.base) != NX_REQUEST_QUARANTINED)
                std::_Exit(6);
            if (reads != model.dma_reads || writes != model.dma_writes)
                std::_Exit(7);
            if (nx_stm32u575_lpdma_copy_stop(&copy, 8) != NX_ERROR_BUSY)
                std::_Exit(8);
            if (nx_stm32u575_lpdma_copy_initialize(&storage, &config, &copy) !=
                NX_ERROR_CONTEXT)
                std::_Exit(9);
            if (!testing::Mock::VerifyAndClearExpectations(&boundary) ||
                HasFailure())
                std::_Exit(10);
            std::_Exit(0);
        },
        testing::ExitedWithCode(0), "");
}

TEST(LpdmaCopyBoundary, MemoryCopyContinuesToRejectPeripheralDestination) {
    nx_stm32u575_lpdma_copy_request_t request{};
    nx_stm32u575_retained_time_storage_t clock{};
    nx_stm32u575_retained_time_snapshot_t sample{&clock, 1U, 0U, 0U};
    nx_request_initialize(&request.base);
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_stm32u575_lpdma_copy_request_prepare(
                  &request, &sample,
                  reinterpret_cast<const void*>(SRAM4_BASE_NS + 0x100U),
                  reinterpret_cast<void*>(LPTIM1_BASE_NS + 0x14U), 12U,
                  reinterpret_cast<nx_stm32u575_lpdma_copy_workspace_t*>(
                      SRAM4_BASE_NS + 0x300U),
                  NX_DEADLINE_NEVER));
}
