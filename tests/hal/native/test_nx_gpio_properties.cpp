/**
 * \file            test_nx_gpio_properties.cpp
 * \brief           GPIO Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for GPIO peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_gpio.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           GPIO Property Test Fixture
 */
class GPIOPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_gpio_t* gpio = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all GPIO instances */
        native_gpio_reset_all();

        /* Get GPIO instance (Port A, Pin 0) */
        gpio = nx_factory_gpio('A', 0);
        ASSERT_NE(nullptr, gpio);

        /* Initialize GPIO */
        nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize GPIO */
        if (gpio != nullptr) {
            nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_gpio_reset_all();
    }

    /**
     * \brief       Generate random pin state (0 or 1)
     */
    uint8_t randomPinState() {
        std::uniform_int_distribution<int> dist(0, 1);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random GPIO port ('A'-'H')
     */
    char randomPort() {
        std::uniform_int_distribution<int> dist(0, 7);
        return static_cast<char>('A' + dist(rng));
    }

    /**
     * \brief       Generate random GPIO pin (0-15)
     */
    uint8_t randomPin() {
        std::uniform_int_distribution<int> dist(0, 15);
        return static_cast<uint8_t>(dist(rng));
    }

    /**
     * \brief       Generate random trigger type
     */
    nx_gpio_trigger_t randomTrigger() {
        std::uniform_int_distribution<int> dist(0, 2);
        return static_cast<nx_gpio_trigger_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotency                                    */
/* *For any* GPIO instance and configuration, initializing multiple times    */
/* with the same configuration SHALL produce the same result state.          */
/* **Validates: Requirements 1.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotency
 *
 * *For any* GPIO instance, initializing it should always succeed and produce
 * a consistent initialized state.
 *
 * **Validates: Requirements 1.1**
 */
TEST_F(GPIOPropertyTest, Property1_InitializationIdempotent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random port and pin */
        char port = randomPort();
        uint8_t pin = randomPin();

        /* Reset this GPIO */
        native_gpio_reset(port - 'A', pin);

        /* Get GPIO instance */
        nx_gpio_t* test_gpio = nx_factory_gpio(port, pin);
        if (test_gpio == nullptr) {
            continue; /* Skip if GPIO not available */
        }

        /* Initialize */
        nx_lifecycle_t* lifecycle =
            test_gpio->write.get_lifecycle(&test_gpio->write);
        ASSERT_NE(nullptr, lifecycle);
        nx_status_t result1 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_OK, result1)
            << "Iteration " << test_iter << ": First init failed for GPIO"
            << port << pin;

        /* Check state after first init */
        native_gpio_state_t state1;
        EXPECT_EQ(NX_OK, native_gpio_get_state(port - 'A', pin, &state1));
        EXPECT_TRUE(state1.initialized)
            << "Iteration " << test_iter << ": GPIO not initialized";

        /* Try to initialize again - should fail with ALREADY_INIT */
        nx_status_t result2 = lifecycle->init(lifecycle);
        EXPECT_EQ(NX_ERR_ALREADY_INIT, result2)
            << "Iteration " << test_iter << ": Double init should fail";

        /* State should remain initialized */
        native_gpio_state_t state2;
        EXPECT_EQ(NX_OK, native_gpio_get_state(port - 'A', pin, &state2));
        EXPECT_TRUE(state2.initialized) << "Iteration " << test_iter
                                        << ": GPIO should still be initialized";

        /* Cleanup */
        lifecycle->deinit(lifecycle);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round-trip                                          */
/* *For any* GPIO instance, initializing then immediately deinitializing     */
/* SHALL restore the GPIO to uninitialized state.                            */
/* **Validates: Requirements 1.7**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round-trip
 *
 * *For any* GPIO instance, init followed by deinit should restore the
 * uninitialized state.
 *
 * **Validates: Requirements 1.7**
 */
TEST_F(GPIOPropertyTest, Property2_LifecycleRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random port and pin */
        char port = randomPort();
        uint8_t pin = randomPin();

        /* Reset this GPIO */
        native_gpio_reset(port - 'A', pin);

        /* Get GPIO instance */
        nx_gpio_t* test_gpio = nx_factory_gpio(port, pin);
        if (test_gpio == nullptr) {
            continue; /* Skip if GPIO not available */
        }

        /* Check initial state */
        native_gpio_state_t state_before;
        EXPECT_EQ(NX_OK, native_gpio_get_state(port - 'A', pin, &state_before));
        EXPECT_FALSE(state_before.initialized)
            << "Iteration " << test_iter << ": Should start uninitialized";

        /* Initialize */
        nx_lifecycle_t* lifecycle =
            test_gpio->write.get_lifecycle(&test_gpio->write);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_gpio_state_t state_init;
        EXPECT_EQ(NX_OK, native_gpio_get_state(port - 'A', pin, &state_init));
        EXPECT_TRUE(state_init.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify back to uninitialized */
        native_gpio_state_t state_after;
        EXPECT_EQ(NX_OK, native_gpio_get_state(port - 'A', pin, &state_after));
        EXPECT_FALSE(state_after.initialized)
            << "Iteration " << test_iter
            << ": Should be uninitialized after deinit";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round-trip                                   */
/* *For any* GPIO instance and state, entering low-power mode then waking    */
/* SHALL restore the original state.                                         */
/* **Validates: Requirements 1.5, 1.6**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round-trip
 *
 * *For any* GPIO instance and pin state, suspend followed by resume should
 * preserve the pin state.
 *
 * **Validates: Requirements 1.5, 1.6**
 */
TEST_F(GPIOPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random pin state */
        uint8_t pin_value = randomPinState();

        /* Write the random value */
        gpio->write.write(&gpio->write, pin_value);

        /* Get state before suspend */
        native_gpio_state_t state_before;
        EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state_before));
        EXPECT_EQ(pin_value, state_before.pin_state);

        /* Suspend */
        nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
        ASSERT_NE(nullptr, lifecycle);
        EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

        /* Verify suspended */
        native_gpio_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Resume */
        EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

        /* Get state after resume */
        native_gpio_state_t state_after;
        EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state_after));
        EXPECT_FALSE(state_after.suspended);

        /* Pin state should be preserved */
        EXPECT_EQ(state_before.pin_state, state_after.pin_state)
            << "Iteration " << test_iter
            << ": Pin state not preserved after suspend/resume";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 11: GPIO Read-Write Consistency                                  */
/* *For any* GPIO pin and level value, writing then immediately reading      */
/* SHALL return the same level value.                                        */
/* **Validates: Requirements 1.2, 1.3**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 11: GPIO Read-Write Consistency
 *
 * *For any* GPIO pin and pin state, writing a value then reading it back
 * should return the same value.
 *
 * **Validates: Requirements 1.2, 1.3**
 */
TEST_F(GPIOPropertyTest, Property11_ReadWriteConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random pin state */
        uint8_t expected_value = randomPinState();

        /* Write the value */
        gpio->write.write(&gpio->write, expected_value);

        /* Read it back */
        uint8_t actual_value = gpio->read.read(&gpio->read);

        /* Should match */
        EXPECT_EQ(expected_value, actual_value)
            << "Iteration " << test_iter
            << ": Read value doesn't match written value";

        /* Verify through helper */
        native_gpio_state_t state;
        EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
        EXPECT_EQ(expected_value, state.pin_state)
            << "Iteration " << test_iter
            << ": State doesn't match written value";
    }
}

