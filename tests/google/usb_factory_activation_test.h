/**
 * \file            usb_factory_activation_test.h
 * \brief           Actual generated activation lookup and hardware exclusion
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_FACTORY_ACTIVATION_TEST_H
#define NEXUS_USB_FACTORY_ACTIVATION_TEST_H

#include "usb_factory_fixture.h"

/** \brief Restore both hardware mappings even after a fatal test assertion. */
class UsbFactoryHardwareReadGuard {
  public:
    UsbFactoryHardwareReadGuard(void* clocks, void* core)
        : clocks_(clocks), core_(core),
          clocks_result_(mprotect(clocks_, 0x80000U, PROT_NONE)),
          core_result_(mprotect(core_, 0x10000U, PROT_NONE)) {
    }
    ~UsbFactoryHardwareReadGuard() {
        if (core_result_ == 0) {
            EXPECT_EQ(mprotect(core_, 0x10000U, PROT_READ | PROT_WRITE), 0);
        }
        if (clocks_result_ == 0) {
            EXPECT_EQ(mprotect(clocks_, 0x80000U, PROT_READ | PROT_WRITE), 0);
        }
    }
    UsbFactoryHardwareReadGuard(const UsbFactoryHardwareReadGuard&) = delete;
    UsbFactoryHardwareReadGuard&
    operator=(const UsbFactoryHardwareReadGuard&) = delete;
    bool Protected() const {
        return clocks_result_ == 0 && core_result_ == 0;
    }

  private:
    void* clocks_;
    void* core_;
    int clocks_result_;
    int core_result_;
};

TEST_F(UsbFactory, ActivationGetterReusesControllerWithoutHardwareOrStartup) {
    EXPECT_CALL(ports, Preflight()).Times(0);
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    const auto reads = hardware->mmio_reads;
    const auto writes = hardware->mmio_writes;
    std::array<uint8_t, sizeof(*NVIC)> before_nvic = {};
    std::memcpy(before_nvic.data(), NVIC, before_nvic.size());
    ASSERT_FALSE(state->snapshot.started);
    ASSERT_FALSE(state->clock_retained);
    {
        UsbFactoryHardwareReadGuard no_hardware(clocks, hardware);
        ASSERT_TRUE(no_hardware.Protected());
        const auto* activation = nx_factory_usb_activation(NX_USB_DEVICE_ID_FS);
        ASSERT_NE(activation, nullptr);
        EXPECT_EQ(activation, nx_factory_usb_activation(NX_USB_DEVICE_ID_FS));
        EXPECT_EQ(activation->context, face->context);
        EXPECT_NE(activation->context, bulk->context);
        EXPECT_EQ(activation->ops, &nx_usb_fs_activation_ops);
        EXPECT_EQ(nx_factory_usb_activation(NX_USB_DEVICE_ID_COUNT), nullptr);
        EXPECT_EQ(nx_factory_usb_activation(
                      static_cast<nx_usb_device_id_t>(UINT16_MAX)),
                  nullptr);
        EXPECT_FALSE(state->snapshot.started);
        EXPECT_FALSE(state->clock_retained);
    }
    EXPECT_EQ(hardware->mmio_reads, reads);
    EXPECT_EQ(hardware->mmio_writes, writes);
    EXPECT_EQ(std::memcmp(before_nvic.data(), NVIC, before_nvic.size()), 0);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
#endif
