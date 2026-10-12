/**
 * \file            gd32_system_transaction_test.cpp
 * \brief           Actual GD32 fixed clock rollback and owned field contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f4xx.h"
#include "private/system.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

NVIC_Type g_gd32_model_nvic{};
SCB_Type g_gd32_model_scb{};
SysTick_Type g_gd32_model_systick{};
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
extern "C" const uint32_t __gVectors[] = {0U};

namespace {
class HardwarePort {
  public:
    MOCK_METHOD(void, Write, (volatile uint32_t * reg, uint32_t value));
};
HardwarePort* hardware_port;
bool hold_hse;
bool hold_hsi;
bool hold_pll;
bool hold_switch;
bool hold_power_release;
bool privileged = true;

class Gd32System : public testing::Test {
    void* registers = MAP_FAILED;

  protected:
    testing::NiceMock<HardwarePort> hardware_mock;

    void SetUp() override {
        registers =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(registers, reinterpret_cast<void*>(0x40000000U));
        std::memset(registers, 0, 0x80000U);
        RCU_CTL = RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB;
        RCU_PLL = UINT32_C(0x24003010);
        PMU_CTL = PMU_CTL_BKPWEN;
        FMC_WS = 1U;
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        hold_power_release = false;
        privileged = true;
        hardware_port = &hardware_mock;
        EXPECT_CALL(hardware_mock, Write(testing::_, testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(hardware_mock, Write(testing::_, testing::_))
            .WillByDefault(
                [](volatile uint32_t* reg, uint32_t value) { *reg = value; });
        SystemCoreClock = 16000000U;
    }
    void TearDown() override {
        testing::Mock::VerifyAndClearExpectations(&hardware_mock);
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        hold_power_release = false;
        privileged = true;
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        if (registers == reinterpret_cast<void*>(0x40000000U)) {
            EXPECT_EQ(0, nx_gd32_clock_stop());
            EXPECT_EQ(0, munmap(registers, 0x80000U));
        } else if (registers != MAP_FAILED) {
            EXPECT_EQ(0, munmap(registers, 0x80000U));
        }
        hardware_port = nullptr;
    }
};
} /* namespace */

TEST_F(Gd32System, ReviewedProfileObservesActualCpuAndApbFrequencies) {
    ASSERT_EQ(0, nx_gd32_clock_start());
    EXPECT_EQ(200000000U, SystemCoreClock);
    EXPECT_EQ(50000000U, rcu_clock_freq_get(CK_APB1));
    EXPECT_EQ(100000000U, rcu_clock_freq_get(CK_APB2));
    EXPECT_EQ(7U, FMC_WS & FMC_WC_WSCNT);
}

TEST_F(Gd32System, StopRestoresOwnedClockVoltageAndFlashFields) {
    const uint32_t old_pll = RCU_PLL;
    const uint32_t old_power = PMU_CTL;
    const uint32_t old_flash = FMC_WS;
    ASSERT_EQ(0, nx_gd32_clock_start());
    PMU_CTL |= PMU_CTL_LVDEN;
    ASSERT_EQ(0, nx_gd32_clock_stop());
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(old_pll, RCU_PLL);
    EXPECT_EQ(old_flash, FMC_WS);
    EXPECT_EQ(old_power | PMU_CTL_LVDEN, PMU_CTL);
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_PMUEN);
}

TEST_F(Gd32System, ExistingPowerInterfaceClockRemainsOwnedByItsCaller) {
    RCU_APB1EN |= RCU_APB1EN_PMUEN;
    ASSERT_EQ(0, nx_gd32_clock_start());
    ASSERT_EQ(0, nx_gd32_clock_stop());
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_PMUEN);
}

TEST_F(Gd32System, HxtalFailureRestoresSnapshotAfterExplicitCleanup) {
    const uint32_t old_pll = RCU_PLL;
    const uint32_t old_power = PMU_CTL;
    const uint32_t old_flash = FMC_WS;
    hold_hse = true;
    EXPECT_NE(0, nx_gd32_clock_start());
    EXPECT_EQ(0, nx_gd32_clock_stop());
    EXPECT_EQ(old_pll, RCU_PLL);
    EXPECT_EQ(old_power, PMU_CTL);
    EXPECT_EQ(old_flash, FMC_WS);
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_PMUEN);
}

TEST_F(Gd32System, ExistingSystemPllRejectsBeforeAnyWrite) {
    RCU_CTL |=
        RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
    RCU_CFG0 = RCU_CKSYSSRC_PLLP | RCU_SCSS_PLLP | RCU_APB1_CKAHB_DIV4 |
               RCU_APB2_CKAHB_DIV2;
    RCU_PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, nx_gd32_clock_start());
    EXPECT_NE(0U, RCU_CTL & RCU_CTL_PLLEN);
}

TEST_F(Gd32System, UnknownDividerRejectsBeforeClockMutation) {
    RCU_CFG0 = RCU_APB1_CKAHB_DIV4;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, nx_gd32_clock_start());
    EXPECT_EQ(RCU_APB1_CKAHB_DIV4, RCU_CFG0);
}

TEST_F(Gd32System, UnprivilegedOrMaskedClockChangeHasNoEffects) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    privileged = false;
    EXPECT_NE(0, nx_gd32_clock_start());
    privileged = true;
    g_gd32_model_mask = 1U;
    EXPECT_NE(0, nx_gd32_clock_start());
    g_gd32_model_mask = 0U;
}

