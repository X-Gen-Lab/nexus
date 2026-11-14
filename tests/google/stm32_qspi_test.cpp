/**
 * \file            stm32_qspi_test.cpp
 * \brief           Actual official QSPI byte transport and physical Flash exit
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_qspi.h"
#if NEXUS_QSPI_CPU_MAP
#include "stm32f4_qspi_mapped.h"
#endif
nx_stm32_system_t g_nx_stm32_system{};
uint32_t SystemCoreClock = 16000000U;
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
}
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
class Hardware {
  public:
    MOCK_METHOD(void, Instruction, (uint8_t, uint32_t, uint32_t));
};
Hardware* hardware;
nx_time_us_t now;
nx_time_us_t wire_end;
nx_time_us_t pulse_end;
nx_time_us_t abort_end;
std::array<uint8_t, 8192> memory;
std::vector<uint8_t> sent;
std::deque<uint8_t> fifo;
std::vector<uint8_t> instructions;
uint8_t opcode;
uint8_t sr1;
uint8_t sr2;
uint8_t sr3;
uint32_t address;
uint32_t expected;
uint32_t pulses;
uint32_t produced;
bool wedge_wire;
bool wedge_pulse;
bool refuse_abort;
bool refuse_gate_off;
bool refuse_gate_on;
bool refuse_af;
bool preserve_foreign;
bool refuse_dlr;
bool refuse_program_terminal_clear;
unsigned foreign_writes;
unsigned legacy_callbacks;
bool privileged;
bool isr;
bool masked;
bool tx;
bool active;
bool dma_armed;
size_t dma_progress;
bool refuse_dma_stop;
bool refuse_dma_descriptor;
#if NEXUS_QSPI_CPU_MAP
bool refuse_map_command;
unsigned map_barriers;
#endif

void dma_flags_clear() {
    DMA2->HISR &= ~DMA2->HIFCR;
    DMA2->HIFCR = 0U;
}

void dma_advance();

void fifo_flags() {
    QUADSPI->SR &= ~static_cast<uint32_t>(QUADSPI_SR_FLEVEL | QUADSPI_SR_FTF);
    QUADSPI->SR |= static_cast<uint32_t>(fifo.size()) << QUADSPI_SR_FLEVEL_Pos;
    const bool indirect_write = (QUADSPI->CCR & QUADSPI_CCR_FMODE) == 0U &&
                                (QUADSPI->CCR & QUADSPI_CCR_DMODE) != 0U &&
                                (QUADSPI->CR & QUADSPI_CR_EN) != 0U;
    if ((indirect_write && fifo.size() < 32U) || (!tx && !fifo.empty()))
        QUADSPI->SR |= QUADSPI_SR_FTF;
    const uint32_t sources =
        ((QUADSPI->SR & QUADSPI_SR_TCF) != 0U ? QUADSPI_CR_TCIE : 0U) |
        ((QUADSPI->SR & QUADSPI_SR_TEF) != 0U ? QUADSPI_CR_TEIE : 0U) |
        ((QUADSPI->SR & QUADSPI_SR_FTF) != 0U ? QUADSPI_CR_FTIE : 0U);
    if ((sources & QUADSPI->CR) != 0U)
        NVIC->ISPR[static_cast<unsigned>(QUADSPI_IRQn) / 32U] |=
            1U << (static_cast<unsigned>(QUADSPI_IRQn) % 32U);
}
void finish_pulse() {
    if ((opcode != 0x02U || sent.empty()) && opcode != 0x20U)
        return;
    if ((sr1 & 2U) == 0U)
        return;
    ++pulses;
    sr1 = static_cast<uint8_t>((sr1 | 1U) & ~2U);
    pulse_end = now + 20U;
    if (opcode == 0x02U) {
        for (size_t i = 0; i < sent.size(); ++i)
            memory.at(address + i) &= sent[i];
    } else {
        const uint32_t begin = address & ~4095U;
        std::fill(memory.begin() + begin, memory.begin() + begin + 4096, 0xffU);
    }
}
uint8_t received_byte() {
    if (opcode == 0x9fU) {
        constexpr std::array<uint8_t, 3> id{0xefU, 0x40U, 0x18U};
        return id.at(produced);
    }
    if (opcode == 0x05U)
        return sr1;
    if (opcode == 0x35U)
        return sr2;
    if (opcode == 0x15U)
        return sr3;
    return opcode == 0x03U || opcode == 0x6bU ? memory.at(address + produced)
                                              : 0xffU;
}
void advance() {
    ++now;
    dma_flags_clear();
    dma_advance();
    if (pulse_end != 0U && now >= pulse_end && !wedge_pulse) {
        pulse_end = 0U;
        sr1 &= static_cast<uint8_t>(~1U);
    }
    if (abort_end != 0U && now >= abort_end && !refuse_abort) {
        if (active && tx)
            finish_pulse();
        abort_end = 0U;
        wire_end = 0U;
        active = false;
        fifo.clear();
        QUADSPI->CR &= ~static_cast<uint32_t>(QUADSPI_CR_ABORT);
        QUADSPI->SR &= ~static_cast<uint32_t>(QUADSPI_SR_BUSY);
        QUADSPI->SR |= QUADSPI_SR_TCF;
    }
    if (active && expected != 0U && wire_end == 0U && !wedge_wire) {
        if (tx && !fifo.empty()) {
            sent.push_back(fifo.front());
            fifo.pop_front();
            if (sent.size() == expected)
                wire_end = now + 1U;
        } else if (!tx && produced < expected && fifo.size() < 32U) {
            fifo.push_back(received_byte());
            if (++produced == expected)
                wire_end = now + 1U;
        }
    }
    if (wire_end != 0U && now >= wire_end && !wedge_wire) {
        wire_end = 0U;
        active = false;
        if (tx || fifo.empty())
            QUADSPI->SR &= ~static_cast<uint32_t>(QUADSPI_SR_BUSY);
        QUADSPI->SR |= QUADSPI_SR_TCF;
        if (opcode == 0x06U)
            sr1 |= 2U;
        if (opcode == 0x04U)
            sr1 &= static_cast<uint8_t>(~2U);
        if (tx)
            finish_pulse();
    }
    fifo_flags();
}
void begin_transfer(bool receive) {
    opcode = static_cast<uint8_t>(QUADSPI->CCR & QUADSPI_CCR_INSTRUCTION);
    address = QUADSPI->AR;
    const bool data = (QUADSPI->CCR & QUADSPI_CCR_DMODE) != 0U;
    expected = data ? QUADSPI->DLR + 1U : 0U;
    tx = !receive;
    active = true;
    sent.clear();
    fifo.clear();
    produced = 0U;
    instructions.push_back(opcode);
    if (hardware != nullptr)
        hardware->Instruction(opcode, address, expected);
    QUADSPI->SR = QUADSPI_SR_BUSY;
    if (expected == 0U)
        wire_end = now + 2U;
    fifo_flags();
}
void dma_advance() {
    if ((DMA2_Stream7->CR & DMA_SxCR_EN) == 0U) {
        dma_armed = false;
        return;
    }
    if ((QUADSPI->CR & QUADSPI_CR_DMAEN) == 0U)
        return;
    if (!dma_armed) {
        dma_progress = 0U;
        dma_armed = true;
    }
    if (!active)
        begin_transfer(false);
    if (fifo.size() < 32U && DMA2_Stream7->NDTR != 0U) {
        const auto* input = reinterpret_cast<const uint8_t*>(
            static_cast<uintptr_t>(DMA2_Stream7->M0AR));
        fifo.push_back(input[dma_progress++]);
        --DMA2_Stream7->NDTR;
        if (DMA2_Stream7->NDTR == 0U) {
            DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
            DMA2->HISR |= DMA_HISR_TCIF7;
            const unsigned irq = static_cast<unsigned>(DMA2_Stream7_IRQn);
            NVIC->ISPR[irq / 32U] |= 1U << (irq % 32U);
        }
    }
}
class QSPI : public testing::Test {
  protected:
    std::array<void*, 2> mapping{MAP_FAILED, MAP_FAILED};
    testing::NiceMock<Hardware> io;
#if NEXUS_QSPI_CPU_MAP
    nx_stm32_qspi_mapped_t mapped{};
    nx_stm32_qspi_t& owner = mapped.controller;
#else
    nx_stm32_qspi_t owner{};
#endif
    nx_stm32_qspi_config_t config{};
    nx_flash_port_t flash{&nx_stm32_qspi_flash_ops, &owner};
    nx_flash_async_port_t async{&nx_stm32_qspi_async_ops, &owner};
    std::array<uint16_t, 4> snapshots{};
    std::array<nx_stm32_platform_pin_config_t, 4> pins{};
    void SetUp() override {
        const std::array<uintptr_t, 2> addresses{0x40000000U, 0x60000000U};
        const std::array<size_t, 2> sizes{0x80000U, 0x1000U};
        for (size_t i = 0; i < mapping.size(); ++i) {
            mapping[i] =
                mmap(reinterpret_cast<void*>(addresses[i]), sizes[i],
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(mapping[i], reinterpret_cast<void*>(addresses[i]));
        }
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        hardware = &io;
        now = wire_end = pulse_end = abort_end = 0U;
        pulses = expected = address = produced = 0U;
        opcode = sr1 = sr2 = sr3 = 0U;
        wedge_wire = wedge_pulse = refuse_abort = refuse_gate_off = false;
        refuse_gate_on = refuse_af = preserve_foreign = refuse_dlr = false;
        refuse_program_terminal_clear = false;
        foreign_writes = 0U;
        legacy_callbacks = 0U;
        isr = masked = tx = active = false;
        privileged = true;
        dma_armed = false;
        dma_progress = 0U;
        refuse_dma_stop = false;
        refuse_dma_descriptor = false;
        fifo.clear();
        sent.clear();
        instructions.clear();
        memory.fill(0xffU);
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->PLLCFGR = 0x24003010U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOFEN;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = FLASH;
        g_nx_stm32_system.power = PWR;
        g_nx_stm32_system.timer = TIM2;
        g_nx_stm32_system.core_hz = 16000000U;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.started = true;
        pins = {{{GPIOB, &snapshots[0], 1U, 6U},
                 {GPIOF, &snapshots[1], 5U, 10U},
                 {GPIOF, &snapshots[2], 5U, 8U},
                 {GPIOF, &snapshots[3], 5U, 9U}}};
        snapshots.fill(1U);
        config = {
            pins.data(),
            pins.size(),
            &g_nx_stm32_system,
            16000000U,
            1000U,
            1000U,
            1000U,
            1U,
            2U,
            nullptr
#if NEXUS_QSPI_CPU_MAP
            ,
            nullptr
#endif
        };
    }
    void TearDown() override {
        hardware = nullptr;
        const std::array<size_t, 2> sizes{0x80000U, 0x1000U};
        for (size_t i = 0; i < mapping.size(); ++i)
            if (mapping[i] != MAP_FAILED)
                munmap(mapping[i], sizes[i]);
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    }
    void Service(nx_flash_async_request_t& request, unsigned limit = 10000U) {
        for (unsigned i = 0;
             i < limit && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i)
            (void)nx_flash_async_service(&async);
    }
};
#if !NEXUS_QSPI_QUAD_READ && !NEXUS_QSPI_CPU_MAP
TEST_F(QSPI, ExactJedecAndUniformGeometryNeverGrantCpuMapping) {
    Open();
    const auto* geometry = nx_flash_port_geometry(&flash);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(0U, geometry->base_address);
    EXPECT_EQ(0x1000000U, geometry->size);
    EXPECT_EQ(1U, geometry->program_unit);
    uint32_t sectors = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_flash_geometry_sector_count(geometry, &sectors));
    EXPECT_EQ(4096U, sectors);
    EXPECT_EQ(1U, geometry->erase_span_count);
    EXPECT_NE(instructions.end(),
              std::find(instructions.begin(), instructions.end(), 0x9fU));
}
TEST_F(QSPI, SuspendedForeignPulseCannotBecomePreparedOrdinaryStorage) {
    sr2 = 0x80U;
    EXPECT_CALL(io, Instruction(0x9fU, 0U, 3U)).Times(1);
    EXPECT_CALL(io, Instruction(0x35U, 0U, 1U)).Times(1);
    EXPECT_CALL(io, Instruction(0x7aU, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_FALSE(owner.prepared);
    EXPECT_EQ(0x80U, sr2);
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
}

TEST_F(QSPI, ReadUsesRealOfficialReceiveAndNeverRetainsCallerOutput) {
    Open();
    std::array<uint8_t, 17> output{};
    for (size_t i = 0; i < output.size(); ++i)
        memory[i + 7] = static_cast<uint8_t>(i);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_read(&flash, 7U, output.data(), output.size()));
    for (size_t i = 0; i < output.size(); ++i)
        EXPECT_EQ(i, output[i]);
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
    EXPECT_EQ(0U, QUADSPI->CR & (QUADSPI_CR_DMAEN | QUADSPI_CR_FTIE |
                                 QUADSPI_CR_TCIE | QUADSPI_CR_TEIE));
}
TEST_F(QSPI, ProgramSplitsPagesAndCountsOnlyPhysicallyReadyPrefix) {
    Open();
    const std::array<uint8_t, 20> input{0x12U, 0x34U, 0x56U};
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 250U,
                                     input.data(), input.size(), 2000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(input.size(), request.base.transferred);
    EXPECT_EQ(2U, pulses);
    EXPECT_EQ(0U, sr1 & 1U);
    EXPECT_EQ(0, std::memcmp(input.data(), memory.data() + 250U, input.size()));
    EXPECT_EQ(nullptr, owner.request);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
}
TEST_F(QSPI,
       CancelledPulseRetainsBorrowUntilActualWipExitAndNeverStartsNextPage) {
    Open();
    std::array<uint8_t, 20> input{};
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 250U,
                                     input.data(), input.size(), 2000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    wedge_pulse = true;
    for (unsigned i = 0U; i < 200U && pulses == 0U; ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_EQ(1U, pulses);
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_cancel(&async, &request));
    Service(request, 20U);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    EXPECT_NE(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    wedge_pulse = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(6U, request.base.transferred);
    EXPECT_EQ(1U, pulses);
}
TEST_F(QSPI, InvalidRangesAndProtectedPartCannotIssueAProgramPulse) {
    Open();
    std::array<uint8_t, 8> input{};
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_prepare(
                              &request, NX_FLASH_ASYNC_PROGRAM, 0xfffffcU,
                              input.data(), input.size(), 2000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_async_submit(&async, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, pulses);
    sr2 = 0x40U;
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     input.data(), input.size(), 2000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, request.base.result);
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(0x40U, sr2);
}
TEST_F(QSPI, SharedClockAndSystemEpochCannotBeAdoptedForANewTransfer) {
    Open();
    ++g_nx_stm32_system.time_epoch;
    uint8_t output = 0U;
    const size_t before = instructions.size();
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(before, instructions.size());
    EXPECT_NE(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
TEST_F(QSPI, ExactErasePreservesItsNeighbourAndStopNeverResetsAPeer) {
    RCC->AHB3ENR = RCC_AHB3ENR_FMCEN;
    Open();
    memory.fill(0x55U);
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_prepare(&request, NX_FLASH_ASYNC_ERASE,
                                                 0U, nullptr, 4096U, 20000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(4096U, request.base.transferred);
    for (size_t i = 0U; i < memory.size(); ++i)
        EXPECT_EQ(i < 4096U ? 0xffU : 0x55U, memory[i]);
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(RCC_AHB3ENR_FMCEN, RCC->AHB3ENR);
}
TEST_F(QSPI, TimedOutReadEndsCpuAccessButRetainsUnfinishedControllerCleanup) {
    Open();
    wedge_wire = true;
    refuse_abort = true;
    uint8_t output = 0U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    EXPECT_NE(NX_SUCCESS, nx_flash_port_recover(&flash));
    wedge_wire = false;
    refuse_abort = false;
    nx_result_t recovered = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 30U && recovered != NX_SUCCESS; ++i)
        recovered = nx_flash_port_recover(&flash);
    EXPECT_EQ(NX_SUCCESS, recovered);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
}
TEST_F(QSPI, AllErasedInputNeverIssuesAPhysicalProgramPulse) {
    Open();
    std::array<uint8_t, 64> input;
    input.fill(0xffU);
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     input.data(), input.size(), 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(input.size(), request.base.transferred);
    EXPECT_EQ(0U, pulses);
}
TEST_F(QSPI, BackpressuredPartialPageAbortStillWaitsForTheActualFlashPulse) {
    Open();
    std::array<uint8_t, 64> input;
    input.fill(0xa5U);
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     input.data(), input.size(), 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    for (unsigned i = 0U; i < 200U && sent.empty(); ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_FALSE(sent.empty());
    ASSERT_LT(sent.size(), input.size());
    ASSERT_EQ(0U, pulses);
    wedge_pulse = true;
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_cancel(&async, &request));
    Service(request, 20U);
    EXPECT_EQ(1U, pulses);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    wedge_pulse = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
    EXPECT_EQ(1U, pulses);
}
TEST_F(QSPI, PinReadbackFailureNeverStartsAFrameOrPublishesPrepared) {
    refuse_af = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_FALSE(owner.prepared);
    EXPECT_TRUE(instructions.empty());
    EXPECT_EQ(&config, owner.config);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
}
TEST_F(QSPI, ForeignIdleDiscoveryNeverInvokesUnboundHalOrRewritesItsRegisters) {
    QUADSPI->DCR = 1U;
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    preserve_foreign = true;
    EXPECT_EXIT(
        {
            const bool clean = nx_stm32_qspi_stop(&owner) == NX_SUCCESS &&
                               foreign_writes == 0U && QUADSPI->DCR == 1U &&
                               owner.config == nullptr &&
                               (RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN) == 0U;
            std::_Exit(clean ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(QSPI, RefusedClockAcquisitionNeverReadsAnUnclockedControllerOnCleanup) {
    refuse_gate_on = true;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_EXIT(
        {
            const bool clean = nx_stm32_qspi_stop(&owner) == NX_SUCCESS &&
                               owner.config == nullptr &&
                               (RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN) == 0U;
            std::_Exit(clean ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(QSPI, RecoverySuccessCannotPretendAnAsyncBorrowHasAlreadySettled) {
    Open();
    const uint8_t input = 0x12U;
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     &input, 1U, 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    wedge_pulse = true;
    for (unsigned i = 0U; i < 200U && pulses == 0U; ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_EQ(1U, pulses);
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_cancel(&async, &request));
    for (unsigned i = 0U; i < 10U; ++i) {
        EXPECT_NE(NX_SUCCESS, nx_flash_port_recover(&flash));
        EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(&request, owner.request);
    }
}
TEST_F(QSPI, RefusedDlrCannotExtendTheBorrowedDestinationOrStartAReadFrame) {
    Open();
    QUADSPI->DLR = 31U;
    refuse_dlr = true;
    instructions.clear();
    std::array<uint8_t, 32> guarded;
    guarded.fill(0xa5U);
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_read(&flash, 0U, guarded.data(), 1U));
    for (const auto value : guarded)
        EXPECT_EQ(0xa5U, value);
    EXPECT_TRUE(instructions.empty());
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
}
TEST_F(QSPI, LostPeripheralGateCannotMasqueradeAsAnIdleAccessibleController) {
    Open();
    RCC->AHB3ENR &= ~static_cast<uint32_t>(RCC_AHB3ENR_QSPIEN);
    uint8_t output = 0xa5U;
    instructions.clear();
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(0xa5U, output);
    EXPECT_TRUE(instructions.empty());
    EXPECT_EQ(&config, owner.config);
}
TEST_F(QSPI, BusAndFlashFacesShareTheSameColdLifecycleWithoutAForwardingLayer) {
    nx_external_bus_port_t bus{&nx_stm32_qspi_bus_ops, &owner};
    ASSERT_EQ(NX_EXTERNAL_BUS_STOPPED, nx_external_bus_port_state(&bus));
    Open();
    ASSERT_EQ(NX_EXTERNAL_BUS_PREPARED, nx_external_bus_port_state(&bus));
    EXPECT_NE(nullptr, nx_flash_port_geometry(&flash));
    EXPECT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(NX_EXTERNAL_BUS_STOPPED, nx_external_bus_port_state(&bus));
    EXPECT_EQ(nullptr, nx_flash_port_geometry(&flash));
}
TEST_F(QSPI, StaticAdmissionNeverRequiresAStartedSystemOrGpioClock) {
    g_nx_stm32_system.started = false;
    RCC->AHB1ENR = 0U;
    const auto before = owner;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_admit(&owner, &config));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_qspi_start(&owner, &config));
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_TRUE(instructions.empty());
}
TEST_F(QSPI, CStartAdmitsTheExactBudgetAndRejectsFiniteOverflowWithoutEffects) {
    config.prepare_budget_us = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_admit(&owner, &config));
    config.prepare_budget_us = 1000U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_start(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    instructions.clear();
    now = NX_DEADLINE_NEVER - 5U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32_qspi_start(&owner, &config));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_TRUE(instructions.empty());
}
TEST_F(QSPI, ARefusedTerminalClearRetainsAndLaterCompletesTheActualDrain) {
    Open();
    const uint8_t input = 0x12U;
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     &input, 1U, 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    refuse_program_terminal_clear = true;
    for (unsigned i = 0U; i < 200U && pulses == 0U; ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_EQ(1U, pulses);
    Service(request, 10U);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    refuse_program_terminal_clear = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(1U, request.base.transferred);
    EXPECT_EQ(1U, pulses);
}
TEST_F(QSPI, AnAsyncCommandReadbackFailurePreservesItsIoCauseBeforeDeadline) {
    Open();
    QUADSPI->DLR = 31U;
    refuse_dlr = true;
    const uint8_t input = 0x12U;
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     &input, 1U, 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    (void)nx_flash_async_service(&async);
    refuse_dlr = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_LT(now, 5000U);
    EXPECT_EQ(0U, pulses);
}
TEST_F(QSPI, PerHandleCallbacksCannotEscapeIntoLegacyGlobalHardwareHooks) {
    Open();
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_EQ(0U, legacy_callbacks);
}
TEST_F(QSPI, AsyncStopSealsAndDrainsWithoutClosingThePhysicalBank) {
    Open();
    nx_external_bus_port_t bus{&nx_stm32_qspi_bus_ops, &owner};
    const uint8_t input = 0x12U;
    nx_flash_async_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     &input, 1U, 5000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &request));
    wedge_pulse = true;
    for (unsigned i = 0U; i < 200U && pulses == 0U; ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_EQ(1U, pulses);
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_async_stop(&async));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    wedge_pulse = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(NX_SUCCESS, nx_flash_async_stop(&async));
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    EXPECT_EQ(NX_EXTERNAL_BUS_PREPARED, nx_external_bus_port_state(&bus));
    uint8_t output = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(input, output);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 1U,
                                     &input, 1U, 5000U));
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_async_submit(&async, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
#endif

/** \brief Optional program DMA keeps all read/status transports on PIO. */
class QSPIProgramDma : public QSPI {
  protected:
    void* payload = MAP_FAILED;
    nx_stm32_qspi_dma_t engine{};
    nx_flash_async_request_t request{};
    nx_stm32_qspi_dma_config_t altered_dma{};
    const nx_dma_memory_region_t region{
        0x20000000U, 0x10000U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    const nx_stm32_qspi_dma_config_t dma{&engine, &region, 1U};
    void SetUp() override {
        QSPI::SetUp();
        payload =
            mmap(reinterpret_cast<void*>(0x20000000U), 0x10000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(0x20000000U), payload);
        config.dma = &dma;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        DMA2_Stream7->FCR = 0x21U;
        std::memset(payload, 0xa5, 0x10000U);
    }
    void TearDown() override {
        refuse_dma_stop = refuse_dma_descriptor = false;
        wedge_wire = wedge_pulse = refuse_abort = false;
        if (owner.config != nullptr) {
            if (owner.request != nullptr) {
                (void)nx_flash_async_cancel(&async, owner.request);
                Service(request);
            }
            nx_result_t result = NX_ERROR_BUSY;
            for (unsigned i = 0U; i < 1000U && result != NX_SUCCESS; ++i)
                result = nx_stm32_qspi_stop(&owner);
            if (result != NX_SUCCESS || owner.request != nullptr ||
                (DMA2_Stream7->CR & DMA_SxCR_EN) != 0U) {
                ADD_FAILURE() << "Cannot reclaim a retained DMA test fixture";
                std::abort();
            }
        }
        if (payload != MAP_FAILED) {
            EXPECT_EQ(0, munmap(payload, 0x10000U));
        }
        QSPI::TearDown();
    }
    void Submit(nx_flash_async_request_t& transfer, size_t bytes = 64U) {
        nx_request_initialize(&transfer.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_async_prepare(&transfer, NX_FLASH_ASYNC_PROGRAM, 0U,
                                         payload, bytes, 20000U));
        ASSERT_EQ(NX_SUCCESS, nx_flash_async_submit(&async, &transfer));
    }
    void WaitForDma(nx_flash_async_request_t& transfer) {
        for (unsigned i = 0U;
             i < 1000U && (DMA2_Stream7->CR & DMA_SxCR_EN) == 0U &&
             nx_request_state(&transfer.base) != NX_REQUEST_SETTLED;
             ++i)
            (void)nx_flash_async_service(&async);
        ASSERT_NE(0U, DMA2_Stream7->CR & DMA_SxCR_EN);
        EXPECT_EQ(DMA_CHANNEL_3, DMA2_Stream7->CR & DMA_SxCR_CHSEL);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(payload), DMA2_Stream7->M0AR);
    }
};
#if !NEXUS_QSPI_QUAD_READ && !NEXUS_QSPI_CPU_MAP
TEST_F(QSPIProgramDma, MemoryCompletionPrecedesWirePulseAndSettled) {
    Open();
    Submit(request);
    WaitForDma(request);
    EXPECT_NE(0U, QUADSPI->CR & QUADSPI_CR_DMAEN);
    for (unsigned i = 0U; i < 200U && DMA2_Stream7->NDTR != 0U; ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_EQ(0U, DMA2_Stream7->NDTR);
    EXPECT_NE(0U, QUADSPI->SR & QUADSPI_SR_BUSY);
    EXPECT_EQ(&request, owner.request);
    EXPECT_NE(nullptr, owner.transport.pTxBuffPtr);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    wedge_pulse = true;
    for (unsigned i = 0U; i < 50U; ++i)
        (void)nx_flash_async_service(&async);
    EXPECT_EQ(1U, pulses);
    EXPECT_NE(0U, sr1 & 1U);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    wedge_pulse = false;
    Service(request);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(64U, request.base.transferred);
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(0, std::memcmp(memory.data(), payload, 64U));
    EXPECT_EQ(0U, DMA2_Stream7->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(0U, DMA2_Stream7->PAR);
    EXPECT_EQ(nullptr, engine.transport.XferCpltCallback);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
    EXPECT_EQ(0U, sr1 & 3U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_EQ(0U, DMA2_Stream7->CR);
    EXPECT_EQ(0x21U, DMA2_Stream7->FCR);
}
TEST_F(QSPIProgramDma, CpuOnlyCcmRejectsBeforeAcceptanceAndDmaEffects) {
    Open();
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_async_prepare(&request, NX_FLASH_ASYNC_PROGRAM, 0U,
                                     reinterpret_cast<void*>(0x10000000U), 64U,
                                     20000U));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_flash_async_submit(&async, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, owner.request);
    EXPECT_EQ(0U, DMA2_Stream7->NDTR);
    EXPECT_EQ(0U, pulses);
}
TEST_F(QSPIProgramDma, MutableOwnerCannotMasqueradeAsImmutableDmaDomains) {
    altered_dma = dma;
    std::memcpy(&owner.transport.Init, &region, sizeof(region));
    altered_dma.regions =
        reinterpret_cast<const nx_dma_memory_region_t*>(&owner.transport.Init);
    config.dma = &altered_dma;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
TEST_F(QSPIProgramDma, ForeignDmaVectorRejectsBeforeControllerOwnership) {
    const unsigned bank = static_cast<unsigned>(DMA2_Stream7_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(DMA2_Stream7_IRQn) % 32U);
    NVIC->ISPR[bank] |= bit;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    EXPECT_NE(0U, NVIC->ISPR[bank] & bit);
}
TEST_F(QSPIProgramDma, CancellationJoinsEngineThenPhysicalFlashPulse) {
    Open();
    Submit(request);
    WaitForDma(request);
    for (unsigned i = 0U; i < 10U && sent.empty(); ++i)
        (void)nx_flash_async_service(&async);
    ASSERT_FALSE(sent.empty());
    wedge_pulse = true;
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_cancel(&async, &request));
    for (unsigned i = 0U; i < 30U; ++i)
        (void)nx_flash_async_service(&async);
    EXPECT_EQ(0U, DMA2_Stream7->CR & DMA_SxCR_EN);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    wedge_pulse = false;
    Service(request);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
}
TEST_F(QSPIProgramDma, RefusedEngineDisableQuarantinesTheOriginalBorrow) {
    Open();
    Submit(request);
    WaitForDma(request);
    refuse_dma_stop = true;
    ASSERT_EQ(NX_SUCCESS, nx_flash_async_cancel(&async, &request));
    for (unsigned i = 0U; i < 1100U; ++i)
        (void)nx_flash_async_service(&async);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, owner.request);
    EXPECT_NE(nullptr, owner.transport.pTxBuffPtr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(payload), DMA2_Stream7->M0AR);
    EXPECT_NE(0U, DMA2_Stream7->CR & DMA_SxCR_EN);
    refuse_dma_stop = false;
    Service(request);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, engine.transport.XferCpltCallback);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
}
TEST_F(QSPIProgramDma, DmaErrorDrainsWithoutReplayingTheProgramFrame) {
    Open();
    Submit(request);
    WaitForDma(request);
    dma_flags_clear();
    DMA2->HISR |= DMA_HISR_TEIF7;
    Service(request);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_LE(pulses, 1U);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
}
TEST_F(QSPIProgramDma, RefusedDescriptorReadbackNeverReportsProgramSuccess) {
    Open();
    refuse_dma_descriptor = true;
    Submit(request);
    Service(request);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
}
#elif !NEXUS_QSPI_CPU_MAP
/** \brief One exact six-pad reference; fitted ordering suffix remains external.
 */
