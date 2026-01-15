/**
 * \file            test_nx_gpio.cpp
 * \brief           Nexus HAL GPIO Checkpoint Verification Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Checkpoint 6: GPIO Verification
 * - Test nx_gpio_t read/write operations
 * - Test runtime mode switching
 * - Test interrupt callbacks
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_gpio.h"
#include "hal/nx_status.h"

/* Platform-specific factory functions */
nx_gpio_t* nx_gpio_native_get(uint8_t port, uint8_t pin);
nx_gpio_t* nx_gpio_native_get_with_config(uint8_t port, uint8_t pin,
                                          const nx_gpio_config_t* cfg);
void nx_gpio_native_simulate_exti(uint8_t port, uint8_t pin);
}

/**
 * \brief           GPIO Checkpoint Test Fixture
 */
class NxGpioCheckpointTest : public ::testing::Test {
  protected:
    nx_gpio_t* gpio;

    void SetUp() override {
        gpio = nullptr;
    }

    void TearDown() override {
        if (gpio) {
            nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }
};

/**
 * \brief           Test GPIO read/write operations
 * \details         Checkpoint 6: Test nx_gpio_t read/write
 */
TEST_F(NxGpioCheckpointTest, ReadWriteOperations) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(0, 5); /* Port A, Pin 5 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Configure as output */
    nx_gpio_config_t config = {.mode = NX_GPIO_MODE_OUTPUT_PP,
                               .pull = NX_GPIO_PULL_NONE,
                               .speed = NX_GPIO_SPEED_LOW,
                               .af_index = 0};
    EXPECT_EQ(NX_OK, gpio->set_config(gpio, &config));

    /* Test write low */
    gpio->write(gpio, 0);
    EXPECT_EQ(0, gpio->read(gpio));

    /* Test write high */
    gpio->write(gpio, 1);
    EXPECT_EQ(1, gpio->read(gpio));

    /* Test toggle */
    gpio->toggle(gpio);
    EXPECT_EQ(0, gpio->read(gpio));

    gpio->toggle(gpio);
    EXPECT_EQ(1, gpio->read(gpio));
}

/**
 * \brief           Test runtime mode switching
 * \details         Checkpoint 6: Test runtime mode switching
 */
TEST_F(NxGpioCheckpointTest, RuntimeModeSwitching) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(1, 3); /* Port B, Pin 3 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Start with input mode */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_INPUT));

    /* Verify mode was set */
    nx_gpio_config_t config;
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_MODE_INPUT, config.mode);

    /* Switch to output mode */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP));

    /* Verify mode was changed */
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_MODE_OUTPUT_PP, config.mode);

    /* Test write after mode switch */
    gpio->write(gpio, 1);
    EXPECT_EQ(1, gpio->read(gpio));

    /* Switch to open-drain output */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_OD));

    /* Verify mode was changed */
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_MODE_OUTPUT_OD, config.mode);

    /* Switch to analog mode */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_ANALOG));

    /* Verify mode was changed */
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_MODE_ANALOG, config.mode);
}

/**
 * \brief           Test runtime pull configuration switching
 * \details         Checkpoint 6: Test runtime configuration
 */
TEST_F(NxGpioCheckpointTest, RuntimePullSwitching) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(2, 7); /* Port C, Pin 7 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test pull-up */
    EXPECT_EQ(NX_OK, gpio->set_pull(gpio, NX_GPIO_PULL_UP));

    nx_gpio_config_t config;
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_PULL_UP, config.pull);

    /* Test pull-down */
    EXPECT_EQ(NX_OK, gpio->set_pull(gpio, NX_GPIO_PULL_DOWN));

    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_PULL_DOWN, config.pull);

    /* Test no pull */
    EXPECT_EQ(NX_OK, gpio->set_pull(gpio, NX_GPIO_PULL_NONE));

    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_GPIO_PULL_NONE, config.pull);
}

/**
 * \brief           Test complete configuration switching
 * \details         Checkpoint 6: Test runtime configuration
 */
