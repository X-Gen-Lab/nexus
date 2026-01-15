/**
 * \file            test_nx_timer.cpp
 * \brief           Nexus Timer Interface Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for the new nx_timer_t interface.
 * Tests timer initialization, start/stop, PWM operations, and lifecycle.
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_timer.h"
#include "hal/nx_status.h"

/* Native platform factory function */
nx_timer_t* nx_timer_native_get(uint8_t timer_index);
nx_timer_t* nx_timer_native_get_with_config(uint8_t timer_index,
                                            const nx_timer_config_t* cfg);
}

/**
 * \brief           Timer Test Fixture
 */
class NxTimerTest : public ::testing::Test {
  protected:
    nx_timer_t* timer;

    void SetUp() override {
        timer = nullptr;
    }

    void TearDown() override {
        if (timer) {
            nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }
};

/**
 * \brief           Test timer instance creation
 */
TEST_F(NxTimerTest, GetTimerInstance) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);
    EXPECT_NE(nullptr, timer->start);
    EXPECT_NE(nullptr, timer->stop);
    EXPECT_NE(nullptr, timer->get_lifecycle);
}

/**
 * \brief           Test timer lifecycle initialization
 */
TEST_F(NxTimerTest, LifecycleInit) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_INITIALIZED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test timer double initialization
 */
TEST_F(NxTimerTest, DoubleInit) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

/**
 * \brief           Test timer start and stop
 */
TEST_F(NxTimerTest, StartStop) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Start timer */
    EXPECT_EQ(NX_OK, timer->start(timer));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Stop timer */
    EXPECT_EQ(NX_OK, timer->stop(timer));
    EXPECT_EQ(NX_DEV_STATE_INITIALIZED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test timer operations on uninitialized timer
 */
TEST_F(NxTimerTest, OperationsOnUninitializedTimer) {
    timer = nx_timer_native_get(1);
    ASSERT_NE(nullptr, timer);

    EXPECT_EQ(NX_ERR_NOT_INIT, timer->start(timer));
    EXPECT_EQ(NX_ERR_NOT_INIT, timer->stop(timer));
    EXPECT_EQ(NX_ERR_NOT_INIT, timer->reset(timer));
    EXPECT_EQ(NX_ERR_NOT_INIT, timer->set_counter(timer, 100));
}

/**
 * \brief           Test timer counter operations
 */
TEST_F(NxTimerTest, CounterOperations) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set counter value */
    EXPECT_EQ(NX_OK, timer->set_counter(timer, 12345));

    /* Get counter value */
    uint32_t counter = timer->get_counter(timer);
    EXPECT_EQ(12345u, counter);

    /* Reset counter */
    EXPECT_EQ(NX_OK, timer->reset(timer));
    counter = timer->get_counter(timer);
    EXPECT_EQ(0u, counter);
}

/**
 * \brief           Test timer callback registration
 */
static int callback_count = 0;
static void* callback_context = nullptr;

static void test_timer_callback(void* context) {
    callback_count++;
    callback_context = context;
}

TEST_F(NxTimerTest, CallbackRegistration) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    callback_count = 0;
    callback_context = nullptr;
    int test_data = 42;

    /* Set callback */
    EXPECT_EQ(NX_OK,
              timer->set_callback(timer, test_timer_callback, &test_data));

    /* Clear callback */
    EXPECT_EQ(NX_OK, timer->clear_callback(timer));
}

/**
 * \brief           Test timer configuration get/set
 */
TEST_F(NxTimerTest, ConfigurationGetSet) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set configuration */
    nx_timer_config_t config = {.mode = NX_TIMER_MODE_ONE_SHOT,
                                .frequency_hz = 2000,
                                .period_us = 500,
                                .auto_reload = false,
                                .prescaler = 10};
    EXPECT_EQ(NX_OK, timer->set_config(timer, &config));

    /* Get configuration */
    nx_timer_config_t read_config = {};
    EXPECT_EQ(NX_OK, timer->get_config(timer, &read_config));
    EXPECT_EQ(NX_TIMER_MODE_ONE_SHOT, read_config.mode);
    EXPECT_EQ(2000u, read_config.frequency_hz);
    EXPECT_EQ(500u, read_config.period_us);
    EXPECT_FALSE(read_config.auto_reload);
    EXPECT_EQ(10u, read_config.prescaler);
}

/**
 * \brief           Test timer frequency and period setting
 */
TEST_F(NxTimerTest, FrequencyAndPeriod) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set frequency */
    EXPECT_EQ(NX_OK, timer->set_frequency(timer, 5000));

    /* Set period */
    EXPECT_EQ(NX_OK, timer->set_period(timer, 2000));

    /* Verify via get_config */
    nx_timer_config_t config = {};
    EXPECT_EQ(NX_OK, timer->get_config(timer, &config));
    EXPECT_EQ(5000u, config.frequency_hz);
    EXPECT_EQ(2000u, config.period_us);
}

/**
 * \brief           Test PWM start and stop
 */
TEST_F(NxTimerTest, PwmStartStop) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Start PWM on channel 0 */
    EXPECT_EQ(NX_OK, timer->pwm_start(timer, 0));

    /* Stop PWM on channel 0 */
    EXPECT_EQ(NX_OK, timer->pwm_stop(timer, 0));
}

