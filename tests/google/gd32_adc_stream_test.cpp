/**
 * \file            gd32_adc_stream_test.cpp
 * \brief           GD32 ADC discrete blocks retain DMA and consumer ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_adc_stream.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern bool g_gd32_model_adc_works;
extern uint32_t g_gd32_model_mask;
extern uint32_t g_gd32_model_barrier_advance;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool s_hold_dma;

/** \brief A disable store cannot manufacture stopped memory ownership. */
extern "C" void nx_gd32_adc_dma_model_disable(uint32_t mask) {
    if (s_hold_dma) {
        mask &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    }
    DMA_CH0CTL(DMA1) &= ~mask;
}

/** \brief Model hardware acknowledgement without clearing other channels. */
extern "C" void nx_gd32_adc_dma_model_ack(uint32_t flags) {
    DMA_INTC0(DMA1) = flags;
    DMA_INTF0(DMA1) &= ~flags;
}

/** \brief The shared register fixture has no implicit UART interrupt owner. */
extern "C" void USART0_IRQHandler(void) {
}

class GD32ADCStream : public ::testing::Test {
  protected:
    nx_gd32_adc_stream_state_t state = {};
    uint8_t channels[2] = {0, 1};
    uint16_t samples[2][4] = {{11, 12, 21, 22}, {31, 32, 41, 42}};
    nx_dma_memory_region_t regions[2] = {};
    nx_stream_slot_t slots[2] = {};
    nx_stream_t stream = {};
    const nx_adc_port_t port = {&nx_gd32_adc_stream_ops, &state};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 100;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0;
        g_gd32_model_barrier_advance = 0;
        g_gd32_model_adc_works = true;
        s_hold_dma = false;
        for (size_t i = 0; i < 2; ++i) {
            regions[i] = {reinterpret_cast<uintptr_t>(samples[i]),
                          sizeof(samples[i]), NX_DMA_MEMORY_WRITE};
            slots[i].data = reinterpret_cast<uint8_t*>(samples[i]);
            slots[i].capacity = sizeof(samples[i]);
        }
        state.regions = regions;
        state.region_count = 2;
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 2), NX_SUCCESS);
        ASSERT_EQ(
            nx_gd32_adc_stream_initialize(&state, channels, 2, 3300, 10000, 5),
            NX_SUCCESS);
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            s_hold_dma = false;
            (void)nx_adc_port_stream_stop(&port);
            for (size_t i = 0; i < 2; ++i) {
                if (slots[i].state == NX_STREAM_SLOT_READY) {
                    nx_stream_block_t block = {};
                    if (nx_stream_acquire(&stream, &block) == NX_SUCCESS) {
                        (void)nx_stream_release(&stream, &block);
                    }
                } else if (slots[i].state == NX_STREAM_SLOT_BORROWED) {
                    nx_stream_block_t block = {};
                    block.data = slots[i].data;
                    block.slot = i;
                    block.epoch = slots[i].epoch;
                    (void)nx_stream_release(&stream, &block);
                }
            }
            EXPECT_EQ(nx_gd32_adc_stream_stop(&state), NX_SUCCESS);
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
    }
    void Complete() {
        DMA_CH0CNT(DMA1) = 0;
        DMA_INTF0(DMA1) = DMA_INTF_FTFIF;
        nx_gd32_adc_stream_irq(&state);
    }
    void Ready() {
        g_gd32_model_now += 2;
        EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
        EXPECT_NE(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    }
};

