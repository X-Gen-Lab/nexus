/**
 * \file            test_hal_gpio_properties.cpp
 * \brief           HAL GPIO Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for GPIO module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstdint>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_gpio.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           GPIO Property Test Fixture
 */
class HalGpioPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_gpio_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_gpio_reset_all();
    }

    hal_gpio_port_t randomPort() {
        std::uniform_int_distribution<int> dist(0, HAL_GPIO_PORT_MAX - 1);
        return static_cast<hal_gpio_port_t>(dist(rng));
    }

    hal_gpio_pin_t randomPin() {
        std::uniform_int_distribution<int> dist(0, 15);
        return static_cast<hal_gpio_pin_t>(dist(rng));
    }

    hal_gpio_level_t randomLevel() {
        std::uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 0 ? HAL_GPIO_LEVEL_LOW : HAL_GPIO_LEVEL_HIGH;
    }

    hal_gpio_config_t makeOutputConfig(hal_gpio_level_t init_level) {
        return hal_gpio_config_t{.direction = HAL_GPIO_DIR_OUTPUT,
                                 .pull = HAL_GPIO_PULL_NONE,
                                 .output_mode = HAL_GPIO_OUTPUT_PP,
                                 .speed = HAL_GPIO_SPEED_LOW,
                                 .init_level = init_level};
    }
};

/**
 * Feature: phase2-core-platform, Property 1: GPIO State Consistency
 *
 * *For any* GPIO pin that is initialized as output, writing a level and then
 * reading it back SHALL return the same level.
 *
 * **Validates: Requirements 1.3, 1.4**
 */
TEST_F(HalGpioPropertyTest, Property1_WriteReadConsistency) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        auto port = randomPort();
        auto pin = randomPin();
        auto level = randomLevel();

        hal_gpio_config_t config = makeOutputConfig(HAL_GPIO_LEVEL_LOW);

        ASSERT_EQ(HAL_OK, hal_gpio_init(port, pin, &config))
            << "Iteration " << i << ": init failed for port=" << port
            << " pin=" << (int)pin;

        ASSERT_EQ(HAL_OK, hal_gpio_write(port, pin, level))
            << "Iteration " << i << ": write failed";

        hal_gpio_level_t read_level;
        ASSERT_EQ(HAL_OK, hal_gpio_read(port, pin, &read_level))
            << "Iteration " << i << ": read failed";

        EXPECT_EQ(level, read_level)
            << "Iteration " << i << ": write/read mismatch for port=" << port
            << " pin=" << (int)pin << " level=" << level;

        hal_gpio_deinit(port, pin);
    }
}

/**
 * Feature: phase2-core-platform, Property 2: GPIO Toggle Inversion
 *
 * *For any* GPIO output pin, calling toggle SHALL invert the current level
 * (LOW becomes HIGH, HIGH becomes LOW).
 *
 * **Validates: Requirements 1.5**
 */
TEST_F(HalGpioPropertyTest, Property2_ToggleInversion) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        auto port = randomPort();
        auto pin = randomPin();
        auto init_level = randomLevel();

        hal_gpio_config_t config = makeOutputConfig(init_level);

        ASSERT_EQ(HAL_OK, hal_gpio_init(port, pin, &config))
            << "Iteration " << i << ": init failed";

        // Read initial level
        hal_gpio_level_t level_before;
        ASSERT_EQ(HAL_OK, hal_gpio_read(port, pin, &level_before))
            << "Iteration " << i << ": read before toggle failed";
        ASSERT_EQ(init_level, level_before)
            << "Iteration " << i << ": initial level mismatch";

        // Toggle
        ASSERT_EQ(HAL_OK, hal_gpio_toggle(port, pin))
            << "Iteration " << i << ": toggle failed";

        // Read after toggle
        hal_gpio_level_t level_after;
        ASSERT_EQ(HAL_OK, hal_gpio_read(port, pin, &level_after))
            << "Iteration " << i << ": read after toggle failed";

        // Verify inversion
        hal_gpio_level_t expected = (init_level == HAL_GPIO_LEVEL_LOW)
                                        ? HAL_GPIO_LEVEL_HIGH
                                        : HAL_GPIO_LEVEL_LOW;
        EXPECT_EQ(expected, level_after)
            << "Iteration " << i << ": toggle did not invert level. "
            << "init=" << init_level << " after=" << level_after;

        hal_gpio_deinit(port, pin);
    }
}

/**
 * Feature: phase2-core-platform, Property 3: GPIO Lifecycle Validity
 *
 * *For any* GPIO pin, init followed by deinit SHALL return HAL_OK, and
 * operations on uninitialized pins SHALL return HAL_ERROR_NOT_INIT.
 *
 * **Validates: Requirements 1.1, 1.2, 1.6**
 */