/**
 * \brief           Test PWM invalid channel
 */
TEST_F(NxTimerTest, PwmInvalidChannel) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Invalid channel (>= 4) */
    EXPECT_EQ(NX_ERR_INVALID_PARAM, timer->pwm_start(timer, 10));
    EXPECT_EQ(NX_ERR_INVALID_PARAM, timer->pwm_stop(timer, 10));
}

/**
 * \brief           Test PWM duty cycle setting
 */
TEST_F(NxTimerTest, PwmDutyCycle) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set duty cycle to 0% */
    EXPECT_EQ(NX_OK, timer->pwm_set_duty_cycle(timer, 0, 0));

    /* Set duty cycle to 50% */
    EXPECT_EQ(NX_OK, timer->pwm_set_duty_cycle(timer, 0, 50));

    /* Set duty cycle to 100% */
    EXPECT_EQ(NX_OK, timer->pwm_set_duty_cycle(timer, 0, 100));

    /* Invalid duty cycle (> 100) */
    EXPECT_EQ(NX_ERR_INVALID_PARAM, timer->pwm_set_duty_cycle(timer, 0, 101));
}

/**
 * \brief           Test PWM configuration get/set
 */
TEST_F(NxTimerTest, PwmConfiguration) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set PWM configuration */
    nx_pwm_config_t pwm_config = {.frequency_hz = 10000,
                                  .duty_cycle = 75,
                                  .channel = 1,
                                  .inverted = true};
    EXPECT_EQ(NX_OK, timer->pwm_set_config(timer, 1, &pwm_config));

    /* Get PWM configuration */
    nx_pwm_config_t read_config = {};
    EXPECT_EQ(NX_OK, timer->pwm_get_config(timer, 1, &read_config));
    EXPECT_EQ(10000u, read_config.frequency_hz);
    EXPECT_EQ(75u, read_config.duty_cycle);
    EXPECT_EQ(1u, read_config.channel);
    EXPECT_TRUE(read_config.inverted);
}

/**
 * \brief           Test timer power management
 */
TEST_F(NxTimerTest, PowerManagement) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_power_t* power = timer->get_power(timer);
    ASSERT_NE(nullptr, power);

    /* Power should be enabled after init */
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test timer suspend and resume
 */
TEST_F(NxTimerTest, SuspendResume) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_power_t* power = timer->get_power(timer);
    ASSERT_NE(nullptr, power);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_FALSE(power->is_enabled(power));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test timer statistics
 */
TEST_F(NxTimerTest, Statistics) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get initial stats */
    nx_timer_stats_t stats = {};
    EXPECT_EQ(NX_OK, timer->get_stats(timer, &stats));
    EXPECT_FALSE(stats.running);
    EXPECT_EQ(0u, stats.overflow_count);
    EXPECT_EQ(0u, stats.capture_count);
    EXPECT_EQ(0u, stats.compare_count);

    /* Start timer */
    EXPECT_EQ(NX_OK, timer->start(timer));
    EXPECT_EQ(NX_OK, timer->get_stats(timer, &stats));
    EXPECT_TRUE(stats.running);

    /* Clear stats */
    EXPECT_EQ(NX_OK, timer->clear_stats(timer));
}

/**
 * \brief           Test timer diagnostic interface
 */
TEST_F(NxTimerTest, DiagnosticInterface) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_diagnostic_t* diagnostic = timer->get_diagnostic(timer);
    ASSERT_NE(nullptr, diagnostic);

    /* Get status */
    nx_timer_stats_t stats = {};
    EXPECT_EQ(NX_OK, diagnostic->get_status(diagnostic, &stats, sizeof(stats)));

    /* Get statistics */
    EXPECT_EQ(NX_OK,
              diagnostic->get_statistics(diagnostic, &stats, sizeof(stats)));

    /* Clear statistics */
    EXPECT_EQ(NX_OK, diagnostic->clear_statistics(diagnostic));
}

/**
 * \brief           Test timer deinitialization
 */
TEST_F(NxTimerTest, Deinit) {
    timer = nx_timer_native_get(0);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_INITIALIZED, lifecycle->get_state(lifecycle));

    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test timer with initial configuration
 */
TEST_F(NxTimerTest, GetWithConfig) {
    nx_timer_config_t config = {.mode = NX_TIMER_MODE_PWM,
                                .frequency_hz = 50000,
                                .period_us = 20,
                                .auto_reload = true,
                                .prescaler = 5};

    timer = nx_timer_native_get_with_config(0, &config);
    ASSERT_NE(nullptr, timer);

    nx_lifecycle_t* lifecycle = timer->get_lifecycle(timer);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify configuration was applied */
    nx_timer_config_t read_config = {};
    EXPECT_EQ(NX_OK, timer->get_config(timer, &read_config));
    EXPECT_EQ(NX_TIMER_MODE_PWM, read_config.mode);
    EXPECT_EQ(50000u, read_config.frequency_hz);
    EXPECT_EQ(20u, read_config.period_us);
    EXPECT_TRUE(read_config.auto_reload);
    EXPECT_EQ(5u, read_config.prescaler);
}
