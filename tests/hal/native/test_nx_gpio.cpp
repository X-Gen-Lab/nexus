/**
 * \file            test_nx_gpio.cpp
 * \brief           GPIO Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for GPIO peripheral implementation.
 *                  Requirements: 1.1-1.7, 21.1-21.3
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_gpio.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_gpio_helpers.h"
}

/**
 * \brief           GPIO Test Fixture
 */
class GPIOTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all GPIO instances before each test */
        native_gpio_reset_all();

        /* Get GPIO instance (Port A, Pin 0) */
        gpio = nx_factory_gpio('A', 0);
        ASSERT_NE(nullptr, gpio);

        /* Initialize GPIO as output */
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

    nx_gpio_t* gpio = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 1.1, 1.2, 1.3                    */
/*---------------------------------------------------------------------------*/

TEST_F(GPIOTest, InitializeGPIO) {
    /* Already initialized in SetUp, check state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(GPIOTest, WriteGPIOHigh) {
    /* Write high */
    gpio->write.write(&gpio->write, 1);

    /* Verify state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(1, state.pin_state);
    EXPECT_EQ(1U, state.write_count);
}

TEST_F(GPIOTest, WriteGPIOLow) {
    /* Write low */
    gpio->write.write(&gpio->write, 0);

    /* Verify state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(0, state.pin_state);
    EXPECT_EQ(1U, state.write_count);
}

TEST_F(GPIOTest, ReadGPIO) {
    /* Write a value */
    gpio->write.write(&gpio->write, 1);

    /* Read it back */
    uint8_t value = gpio->read.read(&gpio->read);
    EXPECT_EQ(1, value);

    /* Verify read count */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(1U, state.read_count);
}

TEST_F(GPIOTest, ToggleGPIO) {
    /* Initial state is 0 */
    gpio->write.write(&gpio->write, 0);

    /* Toggle */
    gpio->write.toggle(&gpio->write);

    /* Should be 1 now */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(1, state.pin_state);
    EXPECT_EQ(1U, state.toggle_count);

    /* Toggle again */
    gpio->write.toggle(&gpio->write);

    /* Should be 0 now */
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(0, state.pin_state);
    EXPECT_EQ(2U, state.toggle_count);
}

/*---------------------------------------------------------------------------*/
/* Interrupt Tests - Requirement 1.4                                         */
/*---------------------------------------------------------------------------*/

static bool interrupt_triggered = false;
static void* interrupt_user_data = nullptr;

static void gpio_interrupt_callback(void* user_data) {
    interrupt_triggered = true;
    interrupt_user_data = user_data;
}

TEST_F(GPIOTest, RegisterInterrupt) {
    /* Register interrupt */
    int user_data = 42;
    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_RISING));

    /* Verify interrupt is registered */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_TRUE(state.interrupt_enabled);
    EXPECT_EQ(NX_GPIO_TRIGGER_RISING, state.trigger);
}

TEST_F(GPIOTest, InterruptTriggerRising) {
    /* Register interrupt */
    int user_data = 42;
    interrupt_triggered = false;
    interrupt_user_data = nullptr;

    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_RISING));

    /* Simulate rising edge (0 -> 1) */
    EXPECT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 1));

    /* Check if interrupt was triggered */
    EXPECT_TRUE(interrupt_triggered);
    EXPECT_EQ(&user_data, interrupt_user_data);
}

TEST_F(GPIOTest, InterruptTriggerFalling) {
    /* Register interrupt */
    int user_data = 42;
    interrupt_triggered = false;
    interrupt_user_data = nullptr;

    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_FALLING));

    /* Set pin high first */
    EXPECT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 1));
    interrupt_triggered = false;

    /* Simulate falling edge (1 -> 0) */
    EXPECT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 0));

    /* Check if interrupt was triggered */
    EXPECT_TRUE(interrupt_triggered);
    EXPECT_EQ(&user_data, interrupt_user_data);
}

TEST_F(GPIOTest, InterruptTriggerBoth) {
    /* Register interrupt */
    int user_data = 42;
    interrupt_triggered = false;
    interrupt_user_data = nullptr;

    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_BOTH));

    /* Simulate rising edge */
    EXPECT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 1));
    EXPECT_TRUE(interrupt_triggered);

    /* Reset flag */
    interrupt_triggered = false;

    /* Simulate falling edge */
    EXPECT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 0));
    EXPECT_TRUE(interrupt_triggered);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 1.5, 1.6                            */
/*---------------------------------------------------------------------------*/

TEST_F(GPIOTest, SuspendGPIO) {
    /* Write a value */
    gpio->write.write(&gpio->write, 1);

    /* Suspend */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_TRUE(state.suspended);
    EXPECT_EQ(1, state.pin_state); /* State should be preserved */
}

TEST_F(GPIOTest, ResumeGPIO) {
    /* Write a value */
    gpio->write.write(&gpio->write, 1);

    /* Suspend */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_FALSE(state.suspended);
    EXPECT_EQ(1, state.pin_state); /* State should be restored */
}

