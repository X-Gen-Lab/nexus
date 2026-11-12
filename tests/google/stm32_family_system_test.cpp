/**
 * \file            stm32_family_system_test.cpp
 * \brief           Actual selected F4 System transactions and foreign clocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/fault.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#ifdef NEXUS_FAMILY_I2S_PROBE
#include "stm32f407_audio.h"
uint32_t g_av_trace;
uint32_t g_av_counter;
#endif
nx_stm32_system_t g_nx_stm32_system{};
uint32_t nx_family_boot_cpacr;
uint32_t nx_family_boot_mpu;
uint32_t nx_family_boot_vtor;
uint32_t nx_family_rng_control;
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
uint32_t masks;
bool irq_enabled;
bool foreign_irq;
bool refuse_power_gate;
uint32_t observed_id;
uint16_t observed_kib;
unsigned stores;
unsigned pll_stores;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* hardware;

class FamilySystem : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        writes;
    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = 0x24003010U;
        power.CR = PWR_CR_DBP;
        observed_id = (NX_TEST_CUT << 16U) | NX_TEST_DEVICE;
        observed_kib = NX_TEST_FLASH_KIB;
        nx_family_boot_cpacr = 0x00f00000U;
        nx_family_boot_mpu = 0U;
        nx_family_boot_vtor = 0x08000000U;
        nx_family_rng_control = 0U;
        masks = stores = pll_stores = 0U;
        irq_enabled = foreign_irq = refuse_power_gate = false;
        hardware = &writes;
        ON_CALL(writes, Call(testing::_, testing::_))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
                if (reg == &rcc.PLLCFGR)
                    ++pll_stores;
                if (refuse_power_gate && reg == &rcc.APB1ENR &&
                    (value & RCC_APB1ENR_PWREN) != 0U)
                    return;
                if (reg == &timer.SR)
                    *reg &= value;
                else
                    *reg = value;
                if (reg == &timer.EGR && (value & TIM_EGR_UG) != 0U) {
                    timer.SR |= TIM_SR_UIF;
                    timer.CNT = 0U;
                }
            });
    }
    void TearDown() override {
        refuse_power_gate = false;
        foreign_irq = false;
        EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
        hardware = nullptr;
    }
};
} /* namespace */

TEST_F(FamilySystem, ActualHsiClockAndTim2StartAndFullyWithdraw) {
    ASSERT_EQ(nx_stm32_validate_identity(NX_TEST_FLASH_KIB * 1024U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_EQ(g_nx_stm32_system.core_hz, 16000000U);
    EXPECT_EQ(timer.PSC, 7U);
    EXPECT_EQ(rcc.CFGR & RCC_CFGR_PPRE1, 5U << RCC_CFGR_PPRE1_Pos);
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLON | RCC_CR_PLLRDY), 0U);
    EXPECT_TRUE(irq_enabled);
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(power.CR, static_cast<uint32_t>(PWR_CR_DBP));
}

TEST_F(FamilySystem,
       InactivePllParametersAreNeitherOwnedNorWrittenByTheHsiClock) {
    const auto incoming = rcc.PLLCFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(pll_stores, 0U);
    EXPECT_EQ(rcc.PLLCFGR, incoming);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    ASSERT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(pll_stores, 0U);
    EXPECT_EQ(rcc.PLLCFGR, incoming);
}

TEST_F(FamilySystem, InactiveMainOutputsDoNotEraseTheSharedAuxiliaryInput) {
    rcc.PLLCFGR = (rcc.PLLCFGR & ~static_cast<uint32_t>(RCC_PLLCFGR_PLLM)) | 8U;
    const auto incoming = rcc.PLLCFGR;
    const auto input_mask =
        static_cast<uint32_t>(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC);
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    EXPECT_EQ(rcc.PLLCFGR & input_mask, NEXUS_CLOCK_PLLCFGR & input_mask);
    EXPECT_EQ(rcc.PLLCFGR & ~input_mask, incoming & ~input_mask);
    EXPECT_EQ(pll_stores, 1U);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    ASSERT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.PLLCFGR, incoming);
    EXPECT_EQ(pll_stores, 2U);
}