class QSPIQuadRead : public QSPIProgramDma {
  protected:
    std::array<uint16_t, 2> quad_snapshots{};
    std::array<nx_stm32_platform_pin_config_t, 6> quad_pins{};
    void SetUp() override {
        QSPIProgramDma::SetUp();
        config.dma = nullptr;
        for (size_t i = 0U; i < pins.size(); ++i)
            quad_pins[i] = pins[i];
        quad_pins[4] = {GPIOF, &quad_snapshots[0], 5U, 7U};
        quad_pins[5] = {GPIOF, &quad_snapshots[1], 5U, 6U};
        config.pins = quad_pins.data();
        config.pin_count = quad_pins.size();
        sr2 = 2U;
    }
};
TEST_F(QSPIQuadRead, ExactSixPadsRead114WithEightDummyCycles) {
    Open();
    EXPECT_EQ(9U, (GPIOF->AFR[0] >> 24U) & 15U);
    EXPECT_EQ(9U, (GPIOF->AFR[0] >> 28U) & 15U);
    std::array<uint8_t, 29> output{};
    for (size_t i = 0U; i < output.size(); ++i)
        memory[i + 17U] = static_cast<uint8_t>(i + 5U);
    EXPECT_CALL(io, Instruction(0x6bU, 17U, output.size()))
        .WillOnce([](uint8_t, uint32_t, uint32_t) {
            EXPECT_EQ(QSPI_INSTRUCTION_1_LINE,
                      QUADSPI->CCR & QUADSPI_CCR_IMODE);
            EXPECT_EQ(QSPI_ADDRESS_1_LINE, QUADSPI->CCR & QUADSPI_CCR_ADMODE);
            EXPECT_EQ(QSPI_DATA_4_LINES, QUADSPI->CCR & QUADSPI_CCR_DMODE);
            EXPECT_EQ(8U, (QUADSPI->CCR & QUADSPI_CCR_DCYC) >>
                              QUADSPI_CCR_DCYC_Pos);
            EXPECT_EQ(0U, QUADSPI->CCR & QUADSPI_CCR_DDRM);
        });
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_read(&flash, 17U, output.data(), output.size()));
    EXPECT_EQ(0,
              std::memcmp(memory.data() + 17U, output.data(), output.size()));
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
    EXPECT_EQ(0U, pulses);
}
TEST_F(QSPIQuadRead, QeZeroIsRejectedWithoutChangingStatusOrProtection) {
    sr2 = 0U;
    EXPECT_CALL(io, Instruction(0x9fU, 0U, 3U)).Times(1);
    EXPECT_CALL(io, Instruction(0x35U, 0U, 1U)).Times(1);
    EXPECT_CALL(io, Instruction(0x01U, testing::_, testing::_)).Times(0);
    EXPECT_CALL(io, Instruction(0x31U, testing::_, testing::_)).Times(0);
    EXPECT_CALL(io, Instruction(0x6bU, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_FALSE(owner.prepared);
    EXPECT_EQ(0U, sr2);
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
}
TEST_F(QSPIQuadRead, MissingQuadPadsRejectsBeforeAnyClockOrInstruction) {
    config.pin_count = 4U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_TRUE(instructions.empty());
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
TEST_F(QSPIQuadRead, SuspendedForeignPulseCannotBecomePreparedQuadStorage) {
    sr2 = 0x82U;
    EXPECT_CALL(io, Instruction(0x9fU, 0U, 3U)).Times(1);
    EXPECT_CALL(io, Instruction(0x35U, 0U, 1U)).Times(1);
    EXPECT_CALL(io, Instruction(0x7aU, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_qspi_prepare(&owner, &config, 1000U));
    EXPECT_FALSE(owner.prepared);
    EXPECT_EQ(0x82U, sr2);
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
}
TEST_F(QSPIQuadRead, RefusedDlrNeverExtendsQuadDestination) {
    Open();
    std::array<uint8_t, 17> output;
    output.fill(0xa5U);
    refuse_dlr = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_read(&flash, 13U, output.data(), output.size()));
    for (uint8_t value : output)
        EXPECT_EQ(0xa5U, value);
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
    EXPECT_EQ(nullptr, owner.transport.pTxBuffPtr);
}
TEST_F(QSPIQuadRead, StalledReadTimeoutEndsOutputAccessBeforeReturn) {
    Open();
    wedge_wire = true;
    refuse_abort = true;
    uint8_t output = 0xa5U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(nullptr, owner.transport.pRxBuffPtr);
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(NX_SUCCESS, nx_flash_port_recover(&flash));
    wedge_wire = false;
    refuse_abort = false;
}
TEST_F(QSPIQuadRead, ProgramDmaRemainsSingleLineWithOneSharedOwner) {
    config.dma = &dma;
    Open();
    Submit(request);
    WaitForDma(request);
    EXPECT_EQ(0x02U, QUADSPI->CCR & QUADSPI_CCR_INSTRUCTION);
    EXPECT_EQ(QSPI_DATA_1_LINE, QUADSPI->CCR & QUADSPI_CCR_DMODE);
    EXPECT_EQ(0U, QUADSPI->CCR & QUADSPI_CCR_DCYC);
    Service(request);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(1U, pulses);
    std::array<uint8_t, 64> output{};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_read(&flash, 0U, output.data(), output.size()));
    EXPECT_EQ(0, std::memcmp(output.data(), payload, output.size()));
    EXPECT_NE(instructions.end(),
              std::find(instructions.begin(), instructions.end(), 0x6bU));
}
#endif

#if NEXUS_QSPI_CPU_MAP
/** \brief One private transport, independent portable owner and exact slots. */
class QSPIMapped : public QSPI {
  protected:
    std::array<uint16_t, 6> map_saved{};
    std::array<nx_stm32_platform_pin_config_t, 6> map_pins{};
    nx_mapped_storage_owner_t map_owner{};
    nx_mapped_storage_slot_t map_slots[2]{};
    nx_stm32_qspi_mapped_config_t map_config{&map_owner, map_slots, 2U};
    nx_mapped_storage_port_t port{&nx_stm32_qspi_mapped_ops, &mapped};
    nx_mapped_storage_loan_t loans[2]{};
    nx_mapped_storage_request_t request{};
    nx_mapped_storage_ticket_t ticket{};
    std::array<uint8_t, 8> payload{0x12U, 0x34U};
    void SetUp() override {
        QSPI::SetUp();
        sr2 = 2U;
        refuse_map_command = false;
        map_barriers = 0U;
        map_saved.fill(1U);
        map_pins = {{{GPIOB, &map_saved[0], 1U, 6U},
                     {GPIOF, &map_saved[1], 5U, 10U},
                     {GPIOF, &map_saved[2], 5U, 8U},
                     {GPIOF, &map_saved[3], 5U, 9U},
                     {GPIOF, &map_saved[4], 5U, 7U},
                     {GPIOF, &map_saved[5], 5U, 6U}}};
        config.pins = map_pins.data();
        config.pin_count = map_pins.size();
        config.mapping = &map_config;
        nx_request_initialize(&request.base);
    }
    nx_result_t Release(unsigned slot) {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < 500U && result == NX_ERROR_BUSY; ++i)
            result = nx_mapped_storage_release(&port, &loans[slot]);
        if (result == NX_SUCCESS)
            loans[slot] = {};
        return result;
    }
    void TearDown() override {
        privileged = true;
        isr = masked = refuse_abort = wedge_pulse = wedge_wire = false;
        refuse_map_command = false;
        refuse_program_terminal_clear = false;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
        for (unsigned i = 0U; i < 2U; ++i)
            if (loans[i].instance != nullptr && Release(i) != NX_SUCCESS)
                std::abort();
        nx_result_t stopped = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < 10000U && stopped == NX_ERROR_BUSY; ++i)
            stopped = nx_stm32_qspi_stop(&owner);
        EXPECT_EQ(NX_SUCCESS, stopped);
        if (stopped != NX_SUCCESS)
            std::abort();
        QSPI::TearDown();
    }
};
TEST_F(QSPIMapped, CpuReadUsesExactQuadFrameAndLastReleaseWithdrawsTheMode) {
    Open();
    QUADSPI->AR = 3U;
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 4U,
                                        8U, &loans[0]));
    EXPECT_EQ(reinterpret_cast<void*>(0x90000004U), loans[0].data);
    EXPECT_EQ(0U, QUADSPI->AR);
    EXPECT_EQ(QUADSPI_CCR_FMODE, QUADSPI->CCR & QUADSPI_CCR_FMODE);
    EXPECT_EQ(0x6bU, QUADSPI->CCR & QUADSPI_CCR_INSTRUCTION);
    EXPECT_EQ(QSPI_DATA_4_LINES, QUADSPI->CCR & QUADSPI_CCR_DMODE);
    EXPECT_EQ(8U, (QUADSPI->CCR & QUADSPI_CCR_DCYC) >> QUADSPI_CCR_DCYC_Pos);
    EXPECT_EQ(0U, QUADSPI->CR &
                      (QUADSPI_CR_TCEN | QUADSPI_CR_TOIE | QUADSPI_CR_DMAEN));
    EXPECT_GT(map_barriers, 0U);
    QUADSPI->SR |= QUADSPI_SR_BUSY;
    fifo.push_back(0x55U);
    ASSERT_EQ(NX_SUCCESS, Release(0U));
    EXPECT_EQ(0U, QUADSPI->CCR & QUADSPI_CCR_FMODE);
    EXPECT_EQ(0U, QUADSPI->SR & (QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL));
}
TEST_F(QSPIMapped, NeverAccessedApertureStillExitsFmodeWithoutHalBusyShortcut) {
    Open();
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[0]));
    ASSERT_EQ(0U, QUADSPI->SR & QUADSPI_SR_BUSY);
    ASSERT_EQ(NX_SUCCESS, Release(0U));
    EXPECT_EQ(0U, QUADSPI->CCR & QUADSPI_CCR_FMODE);
}
TEST_F(QSPIMapped, InvalidContextNeverEntersTheClockFailureCleanupFallback) {
    ASSERT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_mapping_ready(nullptr));
    auto* mmio = reinterpret_cast<nx_stm32_qspi_t*>(QUADSPI);
    ASSERT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_mapping_ready(mmio));
    for (void* context : std::array<void*, 2>{nullptr, mmio}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_qspi_mapped_ops.acquire(
                      context, NX_MAPPED_STORAGE_CPU_READ, 0U, 1U, &loans[0]));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_qspi_mapped_ops.release(context, &loans[0]));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_qspi_mapped_ops.submit(context, &request, &ticket));
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_qspi_mapped_ops.cancel(context, ticket));
        nx_stm32_qspi_mapped_ops.service(context);
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_mapped_ops.stop(context));
    }
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    EXPECT_TRUE(instructions.empty());
}
TEST_F(QSPIMapped, MissingMappingRejectsColdBeforeAnyGateOrInstruction) {
    config.mapping = nullptr;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_admit(&owner, &config));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    EXPECT_TRUE(instructions.empty());
}
TEST_F(QSPIMapped, GenericControlCannotAliasTheTransportDuringColdAdmission) {
    map_config.owner = reinterpret_cast<nx_mapped_storage_owner_t*>(&owner);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_admit(&owner, &config));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
