/**
 * \file            gd32_spi_dma_test.cpp
 * \brief           Real GD32 SPI/DMA register algorithm ownership contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_spi_dma.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
void nx_gd32_spi_dma_model_disable(bool receive, uint32_t mask);
void nx_gd32_spi_dma_model_ack(bool receive, uint32_t mask);
void USART0_IRQHandler(void);
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool s_hold_dma;

/** \brief Inject only the external engine-enable withdrawal hardware port. */
extern "C" void nx_gd32_spi_dma_model_disable(bool receive, uint32_t mask) {
    if (s_hold_dma) {
        mask &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    }
    if (receive) {
        DMA_CH3CTL(DMA1) &= ~mask;
    } else {
        DMA_CH4CTL(DMA1) &= ~mask;
    }
}

/** \brief Preserve neighbouring DMA flags while modeling selected W1C. */
extern "C" void nx_gd32_spi_dma_model_ack(bool receive, uint32_t mask) {
    if (receive) {
        DMA_INTC0(DMA1) = mask;
        DMA_INTF0(DMA1) &= ~mask;
    } else {
        DMA_INTC1(DMA1) = mask;
        DMA_INTF1(DMA1) &= ~mask;
    }
}

/** \brief No UART is implicitly present in the SPI-only register fixture. */
extern "C" void USART0_IRQHandler(void) {
}

class SpiWakeObserver {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t Callback(void* context) {
        return static_cast<SpiWakeObserver*>(context)->notify();
    }
};

class GD32SPIDMA : public testing::Test {
  protected:
    nx_gd32_spi_dma_state_t state = {};
    nx_gd32_spi_dma_endpoint_state_t device = {};
    nx_dma_memory_region_t region = {UINT32_C(0x20000000), 0x10000U,
                                     NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    const nx_spi_port_t bus = {&nx_gd32_spi_dma_ops, &state};
    const nx_spi_endpoint_t endpoint = {&nx_gd32_spi_dma_endpoint_ops, &device};
    nx_spi_request_t request = {};
    uint8_t* source = reinterpret_cast<uint8_t*>(UINT32_C(0x20000100));
    uint8_t* destination = reinterpret_cast<uint8_t*>(UINT32_C(0x20000200));
    void* registers = MAP_FAILED;
    void* memory = MAP_FAILED;

    void SetUp() override {
        registers =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(registers, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        memory = mmap(reinterpret_cast<void*>(region.begin), region.size,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(memory, reinterpret_cast<void*>(region.begin));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 100U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        s_hold_dma = false;
        state.regions = &region;
        state.region_count = 1U;
        ASSERT_EQ(nx_gd32_spi_dma_initialize(&state, &device, GPIOF, GPIO_PIN_6,
                                             1000000U, 0U, 5U),
                  NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(
            nx_spi_request_prepare(&request, source, destination, 4U, 1000U),
            NX_SUCCESS);
    }

    void TearDown() override {
        if (memory != MAP_FAILED) {
            EXPECT_EQ(munmap(memory, region.size), 0);
        }
        if (registers != MAP_FAILED) {
            EXPECT_EQ(munmap(registers, 0x80000U), 0);
        }
    }

    void CompleteMemory() {
        DMA_CH3CNT(DMA1) = 0U;
        DMA_CH4CNT(DMA1) = 0U;
        DMA_INTF0(DMA1) |= DMA_INTF_FTFIF << 22U;
        DMA_INTF1(DMA1) |= DMA_INTF_FTFIF;
        nx_gd32_spi_dma_irq(&state, true);
        nx_gd32_spi_dma_irq(&state, false);
    }
};

TEST_F(GD32SPIDMA, ProgramsExactIndependentDmaRoutesWithoutCopying) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(DMA_CH3CTL(DMA1) & DMA_CHXCTL_PERIEN, 2U << 25U);
    EXPECT_EQ(DMA_CH4CTL(DMA1) & DMA_CHXCTL_PERIEN, 2U << 25U);
    EXPECT_EQ(DMA_CH3CTL(DMA1) & DMA_CHXCTL_TM, 0U);
    EXPECT_EQ(DMA_CH4CTL(DMA1) & DMA_CHXCTL_TM, 1U << 6U);
    EXPECT_EQ(DMA_CH3M0ADDR(DMA1), UINT32_C(0x20000200));
    EXPECT_EQ(DMA_CH4M0ADDR(DMA1), UINT32_C(0x20000100));
    EXPECT_EQ(DMA_CH3PADDR(DMA1), SPI4 + 0xCU);
    EXPECT_EQ(DMA_CH4PADDR(DMA1), SPI4 + 0xCU);
    EXPECT_EQ(DMA_CH3CNT(DMA1), 4U);
    EXPECT_EQ(DMA_CH4CNT(DMA1), 4U);
    EXPECT_NE(DMA_CH3CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_NE(DMA_CH4CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6 << 16U);
    EXPECT_EQ(SPI_CTL1(SPI4), SPI_CTL1_DMAREN | SPI_CTL1_DMATEN);
}

TEST_F(GD32SPIDMA, BothDmaCompletionsRetainCsUntilIndependentWireIdle) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    SPI_STAT(SPI4) = SPI_STAT_TRANS;
    CompleteMemory();
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6 << 16U);
    SPI_STAT(SPI4) = SPI_STAT_TBE;
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_FALSE(state.spi.active);
}

TEST_F(GD32SPIDMA, RejectedDomainsOverlapAndExpiredDeadlineHaveZeroEffects) {
    const uint32_t control = SPI_CTL0(SPI4);
    request.rx = reinterpret_cast<uint8_t*>(UINT32_C(0x10000000));
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_ERROR_PERMISSION);
    request.rx = source + 1U;
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_ERROR_INVALID);
    request.rx = destination;
    request.base.deadline = 100U;
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(SPI_CTL0(SPI4), control);
    EXPECT_EQ(DMA_CH3CTL(DMA1), 0U);
    EXPECT_EQ(DMA_CH4CTL(DMA1), 0U);
    EXPECT_EQ(GPIO_BOP(GPIOF), 0U);
}

