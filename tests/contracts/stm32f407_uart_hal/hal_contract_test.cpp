/**
 * \file            hal_contract_test.cpp
 * \brief           Complete finite HAL TX and disjoint LL RX contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
extern "C" {
#include "nexus/io/uart.h"
#include "stm32f407_uart_hal.h"
GPIO_TypeDef g_nx_stm32_gpioa_model = {};
nx_stm32_system_t g_nx_stm32_system = {};
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
void nx_hal_test_cpu_reset(void);
bool nx_hal_test_isr(bool next);
void nx_hal_test_restore_hook(void (*hook)(void));
void nx_hal_test_dsb_hook(void (*hook)(void));
}

static unsigned s_wakes;
static IRQn_Type s_hold_vector = static_cast<IRQn_Type>(-1);
static void (*s_pending_clear_hook)(IRQn_Type);
static unsigned s_uart_enables;
static uint32_t s_first_enabled_brr;
static uint32_t s_first_enabled_cr1;
static uint32_t s_first_enabled_cr2;
static volatile uint32_t* s_refused_cold_register;
static uint32_t s_cold_held_bits;
static int s_refused_priority = -1;
static unsigned s_cold_writes;
static void (*s_uart_enable_hook)(UART_HandleTypeDef*);
static bool s_hold_dma_enable;
static unsigned s_global_hal_half_callbacks;
static void (*s_dma_read_hook)(DMA_HandleTypeDef*, uint32_t, uint32_t);

extern "C" void nx_hal_test_dma_observed(DMA_HandleTypeDef* handle,
                                         uint32_t mask, uint32_t observed) {
    if (s_dma_read_hook != nullptr)
        s_dma_read_hook(handle, mask, observed);
}

extern "C" void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef*) {
    ++s_global_hal_half_callbacks;
}
extern "C" void nx_hal_model_dma_disable(DMA_HandleTypeDef* handle) {
    handle->Instance->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    if (s_hold_dma_enable && handle->Instance == DMA2_Stream7)
        handle->Instance->CR |= DMA_SxCR_EN;
}

extern "C" void nx_hal_model_cold_write(volatile uint32_t* reg,
                                        uint32_t value) {
    ++s_cold_writes;
    if (reg == s_refused_cold_register)
        value = (value & ~s_cold_held_bits) | (*reg & s_cold_held_bits);
    for (unsigned bank = 0U; bank < 8U; ++bank) {
        if (reg == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    *reg = value;
}
extern "C" void nx_hal_model_priority_write(volatile uint8_t* reg,
                                            uint8_t value) {
    if (s_refused_priority < 0 ||
        reg != &NVIC->IPR[static_cast<unsigned>(s_refused_priority)])
        *reg = value;
}

/** \brief Model both cold and CMSIS writes at the same NVIC byte port. */
extern "C" void nx_hal_model_nvic_priority(IRQn_Type irq, uint32_t priority) {
    const uint8_t value =
        static_cast<uint8_t>((priority << (8U - __NVIC_PRIO_BITS)) & 0xFFU);
    if (static_cast<int32_t>(irq) >= 0)
        nx_hal_model_priority_write(&NVIC->IPR[static_cast<uint32_t>(irq)],
                                    value);
    else
        nx_hal_model_priority_write(
            &SCB->SHPR[(static_cast<uint32_t>(irq) & 0xFU) - 4U], value);
}

/** \brief Observe actual vendor initialization before the first UE write. */
extern "C" void nx_hal_model_uart_enable(UART_HandleTypeDef* handle) {
    if (s_uart_enables++ == 0U) {
        s_first_enabled_brr = handle->Instance->BRR;
        s_first_enabled_cr1 = handle->Instance->CR1;
        s_first_enabled_cr2 = handle->Instance->CR2;
    }
    handle->Instance->CR1 |= USART_CR1_UE;
    if (s_uart_enable_hook != nullptr)
        s_uart_enable_hook(handle);
}
extern "C" void nx_hal_model_nvic_enable(IRQn_Type irq) {
    NVIC->ISER[static_cast<unsigned>(irq) / 32U] |=
        1U << (static_cast<unsigned>(irq) % 32U);
}
extern "C" void nx_hal_model_nvic_disable(IRQn_Type irq) {
    if (irq == s_hold_vector)
        return;
    const unsigned bank = static_cast<unsigned>(irq) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(irq) % 32U);
    NVIC->ICER[bank] = bit;
    NVIC->ISER[bank] &= ~bit;
}
extern "C" void nx_hal_model_nvic_clear(IRQn_Type irq) {
    const unsigned bank = static_cast<unsigned>(irq) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(irq) % 32U);
    NVIC->ICPR[bank] = bit;
    NVIC->ISPR[bank] &= ~bit;
    if (s_pending_clear_hook != nullptr)
        s_pending_clear_hook(irq);
}
extern "C" uint32_t nx_hal_model_nvic_active(IRQn_Type irq) {
    return (NVIC->IABR[static_cast<unsigned>(irq) / 32U] >>
            (static_cast<unsigned>(irq) % 32U)) &
           1U;
}
/** \brief External timer counting for the real System/HAL time bridge. */
static nx_result_t AdvanceClock(nx_time_us_t amount) {
    if (amount > UINT32_MAX - TIM2->CNT)
        return NX_ERROR_INVALID;
    TIM2->CNT += static_cast<uint32_t>(amount);
    return NX_SUCCESS;
}

/** \brief Copied wake observation cannot release the active request. */
static nx_result_t notify(void*) {
    ++s_wakes;
    return NX_SUCCESS;
}

