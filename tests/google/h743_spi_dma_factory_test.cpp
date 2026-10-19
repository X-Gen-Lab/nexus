/**
 * \file            h743_spi_dma_factory_test.cpp
 * \brief           Actual generated finite SPI and shared UART DMA leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_dma_factory_model.h"
#include "nexus/core/fault.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_spi_dma.h"
#if D2_UART_MODE == 0
#include "stm32h743_dma.h"
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
extern "C" {
void DMA1_Stream2_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);
void SPI4_IRQHandler(void);
#if D2_UART_MODE >= 0
void USART3_IRQHandler(void);
#if D2_UART_MODE == 0
void DMA1_Stream0_IRQHandler(void);
#else
void DMA1_Stream1_IRQHandler(void);
#endif
#endif
}
#if D2_UART_MODE == 0
namespace {
struct UartFaultTrace {
    bool observed;
    bool masked;
    bool borrowed;
    bool enabled;
    bool level;
    unsigned state;
    unsigned exception;
};
UartFaultTrace* uart_fault_trace;
nx_stm32h743_uart_dma_state_t* uart_fault_owner;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    if (uart_fault_trace == nullptr || domain != NX_FAULT_PLATFORM ||
        reason != NX_ERROR_IO)
        return;
    uart_fault_trace->masked = nx_arch_irq_is_masked();
    uart_fault_trace->borrowed = uart_fault_owner->uart.active != nullptr;
    uart_fault_trace->enabled = g_h743_model.irq_enabled[39];
    uart_fault_trace->level = nx_h743_spi_dma_factory_level(39);
    uart_fault_trace->state = static_cast<unsigned>(
        nx_request_state(&uart_fault_owner->uart.active->base));
    uart_fault_trace->exception = g_h743_spi_dma_model.exception;
    __atomic_store_n(&uart_fault_trace->observed, true, __ATOMIC_RELEASE);
}
#endif
class H743SpiDmaFactory : public testing::Test {
  protected:
    static constexpr size_t payload_bytes = H743_SPI_DMA_CACHE_ON ? 32U : 8U;
    uint8_t* memory = nullptr;
    nx_spi_request_t request{};
    nx_uart_tx_request_t uart_request{};
    nx_stream_t stream{};
    nx_stream_slot_t slots[2]{};
    const nx_spi_port_t* Port() {
        return nx_factory_spi(NX_SPI_ID_BUS4);
    }
    const nx_spi_endpoint_t* Device() {
        return nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE4);
    }
    nx_stm32h743_spi_dma_state_t* Owner() {
        return static_cast<nx_stm32h743_spi_dma_state_t*>(Port()->context);
    }
#if D2_UART_MODE >= 0
    const nx_uart_port_t* Uart() {
        return nx_factory_uart(NX_UART_ID_CONSOLE);
    }
#endif
    void SetUp() override {
        nx_h743_spi_dma_factory_reset();
        void* raw =
            mmap(reinterpret_cast<void*>(0x24000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, raw) << std::strerror(errno);
        memory = static_cast<uint8_t*>(raw);
        new (memory) std::array<uint8_t, 0x80000U>{};
        for (unsigned n = 0U; n < 64U; ++n)
            memory[n] = static_cast<uint8_t>(n * 7U + 3U);
        nx_request_initialize(&request.base);
        nx_request_initialize(&uart_request.base);
        slots[0].data = memory + 0x3000U;
        slots[1].data = memory + 0x4000U;
        slots[0].capacity = slots[1].capacity = 8U;
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2U));
        g_h743_model.spi_watch_pins = true;
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    unsigned Dispatch() {
        unsigned count = 0U;
        for (unsigned turn = 0U; turn < 8U; ++turn) {
            bool delivered = false;
            for (int irq : {11, 12, 13, 14, 39, 84}) {
                if (nx_h743_spi_dma_factory_level(irq))
                    g_h743_model.irq_pending[irq] = true;
                if (!g_h743_model.irq_enabled[irq] ||
                    !g_h743_model.irq_pending[irq] || nx_arch_irq_is_masked())
                    continue;
                g_h743_model.irq_pending[irq] = false;
                g_h743_model.irq_active[irq] = true;
                g_h743_spi_dma_model.exception =
                    static_cast<unsigned>(irq + 16);
                if (irq == 13)
                    DMA1_Stream2_IRQHandler();
                else if (irq == 14)
                    DMA1_Stream3_IRQHandler();
                else if (irq == 84)
                    SPI4_IRQHandler();
#if D2_UART_MODE >= 0
                else if (irq == 39)
                    USART3_IRQHandler();
#if D2_UART_MODE == 0
                else if (irq == 11)
                    DMA1_Stream0_IRQHandler();
#else
                else if (irq == 12)
                    DMA1_Stream1_IRQHandler();
#endif
#endif
                else
                    ADD_FAILURE() << "Unselected vector delivered " << irq;
                g_h743_spi_dma_model.exception = 0U;
                g_h743_model.irq_active[irq] = false;
                if (nx_h743_spi_dma_factory_level(irq))
                    g_h743_model.irq_pending[irq] = true;
                delivered = true;
                ++count;
            }
            if (!delivered)
                break;
        }
        return count;
    }
    void Tick() {
        nx_h743_spi_dma_factory_step();
        EXPECT_LT(Dispatch(), 8U);
        if (g_nx_stm32h743_system.started) {
            nx_spi_port_service(Port());
#if D2_UART_MODE >= 0
            nx_uart_port_service(Uart());
#endif
        }
    }
    void Pump(unsigned count = 2000U) {
        for (unsigned n = 0U; n < count; ++n)
            Tick();
    }
    void Submit(size_t count = payload_bytes,
                nx_time_us_t deadline = 1000000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_spi_request_prepare(&request, memory, memory + 0x1000U,
                                         count, deadline));
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(Device(), &request));
    }
    void Check(nx_result_t result, size_t count) {
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(result, request.base.result);
        EXPECT_EQ(count, request.base.transferred);
        EXPECT_EQ(nullptr, Owner()->active);
        EXPECT_EQ(0U, DMA1_Stream2->M0AR | DMA1_Stream3->M0AR |
                          DMA1_Stream2->PAR | DMA1_Stream3->PAR);
        EXPECT_EQ(0U, g_h743_model.spi_reset_order_errors);
        EXPECT_EQ(0U, g_h743_model.spi_selected_control_losses);
        EXPECT_EQ(0U, g_h743_spi_dma_model.ordering_errors);
    }
    nx_result_t Stop() {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned n = 0U; n < 2000U && result == NX_ERROR_BUSY; ++n) {
            Tick();
            result = nx_platform_stop();
        }
        EXPECT_EQ(NX_SUCCESS, result);
        return result;
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        if (Owner()->active != nullptr) {
            EXPECT_EQ(NX_SUCCESS, nx_spi_port_cancel(Port(), Owner()->active));
        }
#if D2_UART_MODE == 0
        if (nx_request_state(&uart_request.base) != NX_REQUEST_READY &&
            nx_request_state(&uart_request.base) != NX_REQUEST_SETTLED)
            (void)nx_uart_port_cancel(Uart(), &uart_request);
#elif D2_UART_MODE == 1
        if (g_nx_stm32h743_system.started)
            (void)nx_uart_port_rx_stop(Uart());
#endif
        if (Stop() != NX_SUCCESS || Owner()->active != nullptr)
            std::abort();
        nx_stream_block_t block{};
        while (nx_stream_acquire(&stream, &block) == NX_SUCCESS) {
            if (nx_stream_release(&stream, &block) != NX_SUCCESS)
                std::abort();
        }
        if (stream.outstanding != 0U)
            std::abort();
        EXPECT_EQ(nullptr, Owner()->active);
        EXPECT_EQ(0U, g_h743_spi_dma_model.gated_accesses);
        EXPECT_EQ(0U, g_h743_spi_dma_factory_model.model_errors);
        EXPECT_EQ(0U, g_h743_spi_dma_model.cache_errors);
        EXPECT_EQ(0, munmap(memory, 0x80000U));
    }
};
TEST_F(H743SpiDmaFactory, PureLookupHasNoHardwareEffects) {
    EXPECT_EQ(&nx_stm32h743_spi_dma_ops, Port()->ops);
    EXPECT_EQ(&nx_stm32h743_spi_dma_endpoint_ops, Device()->ops);
    EXPECT_EQ(Port(), nx_factory_spi(NX_SPI_ID_BUS4));
    EXPECT_EQ(Device(), nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE4));
    EXPECT_EQ(0U, g_h743_model.reads | g_h743_model.writes |
                      g_h743_spi_dma_model.reads | g_h743_spi_dma_model.writes);
}
TEST_F(H743SpiDmaFactory, ActualSystemAdmitsCanonicalFacesAndAxiOnly) {
    Start();
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(H743_SPI_DMA_CACHE_ON != 0,
              (g_h743_model.core->CCR & SCB_CCR_DC_Msk) != 0U);
    EXPECT_EQ(1U, Owner()->region_count);
    EXPECT_EQ(0x24000000U, Owner()->regions[0].begin);
    EXPECT_EQ(0x80000U, Owner()->regions[0].size);
    EXPECT_EQ(Port(), Owner()->face);
    auto* endpoint =
        static_cast<nx_stm32h743_spi_dma_endpoint_t*>(Device()->context);
    EXPECT_EQ(Owner(), endpoint->dma);
    EXPECT_EQ(Device(), endpoint->face);
    EXPECT_EQ(GPIOE, endpoint->wire.cs->registers);
    EXPECT_EQ(5U, g_h743_model.priority[13]);
    EXPECT_EQ(5U, g_h743_model.priority[14]);
    EXPECT_EQ(5U, g_h743_model.priority[84]);
}
TEST_F(H743SpiDmaFactory, PublicRequestUsesRealPayloadAndGeneratedVectors) {
    Start();
    Submit();
    Pump();
    Check(NX_SUCCESS, payload_bytes);
    for (size_t n = 0U; n < payload_bytes; ++n)
        EXPECT_EQ(static_cast<uint8_t>(memory[n] ^ 0xffU), memory[0x1000U + n]);
    EXPECT_EQ(0U, g_h743_spi_dma_model.eot_clears_in_irq);
}
TEST_F(H743SpiDmaFactory, EverySelectedSpiVectorRejectsForeignOwner) {
    for (int irq : {13, 14, 84}) {
        g_h743_model.irq_enabled[irq] = true;
        unsigned writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
        EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
        EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        g_h743_model.irq_enabled[irq] = false;
    }
}
TEST_F(H743SpiDmaFactory, IncomingMaskPreventsExternalIrqDelivery) {
    Start();
    Submit();
    g_h743_model.irq_pending[13] = true;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(0U, Dispatch());
    EXPECT_TRUE(g_h743_model.irq_pending[13]);
    nx_arch_irq_restore(saved);
    Pump();
    Check(NX_SUCCESS, payload_bytes);
}
TEST_F(H743SpiDmaFactory, SharedDmaResetCannotGrantColdOwnership) {
    for (uint32_t reset : {static_cast<uint32_t>(RCC_AHB1RSTR_DMA1RST),
                           static_cast<uint32_t>(RCC_AHB1RSTR_DMA2RST)}) {
        RCC->AHB1RSTR |= reset;
        unsigned writes = g_h743_model.writes + g_h743_spi_dma_model.writes;
        auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(writes, g_h743_model.writes + g_h743_spi_dma_model.writes);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        EXPECT_NE(0U, RCC->AHB1RSTR & reset);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        RCC->AHB1RSTR &= ~reset;
    }
}
TEST_F(H743SpiDmaFactory, HiddenForeignAddressIsProbedAfterGateBeforePins) {
    DMA1_Stream3->M0AR = 0x24007654U;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    EXPECT_EQ(0x24007654U, DMA1_Stream3->M0AR);
}
TEST_F(H743SpiDmaFactory, ResetAfterGateAcquisitionRejectsBeforePinLease) {
    for (uint32_t reset : {static_cast<uint32_t>(RCC_AHB1RSTR_DMA1RST),
                           static_cast<uint32_t>(RCC_AHB1RSTR_DMA2RST)}) {
        g_h743_spi_dma_factory_model.reset_on_dma_enable = true;
        g_h743_spi_dma_factory_model.reset_mask = reset;
        unsigned events = g_h743_spi_dma_factory_model.reset_events;
        unsigned pins = g_h743_model.gpio_writes;
        auto result = nx_platform_start();
        EXPECT_EQ(events + 1U, g_h743_spi_dma_factory_model.reset_events);
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(pins, g_h743_model.gpio_writes);
        EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
        EXPECT_NE(0U, RCC->AHB1RSTR & reset);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        RCC->AHB1RSTR &= ~reset;
    }
}
TEST_F(H743SpiDmaFactory, ForeignPadCannotAuthorizeAnotherPinRestore) {
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    GPIOE->MODER = (GPIOE->MODER & ~(3U << 28U)) | (1U << 28U);
    uint32_t original = GPIOE->MODER;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(original, GPIOE->MODER);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
}
TEST_F(H743SpiDmaFactory, LegalNonVParentDoesNotAuthorizeDmaMode) {
    g_h743_model.silicon_id = 0x10030450U;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_FALSE(Owner()->retained);
}
TEST_F(H743SpiDmaFactory, TrimChangesAfterSystemBeforeSpiInitialization) {
    g_h743_spi_dma_factory_model.trim_after_pins = true;
    auto result = nx_platform_start();
    EXPECT_EQ(1U, g_h743_spi_dma_factory_model.trim_events);
    EXPECT_EQ(NX_ERROR_STATE, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(Owner()->retained);
}
TEST_F(H743SpiDmaFactory, StopDoesNotCancelOrRevokeActiveRequest) {
    Start();
    Submit(64U);
    Pump(20U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_EQ(&request, Owner()->active);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(Port(), &request));
    Pump();
    Check(NX_ERROR_CANCELLED, 0U);
    Stop();
}
TEST_F(H743SpiDmaFactory, DeadlineFirstCauseAndRealDrainRemainBorrowed) {
    Start();
    Submit(64U, nx_deadline_after(nx_time_now_us(), 24U));
    Pump();
    Check(NX_ERROR_TIMEOUT, 0U);
}
TEST_F(H743SpiDmaFactory, CsRefusalRetainsPinsGateAndSystemForRetry) {
    Start();
    Submit();
    g_h743_model.spi_hold_cs_high = true;
    Pump(1000U);
    EXPECT_EQ(&request, Owner()->active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    g_h743_model.spi_hold_cs_high = false;
    Pump();
    Check(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDmaFactory, GateWithdrawalRefusalKeepsActualSharedContribution) {
    Start();
    Pump();
    g_h743_model.ignore_write = &RCC->AHB1ENR;
    nx_result_t result = NX_ERROR_BUSY;
    for (unsigned n = 0U; n < 100U && result == NX_ERROR_BUSY; ++n) {
        Tick();
        result = nx_platform_stop();
    }
    EXPECT_EQ(NX_ERROR_IO, result);
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    g_h743_model.ignore_write = nullptr;
    Stop();
}
TEST_F(H743SpiDmaFactory, PreexistingSharedGateAndNeighborArePreserved) {
    constexpr unsigned lane = D2_UART_MODE == 0 ? 1U : 0U;
    auto& foreign = g_h743_spi_dma_model.streams[lane];
    auto& mux = g_h743_spi_dma_model.mux[lane];
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    foreign.CR = DMA_SxCR_EN;
    foreign.M0AR = 0x24007000U;
    mux.CCR = 99U;
    Start();
    Submit();
    Pump();
    Check(NX_SUCCESS, payload_bytes);
    Stop();
    EXPECT_EQ(DMA_SxCR_EN, foreign.CR);
    EXPECT_EQ(0x24007000U, foreign.M0AR);
    EXPECT_EQ(99U, mux.CCR);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
}
#if D2_UART_MODE == 0
TEST_F(H743SpiDmaFactory, IndependentUartMemoryAndWireCompletionShareOneGate) {
    Start();
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&uart_request, memory, 8U, 100000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Uart(), &uart_request));
    Submit();
    Pump(200U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&uart_request.base));
    nx_result_t stopped = NX_ERROR_BUSY;
    for (unsigned n = 0U; n < 100U && stopped == NX_ERROR_BUSY; ++n) {
        stopped = nx_spi_port_stop(Port());
        Tick();
    }
    EXPECT_EQ(NX_SUCCESS, stopped);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN);
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&uart_request.base));
    EXPECT_EQ(NX_SUCCESS, uart_request.base.result);
    EXPECT_EQ(8U, g_h743_spi_dma_factory_model.sent_count);
    EXPECT_EQ(0, std::memcmp(memory, g_h743_spi_dma_factory_model.sent, 8U));
}
TEST_F(H743SpiDmaFactory, RefusedTcMaskIsolatesDeliveryUntilTaskRetry) {
    Start();
    ASSERT_EQ(NX_SUCCESS,
              nx_uart_tx_prepare(&uart_request, memory, 8U, 100000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(Uart(), &uart_request));
    for (unsigned n = 0U; n < 1000U && (USART3->CR1 & USART_CR1_TCIE) == 0U;
         ++n)
        Tick();
    ASSERT_NE(0U, USART3->CR1 & USART_CR1_TCIE);
    ASSERT_EQ(0U, USART3->ISR & USART_ISR_TC);
    g_h743_model.ignore_write = &USART3->CR1;
    for (unsigned n = 0U; n < 1000U && (USART3->ISR & USART_ISR_TC) == 0U; ++n)
        Tick();
    EXPECT_NE(0U, USART3->ISR & USART_ISR_TC);
    EXPECT_FALSE(g_h743_model.irq_enabled[39]);
    EXPECT_TRUE(nx_h743_spi_dma_factory_level(39));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&uart_request.base));
    auto* uart_owner =
        static_cast<nx_stm32h743_uart_dma_state_t*>(Uart()->context);
    EXPECT_EQ(&uart_request, uart_owner->uart.active);
    EXPECT_EQ(NX_SUCCESS, uart_owner->uart.terminal);
    EXPECT_EQ(0U, Dispatch());
    g_h743_model.hold_irq_on = true;
    g_h743_model.ignore_write = nullptr;
    Pump(20U);
    EXPECT_FALSE(g_h743_model.irq_enabled[39]);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&uart_request.base));
    EXPECT_EQ(&uart_request, uart_owner->uart.active);
    EXPECT_EQ(NX_SUCCESS, uart_owner->uart.terminal);
    g_h743_model.hold_irq_on = false;
    Pump(20U);
    EXPECT_TRUE(g_h743_model.irq_enabled[39]);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&uart_request.base));
    EXPECT_EQ(NX_SUCCESS, uart_request.base.result);
    EXPECT_EQ(8U, uart_request.base.transferred);
    EXPECT_EQ(8U, g_h743_spi_dma_factory_model.sent_count);
}
TEST_F(H743SpiDmaFactory, RefusedTcMaskAndDeliveryIsolationRetainFaultLoan) {
    Start();
    auto* trace = static_cast<UartFaultTrace*>(
        mmap(nullptr, 4096U, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
             -1, 0));
    ASSERT_NE(MAP_FAILED, trace);
    new (trace) UartFaultTrace{};
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        uart_fault_trace = trace;
        uart_fault_owner =
            static_cast<nx_stm32h743_uart_dma_state_t*>(Uart()->context);
        if (nx_uart_tx_prepare(&uart_request, memory, 8U, 100000U) !=
                NX_SUCCESS ||
            nx_uart_port_submit(Uart(), &uart_request) != NX_SUCCESS)
            _exit(3);
        for (unsigned n = 0U; n < 1000U && (USART3->CR1 & USART_CR1_TCIE) == 0U;
             ++n)
            Tick();
        if ((USART3->CR1 & USART_CR1_TCIE) == 0U)
            _exit(4);
        g_h743_model.ignore_write = &USART3->CR1;
        g_h743_model.hold_irq_off = true;
        Pump(200U);
        _exit(5);
    }
    int status = 0;
    bool exited = false;
    for (unsigned n = 0U;
         n < 300U && !__atomic_load_n(&trace->observed, __ATOMIC_ACQUIRE);
         ++n) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            exited = true;
            break;
        }
        usleep(1000U);
    }
    EXPECT_FALSE(exited) << status;
    EXPECT_TRUE(trace->observed);
    EXPECT_TRUE(trace->masked);
    EXPECT_TRUE(trace->borrowed);
    EXPECT_TRUE(trace->enabled);
    EXPECT_TRUE(trace->level);
    EXPECT_EQ(static_cast<unsigned>(NX_REQUEST_QUARANTINED), trace->state);
    EXPECT_EQ(55U, trace->exception);
    if (!exited) {
        EXPECT_EQ(0, kill(pid, SIGKILL));
        EXPECT_EQ(pid, waitpid(pid, &status, 0));
        EXPECT_TRUE(WIFSIGNALED(status));
    }
    /* Parent process reclamation is not provider cleanup or loan settlement. */
    trace->~UartFaultTrace();
    EXPECT_EQ(0, munmap(trace, 4096U));
}
#elif D2_UART_MODE == 1
TEST_F(H743SpiDmaFactory, RxPendingCommitKeepsEnableAndStopResponsibility) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Uart(), &stream));
    g_h743_spi_dma_factory_model.hold_commit = true;
    const uint8_t input = 0x3aU;
    nx_h743_spi_dma_factory_feed(&input, 1U);
    Pump(20U);
    ASSERT_TRUE(g_h743_spi_dma_factory_model.rx_pending);
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(Uart()));
    EXPECT_NE(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, g_h743_spi_dma_factory_model.memory_commits);
    g_h743_spi_dma_factory_model.hold_commit = false;
    Pump(20U);
    EXPECT_EQ(0U, DMA1_Stream1->CR & DMA_SxCR_EN);
    EXPECT_EQ(1U, g_h743_spi_dma_factory_model.memory_commits);
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_rx_stop(Uart()));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(1U, block.length);
    EXPECT_EQ(input, block.data[0]);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(Uart()));
}
TEST_F(H743SpiDmaFactory, IndependentRxBlockCommitDoesNotUseSpiPendingCell) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_rx_start(Uart(), &stream));
    const uint8_t input[8] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};
    nx_h743_spi_dma_factory_feed(input, sizeof(input));
    Submit();
    Pump(300U);
    Check(NX_SUCCESS, 8U);
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(8U, block.length);
    EXPECT_EQ(0, std::memcmp(input, block.data, 8U));
    g_h743_spi_dma_factory_model.hold_en[1] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32h743_system.started);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    g_h743_spi_dma_factory_model.hold_en[1] = false;
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_rx_stop(Uart()));
}
#endif
