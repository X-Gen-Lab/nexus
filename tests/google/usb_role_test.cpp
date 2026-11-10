/**
 * \file            usb_role_test.cpp
 * \brief           USB role ownership, backfeed prevention and failed drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_role.h"
#include "nexus/io/native/model.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class RoleHardware {
  public:
    MOCK_METHOD(nx_result_t, Start, (nx_usb_role_kind_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Stop, (nx_time_us_t));
    MOCK_METHOD(nx_result_t, Source, (bool));
    MOCK_METHOD(nx_result_t, Present, (bool*));
};

class UsbRole : public testing::Test {
  protected:
    StrictMock<RoleHardware> hardware;
    nx_usb_role_t role = {};
    static constexpr nx_usb_role_backend_ops_t ops = {
        [](void* c, nx_usb_role_kind_t r, nx_time_us_t d) {
            return static_cast<RoleHardware*>(c)->Start(r, d);
        },
        [](void* c, nx_time_us_t d) {
            return static_cast<RoleHardware*>(c)->Stop(d);
        },
        [](void* c, bool on) {
            return static_cast<RoleHardware*>(c)->Source(on);
        },
        [](void* c, bool* present) {
            return static_cast<RoleHardware*>(c)->Present(present);
        }};
    const nx_usb_role_config_t config = {&ops, &hardware, true, true};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        ASSERT_EQ(nx_usb_role_initialize(&role, &config), NX_SUCCESS);
    }
    void Vbus(bool present) {
        EXPECT_CALL(hardware, Present(_))
            .WillOnce(Invoke([present](bool* value) {
                *value = present;
                return NX_SUCCESS;
            }));
    }
    void StartHost() {
        ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_HOST, 1000U),
                  NX_SUCCESS);
        EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
        Vbus(false);
        EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
        EXPECT_CALL(hardware, Source(true)).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
        Vbus(true);
        EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
        EXPECT_CALL(hardware, Start(NX_USB_ROLE_HOST, 1000U))
            .WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_usb_role_service(&role), NX_SUCCESS);
        EXPECT_EQ(role.active, NX_USB_ROLE_HOST);
    }
};

TEST_F(UsbRole, HostPowerIsAppliedOnlyAfterActualVbusAbsence) {
    StartHost();
    EXPECT_TRUE(role.transport_owned);
    EXPECT_TRUE(role.source_owned);
}

TEST_F(UsbRole, ForeignVbusPreventsSourcePowerAndStart) {
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_HOST, 1000U), NX_SUCCESS);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    for (unsigned i = 0; i < 3U; ++i) {
        Vbus(true);
        EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    }
    EXPECT_FALSE(role.source_owned);
    EXPECT_EQ(role.active, NX_USB_ROLE_OFF);
}

TEST_F(UsbRole, ExplicitCancelClosesPendingPowerWaitWithoutStartingTransport) {
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_DEVICE, 1000U),
              NX_SUCCESS);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    ASSERT_EQ(nx_usb_role_cancel(&role, 2000U), NX_SUCCESS);
    EXPECT_EQ(role.target, NX_USB_ROLE_OFF);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_CANCELLED);
    EXPECT_EQ(role.phase, 0U);
    EXPECT_FALSE(role.transport_owned);
    EXPECT_FALSE(role.source_owned);
}

TEST_F(UsbRole, ExplicitOffAcknowledgesACompletedCancellation) {
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_DEVICE, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_role_cancel(&role, 2000U), NX_SUCCESS);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_usb_role_service(&role), NX_ERROR_CANCELLED);
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_OFF, 3000U), NX_SUCCESS);
    EXPECT_EQ(nx_usb_role_service(&role), NX_SUCCESS);
    EXPECT_EQ(role.result, NX_SUCCESS);
}

TEST_F(UsbRole, FailedHostDrainRetainsPowerAndBlocksNewRole) {
    StartHost();
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_DEVICE, 1000U),
              NX_SUCCESS);
    EXPECT_CALL(hardware, Stop(1000U)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_TRUE(role.transport_owned);
    EXPECT_TRUE(role.source_owned);
    EXPECT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_OFF, 1000U),
              NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Stop(1000U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(true);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(false);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(false);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(true);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Start(NX_USB_ROLE_DEVICE, 1000U))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_SUCCESS);
    EXPECT_FALSE(role.source_owned);
}

TEST_F(UsbRole, FailedStartDrainsBeforePowerWithdrawalAndReportsOriginalError) {
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_DEVICE, 1000U),
              NX_SUCCESS);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(true);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Start(NX_USB_ROLE_DEVICE, 1000U))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_TRUE(role.transport_owned);
    EXPECT_CALL(hardware, Stop(1000U)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Stop(1000U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_IO);
    EXPECT_EQ(role.active, NX_USB_ROLE_OFF);
    EXPECT_FALSE(role.transport_owned);
}

TEST_F(UsbRole, TimeoutDoesNotRevokeAnUndrainedTransportOrSwitchPower) {
    StartHost();
    ASSERT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_OFF, 110U), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_configure(true, 120U), NX_SUCCESS);
    EXPECT_CALL(hardware, Stop(110U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_IO);
    EXPECT_TRUE(role.transport_owned);
    EXPECT_TRUE(role.source_owned);
    EXPECT_NE(role.phase, 0U);
    EXPECT_CALL(hardware, Stop(110U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    EXPECT_CALL(hardware, Source(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_BUSY);
    Vbus(false);
    EXPECT_EQ(nx_usb_role_service(&role), NX_ERROR_TIMEOUT);
    EXPECT_EQ(role.phase, 0U);
}

TEST_F(UsbRole, RejectedRoleAndExpiredDeadlineHaveNoHardwareEffects) {
    EXPECT_EQ(
        nx_usb_role_request(&role, static_cast<nx_usb_role_kind_t>(3), 1000U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_HOST, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_role_request(&role, NX_USB_ROLE_HOST, 100U),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(role.active, NX_USB_ROLE_OFF);
    EXPECT_EQ(role.phase, 0U);
}

TEST_F(UsbRole, DisallowedRoleAndIndependentInstanceRemainUntouched) {
    nx_usb_role_t other = {};
    const nx_usb_role_config_t only_device = {&ops, &hardware, true, false};
    ASSERT_EQ(nx_usb_role_initialize(&other, &only_device), NX_SUCCESS);
    EXPECT_EQ(nx_usb_role_request(&other, NX_USB_ROLE_HOST, 1000U),
              NX_ERROR_PERMISSION);
    StartHost();
    EXPECT_EQ(other.active, NX_USB_ROLE_OFF);
    EXPECT_EQ(other.phase, 0U);
}