TEST_F(QSPIMapped, WritablePortableControlNeverUsesInternalFlashStorage) {
    map_config.owner =
        reinterpret_cast<nx_mapped_storage_owner_t*>(0x08010000U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_qspi_admit(&owner, &config));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
}
TEST_F(QSPIMapped,
       UnsupportedReadersAndControllerAliasesRejectBeforeAdmission) {
    Open();
    for (auto tag : {NX_MAPPED_STORAGE_XIP, NX_MAPPED_STORAGE_DMA_READ})
        EXPECT_EQ(NX_ERROR_UNSUPPORTED,
                  nx_mapped_storage_acquire(&port, tag, 0U, 1U, &loans[0]));
    auto before = owner;
    auto* aliased = reinterpret_cast<nx_mapped_storage_loan_t*>(&owner);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, aliased));
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
    nx_request_t* base = reinterpret_cast<nx_request_t*>(&owner);
    before = owner;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_mapped_storage_submit(
                  &port, reinterpret_cast<nx_mapped_storage_request_t*>(base),
                  &ticket));
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
}
TEST_F(QSPIMapped, FailedMapOwnsCleanupWithoutPublishingAReader) {
    Open();
    refuse_map_command = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[0]));
    EXPECT_EQ(nullptr, loans[0].instance);
    EXPECT_TRUE(map_owner.map_cleanup_pending);
    EXPECT_FALSE(map_owner.accepting);
    refuse_map_command = false;
}
TEST_F(QSPIMapped, LoanExcludesWriterAndOuterSettlesAfterTheSameInnerDeadline) {
    Open();
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        8U, &loans[0]));
    const nx_time_us_t deadline = now + 4000U;
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_prepare(
                              &request, NX_MAPPED_STORAGE_PROGRAM, 0U,
                              payload.data(), payload.size(), deadline));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_mapped_storage_submit(&port, &request, &ticket));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, pulses);
    ASSERT_EQ(NX_SUCCESS, Release(0U));
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_submit(&port, &request, &ticket));
    EXPECT_EQ(deadline, mapped.inner.base.deadline);
    for (unsigned i = 0U;
         i < 10000U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++i)
        nx_mapped_storage_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(0, std::memcmp(payload.data(), memory.data(), payload.size()));
}
TEST_F(QSPIMapped, InnerDrainFailureQuarantinesOuterUntilActualRecovery) {
    Open();
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_prepare(
                              &request, NX_MAPPED_STORAGE_PROGRAM, 0U,
                              payload.data(), payload.size(), now + 4000U));
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_submit(&port, &request, &ticket));
    wedge_wire = true;
    nx_mapped_storage_service(&port);
    /* Explicit refused FCR readback is IO, rather than an ordinary BUSY join.
     */
    opcode = 0x02U;
    refuse_program_terminal_clear = true;
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_cancel(&port, ticket));
    for (unsigned i = 0U; i < 32U; ++i)
        nx_mapped_storage_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&mapped.inner.base));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    wedge_wire = false;
    refuse_program_terminal_clear = false;
    for (unsigned i = 0U;
         i < 1000U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++i)
        nx_mapped_storage_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(QSPIMapped, ClockFaultStillProgressesOwnedCleanupAndQuarantinesOuter) {
    Open();
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_prepare(
                              &request, NX_MAPPED_STORAGE_PROGRAM, 0U,
                              payload.data(), payload.size(), now + 4000U));
    ASSERT_EQ(NX_SUCCESS, nx_mapped_storage_submit(&port, &request, &ticket));
    nx_mapped_storage_service(&port);
    g_nx_stm32_system.timer_error = NX_ERROR_IO;
    for (unsigned i = 0U; i < 8U; ++i)
        nx_mapped_storage_service(&port);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    g_nx_stm32_system.timer_error = NX_SUCCESS;
    for (unsigned i = 0U;
         i < 1000U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++i)
        nx_mapped_storage_service(&port);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}