TEST_F(FamilySystem,
       ObservedAuxiliaryInputRemainsPartOfTheRunningClockContract) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    ASSERT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.PLLCFGR ^= RCC_PLLCFGR_PLLSRC;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.PLLCFGR ^= RCC_PLLCFGR_PLLSRC;
}

TEST_F(FamilySystem, CpuVectorsCoverTheSelectedDeviceFlashExtent) {
    SCB_Type scb{};
    volatile uint32_t mpu = 0U;
    const uintptr_t last = FLASH_BASE + NX_TEST_FLASH_KIB * 1024U - 512U;
    EXPECT_TRUE(nx_stm32_system_cpu_boot(&scb, &mpu, last));
    EXPECT_EQ(scb.VTOR, last);
}

TEST_F(FamilySystem, CpuVectorBeyondTheSelectedFlashIsRejectedBeforeAnyStore) {
    SCB_Type scb{};
    volatile uint32_t mpu = 0U;
    EXPECT_FALSE(nx_stm32_system_cpu_boot(
        &scb, &mpu, FLASH_BASE + NX_TEST_FLASH_KIB * 1024U));
    EXPECT_EQ(stores, 0U);
    EXPECT_EQ(scb.CPACR, 0U);
}

TEST_F(FamilySystem, UnknownObservedCutIsRejectedWithoutClockEffects) {
    observed_id = (0x4321U << 16U) | NX_TEST_DEVICE;
    EXPECT_EQ(nx_stm32_validate_identity(NX_TEST_FLASH_KIB * 1024U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stores, 0U);
}

TEST_F(FamilySystem, DensityMismatchIsNotRepairedOrAdopted) {
    observed_kib = NX_TEST_FLASH_KIB == 512U ? 1024U : 512U;
    EXPECT_EQ(nx_stm32_validate_identity(NX_TEST_FLASH_KIB * 1024U),
              NX_ERROR_IO);
    EXPECT_EQ(stores, 0U);
}

TEST_F(FamilySystem, ForeignTimerOwnerIsRejectedBeforeWrites) {
    timer.CR2 = TIM_CR2_MMS_0;
    EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, -2);
    EXPECT_EQ(stores, 0U);
    timer.CR2 = 0U;
}

TEST_F(FamilySystem, RefusedPowerGateHasBoundedCleanupAndDoesNotStartTheTimer) {
    refuse_power_gate = true;
    const auto result = nx_stm32_system_start(&g_nx_stm32_system, 8U);
    EXPECT_NE(result.primary, 0);
    EXPECT_EQ(result.cleanup, 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
}

#if defined(RCC_CR_PLLSAION)
TEST_F(FamilySystem, ForeignPllSaiCannotSurviveColdClockAdmission) {
    rcc.CR |= RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
    EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, -2);
    EXPECT_EQ(stores, 0U);
    rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY);
}
#endif

#if defined(RCC_DCKCFGR_TIMPRE)
TEST_F(FamilySystem, ForeignTimpreCannotSilentlyDoubleTheMonotonicTimerClock) {
    rcc.DCKCFGR = RCC_DCKCFGR_TIMPRE;
    EXPECT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, -2);
    EXPECT_EQ(stores, 0U);
    rcc.DCKCFGR = 0U;
}

TEST_F(FamilySystem, ClockObservationDoesNotHideAnAlteredTimpreDomain) {
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    ASSERT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.DCKCFGR = RCC_DCKCFGR_TIMPRE;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    rcc.DCKCFGR = 0U;
}
#endif