class F407HalContract : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void* ram = MAP_FAILED;
    uint8_t* payload = nullptr;
    nx_uart_rx_event_t* events = nullptr;
    nx_stm32_uart_hal_state_t state = {};
    /* Config borrows optional format overrides for the fixture lifetime. */
    nx_uart_format_t format = {};
    nx_stm32_uart_dma_cold_config_t config = {};
    nx_dma_memory_region_t region = {};
    IRQn_Type vectors[2] = {USART1_IRQn, DMA2_Stream7_IRQn};
    nx_uart_port_t port = {&nx_stm32_uart_hal_ops, &state};
    nx_uart_tx_request_t request = {};
    nx_irq_wake_t wake = {nullptr, notify, false};

    void SetUp() override {
        nx_hal_test_cpu_reset();
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mmio, MAP_FAILED);
        ram = mmap(reinterpret_cast<void*>(0x20000000U), 0x1000U,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(ram, MAP_FAILED);
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        s_wakes = 0U;
        s_hold_vector = static_cast<IRQn_Type>(-1);
        s_pending_clear_hook = nullptr;
        s_uart_enables = 0U;
        s_uart_enable_hook = nullptr;
        s_hold_dma_enable = false;
        s_global_hal_half_callbacks = 0U;
        s_dma_read_hook = nullptr;
        s_refused_cold_register = nullptr;
        s_cold_held_bits = s_cold_writes = 0U;
        s_refused_priority = -1;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.timer = TIM2;
        g_nx_stm32_system.started = true;
        SystemCoreClock = 168000000U;
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        TIM2->CNT = 100U;
        TIM2->PSC = 83U;
        TIM2->ARR = UINT32_MAX;
        TIM2->CR1 = TIM_CR1_CEN;
        payload = static_cast<uint8_t*>(ram);
        events = reinterpret_cast<nx_uart_rx_event_t*>(payload + 0x400U);
        region = {0x20000000U, 0x1000U,
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        config.uart.registers = USART1;
        config.uart.vectors = vectors;
        config.uart.vector_count = 2U;
        config.uart.rx_storage = events;
        config.uart.rx_capacity = 2U;
        config.uart.profile = NX_UART_RX_EVENTS;
        config.uart.baud = 115200U;
        config.uart.clock_hz = 84000000U;
        config.uart.priority = 5U;
        config.resources = {DMA2, &region, 1U};
        config.transmit = {DMA2_Stream7, DMA2_Stream7_IRQn, 7U, 4U};
        RCC->APB2ENR = RCC_APB2ENR_USART1EN;
        RCC->AHB1ENR = RCC_AHB1ENR_DMA2EN;
        ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 4U, 1000U), NX_SUCCESS);
    }

    void TearDown() override {
        nx_hal_test_restore_hook(nullptr);
        nx_hal_test_dsb_hook(nullptr);
        s_pending_clear_hook = nullptr;
        s_uart_enable_hook = nullptr;
        s_dma_read_hook = nullptr;
        if (ram != MAP_FAILED)
            munmap(ram, 0x1000U);
        if (mmio != MAP_FAILED)
            munmap(mmio, 0x30000U);
    }

    void Submit() {
        ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
        USART1->SR = 0U;
    }

    void UartIrq() {
        bool previous = nx_hal_test_isr(true);
        nx_stm32_uart_hal_irq(&state);
        nx_hal_test_isr(previous);
    }

    void DmaIrq() {
        bool previous = nx_hal_test_isr(true);
        nx_stm32_uart_hal_dma_irq(&state);
        nx_hal_test_isr(previous);
    }

    void DmaComplete() {
        DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        DMA2_Stream7->NDTR = 0U;
        const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
        DmaIrq();
        const_cast<uint32_t&>(DMA2->HISR) = 0U;
    }

    void Receive(uint32_t status, uint8_t byte) {
        USART1->DR = byte;
        USART1->SR = status;
        UartIrq();
        /* SR/DR RX observation clears receive facts, not the independent TC.
         */
        USART1->SR &= ~(uint32_t)(USART_SR_RXNE | USART_SR_PE | USART_SR_FE |
                                  USART_SR_ORE | USART_SR_NE);
    }
};

TEST_F(F407HalContract, InitializesExactHandleParentAndPrivateCallbacks) {
    EXPECT_EQ(state.handle.Instance, USART1);
    EXPECT_EQ(state.handle.hdmatx, &state.dma);
    EXPECT_EQ(state.dma.Parent, &state.handle);
    EXPECT_NE(state.handle.TxCpltCallback, nullptr);
    EXPECT_NE(state.handle.ErrorCallback, nullptr);
    EXPECT_NE(state.handle.AbortTransmitCpltCallback, nullptr);
    EXPECT_EQ(state.handle.RxState, HAL_UART_STATE_READY);
    EXPECT_EQ(state.handle.ReceptionType, HAL_UART_RECEPTION_STANDARD);
    EXPECT_EQ(state.handle.pRxBuffPtr, nullptr);
    EXPECT_EQ(USART1->CR3 & USART_CR3_DMAR, 0U);
    EXPECT_NE(USART1->CR1 & USART_CR1_RXNEIE, 0U);
}

TEST_F(F407HalContract, ExpiredRequestRejectsBeforeHalRetainsPayload) {
    EXPECT_EQ(HAL_RCC_GetPCLK1Freq(), 42000000U);
    EXPECT_EQ(HAL_RCC_GetPCLK2Freq(), 84000000U);
    EXPECT_EQ(nx_time_now_us(), 100U);
    EXPECT_EQ(HAL_GetTick(), 0U);
    ASSERT_EQ(AdvanceClock(900U), NX_SUCCESS);
    EXPECT_EQ(nx_time_now_us(), 1000U);
    EXPECT_EQ(HAL_GetTick(), 1U);
    const uint32_t before = DMA2_Stream7->CR;
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.handle.pTxBuffPtr, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR, before);
}

TEST_F(F407HalContract, MaskedOwnerRejectsWithoutChangingIncomingMask) {
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    nx_arch_irq_restore(saved);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST_F(F407HalContract, FiniteSubmitUsesRealHalWithoutDisarmingRx) {
    Submit();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.handle.gState, HAL_UART_STATE_BUSY_TX);
    EXPECT_EQ(state.handle.pTxBuffPtr, payload);
    EXPECT_EQ(state.dma.Parent, &state.handle);
    EXPECT_EQ(DMA2_Stream7->NDTR, 4U);
    EXPECT_NE(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(USART1->CR3 & USART_CR3_DMAT, 0U);
    EXPECT_NE(USART1->CR1 & USART_CR1_RXNEIE, 0U);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}

TEST_F(F407HalContract, MemoryCompletionStillBorrowsUntilWireAndService) {
    Submit();
    DmaComplete();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    USART1->SR = USART_SR_TC;
    UartIrq();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(state.handle.pTxBuffPtr, nullptr);
    EXPECT_EQ(DMA2_Stream7->M0AR, 0U);
    EXPECT_EQ(state.uart.active, nullptr);
}

TEST_F(F407HalContract, RxAndTcSameIrqRetainBothFactsAndRestoreRxMask) {
    Submit();
    DmaComplete();
    const uint32_t saved1 = USART1->CR1 & (USART_CR1_RXNEIE | USART_CR1_PEIE);
    const uint32_t saved3 = USART1->CR3 & USART_CR3_EIE;
    Receive(USART_SR_RXNE | USART_SR_TC, 0x61U);
    nx_uart_rx_event_t out[2] = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, out, 2U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out[0].byte, 0x61U);
    EXPECT_EQ(out[0].flags, 0U);
    EXPECT_EQ(USART1->CR1 & (USART_CR1_RXNEIE | USART_CR1_PEIE), saved1);
    EXPECT_EQ(USART1->CR3 & USART_CR3_EIE, saved3);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    nx_uart_port_service(&port);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(F407HalContract, RxErrorAndTcPreserveEventWithoutFailingTx) {
    Submit();
    DmaComplete();
    Receive(USART_SR_RXNE | USART_SR_FE | USART_SR_ORE | USART_SR_TC, 0x42U);
    nx_uart_rx_event_t out[2] = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, out, 2U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out[0].byte, 0x42U);
    EXPECT_EQ(out[0].flags, NX_UART_EVENT_FRAMING | NX_UART_EVENT_OVERRUN);
    EXPECT_EQ(out[0].timestamp_us, 100U);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_NE(USART1->CR1 & USART_CR1_RXNEIE, 0U);
}

TEST_F(F407HalContract, StatusOnlyRxErrorRetainsNoByteMeaning) {
    Receive(USART_SR_FE, 0xAAU);
    nx_uart_rx_event_t out = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, &out, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out.byte, 0U);
    EXPECT_EQ(out.flags, NX_UART_EVENT_FRAMING | NX_UART_EVENT_NO_BYTE);
}

TEST_F(F407HalContract, EventOverflowRemainsOrderedBeforeLaterReception) {
    Receive(USART_SR_RXNE, 1U);
    Receive(USART_SR_RXNE, 2U);
    Receive(USART_SR_RXNE, 3U);
    Receive(USART_SR_RXNE, 4U);
    nx_uart_rx_event_t out[4] = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, out, 4U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 3U);
    EXPECT_EQ(out[0].byte, 1U);
    EXPECT_EQ(out[1].byte, 2U);
    EXPECT_EQ(out[2].flags, NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE);
    Receive(USART_SR_RXNE, 5U);
    ASSERT_EQ(nx_uart_port_read_events(&port, out, 4U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out[0].byte, 5U);
}

