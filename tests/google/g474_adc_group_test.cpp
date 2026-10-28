/**
 * \file            g474_adc_group_test.cpp
 * \brief           Finite regular/injected provider against external registers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_adc_group_model.h"
#include "stm32_identity.h"
#include "stm32g474_adc_group.h"
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
testing::MockFunction<void(nx_ag_event_t, unsigned)>* observed;
void event(nx_ag_event_t kind, unsigned rank) {
    if (observed != nullptr) {
        observed->Call(kind, rank);
    }
}
class G474ADCGroup : public testing::Test {
  protected:
    nx_stm32g474_adc_group_state_t hardware{};
    const nx_adc_group_port_t port{&nx_stm32g474_adc_group_ops, &hardware};
    nx_adc_group_request_t request{};
    /* Synthetic policy exercises the shared comparison, not a mode review. */
    nx_silicon_revision_rule_t revision{0x2003U, NX_SILICON_REVISION_REVIEWED,
                                        0U};
    nx_silicon_admission_policy_t policy{0x469U, 524288U, &revision, 1U,
                                         0U,     {1U},    false};
    nx_stm32g474_adc_group_profile_t profile{&policy, 1U};
    const nx_stm32_identity_registers_t identity_registers{&g_ag.debug.IDCODE,
                                                           &g_ag.flash_kib};
    const nx_silicon_source_t silicon_source{&identity_registers,
                                             nx_stm32_identity_read};
    uint8_t channels[16]{2U, 1U};
    uint8_t codes[16]{};
    nx_dma_memory_region_t region{};
    nx_stm32g474_adc_group_config_t config{};
    uint16_t* frames{};
    void SelectRevision(uint16_t expected, uint8_t repeat = 1U) {
        revision.revision_id = expected;
        profile.rank_repeat = repeat;
        nx_ag_reset(expected);
    }
    void SetUp() override {
        SelectRevision(0x2003U);
        void* mapped = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x20000000U));
        frames = static_cast<uint16_t*>(mapped);
        for (size_t i = 0U; i != 2048U; ++i) {
            frames[i] = 0xDEADU;
        }
        region = {(uintptr_t)frames, 4096U, NX_DMA_MEMORY_WRITE};
        config.regions = &region;
        config.region_count = 1U;
        config.channels = channels;
        config.sample_times = codes;
        config.channel_count = 2U;
        config.silicon_source = &silicon_source;
        config.profile = &profile;
        config.period_us = 50U;
        config.trigger_identity = 7U;
        config.reference_mv = 3300U;
        config.poll_limit = 100000U;
        config.group = NX_ADC_REGULAR;
        config.trigger = NX_ADC_TRIGGER_TIMER;
        config.priority = 5U;
        config.exclusive = true;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_ag.event = nullptr;
        g_ag.before_read = nullptr;
        g_ag.before_write = nullptr;
        observed = nullptr;
        g_ag.refused = nullptr;
        g_ag.hold_conversion = false;
        g_ag.hold_trigger_sync = false;
        g_ag.hold_dma = false;
        g_ag.hold_adc_stop = false;
        g_ag.hold_adc_disable = false;
        g_ag.hold_dma_disable = false;
        g_ag.hold_counter_stop = false;
        g_ag.hold_irq_disable = false;
        g_ag.privileged = true;
        g_ag.in_isr = false;
        g_ag.mask = 0U;
        g_ag.active[ADC1_2_IRQn] = false;
        g_ag.active[DMA1_Channel1_IRQn] = false;
        if (hardware.active != nullptr) {
            (void)nx_adc_group_cancel(&port, &request);
        }
        for (unsigned i = 0U; i != 2048U && hardware.retained; ++i) {
            nx_ag_advance(170U);
            (void)nx_adc_group_service(&port);
            if (nx_stm32g474_adc_group_close(&hardware) == NX_SUCCESS) {
                break;
            }
        }
        if (hardware.retained || hardware.active != nullptr) {
            std::abort();
        }
        if (frames != nullptr) {
            EXPECT_EQ(munmap(frames, 4096U), 0);
        }
    }
    void Initialize() {
        ASSERT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
                  NX_SUCCESS);
    }
    void Submit(size_t count = 3U, nx_time_us_t lifetime = 10000U) {
        nx_adc_group_info_t info{};
        ASSERT_EQ(nx_adc_group_info(&port, &info), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_prepare(
                      &request, frames, info.format.scan_stride * count, count,
                      nx_deadline_after(nx_time_now_us(), lifetime)),
                  NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    }
    void IRQ(bool dma) {
        const int irq = dma ? (int)DMA1_Channel1_IRQn : (int)ADC1_2_IRQn;
        g_ag.in_isr = true;
        g_ag.active[irq] = true;
        if (dma) {
            nx_stm32g474_adc_group_dma_irq(&hardware);
        } else {
            nx_stm32g474_adc_group_adc_irq(&hardware);
        }
        g_ag.active[irq] = false;
        g_ag.in_isr = false;
    }
    void UntilArmed() {
        for (unsigned i = 0U; i != 2048U; ++i) {
            if ((TIM6->CR1 & TIM_CR1_CEN) != 0U) {
                return;
            }
            nx_ag_advance(170U);
            (void)nx_adc_group_service(&port);
        }
        FAIL() << "owned trigger never armed";
    }
    void Settle() {
        for (unsigned i = 0U; i != 12000U && nx_request_state(&request.base) !=
                                                 NX_REQUEST_SETTLED;
             ++i) {
            nx_ag_advance(170U);
            (void)nx_adc_group_service(&port);
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(hardware.active, nullptr);
        EXPECT_EQ(g_ag.forbidden_writes, 0U);
    }
    void CheckFrames(size_t count) {
        nx_adc_group_info_t info{};
        ASSERT_EQ(nx_adc_group_info(&port, &info), NX_SUCCESS);
        for (size_t frame = 0U; frame != count; ++frame) {
            for (size_t rank = 0U; rank != config.channel_count; ++rank) {
                int64_t value = -1;
                ASSERT_EQ(nx_adc_format_decode(&info.format, frames,
                                               info.format.scan_stride * count,
                                               frame, rank, &value),
                          NX_SUCCESS);
                EXPECT_EQ(value, g_ag.values[channels[rank]]);
            }
        }
    }
};
TEST_F(G474ADCGroup, ColdInitializationOwnsOnlyTheTwoAnalogPadTransitions) {
    GPIOA->MODER = 0xA5A5A5A0U;
    GPIOA->PUPDR = 0x5A5A5A5FU;
    Initialize();
    EXPECT_EQ(0xA5A5A5AFU, GPIOA->MODER);
    EXPECT_EQ(0x5A5A5A50U, GPIOA->PUPDR);
}
TEST_F(G474ADCGroup, RefusedAnalogPadRetainsCleanupBeforeCalibration) {
    GPIOA->MODER = 0U;
    g_ag.refused = &GPIOA->MODER;
    g_ag.held_bits = 15U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32g474_adc_group_initialize(&hardware, &config));
    EXPECT_FALSE(hardware.initialized);
    EXPECT_TRUE(hardware.retained);
    EXPECT_EQ(0U, ADC12_COMMON->CCR);
    EXPECT_EQ(ADC_CR_DEEPPWD, ADC1->CR);
}
TEST_F(G474ADCGroup, ForeignConverterRejectsBeforeAnalogPadChanges) {
    GPIOA->MODER = 0U;
    GPIOA->PUPDR = 15U;
    ADC2->CR = ADC_CR_ADEN;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32g474_adc_group_initialize(&hardware, &config));
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(15U, GPIOA->PUPDR);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474ADCGroup, InvalidPrivateZeroRankInfoRejectsWithoutAccessOrOutput) {
    EXPECT_EXIT(
        ([&]() {
            /* This is a malformed private context, not a supported mutation
             * of an admitted immutable configuration or an active owner. */
            config.channel_count = 0U;
            nx_stm32g474_adc_group_state_t invalid{};
            invalid.config = &config;
            invalid.initialized = true;
            const nx_adc_group_port_t invalid_port{&nx_stm32g474_adc_group_ops,
                                                   &invalid};
            nx_adc_group_info_t output{};
            output.max_frames = 123U;
            output.active = true;
            unsigned char before[sizeof(output)]{};
            std::memcpy(before, &output, sizeof(output));
            g_ag.before_read = [](volatile uint32_t*) { std::_Exit(2); };
            g_ag.before_write = [](volatile uint32_t*, uint32_t) {
                std::_Exit(3);
            };
            const nx_result_t result =
                nx_adc_group_info(&invalid_port, &output);
            std::_Exit(result == NX_ERROR_STATE && g_ag.writes == 0U &&
                               std::memcmp(before, &output, sizeof(output)) == 0
                           ? 0
                           : 4);
        }()),
        testing::ExitedWithCode(0), "");
}
TEST_F(G474ADCGroup, HardwareLayoutIsActualOrderedRegularSequence) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(&port, &info), NX_SUCCESS);
    EXPECT_EQ(info.format.encoding, NX_ADC_UNSIGNED);
    EXPECT_EQ(info.format.significant_bits, 12U);
    EXPECT_EQ(info.format.storage_bytes, 2U);
    EXPECT_EQ(info.format.sample_stride, 2U);
    EXPECT_EQ(info.format.scan_stride, 4U);
    EXPECT_EQ(info.format.group, NX_ADC_REGULAR);
    EXPECT_EQ(info.format.trigger, NX_ADC_TRIGGER_TIMER);
    EXPECT_EQ(info.format.trigger_identity, 7U);
    EXPECT_EQ(info.adc_clock_hz, 21250000U);
    ASSERT_EQ(info.sample_code_count, 8U);
    ASSERT_NE(info.sample_halfcycles, nullptr);
    EXPECT_EQ(info.sample_halfcycles[0], 5U);
    EXPECT_EQ(info.sample_halfcycles[7], 1281U);
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 3U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, SoftwareRegularUsesActualContFiniteDmaAndZeroIdentity) {
    config.trigger = NX_ADC_TRIGGER_SOFTWARE;
    config.trigger_identity = 0U;
    config.period_us = 0U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(4U));
    for (unsigned i = 0U; i != 2048U && (ADC1->CFGR & ADC_CFGR_CONT) == 0U;
         ++i) {
        nx_ag_advance(170U);
        (void)nx_adc_group_service(&port);
    }
    ASSERT_NE(ADC1->CFGR & ADC_CFGR_CONT, 0U);
    nx_ag_advance(1000U * 170U);
    ASSERT_NE(DMA1->ISR & DMA_ISR_TCIF1, 0U);
    EXPECT_NE(ADC1->CR & ADC_CR_ADSTART, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(g_ag.trigger_edges, 0U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(4U));
}
TEST_F(G474ADCGroup, SixteenPhysicalRanksCrossEverySqrRegisterInOrder) {
    config.channel_count = 16U;
    for (unsigned i = 0U; i != 16U; ++i) {
        channels[i] = (uint8_t)((i & 1U) + 1U);
    }
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(2U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(2U));
}
TEST_F(G474ADCGroup, ZCutDuplicatesEachChannelAndExposesDiscardedPadding) {
    SelectRevision(0x2001U, 2U);
    ASSERT_NO_FATAL_FAILURE(Initialize());
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(&port, &info), NX_SUCCESS);
    EXPECT_EQ(info.format.sample_stride, 4U);
    EXPECT_EQ(info.format.scan_stride, 8U);
    EXPECT_EQ(info.physical_ranks, 4U);
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_TRUE(g_ag.duplicate_disturbance);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
    EXPECT_NE(frames[1], g_ag.values[channels[0]]);
}
TEST_F(G474ADCGroup, ZCutRejectsLogicalRanksThatExceedPhysicalHardware) {
    SelectRevision(0x2001U, 2U);
    config.channel_count = 9U;
    const unsigned writes = g_ag.writes;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, writes);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474ADCGroup, YCutSecondWindowDoesNotDeliverChannelZeroAfterOneShot) {
    SelectRevision(0x2002U);
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    nx_ag_advance(2000U * 170U);
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, DuplicateChannelCannotHaveTwoDifferentSmprCodes) {
    channels[1] = channels[0];
    codes[1] = 1U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_INVALID);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, SamplingCodeEightIsRejectedBeforeRegisterWrites) {
    codes[0] = 8U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_INVALID);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, AdcCutMismatchRejectsBeforeAnyPeripheralEffects) {
    DBGMCU->IDCODE = (0x2001U << 16U) | 0x469U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474ADCGroup, PeriodShorterThanExpandedScanCannotClaimEveryFrame) {
    codes[0] = 7U;
    codes[1] = 7U;
    config.period_us = 50U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, HardwareGapBeyondErrataBoundRejectsWithoutSilentRounding) {
    config.period_us = 1001U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, OneMillisecondPeriodCannotMeetPrimeFreshnessAdmission) {
    config.period_us = 1000U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474ADCGroup, SlowPrimeCannotLeaveAnImpossibleHardwareTriggerBudget) {
    codes[0] = codes[1] = 7U;
    config.period_us = 950U;
    /* This actual scan alone costs>61us at21.25MHz. Even zero task delay
     * cannot put the first edge<1ms after the discarded scan START. */
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474ADCGroup, MissingAdcGateNeverReadsInaccessiblePeripheral) {
    RCC->AHB2ENR &= ~(uint32_t)RCC_AHB2ENR_ADC12EN;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_STATE);
    EXPECT_EQ(g_ag.inaccessible_reads, 0U);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, ForeignPeerAdcAndNvicRejectBeforeEffects) {
    ADC2->CR |= ADC_CR_ADEN;
    g_ag.enabled[ADC1_2_IRQn] = true;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_BUSY);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, ByteSpanMustContainExactlyCompleteFrames) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_EQ(nx_adc_group_prepare(&request, frames, 10U, 3U,
                                   nx_deadline_after(nx_time_now_us(), 10000U)),
              NX_SUCCESS);
    const unsigned writes = g_ag.writes;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(g_ag.writes, writes);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.active, nullptr);
}
TEST_F(G474ADCGroup, SixteenBitDmaCounterCannotWrapIntoAValidFrame) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_EQ(nx_adc_group_prepare(&request, frames, 262144U, 65536U,
                                   nx_deadline_after(nx_time_now_us(), 10000U)),
              NX_SUCCESS);
    EXPECT_NE(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.active, nullptr);
}
TEST_F(G474ADCGroup, FiniteDmaDoesNotWritePastLastFrameWithLateIrq) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(1000U * 170U);
    EXPECT_EQ(DMA1_Channel1->CNDTR, 0U);
    EXPECT_TRUE(g_ag.limited);
    EXPECT_EQ(frames[6], 0xDEADU);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    IRQ(true);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.transferred, 3U);
}
TEST_F(G474ADCGroup, EocAndPartialDmaBeatNeverPublishAFrame) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(51U * 170U);
    EXPECT_EQ(DMA1_Channel1->CNDTR, 5U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, CancelMidRankDiscardsAllGuaranteedFrameCounts) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(10U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(51U * 170U);
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(DMA1_Channel1->CMAR, 0U);
}
TEST_F(G474ADCGroup, RefusedDmaDisableRetainsLoanUntilActualRetry) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    g_ag.hold_dma_disable = true;
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    for (unsigned i = 0U; i != 100U; ++i) {
        nx_ag_advance(170U);
        (void)nx_adc_group_service(&port);
    }
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(hardware.active, &request);
    EXPECT_NE(DMA1_Channel1->CMAR, 0U);
    g_ag.hold_dma_disable = false;
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(G474ADCGroup, ActiveAdcVectorPreventsReleasePublication) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_ag.active[ADC1_2_IRQn] = true;
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    for (unsigned i = 0U; i != 100U; ++i) {
        nx_ag_advance(170U);
        (void)nx_adc_group_service(&port);
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.active, &request);
    g_ag.active[ADC1_2_IRQn] = false;
    ASSERT_NO_FATAL_FAILURE(Settle());
}
TEST_F(G474ADCGroup, CapturedDmaTransferErrorSurvivesW1cAcknowledgement) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    DMA1->ISR |= DMA_ISR_TEIF1;
    IRQ(true);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, OverrunPreservesRequestFailureEvenAfterCounterZero) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    ADC1->ISR |= ADC_ISR_OVR;
    DMA1->ISR |= DMA_ISR_TCIF1;
    DMA1_Channel1->CNDTR = 0U;
    IRQ(false);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, SoftwareInjectedReportsCompleteGroupAndNoDmaLease) {
    config.group = NX_ADC_INJECTED;
    config.trigger = NX_ADC_TRIGGER_SOFTWARE;
    config.trigger_identity = 0U;
    config.period_us = 0U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(DMA1_Channel1->CMAR, 0U);
    EXPECT_EQ(DMA1_Channel1->CCR, 0U);
    EXPECT_FALSE(g_ag.enabled[DMA1_Channel1_IRQn]);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(1U));
}
TEST_F(G474ADCGroup, InjectedHardwareOpmCannotOverwriteJdrWithLateService) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(2000U * 170U);
    EXPECT_EQ(TIM6->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(g_ag.trigger_edges, 1U);
    EXPECT_NE(ADC1->ISR & ADC_ISR_JEOS, 0U);
    IRQ(false);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(1U));
}
TEST_F(G474ADCGroup, InjectedFourJdrRanksAreCopiedOnlyAfterWholeJeos) {
    config.group = NX_ADC_INJECTED;
    config.channel_count = 4U;
    channels[2] = 1U;
    channels[3] = 2U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(51U * 170U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_NO_FATAL_FAILURE(Settle());
    ASSERT_NO_FATAL_FAILURE(CheckFrames(1U));
}
TEST_F(G474ADCGroup,
       RepeatingInjectedFramesCannotHideUndetectableJdrOverwrite) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_EQ(nx_adc_group_prepare(&request, frames, 8U, 2U,
                                   nx_deadline_after(nx_time_now_us(), 10000U)),
              NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(hardware.active, nullptr);
}
TEST_F(G474ADCGroup, JeocAloneCannotStandInForCompleteInjectedFrame) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    g_ag.hold_conversion = true;
    ADC1->ISR |= ADC_ISR_JEOC;
    IRQ(false);
    (void)nx_adc_group_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, InternalVrefSamplingMustMeetActualFourMicroseconds) {
    channels[0] = 18U;
    codes[0] = 4U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, InternalTemperatureRequiresAtLeastFiveMicroseconds) {
    channels[0] = 16U;
    codes[0] = 5U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, InternalVbatRequiresTwelveMicrosecondsNotCodeSix) {
    channels[0] = 17U;
    codes[0] = 6U;
    EXPECT_EQ(nx_stm32g474_adc_group_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroup, RealInternalSourcesUseRetainedEnablesAndRawFormat) {
    channels[0] = 18U;
    channels[1] = 16U;
    codes[0] = 5U;
    codes[1] = 6U;
    config.period_us = 100U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    EXPECT_EQ(ADC12_COMMON->CCR & (ADC_CCR_VREFEN | ADC_CCR_VSENSESEL),
              ADC_CCR_VREFEN | ADC_CCR_VSENSESEL);
    EXPECT_GE(nx_time_now_us(), 120U);
    ASSERT_NO_FATAL_FAILURE(Submit(2U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    ASSERT_NO_FATAL_FAILURE(CheckFrames(2U));
}
TEST_F(G474ADCGroup, AllRankWatchdogIsActualTerminalIrqFact) {
    config.watchdog = true;
    config.watchdog_low = 0U;
    config.watchdog_high = 150U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, ExpiredUnobservedTcCannotBePublishedAsTimelySuccess) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U, 100U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(1000U * 170U);
    IRQ(true);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, SCSOutputAndControlDescriptorAreRejectedBeforeAccess) {
    EXPECT_EXIT(
        ([&]() {
            void* scs =
                mmap(reinterpret_cast<void*>(0xE000E000U), 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (scs != reinterpret_cast<void*>(0xE000E000U)) {
                std::_Exit(2);
            }
            if (nx_stm32g474_adc_group_initialize(&hardware, &config) !=
                NX_SUCCESS) {
                std::_Exit(3);
            }
            const auto* descriptor = static_cast<nx_adc_group_request_t*>(scs);
            auto* info = static_cast<nx_adc_group_info_t*>(scs);
            const bool rejected =
                nx_adc_group_submit(
                    &port, const_cast<nx_adc_group_request_t*>(descriptor)) ==
                    NX_ERROR_INVALID &&
                nx_adc_group_info(&port, info) == NX_ERROR_INVALID;
            std::_Exit(rejected ? 0 : 4);
        }()),
        testing::ExitedWithCode(0), "");
}
TEST_F(G474ADCGroup, GmockObservesPhysicalRankSequenceBeforeTerminalFrame) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    testing::StrictMock<testing::MockFunction<void(nx_ag_event_t, unsigned)>>
        sink;
    observed = &sink;
    g_ag.event = event;
    {
        testing::InSequence order;
        EXPECT_CALL(sink, Call(NX_AG_TIMER_EDGE, 0U));
        EXPECT_CALL(sink, Call(NX_AG_RANK, 0U));
        EXPECT_CALL(sink, Call(NX_AG_RANK, 1U));
        EXPECT_CALL(sink, Call(NX_AG_JEOS, 2U));
    }
    nx_ag_advance(100U * 170U);
    g_ag.event = nullptr;
    observed = nullptr;
    ASSERT_NO_FATAL_FAILURE(Settle());
}
TEST_F(G474ADCGroup, RefusedAdcDisableCannotReleaseEvenAfterJeos) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    g_ag.hold_adc_disable = true;
    nx_ag_advance(100U * 170U);
    for (unsigned i = 0U; i != 100U; ++i) {
        nx_ag_advance(170U);
        (void)nx_adc_group_service(&port);
    }
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(hardware.active, &request);
    g_ag.hold_adc_disable = false;
    ASSERT_NO_FATAL_FAILURE(Settle());
}
TEST_F(G474ADCGroup, SynchronizedOldTriggerCannotTouchSettledStorage) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    g_ag.hold_trigger_sync = true;
    nx_ag_advance(60U * 170U);
    ASSERT_TRUE(g_ag.trigger_pending);
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Settle());
    const uint16_t first = frames[0];
    const uint16_t second = frames[1];
    g_ag.hold_trigger_sync = false;
    nx_ag_advance(100U * 170U);
    IRQ(false);
    EXPECT_EQ(frames[0], first);
    EXPECT_EQ(frames[1], second);
    EXPECT_EQ(hardware.active, nullptr);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(G474ADCGroup, LateCapturedTeCannotBeAcknowledgedIntoSuccess) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(1000U * 170U);
    static unsigned snapshots;
    snapshots = 0U;
    g_ag.before_read = [](volatile uint32_t* reg) {
        if (reg == &DMA1->ISR && ++snapshots == 2U) {
            DMA1->ISR |= DMA_ISR_TEIF1;
        }
    };
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_GE(snapshots, 2U);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup, LateOverrunDuringStopCannotBePublishedAsSuccess) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    nx_ag_advance(1000U * 170U);
    g_ag.before_write = [](volatile uint32_t* reg, uint32_t value) {
        if (reg == &ADC1->CR && (value & ADC_CR_ADSTP) != 0U) {
            ADC1->ISR |= ADC_ISR_OVR;
        }
    };
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474ADCGroup,
       PrimeDataCannotBecomeValidAfterTaskStallsOverOneMillisecond) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_ag_advance(2000U * 170U);
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, ZInjectedTwoLogicalRanksUseFourPhysicalJdrSlots) {
    SelectRevision(0x2001U, 2U);
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(1U));
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(&port, &info), NX_SUCCESS);
    EXPECT_EQ(info.physical_ranks, 4U);
    EXPECT_EQ(info.format.scan_stride, 8U);
}
TEST_F(G474ADCGroup, InternalVbatDividerActuallyEnabledUntilJoinedClose) {
    channels[0] = 17U;
    codes[0] = 7U;
    config.period_us = 100U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(2U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_NE(ADC12_COMMON->CCR & ADC_CCR_VBATSEL, 0U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(2U));
    ASSERT_EQ(nx_stm32g474_adc_group_close(&hardware), NX_SUCCESS);
    EXPECT_EQ(ADC12_COMMON->CCR & ADC_CCR_VBATSEL, 0U);
}
TEST_F(G474ADCGroup, TemperatureStartupDoesNotHoldOneLongInterruptMask) {
    channels[0] = 16U;
    codes[0] = 6U;
    config.period_us = 100U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    EXPECT_GE(nx_time_now_us(), 120U);
    EXPECT_LT(g_ag.max_masked_run, 120U * 170U);
    EXPECT_EQ(g_ag.mask, 0U);
}
TEST_F(G474ADCGroup, UnprivilegedAdmissionRejectsWithoutMmioOrLoan) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_EQ(nx_adc_group_prepare(&request, frames, 12U, 3U,
                                   nx_deadline_after(nx_time_now_us(), 10000U)),
              NX_SUCCESS);
    g_ag.privileged = false;
    const unsigned writes = g_ag.writes;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_CONTEXT);
    EXPECT_EQ(g_ag.writes, writes);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.active, nullptr);
    g_ag.privileged = true;
}
TEST_F(G474ADCGroup, RefusedTimerCenWithdrawalDoesNotReleaseItsBorrow) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(10U));
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    g_ag.hold_counter_stop = true;
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    for (unsigned i = 0U; i != 100U; ++i) {
        nx_ag_advance(170U);
        (void)nx_adc_group_service(&port);
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.active, &request);
    g_ag.hold_counter_stop = false;
    ASSERT_NO_FATAL_FAILURE(Settle());
}
TEST_F(G474ADCGroup, StopDoesNotIssueUgOrEmitAnotherInjectedTrigger) {
    config.group = NX_ADC_INJECTED;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    const unsigned edges = g_ag.trigger_edges;
    ASSERT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    nx_ag_advance(100U * 170U);
    EXPECT_EQ(g_ag.trigger_edges, edges);
    EXPECT_EQ(ADC1->CR & (ADC_CR_JADSTART | ADC_CR_JADSTP), 0U);
}
TEST_F(G474ADCGroup, LiveAdcGateLossIsObservedBeforeAnyAdcRegisterRead) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    RCC->AHB2ENR &= ~(uint32_t)RCC_AHB2ENR_ADC12EN;
    const unsigned reads = g_ag.inaccessible_reads;
    EXPECT_EQ(nx_adc_group_stop(&port), NX_ERROR_STATE);
    EXPECT_EQ(g_ag.inaccessible_reads, reads);
    EXPECT_EQ(hardware.active, &request);
    RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
}
TEST_F(G474ADCGroup, DiscardedPrimeDoesNotCreateValidWindowWatchdogFailure) {
    config.watchdog = true;
    config.watchdog_low = 100U;
    config.watchdog_high = 200U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    nx_ag_advance(2000U * 170U);
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, ClosedPredecessorStopCannotWithdrawAnActiveSuccessor) {
    nx_stm32g474_adc_group_state_t predecessor{};
    const nx_adc_group_port_t old_face{&nx_stm32g474_adc_group_ops,
                                       &predecessor};
    ASSERT_EQ(nx_stm32g474_adc_group_initialize(&predecessor, &config),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32g474_adc_group_close(&predecessor), NX_SUCCESS);
    ASSERT_FALSE(predecessor.retained);
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(UntilArmed());
    const unsigned writes = g_ag.writes;
    const uint64_t cycles = g_ag.cycles;
    const uint32_t counter = TIM6->CR1;
    const uint32_t converter = ADC1->CR;
    const uint32_t interrupts = ADC1->IER;
    const uint32_t dma = DMA1_Channel1->CCR;
    const uint32_t target = DMA1_Channel1->CMAR;
    const uint32_t remaining = DMA1_Channel1->CNDTR;
    EXPECT_EQ(nx_adc_group_stop(&old_face), NX_SUCCESS);
    EXPECT_EQ(g_ag.writes, writes);
    EXPECT_EQ(g_ag.cycles, cycles);
    EXPECT_EQ(TIM6->CR1, counter);
    EXPECT_EQ(ADC1->CR, converter);
    EXPECT_EQ(ADC1->IER, interrupts);
    EXPECT_EQ(DMA1_Channel1->CCR, dma);
    EXPECT_EQ(DMA1_Channel1->CMAR, target);
    EXPECT_EQ(DMA1_Channel1->CNDTR, remaining);
    EXPECT_TRUE(g_ag.enabled[ADC1_2_IRQn]);
    EXPECT_TRUE(g_ag.enabled[DMA1_Channel1_IRQn]);
    EXPECT_EQ(hardware.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
TEST_F(G474ADCGroup, DelayedHardwareArmMustReprimeBeforeStartingItsCounter) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    static bool delayed;
    delayed = false;
    g_ag.before_write = [](volatile uint32_t* reg, uint32_t value) {
        if (!delayed && reg == &ADC1->CR && (value & ADC_CR_ADSTART) != 0U &&
            (ADC1->CFGR & ADC_CFGR_EXTEN) != 0U) {
            delayed = true;
            nx_ag_advance(2000U * 170U);
        }
    };
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_TRUE(delayed);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_GE(g_ag.frames, 5U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, DelayedSoftwareRegularArmCannotPublishAStaleFirstScan) {
    config.trigger = NX_ADC_TRIGGER_SOFTWARE;
    config.trigger_identity = 0U;
    config.period_us = 0U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    static bool delayed;
    delayed = false;
    g_ag.before_write = [](volatile uint32_t* reg, uint32_t value) {
        if (!delayed && reg == &ADC1->CR && (value & ADC_CR_ADSTART) != 0U &&
            (ADC1->CFGR & ADC_CFGR_CONT) != 0U) {
            delayed = true;
            nx_ag_advance(2000U * 170U);
        }
    };
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_TRUE(delayed);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_GE(g_ag.frames, 5U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(3U));
}
TEST_F(G474ADCGroup, DelayedSoftwareInjectedArmCannotPublishAStaleFirstScan) {
    config.group = NX_ADC_INJECTED;
    config.trigger = NX_ADC_TRIGGER_SOFTWARE;
    config.trigger_identity = 0U;
    config.period_us = 0U;
    ASSERT_NO_FATAL_FAILURE(Initialize());
    static unsigned starts;
    static bool delayed;
    starts = 0U;
    delayed = false;
    g_ag.before_write = [](volatile uint32_t* reg, uint32_t value) {
        if (reg == &ADC1->CR && (value & ADC_CR_JADSTART) != 0U &&
            (value & ADC_CR_JADSTP) == 0U &&
            (ADC1->CR & ADC_CR_JADSTART) == 0U && ++starts == 2U) {
            delayed = true;
            nx_ag_advance(2000U * 170U);
        }
    };
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    ASSERT_NO_FATAL_FAILURE(Settle());
    EXPECT_TRUE(delayed);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_GE(g_ag.frames, 3U);
    ASSERT_NO_FATAL_FAILURE(CheckFrames(1U));
}
} /* namespace */