TEST_F(QSPIMapped, FailedAbortRetainsLoanAndColdStopCannotWithdrawItsGate) {
    Open();
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[0]));
    QUADSPI->SR |= QUADSPI_SR_BUSY;
    refuse_abort = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_mapped_storage_release(&port, &loans[0]));
    EXPECT_NE(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN);
    refuse_abort = false;
    EXPECT_EQ(NX_SUCCESS, Release(0U));
}
TEST_F(QSPIMapped, RestartPreservesLoanEpochAndRawFlashFacesCannotBypassOwner) {
    Open();
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[0]));
    const nx_mapped_storage_loan_t stale = loans[0];
    ASSERT_EQ(NX_SUCCESS, Release(0U));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_stop(&owner));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_qspi_prepare(&owner, &config, now + 1000U));
    ASSERT_EQ(NX_SUCCESS,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[0]));
    EXPECT_NE(stale.epoch, loans[0].epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_mapped_storage_release(&port, &stale));
    uint8_t output = 0xa5U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_read(&flash, 0U, &output, 1U));
    EXPECT_EQ(0xa5U, output);
    masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        1U, &loans[1]));
    masked = false;
}
#endif
} /* namespace */
extern "C" uint32_t nx_qspi_model_read(const volatile uint32_t* reg) {
    if (reg == &DMA2_Stream7->M0AR && refuse_dma_descriptor &&
        (DMA2_Stream7->CR & DMA_SxCR_EN) != 0U)
        return *reg ^ 4U;
    if (reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(QUADSPI) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(QUADSPI) + sizeof(*QUADSPI) &&
        (RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN) == 0U)
        ADD_FAILURE() << "Unclocked QSPI cannot establish register readback";
    return *reg;
}
extern "C" void nx_qspi_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DMA2->HIFCR) {
        DMA2->HISR &= ~value;
        *reg = 0U;
        return;
    }
    if (reg == &DMA2_Stream7->CR && refuse_dma_stop &&
        (DMA2_Stream7->CR & DMA_SxCR_EN) != 0U)
        value |= DMA_SxCR_EN;
    if (refuse_dlr && reg == &QUADSPI->DLR)
        return;
    if (preserve_foreign &&
        reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(QUADSPI) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(QUADSPI) + sizeof(*QUADSPI))
        ++foreign_writes;
    for (auto* gpio : {GPIOB, GPIOF})
        if (refuse_af && (reg == &gpio->AFR[0] || reg == &gpio->AFR[1]))
            return;
    for (auto* gpio : {GPIOB, GPIOF})
        if (reg == &gpio->BSRR) {
            gpio->ODR |= value & 0xffffU;
            gpio->ODR &= ~(value >> 16U);
        }
    if (reg == &RCC->AHB3ENR && refuse_gate_off)
        value |= RCC_AHB3ENR_QSPIEN;
    if (reg == &RCC->AHB3ENR && refuse_gate_on)
        value &= ~static_cast<uint32_t>(RCC_AHB3ENR_QSPIEN);
    if (reg == &QUADSPI->FCR) {
        if (refuse_program_terminal_clear && opcode == 0x02U)
            return;
        QUADSPI->SR &= ~value;
        *reg = value;
        return;
    }
    if (reg == &QUADSPI->CR && (value & QUADSPI_CR_ABORT) != 0U &&
        (QUADSPI->CR & QUADSPI_CR_ABORT) == 0U)
        abort_end = now + 2U;
