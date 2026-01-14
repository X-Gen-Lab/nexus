/**
 * \file            test_hal_gpio.cpp
 * \brief           HAL GPIO Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/hal_gpio.h"
#include "native_platform.h"
}

/**
 * \brief           GPIO Test Fixture
 */
class HalGpioTest : public ::testing::Test {
  protected:
    void SetUp() override {
        native_gpio_reset_all();
    }

    void TearDown() override {
        native_gpio_reset_all();
    }
};

/**
 * \brief           Test GPIO initialization
 */
TEST_F(HalGpioTest, InitOutput) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));

    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_A, 0);
    ASSERT_NE(nullptr, state);
    EXPECT_TRUE(state->configured);
    EXPECT_TRUE(state->is_output);
    EXPECT_FALSE(state->level);
}

/**
 * \brief           Test GPIO initialization with high initial level
 */
TEST_F(HalGpioTest, InitOutputHigh) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_HIGH};

    EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_B, 5, &config));

    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_B, 5);
    ASSERT_NE(nullptr, state);
    EXPECT_TRUE(state->level);
}

/**
 * \brief           Test GPIO input initialization
 */
TEST_F(HalGpioTest, InitInput) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_INPUT,
                                .pull = HAL_GPIO_PULL_UP,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_C, 13, &config));

    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_C, 13);
    ASSERT_NE(nullptr, state);
    EXPECT_TRUE(state->configured);
    EXPECT_FALSE(state->is_output);
}

/**
 * \brief           Test GPIO invalid parameters
 */
TEST_F(HalGpioTest, InitInvalidParams) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    /* Invalid port */
    EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_init(HAL_GPIO_PORT_MAX, 0, &config));

    /* Invalid pin */
    EXPECT_EQ(HAL_ERR_PARAM, hal_gpio_init(HAL_GPIO_PORT_A, 16, &config));

    /* Null config - returns HAL_ERROR_NULL_POINTER */
    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_gpio_init(HAL_GPIO_PORT_A, 0, nullptr));
}

/**
 * \brief           Test GPIO write
 */
TEST_F(HalGpioTest, Write) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_D, 12, &config));

    /* Write high */
    EXPECT_EQ(HAL_OK, hal_gpio_write(HAL_GPIO_PORT_D, 12, HAL_GPIO_LEVEL_HIGH));
    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_D, 12);
    EXPECT_TRUE(state->level);

    /* Write low */
    EXPECT_EQ(HAL_OK, hal_gpio_write(HAL_GPIO_PORT_D, 12, HAL_GPIO_LEVEL_LOW));
    EXPECT_FALSE(state->level);
}

/**
 * \brief           Test GPIO read
 */
TEST_F(HalGpioTest, Read) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_HIGH};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_E, 0, &config));

    hal_gpio_level_t level;
    EXPECT_EQ(HAL_OK, hal_gpio_read(HAL_GPIO_PORT_E, 0, &level));
    EXPECT_EQ(HAL_GPIO_LEVEL_HIGH, level);
}

/**
 * \brief           Test GPIO toggle
 */
TEST_F(HalGpioTest, Toggle) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_F, 1, &config));

    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_F, 1);
    EXPECT_FALSE(state->level);

    /* Toggle to high */
    EXPECT_EQ(HAL_OK, hal_gpio_toggle(HAL_GPIO_PORT_F, 1));
    EXPECT_TRUE(state->level);

    /* Toggle to low */
    EXPECT_EQ(HAL_OK, hal_gpio_toggle(HAL_GPIO_PORT_F, 1));
    EXPECT_FALSE(state->level);
}

/**
 * \brief           Test GPIO deinit
 */
TEST_F(HalGpioTest, Deinit) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_HIGH};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_G, 7, &config));
    EXPECT_EQ(HAL_OK, hal_gpio_deinit(HAL_GPIO_PORT_G, 7));

    native_gpio_pin_t* state = native_gpio_get_state(HAL_GPIO_PORT_G, 7);
    EXPECT_FALSE(state->configured);
}

/**
 * \brief           Test operations on uninitialized pin
 * \details         Requirements 1.1, 1.3, 1.4 - operations on uninitialized
 * pins should fail
 */
TEST_F(HalGpioTest, OperationsOnUninitializedPin) {
    hal_gpio_level_t level;

    /* Write on uninitialized pin should fail */
    EXPECT_EQ(HAL_ERROR_NOT_INIT,
              hal_gpio_write(HAL_GPIO_PORT_A, 0, HAL_GPIO_LEVEL_HIGH));

    /* Read on uninitialized pin should fail */
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_read(HAL_GPIO_PORT_A, 0, &level));

    /* Toggle on uninitialized pin should fail */
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_gpio_toggle(HAL_GPIO_PORT_A, 0));
}

/**
 * \brief           Test write on input pin
 * \details         Requirements 1.3 - write on input pin should fail
 */
TEST_F(HalGpioTest, WriteOnInputPin) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_INPUT,
                                .pull = HAL_GPIO_PULL_UP,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 1, &config));

    /* Write on input pin should fail */
    EXPECT_EQ(HAL_ERROR_INVALID_STATE,
              hal_gpio_write(HAL_GPIO_PORT_A, 1, HAL_GPIO_LEVEL_HIGH));

    /* Toggle on input pin should fail */
    EXPECT_EQ(HAL_ERROR_INVALID_STATE, hal_gpio_toggle(HAL_GPIO_PORT_A, 1));
}

/**
 * \brief           Test read with null pointer
 * \details         Requirements 1.4 - read with null level pointer should fail
 */
TEST_F(HalGpioTest, ReadNullPointer) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 2, &config));

    /* Read with null pointer should fail */
    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_gpio_read(HAL_GPIO_PORT_A, 2, nullptr));
}
