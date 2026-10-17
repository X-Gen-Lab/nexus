/**
 * \file            legacy_devices_test.cpp
 * \brief           Production legacy device rejection and pulse contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "legacy_devices.h"
#include "nexus/io/native/model.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
static nx_legacy_flash_state_t* flash_port;
static volatile uint32_t* ignored;
static bool hold_pulse;
static bool refuse_erase_start, suppress_end, refuse_later_clear;
static unsigned pulses, polls;
static unsigned settle_after;
static bool pulse_error, refuse_relock, corrupt_erase;
static unsigned advance_on_relock;
static nx_legacy_adc_state_t* adc_port;
static nx_legacy_pwm_state_t* pwm_port;
static bool hold_calibration, hold_conversion, hold_update;
static uint32_t adc_value;
extern "C" uint32_t nx_legacy_model_read(volatile uint32_t* r) {
    return *r;
}
extern "C" void nx_legacy_model_write(volatile uint32_t* r, uint32_t v) {
    if (r == ignored)
        return;
    if (refuse_later_clear && flash_port != nullptr && pulses != 0U &&
        r == &flash_port->registers->status)
        return;
    if (refuse_erase_start && flash_port != nullptr &&
        r == &flash_port->registers->control && (v & 64U) != 0U)
        return;
    if (flash_port != nullptr && r == &flash_port->registers->control &&
        (v & 128U) != 0U && pulses != 0U && refuse_relock)
        return;
    if (flash_port != nullptr && r == &flash_port->registers->status) {
        *r &= ~v;
    } else if (flash_port != nullptr && r == &flash_port->registers->key) {
        *r = v;
        if (v == 0xcdef89abU)
            flash_port->registers->control &= ~128U;
    } else {
        *r = v;
        if (flash_port != nullptr && r == &flash_port->registers->control &&
            (v & 128U) != 0U)
            nx_native_clock_advance(advance_on_relock);
        if (pwm_port != nullptr && r == pwm_port->pin_set)
            *pwm_port->pin_latch =
                (*pwm_port->pin_latch | (v & 0xffffU)) & ~(v >> 16U);
        if (pwm_port != nullptr && r == &pwm_port->registers->event &&
            (v & 1U) != 0U && !hold_update)
            pwm_port->registers->status |= 1U;
        if (flash_port != nullptr && r == &flash_port->registers->control &&
            (v & 64U) != 0U) {
            flash_port->registers->status |= 1U;
            ++pulses;
        }
    }
}
extern "C" void nx_legacy_model_poll(unsigned kind, void* context) {
    ++polls;
    nx_native_clock_advance(1U);
    if (kind == 0U && !hold_pulse && polls >= settle_after) {
        auto* p = static_cast<nx_legacy_flash_state_t*>(context);
        if ((p->registers->status & 1U) != 0U) {
            if ((p->registers->control & 2U) != 0U && !corrupt_erase) {
                auto offset = p->registers->address - p->geometry->base_address;
                std::memset(const_cast<uint8_t*>(p->memory) + offset, 0xff,
                            p->geometry->sectors[0].size);
            }
            p->registers->status =
                (suppress_end ? 0U : 32U) | (pulse_error ? 4U : 0U);
        }
    }
    if (kind == 1U) {
        auto* p = static_cast<nx_legacy_adc_state_t*>(context);
        if (!hold_calibration)
            p->registers->control2 &= ~12U;
        if ((p->registers->control2 & (1U << 22U)) != 0U && !hold_conversion) {
            p->registers->control2 &= ~(1U << 22U);
            p->registers->data = adc_value;
            p->registers->status |= 2U;
        }
    }
}
extern "C" void nx_legacy_model_program(volatile uint16_t* a, uint16_t v) {
    *a = v;
    ++pulses;
    flash_port->registers->status |= 1U;
}
class LegacyFlash : public testing::Test {
  protected:
    nx_legacy_flash_registers_t registers{};
    alignas(4) std::array<uint8_t, 4096> memory;
    const nx_flash_sector_t sectors[4]{
        {0U, 1024U}, {1024U, 1024U}, {2048U, 1024U}, {3072U, 1024U}};
    const nx_flash_geometry_t geometry{0x08000000U, 4096U, 2U, sectors, 4U, 0U};
    nx_legacy_flash_state_t state{};
    const nx_flash_port_t face{&nx_legacy_flash_ops, &state};
    void SetUp() override {
        nx_native_clock_configure(true, 0U);
        memory.fill(0xffU);
        state.registers = &registers;
        state.memory = memory.data();
        state.geometry = &geometry;
        state.supply_mv = 3300U;
        registers.control = 128U;
        flash_port = &state;
        adc_port = nullptr;
        pwm_port = nullptr;
        ignored = nullptr;
        hold_pulse = false;
        refuse_erase_start = suppress_end = refuse_later_clear = false;
        pulses = polls = 0U;
        settle_after = 0U;
        pulse_error = refuse_relock = corrupt_erase = false;
        advance_on_relock = 0U;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_legacy_flash_initialize(&state));
    }
};
TEST_F(LegacyFlash, PhysicalGeometryBindingDoesNotEraseOrUnlock) {
    Start();
    EXPECT_EQ(&geometry, nx_flash_port_geometry(&face));
    EXPECT_EQ(128U, registers.control);
    EXPECT_EQ(0U, pulses);
}
class LegacyAdc : public testing::Test {
  protected:
    nx_legacy_adc_registers_t registers{};
    uint32_t gate = 1U, divider = 0U, mode = 0x44444444U, lock = 0U;
    volatile bool started = true;
    nx_legacy_adc_state_t state{};
    const nx_adc_port_t face{&nx_legacy_adc_ops, &state};
    void SetUp() override {
        nx_native_clock_configure(true, 0U);
        flash_port = nullptr;
        pwm_port = nullptr;
        adc_port = &state;
        state.registers = &registers;
        state.gate = &gate;
        state.gate_mask = 1U;
        state.divider = &divider;
        state.divider_mask = 0xc000U;
        state.pin_mode = &mode;
        state.pin_lock = &lock;
        state.clock_started = &started;
        state.reference_mv = 3300U;
        state.poll_limit = 20U;
        ignored = nullptr;
        polls = 0U;
        state.startup_delay_us = 4U;
        hold_calibration = hold_conversion = false;
        adc_value = 1234U;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_legacy_adc_initialize(&state));
    }
};
TEST_F(LegacyAdc, CalibrationKeepsPowerAndUsesLongSampleAtFourMHz) {
    Start();
    EXPECT_EQ(0U, mode & 15U);
    EXPECT_EQ(0x44444440U, mode);
    EXPECT_EQ(1U, registers.control2);
    EXPECT_EQ(7U, registers.sample2 & 7U);
    EXPECT_GE(nx_time_now_us(), 4U);
    EXPECT_TRUE(state.retained);
    nx_adc_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_info(&face, &info));
    EXPECT_EQ(12U, info.resolution_bits);
    EXPECT_EQ(1U, info.channel_count);
    nx_adc_sequence_info_t sequence{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sequence_info(&face, &sequence));
    EXPECT_EQ(4000000U, sequence.adc_clock_hz);
    EXPECT_EQ(253U, sequence.scan_cycles);
}
TEST_F(LegacyAdc, ForeignerAndMissingClockRejectBeforePinChange) {
    registers.control2 = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_legacy_adc_initialize(&state));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(0x44444444U, mode);
    registers.control2 = 0U;
    divider = 0x4000U;
    EXPECT_EQ(NX_ERROR_STATE, nx_legacy_adc_initialize(&state));
    divider = 0U;
    gate = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_legacy_adc_initialize(&state));
}
TEST_F(LegacyAdc, RefusedCalibrationRetainsUntilPowerOffProved) {
    hold_calibration = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_legacy_adc_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    ignored = &registers.control2;
    EXPECT_EQ(NX_ERROR_IO, nx_legacy_adc_close(&state));
    EXPECT_TRUE(state.retained);
    ignored = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_legacy_adc_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(LegacyAdc, OneShotReturnsCountAndTimeoutLeavesOutputUntouched) {
    Start();
    uint16_t samples[2]{0xaaaaU, 0xbbbbU};
    size_t count = 99U;
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, samples, 2U, 100U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(1234U, samples[0]);
    EXPECT_EQ(0xbbbbU, samples[1]);
    samples[0] = 0xaaaaU;
    hold_conversion = true;
    EXPECT_EQ(
        NX_ERROR_TIMEOUT,
        nx_adc_port_sample(&face, samples, 2U, nx_time_now_us() + 3U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0xaaaaU, samples[0]);
    EXPECT_EQ(0U, registers.control2 & (1U << 22U));
}
TEST_F(LegacyAdc, AliasedOutputAndNeverDeadlineCannotStartConversion) {
    Start();
    uint16_t sample = 0U;
    size_t count = 17U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_adc_port_sample(&face, reinterpret_cast<uint16_t*>(&state), 1U,
                                 100U, &count));
    EXPECT_EQ(17U, count);
    EXPECT_EQ(
        NX_ERROR_UNSUPPORTED,
        nx_adc_port_sample(&face, &sample, 1U, NX_DEADLINE_NEVER, &count));
    EXPECT_EQ(1U, registers.control2);
}
TEST_F(LegacyAdc, Gd303ErratumDelayRetainsMillisecondMarginBeforeCalibration) {
    state.startup_delay_us = 2000U;
    state.poll_limit = 4000U;
    Start();
    EXPECT_GE(nx_time_now_us(), 2000U);
    EXPECT_EQ(1U, registers.control2);
}
class LegacyPwm : public testing::Test {
  protected:
    nx_legacy_pwm_registers_t registers{};
    uint32_t gate = 2U, mode = 0x44444444U, lock = 0U, latch = 0U, set = 0U;
    volatile bool started = true;
    nx_legacy_pwm_state_t state{};
    const nx_pwm_port_t face{&nx_legacy_pwm_ops, &state};
    void SetUp() override {
        nx_native_clock_configure(true, 0U);
        flash_port = nullptr;
        adc_port = nullptr;
        pwm_port = &state;
        state.registers = &registers;
        state.gate = &gate;
        state.gate_mask = 2U;
        state.pin_mode = &mode;
        state.pin_lock = &lock;
        state.pin_latch = &latch;
        state.pin_set = &set;
        state.clock_started = &started;
        state.tick_hz = 1000000U;
        state.period_ticks = 1000U;
        state.duty_ticks = 100U;
        state.inactive = 0U;
        state.poll_limit = 5U;
        ignored = nullptr;
        hold_update = false;
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_legacy_pwm_initialize(&state));
    }
};
TEST_F(LegacyPwm, FixedBaseStartsAfterSafeIdleAndStopsAtDeclaredLevel) {
    Start();
    EXPECT_EQ(2U, (mode >> 24U) & 15U);
    EXPECT_EQ(7U, registers.prescaler);
    EXPECT_EQ(999U, registers.reload);
    EXPECT_EQ(0U, registers.enable);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_EQ(10U, (mode >> 24U) & 15U);
    EXPECT_EQ(1U, registers.enable);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 0U));
    EXPECT_EQ(0U, registers.compare1);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 1000U));
    EXPECT_EQ(1000U, registers.compare1);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(0U, registers.enable);
    EXPECT_EQ(0U, latch & 64U);
    EXPECT_EQ(2U, (mode >> 24U) & 15U);
}
TEST_F(LegacyPwm, ForeignTimerAndNonExactDividerRejectBeforeEffects) {
    registers.control2 = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_legacy_pwm_initialize(&state));
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(0x44444444U, mode);
    registers.control2 = 0U;
    state.tick_hz = 3000000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_legacy_pwm_initialize(&state));
}
TEST_F(LegacyPwm, RefusedUpdateAndInactiveLatchKeepCleanupOwner) {
    hold_update = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_legacy_pwm_initialize(&state));
    EXPECT_TRUE(state.retained);
    hold_update = false;
    ignored = &registers.control1;
    EXPECT_EQ(NX_ERROR_IO, nx_legacy_pwm_close(&state));
    EXPECT_TRUE(state.retained);
    ignored = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_legacy_pwm_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(LegacyPwm, SharedBaseCannotBeChangedBySetAndQueryIsImmutable) {
    Start();
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_port_set(&face, 500U, 100U));
    nx_pwm_state_t observed{};
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_state(&face, &observed));
    EXPECT_EQ(1000U, observed.period_ticks);
    EXPECT_EQ(100U, observed.duty_ticks);
    EXPECT_FALSE(observed.running);
}
TEST_F(LegacyPwm, StateOutputCannotOverlapItsPrivateContext) {
    Start();
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_pwm_port_state(&face, reinterpret_cast<nx_pwm_state_t*>(&state)));
}
TEST_F(LegacyPwm, FailedTimerWithdrawalStillDetachesPinAtBoardInactiveLevel) {
    state.inactive = 64U;
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    ignored = &registers.enable;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_port_stop(&face));
    EXPECT_EQ(2U, (mode >> 24U) & 15U);
    EXPECT_EQ(64U, latch & 64U);
    EXPECT_EQ(0U, registers.control1 & 1U);
    EXPECT_TRUE(state.retained);
    ignored = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_legacy_pwm_close(&state));
}
TEST_F(LegacyFlash, HalfWordProgramAndWholePageEraseSettleAndRelock) {
    Start();
    const uint8_t bytes[]{0x34U, 0x12U, 0x78U, 0x56U};
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 1024U, bytes, 4U, 100U));
    EXPECT_EQ(0x34U, memory[1024]);
    EXPECT_EQ(0x56U, memory[1027]);
    EXPECT_EQ(128U, registers.control);
    EXPECT_EQ(2U, pulses);
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_erase(&face, 1024U, 1024U, 100U));
    EXPECT_EQ(0xffU, memory[1024]);
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, InvalidRangeAlignmentAndRegionCannotPulse) {
    Start();
    const uint8_t bytes[4]{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 1U, bytes, 2U, 100U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 4094U, bytes, 4U, 100U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, 1U, 1024U, 100U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, 0U, 1025U, 100U));
    nx_flash_region_t region{&face, 1024U, 1024U, false};
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_flash_region_program(&region, 0U, bytes, 2U, 100U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(0U, registers.key);
}
TEST_F(LegacyFlash, NonErasedHalfwordRejectsExceptAllZero) {
    Start();
    memory[0] = 0xf0U;
    memory[1] = 0xffU;
    const uint8_t bytes[]{0xe0U, 0xffU};
    const uint8_t zero[2]{};
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0U, bytes, 2U, 100U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 0U, zero, 2U, 100U));
    EXPECT_EQ(1U, pulses);
}
TEST_F(LegacyFlash, DeadlineNeverAndExpiredRejectBeforeUnlock) {
    Start();
    const uint8_t zero[2]{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 0U, zero, 2U, NX_DEADLINE_NEVER));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_erase(&face, 0U, 1024U, 0U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(0U, registers.key);
}
TEST_F(LegacyFlash, RefusedRelockRetainsFaultForExplicitCloseRetry) {
    Start();
    const uint8_t zero[2]{};
    refuse_relock = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0U, zero, 2U, 100U));
    EXPECT_TRUE(state.fault);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(0U, registers.status & 1U);
    refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_legacy_flash_close(&state));
    EXPECT_FALSE(state.initialized);
}
TEST_F(LegacyFlash, ExpiredDeadlineStillWaitsForCurrentPulseAndStopsNext) {
    Start();
    const uint8_t zero[4]{};
    settle_after = 8U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_program(&face, 0U, zero, 4U, 3U));
    EXPECT_EQ(1U, pulses);
    EXPECT_GE(polls, 8U);
    EXPECT_EQ(0U, registers.status & 1U);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(128U, registers.control);
    EXPECT_EQ(0xffU, memory[2]);
}
TEST_F(LegacyFlash, HardwareErrorIsReportedAfterSettlingAndRelocking) {
    Start();
    const uint8_t zero[2]{};
    pulse_error = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0U, zero, 2U, 100U));
    EXPECT_EQ(0U, registers.status & 1U);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, IncomingMaskAndBusyPulseRejectWithoutUnlock) {
    Start();
    const uint8_t zero[2]{};
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_flash_port_program(&face, 0U, zero, 2U, 100U));
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(saved);
    registers.status = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_program(&face, 0U, zero, 2U, 100U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_legacy_flash_close(&state));
    EXPECT_EQ(0U, registers.key);
    EXPECT_EQ(0U, pulses);
}
TEST_F(LegacyFlash, UnownedRejectedControllerIsNeverReprogrammedByCleanup) {
    registers.control = 16U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_legacy_flash_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_legacy_flash_close(&state));
    EXPECT_EQ(16U, registers.control);
    EXPECT_EQ(0U, registers.key);
}
TEST_F(LegacyFlash, EraseSuccessRequiresPhysicalReadbackOfItsWholePage) {
    Start();
    memory[1023] = 0U;
    corrupt_erase = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 1024U, 10000U));
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, DeadlineIncludesRelockBeforeReturningSuccess) {
    Start();
    const uint8_t zero[2]{};
    advance_on_relock = 5U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_program(&face, 0U, zero, 2U, 4U));
    EXPECT_EQ(128U, registers.control);
    EXPECT_FALSE(state.active);
}

TEST_F(LegacyFlash, AllErasedInputDoesNotConsumeAProgramPulse) {
    Start();
    const uint16_t erased[2]{UINT16_MAX, UINT16_MAX};
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, erased, sizeof(erased), 1000U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(0U, geometry.program_flags);
    EXPECT_EQ(0xffU, memory[0]);
}

TEST_F(LegacyFlash, IndependentRefusedStartWithPriorEopNeverSucceeds) {
    Start();
    registers.status = 32U;
    refuse_erase_start = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 1024U, 100U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, IndependentIdleWithoutEopNeverSucceeds) {
    Start();
    suppress_end = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 1024U, 100U));
    EXPECT_EQ(1U, pulses);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, IndependentStaleEopAndRefusedStartCannotClaimErase) {
    Start();
    registers.status = 32U;
    ignored = &registers.status;
    refuse_erase_start = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 1024U, 100U));
    EXPECT_EQ(0U, pulses);
}

TEST_F(LegacyFlash, RefusedStaleFlagClearPreventsAnyProgramPulse) {
    Start();
    registers.status = 32U;
    ignored = &registers.status;
    const uint16_t zero = 0U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_program(&face, 0U, &zero, sizeof(zero), 100U));
    EXPECT_EQ(0U, pulses);
    EXPECT_EQ(128U, registers.control);
    EXPECT_EQ(0xffU, memory[0]);
}

TEST_F(LegacyFlash, EveryHalfwordNeedsFreshCompletionAfterProvenFlagClear) {
    Start();
    refuse_later_clear = true;
    const uint16_t zero[2]{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_program(&face, 0U, zero, sizeof(zero), 100U));
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(0xffU, memory[2]);
    EXPECT_EQ(128U, registers.control);
}
TEST_F(LegacyFlash, EveryPageNeedsFreshCompletionAfterProvenFlagClear) {
    Start();
    refuse_later_clear = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 2048U, 100U));
    EXPECT_EQ(1U, pulses);
    EXPECT_EQ(128U, registers.control);
}

class PwmStateDispatch : public testing::Test {
  protected:
    testing::MockFunction<nx_result_t(nx_pwm_state_t*)> provider;
    nx_pwm_ops_t methods{nullptr, nullptr, nullptr,
                         [](const void* context, nx_pwm_state_t* output) {
                             return static_cast<PwmStateDispatch*>(
                                        const_cast<void*>(context))
                                 ->provider.Call(output);
                         }};
    const nx_pwm_state_t expected{1000U, 250U, 1000000U, true};
    nx_pwm_port_t face{&methods, this};

    void SetUp() override {
        ON_CALL(provider, Call(testing::_))
            .WillByDefault([this](nx_pwm_state_t* output) {
                /* Copy bytes into valid writable enclosing storage even when
                 * an incorrect dispatcher admits an overlapping output. */
                std::memcpy(output, &expected, sizeof(expected));
                return NX_SUCCESS;
            });
    }
    void ExpectNormalDispatch() {
        nx_pwm_state_t output{};
        EXPECT_CALL(provider, Call(&output)).Times(1);
        EXPECT_EQ(NX_SUCCESS, nx_pwm_port_state(&face, &output));
        EXPECT_EQ(expected.period_ticks, output.period_ticks);
        EXPECT_EQ(expected.duty_ticks, output.duty_ticks);
        EXPECT_EQ(expected.tick_hz, output.tick_hz);
        EXPECT_EQ(expected.running, output.running);
        EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&provider));
    }
};