TEST_F(HalGpioPropertyTest, Property3_LifecycleValidity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        auto port = randomPort();
        auto pin = randomPin();
        auto level = randomLevel();

        hal_gpio_config_t config = makeOutputConfig(HAL_GPIO_LEVEL_LOW);
        hal_gpio_level_t read_level;

        // Operations on uninitialized pin should fail
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_write(port, pin, level))
            << "Iteration " << i << ": write on uninit should fail";
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_read(port, pin, &read_level))
            << "Iteration " << i << ": read on uninit should fail";
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_toggle(port, pin))
            << "Iteration " << i << ": toggle on uninit should fail";

        // Init should succeed
        ASSERT_EQ(HAL_OK, hal_gpio_init(port, pin, &config))
            << "Iteration " << i << ": init should succeed";

        // Operations should now succeed
        EXPECT_EQ(HAL_OK, hal_gpio_write(port, pin, level))
            << "Iteration " << i << ": write after init should succeed";
        EXPECT_EQ(HAL_OK, hal_gpio_read(port, pin, &read_level))
            << "Iteration " << i << ": read after init should succeed";
        EXPECT_EQ(HAL_OK, hal_gpio_toggle(port, pin))
            << "Iteration " << i << ": toggle after init should succeed";

        // Deinit should succeed
        ASSERT_EQ(HAL_OK, hal_gpio_deinit(port, pin))
            << "Iteration " << i << ": deinit should succeed";

        // Operations should fail again after deinit
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_write(port, pin, level))
            << "Iteration " << i << ": write after deinit should fail";
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_read(port, pin, &read_level))
            << "Iteration " << i << ": read after deinit should fail";
        EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_toggle(port, pin))
            << "Iteration " << i << ": toggle after deinit should fail";
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 4: GPIO Parameter Validation
 *
 * *For any* invalid port (>= HAL_GPIO_PORT_MAX) or invalid pin (> 15),
 * calling any GPIO function SHALL return HAL_ERROR_INVALID_PARAM without
 * modifying hardware state.
 *
 * **Validates: Requirements 3.2, 3.8**
 */
TEST_F(HalGpioPropertyTest, Property4_ParameterValidation) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_gpio_reset_all();

        // Generate invalid port values
        std::uniform_int_distribution<int> invalid_port_dist(
            HAL_GPIO_PORT_MAX, HAL_GPIO_PORT_MAX + 10);
        auto invalid_port =
            static_cast<hal_gpio_port_t>(invalid_port_dist(rng));

        // Generate invalid pin values
        std::uniform_int_distribution<int> invalid_pin_dist(16, 255);
        auto invalid_pin = static_cast<hal_gpio_pin_t>(invalid_pin_dist(rng));

        auto valid_port = randomPort();
        auto valid_pin = randomPin();
        auto level = randomLevel();
        hal_gpio_level_t read_level;

        hal_gpio_config_t config = makeOutputConfig(HAL_GPIO_LEVEL_LOW);

        // Test with invalid port
        EXPECT_EQ(HAL_ERR_PARAM,
                  hal_gpio_init(invalid_port, valid_pin, &config))
            << "Iteration " << i << ": init with invalid port should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_write(invalid_port, valid_pin, level))
            << "Iteration " << i << ": write with invalid port should fail";
        EXPECT_EQ(HAL_ERR_PARAM,
                  hal_gpio_read(invalid_port, valid_pin, &read_level))
            << "Iteration " << i << ": read with invalid port should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_toggle(invalid_port, valid_pin))
            << "Iteration " << i << ": toggle with invalid port should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_deinit(invalid_port, valid_pin))
            << "Iteration " << i << ": deinit with invalid port should fail";

        // Test with invalid pin
        EXPECT_EQ(HAL_ERR_PARAM,
                  hal_gpio_init(valid_port, invalid_pin, &config))
            << "Iteration " << i << ": init with invalid pin should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_write(valid_port, invalid_pin, level))
            << "Iteration " << i << ": write with invalid pin should fail";
        EXPECT_EQ(HAL_ERR_PARAM,
                  hal_gpio_read(valid_port, invalid_pin, &read_level))
            << "Iteration " << i << ": read with invalid pin should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_toggle(valid_port, invalid_pin))
            << "Iteration " << i << ": toggle with invalid pin should fail";
        EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_deinit(valid_port, invalid_pin))
            << "Iteration " << i << ": deinit with invalid pin should fail";

        // Test with null config pointer
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_gpio_init(valid_port, valid_pin, nullptr))
            << "Iteration " << i << ": init with null config should fail";

        // Test with null level pointer for read
        ASSERT_EQ(HAL_OK, hal_gpio_init(valid_port, valid_pin, &config));
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_gpio_read(valid_port, valid_pin, nullptr))
            << "Iteration " << i << ": read with null level should fail";
        hal_gpio_deinit(valid_port, valid_pin);
    }
}
