/**
 * \file            stm32_sai_test.cpp
 * \brief           Real finite SAI ownership at external register/IRQ leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_sai_model.h"
#include "stm32f4_sai.h"
#include "stm32f4_sai_blocks.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" nx_result_t nx_stm32_sai_prepare(nx_stm32_sai_state_t*,
                                            const nx_stm32_sai_config_t*)
    __attribute__((weak));
extern "C" const nx_audio_ops_t nx_stm32_sai_ops __attribute__((weak));
#pragma weak nx_stm32_sai_admit
#pragma weak nx_stm32_sai_close
#pragma weak nx_stm32_sai_irq
#pragma weak nx_stm32_sai_dma_irq
#pragma weak nx_stm32_sai_blocks_prepare
#pragma weak nx_stm32_sai_blocks_ops
#pragma weak nx_stm32_sai_blocks_admit
#pragma weak nx_stm32_sai_blocks_close
#pragma weak nx_stm32_sai_blocks_irq
#pragma weak nx_stm32_sai_blocks_dma_irq
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
namespace {
constexpr uint32_t shifts[8] = {0U, 6U, 16U, 22U, 0U, 6U, 16U, 22U};
struct Hardware {
    uint32_t enabled[97]{}, pending[97]{}, active[97]{}, priority[97]{};
    nx_time_us_t now = 10U;
    bool serializer_held = false, dma_held = false, address_held = false;
    bool irq_held = false, prime = true, privileged = true, in_isr = false,
         masked = false;
    bool refuse_enable = false, refuse_error_mask = false, sticky_error = false;
    bool expire_on_irq_enable = false, expire_on_fifo_read = false;
    bool break_epoch_on_irq_enable = false, interrupt_during_now = false;
    bool break_epoch_on_fifo_read = false;
    bool refuse_replacement = false, rotate_during_replacement = false;
    unsigned fifo_reads_before_expiry = 0U;
    nx_stm32_sai_state_t* owner = nullptr;
    unsigned enables = 0U;
    unsigned enable_order[2]{};
    unsigned writes = 0U, disable_order[2]{}, disable_count = 0U;
    uint32_t initial_ndtr[8]{};
} * hardware;
class Sai : public testing::Test {
  protected:
    Hardware hw;
    std::array<uintptr_t, 6> pages = {0x40015000U, 0x40026000U, 0x40021000U,
                                      0x40023000U, 0x40000000U, 0x23000000U};
    std::array<size_t, 6> sizes = {0x1000U, 0x1000U, 0x1000U,
                                   0x1000U, 0x1000U, 0x10000U};
    std::array<void*, 6> maps{};
    nx_stm32_sai_state_t state{};
    nx_stm32_sai_config_t config{};
    nx_stm32_av_pin_t pins[4]{};
    uint16_t snapshots[4]{};
    nx_dma_memory_region_t region{0x23000000U, 0x10000U, 3U};
    nx_stream_slot_t tx_slots[2]{}, rx_slots[2]{};
    nx_stream_t tx_stream{}, rx_stream{};
    nx_frame_stream_t tx_frame{}, rx_frame{};
    nx_frame_dma_loan_t tx_loan{};
    nx_audio_request_t request{};
    nx_audio_port_t port{&nx_stm32_sai_ops, &state};
    void SetUp() override {
        auto* const prepare = &nx_stm32_sai_prepare;
        ASSERT_NE(nullptr, prepare);
        hardware = &hw;
        hw.owner = &state;
        for (size_t i = 0U; i < pages.size(); ++i) {
            maps[i] =
                mmap(reinterpret_cast<void*>(pages[i]), sizes[i],
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(reinterpret_cast<void*>(pages[i]), maps[i]);
        }
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.timer = TIM2;
        g_nx_stm32_system.core_hz = 16000000U;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.started = true;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR;
        RCC->AHB1ENR = RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_GPIOEEN;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        TIM2->PSC = 7U;
        TIM2->ARR = UINT32_MAX;
        TIM2->CR1 = TIM_CR1_CEN;
        TIM2->DIER = TIM_DIER_UIE;
        for (unsigned i = 0; i < 4U; ++i)
            pins[i] = {GPIOE, &snapshots[i], 4U,
                       static_cast<uint8_t>(i == 3U ? 3U : 4U + i), 6U};
        config.common = {pins, 3U, &region, 1U, 8U, 5U};
        config.tx = {DMA2, DMA2_Stream1, DMA2_Stream1_IRQn, 1U, 0U};
        config.rx = {DMA2, DMA2_Stream4, DMA2_Stream4_IRQn, 4U, 1U};
        config.transmit = SAI1_Block_A;
        config.source = SAI1_Block_A;
        config.hclk_hz = 16000000U;
        config.pclk_hz = 16000000U;
        config.max_bit_clock_hz = 3072000U;
        config.frame_bits = 32U;
        config.frame_sync_bits = 16U;
        config.frame_sync_channels = true;
        config.frame_sync_before = true;
        config.sample_bits = 16U;
        config.slot_bits = 16U;
        config.slots = 2U;
        config.active_slots = 3U;
        config.sample_rising = true;
#ifdef NX_SAI_MASTER_TEST
        config.master = true;
        config.sample_bits = 24U;
        config.slot_bits = config.frame_sync_bits = 32U;
        config.frame_bits = 64U;
        RCC->CR |= RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
        RCC->PLLSAICFGR = (NEXUS_CLOCK_PLLSAI_N << RCC_PLLSAICFGR_PLLSAIN_Pos) |
                          (NEXUS_CLOCK_PLLSAI_Q << RCC_PLLSAICFGR_PLLSAIQ_Pos) |
                          RCC_PLLSAICFGR_PLLSAIP_0;
#if defined(STM32F446xx)
        RCC->PLLSAICFGR |= 10U;
#else
        RCC->PLLSAICFGR |= 2U << RCC_PLLSAICFGR_PLLSAIR_Pos;
#endif
        RCC->DCKCFGR = (NEXUS_CLOCK_PLLSAI_DIVQ - 1U)
                       << RCC_DCKCFGR_PLLSAIDIVQ_Pos;
#endif
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&request.base, 1000U));
        for (size_t i = 0; i < 2U; ++i) {
            tx_slots[i].data =
                reinterpret_cast<uint8_t*>(0x23000000U + i * 256U);
            tx_slots[i].capacity = 256U;
            rx_slots[i].data =
                reinterpret_cast<uint8_t*>(0x23001000U + i * 256U);
            rx_slots[i].capacity = 256U;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&tx_stream, tx_slots, 2U));
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&rx_stream, rx_slots, 2U));
    }
    void TearDown() override {
        for (size_t i = 0; i < maps.size(); ++i)
            if (maps[i] != nullptr && maps[i] != MAP_FAILED) {
                EXPECT_EQ(0, munmap(maps[i], sizes[i]));
            }
        hardware = nullptr;
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sai_prepare(&state, &config));
    }
    void Frames(nx_frame_stream_t* framed, nx_stream_t* stream, size_t bytes) {
        const nx_frame_plane_t plane = {0U, bytes, bytes, 1U};
        nx_frame_layout_t layout{};
        ASSERT_EQ(NX_SUCCESS, nx_frame_layout_planar(&plane, 1U, &layout));
        ASSERT_EQ(NX_SUCCESS,
                  nx_frame_stream_initialize(framed, stream, &layout));
    }
    void Tx(size_t bytes = 16U) {
        Frames(&tx_frame, &tx_stream, bytes);
        nx_stream_fill_t fill{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_reserve(&tx_stream, &fill));
        ASSERT_EQ(NX_SUCCESS, nx_frame_stream_publish(&tx_frame, &fill, 0U));
        ASSERT_EQ(NX_SUCCESS, nx_frame_stream_acquire(&tx_frame, &tx_loan));
        request.tx = &tx_loan;
    }
    void Rx(size_t bytes = 16U) {
        Frames(&rx_frame, &rx_stream, bytes);
        request.rx = &rx_frame;
    }
    void Complete(bool receive) {
        const nx_stm32_av_dma_t& dma = receive ? config.rx : config.tx;
        dma.stream->NDTR = 0U;
        dma.stream->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        (dma.lane < 4U ? DMA2->LISR : DMA2->HISR) |= 0x20U << shifts[dma.lane];
        if (config.transmit != nullptr)
            config.transmit->SR = 0U;
        nx_stm32_sai_dma_irq(&state, receive);
    }
};
class SaiBlocks : public Sai {
  protected:
    nx_stm32_sai_blocks_state_t continuous{};
    nx_stm32_sai_blocks_config_t selected{};
    nx_audio_blocks_request_t session{};
    nx_audio_port_t streaming{&nx_stm32_sai_blocks_ops, &continuous};
    nx_stream_slot_t tx_blocks[4]{}, rx_blocks[4]{};
    void OpenBlocks(bool receive = false, bool duplex = false) {
        auto* const prepare = &nx_stm32_sai_blocks_prepare;
        ASSERT_NE(nullptr, prepare);
        selected.audio = config;
        selected.max_service_gap_us = 10U;
        if (receive) {
            selected.audio.receive = SAI1_Block_A;
            selected.audio.transmit = nullptr;
            selected.audio.rx = config.tx;
        } else if (duplex) {
            selected.audio.receive = SAI1_Block_B;
            selected.audio.common.pin_count = 4U;
        }
        ASSERT_EQ(NX_SUCCESS, prepare(&continuous, &selected));
        nx_request_initialize(&session.base);
        ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&session.base, 1000U));
        session.block_bytes = 128U;
        for (size_t i = 0U; i < 4U; ++i) {
            tx_blocks[i].data =
                reinterpret_cast<uint8_t*>(0x23000000U + 256U * i);
            tx_blocks[i].capacity = 256U;
            rx_blocks[i].data =
                reinterpret_cast<uint8_t*>(0x23001000U + 256U * i);
            rx_blocks[i].capacity = 256U;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&tx_stream, tx_blocks, 4U));
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&rx_stream, rx_blocks, 4U));
        session.tx = receive ? nullptr : &tx_stream;
        session.rx = receive || duplex ? &rx_stream : nullptr;
        if (session.tx != nullptr)
            for (unsigned i = 0U; i < 4U; ++i) {
                nx_stream_fill_t fill{};
                ASSERT_EQ(NX_SUCCESS, nx_stream_reserve(&tx_stream, &fill));
                ASSERT_EQ(NX_SUCCESS,
                          nx_stream_publish(&tx_stream, &fill, 128U, 0U));
            }
    }
    void Boundary(bool receive) {
        const auto& d = receive ? selected.audio.rx : selected.audio.tx;
        d.stream->CR ^= DMA_SxCR_CT;
        (d.lane < 4U ? DMA2->LISR : DMA2->HISR) |= 0x20U << shifts[d.lane];
        nx_stm32_sai_blocks_dma_irq(&continuous, receive);
    }
};
#ifdef NX_SAI_MASTER_TEST
TEST_F(SaiBlocks, MasterSai2ReusesTheSystemQClockAndTheSameBankProtocol) {
    config.transmit = config.source = SAI2_Block_B;
    config.tx = {DMA2, DMA2_Stream6, DMA2_Stream6_IRQn, 6U, 3U};
    for (size_t i = 0U; i < 3U; ++i)
        pins[i] = {GPIOE, &snapshots[i], 4U, static_cast<uint8_t>(13U - i),
                   10U};
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, streaming.ops->rate(&continuous, &rate));
    EXPECT_EQ(NEXUS_CLOCK_PLLSAI_SAI1_HZ, rate.numerator_hz);
    EXPECT_EQ(256U, rate.denominator);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&session.base));
    EXPECT_EQ(32U, hw.initial_ndtr[6]);
    Boundary(false);
    EXPECT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_STREAM_SLOT_FREE, tx_blocks[0].state);
    EXPECT_EQ(NX_SUCCESS, streaming.ops->stop(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(128U, session.base.transferred);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sai_blocks_close(&continuous));
}
#endif
#ifndef NX_SAI_MASTER_TEST
TEST_F(SaiBlocks, TransmitReplacesInactiveAddressBeforeReturningTheLoan) {
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&session.base));
    EXPECT_EQ(3U, continuous.tx_loans);
    Boundary(false);
    ASSERT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(tx_blocks[2].data),
              DMA2_Stream1->M0AR);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, tx_blocks[0].state);
    EXPECT_EQ(128U, continuous.transferred);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(NX_SUCCESS, streaming.ops->stop(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(128U, session.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream1->M0AR);
    EXPECT_EQ(0U, DMA2_Stream1->M1AR);
}
TEST_F(SaiBlocks,
       ReceivePublishesDetachedBlocksAndPreservesConsumersAfterStop) {
    OpenBlocks(true);
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    Boundary(true);
    ASSERT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    nx_stream_block_t ready{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&rx_stream, &ready));
    EXPECT_EQ(rx_blocks[0].data, ready.data);
    EXPECT_NE(reinterpret_cast<uintptr_t>(ready.data), DMA2_Stream1->M0AR);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->stop(&continuous));
    EXPECT_EQ(1U, rx_stream.outstanding);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&rx_stream, &ready));
    EXPECT_EQ(0U, rx_stream.outstanding);
}
TEST_F(SaiBlocks, MissedServiceBudgetWithdrawsWithoutReturningAddressedBlocks) {
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    hw.now += 11U;
    hw.dma_held = true;
    EXPECT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&session.base));
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, tx_blocks[0].state);
    hw.dma_held = false;
    EXPECT_EQ(NX_ERROR_OVERFLOW, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
}
TEST_F(SaiBlocks, FifoFailureAndLateIrqCannotPublishOrRestartASession) {
    OpenBlocks(true);
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    SAI1_Block_A->SR |= SAI_xSR_OVRUDR;
    nx_stm32_sai_blocks_irq(&continuous);
    EXPECT_EQ(NX_ERROR_IO, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(0U, rx_stream.outstanding);
    Boundary(true);
    EXPECT_EQ(NX_SUCCESS, streaming.ops->service(&continuous));
    EXPECT_EQ(0U, rx_stream.outstanding);
}
TEST_F(SaiBlocks, ReceivePoolExhaustionPreservesThePublishedPrefix) {
    OpenBlocks(true);
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    for (unsigned i = 0U; i < 2U; ++i) {
        Boundary(true);
        ASSERT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    }
    Boundary(true);
    EXPECT_EQ(NX_ERROR_OVERFLOW, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(256U, session.base.transferred);
    EXPECT_EQ(2U, rx_stream.outstanding);
    EXPECT_EQ(1U, rx_stream.losses);
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_stream_block_t ready{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&rx_stream, &ready));
        EXPECT_EQ(rx_blocks[i].data, ready.data);
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&rx_stream, &ready));
    }
    EXPECT_EQ(0U, rx_stream.outstanding);
}
TEST_F(SaiBlocks, DuplexKeepsIndependentBanksAndCountsTheReceivePrefixOnce) {
    OpenBlocks(false, true);
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    Boundary(false);
    ASSERT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(0U, continuous.transferred);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, tx_blocks[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, rx_blocks[0].state);
    Boundary(true);
    ASSERT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(128U, continuous.transferred);
    EXPECT_EQ(NX_STREAM_SLOT_READY, rx_blocks[0].state);
    EXPECT_EQ(NX_SUCCESS, streaming.ops->stop(&continuous));
    EXPECT_EQ(128U, session.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream1->M0AR);
    EXPECT_EQ(0U, DMA2_Stream4->M0AR);
}
TEST_F(SaiBlocks, LayoutQueriesCannotOverwriteSessionOrBankLoanControls) {
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        streaming.ops->layout(&continuous, reinterpret_cast<nx_audio_layout_t*>(
                                               &continuous.next_transmit)));
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    EXPECT_EQ(NX_ERROR_INVALID,
              streaming.ops->layout(
                  &continuous,
                  reinterpret_cast<nx_audio_layout_t*>(tx_blocks[3].data)));
    EXPECT_EQ(NX_ERROR_INVALID,
              streaming.ops->rate(
                  &continuous, reinterpret_cast<nx_audio_rate_t*>(&session)));
    EXPECT_EQ(NX_SUCCESS, streaming.ops->stop(&continuous));
}
TEST_F(SaiBlocks, ShortBlockAndPartialStoredFrameRejectBeforeAnyLoanOrWrite) {
    OpenBlocks(true);
    ASSERT_TRUE(continuous.cold.initialized);
    const unsigned before = hw.writes;
    for (size_t bytes : {2U, 8U, 12U}) {
        session.block_bytes = bytes;
        EXPECT_EQ(NX_ERROR_INVALID,
                  streaming.ops->start_blocks(&continuous, &session));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&session.base));
        EXPECT_EQ(0U, rx_stream.outstanding);
    }
    EXPECT_EQ(before, hw.writes);
}
TEST_F(SaiBlocks, RefusedTransmitReplacementRetainsBothLoansUntilDmaJoins) {
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    Boundary(false);
    hw.refuse_replacement = true;
    hw.dma_held = true;
    EXPECT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&session.base));
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, tx_blocks[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, tx_blocks[2].state);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(tx_blocks[0].data),
              DMA2_Stream1->M0AR);
    EXPECT_EQ(0U, continuous.transferred);
    hw.refuse_replacement = hw.dma_held = false;
    EXPECT_EQ(NX_ERROR_OVERFLOW, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(NX_STREAM_SLOT_FREE, tx_blocks[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, tx_blocks[2].state);
    EXPECT_EQ(1U, tx_stream.outstanding);
}
TEST_F(SaiBlocks, ReceiveBankRaceCannotPublishBeforeEveryAddressIsDetached) {
    OpenBlocks(true);
    ASSERT_TRUE(continuous.cold.initialized);
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    Boundary(true);
    hw.rotate_during_replacement = hw.dma_held = true;
    EXPECT_EQ(NX_ERROR_BUSY, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&session.base));
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, rx_blocks[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, rx_blocks[2].state);
    EXPECT_EQ(3U, continuous.producer.pending);
    EXPECT_EQ(0U, continuous.transferred);
    hw.dma_held = false;
    EXPECT_EQ(NX_ERROR_OVERFLOW, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&session.base));
    EXPECT_EQ(0U, rx_stream.outstanding);
    EXPECT_EQ(0U, session.base.transferred);
}
TEST_F(SaiBlocks, EpochLossAfterArmingKeepsTheSessionAndEveryBankBorrowed) {
    OpenBlocks();
    ASSERT_TRUE(continuous.cold.initialized);
    hw.break_epoch_on_irq_enable = true;
    ASSERT_EQ(NX_SUCCESS, streaming.ops->start_blocks(&continuous, &session));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_EQ(NX_ERROR_STATE, streaming.ops->service(&continuous));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&session.base));
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, tx_blocks[0].state);
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, tx_blocks[1].state);
    EXPECT_NE(0U, DMA2_Stream1->M0AR);
    EXPECT_NE(0U, DMA2_Stream1->M1AR);
}
TEST_F(Sai, MetadataQueriesNeverOverwriteTheCompleteSystemOwner) {
    Open();
    const auto before = g_nx_stm32_system;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(&port, reinterpret_cast<nx_audio_layout_t*>(
                                              &g_nx_stm32_system)));
    EXPECT_EQ(NX_ERROR_INVALID,
              port.ops->rate(&state, reinterpret_cast<nx_audio_rate_t*>(
                                         &g_nx_stm32_system)));
    EXPECT_EQ(0, std::memcmp(&before, &g_nx_stm32_system, sizeof(before)));
}

TEST_F(Sai, SelectedMclkRoleCannotBeAcceptedByASlaveOwner) {
    config.master = false;
    config.mclk = true;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, ReceivePayloadCannotBorrowTheSystemEvenInAnExplicitDmaDomain) {
    config.transmit = nullptr;
    config.receive = config.source = SAI1_Block_A;
    config.rx = config.tx;
    region.begin = reinterpret_cast<uintptr_t>(&g_nx_stm32_system);
    region.size = sizeof(g_nx_stm32_system);
    rx_slots[0].data = reinterpret_cast<uint8_t*>(&g_nx_stm32_system);
    rx_slots[0].capacity = 32U;
    rx_slots[1].data = reinterpret_cast<uint8_t*>(&g_nx_stm32_system) + 32U;
    rx_slots[1].capacity = 32U;
    Open();
    Rx();
    const unsigned before = hw.writes;
    EXPECT_EQ(NX_ERROR_INVALID, port.ops->submit(&state, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, rx_stream.outstanding);
    EXPECT_EQ(before, hw.writes);
}
TEST_F(Sai, ExpiryAfterDmaArmingCannotEnableTheSerializer) {
    Open();
    Tx();
    hw.expire_on_irq_enable = true;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_NE(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, ExpiryDuringQueuedFifoObservationCannotStartTheWire) {
    Open();
    Tx();
    hw.prime = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_QUEUED, nx_request_state(&request.base));
    SAI1_Block_A->SR = SAI_xSR_FLVL_0;
    hw.expire_on_fifo_read = true;
    hw.fifo_reads_before_expiry = 1U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, port.ops->service(&state));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(Sai, EpochLossAfterArmingKeepsTheLoanWithoutStartingAnySerializer) {
    Open();
    Tx();
    hw.break_epoch_on_irq_enable = true;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_EQ(NX_ERROR_STATE, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
}
TEST_F(Sai, EpochLossDuringQueuedPrimeCannotReturnAnUnprovedLoan) {
    Open();
    Tx();
    hw.prime = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_QUEUED, nx_request_state(&request.base));
    SAI1_Block_A->SR = SAI_xSR_FLVL_0;
    hw.break_epoch_on_fifo_read = true;
    hw.fifo_reads_before_expiry = 1U;
    EXPECT_EQ(NX_ERROR_STATE, port.ops->service(&state));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
}
TEST_F(Sai, InterruptCauseDuringDeadlineSamplingRemainsTheFirstFailure) {
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    hw.now = request.base.deadline;
    hw.interrupt_during_now = true;
    EXPECT_EQ(NX_ERROR_IO, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, RuntimeOwnerCannotOverwriteItsImmutableMemoryDomainTable) {
    config.common.regions =
        reinterpret_cast<const nx_dma_memory_region_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, EveryRealSaiSampleWidthHasAnExactNativeContainerLayout) {
    for (uint8_t bits : {uint8_t{8U}, uint8_t{10U}, uint8_t{16U}, uint8_t{20U},
                         uint8_t{24U}, uint8_t{32U}}) {
        const uint8_t container = bits <= 8U ? 8U : bits <= 16U ? 16U : 32U;
        config.sample_bits = bits;
        config.slot_bits = container;
        config.frame_bits = static_cast<uint16_t>(container * 2U);
        config.frame_sync_bits = container;
        ASSERT_NO_FATAL_FAILURE(Open());
        nx_audio_layout_t layout{};
        ASSERT_EQ(NX_SUCCESS, nx_audio_port_layout(&port, &layout));
        EXPECT_EQ(bits, layout.sample_bits);
        EXPECT_EQ(container, layout.container_bits);
        EXPECT_EQ(NX_FRAME_SAMPLE_LSB, layout.sample_alignment);
        EXPECT_EQ(NX_AUDIO_STORAGE_LITTLE_ENDIAN, layout.storage_order);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sai_close(&state));
    }
}
TEST_F(Sai, RefusedSourceEnableBecomesIoWithoutAnUnboundedStartingState) {
    Open();
    Tx();
    hw.refuse_enable = true;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(NX_ERROR_IO, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, RefusedErrorSourceMaskCannotStarveTaskRecoveryWithALevelIrq) {
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    hw.refuse_error_mask = true;
    hw.sticky_error = true;
    SAI1_Block_A->SR |= SAI_xSR_OVRUDR;
    nx_stm32_sai_irq(&state);
    EXPECT_EQ(0U, hw.enabled[SAI1_IRQn]);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
}
TEST_F(Sai, ColdPrepareUsesJointHalConfigurationWithoutStartingSources) {
    Open();
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_SAIEN);
    EXPECT_EQ(0U, SAI1_Block_A->IMR);
    EXPECT_EQ(0U, DMA2_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, SAI1->GCR);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SAI1EN);
}
TEST_F(Sai, ColdPadsMatchTheDatasheetCharacterizationSlewWithoutChangingPeers) {
    GPIOE->OSPEEDR = 1U;
    Open();
    EXPECT_EQ(1U, GPIOE->OSPEEDR & 3U);
    for (const auto& pin : pins) {
        if (pin.pin != 3U) {
            EXPECT_EQ(2U, (GPIOE->OSPEEDR >> (2U * pin.pin)) & 3U);
        }
    }
}
TEST_F(Sai, UnsupportedGeometryAndDmaRouteRejectBeforeWrites) {
    config.active_slots = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.active_slots = 3U;
    config.tx.channel = 1U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, DmaMetadataCannotJoinAnUnselectedLaneOrVector) {
    config.tx.stream = DMA2_Stream3;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.tx.stream = DMA2_Stream1;
    config.tx.irq = DMA2_Stream3_IRQn;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.tx.irq = DMA2_Stream1_IRQn;
    config.tx.lane = 8U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, SparseSlotsStoreOnlyEnabledSamplesAndKeepWireGeometry) {
    config.slots = 8U;
    config.active_slots = 0x81U;
    config.frame_bits = 128U;
    config.frame_sync_bits = 64U;
    Open();
    ASSERT_TRUE(state.initialized);
    EXPECT_EQ(0x81U, SAI1_Block_A->SLOTR >> SAI_xSLOTR_SLOTEN_Pos);
    nx_audio_layout_t layout{};
    ASSERT_EQ(NX_SUCCESS, port.ops->layout(&state, &layout));
    EXPECT_EQ(8U, layout.slots);
    EXPECT_EQ(0x81U, layout.active_slots);
    nx_frame_audio_format_t format{};
    format.kind = NX_FRAME_AUDIO_SAI;
    format.sample = NX_FRAME_PCM_SIGNED;
    format.alignment = layout.sample_alignment;
    format.frames = 1U;
    format.active_slots = layout.active_slots;
    format.slots = layout.slots;
    format.sample_bits = layout.sample_bits;
    format.slot_bits = layout.container_bits;
    nx_frame_layout_t framed{};
    ASSERT_EQ(NX_SUCCESS, nx_frame_layout_audio(&format, &framed));
    EXPECT_EQ(4U, framed.bytes);
    Tx(4U);
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(2U, hw.initial_ndtr[1]);
    Complete(false);
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(4U, request.base.transferred);
}
TEST_F(Sai, SparsePayloadRejectsPartialStoredFramesBeforeAdmission) {
    config.slots = 8U;
    config.active_slots = 0x55U;
    config.frame_bits = 128U;
    config.frame_sync_bits = 64U;
    Open();
    ASSERT_TRUE(state.initialized);
    Tx(6U);
    const unsigned before = hw.writes;
    EXPECT_EQ(NX_ERROR_INVALID, port.ops->submit(&state, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, hw.writes);
}
TEST_F(Sai, SparseMaskCannotSelectAnUnconfiguredWireSlot) {
    config.active_slots = 0x5U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, Sai2ReusesTheOwnerWithoutTouchingSai1GateVectorOrBlocks) {
    config.source = config.transmit = SAI2_Block_B;
    config.tx = {DMA2, DMA2_Stream6, DMA2_Stream6_IRQn, 6U, 3U};
    for (size_t i = 0U; i < 3U; ++i)
        pins[i] = {GPIOE, &snapshots[i], 4U, static_cast<uint8_t>(13U - i),
                   10U};
    RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
    SAI1_Block_A->CR1 = SAI_xCR1_SAIEN;
    hw.enabled[SAI1_IRQn] = 1U;
    hw.priority[SAI1_IRQn] = 7U;
    Open();
    ASSERT_TRUE(state.initialized);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SAI2EN);
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(1U, hw.enabled[SAI2_IRQn]);
    EXPECT_EQ(8U, hw.initial_ndtr[6]);
    Complete(false);
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sai_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SAI2EN);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SAI1EN);
    EXPECT_EQ(SAI_xCR1_SAIEN, SAI1_Block_A->CR1);
    EXPECT_EQ(1U, hw.enabled[SAI1_IRQn]);
    EXPECT_EQ(7U, hw.priority[SAI1_IRQn]);
}
TEST_F(Sai, Sai2RejectsCrossControllerPeersAndIncorrectRequestRoutes) {
    config.source = config.transmit = SAI2_Block_A;
    config.tx = {DMA2, DMA2_Stream4, DMA2_Stream4_IRQn, 4U, 3U};
    config.receive = SAI1_Block_B;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.receive = nullptr;
    config.tx.channel = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
}
TEST_F(Sai, SharedFrameSynchronizationRequiresApbStrictlyAboveTwiceBitClock) {
    config.receive = SAI1_Block_B;
    config.common.pin_count = 4U;
    config.max_bit_clock_hz = 8000000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
}
TEST_F(Sai, SimplexClockAdmissionAlsoRequiresTheReviewedApbMargin) {
    config.max_bit_clock_hz = 8000000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    config.receive = SAI1_Block_A;
    config.transmit = nullptr;
    config.rx = config.tx;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, ForeignPeerAndGlobalIrqAreNotAdoptedOrReset) {
    SAI1_Block_B->CR1 = SAI_xCR1_SAIEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sai_admit(&state, &config));
    SAI1_Block_B->CR1 = 0U;
    hw.enabled[SAI1_IRQn] = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
}
TEST_F(Sai, TransmitDmaCountsContainersAndSettlesOnlyAfterWireWithdrawal) {
    config.sample_bits = 24U;
    config.slot_bits = 32U;
    config.frame_bits = 64U;
    config.frame_sync_bits = 32U;
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(4U, hw.initial_ndtr[1]);
    EXPECT_EQ(DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1,
              DMA2_Stream1->CR & (DMA_SxCR_MSIZE | DMA_SxCR_PSIZE));
    Complete(false);
    hw.serializer_held = true;
    EXPECT_EQ(NX_ERROR_BUSY, port.ops->service(&state));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    hw.serializer_held = false;
    EXPECT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(16U, request.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream1->M0AR);
    EXPECT_EQ(0U, DMA2_Stream1->PAR);
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, FifoPrimeIsAsynchronousAndHasNoPrivateTickWait) {
    Open();
    Tx();
    hw.prime = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(NX_REQUEST_QUEUED, nx_request_state(&request.base));
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_SAIEN);
    SAI1_Block_A->SR = SAI_xSR_FLVL_0;
    EXPECT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(Sai, LayoutAndRateAreReadonlyAndKeepOpaqueAndLoanStorageIndependent) {
    config.sample_bits = 24U;
    config.slot_bits = 32U;
    config.frame_bits = 64U;
    config.frame_sync_bits = 32U;
    Open();
    nx_audio_layout_t layout{};
    nx_audio_rate_t rate{};
    const unsigned writes = hw.writes;
    ASSERT_EQ(NX_SUCCESS, nx_audio_port_layout(&port, &layout));
    EXPECT_EQ(24U, layout.sample_bits);
    EXPECT_EQ(32U, layout.container_bits);
    EXPECT_EQ(NX_FRAME_SAMPLE_LSB, layout.sample_alignment);
    EXPECT_EQ(NX_AUDIO_STORAGE_LITTLE_ENDIAN, layout.storage_order);
    EXPECT_EQ(3U, layout.active_slots);
    ASSERT_EQ(NX_SUCCESS, port.ops->rate(&state, &rate));
    EXPECT_EQ(0U, rate.numerator_hz);
    EXPECT_EQ(0U, rate.denominator);
    EXPECT_EQ(writes, hw.writes);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(
                  &port, reinterpret_cast<nx_audio_layout_t*>(&config)));
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(&port, reinterpret_cast<nx_audio_layout_t*>(
                                              tx_slots[0].data)));
}
TEST_F(Sai, ReceivePublishesOnlyTheDrainedCompleteFrame) {
    config.transmit = nullptr;
    config.receive = SAI1_Block_A;
    config.rx = config.tx;
    Open();
    Rx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    EXPECT_EQ(1U, rx_stream.outstanding);
    Complete(true);
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_frame_stream_acquire(&rx_frame, &tx_loan));
    block = tx_loan.block;
    EXPECT_EQ(16U, block.length);
    EXPECT_EQ(16U, request.base.transferred);
}
TEST_F(Sai, DuplexOneOwnerStopsSourceBeforeSinkAndRetiresBothDirections) {
    config.receive = SAI1_Block_B;
    config.common.pin_count = 4U;
    Open();
    Tx();
    Rx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    Complete(false);
    Complete(true);
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(2U, hw.disable_count);
    EXPECT_EQ(0U, hw.disable_order[0]);
    EXPECT_EQ(1U, hw.disable_order[1]);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, CancelAndRefusedDmaKeepOriginalBorrowUntilActualRecovery) {
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    hw.dma_held = true;
    EXPECT_EQ(NX_SUCCESS, port.ops->cancel(&state, &request));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sai_close(&state));
    hw.dma_held = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sai_close(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, tx_stream.outstanding);
}
TEST_F(Sai, AddressRefusalAndLateIrqCannotReturnOrReopenOldLoans) {
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    Complete(false);
    hw.address_held = true;
    EXPECT_EQ(NX_ERROR_BUSY, port.ops->service(&state));
    EXPECT_EQ(1U, tx_stream.outstanding);
    hw.address_held = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    const auto before = request;
    nx_stm32_sai_irq(&state);
    nx_stm32_sai_dma_irq(&state, false);
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(Sai, ExhaustedClockCannotTurnCancelWithdrawalIntoAnInfiniteWait) {
    Open();
    Tx();
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    hw.serializer_held = true;
    hw.now = NX_DEADLINE_NEVER;
    ASSERT_EQ(NX_SUCCESS, port.ops->cancel(&state, &request));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(NX_DEADLINE_NEVER, state.drain_until);
    EXPECT_EQ(1U, tx_stream.outstanding);
}
TEST_F(Sai, SystemEpochLossNeverFabricatesASafeResetOrRevokesTheBorrow) {
    Open();
    Tx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(NX_ERROR_STATE, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
    EXPECT_NE(NX_SUCCESS, nx_stm32_sai_close(&state));
}
#else
#ifdef NX_SAI_ALT_CLOCK
TEST_F(Sai, NoDividerHasNoMclkAndPermitsAnExactNonPowerOfTwoFrame) {
    config.no_divider = true;
    config.slots = 3U;
    config.active_slots = 7U;
    config.frame_bits = 96U;
    config.frame_sync_channels = false;
    config.frame_sync_bits = 1U;
    config.max_bit_clock_hz = NEXUS_CLOCK_PLLSAI_SAI1_HZ;
    Open();
    EXPECT_NE(0U, SAI1_Block_A->CR1 & SAI_xCR1_NODIV);
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, port.ops->rate(&state, &rate));
    ASSERT_NE(0U, rate.denominator);
    EXPECT_EQ(uint64_t{7000000U} * rate.denominator,
              uint64_t{96U} * rate.numerator_hz);
}
TEST_F(Sai, CharacterizedMclkPreservesTheExactFractionAtAnotherKernel) {
    config.mckdiv = 1U;
    config.mclk = true;
    pins[3] = {GPIOE, &snapshots[3], 4U, 2U, 6U};
    config.common.pin_count = 4U;
    Open();
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, port.ops->rate(&state, &rate));
    ASSERT_NE(0U, rate.denominator);
    EXPECT_EQ(uint64_t{7000000U} * rate.denominator,
              uint64_t{512U} * rate.numerator_hz);
    EXPECT_EQ(1U << SAI_xCR1_MCKDIV_Pos, SAI1_Block_A->CR1 & SAI_xCR1_MCKDIV);
}
#else
TEST_F(Sai, MasterUsesTheOneSystemKernelWithoutMutatingItsAuxiliaryOwner) {
    const auto control = RCC->CR;
    const auto auxiliary = RCC->PLLSAICFGR;
    const auto mux = RCC->DCKCFGR;
    Open();
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_MODE);
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & (SAI_xCR1_NODIV | SAI_xCR1_MCKDIV));
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, port.ops->rate(&state, &rate));
    ASSERT_NE(0U, rate.denominator);
    EXPECT_EQ(48000U * rate.denominator, rate.numerator_hz);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sai_close(&state));
    EXPECT_EQ(control, RCC->CR);
    EXPECT_EQ(auxiliary, RCC->PLLSAICFGR);
    EXPECT_EQ(mux, RCC->DCKCFGR);
}
TEST_F(Sai, MasterTransmitPrimesBeforeActiveAndRetiresTheCompleteWireLoan) {
    Open();
    Tx();
    hw.prime = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_QUEUED, nx_request_state(&request.base));
    EXPECT_EQ(0U, hw.enables);
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_MODE);
    SAI1_Block_A->SR = SAI_xSR_FLVL_0;
    ASSERT_EQ(NX_SUCCESS, port.ops->service(&state));
    ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    Complete(false);
    EXPECT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, tx_stream.outstanding);
    EXPECT_EQ(0U, DMA2_Stream1->PAR);
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_SAIEN);
    EXPECT_NE(0U, RCC->CR & RCC_CR_PLLSAION);
}
TEST_F(Sai, MasterReceiveGeneratesFramesFromItsOwnAsynchronousBlock) {
    config.transmit = nullptr;
    config.receive = SAI1_Block_A;
    config.rx = config.tx;
    Open();
    Rx();
    EXPECT_EQ(static_cast<uint32_t>(SAI_xCR1_MODE_0),
              SAI1_Block_A->CR1 & SAI_xCR1_MODE);
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    Complete(true);
    EXPECT_EQ(NX_SUCCESS, port.ops->service(&state));
    EXPECT_EQ(1U, rx_stream.outstanding);
    nx_frame_dma_loan_t ready{};
    ASSERT_EQ(NX_SUCCESS, nx_frame_stream_acquire(&rx_frame, &ready));
    EXPECT_EQ(16U, ready.block.length);
}
TEST_F(Sai, MasterDuplexEnablesTheSinkFirstAndStopsTheClockSourceFirst) {
    config.receive = SAI1_Block_B;
    config.common.pin_count = 4U;
    Open();
    Tx();
    Rx();
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(0U, SAI1_Block_A->CR1 & SAI_xCR1_MODE);
    EXPECT_EQ(static_cast<uint32_t>(SAI_xCR1_SYNCEN_0),
              SAI1_Block_B->CR1 & SAI_xCR1_SYNCEN);
    ASSERT_EQ(2U, hw.enables);
    EXPECT_EQ(1U, hw.enable_order[0]);
    EXPECT_EQ(0U, hw.enable_order[1]);
    Complete(false);
    Complete(true);
    EXPECT_EQ(NX_SUCCESS, port.ops->service(&state));
    ASSERT_EQ(2U, hw.disable_count);
    EXPECT_EQ(0U, hw.disable_order[0]);
    EXPECT_EQ(1U, hw.disable_order[1]);
    EXPECT_EQ(0U, tx_stream.outstanding);
    nx_frame_dma_loan_t ready{};
    ASSERT_EQ(NX_SUCCESS, nx_frame_stream_acquire(&rx_frame, &ready));
    EXPECT_EQ(16U, ready.block.length);
}
TEST_F(Sai, MasterDividerKeepsTheExactFractionInsteadOfRoundingTheRate) {
    config.mckdiv = 7U;
    Open();
    EXPECT_EQ(7U << SAI_xCR1_MCKDIV_Pos, SAI1_Block_A->CR1 & SAI_xCR1_MCKDIV);
    nx_audio_rate_t rate{};
    ASSERT_EQ(NX_SUCCESS, port.ops->rate(&state, &rate));
    ASSERT_NE(0U, rate.denominator);
    EXPECT_EQ(24000U * rate.denominator, rate.numerator_hz * 7U);
}
TEST_F(Sai, MasterGeometryDividerAndElectricalLimitsRejectBeforeWrites) {
    config.slots = 3U;
    config.active_slots = 7U;
    config.frame_bits = 96U;
    config.frame_sync_channels = false;
    config.frame_sync_bits = 1U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    config.slots = 2U;
    config.active_slots = 3U;
    config.frame_bits = 64U;
    config.frame_sync_channels = true;
    config.frame_sync_bits = 32U;
    config.mckdiv = 16U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.mckdiv = 1U;
    config.no_divider = true;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sai_admit(&state, &config));
    config.no_divider = false;
    config.mckdiv = 0U;
    config.max_bit_clock_hz = 2000000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, SimplexMasterCannotBypassItsRequiredApbClockMargin) {
    config.no_divider = true;
    config.max_bit_clock_hz = 12288000U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, OnlyAnActuallySelectedMclkPadUsesItsCharacterizedOutputRange) {
    config.mckdiv = 7U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sai_admit(&state, &config));
    pins[3] = {GPIOE, &snapshots[3], 4U, 2U, 6U};
    config.common.pin_count = 4U;
    config.mclk = true;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sai_admit(&state, &config));
    EXPECT_EQ(0U, hw.writes);
    EXPECT_FALSE(state.owned);
}
TEST_F(Sai, LostAuxiliaryMuxCannotStartTheWireOrReleaseTheBorrow) {
    Open();
    Tx();
    hw.prime = false;
    ASSERT_EQ(NX_SUCCESS, port.ops->submit(&state, &request));
    RCC->DCKCFGR ^= RCC_DCKCFGR_PLLSAIDIVQ_0;
    EXPECT_EQ(NX_ERROR_STATE, port.ops->service(&state));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(1U, tx_stream.outstanding);
    EXPECT_EQ(0U, hw.enables);
}
#endif
#endif
} /* namespace */

