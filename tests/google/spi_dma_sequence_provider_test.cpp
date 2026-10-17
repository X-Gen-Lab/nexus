/**
 * \file            spi_dma_sequence_provider_test.cpp
 * \brief           Production DMA width, CS continuity and retained chain drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "spi_dma_sequence.h"
#if defined(NX_SPI_TRANSACTION_STM32)
#include "nexus/io/native/model.h"
#include "stm32f407_dma.h"
extern const nx_spi_dma_sequence_backend_t nx_stm32_spi_dma_sequence_backend;
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
#define DMA_STATE    nx_stm32_spi_dma_state_t
#define DEVICE_STATE nx_stm32_spi_dma_endpoint_state_t
#define BACKEND      nx_stm32_spi_dma_sequence_backend
#define IRQ          nx_stm32_spi_dma_irq
#define RXCR         DMA2_Stream0->CR
#define TXCR         DMA2_Stream3->CR
#define RXCOUNT      DMA2_Stream0->NDTR
#define TXCOUNT      DMA2_Stream3->NDTR
#define WIRE_CONTROL SPI1->CR1
#define DMA_REQUESTS SPI1->CR2
#define STATUS       SPI1->SR
#define TXEMPTY      SPI_SR_TXE
#define BUSY         SPI_SR_BSY
#define CS           GPIOC->BSRR
#else
#include "gd32f470_spi_dma.h"
#include "gd32f4xx.h"
extern const nx_spi_dma_sequence_backend_t nx_gd32_spi_dma_sequence_backend;
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
#define DMA_STATE    nx_gd32_spi_dma_state_t
#define DEVICE_STATE nx_gd32_spi_dma_endpoint_state_t
#define BACKEND      nx_gd32_spi_dma_sequence_backend
#define IRQ          nx_gd32_spi_dma_irq
#define RXCR         DMA_CH3CTL(DMA1)
#define TXCR         DMA_CH4CTL(DMA1)
#define RXCOUNT      DMA_CH3CNT(DMA1)
#define TXCOUNT      DMA_CH4CNT(DMA1)
#define WIRE_CONTROL SPI_CTL0(SPI4)
#define DMA_REQUESTS SPI_CTL1(SPI4)
#define STATUS       SPI_STAT(SPI4)
#define TXEMPTY      SPI_STAT_TBE
#define BUSY         SPI_STAT_TRANS
#define CS           GPIO_BOP(GPIOC)
#endif
}
#include <gtest/gtest.h>
#if defined(NX_SPI_DMA_OWNER_TEST)
#include "nexus/arch/arch.h"
#include "nexus/components/spi_dma_owner.h"
#include "spi_dma_sequence_owner.h"
#include <gmock/gmock.h>
extern "C" {
#if defined(NX_SPI_TRANSACTION_STM32)
bool nx_stm32_spi_dma_owner_contains(const nx_spi_dma_transaction_endpoint_t*,
                                     const nx_spi_dma_transaction_port_t*);
nx_result_t
nx_stm32_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t*,
                                   const nx_irq_wake_t*, uint8_t);
#define OWNER_CONTAINS nx_stm32_spi_dma_owner_contains
#define OWNER_ATTACH   nx_stm32_spi_dma_owner_attach_wake
#else
bool nx_gd32_spi_dma_owner_contains(const nx_spi_dma_transaction_endpoint_t*,
                                    const nx_spi_dma_transaction_port_t*);
nx_result_t
nx_gd32_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t*,
                                  const nx_irq_wake_t*, uint8_t);
#define OWNER_CONTAINS nx_gd32_spi_dma_owner_contains
#define OWNER_ATTACH   nx_gd32_spi_dma_owner_attach_wake
#endif
}
#endif
#include <sys/mman.h>

namespace {
volatile uint32_t* held;
bool busy_on_detach;
bool busy_on_final_disable;
class SPIDMAProviderSequence : public testing::Test {
  protected:
    DMA_STATE state{};
    DEVICE_STATE child{};
    nx_spi_dma_sequence_state_t sequence{};
    nx_spi_dma_sequence_endpoint_t binding{&sequence, &child};
    nx_spi_dma_transaction_endpoint_t endpoint{
        &nx_spi_dma_sequence_endpoint_ops, &binding};
    nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                       &sequence};
    nx_spi_dma_transaction_request_t request{};
    uint16_t tx[2]{0x1234U, 0xABCDU};
    uint16_t rx[2]{};
    nx_dma_memory_region_t regions[2]{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
#if defined(NX_SPI_TRANSACTION_STM32)
    nx_stm32_gpio_state_t cs{GPIOC, 1U, true, true};
#endif
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, (void*)(uintptr_t)0x40000000U);
        held = nullptr;
        busy_on_detach = false;
        busy_on_final_disable = false;
        regions[0] = {(uintptr_t)tx, sizeof(tx), NX_DMA_MEMORY_READ};
        regions[1] = {(uintptr_t)rx, sizeof(rx), NX_DMA_MEMORY_WRITE};
        state.regions = regions;
        state.region_count = 2U;
#if defined(NX_SPI_TRANSACTION_STM32)
        g_nx_stm32_gpioa_model = {};
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        state.spi.registers = SPI1;
        state.spi.rcc = RCC;
        state.spi.clock_hz = 84000000U;
        state.dma = DMA2;
        state.rx = DMA2_Stream0;
        state.tx = DMA2_Stream3;
        child.endpoint = {&state.spi, &cs, 1U, 1000000U, 0U};
        child.dma = &state;
        ASSERT_EQ(nx_stm32_spi_dma_initialize(&state), NX_SUCCESS);
#else
        g_gd32_model_nvic = {};
        g_gd32_model_now = 1U;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0U;
        ASSERT_EQ(nx_gd32_spi_dma_initialize(&state, &child, GPIOC, 1U,
                                             1000000U, 0U, 6U),
                  NX_SUCCESS);
#endif
        STATUS = TXEMPTY;
        sequence.backend = &BACKEND;
        sequence.context = &state;
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                                 NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void TearDown() override {
        held = nullptr;
        busy_on_detach = false;
        busy_on_final_disable = false;
        if (sequence.active != nullptr) {
            EXPECT_EQ(nx_spi_dma_transaction_cancel(&port, &request),
                      NX_SUCCESS);
            RXCR &= ~1U;
            TXCR &= ~1U;
            STATUS = TXEMPTY;
            nx_spi_dma_transaction_service(&port);
        }
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
#if defined(NX_SPI_TRANSACTION_STM32)
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
#endif
    }
    void Complete() {
        RXCOUNT = 0U;
        TXCOUNT = 0U;
#if defined(NX_SPI_TRANSACTION_STM32)
        DMA2->LISR |= 0x20U | (0x20U << 22U);
#else
        DMA_INTF0(DMA1) |= 0x20U << 22U;
        DMA_INTF1(DMA1) |= 0x20U;
#endif
        IRQ(&state, true);
        IRQ(&state, false);
    }
};
TEST_F(SPIDMAProviderSequence, ColdStopCannotTouchAnUnacquiredController) {
    DMA_STATE cold = state;
    cold.initialized = false;
#if !defined(NX_SPI_TRANSACTION_STM32)
    cold.spi.initialized = false;
#endif
    nx_spi_dma_sequence_state_t prebound{};
    prebound.backend = &BACKEND;
    prebound.context = &cold;
    const nx_spi_dma_transaction_port_t cold_port{&nx_spi_dma_sequence_port_ops,
                                                  &prebound};
    RXCR = 0x1FU;
    TXCR = 0x1FU;
#if defined(NX_SPI_TRANSACTION_STM32)
    WIRE_CONTROL = SPI_CR1_SPE | SPI_CR1_MSTR;
    DMA_REQUESTS = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
    const nx_spi_port_t peer{&nx_stm32_spi_dma_ops, &state};
#else
    WIRE_CONTROL = SPI_CTL0_SPIEN | SPI_CTL0_MSTMOD;
    DMA_REQUESTS = SPI_CTL1_DMATEN | SPI_CTL1_DMAREN;
    const nx_spi_port_t peer{&nx_gd32_spi_dma_ops, &state};
#endif
    uint32_t control = WIRE_CONTROL;
    uint32_t requests = DMA_REQUESTS;
    EXPECT_EQ(nx_spi_dma_transaction_stop(&cold_port), NX_SUCCESS);
    EXPECT_EQ(RXCR, 0x1FU);
    EXPECT_EQ(TXCR, 0x1FU);
    EXPECT_EQ(WIRE_CONTROL, control);
    EXPECT_EQ(DMA_REQUESTS, requests);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(nx_spi_port_stop(&peer), NX_SUCCESS);
}
TEST_F(SPIDMAProviderSequence, HardwareHalfwordWidthsAndCsSpanBothSegments) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(RXCR & (3U << 11U), 1U << 11U);
    EXPECT_EQ(RXCR & (3U << 13U), 1U << 13U);
    EXPECT_EQ(TXCR & (3U << 11U), 1U << 11U);
    EXPECT_EQ(RXCOUNT, 1U);
    EXPECT_EQ(CS, 1U << 16U);
    Complete();
    STATUS = TXEMPTY | BUSY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(sequence.phase, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(sequence.phase, 1U);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_EQ(RXCOUNT, 1U);
    Complete();
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(CS, 1U);
    EXPECT_FALSE(state.spi.active);
}
TEST_F(SPIDMAProviderSequence, FutureDmaDomainRejectsBeforeFirstCsOrBorrow) {
    uint16_t foreign = 0U;
    segments[1].rx = &foreign;
    EXPECT_NE(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(sequence.active, nullptr);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(CS, 0U);
    EXPECT_EQ((RXCR | TXCR) & 1U, 0U);
}
TEST_F(SPIDMAProviderSequence, DmaDomainCannotAuthorizeProviderStateAsPayload) {
    nx_dma_memory_region_t broad[3]{
        regions[0],
        regions[1],
        {(uintptr_t)&state, sizeof(state), NX_DMA_MEMORY_WRITE}};
    state.regions = broad;
    state.region_count = 3U;
    segments[0].rx = &state;
    ASSERT_EQ(nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U,
                                             NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(CS, 0U);
}
TEST_F(SPIDMAProviderSequence,
       WithdrawalRechecksWireBusyBeforePhaseSettlement) {
    nx_time_us_t deadline = nx_time_now_us() + 1000U;
    ASSERT_EQ(
        nx_spi_dma_transaction_prepare(&request, segments, 2U, 16U, deadline),
        NX_SUCCESS);
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
#if defined(NX_SPI_TRANSACTION_STM32)
    ASSERT_EQ(nx_native_clock_advance(2000U), NX_SUCCESS);
#else
    g_gd32_model_now += 2000U;
#endif
    STATUS = TXEMPTY;
    busy_on_detach = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&sequence.phase_request.base),
              NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &sequence.phase_request);
    EXPECT_EQ(CS, 1U << 16U);
    busy_on_detach = false;
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(CS, 1U);
}
TEST_F(SPIDMAProviderSequence, FinalEngineDisablePrecedesWireIdleObservation) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    held = &RXCR;
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    ASSERT_NE(RXCR & 1U, 0U);
    held = nullptr;
    STATUS = TXEMPTY;
    busy_on_final_disable = true;
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&sequence.phase_request.base),
              NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, &sequence.phase_request);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_FALSE(busy_on_final_disable);
    STATUS = TXEMPTY;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(CS, 1U);
}
TEST_F(SPIDMAProviderSequence, CancelAfterFirstPhaseRetainsCurrentDmaAndCs) {
    ASSERT_EQ(nx_spi_dma_transaction_submit(&endpoint, &request), NX_SUCCESS);
    Complete();
    nx_spi_dma_transaction_service(&port);
    ASSERT_EQ(sequence.phase, 1U);
    held = &RXCR;
    ASSERT_EQ(nx_spi_dma_transaction_cancel(&port, &request), NX_SUCCESS);
    nx_spi_dma_transaction_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_EQ(sequence.active, &request);
    held = nullptr;
    RXCR &= ~1U;
    nx_spi_dma_transaction_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(CS, 1U);
}
#if defined(NX_SPI_DMA_OWNER_TEST)
/** \brief Compose actual family DMA, retained sequence and original owner. */
class SPIDMAProviderOwner : public SPIDMAProviderSequence {
  protected:
    const nx_spi_dma_owner_binding_t owner_binding{&port, OWNER_CONTAINS,
                                                   OWNER_ATTACH};
    nx_spi_dma_owner_executor_t executor{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[2]{};
    nx_request_t outer{};
    nx_owner_ticket_t ticket{};
    nx_spi_dma_owner_operation_t operation{&endpoint, segments, 2U, 16U};
    testing::StrictMock<testing::MockFunction<nx_result_t()>> notifications;
    const nx_irq_wake_t wake{
        &notifications,
        [](void* p) {
            return static_cast<testing::MockFunction<nx_result_t()>*>(p)
                ->Call();
        },
        true};
    void SetUp() override {
        SPIDMAProviderSequence::SetUp();
        const auto execution =
            nx_spi_dma_owner_executor_port(&executor, &owner_binding);
        const nx_owner_guard_port_t guard{
            nullptr,
            [](void*) -> uintptr_t { return nx_arch_irq_save().value; },
            [](void*, uintptr_t previous) {
                nx_arch_irq_restore({static_cast<uint32_t>(previous)});
            }};
        const nx_clock_t clock{[](void*) { return nx_time_now_us(); }, nullptr};
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 2U, guard, execution, clock,
                                    nullptr),
                  NX_SUCCESS);
        nx_request_initialize(&outer);
        ASSERT_EQ(nx_request_prepare(&outer, NX_DEADLINE_NEVER), NX_SUCCESS);
    }
    void TearDown() override {
        held = nullptr;
        busy_on_detach = false;
        busy_on_final_disable = false;
        if (owner.capacity != 0U) {
            nx_bus_owner_stop(&owner);
            for (unsigned i = 0U; i < 8U && !nx_bus_owner_idle(&owner); ++i) {
                RXCR &= ~1U;
                TXCR &= ~1U;
                STATUS = TXEMPTY;
                (void)nx_bus_owner_service(&owner);
            }
            EXPECT_TRUE(nx_bus_owner_idle(&owner));
        }
        EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
        SPIDMAProviderSequence::TearDown();
    }
    void StartOwner() {
        ASSERT_EQ(nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr,
                                          &ticket),
                  NX_SUCCESS);
        ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
        ASSERT_EQ(nx_request_state(&outer), NX_REQUEST_ACTIVE);
    }
};
TEST_F(SPIDMAProviderOwner, BothDmaIrqsWakeAfterFactsWhileOneCsSpansChain) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, &wake, 5U),
              NX_SUCCESS);
    EXPECT_CALL(notifications, Call())
        .Times(4)
        .WillRepeatedly(testing::Invoke([&] {
            EXPECT_FALSE(nx_arch_irq_is_masked());
            EXPECT_NE(state.active, nullptr);
            EXPECT_EQ(sequence.active, &executor.inner);
            EXPECT_NE(nx_request_state(&outer), NX_REQUEST_SETTLED);
            return NX_SUCCESS;
        }));
    ASSERT_NO_FATAL_FAILURE(StartOwner());
    EXPECT_EQ(CS, 1U << 16U);
    Complete();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    EXPECT_EQ(sequence.phase, 1U);
    EXPECT_EQ(executor.inner.segments, segments);
    EXPECT_EQ(CS, 1U << 16U);
    Complete();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.result, NX_SUCCESS);
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_EQ(executor.inner.segments, nullptr);
    EXPECT_EQ(CS, 1U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(SPIDMAProviderOwner, ForgedPhysicalEndpointRejectsBeforeOuterAdmission) {
    DMA_STATE foreign{};
    DEVICE_STATE wrong = child;
    wrong.dma = &foreign;
    wrong.endpoint.port = &foreign.spi;
    nx_spi_dma_sequence_endpoint_t forged{&sequence, &wrong};
    const nx_spi_dma_transaction_endpoint_t wrong_face{
        &nx_spi_dma_sequence_endpoint_ops, &forged};
    operation.endpoint = &wrong_face;
    EXPECT_FALSE(nx_spi_dma_owner_contains(&owner_binding, &wrong_face));
    EXPECT_EQ(
        nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr, &ticket),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
    EXPECT_EQ(CS, 0U);
    EXPECT_EQ(sequence.active, nullptr);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
}
TEST_F(SPIDMAProviderOwner, WakePolicyRejectionPreservesPreviousLiveSink) {
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, &wake, 5U),
              NX_SUCCESS);
    const nx_irq_wake_t denied{nullptr, [](void*) { return NX_SUCCESS; }, true};