TEST_F(GD32SPIDMA, EndpointIdentityAndBusySecondChildCannotAcquireCs) {
    nx_gd32_spi_dma_state_t other = {};
    const nx_spi_port_t wrong = {&nx_gd32_spi_dma_ops, &other};
    EXPECT_TRUE(nx_spi_endpoint_on_port(&endpoint, &bus));
    EXPECT_FALSE(nx_spi_endpoint_on_port(&endpoint, &wrong));
    nx_gd32_spi_dma_endpoint_state_t child = {};
    ASSERT_EQ(nx_gd32_spi_endpoint_initialize(&state.spi, &child.endpoint,
                                              GPIOA, GPIO_PIN_4, 2000000U, 3U),
              NX_SUCCESS);
    child.dma = &state;
    const nx_spi_endpoint_t second = {&nx_gd32_spi_dma_endpoint_ops, &child};
    nx_spi_request_t another = {};
    nx_request_initialize(&another.base);
    ASSERT_EQ(nx_spi_request_prepare(&another, source, destination, 4U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(nx_spi_endpoint_submit(&second, &another), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&another.base), NX_REQUEST_READY);
    EXPECT_EQ(GPIO_BOP(GPIOA), 0U);
}

TEST_F(GD32SPIDMA, CancelRetainsBorrowAndCsUntilQuarantineActuallyDrains) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    DMA_CH3CNT(DMA1) = 1U;
    DMA_CH4CNT(DMA1) = 2U;
    SPI_STAT(SPI4) = SPI_STAT_TRANS;
    ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    g_gd32_model_now += 1000U;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_ERROR_BUSY);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6 << 16U);
    SPI_STAT(SPI4) = SPI_STAT_TBE;
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(nx_spi_port_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(NVIC->ISER[1] & ((1U << 27U) | (1U << 28U)), 0U);
}

