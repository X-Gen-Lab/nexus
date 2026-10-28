/**
 * \file            g474_acquisition_test.cpp
 * \brief           Real ADC/Flash production behavior against external MMIO
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_acquisition_model.h"
#include "nexus/core/fault.h"
#include "terminal_watchdog.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
nx_stm32g474_adc_state_t* adc;
nx_stm32g474_adc_stream_state_t* acquisition;
nx_stm32g474_flash_state_t* flash;
bool hold_adc;
bool hold_adc_stop;
bool hold_dma_stop;
bool hold_unlock;
bool hold_pulse;
uint32_t flash_error;
unsigned words;
unsigned pulse_polls;
unsigned conversions;
bool cache_active_at_write;
bool flash_nvic_enabled_at_write;
bool global_clear_while_dma_active;
bool hold_flash_lock;
uint32_t flash_remap;
unsigned flash_remap_reads;
unsigned erase_starts;
unsigned refuse_erase_after;
class FaultPort {
  public:
    MOCK_METHOD(void, Observe, ());
};
FaultPort* faults;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    faults->Observe();
}
extern "C" void nx_test_g474_acquisition_write(volatile uint32_t* reg,
                                               uint32_t value) {
    if (reg == g_g474_model.ignore_write)
        return;
    if (adc != nullptr && reg == &adc->registers->ISR) {
        adc->registers->ISR &= ~value;
        return;
    }
    if (flash != nullptr && reg == &flash->registers->SR) {
        flash->registers->SR &= ~value;
        return;
    }
    if (flash != nullptr && reg == &flash->registers->CR &&
        (value & FLASH_CR_STRT) != 0U && erase_starts++ >= refuse_erase_after)
        return;
    if (flash != nullptr && reg == &flash->registers->CR && hold_flash_lock)
        value &= ~(uint32_t)FLASH_CR_LOCK;
    if (acquisition != nullptr && reg == &acquisition->dma->IFCR) {
        if ((value & DMA_IFCR_CGIF1) != 0U &&
            (acquisition->channel->CCR & DMA_CCR_EN) != 0U)
            global_clear_while_dma_active = true;
        const_cast<volatile uint32_t&>(acquisition->dma->ISR) &= ~value;
        return;
    }
    if (acquisition != nullptr && reg == &acquisition->channel->CCR &&
        hold_dma_stop && (value & DMA_CCR_EN) == 0U)
        value |= DMA_CCR_EN;
    *reg = value;
}
extern "C" void nx_test_g474_adc_poll(nx_stm32g474_adc_state_t* p) {
    ++g_g474_model.timer.CNT;
    auto* r = p->registers;
    if ((r->CR & ADC_CR_ADCAL) != 0U && !hold_adc)
        r->CR &= ~(uint32_t)ADC_CR_ADCAL;
    if ((r->CR & ADC_CR_ADEN) != 0U)
        r->ISR |= ADC_ISR_ADRDY;
    if ((r->CR & ADC_CR_ADSTP) != 0U && !hold_adc_stop)
        r->CR &= ~(uint32_t)(ADC_CR_ADSTP | ADC_CR_ADSTART);
    if ((r->CR & ADC_CR_ADDIS) != 0U && !hold_adc_stop)
        r->CR &= ~(uint32_t)(ADC_CR_ADDIS | ADC_CR_ADEN);
    if ((r->CR & ADC_CR_ADSTART) != 0U && (r->CFGR & ADC_CFGR_EXTEN) == 0U &&
        !hold_adc) {
        r->DR = ++conversions == 1U ? 0U : 1234U;
        r->ISR |= ADC_ISR_EOC | ADC_ISR_EOS;
        r->CR &= ~(uint32_t)ADC_CR_ADSTART;
    }
}
extern "C" void nx_test_g474_flash_poll(nx_stm32g474_flash_state_t* p) {
    ++g_g474_model.timer.CNT;
    ++pulse_polls;
    auto* r = p->registers;
    if (r->KEYR == 0xcdef89abU && !hold_unlock)
        r->CR &= ~(uint32_t)FLASH_CR_LOCK;
    if ((r->CR & FLASH_CR_STRT) != 0U && (r->CR & FLASH_CR_PER) != 0U) {
        size_t page = (r->CR & FLASH_CR_PNB) >> FLASH_CR_PNB_Pos;
        if ((r->CR & FLASH_CR_BKER) != 0U)
            page += 128;
        if ((flash_remap & SYSCFG_MEMRMP_FB_MODE) != 0U)
            page ^= 128U;
        std::memset(const_cast<uint8_t*>(p->memory) + page * 2048, 255, 2048);
        r->CR &= ~(uint32_t)FLASH_CR_STRT;
        r->SR |= FLASH_SR_BSY;
    }
    if ((r->SR & FLASH_SR_BSY) != 0U && (!hold_pulse || pulse_polls >= 4)) {
        r->SR &= ~(uint32_t)FLASH_SR_BSY;
        if (flash_error == 0U) {
            if ((r->CR & FLASH_CR_EOPIE) != 0U)
                r->SR |= FLASH_SR_EOP;
        } else {
            r->SR |= flash_error & ~(uint32_t)FLASH_SR_OPERR;
            if ((r->CR & FLASH_CR_ERRIE) != 0U)
                r->SR |= flash_error & FLASH_SR_OPERR;
        }
        if (((r->SR & FLASH_SR_EOP) != 0U && (r->CR & FLASH_CR_EOPIE) != 0U) ||
            ((r->SR & FLASH_SR_OPERR) != 0U && (r->CR & FLASH_CR_ERRIE) != 0U))
            g_g474_model.irq_pending[FLASH_IRQn] = true;
    }
}
extern "C" uint32_t nx_test_g474_flash_remap(void) {
    ++flash_remap_reads;
    return flash_remap;
}
extern "C" void nx_test_g474_flash_word(volatile uint32_t* address,
                                        uint32_t value) {
    ++words;
    flash_nvic_enabled_at_write =
        flash_nvic_enabled_at_write || g_g474_model.irq_enabled[FLASH_IRQn];
    cache_active_at_write =
        cache_active_at_write || (flash->registers->ACR & FLASH_ACR_DCEN) != 0U;
    *address = value;
    flash->registers->SR |= FLASH_SR_BSY;
}
class G474Adc : public testing::Test {
  protected:
    ADC_TypeDef registers{};
    ADC_TypeDef peer{};
    ADC_Common_TypeDef common{};
    nx_stm32g474_adc_state_t state{};
    nx_adc_port_t face{&nx_stm32g474_adc_ops, &state};
    testing::NiceMock<FaultPort> fault_mock;
    void SetUp() override {
        nx_g474_model_reset();
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000;
        g_g474_model.timer.CNT = 1;
        adc = &state;
        acquisition = nullptr;
        flash = nullptr;
        hold_adc = hold_adc_stop = hold_dma_stop = false;
        conversions = 0;
        global_clear_while_dma_active = false;
        faults = &fault_mock;
        state.registers = &registers;
        state.peer = &peer;
        state.common = &common;
        state.reference_mv = 3300;
        state.poll_limit = 32;
        registers.CR = ADC_CR_DEEPPWD;
        peer.CR = ADC_CR_DEEPPWD;
        g_g474_model.rcc.CCIPR = RCC_CCIPR_ADC12SEL_1;
        g_g474_model.rcc.AHB2ENR = RCC_AHB2ENR_ADC12EN;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_initialize(&state));
    }
};
TEST_F(G474Adc, ColdPrepareBindsResourcesAndRefusesLiveRebinding) {
    const nx_stm32g474_adc_config_t config{&registers, &peer, &common, 3300U,
                                           32U};
    state = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_prepare(&state, &config));
    EXPECT_EQ(&registers, state.registers);
    ADC_TypeDef foreign{};
    auto other = config;
    other.registers = &foreign;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_prepare(&state, &other));
    EXPECT_EQ(&registers, state.registers);
    EXPECT_EQ(0U, foreign.CR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_close(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_prepare(&state, &config));
}
TEST_F(G474Adc, ColdPrepareRejectsContextAndInvalidConfigBeforePadEffects) {
    nx_stm32g474_adc_config_t config{&registers, &peer, &common, 3300U, 32U};
    g_g474_model.privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32g474_adc_prepare(&state, &config));
    g_g474_model.privileged = true;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_adc_prepare(nullptr, &config));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_adc_prepare(&state, nullptr));
    config.reference_mv = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32g474_adc_prepare(&state, &config));
    EXPECT_EQ(0U, g_g474_model.gpio.MODER);
    EXPECT_EQ(0U, common.CCR);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Adc, ColdInitializationConfiguresOnlyTheOwnedAnalogPad) {
    g_g474_model.gpio.MODER = 0xA5A5A5A0U;
    g_g474_model.gpio.PUPDR = 0x5A5A5A53U;
    Start();
    EXPECT_EQ(0xA5A5A5A3U, g_g474_model.gpio.MODER);
    EXPECT_EQ(0x5A5A5A50U, g_g474_model.gpio.PUPDR);
    EXPECT_TRUE(state.initialized);
}
TEST_F(G474Adc, AnalogReadbackFailureRetainsCleanupBeforeCalibration) {
    g_g474_model.ignore_write = &g_g474_model.gpio.MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_adc_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(0U, common.CCR);
    EXPECT_EQ(ADC_CR_DEEPPWD, registers.CR);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Adc, ForeignConverterRejectsBeforeChangingAnalogPad) {
    g_g474_model.gpio.MODER = 0xA5A5A5A0U;
    g_g474_model.gpio.PUPDR = 0x5A5A5A53U;
    peer.CR = ADC_CR_ADEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_initialize(&state));
    EXPECT_EQ(0xA5A5A5A0U, g_g474_model.gpio.MODER);
    EXPECT_EQ(0x5A5A5A53U, g_g474_model.gpio.PUPDR);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Adc, CalibrationAndSingleShotUseExactChannelAndClock) {
    Start();
    EXPECT_EQ(10U << ADC_CCR_PRESC_Pos, common.CCR);
    EXPECT_EQ(1U << ADC_SQR1_SQ1_Pos, registers.SQR1);
    EXPECT_EQ(0U, registers.SMPR1);
    uint16_t value = 0;
    size_t count = 0;
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(1234U, value);
    EXPECT_EQ(2U, conversions);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_close(&state));
    EXPECT_EQ(0U, common.CCR);
    EXPECT_EQ(ADC_CR_DEEPPWD, registers.CR);
}
TEST_F(G474Adc, LiveSiblingRejectsBeforeCommonClockMutation) {
    peer.CR = ADC_CR_ADEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_initialize(&state));
    EXPECT_EQ(0U, common.CCR);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Adc, TimeoutStopsConverterBeforeReturningBuffer) {
    Start();
    hold_adc = true;
    uint16_t value = 77;
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(77U, value);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
    EXPECT_FALSE(state.active);
}
TEST_F(G474Adc, FailedAbortRetainsTheCallingFrame) {
    Start();
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        hold_adc = true;
        hold_adc_stop = true;
        EXPECT_CALL(fault_mock, Observe()).WillOnce([&] {
            nexus_test::RecordTerminalObservation(state.active && state.fault);
        });
        uint16_t value = 77;
        size_t count = 0;
        (void)nx_adc_port_sample(&face, &value, 1, 10000, &count);
    }));
}
TEST_F(G474Adc, ExpiredAndControlAliasesRejectWithoutStarting) {
    Start();
    uint16_t value = 0;
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&face, &value, 1, 0, &count));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_port_sample(&face, reinterpret_cast<uint16_t*>(&state), 1,
                                 1000, &count));
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
}
TEST_F(G474Adc, ReviewedBulbClockAndFiniteIdleNeverLeaveTheSamplerEnabled) {
    Start();
    EXPECT_EQ(10U << ADC_CCR_PRESC_Pos, common.CCR);
    EXPECT_EQ(0U, registers.SMPR1 & ADC_SMPR1_SMP1);
    uint16_t value = 0;
    size_t count = 0;
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(0U, registers.CR & (ADC_CR_ADEN | ADC_CR_ADSTART));
}
class G474Flash : public testing::Test {
  protected:
    FLASH_TypeDef registers{};
    alignas(8) uint8_t memory[524288];
    nx_stm32g474_flash_state_t state{&registers, memory, 3300, 0, false, false};
    nx_flash_port_t face{&nx_stm32g474_flash_ops, &state};
    void SetUp() override {
        nx_g474_model_reset();
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000;
        g_g474_model.timer.CNT = 1;
        adc = nullptr;
        acquisition = nullptr;
        flash = &state;
        hold_unlock = hold_pulse = false;
        flash_error = words = pulse_polls = 0;
        cache_active_at_write = false;
        flash_nvic_enabled_at_write = false;
        hold_flash_lock = false;
        flash_remap = flash_remap_reads = 0;
        erase_starts = 0;
        refuse_erase_after = UINT32_MAX;
        g_g474_model.rcc.APB2ENR |= RCC_APB2ENR_SYSCFGEN;
        registers.CR = FLASH_CR_LOCK;
        registers.OPTR = FLASH_OPTR_DBANK;
        std::memset(memory, 255, sizeof(memory));
    }
};
TEST_F(G474Flash, FullPhysicalGeometryHasBothBanksAndEightByteUnits) {
    const auto* geometry = nx_flash_port_geometry(&face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(524288U, geometry->size);
    EXPECT_EQ(8U, geometry->program_unit);
    ASSERT_EQ(256U, geometry->sector_count);
    EXPECT_EQ(262144U, geometry->sectors[128].offset);
    EXPECT_EQ(2048U, geometry->sectors[255].size);
}
TEST_F(G474Flash, DoubleWordProgramAndSecondBankPageEraseArePhysical) {
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 262144, data, 8, 1000));
    EXPECT_EQ(2U, words);
    EXPECT_EQ(0, std::memcmp(data, memory + 262144, 8));
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_erase(&face, 262144, 2048, 1000));
    EXPECT_EQ(255U, memory[262144]);
    EXPECT_EQ(0U, registers.CR & (FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_BKER));
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
}
TEST_F(G474Flash, ErratumDisablesAndResetsDataCacheAcrossPhysicalPulses) {
    registers.ACR = FLASH_ACR_DCEN | FLASH_ACR_ICEN | 4U;
    const uint32_t original = registers.ACR;
    uint8_t data[8] = {};
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_FALSE(cache_active_at_write);
    EXPECT_EQ(original, registers.ACR);
}
TEST_F(G474Flash, RefusedRelockReportsFailureAndRetainsCleanupResponsibility) {
    uint8_t data[8] = {};
    hold_flash_lock = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_TRUE(state.restore_pending);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_flash_close(&state));
    hold_flash_lock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_flash_close(&state));
    EXPECT_FALSE(state.restore_pending);
}
TEST_F(G474Flash, AlignmentBoundsDensityAndDbankRejectBeforeUnlock) {
    uint8_t data[8] = {};
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_program(&face, 4, data, 8, 1000));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, 1, 2048, 1000));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_read(&face, 524288, data, 1));
    registers.OPTR = 0;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_EQ(0U, registers.KEYR);
}
TEST_F(G474Flash, DeadlineDoesNotAbortAnActivePhysicalPulse) {
    uint8_t data[8] = {};
    hold_pulse = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_program(&face, 0, data, 8, 3));
    EXPECT_GE(pulse_polls, 4U);
    EXPECT_EQ(0U, registers.SR & FLASH_SR_BSY);
    EXPECT_FALSE(state.active);
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
}
TEST_F(G474Flash, HardwareErrorsRelockAndAnEccLineCannotBeReprogrammed) {
    uint8_t data[8] = {};
    flash_error = FLASH_SR_PROGERR;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
    flash_error = 0;
    words = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_EQ(0U, words);
}
TEST_F(G474Flash, AliasedReadAndProgramCannotMutateProviderControls) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 0, &state, 8, 1000));
    EXPECT_EQ(0U, registers.KEYR);
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_read(&face, 0, &state, 8));
}
TEST_F(G474Flash, ForeignProgrammingModeRejectsWithoutAnyPhysicalPulse) {
    registers.CR = FLASH_CR_PG;
    uint8_t data[8] = {};
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_EQ(0U, words);
    EXPECT_EQ(FLASH_CR_PG, registers.CR);
}
TEST_F(G474Flash, SwappedBankMappingCannotEraseTheOtherLogicalRegion) {
    std::memset(memory, 0, sizeof(memory));
    flash_remap = SYSCFG_MEMRMP_FB_MODE;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_flash_port_erase(&face, 0, 2048, 1000));
    EXPECT_EQ(0U, memory[0]);
    EXPECT_EQ(0U, memory[262144]);
    EXPECT_EQ(0U, registers.KEYR);
}
TEST_F(G474Flash, MappingCannotBeReadThroughAnUnclockedSyscfg) {
    g_g474_model.rcc.APB2ENR &= ~(uint32_t)RCC_APB2ENR_SYSCFGEN;
    uint8_t data[8] = {};
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_EQ(0U, flash_remap_reads);
    EXPECT_EQ(0U, words);
}
TEST_F(G474Flash, AlternateBootBankOptionIsRejectedWithoutChangingIt) {
    registers.OPTR |= FLASH_OPTR_BFB2;
    uint8_t data[8] = {};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 0, data, 8, 1000));
    EXPECT_EQ(0U, words);
    EXPECT_NE(0U, registers.OPTR & FLASH_OPTR_BFB2);
}
class G474AdcStream : public G474Adc {
  protected:
    TIM_TypeDef timer{};
    DMA_TypeDef dma{};
    DMA_Channel_TypeDef channel{};
    DMAMUX_Channel_TypeDef mux{};
    nx_stm32g474_adc_stream_state_t engine{};
    alignas(2) uint8_t data[2][8]{};
    nx_stream_slot_t slots[2]{};
    nx_stream_t stream{};
    nx_dma_memory_region_t region{};
    nx_adc_port_t stream_face{&nx_stm32g474_adc_stream_ops, &engine};
    void SetUp() override {
        G474Adc::SetUp();
        engine.adc = state;
        adc = &engine.adc;
        acquisition = &engine;
        engine.timer = &timer;
        engine.dma = &dma;
        engine.channel = &channel;
        engine.mux = &mux;
        region = {reinterpret_cast<uintptr_t>(data), sizeof(data),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        engine.regions = &region;
        engine.region_count = 1;
        engine.irq_policy = {NX_IRQ_KERNEL_NONE, 102, 4, 0};
        for (size_t i = 0; i < 2; ++i) {
            slots[i].data = data[i];
            slots[i].capacity = sizeof(data[i]);
        }
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 2));
    }
    void StartStream() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
        ASSERT_EQ(NX_SUCCESS,
                  nx_adc_port_stream_start(&stream_face, &stream, 2500));
    }
    void Complete() {
        channel.CNDTR = 0;
        const_cast<volatile uint32_t&>(dma.ISR) = DMA_ISR_TCIF1;
        nx_stm32g474_adc_stream_irq(&engine);
    }
};
TEST_F(G474AdcStream, ColdPrepareAdmitsDmaBeforeAnalogAndCalibratesOnce) {
    const nx_stm32g474_adc_stream_config_t config{
        {&registers, &peer, &common, 3300U, 32U},
        &timer,
        &dma,
        &channel,
        &mux,
        &region,
        1U,
        {NX_IRQ_KERNEL_NONE, 102U, 4U, 0U}};
    engine = {};
    channel.CCR = DMA_CCR_EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_stream_prepare(&engine, &config));
    EXPECT_EQ(0U, g_g474_model.gpio.MODER);
    EXPECT_FALSE(engine.adc.retained);
    channel.CCR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_prepare(&engine, &config));
    EXPECT_EQ(3U, g_g474_model.gpio.MODER & 3U);
    EXPECT_EQ(5U, mux.CCR);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_stream_prepare(&engine, &config));
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_close(&engine));
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_prepare(&engine, &config));
}
TEST_F(G474AdcStream, ColdPreparePreservesAnOutstandingPublisherBinding) {
    const nx_stm32g474_adc_stream_config_t config{
        {&registers, &peer, &common, 3300U, 32U},
        &timer,
        &dma,
        &channel,
        &mux,
        &region,
        1U,
        {NX_IRQ_KERNEL_NONE, 102U, 4U, 0U}};
    engine.wake_publishers = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_stream_prepare(&engine, &config));
    EXPECT_EQ(1U, engine.wake_publishers);
    EXPECT_EQ(0U, g_g474_model.gpio.MODER);
    EXPECT_FALSE(engine.adc.retained);
}
TEST_F(G474AdcStream, NormalDmaPublishesOnlyAfterTaskObservedDetach) {
    StartStream();
    EXPECT_EQ(5U, mux.CCR);
    EXPECT_EQ(4U, channel.CNDTR);
    EXPECT_NE(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_CIRC);
    Complete();
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    EXPECT_EQ(NX_STREAM_SLOT_READY, slots[0].state);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
    EXPECT_FALSE(engine.filling);
}
TEST_F(G474AdcStream, BackpressureNeverOverwritesBorrowedBlocks) {
    StartStream();
    Complete();
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    Complete();
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    EXPECT_EQ(NX_ERROR_BUSY, nx_adc_port_stream_service(&stream_face));
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, slots[0].state);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_EN);
    EXPECT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
}
TEST_F(G474AdcStream, RefusedDmaStopRetainsProducerLoanForRetry) {
    StartStream();
    hold_dma_stop = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_adc_port_stream_stop(&stream_face));
    EXPECT_TRUE(engine.filling);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
    hold_dma_stop = false;
    EXPECT_EQ(NX_SUCCESS, nx_adc_port_stream_stop(&stream_face));
    EXPECT_FALSE(engine.filling);
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
}
TEST_F(G474AdcStream, ErratumNeverClearsGlobalDmaFlagWhileChannelIsActive) {
    StartStream();
    hold_dma_stop = true;
    Complete();
    EXPECT_FALSE(global_clear_while_dma_active);
    EXPECT_EQ(NX_STREAM_SLOT_FILLING, slots[0].state);
}
TEST_F(G474AdcStream, InvalidDomainAndRateRejectBeforeAnyLoan) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_adc_port_stream_start(&stream_face, &stream, 70000));
    region.size = 2;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_adc_port_stream_start(&stream_face, &stream, 2500));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_EN);
}
TEST_F(G474AdcStream, RejectedForeignMuxNeverBecomesAnOwnedCleanupLease) {
    mux.CCR = 44;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_adc_stream_initialize(&engine));
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_close(&engine));
    EXPECT_EQ(44U, mux.CCR);
    EXPECT_FALSE(engine.adc.retained);
}
TEST_F(G474AdcStream, FailedArmProvesRollbackBeforeRejectingItsProducerLoan) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
    g_g474_model.hold_irq_on = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_stream_start(&stream_face, &stream, 2500));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_FALSE(engine.filling);
    EXPECT_EQ(nullptr, engine.stream);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_EN);
}
TEST_F(G474AdcStream, RefusedDmaCountReadbackRejectsAndReleasesOnlyAfterDrain) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
    g_g474_model.ignore_write = &channel.CNDTR;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_stream_start(&stream_face, &stream, 2500));
    EXPECT_EQ(0U, stream.outstanding);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, channel.CCR & DMA_CCR_EN);
}
TEST_F(G474AdcStream, TransferErrorDropsTheBlockAndReportsALoss) {
    StartStream();
    const_cast<volatile uint32_t&>(dma.ISR) = DMA_ISR_TEIF1;
    nx_stm32g474_adc_stream_irq(&engine);
    EXPECT_EQ(NX_ERROR_IO, nx_adc_port_stream_service(&stream_face));
    EXPECT_EQ(NX_STREAM_SLOT_FREE, slots[0].state);
    EXPECT_EQ(1U, stream.losses);
    EXPECT_EQ(0U, stream.outstanding);
}
TEST_F(G474AdcStream, ConsumerLoanKeepsStopPendingUntilMatchingRelease) {
    StartStream();
    Complete();
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    nx_stream_block_t block{};
    ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
    EXPECT_EQ(NX_ERROR_BUSY, nx_adc_port_stream_stop(&stream_face));
    EXPECT_EQ(NX_STREAM_SLOT_BORROWED, slots[0].state);
    ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
    EXPECT_EQ(NX_SUCCESS, nx_adc_port_stream_stop(&stream_face));
    EXPECT_EQ(nullptr, engine.stream);
}
TEST_F(G474AdcStream, CloseReturnsAllOwnedConfigurationToColdAdmissionState) {
    StartStream();
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_stop(&stream_face));
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_close(&engine));
    EXPECT_EQ(0U, channel.CCR);
    EXPECT_EQ(0U, channel.CNDTR);
    EXPECT_EQ(0U, channel.CPAR);
    EXPECT_EQ(0U, channel.CMAR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
}
TEST_F(G474AdcStream, RateAndClockBoundsRespectTheBulbSamplingMaximum) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
    EXPECT_EQ(10U << ADC_CCR_PRESC_Pos, common.CCR);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_adc_port_stream_start(&stream_face, &stream, 2000));
    EXPECT_EQ(0U, stream.outstanding);
}

TEST_F(G474Flash, AllOnesInputPreservesAnErasedEccLineWithoutPulsing) {
    uint64_t all_ones = UINT64_MAX;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0, &all_ones, 8, 1000));
    EXPECT_EQ(0U, words);
    EXPECT_EQ(NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE,
              nx_flash_port_geometry(&face)->program_flags);
}
TEST_F(G474Flash, LiveMappingGuardAlsoProtectsReadAndProgram) {
    flash_remap = SYSCFG_MEMRMP_FB_MODE;
    uint64_t value = 0;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_flash_port_read(&face, 0, &value, 8));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 0, &value, 8, 1000));
    EXPECT_EQ(0U, words);
    EXPECT_EQ(0U, registers.KEYR);
}
TEST_F(G474AdcStream, CloseRetainsOwnershipWhenColdRestorationIsRefused) {
    StartStream();
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_stop(&stream_face));
    g_g474_model.ignore_write = &channel.CCR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_adc_stream_close(&engine));
    EXPECT_TRUE(engine.adc.retained);
    g_g474_model.ignore_write = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_close(&engine));
    EXPECT_FALSE(engine.adc.retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_adc_stream_initialize(&engine));
}
TEST_F(G474AdcStream, FirstTriggerAndBackpressureHaveExplicitSamplingBounds) {
    StartStream();
    EXPECT_EQ(399U, timer.ARR);
    EXPECT_EQ(390U, timer.CNT);
    channel.CNDTR = 0;
    const_cast<volatile uint32_t&>(dma.ISR) = DMA_ISR_TCIF1;
    nx_stm32g474_adc_stream_irq(&engine);
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&stream_face));
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADEN);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
}

TEST_F(G474Flash, RefusedEraseStartCannotBecomeSuccessWithoutAFreshEop) {
    std::memset(memory, 0, 2048);
    refuse_erase_after = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0, 2048, 1000));
    EXPECT_EQ(0U, memory[0]);
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
}
TEST_F(G474Flash, EveryErasePageMustProveItsOwnCompletion) {
    std::memset(memory, 0, 4096);
    refuse_erase_after = 1;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0, 4096, 1000));
    EXPECT_EQ(255U, memory[0]);
    EXPECT_EQ(0U, memory[2048]);
}
TEST_F(G474Flash, RefusedStatusClearCannotLaunchAPhysicalProgramPulse) {
    registers.SR = FLASH_SR_EOP;
    g_g474_model.ignore_write = &registers.SR;
    uint64_t value = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0, &value, 8, 1000));
    EXPECT_EQ(0U, words);
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
}

TEST_F(G474Flash, EopSourceIsEnabledWithItsNvicDisabledAndPendingDrained) {
    uint64_t value = 0;
    g_g474_model.priority[FLASH_IRQn] = 9;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0, &value, 8, 1000));
    EXPECT_FALSE(flash_nvic_enabled_at_write);
    EXPECT_FALSE(g_g474_model.irq_enabled[FLASH_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_pending[FLASH_IRQn]);
    EXPECT_EQ(9U, g_g474_model.priority[FLASH_IRQn]);
    EXPECT_EQ(0U, registers.CR & (FLASH_CR_EOPIE | FLASH_CR_ERRIE));
}
TEST_F(G474Flash, ForeignFlashVectorAndEccSourceRejectBeforeUnlock) {
    uint64_t value = 0;
    for (bool* foreign : {&g_g474_model.irq_enabled[FLASH_IRQn],
                          &g_g474_model.irq_pending[FLASH_IRQn],
                          &g_g474_model.irq_active[FLASH_IRQn]}) {
        *foreign = true;
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_flash_port_program(&face, 0, &value, 8, 1000));
        EXPECT_EQ(0U, registers.KEYR);
        *foreign = false;
    }
    registers.ECCR = FLASH_ECCR_ECCIE;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_program(&face, 0, &value, 8, 1000));
    EXPECT_EQ(0U, words);
}
TEST_F(G474Flash, RefusedPendingJoinRetainsCleanupUntilAnObservedRetry) {
    uint64_t value = 0;
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0, &value, 8, 1000));
    EXPECT_TRUE(state.restore_pending);
    EXPECT_TRUE(g_g474_model.irq_pending[FLASH_IRQn]);
    EXPECT_EQ(0U, registers.CR & (FLASH_CR_EOPIE | FLASH_CR_ERRIE));
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_flash_close(&state));
    EXPECT_FALSE(state.restore_pending);
    EXPECT_FALSE(g_g474_model.irq_pending[FLASH_IRQn]);
}

TEST_F(G474Adc, RefusedReadyClearCannotAdoptAnOldAdcEnableCompletion) {
    Start();
    registers.ISR = ADC_ISR_ADRDY;
    g_g474_model.ignore_write = &registers.ISR;
    hold_adc = true;
    uint16_t value = 77;
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(77U, value);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADEN);
}
TEST_F(G474Adc, RefusedEndOfConversionClearCannotPublishAnOldRawValue) {
    Start();
    registers.ISR = ADC_ISR_EOC | ADC_ISR_EOS;
    registers.DR = 987;
    g_g474_model.ignore_write = &registers.ISR;
    hold_adc = true;
    uint16_t value = 77;
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(77U, value);
    EXPECT_EQ(0U, count);
}
TEST_F(G474Adc, SecondPrimeShotRequiresItsOwnClearedCompletionFlags) {
    Start();
    g_g474_model.ignore_write = &registers.ISR;
    uint16_t value = 77;
    size_t count = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_adc_port_sample(&face, &value, 1, 10000, &count));
    EXPECT_EQ(77U, value);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(1U, conversions);
}
