/**
 * \file            test_hal_timer_properties.cpp
 * \brief           HAL Timer Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Timer module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstdint>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_timer.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Timer Property Test Fixture
 */
class HalTimerPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_timer_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_timer_reset_all();
    }

    hal_timer_instance_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_TIMER_MAX - 1);
        return static_cast<hal_timer_instance_t>(dist(rng));
    }

    hal_timer_channel_t randomChannel() {
        std::uniform_int_distribution<int> dist(0, HAL_TIMER_CH_MAX - 1);
        return static_cast<hal_timer_channel_t>(dist(rng));
    }

    uint32_t randomPeriodUs() {
        std::uniform_int_distribution<uint32_t> dist(1, 1000000);
        return dist(rng);
    }

    uint32_t randomFrequency() {
        std::uniform_int_distribution<uint32_t> dist(1, 100000);
        return dist(rng);
    }

    uint16_t randomDutyCycle() {
        std::uniform_int_distribution<uint16_t> dist(0, 10000);
        return dist(rng);
    }

    int randomCallbackCount() {
        std::uniform_int_distribution<int> dist(1, 10);
        return dist(rng);
    }
};

/**
 * \brief           Callback counter for property tests
 */
static int property_callback_counter = 0;

/**
 * \brief           Test callback function
 */
static void property_timer_callback(hal_timer_instance_t instance,
                                    void* context) {
    (void)instance;
    int* counter = static_cast<int*>(context);
    if (counter) {
        (*counter)++;
    }
    property_callback_counter++;
}

/**
 * Feature: phase2-core-platform, Property 10: Timer Periodic Callback
 *
 * *For any* timer in PERIODIC mode, the callback SHALL be invoked repeatedly
 * at the configured period interval.
 *
 * **Validates: Requirements 5.4**
 */