TEST_F(GD32SPIDMA, RefusedEngineDisableKeepsBorrowDespiteIdleWire) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    s_hold_dma = true;
    ASSERT_EQ(nx_spi_port_cancel(&bus, &request), NX_SUCCESS);
    nx_spi_port_service(&bus);
    EXPECT_EQ(state.active, &request);
    g_gd32_model_now += 1000U;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6 << 16U);
    s_hold_dma = false;
    DMA_CH3CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    DMA_CH4CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(GD32SPIDMA, DmaErrorRequiresExplicitIdleRecoveryAndNeverReplays) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    DMA_INTF0(DMA1) = DMA_INTF_TAEIF << 22U;
    nx_gd32_spi_dma_irq(&state, true);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    ASSERT_EQ(nx_spi_request_prepare(&request, source, destination, 4U, 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_ERROR_STATE);
    ASSERT_EQ(nx_spi_port_recover(&bus), NX_SUCCESS);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(DMA_CH3CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    EXPECT_EQ(DMA_CH4CTL(DMA1) & DMA_CHXCTL_CHEN, 0U);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
}

TEST_F(GD32SPIDMA, WakeFollowsLatchedFactsAndLateIrqsNeverReplayCompletion) {
    testing::StrictMock<SpiWakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, SpiWakeObserver::Callback, true};
    ASSERT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5U), NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    testing::InSequence ordered;
    EXPECT_CALL(observer, notify()).WillOnce(testing::Invoke([this]() {
        EXPECT_EQ(g_gd32_model_mask, 0U);
        EXPECT_TRUE(state.rx_complete);
        EXPECT_EQ(state.active, &request);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
        return NX_SUCCESS;
    }));
    EXPECT_CALL(observer, notify()).WillOnce(testing::Invoke([this]() {
        EXPECT_EQ(g_gd32_model_mask, 0U);
        EXPECT_TRUE(state.tx_complete);
        EXPECT_EQ(state.active, &request);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
        return NX_SUCCESS;
    }));
    CompleteMemory();
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    request.tx = nullptr;
    request.rx = nullptr;
    request.length = 0U;
    DMA_INTF0(DMA1) = (DMA_INTF_FTFIF << 22U) | DMA_INTF_FTFIF;
    DMA_INTF1(DMA1) = DMA_INTF_FTFIF | (DMA_INTF_FTFIF << 22U);
    nx_gd32_spi_dma_irq(&state, true);
    nx_gd32_spi_dma_irq(&state, false);
    EXPECT_EQ(DMA_INTF0(DMA1), DMA_INTF_FTFIF);
    EXPECT_EQ(DMA_INTF1(DMA1), DMA_INTF_FTFIF << 22U);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(GD32SPIDMA, ErrorWakeObservesLatchedDrainAfterRestoringIncomingMask) {
    testing::StrictMock<SpiWakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, SpiWakeObserver::Callback, true};
    ASSERT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5U), NX_SUCCESS);
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_CALL(observer, notify()).WillOnce(testing::Invoke([this]() {
        EXPECT_EQ(g_gd32_model_mask, 0U);
        EXPECT_TRUE(g_gd32_model_isr);
        EXPECT_TRUE(state.draining);
        EXPECT_EQ(state.active, &request);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
        return NX_SUCCESS;
    }));
    DMA_INTF0(DMA1) = DMA_INTF_TAEIF << 22U;
    g_gd32_model_isr = true;
    nx_gd32_spi_dma_irq(&state, true);
    g_gd32_model_isr = false;
    EXPECT_EQ(g_gd32_model_mask, 0U);
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}

TEST_F(GD32SPIDMA, BothActualIrqPrioritiesMustPermitKernelCallingWake) {
    testing::StrictMock<SpiWakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, SpiWakeObserver::Callback, true};
    NVIC_SetPriority(DMA1_Channel3_IRQn, 3U);
    EXPECT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, nullptr);
    NVIC_SetPriority(DMA1_Channel3_IRQn, 5U);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 4U);
    EXPECT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, nullptr);
    g_gd32_model_isr = true;
    EXPECT_EQ(nx_spi_port_attach_wake(&bus, &wake, 5U), NX_ERROR_CONTEXT);
}

TEST_F(GD32SPIDMA,
       FalseTransferCompleteCounterIsAnErrorAndCannotSettleSuccess) {
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    DMA_CH3CNT(DMA1) = 1U;
    DMA_INTF0(DMA1) = DMA_INTF_FTFIF << 22U;
    nx_gd32_spi_dma_irq(&state, true);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(GD32SPIDMA, CompletedMemoryWithUnboundedRequestStillHasBoundedDrain) {
    request.base.deadline = NX_DEADLINE_NEVER;
    ASSERT_EQ(nx_spi_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    s_hold_dma = true;
    CompleteMemory();
    g_gd32_model_now += 1000U;
    nx_spi_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6 << 16U);
    s_hold_dma = false;
    DMA_CH3CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    DMA_CH4CTL(DMA1) &= ~static_cast<uint32_t>(DMA_CHXCTL_CHEN);
    nx_spi_port_service(&bus);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