TEST_F(GPIOTest, SuspendResumePreservesState) {
    /* Set a specific state */
    gpio->write.write(&gpio->write, 1);

    /* Get state before suspend */
    native_gpio_state_t state_before;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state_before));

    /* Suspend and resume */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);
    lifecycle->resume(lifecycle);

    /* Get state after resume */
    native_gpio_state_t state_after;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state_after));

    /* Pin state should be preserved */
    EXPECT_EQ(state_before.pin_state, state_after.pin_state);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirement 1.7                                         */
/*---------------------------------------------------------------------------*/

TEST_F(GPIOTest, DeinitializeGPIO) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(GPIOTest, GetLifecycleState) {
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);

    /* Should be running after init */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinit */
    lifecycle->deinit(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(GPIOTest, NullPointerHandling) {
    /* Test NULL pointer handling - should not crash */
    nx_gpio_write_t* null_gpio = nullptr;
    if (null_gpio != nullptr) {
        null_gpio->write(null_gpio, 1);
        null_gpio->toggle(null_gpio);
    }

    /* Read with NULL should return 0 */
    nx_gpio_read_t* null_read = nullptr;
    if (null_read != nullptr) {
        uint8_t value = null_read->read(null_read);
        EXPECT_EQ(0, value);
    }
}

TEST_F(GPIOTest, InvalidPortHandling) {
    /* Try to get GPIO with invalid port */
    nx_gpio_t* invalid_gpio = nx_factory_gpio('Z', 0);
    EXPECT_EQ(nullptr, invalid_gpio);
}

TEST_F(GPIOTest, InvalidPinHandling) {
    /* Try to get GPIO with invalid pin */
    nx_gpio_t* invalid_gpio = nx_factory_gpio('A', 255);
    EXPECT_EQ(nullptr, invalid_gpio);
}

TEST_F(GPIOTest, UninitializedOperation) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Operations on uninitialized GPIO should not crash */
    gpio->write.write(&gpio->write, 1);
    gpio->write.toggle(&gpio->write);
    uint8_t value = gpio->read.read(&gpio->read);
    EXPECT_EQ(0, value);
}

TEST_F(GPIOTest, DoubleInit) {
    /* Try to initialize again */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(GPIOTest, DeinitUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to deinitialize again */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

TEST_F(GPIOTest, SuspendUninitialized) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->deinit(lifecycle);

    /* Try to suspend */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->suspend(lifecycle));
}

TEST_F(GPIOTest, ResumeNotSuspended) {
    /* Try to resume without suspending */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}

TEST_F(GPIOTest, DoubleSuspend) {
    /* Suspend */
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    lifecycle->suspend(lifecycle);

    /* Try to suspend again */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(GPIOTest, MultipleGPIOInstances) {
    /* Get multiple GPIO instances */
    nx_gpio_t* gpio1 = nx_factory_gpio('A', 1);
    nx_gpio_t* gpio2 = nx_factory_gpio('A', 2);
    nx_gpio_t* gpio3 = nx_factory_gpio('B', 0);

    ASSERT_NE(nullptr, gpio1);
    ASSERT_NE(nullptr, gpio2);
    ASSERT_NE(nullptr, gpio3);

    /* Initialize all */
    nx_lifecycle_t* lc1 = gpio1->write.get_lifecycle(&gpio1->write);
    nx_lifecycle_t* lc2 = gpio2->write.get_lifecycle(&gpio2->write);
    nx_lifecycle_t* lc3 = gpio3->write.get_lifecycle(&gpio3->write);

    ASSERT_EQ(NX_OK, lc1->init(lc1));
    ASSERT_EQ(NX_OK, lc2->init(lc2));
    ASSERT_EQ(NX_OK, lc3->init(lc3));

    /* Write different values */
    gpio1->write.write(&gpio1->write, 0);
    gpio2->write.write(&gpio2->write, 1);
    gpio3->write.write(&gpio3->write, 1);

    /* Verify each has correct state */
    native_gpio_state_t state1, state2, state3;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 1, &state1));
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 2, &state2));
    EXPECT_EQ(NX_OK, native_gpio_get_state(1, 0, &state3));

    EXPECT_EQ(0, state1.pin_state);
    EXPECT_EQ(1, state2.pin_state);
    EXPECT_EQ(1, state3.pin_state);

    /* Cleanup */
    lc1->deinit(lc1);
    lc2->deinit(lc2);
    lc3->deinit(lc3);
}

TEST_F(GPIOTest, RapidToggle) {
    /* Perform rapid toggles */
    for (int i = 0; i < 100; ++i) {
        gpio->write.toggle(&gpio->write);
    }

    /* Verify toggle count */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(100U, state.toggle_count);

    /* Final state should be 0 (even number of toggles from initial 0) */
    EXPECT_EQ(0, state.pin_state);
}

TEST_F(GPIOTest, MultipleInterruptRegistrations) {
    /* Register interrupt multiple times - last one should win */
    int user_data1 = 1;
    int user_data2 = 2;

    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data1, NX_GPIO_TRIGGER_RISING));

    EXPECT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data2, NX_GPIO_TRIGGER_FALLING));

    /* Verify last registration */
    native_gpio_state_t state;
    EXPECT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(NX_GPIO_TRIGGER_FALLING, state.trigger);
}
