/**
 * \file            native_cold_test.cpp
 * \brief           Cold Native assembly admission and instance isolation.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "provider.h"
#include "spi_dma_provider.h"
#include <gtest/gtest.h>

namespace {

TEST(NativeCold, SpiValidatesEveryEndpointBeforeChangingTheController) {
    nx_native_spi_state_t port{};
    nx_native_spi_endpoint_state_t first{}, second{};
    uint8_t memory[4]{};
    nx_native_spi_endpoint_config_t endpoints[] = {
        {&first, memory, sizeof(memory)}, {&second, nullptr, 4U}};
    EXPECT_EQ(nx_native_spi_prepare(&port, endpoints, 2U), NX_ERROR_INVALID);
    EXPECT_FALSE(port.opened);
    EXPECT_EQ(first.port, nullptr);
    endpoints[1].size = 0U;
    ASSERT_EQ(nx_native_spi_prepare(&port, endpoints, 2U), NX_SUCCESS);
    EXPECT_EQ(first.port, &port);
    EXPECT_EQ(second.port, &port);
    EXPECT_EQ(first.registers, memory);
    EXPECT_EQ(first.register_count, sizeof(memory));
    first.cs = true;
    EXPECT_EQ(nx_native_spi_prepare(&port, endpoints, 2U), NX_ERROR_BUSY);
    EXPECT_TRUE(first.cs);
    EXPECT_TRUE(port.opened);
}

TEST(NativeCold, SpiDuplicateEndpointsAndLiveControllerPreserveBindings) {
    nx_native_spi_state_t port{}, other{};
    nx_native_spi_endpoint_state_t device{&other, nullptr, 0U, false};
    const nx_native_spi_endpoint_config_t duplicate[] = {
        {&device, nullptr, 0U}, {&device, nullptr, 0U}};
    EXPECT_EQ(nx_native_spi_prepare(&port, duplicate, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(device.port, &other);
    port.active = true;
    EXPECT_EQ(nx_native_spi_prepare(&port, duplicate, 1U), NX_ERROR_BUSY);
    EXPECT_EQ(device.port, &other);
    EXPECT_FALSE(port.opened);
}

TEST(NativeCold, ControllerWithoutEndpointsAndNullArgumentsStayExplicit) {
    nx_native_spi_state_t spi{};
    nx_native_i2c_state_t i2c{};
    EXPECT_EQ(nx_native_spi_prepare(nullptr, nullptr, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_i2c_prepare(nullptr, nullptr, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_spi_prepare(&spi, nullptr, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_native_i2c_prepare(&i2c, nullptr, 1U), NX_ERROR_INVALID);
    EXPECT_FALSE(spi.opened);
    EXPECT_FALSE(i2c.opened);
    EXPECT_EQ(nx_native_spi_prepare(&spi, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_native_i2c_prepare(&i2c, nullptr, 0U), NX_SUCCESS);
    EXPECT_TRUE(spi.opened);
    EXPECT_TRUE(i2c.opened);
}

TEST(NativeCold, I2cRejectsTheLastInvalidAddressBeforeOpeningAnyEndpoint) {
    nx_native_i2c_state_t port{}, other{};
    nx_native_i2c_endpoint_state_t first{}, second{};
    uint8_t memory[4]{};
    nx_native_i2c_endpoint_config_t endpoints[] = {
        {&first, memory, sizeof(memory), 0x50U},
        {&second, memory, sizeof(memory), 0x78U}};
    EXPECT_EQ(nx_native_i2c_prepare(&port, endpoints, 2U), NX_ERROR_INVALID);
    EXPECT_FALSE(port.opened);
    EXPECT_EQ(first.port, nullptr);
    endpoints[1].address = 0x51U;
    ASSERT_EQ(nx_native_i2c_prepare(&port, endpoints, 2U), NX_SUCCESS);
    EXPECT_EQ(first.port, &port);
    EXPECT_EQ(second.address, 0x51U);
    EXPECT_FALSE(other.opened);
    port.active = true;
    EXPECT_EQ(nx_native_i2c_prepare(&port, endpoints, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(first.memory, memory);
}

TEST(NativeCold, PwmInvalidDutyPreservesPreviouslyPreparedSelection) {
    nx_native_pwm_state_t port{};
    ASSERT_EQ(nx_native_pwm_prepare(&port, 1000U, 100U, 25U), NX_SUCCESS);
    EXPECT_FALSE(port.state.running);
    EXPECT_EQ(port.state.duty_ticks, 25U);
    EXPECT_EQ(nx_native_pwm_prepare(&port, 2000U, 80U, 81U), NX_ERROR_INVALID);
    EXPECT_EQ(port.base_period, 100U);
    EXPECT_EQ(port.state.tick_hz, 1000U);
    EXPECT_EQ(port.state.duty_ticks, 25U);
}

TEST(NativeCold, I2cDuplicateAndInvalidMemoryLeaveExistingEndpointAlone) {
    nx_native_i2c_state_t port{}, other{};
    nx_native_i2c_endpoint_state_t endpoint{};
    endpoint.port = &other;
    uint8_t memory[4]{};
    nx_native_i2c_endpoint_config_t selection[] = {
        {&endpoint, memory, sizeof(memory), 0x50U},
        {&endpoint, memory, sizeof(memory), 0x51U}};
    EXPECT_EQ(nx_native_i2c_prepare(&port, selection, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(endpoint.port, &other);
    selection[0].memory = nullptr;
    EXPECT_EQ(nx_native_i2c_prepare(&port, selection, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(endpoint.port, &other);
    EXPECT_FALSE(port.opened);
}

TEST(NativeCold, DmaOuterLoanAndForeignIdentityRejectBeforeOpeningEngine) {
    nx_native_spi_dma_state_t engine{}, other{};
    nx_spi_dma_sequence_state_t sequence{};
    sequence.context = &other;
    EXPECT_EQ(nx_native_spi_dma_prepare(&engine, &sequence), NX_ERROR_INVALID);
    EXPECT_FALSE(engine.opened);
    sequence.context = nullptr;
    sequence.pending_release = true;
    EXPECT_EQ(nx_native_spi_dma_prepare(&engine, &sequence), NX_ERROR_BUSY);
    EXPECT_FALSE(engine.opened);
    sequence.pending_release = false;
    ASSERT_EQ(nx_native_spi_dma_prepare(&engine, &sequence), NX_SUCCESS);
    EXPECT_TRUE(engine.opened);
    EXPECT_EQ(sequence.context, &engine);
    EXPECT_EQ(sequence.backend, &nx_native_spi_dma_sequence_backend);
    EXPECT_EQ(nx_native_spi_dma_prepare(&engine, &sequence), NX_ERROR_STATE);
    const nx_spi_dma_transaction_port_t port{&nx_spi_dma_sequence_port_ops,
                                             &sequence};
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(engine.opened);
    ASSERT_EQ(nx_native_spi_dma_prepare(&engine, &sequence), NX_SUCCESS);
    EXPECT_TRUE(engine.opened);
    EXPECT_FALSE(sequence.stopped);
    ASSERT_EQ(nx_spi_dma_transaction_stop(&port), NX_SUCCESS);
}

} /* namespace */
