/**
 * \file            dp83848_test.cpp
 * \brief           Actual PHY register protocol against a strict MDIO port
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/dp83848/dp83848.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
struct Mdio {
    MOCK_METHOD(nx_result_t, read, (uint8_t, uint8_t, uint16_t*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, write, (uint8_t, uint8_t, uint16_t, nx_time_us_t));
};
nx_result_t read(void* ctx, uint8_t a, uint8_t r, uint16_t* v, nx_time_us_t d) {
    return static_cast<Mdio*>(ctx)->read(a, r, v, d);
}
nx_result_t write(void* ctx, uint8_t a, uint8_t r, uint16_t v, nx_time_us_t d) {
    return static_cast<Mdio*>(ctx)->write(a, r, v, d);
}
class Dp83848 : public testing::Test {
  protected:
    testing::StrictMock<Mdio> mock;
    const nx_mdio_ops_t ops{read, write};
    const nx_mdio_port_t mdio{&ops, &mock};
    nx_dp83848_t phy{&mdio, NX_DP83848_TI_ID, 1, false, false, {}};
    void Read(uint8_t reg, uint16_t value) {
        EXPECT_CALL(mock, read(1, reg, testing::_, 1000))
            .WillOnce(testing::DoAll(testing::SetArgPointee<2>(value),
                                     testing::Return(NX_SUCCESS)));
    }
};
} /* namespace */
TEST_F(Dp83848, IdentityMismatchDoesNotConfigureUnknownPhy) {
    testing::InSequence seq;
    Read(2, 0x2000);
    Read(3, 0x5ca2);
    EXPECT_EQ(NX_SUCCESS, nx_dp83848_probe(&phy, 1000));
    EXPECT_TRUE(phy.probed);
    Read(2, 0xffff);
    Read(3, 0xffff);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_dp83848_probe(&phy, 1000));
    EXPECT_FALSE(phy.probed);
}
TEST_F(Dp83848, AutoNegotiationConfigurationDoesNotInventLink) {
    phy.probed = true;
    const nx_dp83848_mode_t mode{NX_DP83848_100_FULL | NX_DP83848_10_FULL, true,
                                 false, 0};
    testing::InSequence seq;
    EXPECT_CALL(mock, write(1, 4, 0x141, 1000))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, write(1, 0, 0x1200, 1000))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(NX_SUCCESS, nx_dp83848_configure(&phy, &mode, 1000));
    Read(0, 0x1000);
    Read(1, 0);
    Read(1, 4);
    Read(16, 5);
    nx_eth_link_t link{true, true, 100};
    EXPECT_EQ(NX_SUCCESS, nx_dp83848_link(&phy, &link, 1000));
    EXPECT_FALSE(link.up);
    EXPECT_EQ(0U, link.speed_mbps);
}
TEST_F(Dp83848, LatchLowIsReadTwiceBeforeConfirmedLink) {
    phy.probed = phy.configured = true;
    phy.mode = {0xf, true, false, 0};
    testing::InSequence seq;
    Read(0, 0x1000);
    Read(1, 0);
    Read(1, 0x24);
    Read(16, 0x17);
    nx_eth_link_t link{};
    EXPECT_EQ(NX_SUCCESS, nx_dp83848_link(&phy, &link, 1000));
    EXPECT_TRUE(link.up);
    EXPECT_TRUE(link.full_duplex);
    EXPECT_EQ(10U, link.speed_mbps);
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100;
}
TEST_F(Dp83848, InvalidModeIsRejectedWithoutMdioEffects) {
    phy.probed = true;
    const nx_dp83848_mode_t no_abilities{0, true, false, 0};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_dp83848_configure(&phy, &no_abilities, 1000));
    const nx_dp83848_mode_t unsupported_speed{0, false, true, 1000};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_dp83848_configure(&phy, &unsupported_speed, 1000));
}
TEST_F(Dp83848, PartialConfigurationFailureNeverRemainsConfigured) {
    phy.probed = phy.configured = true;
    const nx_dp83848_mode_t mode{0xf, true, false, 0};
    testing::InSequence seq;
    EXPECT_CALL(mock, write(1, 4, 0x1e1, 1000))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(mock, write(1, 0, 0x1200, 1000))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, nx_dp83848_configure(&phy, &mode, 1000));
    EXPECT_FALSE(phy.configured);
}
TEST_F(Dp83848, ResetRequiresActualSelfClearingAcknowledgement) {
    phy.probed = phy.configured = true;
    testing::InSequence seq;
    EXPECT_CALL(mock, write(1, 0, 0x8000, 1000))
        .WillOnce(testing::Return(NX_SUCCESS));
    Read(0, 0x8000);
    EXPECT_CALL(mock, read(1, 0, testing::_, 1000))
        .WillOnce(testing::Return(NX_ERROR_TIMEOUT));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dp83848_reset(&phy, 1000));
    EXPECT_FALSE(phy.configured);
}
TEST_F(Dp83848, FixedModeMismatchAndRemoteFaultPreserveOutput) {
    phy.probed = phy.configured = true;
    phy.mode = {0, false, true, 100};
    testing::InSequence seq;
    Read(0, 0x2100);
    Read(1, 4);
    Read(1, 4);
    Read(16, 7);
    nx_eth_link_t link{false, false, 0};
    EXPECT_EQ(NX_ERROR_IO, nx_dp83848_link(&phy, &link, 1000));
    EXPECT_FALSE(link.up);
    Read(0, 0x2100);
    Read(1, 4);
    Read(1, 0x14);
    Read(16, 5);
    EXPECT_EQ(NX_ERROR_IO, nx_dp83848_link(&phy, &link, 1000));
    EXPECT_EQ(0U, link.speed_mbps);
}
TEST_F(Dp83848, NsVariantMustBeExplicitlySelected) {
    testing::InSequence seq;
    Read(2, 0x2000);
    Read(3, 0x5c91);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_dp83848_probe(&phy, 1000));
    phy.expected_id = NX_DP83848_NS_ID;
    Read(2, 0x2000);
    Read(3, 0x5c91);
    EXPECT_EQ(NX_SUCCESS, nx_dp83848_probe(&phy, 1000));
}
