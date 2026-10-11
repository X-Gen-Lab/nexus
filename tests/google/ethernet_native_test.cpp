/**
 * \file            ethernet_native_test.cpp
 * \brief           Native explicit ingress/egress and independent hardware
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "ethernet.h"
#include <cstring>
#include <gtest/gtest.h>
namespace {
class EthernetNative : public testing::Test {
  protected:
    nx_native_eth_t instance{};
    nx_eth_descriptor_t tx[1]{}, rx[1]{};
    nx_eth_tx_slot_t txslot[1]{};
    nx_eth_rx_slot_t rxslot[1]{};
    alignas(4) uint8_t receive[1520]{};
    alignas(4) uint8_t frame[64]{};
    nx_dma_memory_region_t regions[1]{{1U, UINTPTR_MAX - 1U, 3U}};
    nx_eth_tx_request_t request{};
    uint16_t phy[32]{};
    const nx_eth_port_t port{&nx_eth_mac_ops, &instance.mac};
    void SetUp() override {
        auto& mac = instance.mac;
        mac.regions = regions;
        mac.region_count = 1;
        mac.tx_descriptors = tx;
        mac.rx_descriptors = rx;
        mac.tx_slots = txslot;
        mac.rx_slots = rxslot;
        mac.tx_count = mac.rx_count = 1;
        mac.drain_timeout_us = 10000;
        mac.hclk_hz = 168000000;
        mac.address[0] = 2;
        mac.address[5] = 1;
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 82, 4, 0};
        mac.source = {61, 5, 0};
        rxslot[0].data = receive;
        rxslot[0].capacity = sizeof(receive);
        instance.hardware.phy_registers = phy;
        instance.hardware.phy_address = 1;
        frame[12] = 8;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_native_eth_initialize(
                                  &instance, nx_time_now_us() + 10000));
        nx_eth_link_t link{true, true, 100};
        ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &link));
    }
};
} /* namespace */
TEST_F(EthernetNative, ExplicitEgressReturnsOwnBeforeActualProviderSettlement) {
    ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&request, frame, sizeof(frame),
                                            NX_DEADLINE_NEVER));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    uint8_t observed[64]{};
    size_t length = 0;
    EXPECT_EQ(NX_ERROR_OVERFLOW, nx_native_eth_transmit(&instance, observed, 63,
                                                        &length, NX_SUCCESS));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_transmit(&instance, observed, sizeof(observed),
                                     &length, NX_SUCCESS));
    EXPECT_EQ(64U, length);
    EXPECT_EQ(0, std::memcmp(observed, frame, 64));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    nx_eth_mac_irq(&instance.mac);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
}
TEST_F(EthernetNative, ExplicitIngressCannotOverwriteAnAcquiredLoan) {
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_receive(&instance, frame, 64, 0x300));
    nx_eth_mac_irq(&instance.mac);
    nx_eth_rx_frame_t loan{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &loan));
    EXPECT_EQ(64U, loan.length);
    frame[0] = 0x77;
    EXPECT_EQ(NX_ERROR_OVERFLOW,
              nx_native_eth_receive(&instance, frame, 64, 0x300));
    EXPECT_EQ(0, loan.data[0]);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &loan));
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_receive(&instance, frame, 64, 0x300));
    nx_eth_mac_irq(&instance.mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &loan));
    EXPECT_EQ(0x77, loan.data[0]);
    EXPECT_EQ(1U, instance.mac.snapshot.unavailable_events);
}
TEST_F(EthernetNative, MdioReadsOnlyExplicitRegisterFileAndAddress) {
    const nx_mdio_port_t mdio{&nx_eth_mdio_ops, &instance.mac};
    uint16_t value = 0;
    phy[2] = 0x2000;
    phy[3] = 0x5ca2;
    ASSERT_EQ(NX_SUCCESS,
              nx_mdio_read(&mdio, 1, 2, &value, nx_time_now_us() + 10000));
    EXPECT_EQ(0x2000, value);
    ASSERT_EQ(NX_SUCCESS,
              nx_mdio_read(&mdio, 2, 2, &value, nx_time_now_us() + 10000));
    EXPECT_EQ(0xffff, value);
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    static nx_time_us_t clock = 100;
    return ++clock;
}
TEST_F(EthernetNative,
       StopRetainsFailedIrqDetachAndLaterRecoversWithoutReplay) {
    instance.hardware.hold_irq_disable = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_native_eth_poll(&instance.mac);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_native_eth_poll(&instance.mac);
    EXPECT_EQ(NX_ERROR_IO, nx_eth_port_stop(&port));
    EXPECT_TRUE(instance.mac.retained);
    EXPECT_TRUE(instance.hardware.enabled_irq);
    instance.hardware.hold_irq_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
    EXPECT_FALSE(instance.hardware.enabled_irq);
    EXPECT_FALSE(instance.mac.retained);
}
TEST_F(EthernetNative, PendingStatusCannotBeReleasedWhileW1cIsStuck) {
    instance.hardware.hold_interrupt_clear = true;
    instance.hardware.registers[0x1014 / 4] |= 0x40;
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_native_eth_poll(&instance.mac);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    EXPECT_TRUE(instance.mac.retained);
    instance.hardware.hold_interrupt_clear = false;
    nx_native_eth_poll(&instance.mac);
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
}
TEST_F(EthernetNative, ActualPriorityIsRevalidatedBeforeKernelWakeBinding) {
    instance.mac.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82, 4, 5};
    instance.hardware.priority = 2;
    instance.hardware.priority_group = 3;
    const nx_irq_wake_t wake{nullptr, [](void*) { return NX_SUCCESS; }, true};
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_eth_port_attach_wake(&port, &wake, 5));
    EXPECT_EQ(nullptr, instance.mac.wake);
}
TEST_F(EthernetNative, EpochSurvivesStopAndPreventsReleaseOfReusedStorage) {
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_receive(&instance, frame, 64, 0x300));
    nx_eth_mac_irq(&instance.mac);
    nx_eth_rx_frame_t old{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &old));
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &old));
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_native_eth_poll(&instance.mac);
    EXPECT_EQ(NX_ERROR_BUSY, nx_eth_port_stop(&port));
    nx_native_eth_poll(&instance.mac);
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_stop(&port));
    ASSERT_EQ(NX_SUCCESS,
              nx_native_eth_initialize(&instance, nx_time_now_us() + 10000));
    nx_eth_link_t link{true, true, 100};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &link));
    ASSERT_EQ(NX_SUCCESS, nx_native_eth_receive(&instance, frame, 64, 0x300));
    nx_eth_mac_irq(&instance.mac);
    nx_eth_rx_frame_t current{};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_acquire(&port, &current));
    EXPECT_NE(old.epoch, current.epoch);
    EXPECT_EQ(NX_ERROR_STATE, nx_eth_port_release(&port, &old));
    EXPECT_EQ(NX_SUCCESS, nx_eth_port_release(&port, &current));
}