#if defined(NX_SPI_TRANSACTION_STM32)
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, &denied, 6U),
              NX_ERROR_PERMISSION);
#else
    NVIC_SetPriority(DMA1_Channel3_IRQn, 3U);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, &denied, 5U),
              NX_ERROR_PERMISSION);
    NVIC_SetPriority(DMA1_Channel3_IRQn, 6U);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 4U);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, &denied, 5U),
              NX_ERROR_PERMISSION);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 6U);
#endif
    EXPECT_EQ(state.wake, &wake);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(&owner_binding, nullptr, 0U),
              NX_SUCCESS);
    EXPECT_EQ(state.wake, nullptr);
}
TEST_F(SPIDMAProviderOwner,
       StopKeepsBothBorrowLayersUntilFinalEngineWithdraws) {
    ASSERT_NO_FATAL_FAILURE(StartOwner());
    Complete();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    ASSERT_EQ(sequence.phase, 1U);
    held = &RXCR;
    nx_bus_owner_stop(&owner);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(sequence.active, &executor.inner);
    EXPECT_EQ(executor.inner.segments, segments);
    EXPECT_EQ(CS, 1U << 16U);
    EXPECT_FALSE(nx_bus_owner_idle(&owner));
    held = nullptr;
    RXCR &= ~1U;
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
    EXPECT_EQ(outer.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(outer.transferred, 1U);
    EXPECT_EQ(CS, 1U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    EXPECT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
}
#endif
} /* namespace */
/** \brief W1C applies only the selected actual flag bank. */
extern "C" void nx_spi_multi_dma_ack(volatile uint32_t* clear, uint32_t mask) {
    *clear = mask;
    *(clear - 2) &= ~mask;
}
/** \brief One explicit injected engine refuses source withdrawal. */
extern "C" void nx_spi_multi_dma_disable(volatile uint32_t* control,
                                         uint32_t mask) {
    *control &= ~(control == held ? mask & ~1U : mask);
    if (busy_on_detach) {
        STATUS |= BUSY;
    }
}
/** \brief Apply exact selected reset effects for legacy recovery code. */
extern "C" void nx_spi_multi_reset(volatile uint32_t* reset, uint32_t mask,
                                   bool active) {
    if (active) {
        *reset |= mask;
    } else {
        *reset &= ~mask;
    }
}
#if !defined(NX_SPI_TRANSACTION_STM32)
/** \brief SPI-only fixture selects no UART publisher. */
extern "C" void USART0_IRQHandler(void) {
}
#endif

/** \brief A final bus beat can make SPI busy as DMA EN finally clears. */
extern "C" uint32_t nx_spi_dma_sequence_model_read(volatile uint32_t* control) {
    if (busy_on_final_disable && control == &RXCR) {
        *control &= ~1U;
        STATUS |= BUSY;
        busy_on_final_disable = false;
    }
    return *control;
}