TEST_F(NxGpioCheckpointTest, RuntimeCompleteConfigSwitching) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(3, 12); /* Port D, Pin 12 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set initial configuration */
    nx_gpio_config_t config1 = {.mode = NX_GPIO_MODE_INPUT,
                                .pull = NX_GPIO_PULL_UP,
                                .speed = NX_GPIO_SPEED_LOW,
                                .af_index = 0};
    EXPECT_EQ(NX_OK, gpio->set_config(gpio, &config1));

    /* Verify configuration */
    nx_gpio_config_t read_config;
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &read_config));
    EXPECT_EQ(NX_GPIO_MODE_INPUT, read_config.mode);
    EXPECT_EQ(NX_GPIO_PULL_UP, read_config.pull);
    EXPECT_EQ(NX_GPIO_SPEED_LOW, read_config.speed);

    /* Change to different configuration */
    nx_gpio_config_t config2 = {.mode = NX_GPIO_MODE_OUTPUT_PP,
                                .pull = NX_GPIO_PULL_NONE,
                                .speed = NX_GPIO_SPEED_HIGH,
                                .af_index = 0};
    EXPECT_EQ(NX_OK, gpio->set_config(gpio, &config2));

    /* Verify new configuration */
    EXPECT_EQ(NX_OK, gpio->get_config(gpio, &read_config));
    EXPECT_EQ(NX_GPIO_MODE_OUTPUT_PP, read_config.mode);
    EXPECT_EQ(NX_GPIO_PULL_NONE, read_config.pull);
    EXPECT_EQ(NX_GPIO_SPEED_HIGH, read_config.speed);
}

/* Global variable for interrupt callback testing */
static volatile int g_exti_callback_count = 0;
static volatile void* g_exti_callback_context = nullptr;

/**
 * \brief           EXTI callback for testing
 */
static void test_exti_callback(void* context) {
    g_exti_callback_count++;
    g_exti_callback_context = context;
}

/**
 * \brief           Test interrupt callback registration
 * \details         Checkpoint 6: Test interrupt callbacks
 */
TEST_F(NxGpioCheckpointTest, InterruptCallbackRegistration) {
    /* Reset global state */
    g_exti_callback_count = 0;
    g_exti_callback_context = nullptr;

    /* Get GPIO instance */
    gpio = nx_gpio_native_get(4, 10); /* Port E, Pin 10 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Configure as input for interrupt */
    EXPECT_EQ(NX_OK, gpio->set_mode(gpio, NX_GPIO_MODE_INPUT));

    /* Register interrupt callback */
    int test_context = 0x1234;
    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_RISING,
                                    test_exti_callback, &test_context));

    /* Simulate interrupt trigger */
    nx_gpio_native_simulate_exti(4, 10);

    /* Verify callback was called */
    EXPECT_EQ(1, g_exti_callback_count);
    EXPECT_EQ(&test_context, g_exti_callback_context);

    /* Trigger again */
    nx_gpio_native_simulate_exti(4, 10);
    EXPECT_EQ(2, g_exti_callback_count);

    /* Clear interrupt */
    EXPECT_EQ(NX_OK, gpio->clear_exti(gpio));

    /* Trigger should not call callback anymore */
    nx_gpio_native_simulate_exti(4, 10);
    EXPECT_EQ(2, g_exti_callback_count); /* Count should not increase */
}

/**
 * \brief           Test interrupt callback with different trigger types
 * \details         Checkpoint 6: Test interrupt callbacks
 */
TEST_F(NxGpioCheckpointTest, InterruptCallbackTriggerTypes) {
    /* Reset global state */
    g_exti_callback_count = 0;
    g_exti_callback_context = nullptr;

    /* Get GPIO instance */
    gpio = nx_gpio_native_get(5, 2); /* Port F, Pin 2 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test rising edge trigger */
    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_RISING,
                                    test_exti_callback, nullptr));
    nx_gpio_native_simulate_exti(5, 2);
    EXPECT_EQ(1, g_exti_callback_count);

    /* Clear and test falling edge trigger */
    EXPECT_EQ(NX_OK, gpio->clear_exti(gpio));
    g_exti_callback_count = 0;

    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_FALLING,
                                    test_exti_callback, nullptr));
    nx_gpio_native_simulate_exti(5, 2);
    EXPECT_EQ(1, g_exti_callback_count);

    /* Clear and test both edges trigger */
    EXPECT_EQ(NX_OK, gpio->clear_exti(gpio));
    g_exti_callback_count = 0;

    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_BOTH, test_exti_callback,
                                    nullptr));
    nx_gpio_native_simulate_exti(5, 2);
    EXPECT_EQ(1, g_exti_callback_count);
}

/**
 * \brief           Test interrupt callback error handling
 * \details         Checkpoint 6: Test interrupt callbacks
 */
TEST_F(NxGpioCheckpointTest, InterruptCallbackErrorHandling) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(6, 8); /* Port G, Pin 8 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Try to set EXTI with null callback - should fail */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              gpio->set_exti(gpio, NX_GPIO_EXTI_RISING, nullptr, nullptr));

    /* Try to set EXTI with NONE trigger - should clear EXTI */
    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_RISING,
                                    test_exti_callback, nullptr));
    EXPECT_EQ(NX_OK, gpio->set_exti(gpio, NX_GPIO_EXTI_NONE, test_exti_callback,
                                    nullptr));
}

