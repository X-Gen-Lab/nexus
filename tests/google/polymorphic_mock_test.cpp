/**
 * \file            polymorphic_mock_test.cpp
 * \brief           Public GPIO dispatch preserves provider and instance state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/gpio.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using ::testing::Return;
using ::testing::StrictMock;

/** \brief Independent provider storage with strict interaction expectations. */
class GPIOProvider {
  public:
    MOCK_METHOD(nx_result_t, write, (uint32_t set, uint32_t reset));
    MOCK_METHOD(nx_result_t, read, (uint32_t * value), (const));
    MOCK_METHOD(nx_result_t, toggle, (uint32_t mask));
};

/** \brief Dispatch through instance context, never through the shared table. */
nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    return static_cast<GPIOProvider*>(context)->write(set, reset);
}

/** \brief Keep read-only provider operations const-correct. */
nx_result_t read(const void* context, uint32_t* value) {
    return static_cast<const GPIOProvider*>(context)->read(value);
}

/** \brief Preserve a bounded provider's toggle operation. */
nx_result_t toggle(void* context, uint32_t mask) {
    return static_cast<GPIOProvider*>(context)->toggle(mask);
}

const nx_gpio_ops_t operations = {write, read, toggle};

TEST(PolymorphicGPIO, SharedOperationsKeepIndependentInstanceContext) {
    StrictMock<GPIOProvider> first;
    StrictMock<GPIOProvider> second;
    const nx_gpio_port_t first_port = {&operations, &first};
    const nx_gpio_port_t second_port = {&operations, &second};
    EXPECT_CALL(first, write(1, 2)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(second, write(4, 8)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_gpio_port_write(&first_port, 1, 2), NX_SUCCESS);
    EXPECT_EQ(nx_gpio_port_write(&second_port, 4, 8), NX_ERROR_IO);
    EXPECT_EQ(first_port.ops, second_port.ops);
}

TEST(PolymorphicGPIO, ConstFaceDispatchesReadAndToggle) {
    StrictMock<GPIOProvider> provider;
    const nx_gpio_port_t port = {&operations, &provider};
    uint32_t value = 0;
    EXPECT_CALL(provider, read(&value))
        .WillOnce(::testing::DoAll(::testing::SetArgPointee<0>(8),
                                   Return(NX_SUCCESS)));
    EXPECT_CALL(provider, toggle(8)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_gpio_port_read(&port, &value), NX_SUCCESS);
    EXPECT_EQ(value, 8U);
    EXPECT_EQ(nx_gpio_port_toggle(&port, 8), NX_SUCCESS);
}

TEST(PolymorphicGPIO, DifferentProviderTablesCoexistInOneImage) {
    StrictMock<GPIOProvider> first;
    StrictMock<GPIOProvider> second;
    const nx_gpio_ops_t write_only = {write, nullptr, nullptr};
    const nx_gpio_port_t first_port = {&operations, &first};
    const nx_gpio_port_t second_port = {&write_only, &second};
    EXPECT_CALL(first, toggle(2)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(second, write(4, 0)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_gpio_port_toggle(&first_port, 2), NX_SUCCESS);
    EXPECT_EQ(nx_gpio_port_write(&second_port, 4, 0), NX_SUCCESS);
    EXPECT_EQ(nx_gpio_port_toggle(&second_port, 4), NX_ERROR_UNSUPPORTED);
}

TEST(PolymorphicGPIO, InvalidFacesRejectBeforeProviderAccess) {
    StrictMock<GPIOProvider> provider;
    const nx_gpio_port_t no_ops = {nullptr, &provider};
    const nx_gpio_port_t no_context = {&operations, nullptr};
    EXPECT_EQ(nx_gpio_port_write(nullptr, 1, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_gpio_port_write(&no_ops, 1, 0), NX_ERROR_INVALID);
    EXPECT_EQ(nx_gpio_port_write(&no_context, 1, 0), NX_ERROR_INVALID);
}
} /* namespace */
