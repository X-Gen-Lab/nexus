/**
 * \file            connectivity_host_test.cpp
 * \brief           F105/F107 FS HCD boundaries and ES022 channel withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "connectivity_host_model.h"
#include "drivers/usb/connectivity_host_transport.h"
#include "nexus/core/fault.h"
#include <array>
#include <gtest/gtest.h>

namespace {
alignas(USB_OTG_GlobalTypeDef) std::array<uint8_t, 0x1000U> registers;
bool enabled;
bool active;
bool pending;
bool power;
bool hold_halt;
unsigned halts;
unsigned legacy_stops;

USB_OTG_HostChannelTypeDef& channel_regs() {
    return *reinterpret_cast<USB_OTG_HostChannelTypeDef*>(
        registers.data() + USB_OTG_HOST_CHANNEL_BASE);
}

volatile uint32_t& hprt() {
    return *reinterpret_cast<volatile uint32_t*>(registers.data() +
                                                 USB_OTG_HOST_PORT_BASE);
}
} /* namespace */

extern "C" {
RCC_TypeDef nx_f1_host_rcc{};
USB_OTG_GlobalTypeDef* nx_f1_host_core =
    reinterpret_cast<USB_OTG_GlobalTypeDef*>(registers.data());

void nx_f1_host_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &channel_regs().HCINT)
        *reg &= ~value;
    else
        *reg = value;
}
void nx_f1_host_irq_enable(IRQn_Type irq, uint8_t priority) {
    EXPECT_EQ(irq, OTG_FS_IRQn);
    EXPECT_EQ(priority, 5U);
    enabled = true;
}
void nx_f1_host_irq_disable(IRQn_Type irq) {
    EXPECT_EQ(irq, OTG_FS_IRQn);
    enabled = false;
}
bool nx_f1_host_irq_enabled(IRQn_Type) {
    return enabled;
}
bool nx_f1_host_irq_active(IRQn_Type) {
    return active;
}
bool nx_f1_host_irq_pending(IRQn_Type) {
    return pending;
}
bool nx_stm32f1_usb_host_clock_ready(void) {
    return true;
}
bool nx_stm32f1_usb_host_power_ready(void) {
    return power;
}
nx_time_us_t nx_time_now_us(void) {
    return 10U;
}
void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*, unsigned) {
}
HAL_StatusTypeDef HAL_HCD_Init(HCD_HandleTypeDef*) {
    nx_f1_host_core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
    nx_f1_host_core->GCCFG |= USB_OTG_GCCFG_PWRDWN;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_HCD_Start(HCD_HandleTypeDef*) {
    hprt() |= USB_OTG_HPRT_PPWR;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_HCD_Stop(HCD_HandleTypeDef*) {
    ++legacy_stops;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_HCD_HC_Init(HCD_HandleTypeDef*, uint8_t, uint8_t, uint8_t,
                                  uint8_t, uint8_t, uint16_t) {
    channel_regs().HCINTMSK = 0x7FFU;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_HCD_HC_Halt(HCD_HandleTypeDef*, uint8_t selected) {
    EXPECT_EQ(selected, 0U);
    ++halts;
    if (!hold_halt)
        channel_regs().HCCHAR &= ~uint32_t{USB_OTG_HCCHAR_CHENA};
    return HAL_OK;
}
HAL_StatusTypeDef USB_HC_StartXfer(USB_OTG_GlobalTypeDef*, USB_OTG_HCTypeDef*) {
    channel_regs().HCCHAR |= USB_OTG_HCCHAR_CHENA;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_HCD_HC_SubmitRequest(HCD_HandleTypeDef*, uint8_t, uint8_t,
                                           uint8_t, uint8_t, uint8_t*, uint16_t,
                                           uint8_t) {
    channel_regs().HCCHAR |= USB_OTG_HCCHAR_CHENA;
    return HAL_OK;
}
void HAL_HCD_IRQHandler(HCD_HandleTypeDef* hal) {
    if ((channel_regs().HCINT & USB_OTG_HCINT_XFRC) != 0U) {
        channel_regs().HCINT &= ~uint32_t{USB_OTG_HCINT_XFRC};
        channel_regs().HCCHAR &= ~uint32_t{USB_OTG_HCCHAR_CHENA};
        hal->HC_NotifyURBChangeCallback(hal, 0U, URB_DONE);
    }
}
HAL_StatusTypeDef USB_FlushRxFifo(USB_OTG_GlobalTypeDef* core) {
    core->GINTSTS &= ~uint32_t{USB_OTG_GINTSTS_RXFLVL};
    return HAL_OK;
}
HAL_StatusTypeDef USB_FlushTxFifo(USB_OTG_GlobalTypeDef*, uint32_t) {
    return HAL_OK;
}
}

namespace {
class ConnectivityHostTest : public testing::Test {
  protected:
    nx_stm32_usb_host_t host{};
    nx_stm32_usb_host_channel_t channel{};
    nx_usb_host_port_t face{&nx_stm32_usb_host_ops, &host};
    nx_stm32_usb_host_config_t config{};
    nx_usb_host_request_t request{};
    alignas(4) std::array<uint8_t, 64U> bytes{};

    void SetUp() override {
        nx_f1_host_rcc.AHBENR = 0U;
        registers.fill(0U);
        nx_f1_host_core->GOTGCTL = USB_OTG_GOTGCTL_ASVLD;
        nx_f1_host_core->HNPTXSTS = (1U << 16U) | 96U;
        hprt() = USB_OTG_HPRT_PCSTS | USB_OTG_HPRT_PENA | (1U << 17U);
        enabled = active = pending = hold_halt = false;
        power = true;
        halts = legacy_stops = 0U;
        config.channels = &channel;
        config.rx_fifo_words = 128U;
        config.nonperiodic_fifo_words = 96U;
        config.periodic_fifo_words = 64U;
        config.drain_timeout_us = 100U;
        config.phy = NX_STM32_USB_FS_INTERNAL;
        config.channel_count = 1U;
        config.priority = 5U;
        config.irq_policy.external_irq_count = 68U;
        config.irq_policy.priority_bits = 4U;
    }

    void start() {
        ASSERT_EQ(nx_stm32_usb_host_initialize(&host, &config, 1000U),
                  NX_SUCCESS);
        host.hal.ConnectCallback(&host.hal);
        host.hal.PortEnabledCallback(&host.hal);
    }

    void queue_setup() {
        const nx_usb_host_pipe_t pipe{64U, 0U, 0U, NX_USB_HOST_FULL_SPEED,
                                      NX_USB_HOST_CONTROL};
        ASSERT_EQ(nx_usb_host_open(&face, 0, &pipe), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 8U,
                                      true, false, 200U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_host_submit(&face, 0, &request), NX_SUCCESS);
    }
};

TEST_F(ConnectivityHostTest, MissingASessionCannotPublishHostIrqOrPower) {
    nx_f1_host_core->GOTGCTL = 0U;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&host, &config, 1000U),
              NX_ERROR_STATE);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(hprt() & USB_OTG_HPRT_PPWR, 0U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
    EXPECT_EQ(nx_f1_host_rcc.AHBENR & RCC_AHBENR_OTGFSEN, 0U);
}

TEST_F(ConnectivityHostTest, OnlyFullSpeedControlEndpointZeroIsAdmitted) {
    config.channel_count = 2U;
    EXPECT_EQ(nx_stm32_usb_host_validate(&host, &config), NX_ERROR_INVALID);
    config.channel_count = 1U;
    start();
    const nx_usb_host_pipe_t bulk{64U, 1U, 1U, NX_USB_HOST_FULL_SPEED,
                                  NX_USB_HOST_BULK};
    EXPECT_NE(nx_usb_host_open(&face, 0, &bulk), NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
}

TEST_F(ConnectivityHostTest, SetupCompletesAndReturnsActualTxBytes) {
    start();
    queue_setup();
    channel_regs().HCINT = USB_OTG_HCINT_XFRC;
    nx_stm32_usb_host_irq(&host);
    EXPECT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    nx_result_t result = NX_ERROR_STATE;
    size_t bytes_sent = 0U;
    EXPECT_EQ(nx_request_result(&request.base, &result, &bytes_sent),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(bytes_sent, 8U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
}

TEST_F(ConnectivityHostTest, EarlyCancelWaitsForWireEvidenceBeforeHalt) {
    start();
    queue_setup();
    ASSERT_EQ(nx_usb_host_cancel(&face, 0, &request), NX_SUCCESS);
    EXPECT_EQ(halts, 0U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_ERROR_BUSY);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(halts, 0U);

    channel_regs().HCINT = USB_OTG_HCINT_NAK;
    nx_stm32_usb_host_irq(&host);
    EXPECT_EQ(halts, 1U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(legacy_stops, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hprt() & USB_OTG_HPRT_PPWR, 0U);
    EXPECT_EQ(nx_f1_host_core->GCCFG & USB_OTG_GCCFG_PWRDWN, 0U);
}

TEST_F(ConnectivityHostTest, FaultPreventsNewLoanAndActiveIrqRetainsGate) {
    start();
    power = false;
    active = true;
    EXPECT_EQ(nx_usb_host_service(&face), NX_ERROR_BUSY);
    EXPECT_NE(nx_f1_host_rcc.AHBENR & RCC_AHBENR_OTGFSEN, 0U);
    active = false;
    pending = true;
    EXPECT_EQ(nx_usb_host_service(&face), NX_ERROR_BUSY);
    EXPECT_NE(nx_f1_host_rcc.AHBENR & RCC_AHBENR_OTGFSEN, 0U);
    pending = false;
    EXPECT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    EXPECT_EQ(nx_f1_host_rcc.AHBENR & RCC_AHBENR_OTGFSEN, 0U);
}

TEST_F(ConnectivityHostTest, InPacketPreservesExplicitPidAndCompletionBytes) {
    start();
    const nx_usb_host_pipe_t pipe{64U, 0U, 0x80U, NX_USB_HOST_FULL_SPEED,
                                  NX_USB_HOST_CONTROL};
    ASSERT_EQ(nx_usb_host_open(&face, 0U, &pipe), NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 8U,
                                  false, true, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, 0U, &request), NX_SUCCESS);
    EXPECT_EQ(host.hal.hc[0].data_pid, HC_PID_DATA1);
    EXPECT_EQ(host.hal.hc[0].xfer_len, 64U);
    host.hal.hc[0].xfer_count = 6U;
    channel_regs().HCINT = USB_OTG_HCINT_XFRC;
    nx_stm32_usb_host_irq(&host);
    ASSERT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    size_t count = 0U;
    nx_result_t result = NX_ERROR_STATE;
    EXPECT_EQ(nx_request_result(&request.base, &result, &count), NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(count, 6U);
    EXPECT_EQ(nx_usb_host_suspend(&face, true, 2000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
}
} /* namespace */
