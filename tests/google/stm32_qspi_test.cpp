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
    return opcode == 0x03U ? memory.at(address + produced) : 0xffU;
}
void advance() {
    ++now;
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
class QSPI : public testing::Test {
  protected:
    std::array<void*, 2> mapping{MAP_FAILED, MAP_FAILED};
    testing::NiceMock<Hardware> io;
    nx_stm32_qspi_t owner{};
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
        config = {pins.data(), pins.size(), &g_nx_stm32_system,
                  16000000U,   1000U,       1000U,
                  1000U,       1U,          2U};
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
} /* namespace */
extern "C" uint32_t nx_qspi_model_read(const volatile uint32_t* reg) {
    if (reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(QUADSPI) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(QUADSPI) + sizeof(*QUADSPI) &&
        (RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN) == 0U)
        ADD_FAILURE() << "Unclocked QSPI cannot establish register readback";
    return *reg;
}
extern "C" void nx_qspi_model_write(volatile uint32_t* reg, uint32_t value) {
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
    EXPECT_EQ(QUADSPI_IRQn, irq);
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