/**
 * \brief           Test GPIO lifecycle management
 * \details         Checkpoint 6: Verify lifecycle operations
 */
TEST_F(NxGpioCheckpointTest, LifecycleManagement) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(7, 15); /* Port H, Pin 15 */
    ASSERT_NE(nullptr, gpio);

    /* Get lifecycle interface */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);

    /* Check initial state */
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Initialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Try to initialize again - should fail */
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Try to deinitialize again - should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

/**
 * \brief           Test GPIO power management
 * \details         Checkpoint 6: Verify power operations
 */
TEST_F(NxGpioCheckpointTest, PowerManagement) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(0, 1); /* Port A, Pin 1 */
    ASSERT_NE(nullptr, gpio);

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get power interface */
    nx_power_t* power = gpio->get_power(gpio);
    ASSERT_NE(nullptr, power);

    /* Check power is enabled after init */
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test operations on uninitialized GPIO
 * \details         Checkpoint 6: Verify error handling
 */
TEST_F(NxGpioCheckpointTest, UninitializedOperations) {
    /* Get GPIO instance */
    gpio = nx_gpio_native_get(1, 9); /* Port B, Pin 9 */
    ASSERT_NE(nullptr, gpio);

    /* Try operations without initialization - should fail or return 0 */
    EXPECT_EQ(0, gpio->read(gpio));

    /* Write and toggle should not crash but won't do anything */
    gpio->write(gpio, 1);
    gpio->toggle(gpio);

    /* Configuration operations should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP));
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->set_pull(gpio, NX_GPIO_PULL_UP));

    nx_gpio_config_t config;
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->get_config(gpio, &config));
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->set_config(gpio, &config));

    /* EXTI operations should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->set_exti(gpio, NX_GPIO_EXTI_RISING,
                                              test_exti_callback, nullptr));
    EXPECT_EQ(NX_ERR_NOT_INIT, gpio->clear_exti(gpio));
}

/**
 * \brief           Test multiple GPIO instances
 * \details         Checkpoint 6: Verify multiple instances work independently
 */
TEST_F(NxGpioCheckpointTest, MultipleInstances) {
    /* Get multiple GPIO instances */
    nx_gpio_t* gpio1 = nx_gpio_native_get(0, 0); /* Port A, Pin 0 */
    nx_gpio_t* gpio2 = nx_gpio_native_get(0, 1); /* Port A, Pin 1 */
    nx_gpio_t* gpio3 = nx_gpio_native_get(1, 0); /* Port B, Pin 0 */

    ASSERT_NE(nullptr, gpio1);
    ASSERT_NE(nullptr, gpio2);
    ASSERT_NE(nullptr, gpio3);

    /* Initialize all */
    nx_lifecycle_t* lc1 = gpio1->get_lifecycle(gpio1);
    nx_lifecycle_t* lc2 = gpio2->get_lifecycle(gpio2);
    nx_lifecycle_t* lc3 = gpio3->get_lifecycle(gpio3);

    EXPECT_EQ(NX_OK, lc1->init(lc1));
    EXPECT_EQ(NX_OK, lc2->init(lc2));
    EXPECT_EQ(NX_OK, lc3->init(lc3));

    /* Configure as outputs */
    EXPECT_EQ(NX_OK, gpio1->set_mode(gpio1, NX_GPIO_MODE_OUTPUT_PP));
    EXPECT_EQ(NX_OK, gpio2->set_mode(gpio2, NX_GPIO_MODE_OUTPUT_PP));
    EXPECT_EQ(NX_OK, gpio3->set_mode(gpio3, NX_GPIO_MODE_OUTPUT_PP));

    /* Write different values */
    gpio1->write(gpio1, 0);
    gpio2->write(gpio2, 1);
    gpio3->write(gpio3, 0);

    /* Verify independent operation */
    EXPECT_EQ(0, gpio1->read(gpio1));
    EXPECT_EQ(1, gpio2->read(gpio2));
    EXPECT_EQ(0, gpio3->read(gpio3));

    /* Toggle one should not affect others */
    gpio2->toggle(gpio2);

    EXPECT_EQ(0, gpio1->read(gpio1));
    EXPECT_EQ(0, gpio2->read(gpio2));
    EXPECT_EQ(0, gpio3->read(gpio3));

    /* Cleanup */
    lc1->deinit(lc1);
    lc2->deinit(lc2);
    lc3->deinit(lc3);
}
