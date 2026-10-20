/**
 * \file            h743_uart_continuous_factory_test.cpp
 * \brief           Ordinary factory DBM receive and complete outer lease drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_continuous_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_uart_continuous.h"
#include "stm32h743_uart_duplex.h"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
extern "C" {
#if H743_CONTINUOUS_DUPLEX
void DMA1_Stream0_IRQHandler(void);
#endif
void DMA1_Stream1_IRQHandler(void);
void USART3_IRQHandler(void);
}
class H743ContinuousFactory : public testing::Test {
  protected:
    uint8_t* memory = nullptr;
    nx_stream_slot_t slots[4]{};
    nx_stream_t stream{};
    nx_uart_tx_request_t transmit{};
    std::array<uint8_t, 128> input{};
    const nx_uart_port_t* Port() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
    void SetUp() override {
        nx_h743_uart_continuous_model_reset();
        void* raw =
            mmap(reinterpret_cast<void*>(0x24000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, raw) << std::strerror(errno);
        memory = static_cast<uint8_t*>(raw);
        std::memset(memory, 0, 0x80000U);
        nx_request_initialize(&transmit.base);
        for (size_t i = 0U; i < 4U; ++i) {
            slots[i].data = memory + i * 64U;
            slots[i].capacity = 64U;
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 4U));
        for (size_t i = 0U; i < input.size(); ++i)
            input[i] = static_cast<uint8_t>(i * 3U + 7U);
        for (size_t i = 0U; i < 64U; ++i)
            memory[512U + i] = static_cast<uint8_t>(i * 5U + 3U);
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Tick() {
        nx_h743_uart_continuous_model_step();
        for (int irq : {static_cast<int>(DMA1_Stream0_IRQn),
                        static_cast<int>(DMA1_Stream1_IRQn),
                        static_cast<int>(USART3_IRQn)}) {
            if (!g_h743_model.irq_enabled[irq] ||
                !nx_h743_uart_continuous_model_level(irq))
                continue;
            g_h743_model.irq_active[irq] = true;
            g_h743_uart_continuous_model.exception =
                static_cast<unsigned>(irq + 16);
            if (irq == DMA1_Stream0_IRQn) {
#if H743_CONTINUOUS_DUPLEX
                DMA1_Stream0_IRQHandler();
#endif
            } else if (irq == DMA1_Stream1_IRQn)
                DMA1_Stream1_IRQHandler();
            else
                USART3_IRQHandler();
            g_h743_uart_continuous_model.exception = 0U;
            g_h743_model.irq_active[irq] = false;
        }
        nx_uart_port_service(Port());
    }
    void Feed(size_t bytes) {
        nx_h743_uart_continuous_model_feed(input.data(), bytes);
        for (unsigned i = 0U;
             i < 30000U && g_h743_uart_continuous_model.input_index != bytes;
             ++i)
            Tick();
        ASSERT_EQ(bytes, g_h743_uart_continuous_model.input_index);
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        g_h743_uart_continuous_model.hold_tx_en = false;
        g_h743_uart_continuous_model.hold_tx_wire = false;
        g_h743_uart_continuous_model.refuse_write = nullptr;
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned turn = 0U; turn < 10000U && result != NX_SUCCESS;
             ++turn) {
            nx_stream_block_t block{};
            while (nx_stream_acquire(&stream, &block) == NX_SUCCESS)
                ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
            Tick();
            result = nx_platform_stop();
        }
        EXPECT_EQ(NX_SUCCESS, result);
        if (result != NX_SUCCESS)
            std::abort();
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        EXPECT_EQ(0U, stream.outstanding);
        EXPECT_EQ(0U, g_h743_uart_continuous_model.cache_errors);
        EXPECT_EQ(0U, g_h743_uart_continuous_model.target_errors);
        EXPECT_EQ(0U, g_h743_uart_continuous_model.forbidden);
        EXPECT_EQ(0, munmap(memory, 0x80000U));
    }
};
TEST_F(H743ContinuousFactory, TypedLookupDoesNotAccessHardware) {
    ASSERT_NE(nullptr, Port());
    EXPECT_EQ(Port(), nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(0U, g_h743_model.reads | g_h743_model.writes);
}
TEST_F(H743ContinuousFactory, ClockAndCacheFollowTheAuthoredSelection) {
    Start();
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(H743_CONTINUOUS_CACHE_ON != 0,
              (g_h743_model.core->CCR & SCB_CCR_DC_Msk) != 0U);
    EXPECT_EQ(45U, DMAMUX1_Channel1->CCR);
    EXPECT_EQ(0U, DMA1_Stream1->M0AR | DMA1_Stream1->M1AR);
    EXPECT_EQ(5U, g_h743_model.priority[DMA1_Stream1_IRQn]);
}
TEST_F(H743ContinuousFactory, PublicStartAndGeneratedVectorsMoveTwoBlocks) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    Feed(128U);
    for (size_t i = 0U; i < 2U; ++i) {
        nx_stream_block_t block{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
        ASSERT_EQ(64U, block.length);
        EXPECT_EQ(0, std::memcmp(input.data() + i * 64U, block.data, 64U));
        EXPECT_NE(reinterpret_cast<uintptr_t>(block.data), DMA1_Stream1->M0AR);
        EXPECT_NE(reinterpret_cast<uintptr_t>(block.data), DMA1_Stream1->M1AR);
        EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    }
    EXPECT_EQ(1U, g_h743_uart_continuous_model.enabled_transitions);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
}
TEST_F(H743ContinuousFactory, HeldConsumerKeepsTheOuterPlatformAndPinsLive) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    Feed(64U);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_NE(0U, RCC->APB1LENR & RCC_APB1LENR_USART3EN);
    EXPECT_EQ(0, std::memcmp(input.data(), block.data, block.length));
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(0U, RCC->APB1LENR & RCC_APB1LENR_USART3EN);
}
TEST_F(H743ContinuousFactory, ColdForeignTargetRemainsOwnedByItsOriginalUser) {
    DMA1_Stream1->M1AR = 0x24001000U;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0x24001000U, DMA1_Stream1->M1AR);
    EXPECT_EQ(0U, stream.outstanding);
    DMA1_Stream1->M1AR = 0U;
}

TEST_F(H743ContinuousFactory, SuccessfulStopRestoresColdOwnerForNextStart) {
    for (unsigned run = 0U; run < 2U; ++run) {
        Start();
        ASSERT_TRUE(g_nx_stm32h743_system.started);
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 4U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
        Feed(64U);
        nx_stream_block_t block{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
        ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
        EXPECT_FALSE(g_nx_stm32h743_system.started);
    }
}

TEST_F(H743ContinuousFactory, FailedColdRestoreRetainsSystemAndCacheForRetry) {
    for (volatile uint32_t* reg : {&USART3->BRR, &DMA1_Stream1->FCR}) {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 4U));
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
        Feed(64U);
        nx_stream_block_t block{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
        g_h743_uart_continuous_model.refuse_write = reg;
        EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
        EXPECT_TRUE(g_nx_stm32h743_system.started);
        EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
        EXPECT_NE(0U, RCC->APB1LENR & RCC_APB1LENR_USART3EN);
        EXPECT_EQ(H743_CONTINUOUS_CACHE_ON != 0,
                  (g_h743_model.core->CCR & SCB_CCR_DC_Msk) != 0U);
        g_h743_uart_continuous_model.refuse_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
}

#if H743_CONTINUOUS_DUPLEX
TEST_F(H743ContinuousFactory, GeneratedDuplexVectorsMoveBothIndependentLoans) {
    Start();
    ASSERT_EQ(&nx_stm32h743_uart_duplex_ops, Port()->ops);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&transmit, memory + 512U, 64U, 100000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &transmit));
    Feed(128U);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&transmit.base));
    EXPECT_EQ(NX_SUCCESS, transmit.base.result);
    EXPECT_EQ(64U, g_h743_uart_continuous_model.tx_count);
    EXPECT_EQ(0, std::memcmp(memory + 512U, g_h743_uart_continuous_model.output,
                             64U));
    EXPECT_EQ(46U, DMAMUX1_Channel0->CCR);
}
TEST_F(H743ContinuousFactory, RefusedTxDrainKeepsCacheTimebaseAndGateOwned) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Port(), &stream));
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&transmit, memory + 512U, 64U, 100000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Port(), &transmit));
    Tick();
    g_h743_uart_continuous_model.hold_tx_en = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    for (unsigned i = 0U; i < 2000U; ++i)
        Tick();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&transmit.base));
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_NE(0U, RCC->APB1LENR & RCC_APB1LENR_USART3EN);
    EXPECT_EQ(H743_CONTINUOUS_CACHE_ON != 0,
              (g_h743_model.core->CCR & SCB_CCR_DC_Msk) != 0U);
}
#endif
