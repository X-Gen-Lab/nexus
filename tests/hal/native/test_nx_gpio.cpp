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
#include "devices/native_gpio_helpers.h"
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

TEST_F(GPIOTest, ReadAndWriteShareLifecycleAndPower) {
    nx_lifecycle_t* read_lifecycle = gpio->read.get_lifecycle(&gpio->read);
    nx_lifecycle_t* write_lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, read_lifecycle);
    ASSERT_EQ(read_lifecycle, write_lifecycle);
    EXPECT_EQ(gpio->read.get_power(&gpio->read),
              gpio->write.get_power(&gpio->write));

    ASSERT_EQ(NX_OK, read_lifecycle->suspend(read_lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED,
              write_lifecycle->get_state(write_lifecycle));
    ASSERT_EQ(NX_OK, write_lifecycle->resume(write_lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, read_lifecycle->get_state(read_lifecycle));
}

TEST_F(GPIOTest, FactoryReturnsRegisteredCapabilities) {
    nx_gpio_read_t* read = nx_factory_gpio_read('A', 0);
    nx_gpio_write_t* write = nx_factory_gpio_write('A', 0);
    ASSERT_EQ(&gpio->read, read);
    ASSERT_EQ(&gpio->write, write);
    write->write(write, 1);
    EXPECT_EQ(1, read->read(read));
    EXPECT_EQ(nullptr, nx_factory_gpio_read('Z', 0));
    EXPECT_EQ(nullptr, nx_factory_gpio_write('Z', 0));
    EXPECT_EQ(nullptr, nx_factory_gpio_read('A', 255));
    EXPECT_EQ(nullptr, nx_factory_gpio_write('A', 255));
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

TEST_F(GPIOTest, InvalidInterruptTriggerPreservesRegistration) {
    int user_data = 42;
    ASSERT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_RISING));
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                       static_cast<nx_gpio_trigger_t>(255)));
    native_gpio_state_t state{};
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
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

TEST_F(GPIOTest, SuspendedAndDeinitializedInterruptsDoNotCallUser) {
    int user_data = 42;
    interrupt_triggered = false;
    ASSERT_EQ(NX_OK,
              gpio->read.register_exti(&gpio->read, gpio_interrupt_callback,
                                       &user_data, NX_GPIO_TRIGGER_BOTH));
    nx_lifecycle_t* lifecycle = gpio->read.get_lifecycle(&gpio->read);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    ASSERT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 1));
    EXPECT_FALSE(interrupt_triggered);
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));
    ASSERT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 0));
    EXPECT_TRUE(interrupt_triggered);
    ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    interrupt_triggered = false;
    ASSERT_EQ(NX_OK, native_gpio_simulate_pin_change(0, 0, 1));
    EXPECT_FALSE(interrupt_triggered);
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

TEST_F(GPIOTest, SuspendedOperationsPreserveOutputAndCounters) {
    gpio->write.write(&gpio->write, 1);
    nx_lifecycle_t* lifecycle = gpio->read.get_lifecycle(&gpio->read);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    native_gpio_state_t before{};
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 0, &before));
    gpio->write.write(&gpio->write, 0);
    gpio->write.toggle(&gpio->write);
    EXPECT_EQ(0, gpio->read.read(&gpio->read));
    EXPECT_EQ(NX_ERR_NOT_INIT,
              gpio->read.register_exti(&gpio->read, nullptr, nullptr,
                                       NX_GPIO_TRIGGER_RISING));
    native_gpio_state_t after{};
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 0, &after));
    EXPECT_EQ(before.pin_state, after.pin_state);
    EXPECT_EQ(before.write_count, after.write_count);
    EXPECT_EQ(before.read_count, after.read_count);
    EXPECT_EQ(before.toggle_count, after.toggle_count);
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(1, gpio->read.read(&gpio->read));
}

