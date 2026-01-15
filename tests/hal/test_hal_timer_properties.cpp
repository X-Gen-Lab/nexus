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

/**
 * Feature: stm32f4-hal-adapter, Property 12: Timer Period Configuration
 *
 * *For any* valid period value, the timer SHALL be configured with the
 * requested period and the configured period SHALL match the requested value.
 *
 * **Validates: Requirements 7.1, 7.9**
 */
TEST_F(HalTimerPropertyTest, Property12_TimerPeriodConfiguration) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomInstance();
        auto period_us = randomPeriodUs();

        hal_timer_config_t config = {.period_us = period_us,
                                     .mode = HAL_TIMER_MODE_PERIODIC,
                                     .direction = HAL_TIMER_DIR_UP};

        /* Initialize timer with random period */
        ASSERT_EQ(HAL_OK, hal_timer_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " period_us=" << period_us;

        /* Verify timer is initialized */
        EXPECT_TRUE(native_timer_is_initialized(instance))
            << "Iteration " << i << ": timer should be initialized";

        /* Verify configured period matches requested period */
        uint32_t actual_period = native_timer_get_period_us(instance);
        EXPECT_EQ(period_us, actual_period)
            << "Iteration " << i << ": period mismatch. "
            << "Expected " << period_us << ", got " << actual_period;

        /* Verify timer mode is correctly set */
        hal_timer_mode_t actual_mode = native_timer_get_mode(instance);
        EXPECT_EQ(HAL_TIMER_MODE_PERIODIC, actual_mode)
            << "Iteration " << i << ": mode mismatch";

        /* Deinitialize and verify */
        ASSERT_EQ(HAL_OK, hal_timer_deinit(instance))
            << "Iteration " << i << ": deinit failed";

        EXPECT_FALSE(native_timer_is_initialized(instance))
            << "Iteration " << i << ": timer should be deinitialized";
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 13: Timer Start/Stop Control
 *
 * *For any* initialized timer, starting SHALL set running state to true,
 * and stopping SHALL set running state to false. The running state SHALL
 * be consistent with the last start/stop operation.
 *
 * **Validates: Requirements 7.2, 7.3**
 */
TEST_F(HalTimerPropertyTest, Property13_TimerStartStopControl) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomInstance();
        auto period_us = randomPeriodUs();

        hal_timer_config_t config = {.period_us = period_us,
                                     .mode = HAL_TIMER_MODE_PERIODIC,
                                     .direction = HAL_TIMER_DIR_UP};

        /* Initialize timer */
        ASSERT_EQ(HAL_OK, hal_timer_init(instance, &config))
            << "Iteration " << i << ": init failed";

        /* Timer should not be running after init */
        EXPECT_FALSE(native_timer_is_running(instance))
            << "Iteration " << i << ": timer should not be running after init";

        /* Start timer */
        ASSERT_EQ(HAL_OK, hal_timer_start(instance))
            << "Iteration " << i << ": start failed";

        /* Timer should be running after start */
        EXPECT_TRUE(native_timer_is_running(instance))
            << "Iteration " << i << ": timer should be running after start";

        /* Stop timer */
        ASSERT_EQ(HAL_OK, hal_timer_stop(instance))
            << "Iteration " << i << ": stop failed";

        /* Timer should not be running after stop */
        EXPECT_FALSE(native_timer_is_running(instance))
            << "Iteration " << i << ": timer should not be running after stop";

        /* Start again to verify multiple start/stop cycles */
        ASSERT_EQ(HAL_OK, hal_timer_start(instance))
            << "Iteration " << i << ": second start failed";

        EXPECT_TRUE(native_timer_is_running(instance))
            << "Iteration " << i
            << ": timer should be running after second start";

        /* Stop again */
        ASSERT_EQ(HAL_OK, hal_timer_stop(instance))
            << "Iteration " << i << ": second stop failed";

        EXPECT_FALSE(native_timer_is_running(instance))
            << "Iteration " << i
            << ": timer should not be running after second stop";

        hal_timer_deinit(instance);
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 14: PWM Duty Cycle Precision
 *
 * *For any* PWM configuration, the duty cycle SHALL be accurately set
 * and retrievable. Setting duty cycle to X SHALL result in reading X back.
 * The duty cycle range is 0-10000 (0.00% to 100.00%).
 *
 * **Validates: Requirements 7.7, 7.8**
 */
TEST_F(HalTimerPropertyTest, Property14_PwmDutyCyclePrecision) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_timer_reset_all();

        auto instance = randomInstance();
        auto channel = randomChannel();
        auto frequency = randomFrequency();
        auto initial_duty = randomDutyCycle();

        hal_pwm_config_t config = {.frequency = frequency,
                                   .duty_cycle = initial_duty};

        /* Initialize PWM */
        ASSERT_EQ(HAL_OK, hal_pwm_init(instance, channel, &config))
            << "Iteration " << i << ": init failed";

        /* Verify PWM is initialized */
        EXPECT_TRUE(native_pwm_is_initialized(instance, channel))
            << "Iteration " << i << ": PWM should be initialized";

        /* Verify initial duty cycle */
        uint16_t actual_duty = native_pwm_get_duty_cycle(instance, channel);
        EXPECT_EQ(initial_duty, actual_duty)
            << "Iteration " << i << ": initial duty cycle mismatch";

        /* Test multiple duty cycle changes */
        for (int j = 0; j < 5; ++j) {
            auto new_duty = randomDutyCycle();

            ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, new_duty))
                << "Iteration " << i << ", change " << j << ": set_duty failed";

            actual_duty = native_pwm_get_duty_cycle(instance, channel);
            EXPECT_EQ(new_duty, actual_duty)
                << "Iteration " << i << ", change " << j
                << ": duty cycle mismatch. Expected " << new_duty << ", got "
                << actual_duty;
        }

        /* Test boundary values for precision */
        /* 0% duty cycle */
        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, 0))
            << "Iteration " << i << ": set_duty(0) failed";
        EXPECT_EQ(0u, native_pwm_get_duty_cycle(instance, channel))
            << "Iteration " << i << ": 0% duty cycle precision error";

        /* 50% duty cycle */
        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, 5000))
            << "Iteration " << i << ": set_duty(5000) failed";
        EXPECT_EQ(5000u, native_pwm_get_duty_cycle(instance, channel))
            << "Iteration " << i << ": 50% duty cycle precision error";

        /* 100% duty cycle */
        ASSERT_EQ(HAL_OK, hal_pwm_set_duty(instance, channel, 10000))
            << "Iteration " << i << ": set_duty(10000) failed";
        EXPECT_EQ(10000u, native_pwm_get_duty_cycle(instance, channel))
            << "Iteration " << i << ": 100% duty cycle precision error";

        /* Verify frequency is preserved */
        uint32_t actual_freq = native_pwm_get_frequency(instance, channel);
        EXPECT_EQ(frequency, actual_freq)
            << "Iteration " << i << ": frequency should be preserved";
    }
}