TEST_F(HalTimerPropertyTest, Property10_PeriodicCallback) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();
        property_callback_counter = 0;

        auto instance = randomInstance();
        auto period_us = randomPeriodUs();
        auto num_callbacks = randomCallbackCount();
        int local_counter = 0;

        hal_timer_config_t config = {.period_us = period_us,
                                     .mode = HAL_TIMER_MODE_PERIODIC,
                                     .direction = HAL_TIMER_DIR_UP};

        ASSERT_EQ(HAL_OK, hal_timer_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance;

        ASSERT_EQ(HAL_OK,
                  hal_timer_set_callback(instance, property_timer_callback,
                                         &local_counter))
            << "Iteration " << i << ": set_callback failed";

        ASSERT_EQ(HAL_OK, hal_timer_start(instance))
            << "Iteration " << i << ": start failed";

        /* Simulate multiple period elapsed events */
        for (int j = 0; j < num_callbacks; ++j) {
            EXPECT_TRUE(native_timer_simulate_period_elapsed(instance))
                << "Iteration " << i << ", callback " << j
                << ": simulate failed";

            /* Timer should still be running in PERIODIC mode */
            EXPECT_TRUE(native_timer_is_running(instance))
                << "Iteration " << i << ", callback " << j
                << ": timer stopped unexpectedly";
        }

        /* Verify callback was invoked the expected number of times */
        EXPECT_EQ(num_callbacks, local_counter)
            << "Iteration " << i << ": expected " << num_callbacks
            << " callbacks, got " << local_counter;

        EXPECT_EQ(static_cast<uint32_t>(num_callbacks),
                  native_timer_get_callback_count(instance))
            << "Iteration " << i << ": callback count mismatch";

        hal_timer_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 11: Timer Oneshot Callback
 *
 * *For any* timer in ONESHOT mode, the callback SHALL be invoked exactly once
 * after the configured period.
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(HalTimerPropertyTest, Property11_OneshotCallback) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();
        property_callback_counter = 0;

        auto instance = randomInstance();
        auto period_us = randomPeriodUs();
        int local_counter = 0;

        hal_timer_config_t config = {.period_us = period_us,
                                     .mode = HAL_TIMER_MODE_ONESHOT,
                                     .direction = HAL_TIMER_DIR_UP};

        ASSERT_EQ(HAL_OK, hal_timer_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance;

        ASSERT_EQ(HAL_OK,
                  hal_timer_set_callback(instance, property_timer_callback,
                                         &local_counter))
            << "Iteration " << i << ": set_callback failed";

        ASSERT_EQ(HAL_OK, hal_timer_start(instance))
            << "Iteration " << i << ": start failed";

        /* Verify timer is running before period elapsed */
        EXPECT_TRUE(native_timer_is_running(instance))
            << "Iteration " << i << ": timer should be running";

        /* Simulate first period elapsed - callback should be invoked */
        EXPECT_TRUE(native_timer_simulate_period_elapsed(instance))
            << "Iteration " << i << ": first simulate failed";

        /* Verify callback was invoked exactly once */
        EXPECT_EQ(1, local_counter)
            << "Iteration " << i << ": callback should be invoked exactly once";

        /* Timer should stop after ONESHOT callback */
        EXPECT_FALSE(native_timer_is_running(instance))
            << "Iteration " << i << ": timer should stop after oneshot";

        /* Subsequent simulate calls should fail (timer not running) */
        EXPECT_FALSE(native_timer_simulate_period_elapsed(instance))
            << "Iteration " << i << ": simulate should fail when timer stopped";

        /* Callback count should still be 1 */
        EXPECT_EQ(1, local_counter)
            << "Iteration " << i << ": callback count should remain 1";

        hal_timer_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 12: PWM Duty Cycle Range
 *
 * *For any* duty cycle value from 0 to 10000, the actual duty cycle SHALL be
 * proportional (0% to 100%).
 *
 * **Validates: Requirements 5.7**
 */
TEST_F(HalTimerPropertyTest, Property12_PwmDutyCycleRange) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomInstance();
        auto channel = randomChannel();
        auto frequency = randomFrequency();
        auto duty_cycle = randomDutyCycle();

        hal_pwm_config_t config = {.frequency = frequency,
                                   .duty_cycle = duty_cycle};

        ASSERT_EQ(HAL_OK, hal_pwm_init(instance, channel, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " channel=" << channel;

        /* Verify initial duty cycle is set correctly */
        uint16_t actual_duty = native_pwm_get_duty_cycle(instance, channel);
        EXPECT_EQ(duty_cycle, actual_duty)
            << "Iteration " << i << ": initial duty cycle mismatch. "
            << "Expected " << duty_cycle << ", got " << actual_duty;

        /* Test setting a new random duty cycle */
        auto new_duty_cycle = randomDutyCycle();
        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, new_duty_cycle))
            << "Iteration " << i << ": set_duty failed";

        actual_duty = native_pwm_get_duty_cycle(instance, channel);
        EXPECT_EQ(new_duty_cycle, actual_duty)
            << "Iteration " << i << ": new duty cycle mismatch. "
            << "Expected " << new_duty_cycle << ", got " << actual_duty;

        /* Verify duty cycle is within valid range (0-10000) */
        EXPECT_LE(actual_duty, 10000u)
            << "Iteration " << i << ": duty cycle exceeds maximum";

        /* Test boundary values */
        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, 0))
            << "Iteration " << i << ": set_duty(0) failed";
        EXPECT_EQ(0u, native_pwm_get_duty_cycle(instance, channel))
            << "Iteration " << i << ": 0% duty cycle mismatch";

        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, 10000))
            << "Iteration " << i << ": set_duty(10000) failed";
        EXPECT_EQ(10000u, native_pwm_get_duty_cycle(instance, channel))
            << "Iteration " << i << ": 100% duty cycle mismatch";

        /* Verify invalid duty cycle is rejected */
        EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
                  hal_pwm_set_duty(instance, channel, 10001))
            << "Iteration " << i << ": should reject duty > 10000";
    }
}
