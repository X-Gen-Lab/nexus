/**
 * \file            h743_conversion_test.cpp
 * \brief           H743 calibrated one-shot ADC and physical Flash boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_fixture.h"
#include "h743_peripheral_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include "terminal_watchdog.h"
#include <array>
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
class FlashFaultBoundary {
  public:
    MOCK_METHOD(void, Observe, ());
};
FlashFaultBoundary* flash_fault;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    EXPECT_EQ(NX_FAULT_PLATFORM, domain);
    EXPECT_EQ(NX_ERROR_IO, reason);
    EXPECT_NE(nullptr, file);
    EXPECT_NE(0U, line);
    ++nexus_test::h743_ram::cpu.flash_fault_observers;
    ADD_FAILURE() << "RAM maintenance called a Flash-resident fault observer";
    flash_fault->Observe();
}
class H743Adc : public testing::Test {
  protected:
    ADC_TypeDef registers{};
    ADC_TypeDef peer{};
    ADC_Common_TypeDef common{};
    nx_stm32h743_adc_state_t state{};
    nx_adc_port_t face{&nx_stm32h743_adc_ops, &state};
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.AHB1ENR = RCC_AHB1ENR_ADC12EN;
        registers.CR = peer.CR = ADC_CR_DEEPPWD;
        registers.CFGR = ADC_CFGR_JQDIS;
        state.registers = &registers;
        state.peer = &peer;
        state.common = &common;
        state.gpio = &g_h743_model.gpio_a;
        state.reference_mv = 3300U;
        state.supply_mv = 3300U;
        state.poll_limit = 64U;
        g_h743_model.adc = &registers;
    }
};
TEST_F(H743Adc, ColdPrepareKeepsLiveResourcesUntilSuccessfulStop) {
    const nx_stm32h743_adc_config_t config{
        &registers, &peer, &common, &g_h743_model.gpio_a, 3300U, 3300U, 64U};
    state = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_prepare(&state, &config));
    ADC_TypeDef foreign{};
    auto other = config;
    other.registers = &foreign;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_adc_prepare(&state, &other));
    EXPECT_EQ(&registers, state.registers);
    EXPECT_EQ(0U, foreign.CR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_prepare(&state, &config));
}
TEST_F(H743Adc, ColdPrepareRejectsInvalidSupplyBeforeAnyHardwareWrite) {
    const nx_stm32h743_adc_config_t invalid{
        &registers, &peer, &common, &g_h743_model.gpio_a, 3300U, 2000U, 64U};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_prepare(nullptr, &invalid));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_prepare(&state, nullptr));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_prepare(&state, &invalid));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Adc, OffsetAndLinearityCalibrationPrecedeSingleShotPublication) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    EXPECT_TRUE(g_h743_model.adc_calibrated);
    EXPECT_EQ(0U, g_h743_model.adc_illegal_linear_writes);
    EXPECT_EQ(ADC_CCR_CKMODE, common.CCR & ADC_CCR_CKMODE);
    EXPECT_EQ(1U << 15U, registers.PCSEL);
    nx_adc_info_t info{};
    EXPECT_EQ(NX_SUCCESS, nx_adc_port_info(&face, &info));
    EXPECT_EQ(16U, info.resolution_bits);
    uint16_t sample = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, &sample, 1U, 10000U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0xabcdu, sample);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    EXPECT_EQ(0U, common.CCR);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Adc, UnqualifiedAnalogSupplyRejectsBeforeAnyDomainEffects) {
    state.supply_mv = 3301U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_initialize(&state));
    state.supply_mv = 2399U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_initialize(&state));
    state.supply_mv = 3300U;
    state.reference_mv = 1999U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_initialize(&state));
    state.reference_mv = 3300U;
    state.supply_mv = 3000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_adc_initialize(&state));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Adc, ForeignPeerRejectsBeforeCommonOrGpioMutation) {
    peer.CR = ADC_CR_ADEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_adc_initialize(&state));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Adc, CalibrationTimeoutKeepsTheDomainUntilExplicitSuccessfulStop) {
    g_h743_model.hold_adc_calibration = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32h743_adc_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_adc_stop(&state));
    g_h743_model.hold_adc_calibration = false;
    nx_h743_model_adc_poll(&state);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
}
TEST_F(H743Adc, ConversionTimeoutCannotPublishAnUncertainSample) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    g_h743_model.hold_adc_conversion = true;
    uint16_t sample = 0x7777U;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&face, &sample, 1U, 10000U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x7777U, sample);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
}
TEST_F(H743Adc, OutputAliasesRejectBeforeStartingConversion) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    size_t count = 0U;
    uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_port_sample(&face, reinterpret_cast<uint16_t*>(&registers),
                                 1U, 10000U, &count));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743Adc, SuccessfulStopPermitsACompleteRecalibratedRestart) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    g_h743_model.adc_calibrated = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    EXPECT_TRUE(g_h743_model.adc_calibrated);
    EXPECT_EQ(0U, g_h743_model.adc_illegal_linear_writes);
}

TEST_F(H743Adc,
       OldCutAdcClocksEightAndSixteenMhzLeaveBoostAndReservedBitsClear) {
    g_h743_model.silicon_id = 0x10030450U;
    for (uint32_t core : {32000000U, 64000000U}) {
        g_nx_stm32h743_system.core_hz = core;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
        EXPECT_EQ(0U, registers.CR & (ADC_CR_BOOST | (3U << 6U)));
        EXPECT_TRUE(g_h743_model.adc_calibrated);
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    }
}
TEST_F(H743Adc, NewCutAdcClocksEightAndSixteenMhzSelectTheirOwnBoostRange) {
    for (uint32_t core : {32000000U, 64000000U}) {
        g_nx_stm32h743_system.core_hz = core;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
        EXPECT_EQ(core == 32000000U ? ADC_CR_BOOST_0 : ADC_CR_BOOST_1,
                  registers.CR & ADC_CR_BOOST);
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    }
}
TEST_F(H743Adc, RefusedNewCalibrationCannotReuseStaleDigitalReadyFlags) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
    ASSERT_FALSE(g_h743_model.adc_calibrated);
    g_h743_model.refuse_adc_calibration_command = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_adc_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(g_h743_model.adc_calibrated);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
}

TEST_F(H743Adc, RefusedReadyFlagClearCannotAdmitAnEnable) {
    registers.ISR = ADC_ISR_ADRDY;
    g_h743_model.ignore_write = &registers.ISR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_adc_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADEN);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
}
TEST_F(H743Adc, RefusedSampleFlagClearCannotPublishOrStartAConversion) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    registers.ISR = ADC_ISR_EOC | ADC_ISR_EOS;
    registers.DR = 0x2468U;
    g_h743_model.ignore_write = &registers.ISR;
    uint16_t value = 0x5555U;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &value, 1U, 10000U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x5555U, value);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
    EXPECT_TRUE(state.fault);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
}

class H743Flash : public testing::Test {
  protected:
    static constexpr uint32_t spare_offset = 131072U;
    std::array<uint8_t, 2097152U> memory{};
    nx_stm32h743_flash_state_t state{};
    nx_flash_port_t face{&nx_stm32h743_flash_ops, &state};
    alignas(1024) nx_stm32h743_flash_ram_workspace_t execution{};
    testing::NiceMock<nexus_test::h743_ram::RamHardwareBoundary> hardware;
    testing::NiceMock<FlashFaultBoundary> faults;
    void SetUp() override {
        nx_h743_model_reset();
        memory.fill(0xffU);
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        state.registers = &g_h743_model.flash;
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        state.registers->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
        state.memory = memory.data();
        state.supply_mv = 3300U;
        state.poll_limit = 8U;
        g_h743_model.physical_flash = &state;
        flash_fault = &faults;
        state.registers->CR1 = state.registers->CR2 = FLASH_CR_LOCK;
        nexus_test::h743_fixture::ResetExternalCpu();
        nexus_test::h743_fixture::SetIsolatedTimer(64000000U);
        ASSERT_EQ(NX_SUCCESS,
                  nexus_test::h743_fixture::Prepare(&state, &execution));
        nexus_test::h743_ram::cpu.observer = &hardware;
    }
    void TearDown() override {
        EXPECT_EQ(0U, nexus_test::h743_ram::cpu.flash_fault_observers);
        nexus_test::h743_fixture::CheckedStop(&execution);
        nexus_test::h743_fixture::StopIsolatedTimer();
        flash_fault = nullptr;
    }
};
TEST_F(H743Flash, GeometryExposesAllPhysicalSectorsAndFlashwordGranularity) {
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(&face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(2097152U, geometry->size);
    EXPECT_EQ(32U, geometry->program_unit);
    EXPECT_EQ(NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE, geometry->program_flags);
    ASSERT_EQ(16U, geometry->sector_count);
    EXPECT_EQ(0x1e0000U, geometry->sectors[15].offset);
    EXPECT_EQ(131072U, geometry->sectors[15].size);
}
TEST_F(H743Flash, Unsafe64MHzProgrammingDelayRejectsBeforeUnlockOrPulse) {
    /* RM0433 Rev9 Table17: VOS3/HSI64 needs WRHIGHFREQ1, not reset0. */
    g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
    state.registers->ACR = FLASH_ACR_LATENCY_7WS;
    std::array<uint8_t, 32U> input{};
    input.fill(0x55U);
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(0xffU, memory[spare_offset]);
    EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR1);
}
TEST_F(H743Flash, Unsafe64MHzEraseDelayRejectsBeforeUnlockOrStart) {
    g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
    state.registers->ACR = FLASH_ACR_LATENCY_7WS;
    memory[spare_offset] = 0U;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_erase(&face, spare_offset, 131072U, 10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, memory[spare_offset]);
    EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR1);
}
TEST_F(H743Flash, UnsettledVoltageCannotAuthorizeAFlashPulse) {
    g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_1 | PWR_CSR1_ACTVOSRDY;
    state.registers->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743Flash, BankTwoFlashwordDrainsAndRelocksWithoutChangingBankOne) {
    std::array<uint8_t, 32U> input{};
    input.fill(0x55U);
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 1048576U, input.data(),
                                                input.size(), 10000U));
    EXPECT_EQ(0x55U, memory[1048576U]);
    EXPECT_EQ(0xffU, memory[0]);
    EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR2 & FLASH_CR_LOCK);
    EXPECT_EQ(0U, state.registers->CR2 & FLASH_CR_PG);
    EXPECT_FALSE(state.active);
}
TEST_F(H743Flash, AllErasedInputDoesNotConsumeAnEccFlashwordPulse) {
    std::array<uint8_t, 32U> input{};
    input.fill(0xffU);
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.active);
}
TEST_F(H743Flash, NonErasedEccFlashwordRejectsBeforeControllerUnlock) {
    std::array<uint8_t, 32U> input{};
    memory[spare_offset] = 0U;
    uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743Flash, CacheAndBankSwapAndVoltageRejectBeforeAnyPulse) {
    std::array<uint8_t, 32U> input{};
    g_h743_model.core->CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(
        NX_ERROR_UNSUPPORTED,
        nx_flash_port_program(&face, spare_offset, input.data(), 32U, 10000U));
    g_h743_model.core->CCR = 0U;
    state.registers->OPTSR_CUR |= FLASH_OPTSR_SWAP_BANK_OPT;
    EXPECT_EQ(
        NX_ERROR_UNSUPPORTED,
        nx_flash_port_program(&face, spare_offset, input.data(), 32U, 10000U));
    state.registers->OPTSR_CUR &= ~(uint32_t)FLASH_OPTSR_SWAP_BANK_OPT;
    state.supply_mv = 2500U;
    EXPECT_EQ(
        NX_ERROR_UNSUPPORTED,
        nx_flash_port_program(&face, spare_offset, input.data(), 32U, 10000U));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Flash, EraseRequiresWholeExactSectorsAndSelectsThePhysicalBank) {
    memory[1048576U] = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_erase(&face, 1048576U, 1024U, 10000U));
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 1048576U, 131072U, 10000U));
    EXPECT_EQ(0xffU, memory[1048576U]);
    EXPECT_FALSE(state.active);
}
TEST_F(H743Flash, PulseErrorDoesNotLeaveProgrammingOrAnUnlockedController) {
    std::array<uint8_t, 32U> input{};
    nexus_test::h743_ram::cpu.pulse_error = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, spare_offset,
                                                 input.data(), 32U, 10000U));
    EXPECT_EQ(0U, state.registers->CR1 & FLASH_CR_PG);
    EXPECT_NE(0U, state.registers->CR1 & FLASH_CR_LOCK);
    EXPECT_FALSE(state.active);
}
TEST_F(H743Flash, DeadlineExpiryIsReportedOnlyAfterTheUnabortablePulseSettles) {
    std::array<uint8_t, 32U> input{};
    g_h743_model.flash_poll_advance = 10000U;
    EXPECT_EQ(
        NX_ERROR_TIMEOUT,
        nx_flash_port_program(&face, spare_offset, input.data(), 32U, 1000U));
    EXPECT_EQ(0U, memory[spare_offset]);
    EXPECT_FALSE(state.active);
    EXPECT_NE(0U, state.registers->CR1 & FLASH_CR_LOCK);
}
TEST_F(H743Flash, AnUnobservedEraseStartCannotBeReportedAsSuccess) {
    memory[spare_offset] = 0U;
    g_h743_model.hold_flash_start = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, spare_offset, 131072U, 10000U));
    EXPECT_EQ(0U, memory[spare_offset]);
    EXPECT_FALSE(state.active);
    EXPECT_NE(0U, state.registers->CR1 & FLASH_CR_LOCK);
}
TEST_F(H743Flash, StuckPulseRetainsTheCallerAndParksAfterTheObserverReturns) {
    auto& cpu = nexus_test::h743_ram::cpu;
    cpu.clear_after = 0U;
    g_h743_model.flash_poll_advance = 10000U;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        std::array<uint8_t, 32U> input{};
        input.fill(0x66U);
        bool recorded = false;
        EXPECT_CALL(hardware, Poll(testing::_, true, true))
            .WillRepeatedly([&](uint32_t pending, bool, bool) {
                if (!recorded && cpu.polls > state.poll_limit) {
                    recorded = true;
                    nexus_test::RecordTerminalObservation(
                        cpu.primask == 1U && state.active && execution.active &&
                        nexus_test::h743_ram::PrivateStack() &&
                        g_h743_model.core->VTOR ==
                            nexus_test::h743_ram::ram_vector_address &&
                        (pending & FLASH_SR_BSY) != 0U &&
                        (state.registers->CR1 & FLASH_CR_PG) != 0U &&
                        execution.guard.remaining_effects != 0U &&
                        g_h743_model.timer.CNT >= 10000U &&
                        cpu.flash_fault_observers == 0U && input[0] == 0x66U);
                }
            });
        (void)nx_flash_port_program(&face, spare_offset, input.data(), 32U,
                                    10000U);
    }));
}
TEST_F(H743Flash, RefusedControlRelockRetainsTheExecutionOwner) {
    auto& cpu = nexus_test::h743_ram::cpu;
    cpu.reject_lock = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        std::array<uint8_t, 32U> input{};
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_LOCK))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    cpu.primask == 1U && state.active && execution.active &&
                    nexus_test::h743_ram::PrivateStack() &&
                    g_h743_model.core->VTOR ==
                        nexus_test::h743_ram::ram_vector_address &&
                    (state.registers->CR1 & FLASH_CR_LOCK) == 0U &&
                    g_h743_model.flash_store_count == 8U &&
                    execution.guard.remaining_effects != 0U &&
                    cpu.flash_fault_observers == 0U);
            });
        (void)nx_flash_port_program(&face, spare_offset, input.data(), 32U,
                                    10000U);
    }));
}