TEST_F(F407HalContract, CancelRetainsBorrowUntilAbortAndWireAreDrained) {
    Submit();
    DMA2_Stream7->NDTR = 2U;
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    DmaIrq();
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.handle.pTxBuffPtr, nullptr);
    EXPECT_EQ(DMA2_Stream7->M0AR, 0U);
}

TEST_F(F407HalContract, ClosedRxCannotBeReactivatedByALateUartIrq) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    const uint32_t before1 = USART1->CR1;
    const uint32_t before3 = USART1->CR3;
    Receive(USART_SR_RXNE | USART_SR_FE | USART_SR_TC, 0x77U);
    EXPECT_EQ(USART1->CR1, before1);
    EXPECT_EQ(USART1->CR3, before3);
    EXPECT_EQ(state.uart.rx_count, 0U);
    EXPECT_EQ(USART1->CR1 & (USART_CR1_RXNEIE | USART_CR1_PEIE), 0U);
}

TEST_F(F407HalContract, RxWakeDoesNotPublishTxSettlement) {
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    Submit();
    Receive(USART_SR_RXNE, 0x15U);
    EXPECT_GE(s_wakes, 1U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.uart.rx_count, 1U);
}

TEST_F(F407HalContract, PayloadOutsideDeclaredDomainRejectsWithoutHalEntry) {
    request.data = reinterpret_cast<const uint8_t*>(0x10000000U);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.handle.pTxBuffPtr, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, DmaPayloadCannotAliasWholeProviderMetadata) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    auto* aliased = new (payload + 0x800U) nx_stm32_uart_hal_state_t{};
    ASSERT_EQ(nx_stm32_uart_hal_initialize(aliased, &config), NX_SUCCESS);
    const nx_uart_port_t aliased_port = {&nx_stm32_uart_hal_ops, aliased};
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(aliased),
                                 sizeof(*aliased), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&aliased_port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(aliased->uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, DmaPayloadCannotAliasStaticConfiguration) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    auto* copied =
        new (payload + 0xC00U) nx_stm32_uart_dma_cold_config_t(config);
    state = {};
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, copied), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(copied),
                                 sizeof(*copied), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, DmaPayloadCannotBorrowOwnedRxRing) {
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(events),
                                 sizeof(*events), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, RequestMetadataCannotAliasOwnedRxRing) {
    auto* aliased = new (events) nx_uart_tx_request_t{};
    nx_request_initialize(&aliased->base);
    ASSERT_EQ(nx_uart_tx_prepare(aliased, payload, 4U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, aliased), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&aliased->base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, FifoErrorInterruptMustBeWithdrawnBeforeSettlement) {
    Submit();
    DMA2_Stream7->FCR |= DMA_SxFCR_FEIE;
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(DMA2_Stream7->FCR & DMA_SxFCR_FEIE, 0U);
    EXPECT_EQ(DMA2_Stream7->CR & (DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                  DMA_SxCR_TEIE | DMA_SxCR_DMEIE),
              0U);
    EXPECT_EQ(state.dma.XferCpltCallback, nullptr);
    EXPECT_EQ(state.dma.XferErrorCallback, nullptr);
}

TEST_F(F407HalContract, RefusedVectorWithdrawalKeepsWakeAndLifecycleRetained) {
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    s_hold_vector = USART1_IRQn;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_EQ(state.uart.wake, &wake);
    s_hold_vector = static_cast<IRQn_Type>(-1);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(state.uart.wake, nullptr);
    EXPECT_FALSE(state.uart.initialized);
}

TEST_F(F407HalContract, InvalidRemainingCounterCannotPublishSuccess) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    DMA2_Stream7->NDTR = 5U;
    nx_uart_port_service(&port);
    if (nx_request_state(&request.base) == NX_REQUEST_SETTLED) {
        EXPECT_NE(request.base.result, NX_SUCCESS);
        EXPECT_EQ(request.base.transferred, 0U);
    } else {
        EXPECT_EQ(state.uart.active, &request);
    }
}

TEST_F(F407HalContract, LateIrqsCannotModifyReleasedRequest) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_request_t before = request.base;
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    DmaIrq();
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(std::memcmp(&request.base, &before, sizeof(before)), 0);
    EXPECT_EQ(state.uart.active, nullptr);
}

TEST_F(F407HalContract, SevenBitParityReceptionPreservesPayloadAndError) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    format = {7U, NX_UART_PARITY_EVEN, NX_UART_STOP_1, NX_UART_FLOW_NONE};
    config.uart.format = &format;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    Receive(USART_SR_RXNE | USART_SR_PE, 0xFFU);
    nx_uart_rx_event_t out = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, &out, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out.byte, 0x7FU);
    EXPECT_EQ(out.flags, NX_UART_EVENT_PARITY);
}

static nx_result_t s_stop_from_wake;
static unsigned s_task_wakes;
static nx_result_t StopFromWake(void* context) {
    if (!nx_arch_in_isr())
        ++s_task_wakes;
    s_stop_from_wake = nx_uart_port_stop(static_cast<nx_uart_port_t*>(context));
    return NX_SUCCESS;
}

TEST_F(F407HalContract, IrqWakeCannotStopItsOwnLifecycle) {
    const nx_irq_wake_t observed = {&port, StopFromWake, false};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &observed, 5U), NX_SUCCESS);
    s_stop_from_wake = NX_SUCCESS;
    s_task_wakes = 0U;
    Receive(USART_SR_RXNE, 0x33U);
    EXPECT_EQ(s_stop_from_wake, NX_ERROR_CONTEXT);
    EXPECT_EQ(s_task_wakes, 0U);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_FALSE(state.uart.closing);
}

TEST_F(F407HalContract, SynchronousAbortDoesNotCallIrqWakeFromTask) {
    const nx_irq_wake_t observed = {&port, StopFromWake, true};
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &observed, 5U), NX_SUCCESS);
    s_task_wakes = 0U;
    Submit();
    DmaComplete();
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(s_task_wakes, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
}

static nx_stm32_uart_hal_state_t* s_preempted_owner;
static bool s_vector_masked_during_entry;
static void PreemptHalSubmit() {
    if ((USART1->CR3 & USART_CR3_DMAT) == 0U)
        return;
    nx_hal_test_restore_hook(nullptr);
    USART1->DR = 0x63U;
    USART1->SR = USART_SR_RXNE;
    bool previous = nx_hal_test_isr(true);
    nx_stm32_uart_hal_irq(s_preempted_owner);
    nx_hal_test_isr(previous);
    s_vector_masked_during_entry =
        (NVIC->ISER[USART1_IRQn / 32U] & (1U << (USART1_IRQn % 32U))) == 0U;
}

TEST_F(F407HalContract, IrqDuringHalEntryDefersWithoutLosingRxFact) {
    s_preempted_owner = &state;
    s_vector_masked_during_entry = false;
    nx_hal_test_restore_hook(PreemptHalSubmit);
    ASSERT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    nx_hal_test_restore_hook(nullptr);
    EXPECT_TRUE(s_vector_masked_during_entry);
    EXPECT_EQ(state.hal_depth, 0U);
    EXPECT_NE(NVIC->ISER[USART1_IRQn / 32U] & (1U << (USART1_IRQn % 32U)), 0U);
    EXPECT_NE(USART1->SR & USART_SR_RXNE, 0U);
    EXPECT_EQ(state.uart.rx_count, 0U);
    UartIrq();
    USART1->SR = 0U;
    nx_uart_rx_event_t out = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, &out, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out.byte, 0x63U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}