TEST_F(GPIOTest, PowerDisableEnablePreservesOutput) {
    nx_power_t* power = gpio->write.get_power(&gpio->write);
    ASSERT_NE(nullptr, power);
    EXPECT_TRUE(power->is_enabled(power));
    gpio->write.write(&gpio->write, 1);
    ASSERT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));
    ASSERT_EQ(NX_OK, power->disable(power));
    gpio->write.write(&gpio->write, 0);
    EXPECT_EQ(0, gpio->read.read(&gpio->read));
    ASSERT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
    EXPECT_EQ(1, gpio->read.read(&gpio->read));
    EXPECT_EQ(NX_OK, power->enable(power));
}

TEST_F(GPIOTest, PowerRejectsUninitializedDevice) {
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    nx_power_t* power = gpio->write.get_power(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_NE(nullptr, power);
    ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_FALSE(power->is_enabled(power));
    EXPECT_EQ(NX_ERR_NOT_INIT, power->enable(power));
    EXPECT_EQ(NX_ERR_NOT_INIT, power->disable(power));
}

TEST_F(GPIOTest, PowerCallbackUnsupported) {
    nx_power_t* power = gpio->write.get_power(&gpio->write);
    ASSERT_NE(nullptr, power);
    auto callback = [](void*, bool) {};
    EXPECT_EQ(NX_ERR_NOT_SUPPORTED,
              power->set_callback(power, callback, nullptr));
    EXPECT_EQ(NX_OK, power->set_callback(power, nullptr, nullptr));
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
    /* Invoke real entry points with a missing receiver. */
    gpio->write.write(nullptr, 1);
    gpio->write.toggle(nullptr);
    EXPECT_EQ(0, gpio->read.read(nullptr));
    EXPECT_EQ(nullptr, gpio->read.get_lifecycle(nullptr));
    EXPECT_EQ(nullptr, gpio->write.get_lifecycle(nullptr));
    EXPECT_EQ(nullptr, gpio->read.get_power(nullptr));
    EXPECT_EQ(nullptr, gpio->write.get_power(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR,
              gpio->read.register_exti(nullptr, gpio_interrupt_callback,
                                       nullptr, NX_GPIO_TRIGGER_RISING));
    native_gpio_state_t state{};
    ASSERT_EQ(NX_OK, native_gpio_get_state(0, 0, &state));
    EXPECT_EQ(0U, state.write_count);
    EXPECT_EQ(0U, state.toggle_count);

    nx_lifecycle_t* lifecycle = gpio->read.get_lifecycle(&gpio->read);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_ERR_NULL_PTR, lifecycle->init(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR, lifecycle->deinit(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR, lifecycle->suspend(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR, lifecycle->resume(nullptr));
    EXPECT_EQ(NX_DEV_STATE_ERROR, lifecycle->get_state(nullptr));

    nx_power_t* power = gpio->read.get_power(&gpio->read);
    ASSERT_NE(nullptr, power);
    EXPECT_EQ(NX_ERR_NULL_PTR, power->enable(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR, power->disable(nullptr));
    EXPECT_FALSE(power->is_enabled(nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR,
              power->set_callback(nullptr, nullptr, nullptr));
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
    /* The Native baseline enables A0 and A2. Disabled pins are not devices.
     * A0 was initialized by SetUp; initialize the other supported pin. */
    nx_gpio_t* other = nx_factory_gpio('A', 2);
    ASSERT_NE(nullptr, other);
    ASSERT_NE(gpio, other);
    nx_lifecycle_t* other_lifecycle =
        other->write.get_lifecycle(&other->write);
    ASSERT_NE(nullptr, other_lifecycle);
    ASSERT_EQ(NX_OK, other_lifecycle->init(other_lifecycle));

    gpio->write.write(&gpio->write, 0);
    other->write.write(&other->write, 1);
    EXPECT_EQ(0, gpio->read.read(&gpio->read));
    EXPECT_EQ(1, other->read.read(&other->read));

    /* Changing or deinitializing one instance must not affect the other. */
    gpio->write.toggle(&gpio->write);
    EXPECT_EQ(1, gpio->read.read(&gpio->read));
    EXPECT_EQ(1, other->read.read(&other->read));
    ASSERT_EQ(NX_OK, other_lifecycle->deinit(other_lifecycle));
    nx_lifecycle_t* lifecycle = gpio->write.get_lifecycle(&gpio->write);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));
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