TEST_F(Gd32System, RefusedFlashLatencyCannotSwitchToHighSpeed) {
    EXPECT_CALL(hardware_mock, Write(&FMC_WS, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_NE(0, nx_gd32_clock_start());
    EXPECT_EQ(RCU_SCSS_IRC16M, RCU_CFG0 & RCU_CFG0_SCSS);
}

TEST_F(Gd32System, RefusedPllDivisorsCannotPublishReviewedProfile) {
    EXPECT_CALL(hardware_mock, Write(&RCU_PLL, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_NE(0, nx_gd32_clock_start());
    EXPECT_EQ(RCU_SCSS_IRC16M, RCU_CFG0 & RCU_CFG0_SCSS);
}

TEST_F(Gd32System, FailedHsiSwitchNeverDisablesTheLivePll) {
    ASSERT_EQ(0, nx_gd32_clock_start());
    hold_hsi = true;
    EXPECT_NE(0, nx_gd32_clock_stop());
    EXPECT_NE(0U, RCU_CTL & RCU_CTL_PLLEN);
    EXPECT_NE(0, nx_gd32_clock_start());
}

TEST_F(Gd32System, AuxiliaryClockConsumerBlocksStopBeforeAnyClockWrite) {
    ASSERT_EQ(0, nx_gd32_clock_start());
    RCU_CTL |= RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, nx_gd32_clock_stop());
    EXPECT_EQ(RCU_SCSS_PLLP, RCU_CFG0 & RCU_CFG0_SCSS);
    EXPECT_EQ(200000000U, SystemCoreClock);
    RCU_CTL &= ~(uint32_t)(RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB);
}

TEST_F(Gd32System, UnacknowledgedHighDriveExitRetainsPowerResponsibility) {
    ASSERT_EQ(0, nx_gd32_clock_start());
    hold_power_release = true;
    EXPECT_NE(0, nx_gd32_clock_stop());
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_PMUEN);
    EXPECT_NE(0U, PMU_CTL & PMU_CTL_HDEN);
    hold_power_release = false;
    EXPECT_EQ(0, nx_gd32_clock_stop());
}

/** \brief Observe only hardware writes at the external register port. */
extern "C" void nx_gd32_system_model_write(volatile uint32_t* reg,
                                           uint32_t value) {
    hardware_port->Write(reg, value);
}
/** \brief Advance modeled stability flags and actual source acknowledgements.
 */
extern "C" void nx_gd32_model_clock_poll(void) {
    if (hold_hsi)
        RCU_CTL &= ~RCU_CTL_IRC16MSTB;
    else if ((RCU_CTL & RCU_CTL_IRC16MEN) != 0U)
        RCU_CTL |= RCU_CTL_IRC16MSTB;
    if ((RCU_CTL & RCU_CTL_HXTALEN) != 0U && !hold_hse)
        RCU_CTL |= RCU_CTL_HXTALSTB;
    else
        RCU_CTL &= ~RCU_CTL_HXTALSTB;
    if ((RCU_CTL & RCU_CTL_PLLEN) != 0U && !hold_pll)
        RCU_CTL |= RCU_CTL_PLLSTB;
    else
        RCU_CTL &= ~RCU_CTL_PLLSTB;
    if ((PMU_CTL & PMU_CTL_HDEN) != 0U)
        PMU_CS |= PMU_CS_HDRF;
    else
        PMU_CS &= ~PMU_CS_HDRF;
    if ((PMU_CTL & PMU_CTL_HDS) != 0U)
        PMU_CS |= PMU_CS_HDSRF;
    else if (!hold_power_release)
        PMU_CS &= ~PMU_CS_HDSRF;
    if (!hold_switch)
        RCU_CFG0 = (RCU_CFG0 & ~(uint32_t)RCU_CFG0_SCSS) |
                   ((RCU_CFG0 & RCU_CFG0_SCS) << 2U);
}
/** \brief Decode observed clock selection and bus dividers, not owned state.
 */
extern "C" uint32_t rcu_clock_freq_get(rcu_clock_freq_enum clock) {
    bool pll = (RCU_CFG0 & RCU_CFG0_SCSS) == RCU_SCSS_PLLP;
    uint32_t core = pll ? 200000000U : 16000000U;
    if (clock == CK_AHB)
        return core;
    if (clock == CK_APB1)
        return (RCU_CFG0 & RCU_CFG0_APB1PSC) == RCU_APB1_CKAHB_DIV4 ? core / 4U
                                                                    : core;
    return (RCU_CFG0 & RCU_CFG0_APB2PSC) == RCU_APB2_CKAHB_DIV2 ? core / 2U
                                                                : core;
}
/** \brief Keep the official SDK helper as an externally mocked register write.
 */
extern "C" void fmc_wscnt_set(uint32_t states) {
    hardware_port->Write(&FMC_WS, (FMC_WS & ~(uint32_t)FMC_WC_WSCNT) | states);
}
/** \brief Model the SDK timer multiplier's actual clock register field. */
extern "C" void rcu_timer_clock_prescaler_config(uint32_t divider) {
    RCU_CFG1 = (RCU_CFG1 & ~(uint32_t)RCU_CFG1_TIMERSEL) | divider;
}
/** \brief Model the official SDK clock enable encoding. */
extern "C" void rcu_periph_clock_enable(rcu_periph_enum peripheral) {
    hardware_port->Write(&REG32(RCU + ((uint32_t)peripheral >> 6U)),
                         REG32(RCU + ((uint32_t)peripheral >> 6U)) |
                             (UINT32_C(1) << ((uint32_t)peripheral & 31U)));
}
/** \brief Query external CPU privilege without inspecting provider state. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Query externally injected exception context. */
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
/** \brief Query the incoming exclusion mask without changing it. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
/** \brief Host register writes are synchronous, with no physical timing proof.
 */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The modeled host has no Cortex-M pipeline. */
extern "C" void nx_arch_isb(void) {
}