extern "C" void nx_family_poll(void* opaque) {
    auto* state = static_cast<nx_stm32_system_t*>(opaque);
    auto* rcc = state->rcc;
    if ((rcc->CR & RCC_CR_PLLON) != 0U)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
    if ((rcc->CR & RCC_CR_HSEON) == 0U)
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_HSERDY);
    rcc->CFGR = (rcc->CFGR & ~static_cast<uint32_t>(RCC_CFGR_SWS)) |
                ((rcc->CFGR & static_cast<uint32_t>(RCC_CFGR_SW)) << 2U);
}
extern "C" void nx_family_write(volatile uint32_t* reg, uint32_t value) {
    ++stores;
    hardware->Call(reg, value);
}
extern "C" bool nx_family_irq_available(int) {
    return !irq_enabled && !foreign_irq;
}
extern "C" int nx_family_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    return 0;
}
extern "C" uint32_t nx_family_idcode(void) {
    return observed_id;
}
extern "C" uint16_t nx_family_flash_kib(void) {
    return observed_kib;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t saved = {masks};
    masks = 1U;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    masks = saved.value;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {masks, 0U, 0U};
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected family-system terminal fault");
}

#ifdef NEXUS_FAMILY_I2S_PROBE
namespace {
bool aux_hold_release;
uint32_t av_enabled[82], av_pending[82], av_active[82], av_priority[82];
struct FamilyAudio {
    GPIO_TypeDef gpio{};
    SPI_TypeDef spi{};
    DMA_TypeDef dma{};
    DMA_Stream_TypeDef stream{};
    uint16_t snapshots[3]{};
    nx_stm32_av_pin_t pins[3]{{&gpio, &snapshots[0], 1U, 12U, 5U},
                              {&gpio, &snapshots[1], 1U, 13U, 5U},
                              {&gpio, &snapshots[2], 1U, 15U, 5U}};
    alignas(8) uint8_t payload[16]{};
    nx_dma_memory_region_t region{reinterpret_cast<uintptr_t>(payload),
                                  sizeof(payload),
                                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_stm32_audio_clock_t clock{};
    nx_stm32_audio_config_t config{};
    nx_stm32_audio_state_t state{};
    FamilyAudio() {
        aux_hold_release = false;
        for (unsigned i = 0U; i < 82U; ++i)
            av_enabled[i] = av_pending[i] = av_active[i] = av_priority[i] = 0U;
        spi.SR = SPI_SR_TXE;
        config.common = {pins, 3U, &region, 1U, 16U, 5U};
        config.tx = {&dma, &stream, DMA1_Stream4_IRQn, 4U, 0U};
        config.registers = &spi;
        config.clock = &clock;
        config.ws_gpio = config.ck_gpio = &gpio;
        config.ws_pin = 12U;
        config.ck_pin = 13U;
        config.hclk_hz = 16000000U;
        config.pclk_hz = 4000000U;
        config.pll_input_hz = 16000000U;
        config.pll_m = 16U;
        config.pll_n = 192U;
        config.pll_r = 5U;
        config.divider = 12U;
        config.odd = true;
        config.controller = 2U;
        config.sample_bits = config.slot_bits = 16U;
        config.actual_rate = {48000U, 1U};
        config.master = true;
    }
    ~FamilyAudio() {
        aux_hold_release = false;
        if (state.owned) {
            EXPECT_EQ(nx_stm32_audio_close(&state), NX_SUCCESS);
        }
    }
};
} /* namespace */

TEST_F(FamilySystem, RealI2sMasterUsesThePreparedInputWithTheMainPllDisabled) {
    rcc.PLLCFGR = (rcc.PLLCFGR & ~static_cast<uint32_t>(RCC_PLLCFGR_PLLM)) | 8U;
    const auto incoming = rcc.PLLCFGR;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    rcc.AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    FamilyAudio audio;
    const auto prepared = nx_stm32_audio_prepare(&audio.state, &audio.config);
    EXPECT_EQ(prepared, NX_SUCCESS);
    if (prepared != NX_SUCCESS)
        return;
    EXPECT_EQ(audio.clock.users, 1U);
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLON | RCC_CR_PLLRDY), 0U);
    EXPECT_EQ(rcc.CR & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY),
              static_cast<uint32_t>(RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY));
    EXPECT_EQ(nx_stm32_audio_close(&audio.state), NX_SUCCESS);
    EXPECT_EQ(audio.clock.users, 0U);
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 8U), 0);
    EXPECT_EQ(rcc.PLLCFGR, incoming);
}