/**
 * Feature: native-hal-validation, Property 11: GPIO Read-Write Consistency
 *
 * *For any* sequence of writes, the final read should return the last written
 * value.
 *
 * **Validates: Requirements 1.2, 1.3**
 */
TEST_F(GPIOPropertyTest, Property11_LastWriteWins) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random sequence of writes */
        std::uniform_int_distribution<int> count_dist(1, 10);
        int write_count = count_dist(rng);

        uint8_t last_value = 0;
        for (int i = 0; i < write_count; ++i) {
            last_value = randomPinState();
            gpio->write.write(&gpio->write, last_value);
        }

        /* Read should return last written value */
        uint8_t actual_value = gpio->read.read(&gpio->read);
        EXPECT_EQ(last_value, actual_value)
            << "Iteration " << test_iter
            << ": Read doesn't return last written value";
    }
}

/**
 * Feature: native-hal-validation, Property 11: GPIO Read-Write Consistency
 *
 * *For any* GPIO pin, toggling an even number of times should return to the
 * original state.
 *
 * **Validates: Requirements 1.2, 1.3**
 */
TEST_F(GPIOPropertyTest, Property11_ToggleRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Set initial state */
        uint8_t initial_value = randomPinState();
        gpio->write.write(&gpio->write, initial_value);

        /* Generate random even number of toggles */
        std::uniform_int_distribution<int> count_dist(1, 50);
        int toggle_count = count_dist(rng) * 2; /* Ensure even */

        /* Perform toggles */
        for (int i = 0; i < toggle_count; ++i) {
            gpio->write.toggle(&gpio->write);
        }

        /* Should be back to initial state */
        uint8_t final_value = gpio->read.read(&gpio->read);
        EXPECT_EQ(initial_value, final_value)
            << "Iteration " << test_iter
            << ": Even toggles didn't return to initial state";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 12: GPIO Interrupt Trigger                                       */
/* *For any* configured GPIO interrupt, simulating a pin change SHALL        */
/* trigger the interrupt callback.                                           */
/* **Validates: Requirements 1.4**                                           */
/*---------------------------------------------------------------------------*/

static int property12_callback_count = 0;
static void* property12_user_data = nullptr;

static void property12_callback(void* user_data) {
    property12_callback_count++;
    property12_user_data = user_data;
}

/**
 * Feature: native-hal-validation, Property 12: GPIO Interrupt Trigger
 *
 * *For any* GPIO pin with interrupt configured, simulating the appropriate
 * edge should trigger the callback.
 *
 * **Validates: Requirements 1.4**
 */
TEST_F(GPIOPropertyTest, Property12_InterruptTrigger) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random trigger type */
        nx_gpio_trigger_t trigger = randomTrigger();

        /* Reset callback state */
        property12_callback_count = 0;
        property12_user_data = nullptr;

        /* Unregister any previous interrupt */
        gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                 NX_GPIO_TRIGGER_RISING);

        /* Ensure we start from a known state (0) */
        native_gpio_simulate_pin_change(0, 0, 0);

        /* Register interrupt */
        int user_data = test_iter;
        EXPECT_EQ(NX_OK,
                  gpio->read.register_exti(&gpio->read, property12_callback,
                                           &user_data, trigger));

        /* Simulate appropriate edge based on trigger type */
        int expected_triggers = 0;

        if (trigger == NX_GPIO_TRIGGER_RISING) {
            /* Simulate rising edge (0 -> 1) */
            native_gpio_simulate_pin_change(0, 0, 1);
            expected_triggers = 1;
        } else if (trigger == NX_GPIO_TRIGGER_FALLING) {
            /* First go high, then simulate falling edge (1 -> 0) */
            native_gpio_simulate_pin_change(0, 0, 1);
            native_gpio_simulate_pin_change(0, 0, 0);
            expected_triggers = 1;
        } else if (trigger == NX_GPIO_TRIGGER_BOTH) {
            /* Simulate both edges: 0 -> 1 -> 0 */
            native_gpio_simulate_pin_change(0, 0, 1); /* Rising */
            native_gpio_simulate_pin_change(0, 0, 0); /* Falling */
            expected_triggers = 2;
        }

        /* Verify callback was triggered */
        EXPECT_EQ(expected_triggers, property12_callback_count)
            << "Iteration " << test_iter
            << ": Callback not triggered correct number of times for trigger "
               "type "
            << trigger;

        if (expected_triggers > 0) {
            EXPECT_EQ(&user_data, property12_user_data)
                << "Iteration " << test_iter
                << ": User data not passed correctly";
        }
    }
}