TEST_F(F407HalContract, LateDmaErrorBeforeSettlementCannotReportSuccess) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    EXPECT_TRUE(state.uart.tx_complete);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TEIF7;
    nx_uart_port_service(&port);
    if (nx_request_state(&request.base) == NX_REQUEST_SETTLED) {
        EXPECT_EQ(request.base.result, NX_ERROR_IO);
        EXPECT_EQ(request.base.transferred, 0U);
    } else {
        EXPECT_EQ(state.uart.active, &request);
    }
}

/** \brief Model a refused address-register write at its completion boundary. */
static void RetainDmaAddress() {
    if ((DMA2_Stream7->CR & DMA_SxCR_EN) == 0U && DMA2_Stream7->M0AR == 0U)
        DMA2_Stream7->M0AR = 0x20000000U;
}

TEST_F(F407HalContract, RefusedDmaAddressWithdrawalRetainsBorrowForRetry) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_hal_test_dsb_hook(RetainDmaAddress);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.uart.active, &request);
    nx_hal_test_dsb_hook(nullptr);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(DMA2_Stream7->M0AR, 0U);
}

/** \brief Model a source that remains enabled despite the detach write. */
static void RetainDmaErrorSource() {
    if ((DMA2_Stream7->CR & DMA_SxCR_EN) == 0U)
        DMA2_Stream7->CR |= DMA_SxCR_TEIE;
}

TEST_F(F407HalContract, RefusedDmaIrqWithdrawalRetainsBorrowForRetry) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_hal_test_dsb_hook(RetainDmaErrorSource);
    nx_uart_port_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.uart.active, &request);
    nx_hal_test_dsb_hook(nullptr);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_TEIE, 0U);
}

TEST_F(F407HalContract, TooLowBaudRejectsBeforeOwnerOrHardwareEffects) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    config.uart.baud = 1200U;
    uint8_t before[sizeof(state)];
    std::memcpy(before, &state, sizeof(state));
    const uint32_t cr1 = USART1->CR1;
    const uint32_t brr = USART1->BRR;
    const uint32_t dma = DMA2_Stream7->CR;
    s_uart_enables = 0U;

    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(std::memcmp(before, &state, sizeof(state)), 0);
    EXPECT_EQ(USART1->CR1, cr1);
    EXPECT_EQ(USART1->BRR, brr);
    EXPECT_EQ(DMA2_Stream7->CR, dma);
    EXPECT_EQ(s_uart_enables, 0U);
}

TEST_F(F407HalContract, NearestBaudDivisorIsReadyBeforeFirstUeOnApb2) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    config.uart.baud = 230400U;
    s_uart_enables = 0U;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    EXPECT_EQ(USART1->BRR, 365U);
    EXPECT_EQ(s_first_enabled_brr, 365U);
    EXPECT_EQ(s_uart_enables, 1U);
}

TEST_F(F407HalContract, NearestBaudDivisorIsReadyBeforeFirstUeOnApb1) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    config.uart.registers = USART2;
    config.uart.clock_hz = 42000000U;
    config.uart.baud = 921600U;
    vectors[0] = USART2_IRQn;
    vectors[1] = DMA1_Stream6_IRQn;
    config.resources.dma = DMA1;
    config.transmit = {DMA1_Stream6, DMA1_Stream6_IRQn, 6U, 4U};
    RCC->APB1ENR = RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    s_uart_enables = 0U;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    EXPECT_EQ(USART2->BRR, 46U);
    EXPECT_EQ(s_first_enabled_brr, 46U);
    EXPECT_EQ(s_uart_enables, 1U);
}

TEST_F(F407HalContract, CompleteFrameIsReadyBeforeFirstUe) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    format = {8U, NX_UART_PARITY_ODD, NX_UART_STOP_2, NX_UART_FLOW_NONE};
    config.uart.format = &format;
    s_uart_enables = 0U;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    EXPECT_EQ(s_first_enabled_cr1 &
                  (USART_CR1_UE | USART_CR1_M | USART_CR1_PCE | USART_CR1_PS),
              USART_CR1_M | USART_CR1_PCE | USART_CR1_PS);
    EXPECT_EQ(s_first_enabled_cr2 & USART_CR2_STOP, UART_STOPBITS_2);
}

TEST_F(F407HalContract, ClosingOwnerCanDetachWakeWithoutRevokingBorrow) {
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    Submit();
    ASSERT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    ASSERT_TRUE(state.uart.closing);
    EXPECT_EQ(nx_uart_port_attach_wake(&port, nullptr, 5U), NX_SUCCESS);
    const unsigned before = s_wakes;
    DmaComplete();
    EXPECT_EQ(s_wakes, before);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(F407HalContract, LostWireTcAfterMemoryCompletionHasFiniteDrainGrace) {
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 4U, 1000000U), NX_SUCCESS);
    Submit();
    DmaComplete();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    ASSERT_EQ(AdvanceClock(1001U), NX_SUCCESS);
    nx_uart_port_service(&port);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(state.uart.terminal, NX_ERROR_IO);
    EXPECT_EQ(nx_request_prepare(&request.base, 2000000U), NX_ERROR_STATE);
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}

static bool s_rx_arrived_during_hal;
static void ReceiveDuringMaskedHalHandler() {
    if ((USART1->CR1 & (USART_CR1_RXNEIE | USART_CR1_PEIE)) != 0U ||
        (USART1->CR3 & USART_CR3_EIE) != 0U)
        return;
    nx_hal_test_restore_hook(nullptr);
    USART1->DR = 0x52U;
    USART1->SR |= USART_SR_RXNE | USART_SR_FE;
    s_rx_arrived_during_hal = true;
}

TEST_F(F407HalContract, NewRxErrorDuringHalTxHandlerSurvivesForNextIrq) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    s_rx_arrived_during_hal = false;
    nx_hal_test_restore_hook(ReceiveDuringMaskedHalHandler);
    UartIrq();
    nx_hal_test_restore_hook(nullptr);
    EXPECT_TRUE(s_rx_arrived_during_hal);
    EXPECT_NE(USART1->SR & USART_SR_RXNE, 0U);
    EXPECT_EQ(state.uart.rx_count, 0U);
    UartIrq();
    USART1->SR &= ~(uint32_t)(USART_SR_RXNE | USART_SR_FE);
    nx_uart_rx_event_t out = {};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&port, &out, 1U, &count), NX_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(out.byte, 0x52U);
    EXPECT_EQ(out.flags, NX_UART_EVENT_FRAMING);
    EXPECT_EQ(state.handle.RxState, HAL_UART_STATE_READY);
    EXPECT_EQ(state.handle.ReceptionType, HAL_UART_RECEPTION_STANDARD);
    EXPECT_EQ(state.handle.pRxBuffPtr, nullptr);
    nx_uart_port_service(&port);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
}

/** \brief Apply the W1C command with a fresh TE before the posted write lands.
 */
static void ErrorAtFinalFlagAck(IRQn_Type irq) {
    if (irq != DMA2_Stream7_IRQn || DMA2_Stream7->M0AR != 0U)
        return;
    s_pending_clear_hook = nullptr;
    const_cast<uint32_t&>(DMA2->HISR) |= DMA_HISR_TEIF7;
    const_cast<uint32_t&>(DMA2->HISR) &= ~DMA2->HIFCR;
}

