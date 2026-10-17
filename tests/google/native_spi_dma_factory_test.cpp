/**
 * \file            native_spi_dma_factory_test.cpp
 * \brief           Ordinary generated Native segmented SPI and owner binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/spi_dma_owner.h"
#include "nexus/io/native/model.h"
#include "nexus/io/native/spi_dma_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "spi_dma_provider.h"
#include <gtest/gtest.h>

namespace {
class NativeSPIDMAFactory : public testing::Test {
  protected:
    nx_spi_dma_owner_executor_t executor{};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[2]{};
    nx_request_t outer{};
    nx_owner_ticket_t ticket{};
    uint16_t tx[4]{0x1234U, 0x5678U, 0x9ABCU, 0xDEF0U};
    uint16_t rx[4]{};
    uint16_t dummy{0xA5A5U};
    uint16_t discard{};
    nx_spi_dma_segment_t segments[2]{{tx, rx, 1U, 0U},
                                     {tx + 1U, rx + 1U, 1U, 0U}};
    nx_spi_dma_owner_operation_t operation{nullptr, segments, 2U, 16U};
    unsigned hints = 0U;
    const nx_irq_wake_t wake{&hints,
                             [](void* p) {
                                 ++*static_cast<unsigned*>(p);
                                 return NX_ERROR_IO;
                             },
                             false};
    static const nx_spi_dma_transaction_port_t* Bus() {
        return nx_factory_spi_dma_transaction(NX_SPI_DMA_TRANSACTION_ID_BUS);
    }
    static const nx_spi_dma_owner_binding_t* Capability() {
        return nx_factory_spi_dma_owner_binding(NX_SPI_DMA_TRANSACTION_ID_BUS);
    }
    static const nx_spi_dma_transaction_endpoint_t* First() {
        return nx_factory_spi_dma_transaction_device(
            NX_SPI_DMA_TRANSACTION_DEVICE_ID_DEVICE_A);
    }
    static const nx_spi_dma_transaction_endpoint_t* Second() {
        return nx_factory_spi_dma_transaction_device(
            NX_SPI_DMA_TRANSACTION_DEVICE_ID_DEVICE_B);
    }
    static const nx_spi_dma_transaction_endpoint_t* Other() {
        return nx_factory_spi_dma_transaction_device(
            NX_SPI_DMA_TRANSACTION_DEVICE_ID_OTHER_DEVICE);
    }
    static nx_native_spi_dma_state_t*
    Hardware(const nx_spi_dma_transaction_port_t* port) {
        auto* sequence =
            static_cast<nx_spi_dma_sequence_state_t*>(port->context);
        return static_cast<nx_native_spi_dma_state_t*>(sequence->context);
    }
    static nx_native_spi_dma_endpoint_state_t*
    Device(const nx_spi_dma_transaction_endpoint_t* face) {
        auto* child =
            static_cast<nx_spi_dma_sequence_endpoint_t*>(face->context);
        return static_cast<nx_native_spi_dma_endpoint_state_t*>(
            child->endpoint);
    }
    void SetUp() override {
        ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        nx_request_initialize(&outer);
        ASSERT_EQ(nx_request_prepare(&outer, NX_DEADLINE_NEVER), NX_SUCCESS);
    }
    void TearDown() override {
        if (owner.capacity != 0U) {
            nx_bus_owner_stop(&owner);
            for (unsigned i = 0U; i < 40U && !nx_bus_owner_idle(&owner); ++i) {
                (void)nx_bus_owner_service(&owner);
            }
            EXPECT_TRUE(nx_bus_owner_idle(&owner));
        }
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    void OpenOwner() {
        ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
        ASSERT_NE(Capability(), nullptr);
        const auto execution =
            nx_spi_dma_owner_executor_port(&executor, Capability());
        const nx_owner_guard_port_t guard{
            nullptr,
            [](void*) -> uintptr_t { return nx_arch_irq_save().value; },
            [](void*, uintptr_t previous) {
                nx_arch_irq_restore({static_cast<uint32_t>(previous)});
            }};
        const nx_clock_t clock{[](void*) { return nx_time_now_us(); }, nullptr};
        ASSERT_EQ(nx_bus_owner_init(&owner, slots, 2U, guard, execution, clock,
                                    nullptr),
                  NX_SUCCESS);
        operation.endpoint = First();
    }
    void Transfer() {
        ASSERT_EQ(nx_request_prepare(&outer, NX_DEADLINE_NEVER), NX_SUCCESS);
        ASSERT_EQ(nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr,
                                          &ticket),
                  NX_SUCCESS);
        for (unsigned i = 0U;
             i < 40U && nx_request_state(&outer) != NX_REQUEST_SETTLED; ++i) {
            (void)nx_bus_owner_service(&owner);
        }
        ASSERT_EQ(nx_request_state(&outer), NX_REQUEST_SETTLED);
        EXPECT_EQ(outer.result, NX_SUCCESS);
        EXPECT_EQ(executor.inner.segments, nullptr);
        EXPECT_FALSE(executor.pending);
        EXPECT_TRUE(nx_bus_owner_idle(&owner));
    }
};
TEST_F(NativeSPIDMAFactory, ColdLookupHasStableIdentityWithoutOpeningHardware) {
    const auto* capability = Capability();
    ASSERT_NE(capability, nullptr);
    ASSERT_NE(Bus(), nullptr);
    EXPECT_EQ(capability->port, Bus());
    EXPECT_EQ(capability->contains, nx_native_spi_dma_owner_contains);
    EXPECT_EQ(capability, Capability());
    ASSERT_NE(Hardware(Bus()), nullptr);
    EXPECT_FALSE(Hardware(Bus())->opened);
    EXPECT_EQ(Hardware(Bus())->active, nullptr);
    EXPECT_EQ(Hardware(Bus())->selected, nullptr);
    const auto before = nx_time_now_us();
    EXPECT_TRUE(nx_spi_dma_owner_contains(capability, First()));
    EXPECT_TRUE(nx_spi_dma_owner_contains(capability, Second()));
    EXPECT_FALSE(nx_spi_dma_owner_contains(capability, Other()));
    EXPECT_EQ(nx_time_now_us(), before);
    EXPECT_FALSE(Device(First())->cs);
    EXPECT_FALSE(Device(Second())->cs);
    EXPECT_FALSE(Device(Other())->cs);
    EXPECT_EQ(nx_factory_spi_dma_owner_binding(NX_SPI_DMA_TRANSACTION_ID_COUNT),
              nullptr);
}
TEST_F(NativeSPIDMAFactory, GeneratedTwoCsUseActualHalfwordProviderAndOwner) {
    ASSERT_NO_FATAL_FAILURE(OpenOwner());
    const size_t first_count = Device(First())->assertions;
    const size_t second_count = Device(Second())->assertions;
    for (const auto* face : {First(), Second()}) {
        operation.endpoint = face;
        ASSERT_NO_FATAL_FAILURE(Transfer());
        EXPECT_EQ(outer.transferred, 2U);
        EXPECT_EQ(rx[0], tx[0]);
        EXPECT_EQ(rx[1], tx[1]);
        EXPECT_FALSE(Device(face)->cs);
    }
    EXPECT_EQ(Device(First())->assertions, first_count + 1U);
    EXPECT_EQ(Device(Second())->assertions, second_count + 1U);
    EXPECT_FALSE(Device(Other())->cs);
}
TEST_F(NativeSPIDMAFactory, ExplicitFixedCellsAndFailedHintsRemainFunctional) {
    ASSERT_NO_FATAL_FAILURE(OpenOwner());
    ASSERT_EQ(nx_spi_dma_owner_attach_wake(Capability(), &wake, 0U),
              NX_SUCCESS);
    segments[0] = {&dummy, rx, 2U, NX_SPI_MEMORY_TX_FIXED};
    segments[1] = {tx + 2U, &discard, 2U, NX_SPI_MEMORY_RX_FIXED};
    ASSERT_NO_FATAL_FAILURE(Transfer());
    EXPECT_EQ(outer.transferred, 4U);
    EXPECT_EQ(rx[0], dummy);
    EXPECT_EQ(rx[1], dummy);
    EXPECT_EQ(discard, tx[3]);
    EXPECT_GE(hints, 2U);
    EXPECT_EQ(nx_spi_dma_owner_attach_wake(Capability(), nullptr, 0U),
              NX_SUCCESS);
}
TEST_F(NativeSPIDMAFactory, OtherGeneratedControllerRejectsBeforeAdmission) {
    ASSERT_NO_FATAL_FAILURE(OpenOwner());
    operation.endpoint = Other();
    EXPECT_EQ(
        nx_spi_dma_owner_submit(&owner, &outer, &operation, nullptr, &ticket),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&outer), NX_REQUEST_READY);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    EXPECT_EQ(Hardware(Bus())->active, nullptr);
    const auto* other =
        nx_factory_spi_dma_transaction(NX_SPI_DMA_TRANSACTION_ID_OTHER_BUS);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(Hardware(other)->active, nullptr);
    EXPECT_FALSE(Device(First())->cs);
    EXPECT_FALSE(Device(Other())->cs);
}
TEST_F(NativeSPIDMAFactory,
       CompletePlatformStopStartReusesTheSameTypedObjects) {
    ASSERT_NO_FATAL_FAILURE(OpenOwner());
    ASSERT_NO_FATAL_FAILURE(Transfer());
    const auto* old_capability = Capability();
    const auto* old_face = First();
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_FALSE(Hardware(Bus())->opened);
    EXPECT_EQ(Capability(), old_capability);
    EXPECT_EQ(First(), old_face);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_TRUE(Hardware(Bus())->opened);
    ASSERT_NO_FATAL_FAILURE(Transfer());
    EXPECT_EQ(outer.transferred, 2U);
    EXPECT_EQ(rx[0], tx[0]);
    EXPECT_EQ(rx[1], tx[1]);
}
} /* namespace */