/**
 * Feature: native-hal-validation, Property 12: GPIO Interrupt Trigger
 *
 * *For any* GPIO pin, interrupt should only trigger on configured edges.
 *
 * **Validates: Requirements 1.4**
 */
TEST_F(GPIOPropertyTest, Property12_InterruptOnlyOnConfiguredEdge) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Test rising edge only */
        property12_callback_count = 0;
        int user_data = test_iter;

        /* Unregister any previous interrupt and set initial state */
        gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                 NX_GPIO_TRIGGER_RISING);
        native_gpio_simulate_pin_change(0, 0, 1);

        /* Now register rising edge interrupt */
        EXPECT_EQ(NX_OK,
                  gpio->read.register_exti(&gpio->read, property12_callback,
                                           &user_data, NX_GPIO_TRIGGER_RISING));

        /* Simulate falling edge - should NOT trigger */
        native_gpio_simulate_pin_change(0, 0, 0);
        EXPECT_EQ(0, property12_callback_count)
            << "Iteration " << test_iter
            << ": Rising-only interrupt triggered on falling edge";

        /* Simulate rising edge - SHOULD trigger */
        native_gpio_simulate_pin_change(0, 0, 1);
        EXPECT_EQ(1, property12_callback_count)
            << "Iteration " << test_iter
            << ": Rising-only interrupt didn't trigger on rising edge";

        /* Test falling edge only */
        property12_callback_count = 0;

        /* Unregister previous interrupt and set initial state */
        gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                 NX_GPIO_TRIGGER_FALLING);
        native_gpio_simulate_pin_change(0, 0, 0);

        /* Now register falling edge interrupt */
        EXPECT_EQ(NX_OK, gpio->read.register_exti(
                             &gpio->read, property12_callback, &user_data,
                             NX_GPIO_TRIGGER_FALLING));

        /* Simulate rising edge - should NOT trigger */
        native_gpio_simulate_pin_change(0, 0, 1);
        EXPECT_EQ(0, property12_callback_count)
            << "Iteration " << test_iter
            << ": Falling-only interrupt triggered on rising edge";

        /* Simulate falling edge - SHOULD trigger */
        native_gpio_simulate_pin_change(0, 0, 0);
        EXPECT_EQ(1, property12_callback_count)
            << "Iteration " << test_iter
            << ": Falling-only interrupt didn't trigger on falling edge";
    }
}