TEST_F(F407HalContract, FinalCompletionAckCannotEraseAnUnobservedDmaError) {
    Submit();
    DmaComplete();
    USART1->SR = USART_SR_TC;
    UartIrq();
    s_pending_clear_hook = ErrorAtFinalFlagAck;
    nx_uart_port_service(&port);
    if (nx_request_state(&request.base) == NX_REQUEST_SETTLED) {
        EXPECT_EQ(request.base.result, NX_ERROR_IO);
        EXPECT_EQ(request.base.transferred, 0U);
    } else {
        EXPECT_EQ(state.uart.active, &request);
    }
}

/** \brief The enclosing Platform retains GPIO/DMA gates across UART leases. */
class F407HalCold : public F407HalContract {
  protected:
    nx_stm32_uart_cold_receipt_t receipt = {};
    uint16_t saved[2] = {};
    nx_stm32_platform_pin_config_t pins[2] = {};
    uint16_t original_pins[2] = {};
    void SetUp() override {
        F407HalContract::SetUp();
        ASSERT_FALSE(HasFatalFailure());
        USART1->SR = USART_SR_TC;
        ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
        state = {};
        std::memset(USART1, 0, sizeof(*USART1));
        std::memset(DMA2_Stream7, 0, sizeof(*DMA2_Stream7));
        g_nx_stm32_gpioa_model = {};
        RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
        RCC->AHB1ENR = RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_GPIOAEN;
        GPIOA->MODER = (3U << 20U) | 3U;
        GPIOA->AFR[1] = 0x00000A90U;
        GPIOA->PUPDR = 1U << 18U;
        GPIOA->OSPEEDR = 1U << 20U;
        pins[0] = {GPIOA, &saved[0], 0U, 9U};
        pins[1] = {GPIOA, &saved[1], 0U, 10U};
        config.uart.pins = pins;
        for (unsigned i = 0U; i < 2U; ++i) {
            original_pins[i] = nx_stm32_pin_capture(GPIOA, pins[i].pin);
            NVIC->IPR[static_cast<unsigned>(vectors[i])] = 0xB0U;
        }
        s_cold_writes = s_uart_enables = 0U;
    }
    void Start() {
        ASSERT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
                  NX_SUCCESS);
        ASSERT_TRUE(state.uart.initialized);
        ASSERT_NE(receipt.effects, 0U);
    }
    void Returned() {
        EXPECT_EQ(receipt.effects, 0U);
        EXPECT_EQ(receipt.clock_added, 0U);
        EXPECT_EQ(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN);
        EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_GPIOAEN);
        for (unsigned i = 0U; i < 2U; ++i) {
            EXPECT_EQ(nx_stm32_pin_capture(GPIOA, pins[i].pin),
                      original_pins[i]);
            EXPECT_EQ(NVIC_GetEnableIRQ(vectors[i]), 0U);
            EXPECT_EQ(NVIC->IPR[static_cast<unsigned>(vectors[i])], 0xB0U);
        }
    }
};

TEST_F(F407HalCold, ExactLeaseStartsStopsRestoresAndRestarts) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(state.handle.Instance, USART1);
    EXPECT_EQ(state.handle.hdmatx, &state.dma);
    EXPECT_EQ(state.dma.Parent, &state.handle);
    EXPECT_EQ(receipt.clock_added, RCC_APB2ENR_USART1EN);
    EXPECT_EQ(GPIOA->MODER & 0x003C0000U, 0x00280000U);
    for (IRQn_Type irq : vectors) {
        EXPECT_NE(NVIC_GetEnableIRQ(irq), 0U);
        EXPECT_EQ(NVIC->IPR[static_cast<unsigned>(irq)], 0x50U);
    }
    USART1->SR = USART_SR_TC;
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
    Start();
}

TEST_F(F407HalCold, PreexistingUartGateRemainsAfterLeaseReturn) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    Start();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(receipt.clock_added, 0U);
    USART1->SR = USART_SR_TC;
    ASSERT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_EQ(receipt.effects, 0U);
}

TEST_F(F407HalCold, MaskedColdEntryPreservesMaskAndHasNoEffects) {
    auto saved_mask = nx_arch_irq_save();
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_CONTEXT);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(s_cold_writes, 0U);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(receipt.effects, 0U);
    nx_arch_irq_restore(saved_mask);
    Returned();
}

TEST_F(F407HalCold, IsrColdEntryAndCloseAreRefusedWithoutEffects) {
    bool previous = nx_hal_test_isr(true);
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_ERROR_CONTEXT);
    nx_hal_test_isr(previous);
    EXPECT_EQ(s_cold_writes, 0U);
    Returned();
}

TEST_F(F407HalCold, LowBaudRejectsBeforeAnyResourceAcquisition) {
    config.uart.baud = 1200U;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_cold_writes, 0U);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(state.config, nullptr);
    Returned();
}

TEST_F(F407HalCold, UnsupportedFrameRejectsBeforeAnyPinOrClockWrite) {
    format = {9U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE};
    config.uart.format = &format;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_cold_writes, 0U);
    EXPECT_EQ(s_uart_enables, 0U);
    Returned();
}

TEST_F(F407HalCold, UnsupportedDmaRouteRejectsBeforeEffects) {
    config.transmit.channel = 3U;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_cold_writes, 0U);
    EXPECT_EQ(state.config, nullptr);
    Returned();
}

TEST_F(F407HalCold, ForeignRunningDmaIsNeverReconfiguredOrAcquired) {
    DMA2_Stream7->CR = DMA_SxCR_EN | DMA_SxCR_MINC;
    const uint32_t before = DMA2_Stream7->CR;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_EQ(DMA2_Stream7->CR, before);
    EXPECT_EQ(s_cold_writes, 0U);
    EXPECT_EQ(receipt.effects, 0U);
    Returned();
}

TEST_F(F407HalCold, ForeignLiveUartIsNotAdoptedAndReceiptCanReturn) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    USART1->CR1 = USART_CR1_UE | USART_CR1_RXNEIE;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_EQ(USART1->CR1, USART_CR1_UE | USART_CR1_RXNEIE);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_EQ(receipt.effects, 0U);
}

TEST_F(F407HalCold, RefusedClockAcquisitionRetainsItsCleanupReceipt) {
    s_refused_cold_register = &RCC->APB2ENR;
    s_cold_held_bits = RCC_APB2ENR_USART1EN;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_IO);
    EXPECT_NE(receipt.effects, 0U);
    EXPECT_EQ(receipt.clock_added, RCC_APB2ENR_USART1EN);
    EXPECT_FALSE(state.uart.initialized);
    s_refused_cold_register = nullptr;
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalCold, FailedPriorityVerificationRollsBackThePartialOwner) {
    s_refused_priority = USART1_IRQn;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_IO);
    EXPECT_EQ(s_uart_enables, 1U);
    EXPECT_FALSE(state.uart.initialized);
    Returned();
    s_refused_priority = -1;
    Start();
}

