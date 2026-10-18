/**
 * \file            h743_spi_dma_test.cpp
 * \brief           Finite public DMA loans through actual memory and wire stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_dma_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_spi_dma.h"
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

struct ChildTrace {
    volatile bool observed;
    volatile bool fault;
    volatile bool borrowed;
    volatile bool masked;
    volatile bool selected;
    volatile unsigned state;
    volatile unsigned second_result;
    volatile unsigned second_state;
    volatile unsigned starts;
    volatile unsigned gated;
    volatile unsigned deliveries;
    volatile unsigned irq_reads;
    volatile unsigned irq_writes;
    volatile unsigned service_reads;
    volatile unsigned service_writes;
};
static testing::MockFunction<void()>* s_enable_observer;
static ChildTrace* s_child;
static nx_stm32h743_spi_dma_state_t* s_child_owner;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char*, unsigned) {
    ++g_h743_spi_dma_model.faults;
    if (s_child != nullptr) {
        s_child->fault = domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO;
        s_child->borrowed = s_child_owner->active != nullptr;
        s_child->masked = nx_arch_irq_is_masked();
        s_child->state = static_cast<unsigned>(
            nx_request_state(&s_child_owner->active->base));
        s_child->selected = (g_h743_model.gpio_e.ODR & (1U << 11U)) == 0U;
        __atomic_store_n(&s_child->observed, true, __ATOMIC_RELEASE);
    }
}

class H743SpiDma : public testing::Test {
  protected:
    using Payload = std::array<uint8_t, 65536>;
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
    unsigned max_service_reads = 0U;
    unsigned max_service_writes = 0U;
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
        rx = new (memory + 0x22000U) Payload{};
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
        g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
        g_h743_model.rcc.APB2ENR |= RCC_APB2ENR_SPI4EN;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(cs, cs->mask));
        for (unsigned pin : {12U, 13U, 14U})
            ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pin_alternate(
                                      &g_h743_model.gpio_e, pin, 5U));
        nx_request_initialize(&request->base);
        for (size_t n = 0U; n < tx->size(); ++n)
            (*tx)[n] = static_cast<uint8_t>(n * 13U + 7U);
        g_h743_model.spi_watch_pins = true;
    }
    void Init() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_dma_initialize(owner));
    }
    void Prepare(size_t length = 4U, nx_time_us_t deadline = 2000000U) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_spi_request_prepare(request, tx->data(), rx->data(),
                                         length, deadline));
    }
    void Service() {
        unsigned reads = g_h743_spi_dma_model.reads;
        unsigned writes = g_h743_spi_dma_model.writes;
        nx_spi_port_service(port);
        max_service_reads =
            std::max(max_service_reads, g_h743_spi_dma_model.reads - reads);
        max_service_writes =
            std::max(max_service_writes, g_h743_spi_dma_model.writes - writes);
    }
    unsigned Dispatch() {
        unsigned deliveries = 0U;
        for (unsigned turn = 0U; turn < 8U; ++turn) {
            bool delivered = false;
            for (int irq : {13, 14, 84}) {
                if (nx_h743_spi_dma_model_level(irq))
                    g_h743_model.irq_pending[irq] = true;
                if (!g_h743_model.irq_enabled[irq] ||
                    !g_h743_model.irq_pending[irq] || nx_arch_irq_is_masked())
                    continue;
                g_h743_model.irq_pending[irq] = false;
                g_h743_model.irq_active[irq] = true;
                g_h743_spi_dma_model.exception =
                    static_cast<unsigned>(irq + 16);
                unsigned reads = g_h743_spi_dma_model.reads;
                unsigned writes = g_h743_spi_dma_model.writes;
                if (irq == 84)
                    nx_stm32h743_spi_dma_wire_irq(owner);
                else
                    nx_stm32h743_spi_dma_irq(owner, irq == 13);
                g_h743_spi_dma_model.maximum_irq_reads =
                    std::max(g_h743_spi_dma_model.maximum_irq_reads,
                             g_h743_spi_dma_model.reads - reads);
                g_h743_spi_dma_model.maximum_irq_writes =
                    std::max(g_h743_spi_dma_model.maximum_irq_writes,
                             g_h743_spi_dma_model.writes - writes);
                g_h743_spi_dma_model.exception = 0U;
                g_h743_model.irq_active[irq] = false;
                if (nx_h743_spi_dma_model_level(irq))
                    g_h743_model.irq_pending[irq] = true;
                ++deliveries;
                delivered = true;
            }
            if (!delivered)
                break;
        }
        return deliveries;
    }
    void Launch() {
        for (unsigned n = 0U;
             n < 80U && g_h743_spi_dma_model.starts == 0U &&
             nx_request_state(&request->base) != NX_REQUEST_SETTLED;
             ++n) {
            Service();
            if (g_h743_spi_dma_model.starts == 0U)
                nx_h743_spi_dma_model_step();
            EXPECT_LT(Dispatch(), 8U);
        }
        ASSERT_EQ(1U, g_h743_spi_dma_model.starts);
    }
    void Start(size_t length = 4U) {
        Init();
        Prepare(length);
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
        Launch();
    }
    void Pump(unsigned limit = 700000U) {
        for (unsigned n = 0U; n < limit && owner->active != nullptr; ++n) {
            nx_h743_spi_dma_model_step();
            EXPECT_LT(Dispatch(), 8U);
            Service();
        }
    }
    void Settled(nx_result_t result, size_t bytes) {
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request->base));
        EXPECT_EQ(result, request->base.result);
        EXPECT_EQ(bytes, request->base.transferred);
        EXPECT_EQ(nullptr, owner->active);
        EXPECT_EQ(nullptr, owner->endpoint);
        EXPECT_EQ(nullptr, owner->wire.held_cs);
        EXPECT_EQ(0U, owner->rx->CR & DMA_SxCR_EN);
        EXPECT_EQ(0U, owner->tx->CR & DMA_SxCR_EN);
        EXPECT_EQ(0U, owner->rx->M0AR | owner->tx->M0AR | owner->rx->PAR |
                          owner->tx->PAR | owner->rx->NDTR | owner->tx->NDTR);
        EXPECT_EQ(0U, g_h743_spi_dma_model.spi.CFG1 &
                          (SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN));
        EXPECT_NE(0U, g_h743_model.gpio_e.ODR & cs->mask);
        EXPECT_EQ(0U, g_h743_spi_dma_model.ordering_errors);
        EXPECT_EQ(0U, g_h743_model.spi_reset_order_errors);
        EXPECT_EQ(0U, g_h743_model.spi_selected_control_losses);
    }
    void Child(unsigned scenario) {
        const bool terminal = scenario == 1U;
        auto* trace = static_cast<ChildTrace*>(
            mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0));
        ASSERT_NE(MAP_FAILED, trace);
        new (trace) ChildTrace{};
        pid_t pid = fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            s_child = trace;
            s_child_owner = owner;
            Init();
            Prepare(4U);
            g_h743_spi_dma_model.ignore_start = scenario == 0U;
            if (nx_spi_endpoint_submit(endpoint, request) != NX_SUCCESS)
                _exit(3);
            Launch();
            if (terminal) {
                g_h743_spi_dma_model.hold_mask[0] = true;
                g_h743_spi_dma_model.hold_w1c[0] = true;
                g_h743_spi_dma_model.hold_disable[13] = true;
                nx_h743_spi_dma_model_error(true);
                Dispatch();
                _exit(4);
            }
            if (scenario == 2U) {
                g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
                Service();
                g_h743_model.core->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;
            }
            if (scenario == 3U) {
                nx_h743_spi_dma_model_error(true);
                g_h743_model.rcc.AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_DMA1EN;
                trace->deliveries = Dispatch();
                trace->gated = g_h743_spi_dma_model.gated_accesses;
            }
            if (scenario == 4U) {
                nx_spi_request_t second{};
                nx_request_initialize(&second.base);
                if (nx_spi_request_prepare(&second, tx->data(), rx->data(), 1U,
                                           2000000U) != NX_SUCCESS)
                    _exit(5);
                g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
                trace->second_result = static_cast<unsigned>(
                    nx_spi_endpoint_submit(endpoint, &second));
                trace->second_state =
                    static_cast<unsigned>(nx_request_state(&second.base));
                g_h743_model.core->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;
            }
            for (unsigned n = 0U; n < 200U; ++n) {
                nx_h743_spi_dma_model_step();
                Service();
            }
            trace->borrowed = owner->active == request;
            trace->state =
                static_cast<unsigned>(nx_request_state(&request->base));
            trace->selected = (g_h743_model.gpio_e.ODR & cs->mask) == 0U;
            trace->starts = g_h743_spi_dma_model.starts;
            trace->irq_reads = g_h743_spi_dma_model.maximum_irq_reads;
            trace->irq_writes = g_h743_spi_dma_model.maximum_irq_writes;
            trace->service_reads = max_service_reads;
            trace->service_writes = max_service_writes;
            __atomic_store_n(&trace->observed, true, __ATOMIC_RELEASE);
            for (;;)
                pause();
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
        EXPECT_TRUE(trace->borrowed);
        EXPECT_EQ(static_cast<unsigned>(NX_REQUEST_QUARANTINED), trace->state);
        EXPECT_TRUE(trace->selected);
        if (terminal) {
            EXPECT_TRUE(trace->fault);
            EXPECT_TRUE(trace->masked);
        } else
            EXPECT_EQ(1U, trace->starts);
        if (scenario == 3U) {
            EXPECT_EQ(0U, trace->gated);
            EXPECT_LT(trace->deliveries, 3U);
        }
        if (scenario == 4U) {
            EXPECT_EQ(static_cast<unsigned>(NX_ERROR_STATE),
                      trace->second_result);
            EXPECT_EQ(static_cast<unsigned>(NX_REQUEST_READY),
                      trace->second_state);
        }
        if (!terminal) {
            EXPECT_LE(trace->irq_reads, 16U);
            EXPECT_LE(trace->irq_writes, 8U);
            EXPECT_LE(trace->service_reads, 64U);
            EXPECT_LE(trace->service_writes, 16U);
        }
        if (!exited) {
            EXPECT_EQ(0, kill(pid, SIGKILL));
            EXPECT_EQ(pid, waitpid(pid, &status, 0));
            EXPECT_TRUE(WIFSIGNALED(status));
        }
        trace->~ChildTrace();
        EXPECT_EQ(0, munmap(trace, 4096U));
    }
    void TearDown() override {
        if (memory == nullptr)
            return;
        if (owner->active != nullptr) {
            (void)nx_spi_port_cancel(port, owner->active);
            Pump();
        }
        EXPECT_EQ(nullptr, owner->active);
        if (owner->active != nullptr)
            std::abort();
        nx_result_t stopped = NX_ERROR_BUSY;
        for (unsigned n = 0U; n < 80U && stopped != NX_SUCCESS; ++n) {
            stopped = nx_spi_port_stop(port);
            if (stopped != NX_SUCCESS) {
                nx_h743_spi_dma_model_step();
                Service();
            }
        }
        EXPECT_EQ(NX_SUCCESS, stopped);
        if (stopped != NX_SUCCESS)
            std::abort();
        RecordProperty("service_max_reads",
                       static_cast<int>(max_service_reads));
        RecordProperty("service_max_writes",
                       static_cast<int>(max_service_writes));
        RecordProperty(
            "irq_max_reads",
            static_cast<int>(g_h743_spi_dma_model.maximum_irq_reads));
        RecordProperty(
            "irq_max_writes",
            static_cast<int>(g_h743_spi_dma_model.maximum_irq_writes));
        EXPECT_LE(max_service_reads, 64U);
        EXPECT_LE(max_service_writes, 16U);
        EXPECT_LE(g_h743_spi_dma_model.maximum_irq_reads, 16U);
        EXPECT_LE(g_h743_spi_dma_model.maximum_irq_writes, 8U);
        EXPECT_EQ(0U, g_h743_spi_dma_model.gated_accesses);
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_gpio_stop(cs, cs->mask));
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
        tx->~Payload();
        rx->~Payload();
        request->~nx_spi_request_t();
        device->~nx_stm32h743_spi_dma_endpoint_t();
        owner->~nx_stm32h743_spi_dma_state_t();
        endpoint->~nx_spi_endpoint_t();
        port->~nx_spi_port_t();
        cs_face->~nx_gpio_port_t();
        cs->~nx_stm32h743_gpio_state_t();
        region->~nx_dma_memory_region_t();
        EXPECT_EQ(0, munmap(memory, 0x80000U));
        memory = nullptr;
    }
};
TEST_F(H743SpiDma, StaticFacesDoNotReadOrWriteHardware) {
    const unsigned reads = g_h743_spi_dma_model.reads,
                   writes = g_h743_spi_dma_model.writes;
    EXPECT_EQ(owner, port->context);
    EXPECT_EQ(device, endpoint->context);
    EXPECT_EQ(reads, g_h743_spi_dma_model.reads);
    EXPECT_EQ(writes, g_h743_spi_dma_model.writes);
}
TEST_F(H743SpiDma, InitializeRejectsNonVAndBadTrimWithoutAcquisition) {
    g_h743_model.silicon_id = 0x10030450U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_FALSE(owner->retained);
    g_h743_model.silicon_id = 0x20030450U;
    g_h743_model.rcc.HSICFGR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_dma_initialize(owner));
    g_h743_model.rcc.HSICFGR = 0x40U << RCC_HSICFGR_HSITRIM_Pos;
}
TEST_F(H743SpiDma, GateOffRejectsBeforeUnclockedProbe) {
    g_h743_model.rcc.AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_DMA1EN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_EQ(0U, g_h743_spi_dma_model.gated_accesses);
    g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
}
TEST_F(H743SpiDma, ForeignStreamMuxAndPendingAreNotCleared) {
    owner->rx->CR = DMA_SxCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_EQ(DMA_SxCR_EN, owner->rx->CR);
    owner->rx->CR = 0U;
    owner->rx_mux->CCR = 46U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_EQ(46U, owner->rx_mux->CCR);
    owner->rx_mux->CCR = 0U;
    g_h743_model.irq_pending[13] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_TRUE(g_h743_model.irq_pending[13]);
    g_h743_model.irq_pending[13] = false;
}
TEST_F(H743SpiDma, PartialInitializeRetainsRetryResponsibility) {
    g_h743_model.hold_spi_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_TRUE(owner->retained);
    EXPECT_EQ(nullptr, owner->active);
    EXPECT_NE(NX_SUCCESS, nx_spi_port_stop(port));
    g_h743_model.hold_spi_reset = false;
}
TEST_F(H743SpiDma, RequestCopiesActualCallerDataAndWholeCs) {
    Start();
    Pump();
    Settled(NX_SUCCESS, 4U);
    for (unsigned n = 0U; n < 4U; ++n)
        EXPECT_EQ(static_cast<uint8_t>(~(*tx)[n]), (*rx)[n]);
    EXPECT_EQ(4U, g_h743_spi_dma_model.frames);
}
TEST_F(H743SpiDma, OneAndMaximumByteCountsRemainFinite) {
    Start(65535U);
    Pump();
    Settled(NX_SUCCESS, 65535U);
    EXPECT_EQ(static_cast<uint8_t>(~(*tx)[65534U]), (*rx)[65534U]);
    Prepare(1U);
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    Pump();
    Settled(NX_SUCCESS, 1U);
}
TEST_F(H743SpiDma, InvalidAndProtectedPayloadsNeverBorrow) {
    Init();
    Prepare();
    for (const void* address :
         {static_cast<const void*>(owner), static_cast<const void*>(device),
          static_cast<const void*>(port), static_cast<const void*>(endpoint),
          static_cast<const void*>(region), static_cast<const void*>(cs),
          static_cast<const void*>(cs_face),
          static_cast<const void*>(&g_nx_stm32h743_system)}) {
        request->tx = static_cast<const uint8_t*>(address);
        EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_submit(endpoint, request));
        EXPECT_EQ(nullptr, owner->active);
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request->base));
    }
    request->tx = tx->data();
    request->rx = tx->data();
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_submit(endpoint, request));
    request->rx = rx->data();
    request->length = 65536U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_submit(endpoint, request));
}
TEST_F(H743SpiDma, WrappedRegionCountRejectsBeforeReadingTable) {
    Init();
    Prepare();
    owner->region_count = SIZE_MAX;
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_submit(endpoint, request));
    owner->region_count = 1U;
}
TEST_F(H743SpiDma, NonAxiAndFixedModesAreExplicitlyRejected) {
    Init();
    Prepare();
    request->tx = reinterpret_cast<const uint8_t*>(0x20000000U);
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_spi_endpoint_submit(endpoint, request));
    request->tx = tx->data();
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_spi_endpoint_submit_memory(endpoint, request,
                                            NX_SPI_MEMORY_RX_FIXED));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_submit_memory(endpoint, request, 4U));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request->base));
}
TEST_F(H743SpiDma, AlreadyExpiredAdmissionHasNoHardwareEffects) {
    Init();
    Prepare(4U, nx_time_now_us());
    unsigned writes = g_h743_spi_dma_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_spi_endpoint_submit(endpoint, request));
    EXPECT_EQ(writes, g_h743_spi_dma_model.writes);
    EXPECT_EQ(nullptr, owner->active);
}
TEST_F(H743SpiDma, BusyReinitializeNeverRevokesLiveRequest) {
    Start();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_spi_dma_initialize(owner));
    EXPECT_EQ(owner->active, request);
    Pump();
    Settled(NX_SUCCESS, 4U);
}
TEST_F(H743SpiDma, PrestartTsizeFailureIsAcceptedAndNeverNeedsSusp) {
    Init();
    Prepare();
    g_h743_spi_dma_model.ignore = &g_h743_spi_dma_model.spi.CR2;
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    for (unsigned n = 0U; n < 40U && owner->active != nullptr; ++n) {
        Service();
        nx_h743_spi_dma_model_step();
    }
    g_h743_spi_dma_model.ignore = nullptr;
    Pump();
    Settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(0U, g_h743_spi_dma_model.starts);
    EXPECT_EQ(0U, g_h743_spi_dma_model.suspend_commands);
    EXPECT_EQ(0U, g_h743_spi_dma_model.frames);
}
TEST_F(H743SpiDma, PrestartSpeFailureDoesNotReturnAnAdmissionError) {
    Init();
    Prepare();
    g_h743_spi_dma_model.ignore = &g_h743_spi_dma_model.spi.CR1;
    g_h743_spi_dma_model.ignore_mask = SPI_CR1_SPE;
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    for (unsigned n = 0U; n < 15U; ++n) {
        Service();
        nx_h743_spi_dma_model_step();
    }
    g_h743_spi_dma_model.ignore = nullptr;
    Pump();
    Settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(0U, g_h743_spi_dma_model.starts);
}
TEST_F(H743SpiDma, AttemptedButRefusedStartKeepsLoanWithoutFakeFlags) {
    Child(0U);
}
TEST_F(H743SpiDma, CpuPublicationPrecedesDmaEnable) {
    Init();
    Prepare(1U);
    nx_h743_spi_dma_model_cpu_store(tx->data(), 0x5aU);
    testing::MockFunction<void()> observer;
    s_enable_observer = &observer;
    EXPECT_CALL(observer, Call()).Times(2).WillRepeatedly([] {
        EXPECT_FALSE(g_h743_spi_dma_model.cpu_pending);
    });
    g_h743_spi_dma_model.enable_observer = [] { s_enable_observer->Call(); };
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    Pump();
    g_h743_spi_dma_model.enable_observer = nullptr;
    s_enable_observer = nullptr;
    Settled(NX_SUCCESS, 1U);
    EXPECT_EQ(0xa5U, (*rx)[0]);
    EXPECT_FALSE(g_h743_spi_dma_model.cpu_pending);
}
TEST_F(H743SpiDma, TxTcDoesNotReleaseRxOrWire) {
    Start(4U);
    g_h743_spi_dma_model.hold_wire = true;
    for (unsigned n = 0U; n < 8U; ++n) {
        nx_h743_spi_dma_model_step();
        Dispatch();
        Service();
    }
    EXPECT_EQ(0U, owner->tx->NDTR);
    EXPECT_EQ(4U, owner->rx->NDTR);
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request->base));
    g_h743_spi_dma_model.hold_wire = false;
    Pump();
    Settled(NX_SUCCESS, 4U);
}
TEST_F(H743SpiDma, EotIrqPreservesTaskStopEvidence) {
    Start(1U);
    for (unsigned n = 0U;
         n < 20U && (g_h743_spi_dma_model.spi.SR & SPI_SR_EOT) == 0U; ++n)
        nx_h743_spi_dma_model_step();
    ASSERT_NE(0U, g_h743_spi_dma_model.spi.SR & SPI_SR_EOT);
    Dispatch();
    EXPECT_EQ(SPI_SR_EOT | SPI_SR_TXC,
              g_h743_spi_dma_model.spi.SR & (SPI_SR_EOT | SPI_SR_TXC));
    EXPECT_EQ(0U, g_h743_spi_dma_model.spi.IER & SPI_IER_EOTIE);
    EXPECT_EQ(0U, g_h743_spi_dma_model.eot_clears_in_irq);
    Pump();
    Settled(NX_SUCCESS, 1U);
}
TEST_F(H743SpiDma, EotAfterObserveNeedsNoSyntheticSusp) {
    Start(1U);
    g_h743_spi_dma_model.complete_on_cr1_read = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(0U, g_h743_spi_dma_model.unsafe_commands);
}
TEST_F(H743SpiDma, CancelAndSoftwareStopTcNeverBecomeSuccess) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, DeadlineFirstCauseSurvivesLateEot) {
    Init();
    Prepare(1U, nx_time_now_us() + 200U);
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_submit(endpoint, request));
    Launch();
    g_h743_model.timer.CNT += 200U;
    Service();
    Pump();
    Settled(NX_ERROR_TIMEOUT, 0U);
}
TEST_F(H743SpiDma, DmaEnableRefusalKeepsBorrowUntilRealStop) {
    Start();
    g_h743_spi_dma_model.hold_en[0] = true;
    g_h743_spi_dma_model.hold_wire = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Dispatch();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request->base));
    g_h743_spi_dma_model.hold_en[0] = false;
    g_h743_spi_dma_model.hold_wire = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, PendingRxMemoryCommitKeepsActualEnableHigh) {
    Start(1U);
    g_h743_spi_dma_model.hold_rx_commit = true;
    for (unsigned n = 0U; n < 20U && !g_h743_spi_dma_model.rx_pending; ++n)
        nx_h743_spi_dma_model_step();
    ASSERT_TRUE(g_h743_spi_dma_model.rx_pending);
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 20U; ++n)
        Service();
    EXPECT_NE(0U, owner->rx->CR & DMA_SxCR_EN);
    EXPECT_TRUE(g_h743_spi_dma_model.rx_stop_pending);
    EXPECT_EQ(0U, (*rx)[0]);
    EXPECT_EQ(request, owner->active);
    nx_h743_spi_dma_model_dsb();
    EXPECT_TRUE(g_h743_spi_dma_model.rx_pending);
    g_h743_spi_dma_model.hold_rx_commit = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(static_cast<uint8_t>(~(*tx)[0]), (*rx)[0]);
}
TEST_F(H743SpiDma, SpiSuspendRefusalRetainsUntilRealEvent) {
    Start();
    g_h743_spi_dma_model.hold_wire = true;
    g_h743_spi_dma_model.hold_suspend = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request->base));
    g_h743_spi_dma_model.hold_suspend = false;
    g_h743_spi_dma_model.hold_wire = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, StickyTransferErrorWithdrawsLevelBeforeTaskRuns) {
    Start();
    nx_h743_spi_dma_model_error(true);
    EXPECT_LT(Dispatch(), 3U);
    EXPECT_FALSE(nx_h743_spi_dma_model_level(13));
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, ForeignExceptionHasZeroDmaEffects) {
    Start();
    nx_h743_spi_dma_model_error(true);
    g_h743_spi_dma_model.exception = 31U;
    unsigned writes = g_h743_spi_dma_model.writes;
    nx_stm32h743_spi_dma_irq(owner, true);
    EXPECT_EQ(writes, g_h743_spi_dma_model.writes);
    g_h743_spi_dma_model.exception = 0U;
    Dispatch();
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, MaskRefusalIsolatesOnlyOwnedVector) {
    Start();
    g_h743_spi_dma_model.hold_mask[0] = true;
    nx_h743_spi_dma_model_error(true);
    EXPECT_LT(Dispatch(), 3U);
    EXPECT_FALSE(g_h743_model.irq_enabled[13]);
    EXPECT_TRUE(g_h743_model.irq_enabled[14]);
    g_h743_spi_dma_model.hold_mask[0] = false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, W1cRefusalKeepsDiagnosticsAndLoan) {
    Start();
    g_h743_spi_dma_model.hold_w1c[0] = true;
    nx_h743_spi_dma_model_error(true);
    Dispatch();
    for (unsigned n = 0U; n < 160U; ++n) {
        nx_h743_spi_dma_model_step();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_FALSE(g_h743_model.irq_enabled[13]);
    EXPECT_NE(0U, owner->rx_seen & DMA_LISR_TEIF2);
    g_h743_spi_dma_model.hold_w1c[0] = false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, DualRefusalNeverTailChainsIntoTaskStarvation) {
    Start();
    g_h743_spi_dma_model.hold_mask[0] = g_h743_spi_dma_model.hold_w1c[0] = true;
    g_h743_spi_dma_model.dma.LISR |= DMA_LISR_HTIF0;
    nx_h743_spi_dma_model_error(true);
    EXPECT_LT(Dispatch(), 3U);
    EXPECT_FALSE(g_h743_model.irq_enabled[13]);
    EXPECT_NE(0U, g_h743_spi_dma_model.dma.LISR & DMA_LISR_HTIF0);
    g_h743_spi_dma_model.hold_mask[0] = g_h743_spi_dma_model.hold_w1c[0] =
        false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, PendingClearFailureRetainsUntilRealRetry) {
    Start();
    g_h743_spi_dma_model.hold_pending_clear[13] = true;
    g_h743_spi_dma_model.pending_on_disable[13] = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Dispatch();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_FALSE(g_h743_model.irq_enabled[13]);
    g_h743_spi_dma_model.hold_pending_clear[13] = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, ClearAddressRefusalRetainsHardwareLoan) {
    Start();
    g_h743_spi_dma_model.ignore = &owner->rx->M0AR;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Dispatch();
        Service();
    }
    EXPECT_NE(0U, owner->rx->M0AR);
    EXPECT_EQ(request, owner->active);
    g_h743_spi_dma_model.ignore = nullptr;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, CsReadbackFailureRetainsSelectedLoan) {
    Start();
    g_h743_model.spi_hold_cs_high = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(cs, owner->wire.held_cs);
    g_h743_model.spi_hold_cs_high = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, SckHandoffFailureCannotResetOrReleaseLoan) {
    Start();
    g_h743_model.spi_hold_sck_output = true;
    unsigned resets = g_h743_spi_dma_model.resets;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(resets, g_h743_spi_dma_model.resets);
    g_h743_model.spi_hold_sck_output = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
}
TEST_F(H743SpiDma, ResetRefusalAfterCsHighNeverReplaysPayload) {
    Start();
    g_h743_model.hold_spi_reset = true;
    ASSERT_EQ(NX_SUCCESS, nx_spi_port_cancel(port, request));
    for (unsigned n = 0U; n < 200U; ++n) {
        nx_h743_spi_dma_model_step();
        Service();
    }
    EXPECT_EQ(request, owner->active);
    EXPECT_NE(0U, g_h743_model.gpio_e.ODR & cs->mask);
    unsigned frames = g_h743_spi_dma_model.frames;
    g_h743_model.hold_spi_reset = false;
    Pump();
    Settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(frames, g_h743_spi_dma_model.frames);
}
TEST_F(H743SpiDma, SettledRequestAndBuffersAreNeverTouchedAgain) {
    Start(1U);
    Pump();
    Settled(NX_SUCCESS, 1U);
    ASSERT_EQ(0, mprotect(memory + 0x1000U, 4096U, PROT_NONE));
    ASSERT_EQ(0, mprotect(memory + 0x2000U, 4096U, PROT_NONE));
    ASSERT_EQ(0, mprotect(memory + 0x22000U, 4096U, PROT_NONE));
    Service();
    Dispatch();
    nx_h743_spi_dma_model_step();
    ASSERT_EQ(0, mprotect(memory + 0x1000U, 4096U, PROT_READ | PROT_WRITE));
    ASSERT_EQ(0, mprotect(memory + 0x2000U, 4096U, PROT_READ | PROT_WRITE));
    ASSERT_EQ(0, mprotect(memory + 0x22000U, 4096U, PROT_READ | PROT_WRITE));
}
TEST_F(H743SpiDma, UnisolatableLevelEntersMaskedNonreturningFault) {
    Child(1U);
}

TEST_F(H743SpiDma, ObservedCacheViolationRemainsTaintedAfterBitRestored) {
    Child(2U);
}
TEST_F(H743SpiDma, ExternalModelEotClearAndIdleSuspendDoNotInventProof) {
    auto& m = g_h743_spi_dma_model;
    m.spi.CR2 = 1U;
    m.spi.SR |= SPI_SR_EOT | SPI_SR_TXC;
    nx_h743_spi_dma_model_write(&m.spi.CR1, SPI_CR1_CSUSP);
    EXPECT_EQ(0U, m.spi.CR1 & (SPI_CR1_CSUSP | SPI_CR1_CSTART));
    EXPECT_EQ(0U, m.spi.SR & SPI_SR_SUSP);
    nx_h743_spi_dma_model_write(&m.spi.IFCR, SPI_IFCR_EOTC);
    EXPECT_EQ(0U, m.spi.SR & (SPI_SR_EOT | SPI_SR_TXC));
    m.spi.CR2 = 0U;
}
TEST_F(H743SpiDma, ExternalModelNvicDisablePreservesStickyPending) {
    auto& m = g_h743_spi_dma_model;
    m.streams[2].CR = DMA_SxCR_TEIE;
    m.dma.LISR = DMA_LISR_TEIF2;
    nx_h743_spi_dma_model_enable(13);
    g_h743_model.irq_pending[13] = true;
    EXPECT_TRUE(nx_h743_spi_dma_model_level(13));
    nx_h743_spi_dma_model_disable(13);
    EXPECT_FALSE(g_h743_model.irq_enabled[13]);
    EXPECT_TRUE(g_h743_model.irq_pending[13]);
    EXPECT_NE(0U, m.dma.LISR & DMA_LISR_TEIF2);
    nx_h743_spi_dma_model_write(&m.dma.LIFCR, DMA_LIFCR_CTEIF2);
    nx_h743_spi_dma_model_clear(13);
    m.streams[2].CR = 0U;
}
TEST_F(H743SpiDma, ColdConfiguredAddressesAndFifoAreNotAdopted) {
    for (volatile uint32_t* reg :
         {&owner->rx->PAR, &owner->tx->M1AR, &owner->rx->FCR}) {
        const uint32_t original = *reg;
        const uint32_t foreign = reg == &owner->rx->FCR ? 0x25U : 0x24004000U;
        *reg = foreign;
        const unsigned writes = g_h743_spi_dma_model.writes;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_spi_dma_initialize(owner));
        EXPECT_EQ(writes, g_h743_spi_dma_model.writes);
        EXPECT_FALSE(owner->retained);
        if (owner->retained) {
            for (unsigned n = 0U; n < 80U && owner->retained; ++n) {
                (void)nx_spi_port_stop(port);
                nx_h743_spi_dma_model_step();
                Service();
            }
            ASSERT_FALSE(owner->retained);
        }
        EXPECT_EQ(foreign, *reg);
        *reg = original;
    }
}
TEST_F(H743SpiDma, IrqGateViolationIsolatesWithoutUnclockedAccess) {
    Child(3U);
}
TEST_F(H743SpiDma, RealNonVParentBootStillRejectsSpiAdmission) {
    for (uint32_t revision : {0x1003U, 0x2001U, 0x1234U}) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
        g_h743_model.silicon_id = (revision << 16U) | 0x450U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_start(&g_nx_stm32h743_system,
                                                        64000000U, 0U, 8U)
                                  .primary);
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32h743_spi_dma_initialize(owner));
        EXPECT_FALSE(owner->retained);
    }
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
    g_h743_model.silicon_id = 0x20030450U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_start(&g_nx_stm32h743_system,
                                                    64000000U, 0U, 8U)
                              .primary);
}

TEST_F(H743SpiDma, SpiMaskRefusalPreservesEvidenceAndIsolatesVector) {
    Start(1U);
    g_h743_spi_dma_model.hold_mask[2] = true;
    for (unsigned n = 0U; n < 16U; ++n)
        nx_h743_spi_dma_model_step();
    EXPECT_LT(Dispatch(), 3U);
    EXPECT_FALSE(g_h743_model.irq_enabled[84]);
    EXPECT_EQ(SPI_SR_EOT | SPI_SR_TXC,
              g_h743_spi_dma_model.spi.SR & (SPI_SR_EOT | SPI_SR_TXC));
    EXPECT_EQ(0U, g_h743_spi_dma_model.eot_clears_in_irq);
    EXPECT_EQ(request, owner->active);
    g_h743_spi_dma_model.hold_mask[2] = false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, SpiFlagClearFailureLatchesCleanupErrorBeforeRetry) {
    Start(1U);
    g_h743_spi_dma_model.hold_w1c[2] = true;
    Pump(200U);
    EXPECT_EQ(request, owner->active);
    EXPECT_FALSE(g_h743_model.irq_enabled[84]);
    EXPECT_NE(0U, g_h743_spi_dma_model.spi.SR & SPI_SR_EOT);
    g_h743_spi_dma_model.hold_w1c[2] = false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}
TEST_F(H743SpiDma, SuccessfulDataStillReportsRealCsWithdrawalFailure) {
    Start(1U);
    g_h743_model.spi_hold_cs_high = true;
    Pump(200U);
    EXPECT_EQ(request, owner->active);
    EXPECT_EQ(cs, owner->wire.held_cs);
    g_h743_model.spi_hold_cs_high = false;
    Pump();
    Settled(NX_ERROR_IO, 0U);
}

TEST_F(H743SpiDma, RejectedSecondSubmitLatchesObservedCacheViolation) {
    Child(4U);
}

TEST_F(H743SpiDma, ExternalTransferErrorWaitsForPendingMemoryCommit) {
    Start(1U);
    g_h743_spi_dma_model.hold_rx_commit = true;
    for (unsigned n = 0U; n < 16U && !g_h743_spi_dma_model.rx_pending; ++n)
        nx_h743_spi_dma_model_step();
    ASSERT_TRUE(g_h743_spi_dma_model.rx_pending);
    EXPECT_NE(0U, owner->rx->CR & DMA_SxCR_EN);
    nx_h743_spi_dma_model_error(true);
    EXPECT_NE(0U, owner->rx->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, g_h743_spi_dma_model.memory_commits);
    EXPECT_LT(Dispatch(), 3U);
    Service();
    EXPECT_EQ(request, owner->active);
    g_h743_spi_dma_model.hold_rx_commit = false;
    Pump();
    EXPECT_EQ(1U, g_h743_spi_dma_model.memory_commits);
    Settled(NX_ERROR_IO, 0U);
}