/**
 * Feature: native-hal-validation, Property 12: GPIO Interrupt Trigger
 *
 * *For any* GPIO pin with BOTH edge trigger, both rising and falling edges
 * should trigger the callback.
 *
 * **Validates: Requirements 1.4**
 */
TEST_F(GPIOPropertyTest, Property12_BothEdgesTrigger) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Register BOTH edge interrupt */
        property12_callback_count = 0;
        int user_data = test_iter;

        /* Unregister any previous interrupt and set initial state */
        gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                 NX_GPIO_TRIGGER_BOTH);
        native_gpio_simulate_pin_change(0, 0, 0);

        /* Now register BOTH edge interrupt */
        EXPECT_EQ(NX_OK,
                  gpio->read.register_exti(&gpio->read, property12_callback,
                                           &user_data, NX_GPIO_TRIGGER_BOTH));

        /* Generate random sequence of edges */
        std::uniform_int_distribution<int> count_dist(2, 10);
        int edge_count = count_dist(rng);

        uint8_t current_state = 0;
        for (int i = 0; i < edge_count; ++i) {
            /* Toggle state */
            current_state = 1 - current_state;
            native_gpio_simulate_pin_change(0, 0, current_state);
        }

        /* Should have triggered for each edge */
        EXPECT_EQ(edge_count, property12_callback_count)
            << "Iteration " << test_iter
            << ": BOTH edge interrupt didn't trigger for all edges";
    }
}