TEST_F(F407HalCold, BorrowedRequestCloseRetainsPinsClockAndReceiptUntilDrain) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    Submit();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_NE(receipt.effects, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 0x003C0000U, 0x00280000U);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(state.uart.wake, &wake);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    USART1->SR |= USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalCold, RefusedVectorWithdrawalRetainsLeaseForRetry) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    USART1->SR = USART_SR_TC;
    s_hold_vector = USART1_IRQn;
    EXPECT_NE(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_NE(receipt.effects, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_TRUE(state.uart.initialized);
    s_hold_vector = static_cast<IRQn_Type>(-1);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalCold, RefusedGateReturnKeepsSnapshotReceiptForRetry) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    USART1->SR = USART_SR_TC;
    s_refused_cold_register = &RCC->APB2ENR;
    s_cold_held_bits = RCC_APB2ENR_USART1EN;
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_ERROR_IO);
    EXPECT_NE(receipt.clock_added, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    s_refused_cold_register = nullptr;
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalCold, AlienEmptyReceiptCannotStopOrOverwriteLiveOwner) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    const auto before = state;
    nx_stm32_uart_cold_receipt_t alien = {};
    const auto writes = s_cold_writes;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &alien),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &alien),
              NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&before, &state, sizeof(state)), 0);
    EXPECT_EQ(s_cold_writes, writes);
    EXPECT_EQ(alien.effects, 0U);
}

TEST_F(F407HalCold, EmptyReceiptClosePreservesUnownedStorageWithoutEffects) {
    state.uart.registers = USART1;
    const auto before = state;
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&before, &state, sizeof(state)), 0);
    EXPECT_EQ(s_cold_writes, 0U);
    Returned();
}

TEST_F(F407HalCold, IndependentUsartOwnersKeepPrivateHandlesAndLeases) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    nx_stm32_uart_hal_state_t second = {};
    auto other = config;
    uint16_t other_saved[2] = {};
    nx_stm32_platform_pin_config_t other_pins[] = {
        {GPIOA, &other_saved[0], 0U, 2U}, {GPIOA, &other_saved[1], 0U, 3U}};
    const IRQn_Type other_vectors[] = {USART2_IRQn, DMA1_Stream6_IRQn};
    other.uart.registers = USART2;
    other.uart.clock_hz = 42000000U;
    other.uart.pins = other_pins;
    other.uart.vectors = other_vectors;
    other.uart.rx_storage = payload + 0x500U;
    other.resources.dma = DMA1;
    other.transmit = {DMA1_Stream6, DMA1_Stream6_IRQn, 6U, 4U};
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    nx_stm32_uart_cold_receipt_t second_receipt = {};
    ASSERT_EQ(nx_stm32_uart_hal_cold_prepare(&second, &other, &second_receipt),
              NX_SUCCESS);
    EXPECT_EQ(second.handle.hdmatx, &second.dma);
    EXPECT_EQ(second.dma.Parent, &second.handle);
    EXPECT_NE(second.handle.hdmatx, state.handle.hdmatx);
    EXPECT_EQ(second.handle.Instance, USART2);
    USART2->SR = USART_SR_TC;
    ASSERT_EQ(nx_stm32_uart_hal_cold_close(&second, &other, &second_receipt),
              NX_SUCCESS);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_NE(USART1->CR1 & USART_CR1_UE, 0U);
    EXPECT_NE(NVIC_GetEnableIRQ(USART1_IRQn), 0U);
    EXPECT_NE(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    EXPECT_EQ(state.handle.hdmatx, &state.dma);
    EXPECT_EQ(state.dma.Parent, &state.handle);
}

TEST_F(F407HalContract, RequestMetadataCannotMutateTheDeclaredDmaAuthority) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    auto* table = reinterpret_cast<nx_dma_memory_region_t*>(payload + 0x800U);
    table[0] = region;
    table[1] = region;
    auto* aliased = reinterpret_cast<nx_uart_tx_request_t*>(
        reinterpret_cast<uint8_t*>(&table[1]) +
        offsetof(nx_dma_memory_region_t, permissions));
    nx_request_initialize(&aliased->base);
    ASSERT_EQ(nx_uart_tx_prepare(aliased, payload, 4U, 1000U), NX_SUCCESS);
    ASSERT_EQ(table[1].permissions, 0U);
    config.resources.regions = table;
    config.resources.region_count = 2U;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, aliased), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&aliased->base), NX_REQUEST_READY);
    EXPECT_EQ(table[1].permissions, 0U);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_F(F407HalContract, PayloadCannotBorrowTheRetainedDmaRegionTable) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    auto* table = new (payload + 0x800U) nx_dma_memory_region_t(region);
    config.resources.regions = table;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(table),
                                 sizeof(*table), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407HalContract, PayloadCannotBorrowTheRetainedFrameFacts) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    auto* frame = new (payload + 0x800U) nx_uart_format_t{
        8U, NX_UART_PARITY_NONE, NX_UART_STOP_1, NX_UART_FLOW_NONE};
    config.uart.format = frame;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(frame),
                                 sizeof(*frame), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407HalContract, PayloadCannotBorrowTheRetainedVectorFacts) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    auto* selected = reinterpret_cast<IRQn_Type*>(payload + 0x800U);
    selected[0] = USART1_IRQn;
    selected[1] = DMA2_Stream7_IRQn;
    config.uart.vectors = selected;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(selected),
                                 sizeof(vectors), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407HalCold, PayloadCannotBorrowTheRetainedColdPinFacts) {
    auto* selected =
        reinterpret_cast<nx_stm32_platform_pin_config_t*>(payload + 0x800U);
    selected[0] = pins[0];
    selected[1] = pins[1];
    config.uart.pins = selected;
    Start();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(selected),
                                 2U * sizeof(*selected), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407HalContract, PayloadAdjacentToDmaTableRemainsAdmissible) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    auto* table = new (payload + 0x800U) nx_dma_memory_region_t(region);
    config.resources.regions = table;
    ASSERT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_SUCCESS);
    const auto* adjacent = reinterpret_cast<const uint8_t*>(table + 1U);
    ASSERT_EQ(nx_uart_tx_prepare(&request, adjacent, 4U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}

TEST_F(F407HalContract, LateDmaErrorPreservesTheFirstCancellationCause) {
    Submit();
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(state.uart.terminal, NX_ERROR_CANCELLED);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TEIF7;
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(F407HalContract, LateDmaErrorPreservesTheFirstTimeoutCause) {
    Submit();
    ASSERT_EQ(AdvanceClock(901U), NX_SUCCESS);
    nx_uart_port_service(&port);
    ASSERT_EQ(state.uart.terminal, NX_ERROR_TIMEOUT);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TEIF7;
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(F407HalContract, RuntimeInitRequiresTheActualUartClockBeforeEffects) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    RCC->APB2ENR &= ~static_cast<uint32_t>(RCC_APB2ENR_USART1EN);
    s_uart_enables = 0U;
    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(USART1->CR1 & USART_CR1_UE, 0U);
}

TEST_F(F407HalContract, RuntimeInitRequiresTheActualDmaClockBeforeEffects) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    RCC->AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_DMA2EN);
    s_uart_enables = 0U;
    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(USART1->CR1 & USART_CR1_UE, 0U);
}

static void RefuseProgrammedBrr(UART_HandleTypeDef* handle) {
    handle->Instance->BRR ^= 1U;
}
static void RefuseProgrammedFrame(UART_HandleTypeDef* handle) {
    handle->Instance->CR1 ^= USART_CR1_PCE;
}
static void RefuseFiniteDmaMode(UART_HandleTypeDef* handle) {
    handle->hdmatx->Instance->CR |= DMA_SxCR_CIRC;
}

TEST_F(F407HalContract, FailedBrrReadbackCannotPublishAUsableOwner) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    s_uart_enable_hook = RefuseProgrammedBrr;
    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    s_uart_enable_hook = nullptr;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(F407HalContract, FailedFrameReadbackRetainsTheClosingCleanupOwner) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    s_uart_enable_hook = RefuseProgrammedFrame;
    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    s_uart_enable_hook = nullptr;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