TEST_F(PwmStateDispatch, StateOutputCannotOverwriteItsCurrentInterfaceFace) {
    ExpectNormalDispatch();
    struct alignas(nx_pwm_state_t) Storage {
        nx_pwm_port_t face;
        std::array<uint8_t, sizeof(nx_pwm_state_t)> tail;
    } storage{face, {}};
    std::array<uint8_t, sizeof(storage)> before{};
    std::memcpy(before.data(), &storage, before.size());
    EXPECT_CALL(provider, Call(testing::_)).Times(0);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_pwm_port_state(&storage.face,
                          reinterpret_cast<nx_pwm_state_t*>(&storage.face)));
    EXPECT_EQ(0, std::memcmp(before.data(), &storage, before.size()));
}

TEST_F(PwmStateDispatch, StateOutputCannotOverwriteItsCurrentReadonlyMethods) {
    ExpectNormalDispatch();
    struct alignas(nx_pwm_state_t) Storage {
        nx_pwm_ops_t methods;
        std::array<uint8_t, sizeof(nx_pwm_state_t)> tail;
    } storage{methods, {}};
    const nx_pwm_port_t observed{&storage.methods, this};
    std::array<uint8_t, sizeof(storage)> before{};
    std::memcpy(before.data(), &storage, before.size());
    EXPECT_CALL(provider, Call(testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pwm_port_state(&observed, reinterpret_cast<nx_pwm_state_t*>(
                                               &storage.methods)));
    EXPECT_EQ(0, std::memcmp(before.data(), &storage, before.size()));
}
