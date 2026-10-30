/**
 * \file            gd32_official_spi_test.cpp
 * \brief           Production SPI contracts through the original GD32 library
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
NVIC_Type g_gd32_model_nvic;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

static nx_time_us_t s_now;

/** \brief Deterministic host progress does not model physical SPI timing. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++s_now;
}

class Gd32OfficialSpi : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        s_now = 100U;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
};

TEST_F(Gd32OfficialSpi, TransmitsAndReceivesThroughRealLibraryDataAccess) {
    nx_gd32_spi_state_t port = {};
    nx_gd32_spi_endpoint_state_t endpoint = {};
    ASSERT_EQ(nx_gd32_spi_initialize_at(&port, &endpoint,
                                        &nx_gd32_spi0_controller, GPIOA,
                                        GPIO_PIN_4, 2000000U, 0U),
              NX_SUCCESS);
    /* Explicit flags and shared DATA storage check library composition only;
     * no physical peer, automatic flag behavior or bus timing is simulated. */
    SPI_STAT(SPI0) = SPI_STAT_TBE | SPI_STAT_RBNE;
    const uint8_t transmit[] = {0x12U, 0x80U, 0xFEU};
    uint8_t receive[sizeof(transmit)] = {};
    size_t transferred = 0U;
    ASSERT_EQ(nx_gd32_spi_endpoint_transfer(&endpoint, transmit, receive,
                                            sizeof(transmit), 10000U,
                                            &transferred),
              NX_SUCCESS);
    EXPECT_EQ(transferred, sizeof(transmit));
    for (size_t i = 0U; i < sizeof(transmit); ++i) {
        EXPECT_EQ(receive[i], transmit[i]);
    }
    EXPECT_EQ(SPI_DATA(SPI0), 0xFEU);
    EXPECT_EQ(SPI_CTL0(SPI0) & SPI_CTL0_SPIEN, 0U);
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_4);
    EXPECT_FALSE(port.active);
    EXPECT_TRUE(port.initialized);
    ASSERT_EQ(nx_gd32_spi_stop(&port), NX_SUCCESS);
}

TEST_F(Gd32OfficialSpi, DummyAndDiscardKeepIndependentControllerOwnership) {
    nx_gd32_spi_state_t first = {};
    nx_gd32_spi_state_t second = {};
    nx_gd32_spi_endpoint_state_t first_endpoint = {};
    nx_gd32_spi_endpoint_state_t second_endpoint = {};
    ASSERT_EQ(nx_gd32_spi_initialize_at(&first, &first_endpoint,
                                        &nx_gd32_spi0_controller, GPIOA,
                                        GPIO_PIN_4, 2000000U, 0U),
              NX_SUCCESS);
    ASSERT_EQ(nx_gd32_spi_initialize_at(&second, &second_endpoint,
                                        &nx_gd32_spi4_controller, GPIOF,
                                        GPIO_PIN_6, 1000000U, 3U),
              NX_SUCCESS);
    SPI_STAT(SPI0) = SPI_STAT_TBE | SPI_STAT_RBNE;
    SPI_STAT(SPI4) = SPI_STAT_TBE | SPI_STAT_RBNE;
    uint8_t receive[2] = {};
    size_t transferred = 0U;
    ASSERT_EQ(nx_gd32_spi_endpoint_transfer(&first_endpoint, nullptr, receive,
                                            sizeof(receive), 10000U,
                                            &transferred),
              NX_SUCCESS);
    EXPECT_EQ(transferred, sizeof(receive));
    EXPECT_EQ(receive[0], UINT8_MAX);
    EXPECT_EQ(receive[1], UINT8_MAX);
    const uint8_t transmit[] = {0x39U, 0x91U};
    ASSERT_EQ(nx_gd32_spi_endpoint_transfer(&second_endpoint, transmit, nullptr,
                                            sizeof(transmit), 10000U,
                                            &transferred),
              NX_SUCCESS);
    EXPECT_EQ(SPI_DATA(SPI0), UINT8_MAX);
    EXPECT_EQ(SPI_DATA(SPI4), 0x91U);
    EXPECT_EQ(GPIO_BOP(GPIOA), GPIO_PIN_4);
    EXPECT_EQ(GPIO_BOP(GPIOF), GPIO_PIN_6);
    EXPECT_EQ(SPI_CTL0(SPI4) & (SPI_CTL0_CKPH | SPI_CTL0_CKPL),
              SPI_CTL0_CKPH | SPI_CTL0_CKPL);
    ASSERT_EQ(nx_gd32_spi_stop(&first), NX_SUCCESS);
    EXPECT_TRUE(second.initialized);
    ASSERT_EQ(nx_gd32_spi_stop(&second), NX_SUCCESS);
}

TEST_F(Gd32OfficialSpi, RejectedPayloadHasNoWireOrDataEffects) {
    nx_gd32_spi_state_t port = {};
    nx_gd32_spi_endpoint_state_t endpoint = {};
    ASSERT_EQ(nx_gd32_spi_initialize_at(&port, &endpoint,
                                        &nx_gd32_spi0_controller, GPIOA,
                                        GPIO_PIN_4, 2000000U, 0U),
              NX_SUCCESS);
    SPI_DATA(SPI0) = 0xA5U;
    GPIO_BOP(GPIOA) = UINT32_C(0x12345678);
    const uint32_t control = SPI_CTL0(SPI0);
    const uint32_t clock = RCU_APB2EN;
    size_t transferred = 0U;
    EXPECT_EQ(nx_gd32_spi_endpoint_transfer(&endpoint, nullptr, nullptr, 1U,
                                            10000U, &transferred),
              NX_ERROR_INVALID);
    EXPECT_EQ(SPI_DATA(SPI0), 0xA5U);
    EXPECT_EQ(GPIO_BOP(GPIOA), UINT32_C(0x12345678));
    EXPECT_EQ(SPI_CTL0(SPI0), control);
    EXPECT_EQ(RCU_APB2EN, clock);
    EXPECT_FALSE(port.active);
    ASSERT_EQ(nx_gd32_spi_stop(&port), NX_SUCCESS);
}