TEST_F(F407HalContract, UnexpectedCircularDmaReadbackRefusesFiniteMode) {
    ASSERT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
    state = {};
    s_uart_enable_hook = RefuseFiniteDmaMode;
    EXPECT_EQ(nx_stm32_uart_hal_initialize(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.uart.closing);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    s_uart_enable_hook = nullptr;
    EXPECT_EQ(nx_uart_port_stop(&port), NX_SUCCESS);
}

class F407HalOwnership : public F407HalContract,
                         public testing::WithParamInterface<unsigned> {
  protected:
    DMA_HandleTypeDef foreign = {};
    void Corrupt() {
        switch (GetParam()) {
            case 0:
                USART1->CR3 |= USART_CR3_DMAR;
                break;
            case 1:
                state.handle.RxState = HAL_UART_STATE_BUSY_RX;
                break;
            case 2:
                state.handle.ReceptionType = HAL_UART_RECEPTION_TOIDLE;
                break;
            case 3:
                state.handle.hdmarx = &foreign;
                break;
            case 4:
                state.handle.pRxBuffPtr = payload + 0x300U;
                break;
            case 5:
                state.handle.RxXferCount = 1U;
                break;
            case 6:
                state.handle.RxXferSize = 1U;
                break;
            case 7:
                state.handle.hdmatx = &foreign;
                break;
            case 8:
                state.dma.Instance = DMA1_Stream0;
                break;
            case 9:
                state.dma.Parent = &foreign;
                break;
            default:
                FAIL() << "unknown owned RX invariant";
        }
    }
};

TEST_P(F407HalOwnership, InvalidRxDomainRejectsBeforeRequestAdmission) {
    Corrupt();
    const auto before = foreign;
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(std::memcmp(&foreign, &before, sizeof(foreign)), 0);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}
INSTANTIATE_TEST_SUITE_P(OwnedRxFacts, F407HalOwnership,
                         testing::Range(0U, 10U));

TEST_F(F407HalContract, InvalidRxDmaDomainQuarantinesWithoutForeignHalAbort) {
    Submit();
    DMA_HandleTypeDef foreign = {};
    foreign.Instance = DMA1_Stream0;
    foreign.Parent = &state.handle;
    foreign.State = HAL_DMA_STATE_BUSY;
    DMA1_Stream0->CR = DMA_SxCR_EN | DMA_SxCR_MINC;
    const uint32_t foreign_cr = DMA1_Stream0->CR;
    state.handle.hdmarx = &foreign;
    state.handle.RxState = HAL_UART_STATE_BUSY_RX;
    state.handle.RxXferSize = state.handle.RxXferCount = 1U;
    state.handle.pRxBuffPtr = payload + 0x300U;
    USART1->CR3 |= USART_CR3_DMAR;
    USART1->DR = 0x63U;
    USART1->SR = USART_SR_RXNE | USART_SR_ORE;
    UartIrq();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(state.uart.terminal, NX_ERROR_IO);
    EXPECT_EQ(DMA1_Stream0->CR, foreign_cr);
    EXPECT_EQ(foreign.State, HAL_DMA_STATE_BUSY);
    EXPECT_EQ(foreign.XferAbortCallback, nullptr);
    EXPECT_EQ(state.uart.rx_count, 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(USART1_IRQn), 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    EXPECT_EQ(nx_uart_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.uart.active, &request);
    /* Only the retained owner's caller can restore a corrupted private domain.
     * No foreign receive loan is canceled or adopted by the provider. */
    state.handle.hdmarx = nullptr;
    state.handle.RxState = HAL_UART_STATE_READY;
    state.handle.RxXferSize = state.handle.RxXferCount = 0U;
    state.handle.pRxBuffPtr = nullptr;
    USART1->CR3 &= ~static_cast<uint32_t>(USART_CR3_DMAR);
    USART1->SR = USART_SR_TC;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    ASSERT_NE(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    /* Forced stream disable raises TCIF; the HAL abort callback joins it. */
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    DmaIrq();
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(DMA1_Stream0->CR, foreign_cr);
}

static void RefuseUartDeinitAtVisibilityBarrier() {
    if ((USART1->CR1 & USART_CR1_UE) == 0U)
        USART1->CR1 |= USART_CR1_UE;
}
static void RefuseDmaDeinitAtVisibilityBarrier() {
    if (DMA2_Stream7->M0AR == 0U) {
        DMA2_Stream7->M0AR = 0x20000000U;
        DMA2_Stream7->CR |= DMA_SxCR_TCIE;
        DMA2_Stream7->FCR |= DMA_SxFCR_FEIE;
    }
}

TEST_F(F407HalCold, RefusedUartDeinitRetainsPadsGateAndCleanupOwner) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    USART1->SR = USART_SR_TC;
    nx_hal_test_dsb_hook(RefuseUartDeinitAtVisibilityBarrier);
    EXPECT_NE(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_NE(receipt.effects, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 0x003C0000U, 0x00280000U);
    nx_hal_test_dsb_hook(nullptr);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalCold, RefusedDmaDeinitReferencesRetainTheLeaseForRetry) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    USART1->SR = USART_SR_TC;
    DMA2_Stream7->M0AR = 0x20000000U;
    DMA2_Stream7->CR |= DMA_SxCR_TCIE;
    DMA2_Stream7->FCR |= DMA_SxFCR_FEIE;
    nx_hal_test_dsb_hook(RefuseDmaDeinitAtVisibilityBarrier);
    EXPECT_NE(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    EXPECT_TRUE(state.uart.initialized);
    EXPECT_NE(receipt.effects, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_USART1EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 0x003C0000U, 0x00280000U);
    nx_hal_test_dsb_hook(nullptr);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
    Returned();
}

TEST_F(F407HalContract, HeldEnableTeNeverRunsTheVendorIrqPollOrFalseReady) {
    Submit();
    s_hold_dma_enable = true;
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TEIF7;
    DmaIrq();
    EXPECT_NE(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(state.dma.State, HAL_DMA_STATE_READY);
    EXPECT_NE(state.handle.gState, HAL_UART_STATE_READY);
    EXPECT_EQ(state.handle.ErrorCode, HAL_UART_ERROR_NONE);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.terminal, NX_ERROR_IO);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    /* A hardware drain is distinct from the IRQ asking the stream to stop. */
    s_hold_dma_enable = false;
    DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_uart_port_service(&port);
    /* The retained TE source can join through the actual HAL in task context.
     * Wire completion remains an independent observation and task predicate. */
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    const_cast<uint32_t&>(DMA2->HISR) &= ~DMA2->HIFCR;
    USART1->SR = USART_SR_TC;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(F407HalContract, LiveHalfTransferUsesOnlyItsPrivateHandleCallback) {
    Submit();
    DMA2_Stream7->NDTR = 2U;
    ASSERT_NE(DMA2_Stream7->CR & DMA_SxCR_HTIE, 0U);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_HTIF7;
    DmaIrq();
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    EXPECT_EQ(s_global_hal_half_callbacks, 0U);
    EXPECT_NE(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(state.dma.State, HAL_DMA_STATE_BUSY);
    EXPECT_EQ(state.handle.gState, HAL_UART_STATE_BUSY_TX);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_HTIE, 0U);
    Receive(USART_SR_RXNE, 0x7AU);
    EXPECT_EQ(state.uart.rx_count, 1U);
}

TEST_F(F407HalCold, ForeignCtsInterruptIsRefusedBeforeUartActivation) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    USART1->CR3 = USART_CR3_CTSIE;
    USART1->SR = USART_SR_CTS;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(USART1_IRQn), 0U);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_NE(USART1->CR3 & USART_CR3_CTSIE, 0U);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
}

TEST_F(F407HalCold, ForeignLinBreakInterruptIsRefusedBeforeUartActivation) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    USART1->CR2 = USART_CR2_LBDIE;
    USART1->SR = USART_SR_LBD;
    EXPECT_EQ(nx_stm32_uart_hal_cold_prepare(&state, &config, &receipt),
              NX_ERROR_BUSY);
    EXPECT_EQ(s_uart_enables, 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(USART1_IRQn), 0U);
    EXPECT_FALSE(state.uart.initialized);
    EXPECT_NE(USART1->CR2 & USART_CR2_LBDIE, 0U);
    EXPECT_EQ(nx_stm32_uart_hal_cold_close(&state, &config, &receipt),
              NX_SUCCESS);
}

TEST_F(F407HalContract, DirectCancelNeverAbortsAForeignTxHandle) {
    Submit();
    DMA_HandleTypeDef foreign = {};
    foreign.Instance = DMA1_Stream0;
    foreign.Parent = &state.handle;
    foreign.State = HAL_DMA_STATE_BUSY;
    DMA1_Stream0->CR = DMA_SxCR_EN | DMA_SxCR_TCIE;
    const auto original = foreign;
    const uint32_t original_cr = DMA1_Stream0->CR;
    state.handle.hdmatx = &foreign;
    EXPECT_EQ(nx_uart_port_cancel(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&original, &foreign, sizeof(foreign)), 0);
    EXPECT_EQ(DMA1_Stream0->CR, original_cr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(state.uart.terminal, NX_ERROR_IO);
    EXPECT_EQ(NVIC_GetEnableIRQ(USART1_IRQn), 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    state.handle.hdmatx = &state.dma;
    USART1->SR = USART_SR_TC;
    nx_uart_port_service(&port);
    ASSERT_NE(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
    DmaIrq();
    const_cast<uint32_t&>(DMA2->HISR) = 0U;
    nx_uart_port_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(DMA1_Stream0->CR, original_cr);
}

class F407HalSnapshot : public F407HalCold,
                        public testing::WithParamInterface<unsigned> {};

TEST_P(F407HalSnapshot, RequestAdmissionCannotMutateAColdRestoreSnapshot) {
    GPIOA->MODER &= ~0x003C0000U;
    GPIOA->OSPEEDR &= ~0x003C0000U;
    GPIOA->PUPDR &= ~0x003C0000U;
    GPIOA->AFR[1] &= ~0x00000FF0U;
    auto* aliased = new (payload + 0x800U) nx_uart_tx_request_t{};
    nx_request_initialize(&aliased->base);
    ASSERT_EQ(nx_uart_tx_prepare(aliased, payload, 4U, 1000U), NX_SUCCESS);
    auto* leaf = reinterpret_cast<uint16_t*>(&aliased->base.state);
    pins[GetParam()].snapshot = leaf;
    Start();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(*leaf, 0U);
    ASSERT_EQ(nx_request_state(&aliased->base), NX_REQUEST_READY);
    EXPECT_EQ(nx_uart_port_submit(&port, aliased), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&aliased->base), NX_REQUEST_READY);
    EXPECT_EQ(*leaf, 0U);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_P(F407HalSnapshot, PayloadCannotBorrowAColdRestoreSnapshot) {
    auto* leaf = new (payload + 0x800U) uint16_t{};
    pins[GetParam()].snapshot = leaf;
    Start();
    ASSERT_FALSE(HasFatalFailure());
    const uint16_t before = *leaf;
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(leaf),
                                 sizeof(*leaf), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(*leaf, before);
    EXPECT_EQ(state.uart.active, nullptr);
    EXPECT_EQ(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
}

TEST_P(F407HalSnapshot, PayloadAdjacentToAColdRestoreSnapshotIsAdmissible) {
    auto* leaf = new (payload + 0x800U) uint16_t{};
    pins[GetParam()].snapshot = leaf;
    Start();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(nx_uart_tx_prepare(&request,
                                 reinterpret_cast<const uint8_t*>(leaf + 1U),
                                 4U, 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
}
INSTANTIATE_TEST_SUITE_P(RetainedPinSnapshot, F407HalSnapshot,
                         testing::Range(0U, 2U));

static bool s_fresh_te_injected;
static void FreshTeAfterTheErrorPrefilter(DMA_HandleTypeDef* handle,
                                          uint32_t mask, uint32_t observed) {
    if (handle->Instance != DMA2_Stream7 || (mask & DMA_HISR_TEIF7) == 0U ||
        (observed & DMA_HISR_TEIF7) != 0U)
        return;
    s_dma_read_hook = nullptr;
    const_cast<uint32_t&>(DMA2->HISR) |= DMA_HISR_TEIF7;
    s_fresh_te_injected = true;
}

TEST_F(F407HalContract, FreshTeAfterThePrefilterCannotReachAHeldEnablePoll) {
    wake.calls_kernel = true;
    ASSERT_EQ(nx_uart_port_attach_wake(&port, &wake, 5U), NX_SUCCESS);
    Submit();
    DMA2_Stream7->NDTR = 2U;
    s_hold_dma_enable = true;
    s_fresh_te_injected = false;
    s_dma_read_hook = FreshTeAfterTheErrorPrefilter;
    const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_HTIF7;
    DmaIrq();
    ASSERT_TRUE(s_fresh_te_injected);
    EXPECT_NE(state.dma.State, HAL_DMA_STATE_READY);
    EXPECT_NE(state.handle.gState, HAL_UART_STATE_READY);
    EXPECT_EQ(state.handle.ErrorCode, HAL_UART_ERROR_NONE);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(s_global_hal_half_callbacks, 0U);
    /* Acknowledging the observed HT cannot erase a later error source. */
    const_cast<uint32_t&>(DMA2->HISR) &= ~DMA2->HIFCR;
    EXPECT_NE(DMA2->HISR & DMA_HISR_TEIF7, 0U);
    const unsigned before_deferred_wake = s_wakes;
    DmaIrq();
    EXPECT_GT(s_wakes, before_deferred_wake);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.uart.active, &request);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
    EXPECT_NE(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
    const unsigned before_task_progress = s_wakes;
    s_hold_dma_enable = false;
    DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    nx_uart_port_service(&port);
    EXPECT_EQ(s_wakes, before_task_progress);
}

TEST_F(F407HalContract,
       WireOnlyCancelCompletionLeavesTheNextHalTransferUsable) {
    Submit();
    ASSERT_EQ(nx_uart_port_cancel(&port, &request), NX_SUCCESS);
    USART1->SR = USART_SR_TC;
    UartIrq();
    nx_uart_port_service(&port);
    if (nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
        EXPECT_EQ(state.uart.active, &request);
        ASSERT_NE(NVIC_GetEnableIRQ(DMA2_Stream7_IRQn), 0U);
        const_cast<uint32_t&>(DMA2->HISR) = DMA_HISR_TCIF7;
        DmaIrq();
        const_cast<uint32_t&>(DMA2->HISR) = 0U;
        nx_uart_port_service(&port);
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(request.base.result, NX_ERROR_CANCELLED);
    ASSERT_EQ(request.base.transferred, 0U);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, 4U, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.handle.gState, HAL_UART_STATE_BUSY_TX);
    EXPECT_EQ(state.dma.State, HAL_DMA_STATE_BUSY);
    EXPECT_NE(DMA2_Stream7->CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(state.handle.pTxBuffPtr, payload);
}