#if NEXUS_QSPI_CPU_MAP
    if (reg == &QUADSPI->CCR && refuse_map_command &&
        (value & QUADSPI_CCR_FMODE) == QUADSPI_CCR_FMODE)
        value ^= 1U << QUADSPI_CCR_DCYC_Pos;
#endif
    *reg = value;
    if (reg == &QUADSPI->CCR && (value & QUADSPI_CCR_DMODE) == 0U &&
        (value & QUADSPI_CCR_ADMODE) == 0U &&
        (value & QUADSPI_CCR_IMODE) != 0U &&
        (QUADSPI->CR & QUADSPI_CR_EN) != 0U)
        begin_transfer(false);
    if (reg == &QUADSPI->AR) {
        const uint32_t mode = QUADSPI->CCR & QUADSPI_CCR_FMODE;
        if (mode == QUADSPI_CCR_FMODE_0)
            begin_transfer(true);
        else if ((QUADSPI->CCR & QUADSPI_CCR_ADMODE) != 0U &&
                 (QUADSPI->CCR & QUADSPI_CCR_DMODE) == 0U)
            begin_transfer(false);
    }
    fifo_flags();
}
extern "C" uint8_t nx_qspi_model_read8(volatile uint32_t* reg) {
    EXPECT_EQ(&QUADSPI->DR, reg);
    EXPECT_FALSE(fifo.empty());
    if (fifo.empty())
        return 0xffU;
    uint8_t value = fifo.front();
    fifo.pop_front();
    if (fifo.empty() && !active)
        QUADSPI->SR &= ~static_cast<uint32_t>(QUADSPI_SR_BUSY);
    fifo_flags();
    return value;
}
extern "C" void nx_qspi_model_write8(volatile uint32_t* reg, uint8_t value) {
    EXPECT_EQ(&QUADSPI->DR, reg);
    if (!active)
        begin_transfer(false);
    EXPECT_LT(sent.size() + fifo.size(), expected);
    EXPECT_LT(fifo.size(), 32U);
    fifo.push_back(value);
    fifo_flags();
}
extern "C" void nx_qspi_irq_clear(IRQn_Type irq) {
    EXPECT_TRUE(irq == QUADSPI_IRQn || irq == DMA2_Stream7_IRQn);
    NVIC->ISPR[static_cast<unsigned>(irq) / 32U] &=
        ~(1U << (static_cast<unsigned>(irq) % 32U));
    fifo_flags();
}
extern "C" uint64_t nx_stm32_system_clock_read(void* selected) {
    if (selected != &g_nx_stm32_system || !g_nx_stm32_system.started ||
        g_nx_stm32_system.timer_error != NX_SUCCESS)
        return NX_DEADLINE_NEVER;
    advance();
    return now;
}
extern "C" uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(now / 1000U);
}
extern "C" bool nx_qspi_privileged(void) {
    return privileged;
}
extern "C" bool nx_qspi_in_isr(void) {
    return isr;
}
extern "C" bool nx_qspi_masked(void) {
    return masked;
}
extern "C" void nx_qspi_dsb(void) {
#if NEXUS_QSPI_CPU_MAP
    ++map_barriers;
#endif
    dma_flags_clear();
}
extern "C" void HAL_QSPI_MspInit(QSPI_HandleTypeDef*) {
    ++legacy_callbacks;
}
extern "C" void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef*) {
    ++legacy_callbacks;
}
extern "C" void HAL_QSPI_FifoThresholdCallback(QSPI_HandleTypeDef*) {
    ++legacy_callbacks;
}