TEST_F(GD32ADCStream, FullScanPublicationRequiresEngineDetach) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    Ready();
    EXPECT_EQ(DMA_CH0CNT(DMA1), 4U);
    EXPECT_EQ(DMA_CH0CTL(DMA1) & DMA_CHXCTL_PERIEN, 0U);
    EXPECT_EQ(ADC_RSQ0(ADC0) & ADC_RSQ0_RL, 1U << 20U);
    EXPECT_EQ(ADC_RSQ2(ADC0), 1U << 5U);
    EXPECT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_ETSRC, ADC_EXTTRIG_ROUTINE_T2_TRGO);
    s_hold_dma = true;
    Complete();
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    s_hold_dma = false;
    ASSERT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, sizeof(samples[0]));
    EXPECT_EQ(block.flags, NX_STREAM_BOUNDARY_TRIGGER);
    EXPECT_EQ(block.lost_blocks, 0U);
    EXPECT_EQ(block.data, reinterpret_cast<uint8_t*>(samples[0]));
    EXPECT_EQ(DMA_CH0M0ADDR(DMA1),
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(samples[1])));
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(GD32ADCStream, BackpressureNeverOverwritesAndResumeIsExplicit) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    Ready();
    Complete();
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    nx_stream_block_t first = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &first), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    Ready();
    Complete();
    nx_stream_block_t second = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &second), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    EXPECT_EQ(DMA_CH0CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(stream.losses, 0U);
    EXPECT_EQ(samples[0][0], 11U);
    EXPECT_EQ(samples[1][0], 31U);
    ASSERT_EQ(nx_stream_release(&stream, &first), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(samples[1][0], 31U);
    EXPECT_EQ(nx_stream_release(&stream, &second), NX_SUCCESS);
}

TEST_F(GD32ADCStream, InvalidDomainAndRateRejectBeforeLoanOrEffects) {
    regions[1].permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(DMA_CH0CTL(DMA1), 0U);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    regions[1].permissions = NX_DMA_MEMORY_WRITE;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 500000),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 999),
              NX_ERROR_UNSUPPORTED);
    slots[1].capacity = 6;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_ERROR_INVALID);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    slots[1].capacity = sizeof(samples[1]);
}

TEST_F(GD32ADCStream, StopRetainsProducerUntilEngineStopsAndLateIRQIsHarmless) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    Ready();
    s_hold_dma = true;
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_ETMRC, 0U);
    s_hold_dma = false;
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    Complete();
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_STATE);
}

/** \brief Return a bounded notification without adding a hidden executor. */
static nx_result_t wake_notify(void* context) {
    ++*static_cast<unsigned*>(context);
    return NX_SUCCESS;
}

TEST_F(GD32ADCStream, WakeUsesActualDmaPriorityAndLatchedPublication) {
    unsigned notifications = 0;
    const nx_irq_wake_t wake = {&notifications, wake_notify, true};
    EXPECT_EQ(nx_adc_port_attach_wake(&port, &wake, 6), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_adc_port_attach_wake(&port, &wake, 5), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    Ready();
    Complete();
    EXPECT_EQ(notifications, 1U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
}

TEST_F(GD32ADCStream,
       EveryBlockRequiresStabilityAndRecalibrationBeforeTrigger) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_ETMRC, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_CLB, 0U);
    g_gd32_model_now += 2;
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(ADC_CTL1(ADC0) & ADC_CTL1_CLB, 0U);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    Complete();
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    Ready();
    Complete();
}

TEST_F(GD32ADCStream, CalibrationTimeoutReleasesOnlyQuiescentProducer) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    g_gd32_model_now += 2;
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    g_gd32_model_adc_works = false;
    s_hold_dma = true;
    g_gd32_model_now += 101;
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN, 0U);
    s_hold_dma = false;
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_TIMEOUT);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.losses, 0U);
    EXPECT_EQ(ADC_CTL1(ADC0) & ADC_CTL1_ADCON, 0U);
    g_gd32_model_adc_works = true;
}

TEST_F(GD32ADCStream, DmaFaultDiscardsIncompleteBlockAndCountsOneObservedLoss) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    Ready();
    DMA_CH0CNT(DMA1) = 2;
    DMA_INTF0(DMA1) = DMA_INTF_TAEIF;
    nx_gd32_adc_stream_irq(&state);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.losses, 1U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    Ready();
    Complete();
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.lost_blocks, 1U);
    EXPECT_EQ(block.flags,
              NX_STREAM_BOUNDARY_TRIGGER | NX_STREAM_BOUNDARY_LOSS);
    EXPECT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
}

TEST_F(GD32ADCStream, StabilityClockStartsAfterPowerStoreHasCompleted) {
    g_gd32_model_barrier_advance = 5;
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000), NX_SUCCESS);
    EXPECT_EQ(state.ready_at, g_gd32_model_now + 2);
    EXPECT_FALSE(nx_deadline_expired(state.ready_at, g_gd32_model_now));
    g_gd32_model_barrier_advance = 0;
}
