/**
 * \file            h743_spi_dma_byte_test.cpp
 * \brief           Default DMA cleanup byte loads retain real request loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_dma_byte_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_spi_dma.h"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>

extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ADD_FAILURE() << "Unexpected nonreturning DMA withdrawal failure";
    std::abort();
}

class H743SpiDmaDefaultByte : public testing::TestWithParam<bool> {
  protected:
    using Payload = std::array<uint8_t, 64>;
    uint8_t* memory = nullptr;
    nx_stm32h743_spi_dma_state_t* owner = nullptr;
    nx_stm32h743_spi_dma_endpoint_t* device = nullptr;
    nx_stm32h743_gpio_state_t* cs = nullptr;
    nx_gpio_port_t* cs_face = nullptr;
    nx_spi_port_t* port = nullptr;
    nx_spi_endpoint_t* endpoint = nullptr;
    nx_dma_memory_region_t* region = nullptr;
    nx_spi_request_t* request = nullptr;
    Payload* tx = nullptr;
    Payload* rx = nullptr;
    void SetUp() override {
        nx_h743_spi_dma_model_reset();
        void* raw =
            mmap(reinterpret_cast<void*>(0x24000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, raw) << std::strerror(errno);
        ASSERT_EQ(reinterpret_cast<void*>(0x24000000U), raw);
        memory = static_cast<uint8_t*>(raw);
        region = new (memory) nx_dma_memory_region_t{0x24000000U, 0x80000U, 3U};
        cs = new (memory + 0x100U) nx_stm32h743_gpio_state_t{
            &g_h743_model.gpio_e, 1U << 11U, true, false, false};
        cs_face =
            new (memory + 0x200U) nx_gpio_port_t{&nx_stm32h743_gpio_ops, cs};
        owner = new (memory + 0x300U) nx_stm32h743_spi_dma_state_t{};
        device = new (memory + 0x600U) nx_stm32h743_spi_dma_endpoint_t{};
        port = new (memory + 0x700U)
            nx_spi_port_t{&nx_stm32h743_spi_dma_ops, owner};
        endpoint = new (memory + 0x800U)
            nx_spi_endpoint_t{&nx_stm32h743_spi_dma_endpoint_ops, device};
        request = new (memory + 0x1000U) nx_spi_request_t{};
        tx = new (memory + 0x2000U) Payload{};
        rx = new (memory + 0x4000U) Payload{};
        owner->wire.registers = &g_h743_spi_dma_model.spi;
        owner->wire.reset = &g_h743_model.rcc.APB2RSTR;
        owner->wire.reset_mask = RCC_APB2RSTR_SPI4RST;
        owner->wire.clock_hz = 64000000U;
        owner->wire.poll_limit = 1U;
        owner->dma = &g_h743_spi_dma_model.dma;
        owner->rx = &g_h743_spi_dma_model.streams[2];
        owner->tx = &g_h743_spi_dma_model.streams[3];
        owner->rx_mux = &g_h743_spi_dma_model.mux[2];
        owner->tx_mux = &g_h743_spi_dma_model.mux[3];
        owner->regions = region;
        owner->region_count = 1U;
        owner->face = port;
        device->wire = {&owner->wire, cs, cs->mask, 1500000U, 0U};
        device->dma = owner;
        device->face = endpoint;
        device->cs_face = cs_face;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_start(&g_nx_stm32h743_system,
                                                        64000000U, 0U, 8U)
                                  .primary);
        if (GetParam())
            g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
        g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
        g_h743_model.rcc.APB2ENR |= RCC_APB2ENR_SPI4EN;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(cs, cs->mask));
        for (unsigned pin : {12U, 13U, 14U})
            ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pin_alternate(
                                      &g_h743_model.gpio_e, pin, 5U));
        nx_request_initialize(&request->base);
        tx->fill(0x57U);
        g_h743_model.spi_watch_pins = true;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_dma_initialize(owner));
    }
    void Step() {
        nx_h743_spi_dma_model_step();
        nx_spi_port_service(port);
    }
    void EmptyExternalFifo() {
        g_h743_spi_dma_model.rx_count = 0U;
        g_h743_spi_dma_model.spi.SR &=
            ~static_cast<uint32_t>(NX_STM32H743_SPI_FIFO_LEVELS);
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        EmptyExternalFifo();
        if (owner->active != nullptr) {
            (void)nx_spi_port_cancel(port, owner->active);
            for (unsigned n = 0U; n < 200U && owner->active != nullptr; ++n)
                Step();
        }
        EXPECT_EQ(nullptr, owner->active);
        if (owner->active != nullptr)
            std::abort();
        nx_result_t stopped = NX_ERROR_BUSY;
        for (unsigned n = 0U; n < 200U && stopped != NX_SUCCESS; ++n) {
            stopped = nx_spi_port_stop(port);
            if (stopped != NX_SUCCESS)
                Step();
        }
        EXPECT_EQ(NX_SUCCESS, stopped);
        if (stopped != NX_SUCCESS)
            std::abort();
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_gpio_stop(cs, cs->mask));
        g_h743_model.core->CCR &= ~static_cast<uint32_t>(SCB_CCR_DC_Msk);
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
        EXPECT_EQ(0, munmap(memory, 0x80000U));
        memory = nullptr;
    }
};

TEST_P(H743SpiDmaDefaultByte, NonemptyRxRetainsLoanUntilExternalEmptyEvent) {
    auto& hardware = g_h743_spi_dma_model;
    /* Initialization already completed its required checked wire reset. */
    const unsigned initialized_resets = hardware.resets;
    hardware.hold_wire = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_request_prepare(request, tx->data(),
                                                 rx->data(), 32U, 2000000U));
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    for (unsigned n = 0U; n < 100U && hardware.starts == 0U; ++n)
        Step();
    ASSERT_EQ(1U, hardware.starts);
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U;
         n < 100U && ((hardware.spi.CR1 & SPI_CR1_CSTART) != 0U ||
                      (hardware.streams[2].CR & DMA_SxCR_EN) != 0U ||
                      (hardware.streams[3].CR & DMA_SxCR_EN) != 0U);
         ++n)
        Step();
    ASSERT_EQ(0U, hardware.spi.CR1 & SPI_CR1_CSTART);
    ASSERT_EQ(0U,
              (hardware.streams[2].CR | hardware.streams[3].CR) & DMA_SxCR_EN);
    hardware.rx_count = 1U;
    hardware.rx_fifo[0] = 0x6dU;
    hardware.spi.RXDR = 0xaabbcc6dU;
    hardware.spi.SR |= SPI_SR_RXP | (1U << SPI_SR_RXPLVL_Pos);
    for (unsigned n = 0U; n < 20U; ++n)
        Step();
    /* The real byte load cannot mutate RAM's FIFO model. Accidentally using
     * its overridden byte hook would pop this cell and release the loan.
     */
    EXPECT_EQ(1U, hardware.rx_count);
    EXPECT_EQ(0xaabbcc6dU, hardware.spi.RXDR);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request->base));
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(device, owner->endpoint);
    EXPECT_EQ(0U, g_h743_model.gpio_e.ODR & cs->mask);
    EXPECT_NE(0U, hardware.spi.CR1 & SPI_CR1_SPE);
    EXPECT_EQ(initialized_resets, hardware.resets);
    EmptyExternalFifo();
    for (unsigned n = 0U; n < 200U && owner->active != nullptr; ++n)
        Step();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request->base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request->base.result);
    EXPECT_EQ(0U, request->base.transferred);
    EXPECT_EQ(nullptr, owner->active);
    EXPECT_EQ(nullptr, owner->endpoint);
    EXPECT_EQ(nullptr, owner->wire.held_cs);
    EXPECT_EQ(0U, owner->rx->M0AR | owner->tx->M0AR | owner->rx->PAR |
                      owner->tx->PAR | owner->rx->NDTR | owner->tx->NDTR);
    EXPECT_EQ(0U, hardware.mux[2].CCR | hardware.mux[3].CCR);
    for (int irq : {13, 14, 84}) {
        EXPECT_FALSE(g_h743_model.irq_enabled[irq]);
        EXPECT_FALSE(g_h743_model.irq_pending[irq]);
        EXPECT_FALSE(g_h743_model.irq_active[irq]);
    }
    EXPECT_EQ(cs->mask, g_h743_model.gpio_e.ODR & cs->mask);
    EXPECT_EQ(0U, hardware.ordering_errors);
    EXPECT_EQ(0U, g_h743_model.spi_reset_order_errors);
    EXPECT_EQ(GetParam() ? 2U : 0U, hardware.cache_invalidates);
}
INSTANTIATE_TEST_SUITE_P(CacheOffAndOn, H743SpiDmaDefaultByte, testing::Bool());