TEST_F(FamilySystem,
       RealI2sMasterRetainsItsInputLeaseUntilAuxiliaryReleaseJoins) {
    rcc.PLLCFGR = (rcc.PLLCFGR & ~static_cast<uint32_t>(RCC_PLLCFGR_PLLM)) | 8U;
    ASSERT_EQ(nx_stm32_system_start(&g_nx_stm32_system, 8U).primary, 0);
    rcc.AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    FamilyAudio audio;
    const auto prepared = nx_stm32_audio_prepare(&audio.state, &audio.config);
    EXPECT_EQ(prepared, NX_SUCCESS);
    if (prepared != NX_SUCCESS)
        return;
    aux_hold_release = true;
    EXPECT_EQ(nx_stm32_audio_close(&audio.state), NX_ERROR_IO);
    EXPECT_TRUE(audio.state.owned);
    EXPECT_TRUE(audio.state.clock_user);
    EXPECT_EQ(audio.clock.users, 1U);
    EXPECT_EQ(rcc.PLLCFGR & RCC_PLLCFGR_PLLM, 16U);
    aux_hold_release = false;
    EXPECT_EQ(nx_stm32_audio_close(&audio.state), NX_SUCCESS);
    EXPECT_EQ(audio.clock.users, 0U);
}

extern "C" void nx_arch_dmb(void) {
}
extern "C" bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    if (cycles == nullptr)
        return false;
    *cycles = ++g_av_counter;
    return true;
}
extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    return *reg;
}
extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    *reg = value;
}
extern "C" void nx_av_model_poll(void) {
    auto* rcc = g_nx_stm32_system.rcc;
    if ((rcc->CR & RCC_CR_PLLI2SON) != 0U)
        rcc->CR |= RCC_CR_PLLI2SRDY;
    else if (!aux_hold_release)
        rcc->CR &= ~static_cast<uint32_t>(RCC_CR_PLLI2SRDY);
}
extern "C" nx_time_us_t nx_av_model_now(void) {
    return nx_time_now_us();
}
extern "C" uint32_t nx_av_model_cycles(void) {
    return ++g_av_counter;
}
extern "C" bool nx_av_model_privileged(void) {
    return true;
}
extern "C" bool nx_av_model_in_isr(void) {
    return false;
}
extern "C" bool nx_av_model_masked(void) {
    return masks != 0U;
}
extern "C" uint32_t nx_av_model_irq_active(IRQn_Type irq) {
    return av_active[static_cast<unsigned>(irq)];
}
extern "C" uint32_t nx_av_model_irq_pending(IRQn_Type irq) {
    return av_pending[static_cast<unsigned>(irq)];
}
extern "C" uint32_t nx_av_model_irq_enabled(IRQn_Type irq) {
    return av_enabled[static_cast<unsigned>(irq)];
}
extern "C" void nx_av_model_irq_enable(IRQn_Type irq) {
    av_enabled[static_cast<unsigned>(irq)] = 1U;
}
extern "C" void nx_av_model_irq_disable(IRQn_Type irq) {
    av_enabled[static_cast<unsigned>(irq)] = 0U;
}
extern "C" void nx_av_model_irq_clear(IRQn_Type irq) {
    av_pending[static_cast<unsigned>(irq)] = 0U;
}
extern "C" void nx_av_model_irq_priority(IRQn_Type irq, uint32_t priority) {
    av_priority[static_cast<unsigned>(irq)] = priority;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return av_priority[static_cast<unsigned>(irq)];
}
#endif