class H743FlashTiming : public H743Flash {
  protected:
    nx_stm32h743_flash_timing_state_t timing{};
    void SetUp() override {
        H743Flash::SetUp();
        state.registers->ACR = FLASH_ACR_LATENCY_7WS;
    }
    void TearDown() override {
        EXPECT_FALSE(timing.retained);
        if (timing.retained)
            std::abort();
        H743Flash::TearDown();
    }
};
TEST_F(H743FlashTiming, Cold64MHzOwnsOnlyTheProgrammingDelayUntilStop) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0,
              state.registers->ACR);
    std::array<uint8_t, 32U> input{};
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_FALSE(timing.retained);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, state.registers->ACR);
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_erase(&face, spare_offset, 131072U, 10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743FlashTiming, Cold32MHzNeedsNoTimingMutationOrRetainedState) {
    g_nx_stm32h743_system.core_hz = 32000000U;
    g_h743_model.rcc.CR |= RCC_CR_HSIDIV_2 | RCC_CR_HSIDIVF;
    nexus_test::h743_fixture::SetIsolatedTimer(32000000U);
    const uint32_t writes = g_h743_model.writes;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    EXPECT_FALSE(timing.retained);
    EXPECT_EQ(writes, g_h743_model.writes);
    std::array<uint8_t, 32U> input{};
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, spare_offset, input.data(),
                                    input.size(), 10000U));
}
TEST_F(H743FlashTiming, BothBanksMustBeQuiescentBeforeAnyTimingWrite) {
    for (uint32_t busy : {uint32_t(FLASH_SR_BSY), uint32_t(FLASH_SR_QW),
                          uint32_t(FLASH_SR_WBNE)}) {
        for (volatile uint32_t* bank :
             {&state.registers->SR1, &state.registers->SR2}) {
            *bank = busy;
            const uint32_t writes = g_h743_model.writes;
            EXPECT_EQ(NX_ERROR_BUSY,
                      nx_stm32h743_flash_timing_initialize(&timing));
            EXPECT_FALSE(timing.retained);
            EXPECT_EQ(writes, g_h743_model.writes);
            *bank = 0U;
        }
    }
}
TEST_F(H743FlashTiming, RetainedPulseControlsRejectBeforeAcquiringAcr) {
    for (uint32_t control :
         {uint32_t(FLASH_CR_PG), uint32_t(FLASH_CR_SER), uint32_t(FLASH_CR_BER),
          uint32_t(FLASH_CR_START)}) {
        state.registers->CR2 = FLASH_CR_LOCK | control;
        const uint32_t writes = g_h743_model.writes;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_flash_timing_initialize(&timing));
        EXPECT_FALSE(timing.retained);
        EXPECT_EQ(writes, g_h743_model.writes);
    }
    /* The external foreign controller owner withdraws its test controls. */
    state.registers->CR2 = FLASH_CR_LOCK;
}
TEST_F(H743FlashTiming, ForeignTimingCannotBeAdoptedOrOverwritten) {
    for (uint32_t acr :
         {uint32_t(FLASH_ACR_LATENCY_6WS),
          uint32_t(FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0),
          uint32_t(FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_1)}) {
        state.registers->ACR = acr;
        const uint32_t writes = g_h743_model.writes;
        EXPECT_EQ(NX_ERROR_STATE,
                  nx_stm32h743_flash_timing_initialize(&timing));
        EXPECT_FALSE(timing.retained);
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(acr, state.registers->ACR);
    }
}
TEST_F(H743FlashTiming, DeclaredFrequencyCannotOverrideObservedClockState) {
    g_h743_model.rcc.CR |= RCC_CR_HSIDIV_2 | RCC_CR_HSIDIVF;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_initialize(&timing));
    g_h743_model.rcc.CR &= ~(uint32_t)(RCC_CR_HSIDIV | RCC_CR_HSIDIVF);
    g_h743_model.rcc.CFGR = RCC_CFGR_SW_PLL1 | RCC_CFGR_SWS_PLL1;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_initialize(&timing));
    g_h743_model.rcc.CFGR = 0U;
    g_h743_model.rcc.D1CFGR = RCC_D1CFGR_HPRE_DIV2;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_initialize(&timing));
    EXPECT_FALSE(timing.retained);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashTiming, UnsettledVoltageAndCacheStateNeverAcquireTiming) {
    const uint32_t original_ccr = g_h743_model.core->CCR;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_1 | PWR_CSR1_ACTVOSRDY;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_initialize(&timing));
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
    g_h743_model.core->CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_initialize(&timing));
    EXPECT_FALSE(timing.retained);
    EXPECT_EQ(0U, g_h743_model.writes);
    /* Withdraw only this test's external cache injection before the real
     * cold-context RAM owner stop. Teardown must not repair live state. */
    g_h743_model.core->CCR = original_ccr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&execution));
    EXPECT_FALSE(execution.prepared);
}
TEST_F(H743FlashTiming, FailedAcquireKeepsCleanupResponsibilityUntilObserved) {
    g_h743_model.ignore_write = &state.registers->ACR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_timing_initialize(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, state.registers->ACR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_FALSE(timing.retained);
}
TEST_F(H743FlashTiming, FailedRestoreRetainsOwnerUntilSuccessfulRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    g_h743_model.ignore_write = &state.registers->ACR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(FLASH_ACR_WRHIGHFREQ_0,
              state.registers->ACR & FLASH_ACR_WRHIGHFREQ);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_FALSE(timing.retained);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, state.registers->ACR);
}
TEST_F(H743FlashTiming, BusyPeerBankPreventsRestoreAndKeepsTheTimingOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    state.registers->SR2 = FLASH_SR_QW;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(writes, g_h743_model.writes);
    state.registers->SR2 = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
}
TEST_F(H743FlashTiming, ForeignLatencyIsNeverRestoredOverOrReleased) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    state.registers->ACR = FLASH_ACR_LATENCY_6WS | FLASH_ACR_WRHIGHFREQ_0;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(writes, g_h743_model.writes);
    /* External policy restores its conflicting latency before owner retry. */
    state.registers->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_FALSE(timing.retained);
}
TEST_F(H743FlashTiming, CompleteStopDoesNotTouchAnyLaterOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    state.registers->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1;
    const uint32_t reads = g_h743_model.reads;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743FlashTiming,
       LiveOwnerCannotBeReinitializedOrStoppedWithoutPrivilege) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_initialize(&timing));
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_flash_timing_initialize(&timing));
    g_h743_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_TRUE(timing.retained);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.privileged = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_timing_stop(&timing));
    EXPECT_FALSE(timing.retained);
}
