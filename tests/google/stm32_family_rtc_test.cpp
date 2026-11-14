/**
 * \file            stm32_family_rtc_test.cpp
 * \brief           New F4 RTC provenance, optional SRAM and wake ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_rtc_model.h"
#include <gtest/gtest.h>

class FamilyRtc : public testing::Test {
  protected:
    nx_stm32_rtc_state_t state{};
    nx_stm32_rtc_config_t config{};
    nx_rtc_port_t port{&nx_stm32_rtc_ops, &state};
    nx_rtc_standby_t standby{};
    nx_memory_window_lease_t memory{};
    nx_rtc_snapshot_t snapshot{};
    nx_rtc_calendar_t replacement{25U, 1U, 2U, 4U, 3U, 4U, 5U};
    static nx_time_us_t now(void*) {
        return g_f407_rtc_model.now++;
    }
    void SetUp() override {
        nx_f407_rtc_model_reset(true);
        config.clock = {now, nullptr};
        config.timeout_us = 100U;
        config.poll_limit = 32U;
        config.policy = NX_STM32_RTC_ADOPT_RETAINED;
    }
    void TearDown() override {
        auto& model = g_f407_rtc_model;
        model.masked = false;
        model.hold_synchronization = false;
        if (standby.owner != nullptr) {
            model.masked = true;
            EXPECT_EQ(nx_rtc_port_standby_restore(&port, &standby), NX_SUCCESS);
            model.masked = false;
        }
        if (memory.owner != nullptr) {
            EXPECT_EQ(nx_rtc_port_sram_release(&port, &memory, memory.epoch),
                      NX_SUCCESS);
        }
        if (state.opened) {
            EXPECT_EQ(nx_rtc_port_stop(&port), NX_SUCCESS);
            if (state.effects != 0U) {
                nx_rtc_backup_handoff_t handoff{};
                EXPECT_EQ(nx_rtc_port_backup_handoff(&port, &handoff),
                          NX_SUCCESS);
            }
            EXPECT_EQ(nx_stm32_rtc_close(&state), NX_SUCCESS);
        }
    }
};

TEST_F(FamilyRtc, RetainedCalendarAndBackupRegistersAreNotReprovisioned) {
    const auto old_time = g_f407_rtc_model.rtc.TR;
    const auto old_date = g_f407_rtc_model.rtc.DR;
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_read(&port, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.calendar.year, 99U);
    ASSERT_EQ(nx_rtc_port_backup_write(&port, 19U, 0x12345678U), NX_SUCCESS);
    uint32_t word{};
    ASSERT_EQ(nx_rtc_port_backup_read(&port, 19U, &word), NX_SUCCESS);
    EXPECT_EQ(word, 0x12345678U);
    EXPECT_EQ(g_f407_rtc_model.rtc.TR, old_time);
    EXPECT_EQ(g_f407_rtc_model.rtc.DR, old_date);
    EXPECT_EQ(g_f407_rtc_model.backup_resets, 0U);
}

TEST_F(FamilyRtc, CalendarReplacementSynchronizesTheRealInitCrossing) {
    config.allow_set = true;
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    g_f407_rtc_model.synchronization_required = true;
    ASSERT_EQ(nx_rtc_port_set(&port, &replacement), NX_SUCCESS);
    EXPECT_EQ(g_f407_rtc_model.unsynchronized_init, 0U);
    EXPECT_GT(g_f407_rtc_model.synchronizations, 0U);
    ASSERT_EQ(nx_rtc_port_read(&port, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.calendar.year, replacement.year);
}

TEST_F(FamilyRtc, UnavailableTamperTwoCannotBecomeAWakePublisher) {
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(nx_rtc_port_wake_route(&port, NX_RTC_EVENT_TAMPER2),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_f407_rtc_model.writes, writes);
    EXPECT_EQ(g_f407_rtc_model.exti.EMR & (1U << 21U), 0U);
}

TEST_F(FamilyRtc, SuspendedWakeReceiptRetainsPendingRtcEvidence) {
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    nx_rtc_wakeup_t wake{true, NX_RTC_WAKE_DIV16, 1U};
    ASSERT_EQ(nx_rtc_port_wakeup(&port, &wake), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_wake_route(&port, NX_RTC_EVENT_WAKEUP), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_rtc_suspend(&state), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_standby_prepare(&port, 0U, 16U, &standby),
              NX_SUCCESS);
    auto& model = g_f407_rtc_model;
    model.masked = true;
    ASSERT_EQ(nx_rtc_port_standby_validate(&port, &standby), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_standby_commit(&port, &standby), NX_SUCCESS);
    model.rtc.ISR |= RTC_ISR_WUTF;
    model.exti.PR |= 1U << 22U;
    ASSERT_EQ(nx_rtc_port_standby_restore(&port, &standby), NX_SUCCESS);
    model.masked = false;
    ASSERT_EQ(nx_stm32_rtc_resume(&state), NX_SUCCESS);
    EXPECT_NE(model.rtc.ISR & RTC_ISR_WUTF, 0U);
    EXPECT_NE(model.exti.PR & (1U << 22U), 0U);
    EXPECT_EQ(model.backup_resets, 0U);
}

#if defined(STM32F411xE)
TEST_F(FamilyRtc, AbsentBackupSramRejectsWithoutChangingPowerOrGates) {
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(nx_rtc_port_sram_prepare(&port, true), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_rtc_port_sram_acquire(&port, &memory), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    EXPECT_EQ(memory.owner, nullptr);
    EXPECT_EQ(state.sram_effects, 0U);
}
#else
TEST_F(FamilyRtc, BackupRegulatorControlPreservesForeignUnderdriveFlags) {
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    auto& model = g_f407_rtc_model;
    model.power.CSR |= PWR_CSR_UDRDY;
    ASSERT_EQ(nx_rtc_port_sram_prepare(&port, true), NX_SUCCESS);
    EXPECT_EQ(model.power.CSR & PWR_CSR_UDRDY,
              static_cast<uint32_t>(PWR_CSR_UDRDY));
    ASSERT_EQ(nx_rtc_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(model.power.CSR & PWR_CSR_UDRDY,
              static_cast<uint32_t>(PWR_CSR_UDRDY));
}

TEST_F(FamilyRtc, BackupSramIsAnExplicitReleasableRetainedLease) {
    ASSERT_EQ(nx_stm32_rtc_prepare(&state, &config), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_sram_prepare(&port, true), NX_SUCCESS);
    ASSERT_EQ(nx_rtc_port_sram_acquire(&port, &memory), NX_SUCCESS);
    EXPECT_EQ(memory.bytes, 4096U);
    EXPECT_EQ(memory.address,
              reinterpret_cast<uintptr_t>(g_f407_rtc_model.sram));
    EXPECT_NE(g_f407_rtc_model.power.CSR & PWR_CSR_BRE, 0U);
    EXPECT_EQ(nx_stm32_rtc_suspend(&state), NX_ERROR_BUSY);
}
#endif