extern "C" uint32_t nx_sai_read(volatile uint32_t* reg) {
    if ((hardware->expire_on_fifo_read || hardware->break_epoch_on_fifo_read) &&
        reg == &SAI1_Block_A->SR && (*reg & SAI_xSR_FLVL) != 0U) {
        if (hardware->fifo_reads_before_expiry != 0U) {
            --hardware->fifo_reads_before_expiry;
        } else {
            if (hardware->expire_on_fifo_read) {
                hardware->expire_on_fifo_read = false;
                hardware->now = 1000U;
            }
            if (hardware->break_epoch_on_fifo_read) {
                hardware->break_epoch_on_fifo_read = false;
                ++g_nx_stm32_system.time_epoch;
            }
        }
    }
    return *reg;
}
extern "C" void nx_sai_write(volatile uint32_t* reg, uint32_t value) {
    ++hardware->writes;
    const bool a = reg == &SAI1_Block_A->CR1 || reg == &SAI2_Block_A->CR1;
    const bool b = reg == &SAI1_Block_B->CR1 || reg == &SAI2_Block_B->CR1;
    if ((a || b) && (*reg & SAI_xCR1_SAIEN) == 0U &&
        (value & SAI_xCR1_SAIEN) != 0U) {
        if (hardware->enables < 2U)
            hardware->enable_order[hardware->enables] = b ? 1U : 0U;
        ++hardware->enables;
    }
    if ((a || b) && hardware->refuse_enable && (*reg & SAI_xCR1_SAIEN) == 0U)
        value &= ~static_cast<uint32_t>(SAI_xCR1_SAIEN);
    if ((reg == &SAI1_Block_A->IMR || reg == &SAI1_Block_B->IMR ||
         reg == &SAI2_Block_A->IMR || reg == &SAI2_Block_B->IMR) &&
        hardware->refuse_error_mask && value == 0U)
        return;

    if ((a || b) && (*reg & SAI_xCR1_SAIEN) != 0U &&
        (value & SAI_xCR1_SAIEN) == 0U) {
        if (hardware->serializer_held)
            value |= SAI_xCR1_SAIEN;
        else if (hardware->disable_count < 2U)
            hardware->disable_order[hardware->disable_count++] = b ? 1U : 0U;
    }
    for (unsigned i = 0U; i < 8U; ++i) {
        auto* stream = reinterpret_cast<DMA_Stream_TypeDef*>(DMA2_Stream0_BASE +
                                                             i * 0x18U);
        if (reg == &stream->CR && (value & DMA_SxCR_EN) != 0U &&
            (*reg & DMA_SxCR_EN) == 0U)
            hardware->initial_ndtr[i] = stream->NDTR;
        if (reg == &stream->CR && hardware->dma_held &&
            (*reg & DMA_SxCR_EN) != 0U)
            value |= DMA_SxCR_EN;
        if (hardware->address_held &&
            (reg == &stream->M0AR || reg == &stream->M1AR ||
             reg == &stream->PAR) &&
            value == 0U)
            return;
        if ((reg == &stream->M0AR || reg == &stream->M1AR) && value != 0U &&
            (stream->CR & (DMA_SxCR_EN | DMA_SxCR_DBM)) ==
                (DMA_SxCR_EN | DMA_SxCR_DBM)) {
            if (hardware->refuse_replacement)
                return;
            if (hardware->rotate_during_replacement) {
                hardware->rotate_during_replacement = false;
                stream->CR ^= DMA_SxCR_CT;
                (i < 4U ? DMA2->LISR : DMA2->HISR) |= 0x20U << shifts[i];
            }
        }
    }
    *reg = value;
    if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_SAI1RST) != 0U) {
        SAI1->GCR = 0U;
        for (auto* block : {SAI1_Block_A, SAI1_Block_B}) {
            block->CR1 = 0U;
            block->CR2 = 0U;
            block->IMR = 0U;
            block->SR = 0U;
        }
    }
    if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_SAI2RST) != 0U) {
        SAI2->GCR = 0U;
        for (auto* block : {SAI2_Block_A, SAI2_Block_B}) {
            block->CR1 = 0U;
            block->CR2 = 0U;
            block->IMR = 0U;
            block->SR = 0U;
        }
    }
    if (reg == &DMA2->LIFCR)
        DMA2->LISR &= ~value;
    if (reg == &DMA2->HIFCR)
        DMA2->HISR &= ~value;
    for (auto* block :
         {SAI1_Block_A, SAI1_Block_B, SAI2_Block_A, SAI2_Block_B}) {
        if (reg == &block->CLRFR && !hardware->sticky_error)
            block->SR &= ~value;
        if (reg == &block->CR2 && (value & SAI_xCR2_FFLUSH) != 0U) {
            block->CR2 &= ~static_cast<uint32_t>(SAI_xCR2_FFLUSH);
            block->SR &= ~static_cast<uint32_t>(SAI_xSR_FLVL);
        }
        if (reg == &block->CR1 && (value & SAI_xCR1_DMAEN) != 0U &&
            hardware->prime && (value & SAI_xCR1_MODE_0) == 0U &&
            (block->SR & SAI_xSR_FLVL) == 0U) {
            for (unsigned i = 0U; i < 8U; ++i) {
                auto* stream = reinterpret_cast<DMA_Stream_TypeDef*>(
                    DMA2_Stream0_BASE + i * 0x18U);
                if ((stream->CR & DMA_SxCR_EN) != 0U && stream->NDTR != 0U &&
                    stream->PAR == reinterpret_cast<uintptr_t>(&block->DR)) {
                    --stream->NDTR;
                    block->SR |= SAI_xSR_FLVL_0;
                    break;
                }
            }
        }
    }
}
extern "C" nx_time_us_t nx_sai_now(void) {
    if (hardware->interrupt_during_now) {
        hardware->interrupt_during_now = false;
        SAI1_Block_A->SR |= SAI_xSR_OVRUDR;
        nx_stm32_sai_irq(hardware->owner);
    }
    return hardware->now;
}
extern "C" bool nx_sai_task_privileged(void) {
    return hardware->privileged;
}
extern "C" bool nx_sai_isr(void) {
    return hardware->in_isr;
}
extern "C" bool nx_sai_masked(void) {
    return hardware->masked;
}
extern "C" uint32_t nx_sai_irq_enabled(IRQn_Type irq) {
    return hardware->enabled[irq];
}
extern "C" uint32_t nx_sai_irq_active(IRQn_Type irq) {
    return hardware->active[irq];
}
extern "C" uint32_t nx_sai_irq_pending(IRQn_Type irq) {
    return hardware->pending[irq];
}
extern "C" uint32_t nx_sai_irq_priority_read(IRQn_Type irq) {
    return hardware->priority[irq];
}
extern "C" void nx_sai_irq_priority(IRQn_Type irq, uint32_t priority) {
    hardware->priority[irq] = priority;
}
extern "C" void nx_sai_irq_enable(IRQn_Type irq) {
    hardware->enabled[irq] = 1U;
    if (hardware->expire_on_irq_enable)
        hardware->now = 1000U;
    if (hardware->break_epoch_on_irq_enable) {
        hardware->break_epoch_on_irq_enable = false;
        ++g_nx_stm32_system.time_epoch;
    }
}
extern "C" void nx_sai_irq_disable(IRQn_Type irq) {
    if (!hardware->irq_held)
        hardware->enabled[irq] = 0U;
}
extern "C" void nx_sai_irq_clear(IRQn_Type irq) {
    if (!hardware->irq_held)
        hardware->pending[irq] = 0U;
}
