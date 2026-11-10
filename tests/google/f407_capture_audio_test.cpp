/**
 * \file            f407_capture_audio_test.cpp
 * \brief           Real F407 capture/audio loans at external MMIO boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_capture_audio_model.h"
extern "C" {
#include "stm32f407_audio.h"
#include "stm32f407_capture.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <numeric>
#include <sys/mman.h>

extern "C" {
nx_stm32_system_t g_nx_stm32_system;
uint32_t g_av_trace;
uint32_t g_av_counter;
}
namespace {
struct Model {
    RCC_TypeDef rcc = {};
    DCMI_TypeDef dcmi = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef streams[8] = {};
    GPIO_TypeDef gpio = {};
    SPI_TypeDef spi = {}, extension = {}, spi3 = {};
    TIM_TypeDef timer = {};
    uint32_t enabled[82] = {}, pending[82] = {}, active[82] = {},
             priority[82] = {};
    nx_time_us_t now = 0U;
    uint32_t cycles = 0U;
    unsigned writes = 0U, resets = 0U;
    bool hold_capture = false, hold_dma = false, hold_address = false;
    bool hold_fifo = false, hold_irq = false, hold_pll = false;
    bool hold_esur = false, hold_ier = false, race_bank = false;
    bool automatic_prefill = true, master_tail = false;
    bool read_dr = false;
    bool hold_priority_restore = false;
    bool inject_capture_error = false;
    bool lose_ws_after_prefill = false, clear_txe_on_disable = false;
    bool refuse_txdma_enable = false;
    bool refuse_i2se_enable = false, busy_on_txe_read = false;
    bool privileged = true, in_isr = false;
    uint32_t masks[3] = {};
    unsigned enable_attempts = 0U;
    bool advance_time = false;
    unsigned fifo_polls = 0U, wire_polls = 0U;
    uint32_t dma_resets = 0U;
} * model;
constexpr uint32_t kLaneShift[8] = {0U, 6U, 16U, 22U, 0U, 6U, 16U, 22U};

class CaptureAudio : public testing::Test {
  protected:
    Model hardware;
    alignas(8) uint8_t pool[5][256] = {};
    alignas(8) uint8_t rx_pool[5][256] = {};
    nx_stream_slot_t slots[5] = {}, rx_slots[5] = {};
    nx_stream_t stream = {}, rx_stream = {};
    nx_frame_stream_t frame = {}, rx_frame = {};
    nx_stm32_av_pin_t pins[17] = {};
    uint16_t snapshots[17] = {};
    nx_dma_memory_region_t regions[2] = {};
    nx_stm32_capture_config_t capture_config = {};
    nx_stm32_capture_state_t capture = {};
    nx_stm32_audio_config_t audio_config = {};
    nx_stm32_audio_state_t audio = {};
    nx_stm32_audio_clock_t clock = {};
    nx_frame_dma_loan_t tx_loan = {};
    nx_capture_request_t snapshot = {};
    nx_audio_request_t audio_request = {};

    void SetUp() override {
        model = &hardware;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &hardware.rcc;
        g_nx_stm32_system.core_hz = 168000000U;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.timer = &hardware.timer;
        hardware.timer.PSC = 83U;
        hardware.timer.ARR = UINT32_MAX;
        hardware.timer.CR1 = TIM_CR1_CEN;
        hardware.timer.DIER = TIM_DIER_UIE;
        hardware.rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
        hardware.rcc.AHB1ENR =
            RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
        hardware.rcc.PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        hardware.rcc.CFGR =
            NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        hardware.rcc.CR =
            RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        hardware.spi.SR = SPI_SR_TXE;
        hardware.extension.SR = SPI_SR_TXE;
        hardware.spi3.SR = SPI_SR_TXE;
        hardware.gpio.IDR = 1U << 12U;
        for (size_t i = 0U; i < 17U; ++i)
            pins[i] = {&hardware.gpio, &snapshots[i], 0U,
                       static_cast<uint8_t>(i % 16U), 13U};
        regions[0] = {reinterpret_cast<uintptr_t>(pool), sizeof(pool),
                      NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        regions[1] = {reinterpret_cast<uintptr_t>(rx_pool), sizeof(rx_pool),
                      NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        for (size_t i = 0U; i < 5U; ++i) {
            slots[i].data = pool[i];
            slots[i].capacity = sizeof(pool[i]);
            rx_slots[i].data = rx_pool[i];
            rx_slots[i].capacity = sizeof(rx_pool[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
        ASSERT_EQ(nx_stream_initialize(&rx_stream, rx_slots, 5U), NX_SUCCESS);
        layout(&frame, &stream, 256U);
        layout(&rx_frame, &rx_stream, 16U);
        capture_config.common = {pins, 11U, regions, 2U, 32U, 5U};
        capture_config.memory = dma(1U, 1U);
        capture_config.registers = &hardware.dcmi;
        capture_config.hclk_hz = 168000000U;
        capture_config.pixel_clock_hz = 1000000U;
        capture_config.max_service_gap_us = 10U;
        capture_config.bits = 8U;
        capture_config.frame_divisor = 1U;
        audio_config.common = {pins, 3U, regions, 2U, 128U, 5U};
        audio_config.tx = dma(4U, 0U);
        audio_config.registers = &hardware.spi;
        audio_config.extension = &hardware.extension;
        audio_config.clock = &clock;
        audio_config.ws_gpio = &hardware.gpio;
        audio_config.ck_gpio = &hardware.gpio;
        audio_config.ws_pin = 12U;
        audio_config.ck_pin = 13U;
        audio_config.controller = 2U;
        audio_config.hclk_hz = 168000000U;
        audio_config.pclk_hz = 42000000U;
        audio_config.sample_bits = 16U;
        audio_config.slot_bits = 16U;
        audio_config.divider = 31U;
        audio_config.pll_input_hz = 8000000U;
        audio_config.pll_m = 8U;
        audio_config.pll_hse = true;
        audio_config.pll_n = 192U;
        audio_config.pll_r = 2U;
        audio_config.actual_rate = {48000U, 1U};
        nx_request_initialize(&snapshot.base);
        ASSERT_EQ(nx_request_prepare(&snapshot.base, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
        snapshot.destination = &stream;
        snapshot.raw_frame = &frame;
        snapshot.storage_bytes = 256U;
        nx_request_initialize(&audio_request.base);
        ASSERT_EQ(nx_request_prepare(&audio_request.base, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
        g_av_trace = 0U;
        g_av_counter = 0U;
    }
    static void layout(nx_frame_stream_t* output, nx_stream_t* stream,
                       size_t bytes) {
        nx_frame_plane_t plane = {0U, bytes, bytes, 1U};
        nx_frame_layout_t checked;
        ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &checked), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(output, stream, &checked),
                  NX_SUCCESS);
    }
    nx_stm32_av_dma_t dma(uint8_t lane, uint8_t channel) {
        return {&hardware.dma, &hardware.streams[lane],
                static_cast<IRQn_Type>(lane == 1U   ? 57U
                                       : lane == 7U ? 70U
                                                    : 11U + lane),
                lane, channel};
    }
    void capture_open() {
        ASSERT_EQ(nx_stm32_capture_prepare(&capture, &capture_config),
                  NX_SUCCESS);
    }
    static void calculate_rate(nx_stm32_audio_config_t* config) {
        uint32_t channels = config->standard >= 3U ? 1U : 2U;
        uint32_t factor =
            config->mclk ? 128U * channels : config->slot_bits * channels;
        uint64_t numerator =
            static_cast<uint64_t>(config->pll_input_hz) * config->pll_n;
        uint64_t denominator = static_cast<uint64_t>(config->pll_m) *
                               config->pll_r * factor *
                               (2U * config->divider + (config->odd ? 1U : 0U));
        uint64_t divisor = std::gcd(numerator, denominator);
        config->actual_rate = {numerator / divisor, denominator / divisor};
    }
    void audio_open() {
        if (audio_config.master)
            calculate_rate(&audio_config);
        for (size_t i = 0U; i < audio_config.common.pin_count; ++i)
            pins[i].af = 5U;
        ASSERT_EQ(nx_stm32_audio_prepare(&audio, &audio_config), NX_SUCCESS);
    }
    void flag(uint8_t lane, uint32_t flags) {
        if (lane < 4U)
            hardware.dma.LISR |= flags << kLaneShift[lane];
        else
            hardware.dma.HISR |= flags << kLaneShift[lane];
    }
    void captured(size_t remaining = 0U) {
        hardware.dcmi.CR &= ~static_cast<uint32_t>(DCMI_CR_CAPTURE);
        hardware.dcmi.SR = 0U;
        hardware.dcmi.RISR = DCMI_RISR_FRAME_RIS;
        hardware.streams[capture_config.memory.lane].NDTR =
            static_cast<uint32_t>(remaining);
        hardware.streams[capture_config.memory.lane].FCR |= DMA_SxFCR_FS_2;
    }
    void tx_prepare() {
        layout(&frame, &stream, 16U);
        nx_stream_fill_t fill;
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_publish(&frame, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&frame, &tx_loan), NX_SUCCESS);
        audio_request.tx = &tx_loan;
    }
    void tx_complete() {
        hardware.streams[audio_config.tx.lane].NDTR = 0U;
        hardware.streams[audio_config.tx.lane].CR &=
            ~static_cast<uint32_t>(DMA_SxCR_EN);
        hardware.spi.SR = SPI_SR_TXE;
        hardware.extension.SR = SPI_SR_TXE;
        flag(audio_config.tx.lane, 0x20U);
    }
};

TEST_F(CaptureAudio, RawSnapshotRequiresBothFrameAndExactStorage) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    hardware.streams[1].NDTR = 0U;
    flag(1U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    captured();
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(snapshot.base.transferred, 256U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(hardware.streams[1].M0AR, 0U);
    EXPECT_EQ(hardware.enabled[DCMI_IRQn], 0U);
    EXPECT_EQ(hardware.enabled[57], 0U);
}
TEST_F(CaptureAudio, EmptyCropFrameCannotMakeRawImageComplete) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured(64U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_IO);
    EXPECT_EQ(snapshot.base.result, NX_ERROR_IO);
    EXPECT_EQ(snapshot.base.transferred, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, JpegShortImageNeedsNoDmaTcAndReturnsWordPrefix) {
    capture_config.jpeg = true;
    snapshot.raw_frame = nullptr;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured(59U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    EXPECT_EQ(snapshot.base.transferred, 20U);
    EXPECT_EQ(slots[0].length, 20U);
}
TEST_F(CaptureAudio, JpegFrameWaitsForSourceFifoBusAndBothIrqs) {
    capture_config.jpeg = true;
    snapshot.raw_frame = nullptr;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured(60U);
    hardware.dcmi.SR = DCMI_SR_FNE;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_QUARANTINED);
    hardware.dcmi.SR = 0U;
    hardware.active[57] = 1U;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    hardware.active[57] = 0U;
    hardware.active[DCMI_IRQn] = 1U;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    hardware.active[DCMI_IRQn] = 0U;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    EXPECT_EQ(snapshot.base.transferred, 16U);
}
TEST_F(CaptureAudio, CancelStoppedSensorRetainsBorrowUntilExclusiveRecovery) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    hardware.hold_capture = true;
    EXPECT_EQ(nx_stm32_capture_ops.cancel(&capture, &snapshot), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_capture_close(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    hardware.hold_capture = false;
    EXPECT_EQ(nx_stm32_capture_ops.recover(&capture), NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(hardware.dma_resets, 0U);
    EXPECT_GT(hardware.resets, 0U);
}
TEST_F(CaptureAudio, FailedDmaStopAndAddressDetachKeepExactBorrow) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    hardware.hold_dma = true;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_NE(capture.active, nullptr);
    hardware.hold_dma = false;
    hardware.hold_address = true;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    hardware.hold_address = false;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
}
TEST_F(CaptureAudio, DmaAndSourceErrorsPublishNoImage) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    flag(1U, 0x08U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_IO);
    EXPECT_EQ(snapshot.base.transferred, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, EmbeddedUnmasksAllCodesAndDoesNotOwnSyncPads) {
    capture_config.embedded = true;
    capture_config.common.pin_count = 9U;
    capture_config.codes[0] = 0xabU;
    capture_config.codes[1] = 0x80U;
    capture_config.codes[2] = 0x9dU;
    capture_config.codes[3] = 0xb6U;
    capture_open();
    EXPECT_EQ(hardware.dcmi.ESUR, UINT32_MAX);
    EXPECT_EQ(hardware.dcmi.ESCR, 0xb69d80abU);
    EXPECT_EQ(hardware.dcmi.IER, 0U);
    EXPECT_EQ((hardware.gpio.MODER >> 18U) & 3U, 0U);
}
TEST_F(CaptureAudio, EmbeddedUnmaskAndHalInterruptMaskReadbackAreRequired) {
    capture_config.embedded = true;
    capture_config.common.pin_count = 9U;
    hardware.hold_esur = true;
    EXPECT_EQ(nx_stm32_capture_prepare(&capture, &capture_config), NX_ERROR_IO);
    EXPECT_TRUE(capture.owned);
    EXPECT_FALSE(capture.initialized);
    EXPECT_EQ(nx_stm32_capture_close(&capture), NX_SUCCESS);
}
TEST_F(CaptureAudio, CropStoresInputClockCountsWithoutSubtractingOne) {
    capture_config.crop = {3U, 5U, 0U, 0U, true};
    capture_open();
    EXPECT_EQ(hardware.dcmi.CWSTRTR, 3U | (5U << 16U));
    EXPECT_EQ(hardware.dcmi.CWSIZER, 0U);
    EXPECT_NE(hardware.dcmi.CR & DCMI_CR_CRE, 0U);
}
TEST_F(CaptureAudio, HighBitSamplesUseSixteenBitContainersAndSnapshotNoFcrc) {
    capture_config.bits = 12U;
    capture_config.common.pin_count = 15U;
    capture_open();
    EXPECT_EQ(hardware.dcmi.CR & (DCMI_CR_EDM_0 | DCMI_CR_EDM_1),
              DCMI_CR_EDM_1);
    EXPECT_EQ(hardware.dcmi.CR & (DCMI_CR_FCRC_0 | DCMI_CR_FCRC_1), 0U);
    ASSERT_EQ(nx_stm32_capture_close(&capture), NX_SUCCESS);
    capture_config.frame_divisor = 2U;
    EXPECT_EQ(nx_stm32_capture_prepare(&capture, &capture_config),
              NX_ERROR_INVALID);
}
TEST_F(CaptureAudio, DmaStreamSevenUsesOwnFlagsAndKeepsNeighbor) {
    capture_config.memory = dma(7U, 1U);
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    hardware.dma.HISR = 0x20U;
    captured();
    flag(7U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    EXPECT_EQ(hardware.dma.HISR, 0x20U);
}
TEST_F(CaptureAudio, ContinuousPublishesOnlyDetachedStorageBlocks) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    hardware.streams[1].CR |= DMA_SxCR_CT;
    flag(1U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(slots[0].length, 256U);
    EXPECT_EQ(slots[0].flags & NX_STREAM_BOUNDARY_TRIGGER, 0U);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[2].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(hardware.streams[1].M0AR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pool[2])));
}
TEST_F(CaptureAudio, AmbiguousCtNeverPublishesOldOrReplacementTarget) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    flag(1U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_OVERFLOW);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.losses, 1U);
}
TEST_F(CaptureAudio, ReplacementBankRaceRetainsAllThreeUntilActualDmaDrain) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    hardware.streams[1].CR |= DMA_SxCR_CT;
    flag(1U, 0x20U);
    hardware.race_bank = true;
    hardware.hold_dma = true;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(capture.posted.pending, 3U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[2].state, NX_STREAM_SLOT_FILLING);
    hardware.hold_dma = false;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_OVERFLOW);
    EXPECT_EQ(stream.outstanding, 0U);
}
TEST_F(CaptureAudio, NoReplacementStopsWithoutOverwritingConsumerBlocks) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    for (unsigned i = 0U; i < 3U; ++i) {
        hardware.streams[1].CR ^= DMA_SxCR_CT;
        flag(1U, 0x20U);
        ASSERT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    }
    hardware.streams[1].CR ^= DMA_SxCR_CT;
    flag(1U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_OVERFLOW);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(slots[3].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(slots[4].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, MissedObservationBudgetStopsEvenWithPlausibleCt) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    hardware.now = 11U;
    hardware.streams[1].CR |= DMA_SxCR_CT;
    flag(1U, 0x20U);
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_OVERFLOW);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, LateCaptureIrqCannotTouchSettledRequest) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    ASSERT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    nx_capture_request_t before = snapshot;
    hardware.dcmi.RISR = DCMI_RISR_OVR_RIS | DCMI_RISR_ERR_RIS;
    flag(1U, 0x08U);
    nx_stm32_capture_irq(&capture);
    nx_stm32_capture_dma_irq(&capture);
    EXPECT_EQ(std::memcmp(&snapshot, &before, sizeof(before)), 0);
    EXPECT_EQ(capture.active, nullptr);
}
TEST_F(CaptureAudio, AudioSlaveTxRequiresRealPrefillBeforeActive) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUEUED);
    hardware.now = 3U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_ACTIVE);
    EXPECT_NE(tx_loan.owner, nullptr);
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(tx_loan.owner, nullptr);
    EXPECT_EQ(audio_request.base.transferred, 16U);
}
TEST_F(CaptureAudio, AudioCancelPrefillKeepsLoanUntilWireAndIrqsDetach) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_audio_ops.cancel(&audio, &audio_request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUARANTINED);
    hardware.spi.SR = SPI_SR_TXE;
    hardware.active[SPI2_IRQn] = 1U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    hardware.active[SPI2_IRQn] = 0U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_CANCELLED);
    EXPECT_EQ(tx_loan.owner, nullptr);
    EXPECT_EQ(audio_request.base.transferred, 0U);
}
TEST_F(CaptureAudio, AudioSourceErrorIsLatchedBeforeDestructiveStatusRead) {
    audio_open();
    tx_prepare();
    hardware.spi.SR |= SPI_SR_UDR;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_IO);
    EXPECT_EQ(audio_request.base.result, NX_ERROR_IO);
    EXPECT_EQ(audio_request.base.transferred, 0U);
}
TEST_F(CaptureAudio, AudioSlaveReceivePublishesAfterMemoryAndSourceDetach) {
    audio_config.receive = true;
    audio_config.tx = {};
    audio_config.rx = dma(3U, 0U);
    audio_request.rx = &rx_frame;
    audio_open();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.streams[3].NDTR = 0U;
    flag(3U, 0x20U);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_READY);
    EXPECT_EQ(rx_slots[0].length, 16U);
    EXPECT_EQ(hardware.streams[3].M0AR, 0U);
}
TEST_F(CaptureAudio, AudioDuplexWaitsForBothDirectionsAndAbortsRxOnError) {
    audio_config.duplex = true;
    audio_config.common.pin_count = 4U;
    audio_config.rx = dma(3U, 3U);
    audio_open();
    tx_prepare();
    audio_request.rx = &rx_frame;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_FILLING);
    flag(3U, 0x08U);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_IO);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, AudioDmaDetachFailurePreservesBothDuplexLoans) {
    audio_config.duplex = true;
    audio_config.common.pin_count = 4U;
    audio_config.rx = dma(3U, 3U);
    audio_open();
    tx_prepare();
    audio_request.rx = &rx_frame;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.hold_dma = true;
    EXPECT_EQ(nx_stm32_audio_ops.cancel(&audio, &audio_request), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_FILLING);
    hardware.hold_dma = false;
    hardware.spi.SR = SPI_SR_TXE;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_CANCELLED);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, AudioMasterPlli2sAndMclkPreserveMainPllAndSharedDma) {
    audio_config.master = true;
    audio_config.mclk = true;
    audio_config.divider = 4U;
    audio_config.common.pin_count = 4U;
    uint32_t main_pll = hardware.rcc.PLLCFGR;
    audio_open();
    EXPECT_NE(hardware.rcc.CR & RCC_CR_PLLI2SRDY, 0U);
    EXPECT_NE(hardware.spi.I2SPR & SPI_I2SPR_MCKOE, 0U);
    EXPECT_EQ(hardware.rcc.PLLCFGR, main_pll);
    nx_audio_rate_t rate;
    EXPECT_EQ(nx_stm32_audio_ops.rate(&audio, &rate), NX_SUCCESS);
    EXPECT_EQ(rate.numerator_hz, 46875U);
    EXPECT_EQ(rate.denominator, 1U);
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.CR & RCC_CR_PLLI2SON, 0U);
    EXPECT_NE(hardware.rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    EXPECT_EQ(hardware.dma_resets, 0U);
}
TEST_F(CaptureAudio,
       AudioCompatibleSharedPlli2sLivesUntilLastControllerCloses) {
    audio_config.master = true;
    audio_open();
    nx_stm32_audio_config_t second = audio_config;
    second.controller = 3U;
    second.registers = &hardware.spi3;
    second.tx = dma(5U, 0U);
    nx_stm32_audio_state_t other = {};
    ASSERT_EQ(nx_stm32_audio_prepare(&other, &second), NX_SUCCESS);
    EXPECT_EQ(clock.users, 3U);
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_SUCCESS);
    EXPECT_NE(hardware.rcc.CR & RCC_CR_PLLI2SON, 0U);
    EXPECT_EQ(clock.users, 2U);
    EXPECT_EQ(nx_stm32_audio_close(&other), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.CR & RCC_CR_PLLI2SON, 0U);
}
TEST_F(CaptureAudio, AudioIncompatiblePlli2sRejectedWithoutHardwareWrites) {
    audio_config.master = true;
    audio_open();
    nx_stm32_audio_config_t second = audio_config;
    second.controller = 3U;
    second.registers = &hardware.spi3;
    second.tx = dma(5U, 0U);
    second.pll_n = 256U;
    calculate_rate(&second);
    nx_stm32_audio_state_t other = {};
    unsigned before = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_prepare(&other, &second), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.writes, before);
    EXPECT_FALSE(other.owned);
}
TEST_F(CaptureAudio, AudioMasterReceiveDoesNotUseAlwaysLowBusyAsCompletion) {
    audio_config.master = true;
    audio_config.receive = true;
    audio_config.tx = {};
    audio_config.rx = dma(3U, 0U);
    audio_request.rx = &rx_frame;
    audio_open();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(hardware.streams[3].NDTR, 7U);
    hardware.streams[3].NDTR = 0U;
    hardware.master_tail = true;
    flag(3U, 0x20U);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_TRUE(audio.terminal_stopped);
    EXPECT_GE(hardware.cycles, 100U);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_READY);
    uint16_t tail;
    std::memcpy(&tail, rx_pool[0] + 14U, sizeof(tail));
    EXPECT_EQ(tail, 0x5a3cU);
}
TEST_F(CaptureAudio, AudioFormatUsesHalfwordsAndHardwareSlotPadding) {
    audio_config.sample_bits = 24U;
    audio_config.slot_bits = 32U;
    audio_config.standard = 2U;
    hardware.gpio.IDR &= ~(1U << 12U);
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(hardware.spi.I2SCFGR & SPI_I2SCFGR_DATLEN, SPI_I2SCFGR_DATLEN_0);
    EXPECT_NE(hardware.spi.I2SCFGR & SPI_I2SCFGR_CHLEN, 0U);
    EXPECT_EQ(hardware.streams[4].NDTR, 7U);
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
}
TEST_F(CaptureAudio, AudioLateIrqAndOtherControllerCannotChangeSettledRequest) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    nx_audio_request_t before = audio_request;
    flag(4U, 0x08U);
    hardware.spi.SR = SPI_SR_UDR;
    nx_stm32_audio_dma_irq(&audio, false);
    nx_stm32_audio_irq(&audio);
    EXPECT_EQ(std::memcmp(&before, &audio_request, sizeof(before)), 0);
}
TEST_F(CaptureAudio, CaptureDeadlineIncludesFinalFifoAndBusWithdrawal) {
    capture_config.jpeg = true;
    snapshot.raw_frame = nullptr;
    snapshot.base.deadline = 1U;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured(60U);
    hardware.dcmi.SR = DCMI_SR_FNE;
    hardware.advance_time = true;
    hardware.fifo_polls = 2U;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_TIMEOUT);
    EXPECT_EQ(snapshot.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(snapshot.base.transferred, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
}
TEST_F(CaptureAudio, AudioDeadlineIncludesFinalSerializerAndIrqWithdrawal) {
    audio_open();
    tx_prepare();
    audio_request.base.deadline = 1U;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    hardware.spi.SR = SPI_SR_BSY;
    hardware.advance_time = true;
    hardware.wire_polls = 2U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_TIMEOUT);
    EXPECT_EQ(audio_request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(audio_request.base.transferred, 0U);
    EXPECT_EQ(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, AudioRateCannotOverwriteAnAcceptedImmutableFrame) {
    audio_open();
    tx_prepare();
    pool[0][0] = 0x5aU;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_audio_ops.rate(
                  &audio, reinterpret_cast<nx_audio_rate_t*>(pool[0])),
              NX_ERROR_INVALID);
    EXPECT_EQ(pool[0][0], 0x5aU);
    EXPECT_NE(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, CaptureCannotBorrowAcrossARecreatedSystemEpoch) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_NE(nx_stm32_capture_close(&capture), NX_SUCCESS);
}
TEST_F(CaptureAudio, AudioCannotBorrowAcrossARecreatedSystemEpoch) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_NE(nx_stm32_audio_close(&audio), NX_SUCCESS);
}
TEST_F(CaptureAudio, MasterRateMustMatchTheActualPllAndPrescaler) {
    audio_config.master = true;
    unsigned before = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_prepare(&audio, &audio_config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, before);
    EXPECT_FALSE(audio.owned);
}
TEST_F(CaptureAudio, RejectedRawLayoutNeverCreatesARequestOrProducerBorrow) {
    capture_open();
    frame.layout.planes[0].row_bytes = 0U;
    frame.layout.planes[0].stride = 0U;
    unsigned before = hardware.writes;
    EXPECT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_READY);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(hardware.writes, before);
}
TEST_F(CaptureAudio, RejectedDuplexKeepsNoProviderSourceReference) {
    audio_config.duplex = true;
    audio_config.common.pin_count = 4U;
    audio_config.rx = dma(3U, 3U);
    audio_open();
    tx_prepare();
    audio_request.rx = &rx_frame;
    rx_frame.layout.bytes = 24U;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    EXPECT_EQ(audio.active, nullptr);
    EXPECT_EQ(audio.transmit.data, nullptr);
    EXPECT_NE(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, SettledAudioDetachesEveryProviderPayloadReference) {
    audio_config.duplex = true;
    audio_config.common.pin_count = 4U;
    audio_config.rx = dma(3U, 3U);
    audio_open();
    tx_prepare();
    audio_request.rx = &rx_frame;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    hardware.streams[3].NDTR = 0U;
    flag(3U, 0x20U);
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(audio.active, nullptr);
    EXPECT_EQ(audio.transmit.data, nullptr);
    EXPECT_EQ(audio.receive.data, nullptr);
}
TEST_F(CaptureAudio, SettledCaptureDetachesPostedAndTargetReferences) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    ASSERT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    EXPECT_EQ(capture.active, nullptr);
    EXPECT_EQ(capture.stream, nullptr);
    EXPECT_EQ(capture.posted.stream, nullptr);
    EXPECT_EQ(capture.targets[0].data, nullptr);
    EXPECT_EQ(capture.replacement.data, nullptr);
}
TEST_F(CaptureAudio, FailedReceiveReservationStillReturnsAnAcceptedTxLoan) {
    audio_config.duplex = true;
    audio_config.common.pin_count = 4U;
    audio_config.rx = dma(3U, 3U);
    audio_open();
    tx_prepare();
    audio_request.rx = &rx_frame;
    rx_stream.epoch = UINT64_MAX;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    if (nx_request_state(&audio_request.base) != NX_REQUEST_SETTLED)
        (void)nx_stm32_audio_ops.service(&audio);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(audio_request.base.result, NX_ERROR_EXHAUSTED);
    EXPECT_EQ(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, CaptureRestoresExactPriorIrqPrioritiesAtPlatformClose) {
    hardware.priority[DCMI_IRQn] = 3U;
    hardware.priority[57] = 4U;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[DCMI_IRQn], 5U);
    EXPECT_EQ(hardware.priority[57], 5U);
    captured();
    ASSERT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_capture_close(&capture), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[DCMI_IRQn], 3U);
    EXPECT_EQ(hardware.priority[57], 4U);
}
TEST_F(CaptureAudio, AudioRestoresExactPriorIrqPrioritiesAtPlatformClose) {
    hardware.priority[SPI2_IRQn] = 1U;
    hardware.priority[audio_config.tx.irq] = 2U;
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[SPI2_IRQn], 5U);
    tx_complete();
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_audio_close(&audio), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[SPI2_IRQn], 1U);
    EXPECT_EQ(hardware.priority[audio_config.tx.irq], 2U);
}
TEST_F(CaptureAudio, MasterCannotRedirectAForeignExternalClockMux) {
    audio_config.master = true;
    audio_config.divider = 31U;
    audio_config.actual_rate = {1500000U, 31U};
    hardware.rcc.CFGR |= RCC_CFGR_I2SSRC;
    unsigned before = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_prepare(&audio, &audio_config), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.writes, before);
    EXPECT_FALSE(audio.owned);
}
TEST_F(CaptureAudio, HalInitInterruptEnablesMustBeActuallyMaskedAndReadBack) {
    hardware.hold_ier = true;
    EXPECT_EQ(nx_stm32_capture_prepare(&capture, &capture_config), NX_ERROR_IO);
    EXPECT_TRUE(capture.owned);
    EXPECT_NE(hardware.dcmi.IER, 0U);
    EXPECT_EQ(nx_stm32_capture_close(&capture), NX_ERROR_BUSY);
    EXPECT_TRUE(capture.owned);
    hardware.hold_ier = false;
    EXPECT_EQ(nx_stm32_capture_close(&capture), NX_SUCCESS);
}
TEST_F(CaptureAudio, SnapshotCannotSettleWhileDmaFifoStillOwnsTheBus) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    hardware.hold_fifo = true;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_QUARANTINED);
    hardware.hold_fifo = false;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
}
TEST_F(CaptureAudio, SlaveReceiverKeepsFillWhileShiftRegisterIsBusy) {
    audio_config.receive = true;
    audio_config.tx = {};
    audio_config.rx = dma(3U, 0U);
    audio_request.rx = &rx_frame;
    audio_open();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.streams[3].NDTR = 0U;
    hardware.spi.SR = SPI_SR_BSY;
    flag(3U, 0x20U);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_EQ(rx_slots[0].state, NX_STREAM_SLOT_FILLING);
    hardware.spi.SR = 0U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
}
TEST_F(CaptureAudio, FailedLastPlli2sReleaseKeepsItsClockAndGateReceipts) {
    audio_config.master = true;
    audio_open();
    hardware.hold_pll = true;
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_ERROR_IO);
    EXPECT_TRUE(audio.owned);
    EXPECT_TRUE(audio.clock_user);
    EXPECT_EQ(clock.users, 1U);
    EXPECT_NE(hardware.rcc.APB1ENR & RCC_APB1ENR_SPI2EN, 0U);
    hardware.hold_pll = false;
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_SUCCESS);
    EXPECT_FALSE(audio.clock_user);
    EXPECT_EQ(clock.users, 0U);
}
TEST_F(CaptureAudio, FailedCaptureCloseRevokesFurtherAdmission) {
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot), NX_SUCCESS);
    captured();
    ASSERT_EQ(nx_stm32_capture_ops.service(&capture), NX_SUCCESS);
    nx_stream_block_t block;
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    hardware.hold_priority_restore = true;
    ASSERT_EQ(nx_stm32_capture_close(&capture), NX_ERROR_IO);
    ASSERT_EQ(nx_request_prepare(&snapshot.base, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32_capture_ops.snapshot(&capture, &snapshot),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&snapshot.base), NX_REQUEST_READY);
    EXPECT_EQ(stream.outstanding, 0U);
}
TEST_F(CaptureAudio, FailedAudioCloseRevokesFurtherAdmission) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    hardware.hold_priority_restore = true;
    ASSERT_EQ(nx_stm32_audio_close(&audio), NX_ERROR_IO);
    tx_prepare();
    ASSERT_EQ(nx_request_prepare(&audio_request.base, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    EXPECT_NE(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, OverrunDuringTargetReplacementCannotPublishAQuietBlock) {
    capture_config.continuous = true;
    capture_open();
    ASSERT_EQ(nx_stm32_capture_ops.start_blocks(&capture, &stream), NX_SUCCESS);
    hardware.streams[1].CR |= DMA_SxCR_CT;
    flag(1U, 0x20U);
    hardware.inject_capture_error = true;
    EXPECT_EQ(nx_stm32_capture_ops.service(&capture), NX_ERROR_IO);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(slots[2].state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.losses, 1U);
}
TEST_F(CaptureAudio, SlaveTxWaitsForThreeActualMicroticksAfterEnable) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    for (nx_time_us_t tick : {0U, 1U, 2U}) {
        hardware.now = tick;
        EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUEUED);
    }
    hardware.now = 3U;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_ACTIVE);
}
TEST_F(CaptureAudio, SlaveWsLostDuringPrefillNeverAuthorizesExternalClocks) {
    audio_open();
    tx_prepare();
    hardware.lose_ws_after_prefill = true;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.now = 3U;
    (void)nx_stm32_audio_ops.service(&audio);
    EXPECT_NE(nx_request_state(&audio_request.base), NX_REQUEST_ACTIVE);
    EXPECT_NE(tx_loan.owner, nullptr);
    hardware.spi.SR = SPI_SR_TXE;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_IO);
    EXPECT_EQ(audio_request.base.transferred, 0U);
}
TEST_F(CaptureAudio, NeverEnabledSerializerFailureNeedsNoInventedTxe) {
    audio_open();
    tx_prepare();
    hardware.spi.SR = 0U;
    hardware.refuse_txdma_enable = true;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    for (unsigned i = 0U;
         i < 3U && nx_request_state(&audio_request.base) != NX_REQUEST_SETTLED;
         ++i)
        (void)nx_stm32_audio_ops.service(&audio);
    EXPECT_EQ(hardware.enable_attempts, 0U);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(audio_request.base.result, NX_ERROR_IO);
    EXPECT_EQ(tx_loan.owner, nullptr);
    EXPECT_EQ(hardware.streams[4].M0AR, 0U);
}
TEST_F(CaptureAudio, ProvedWireDrainSurvivesDisabledTxeAndAddressRetry) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    hardware.clear_txe_on_disable = true;
    hardware.hold_address = true;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_EQ(hardware.spi.SR & SPI_SR_TXE, 0U);
    hardware.hold_address = false;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, ObservedTimebaseResetNeverBecomesARecoveredOldLoan) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.rcc.APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_STATE);
    hardware.rcc.APB1RSTR &= ~static_cast<uint32_t>(RCC_APB1RSTR_TIM2RST);
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, EveryRejectedCpuContextPreservesAllIncomingMasks) {
    audio_open();
    tx_prepare();
    unsigned writes = hardware.writes;
    for (unsigned context = 0U; context < 5U; ++context) {
        hardware.privileged = context != 0U;
        hardware.in_isr = context == 1U;
        hardware.masks[0] = context == 2U ? 1U : 0U;
        hardware.masks[1] = context == 3U ? 32U : 0U;
        hardware.masks[2] = context == 4U ? 1U : 0U;
        uint32_t incoming[3];
        std::memcpy(incoming, hardware.masks, sizeof(incoming));
        EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
                  NX_ERROR_CONTEXT);
        EXPECT_EQ(std::memcmp(incoming, hardware.masks, sizeof(incoming)), 0);
        EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
        EXPECT_EQ(hardware.writes, writes);
    }
}
TEST_F(CaptureAudio, CopiedAndStaleConsumerControlsRejectWithoutAnotherBorrow) {
    audio_open();
    tx_prepare();
    nx_frame_dma_loan_t previous = tx_loan;
    nx_frame_dma_loan_t copy = tx_loan;
    audio_request.tx = &copy;
    EXPECT_NE(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    const nx_dma_drain_facts_t proof = {16U, 0U, true, true, true};
    ASSERT_EQ(nx_frame_dma_release(&frame, &tx_loan, &proof), NX_SUCCESS);
    tx_loan = previous;
    audio_request.tx = &tx_loan;
    EXPECT_NE(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    EXPECT_EQ(stream.outstanding, 0U);
}
TEST_F(CaptureAudio, PlatformCloseCannotDiscardAudioClockHistoryLoss) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    hardware.rcc.AHB1RSTR = RCC_AHB1RSTR_DMA1RST;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_STATE);
    hardware.rcc.AHB1RSTR = 0U;
    tx_complete();
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_TRUE(audio.owned);
}
TEST_F(CaptureAudio, RejectsMalformedWholeLoanGeometryWithoutAdmission) {
    audio_open();
    tx_prepare();
    const nx_frame_layout_t original = frame.layout;
    const unsigned writes = hardware.writes;
    for (unsigned geometry = 0U; geometry < 5U; ++geometry) {
        SCOPED_TRACE(geometry);
        frame.layout = original;
        auto& plane = frame.layout.planes[0];
        if (geometry == 0U)
            plane.rows = 0U;
        else if (geometry == 1U)
            plane.row_bytes = plane.stride = 0U;
        else if (geometry == 2U) {
            plane.row_bytes = plane.stride = SIZE_MAX / 2U + 9U;
            plane.rows = 2U;
        } else if (geometry == 3U) {
            plane.row_bytes = plane.stride = 4U;
            plane.rows = 2U;
        } else {
            plane.row_bytes = 4U;
            plane.stride = 8U;
            plane.rows = 2U;
        }
        EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
                  NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
        EXPECT_EQ(hardware.writes, writes);
        EXPECT_EQ(tx_loan.owner, &frame);
    }
}
TEST_F(CaptureAudio, CanonicalAudioRowsKeepTheWholeConsumerLoan) {
    audio_open();
    const nx_frame_audio_format_t format = {NX_FRAME_AUDIO_I2S,
                                            NX_FRAME_PCM_SIGNED,
                                            NX_FRAME_SAMPLE_LSB,
                                            2U,
                                            3U,
                                            2U,
                                            16U,
                                            16U,
                                            false};
    nx_frame_layout_t checked;
    ASSERT_EQ(nx_frame_layout_audio(&format, &checked), NX_SUCCESS);
    ASSERT_EQ(checked.planes[0].rows, 2U);
    ASSERT_EQ(nx_frame_stream_initialize(&frame, &stream, &checked),
              NX_SUCCESS);
    nx_stream_fill_t fill;
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&frame, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&frame, &tx_loan), NX_SUCCESS);
    audio_request.tx = &tx_loan;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_EQ(hardware.streams[4].NDTR, 3U);
    tx_complete();
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(audio_request.base.transferred, 8U);
    EXPECT_EQ(tx_loan.owner, nullptr);
}
TEST_F(CaptureAudio, StereoCounterBoundAcceptsTheLargestWholeBlock) {
    audio_open();
    alignas(8) static uint8_t large[131072];
    regions[0] = {reinterpret_cast<uintptr_t>(large), sizeof(large),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    for (size_t bytes : {131070U, 131072U, 131068U}) {
        slots[0] = {};
        slots[0].data = large;
        slots[0].capacity = sizeof(large);
        stream = {};
        frame = {};
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 1U), NX_SUCCESS);
        layout(&frame, &stream, bytes);
        nx_stream_fill_t fill;
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_publish(&frame, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&frame, &tx_loan), NX_SUCCESS);
        audio_request.tx = &tx_loan;
        if (bytes == 131068U) {
            ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
                      NX_SUCCESS);
            EXPECT_EQ(hardware.streams[4].NDTR, 65533U);
            tx_complete();
            EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
            EXPECT_EQ(audio_request.base.transferred, bytes);
        } else {
            const unsigned writes = hardware.writes;
            EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
                      NX_ERROR_INVALID);
            EXPECT_EQ(hardware.writes, writes);
            const nx_dma_drain_facts_t proof = {bytes / 2U, 0U, true, true,
                                                true};
            ASSERT_EQ(nx_frame_dma_release(&frame, &tx_loan, &proof),
                      NX_SUCCESS);
        }
    }
}
TEST_F(CaptureAudio, RequestInsideUnusedPoolCapacityCannotBeBorrowed) {
    audio_open();
    tx_prepare();
    auto* alias = ::new (pool[0] + 128U) nx_audio_request_t{};
    static_assert(sizeof(*alias) <= 128U, "Control fits unused capacity");
    nx_request_initialize(&alias->base);
    ASSERT_EQ(nx_request_prepare(&alias->base, NX_DEADLINE_NEVER), NX_SUCCESS);
    alias->tx = &tx_loan;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, alias), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&alias->base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(tx_loan.owner, &frame);
}
TEST_F(CaptureAudio, DmaReadPermissionRejectsCpuOnlyStorageBeforePrefill) {
    audio_open();
    tx_prepare();
    regions[0].permissions = NX_DMA_MEMORY_WRITE;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
}
TEST_F(CaptureAudio, UnconfirmedSerializerEnableStillRequiresWireDrain) {
    audio_open();
    tx_prepare();
    hardware.spi.SR = 0U;
    hardware.refuse_i2se_enable = true;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    EXPECT_GT(hardware.enable_attempts, 0U);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(tx_loan.owner, nullptr);
    EXPECT_NE(nx_stm32_audio_ops.stop(&audio), NX_SUCCESS);
}
TEST_F(CaptureAudio, BusyMustBeObservedAfterTheFreshTxeObservation) {
    audio_open();
    tx_prepare();
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request), NX_SUCCESS);
    tx_complete();
    hardware.busy_on_txe_read = true;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_ERROR_BUSY);
    EXPECT_NE(tx_loan.owner, nullptr);
    hardware.busy_on_txe_read = false;
    hardware.spi.SR = SPI_SR_TXE;
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
}
TEST_F(CaptureAudio, UnalignedConsumerControlRejectsBeforeDereference) {
    audio_open();
    tx_prepare();
    alignas(8) uint8_t bytes[sizeof(tx_loan) + 8U];
    auto* misaligned = reinterpret_cast<nx_frame_dma_loan_t*>(bytes + 1U);
    nx_frame_dma_loan_t copy = tx_loan;
    copy.identity = misaligned;
    std::memcpy(bytes + 1U, &copy, sizeof(copy));
    audio_request.tx = misaligned;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
}
TEST_F(CaptureAudio, SettledAudioControlsCanBecomeInaccessibleBeforeLateIrqs) {
    audio_open();
    struct Controls {
        nx_audio_request_t request;
        nx_frame_dma_loan_t loan;
    };
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);
    auto* isolated = ::new (page) Controls{};
    layout(&frame, &stream, 16U);
    nx_stream_fill_t fill;
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&frame, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&frame, &isolated->loan), NX_SUCCESS);
    nx_request_initialize(&isolated->request.base);
    ASSERT_EQ(nx_request_prepare(&isolated->request.base, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    isolated->request.tx = &isolated->loan;
    ASSERT_EQ(nx_stm32_audio_ops.submit(&audio, &isolated->request),
              NX_SUCCESS);
    tx_complete();
    ASSERT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    ASSERT_EQ(mprotect(page, 4096U, PROT_NONE), 0);
    nx_stm32_audio_irq(&audio);
    nx_stm32_audio_dma_irq(&audio, false);
    EXPECT_EQ(nx_stm32_audio_ops.service(&audio), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_audio_close(&audio), NX_SUCCESS);
    EXPECT_EQ(munmap(page, 4096U), 0);
}
TEST_F(CaptureAudio, ReceiveRowsCannotDescribeOnlyHalfTheStoredFrame) {
    audio_config.receive = true;
    audio_config.tx = {};
    audio_config.rx = dma(3U, 0U);
    audio_open();
    audio_request.rx = &rx_frame;
    rx_frame.layout.planes[0] = {0U, 4U, 4U, 2U};
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    EXPECT_EQ(rx_stream.outstanding, 0U);
    EXPECT_EQ(hardware.writes, writes);
}
TEST_F(CaptureAudio, ReceiveRowExtentCannotWrapToAPlausibleStorageLength) {
    audio_config.receive = true;
    audio_config.tx = {};
    audio_config.rx = dma(3U, 0U);
    audio_open();
    audio_request.rx = &rx_frame;
    rx_frame.layout.planes[0] = {0U, SIZE_MAX / 2U + 9U, SIZE_MAX / 2U + 9U,
                                 2U};
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_audio_ops.submit(&audio, &audio_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&audio_request.base), NX_REQUEST_READY);
    EXPECT_EQ(rx_stream.outstanding, 0U);
    EXPECT_EQ(hardware.writes, writes);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (model->hold_fifo)
        for (auto& dma : model->streams)
            if (reg == &dma.FCR)
                value &= ~static_cast<uint32_t>(DMA_SxFCR_FS);
    for (SPI_TypeDef* bus : {&model->spi, &model->extension, &model->spi3}) {
        if (reg == &bus->SR) {
            if (bus == &model->spi && model->busy_on_txe_read &&
                (value & SPI_SR_TXE) != 0U)
                bus->SR |= SPI_SR_BSY;
            bus->SR &= ~static_cast<uint32_t>(SPI_SR_UDR | SPI_SR_FRE);
            if (model->read_dr)
                bus->SR &= ~static_cast<uint32_t>(SPI_SR_OVR);
        }
        if (reg == &bus->DR) {
            bus->SR &= ~static_cast<uint32_t>(SPI_SR_RXNE);
            model->read_dr = true;
        }
    }
    return value;
}
extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++model->writes;
    if (reg == &model->dcmi.CR && model->hold_capture &&
        (value & DCMI_CR_CAPTURE) == 0U)
        value |= model->dcmi.CR & static_cast<uint32_t>(DCMI_CR_CAPTURE);
    if ((reg == &model->dcmi.ESUR && model->hold_esur) ||
        (reg == &model->dcmi.IER && model->hold_ier))
        return;
    for (size_t lane = 0U; lane < 8U; ++lane) {
        auto* dma = &model->streams[lane];
        if (reg == &dma->CR && model->hold_dma && (value & DMA_SxCR_EN) == 0U)
            value |= dma->CR & static_cast<uint32_t>(DMA_SxCR_EN);
        if (reg == &dma->FCR)
            value =
                (value & ~static_cast<uint32_t>(DMA_SxFCR_FS)) | DMA_SxFCR_FS_2;
        if (reg == &dma->M0AR || reg == &dma->M1AR || reg == &dma->PAR) {
            if (value == 0U && model->hold_address)
                return;
            if (reg == &dma->M0AR && (dma->CR & DMA_SxCR_EN) != 0U &&
                model->inject_capture_error) {
                model->dcmi.RISR |= DCMI_RISR_OVR_RIS;
                model->inject_capture_error = false;
            }
            if (reg == &dma->M0AR && (dma->CR & DMA_SxCR_EN) != 0U &&
                model->race_bank) {
                dma->CR ^= DMA_SxCR_CT;
                model->race_bank = false;
            }
        }
    }
    if (reg == &model->dma.LIFCR)
        model->dma.LISR &= ~value;
    if (reg == &model->dma.HIFCR)
        model->dma.HISR &= ~value;
    if (reg == &model->dcmi.ICR)
        model->dcmi.RISR &= ~value;
    if (reg == &model->rcc.AHB1RSTR &&
        (value & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)
        ++model->dma_resets;
    if (reg == &model->rcc.AHB2RSTR && (value & RCC_AHB2RSTR_DCMIRST) != 0U) {
        model->dcmi = {};
        ++model->resets;
    }
    if (reg == &model->spi.CR2 && model->refuse_txdma_enable)
        value &= ~static_cast<uint32_t>(SPI_CR2_TXDMAEN);
    if (reg == &model->spi.I2SCFGR) {
        if ((value & SPI_I2SCFGR_I2SE) != 0U) {
            ++model->enable_attempts;
            if (model->refuse_i2se_enable)
                value &= ~static_cast<uint32_t>(SPI_I2SCFGR_I2SE);
        } else if (model->clear_txe_on_disable)
            model->spi.SR &= ~static_cast<uint32_t>(SPI_SR_TXE | SPI_SR_BSY);
    }
    *reg = value;
    if (model->master_tail && reg == &model->spi.I2SCFGR &&
        (value & SPI_I2SCFGR_I2SE) == 0U) {
        model->spi.DR = 0x5a3cU;
        model->spi.SR |= SPI_SR_RXNE;
        model->master_tail = false;
    }
    if (model->automatic_prefill) {
        for (const auto& pair :
             {std::pair<SPI_TypeDef*, unsigned>(&model->spi, 4U),
              {&model->spi3, 5U}}) {
            auto* bus = pair.first;
            auto* dma = &model->streams[pair.second];
            if ((bus->I2SCFGR & SPI_I2SCFGR_I2SE) != 0U &&
                (bus->CR2 & SPI_CR2_TXDMAEN) != 0U &&
                (bus->SR & SPI_SR_TXE) != 0U && (dma->CR & DMA_SxCR_EN) != 0U &&
                dma->NDTR > 0U) {
                --dma->NDTR;
                bus->SR = SPI_SR_BSY;
                if (model->lose_ws_after_prefill)
                    model->gpio.IDR &= ~(1U << 12U);
            }
        }
    }
}
extern "C" void nx_av_model_poll(void) {
    if (model->advance_time)
        ++model->now;
    if (model->fifo_polls > 0U && --model->fifo_polls == 0U)
        model->dcmi.SR = 0U;
    if (model->wire_polls > 0U && --model->wire_polls == 0U)
        model->spi.SR = SPI_SR_TXE;
    model->cycles += 100U;
    if (!model->hold_pll) {
        if ((model->rcc.CR & RCC_CR_PLLI2SON) != 0U)
            model->rcc.CR |= RCC_CR_PLLI2SRDY;
        else
            model->rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLI2SRDY);
    }
}
extern "C" nx_time_us_t nx_av_model_now(void) {
    return model->now;
}
extern "C" uint32_t nx_av_model_cycles(void) {
    return model->cycles;
}
extern "C" uint32_t nx_av_model_irq_active(IRQn_Type irq) {
    return model->active[irq];
}
extern "C" uint32_t nx_av_model_irq_pending(IRQn_Type irq) {
    return model->pending[irq];
}
extern "C" uint32_t nx_av_model_irq_enabled(IRQn_Type irq) {
    return model->enabled[irq];
}
extern "C" void nx_av_model_irq_enable(IRQn_Type irq) {
    model->enabled[irq] = 1U;
}
extern "C" void nx_av_model_irq_disable(IRQn_Type irq) {
    if (!model->hold_irq)
        model->enabled[irq] = 0U;
}
extern "C" void nx_av_model_irq_clear(IRQn_Type irq) {
    if (!model->hold_irq)
        model->pending[irq] = 0U;
}
extern "C" void nx_av_model_irq_priority(IRQn_Type irq, uint32_t priority) {
    if (!model->hold_priority_restore || priority == 5U)
        model->priority[irq] = priority;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return model->priority[irq];
}

extern "C" bool nx_av_model_privileged(void) {
    return model->privileged;
}
extern "C" bool nx_av_model_in_isr(void) {
    return model->in_isr;
}
extern "C" bool nx_av_model_masked(void) {
    return (model->masks[0] | model->masks[1] | model->masks[2]) != 0U;
}
