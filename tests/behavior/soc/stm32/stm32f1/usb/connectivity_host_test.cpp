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
#include <cstring>
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
nx_time_us_t clock_step;
nx_time_us_t fake_time;
bool hold_channel_mask;
bool fail_open;

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
    if (reg == &channel_regs().HCINTMSK && hold_channel_mask)
        return;
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
    fake_time += clock_step;
    return fake_time;
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
HAL_StatusTypeDef HAL_HCD_HC_Init(HCD_HandleTypeDef* hal, uint8_t id,
                                  uint8_t endpoint, uint8_t address, uint8_t,
                                  uint8_t type, uint16_t mps) {
    EXPECT_EQ(id, 0U);
    if (fail_open)
        return HAL_ERROR;
    hal->hc[id].ep_is_in = (endpoint & 0x80U) != 0U ? 1U : 0U;
    hal->hc[id].dev_addr = address;
    hal->hc[id].ep_type = type;
    hal->hc[id].max_packet = mps;
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
HAL_StatusTypeDef HAL_HCD_HC_SubmitRequest(HCD_HandleTypeDef* hal, uint8_t id,
                                           uint8_t direction, uint8_t type,
                                           uint8_t token, uint8_t* buffer,
                                           uint16_t length, uint8_t) {
    hal->hc[id].ep_is_in = direction;
    if (token == 0U) {
        hal->hc[id].data_pid = HC_PID_SETUP;
    } else if (type == EP_TYPE_BULK) {
        const uint8_t toggle =
            direction != 0U ? hal->hc[id].toggle_in : hal->hc[id].toggle_out;
        hal->hc[id].data_pid = toggle != 0U ? HC_PID_DATA1 : HC_PID_DATA0;
    } else {
        hal->hc[id].data_pid = HC_PID_DATA1;
    }
    hal->hc[id].xfer_buff = buffer;
    hal->hc[id].xfer_len = length;
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
        fake_time = 10U;
        clock_step = 0U;
        hold_channel_mask = fail_open = false;
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
#if defined(NX_STM32F1_USB_HOST_VIEWS)
    constexpr nx_usb_host_pipe_id_t input_pipe = 1U;
#else
    constexpr nx_usb_host_pipe_id_t input_pipe = 0U;
#endif
    const nx_usb_host_pipe_t pipe{64U, 0U, 0x80U, NX_USB_HOST_FULL_SPEED,
                                  NX_USB_HOST_CONTROL};
    ASSERT_EQ(nx_usb_host_open(&face, input_pipe, &pipe), NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 8U,
                                  false, true, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, input_pipe, &request), NX_SUCCESS);
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
#if defined(NX_STM32F1_USB_HOST_BULK)
TEST_F(ConnectivityHostTest,
       BulkRoleUsesOneDirectDeviceAndPreservesExplicitPid) {
    start();
    const nx_usb_host_pipe_t control{64U, 7U, 0U, NX_USB_HOST_FULL_SPEED,
                                     NX_USB_HOST_CONTROL};
    const nx_usb_host_pipe_t out{64U, 7U, 0x03U, NX_USB_HOST_FULL_SPEED,
                                 NX_USB_HOST_BULK};
    const nx_usb_host_pipe_t in{32U, 7U, 0x83U, NX_USB_HOST_FULL_SPEED,
                                NX_USB_HOST_BULK};
    ASSERT_EQ(nx_usb_host_open(&face, 0U, &control), NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_open(&face, 2U, &out), NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_open(&face, 3U, &in), NX_SUCCESS);
    auto wrong = in;
    wrong.device_address = 8U;
    EXPECT_EQ(nx_usb_host_open(&face, 3U, &wrong), NX_ERROR_INVALID);
    wrong = in;
    wrong.device_address = 0U;
    EXPECT_EQ(nx_usb_host_open(&face, 3U, &wrong), NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_host_open(&face, 2U, &in), NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_host_open(&face, 4U, &in), NX_ERROR_INVALID);

    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 8U,
                                  false, false, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, 2U, &request), NX_SUCCESS);
    EXPECT_EQ(host.hal.hc[0].ep_type, EP_TYPE_BULK);
    EXPECT_EQ(host.hal.hc[0].dev_addr, 7U);
    EXPECT_EQ(host.hal.hc[0].ep_is_in, 0U);
    EXPECT_EQ(host.hal.hc[0].data_pid, HC_PID_DATA0);
    EXPECT_EQ(nx_usb_host_cancel(&face, 3U, &request), NX_ERROR_INVALID);
    channel_regs().HCINT = USB_OTG_HCINT_XFRC;
    nx_stm32_usb_host_irq(&host);
    ASSERT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);

    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 19U,
                                  false, true, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, 3U, &request), NX_SUCCESS);
    EXPECT_EQ(host.hal.hc[0].ep_is_in, 1U);
    EXPECT_EQ(host.hal.hc[0].max_packet, 32U);
    EXPECT_EQ(host.hal.hc[0].data_pid, HC_PID_DATA1);
    EXPECT_EQ(host.hal.hc[0].xfer_len, 32U);
    host.hal.hc[0].xfer_count = 9U;
    channel_regs().HCINT = USB_OTG_HCINT_XFRC;
    nx_stm32_usb_host_irq(&host);
    ASSERT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    nx_result_t result = NX_ERROR_STATE;
    size_t size = 0U;
    ASSERT_EQ(nx_request_result(&request.base, &result, &size), NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(size, 9U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
}

TEST_F(ConnectivityHostTest, BulkNakWaitsForHaltAndReusesTheApplicationPid) {
    start();
    const nx_usb_host_pipe_t in{64U, 7U, 0x81U, NX_USB_HOST_FULL_SPEED,
                                NX_USB_HOST_BULK};
    ASSERT_EQ(nx_usb_host_open(&face, 3U, &in), NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 8U,
                                  false, true, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, 3U, &request), NX_SUCCESS);
    hold_halt = true;
    channel_regs().HCINT = USB_OTG_HCINT_NAK;
    nx_stm32_usb_host_irq(&host);
    EXPECT_EQ(halts, 1U);
    EXPECT_EQ(nx_usb_host_service(&face), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(channel.request, &request);
    EXPECT_EQ(nx_usb_host_close(&face, 3U), NX_ERROR_BUSY);
    EXPECT_EQ(host.hal.hc[0].xfer_buff, bytes.data());
    channel_regs().HCCHAR &= ~uint32_t{USB_OTG_HCCHAR_CHENA};
    ASSERT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    nx_result_t result = NX_SUCCESS;
    size_t size = 1U;
    ASSERT_EQ(nx_request_result(&request.base, &result, &size), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_NACK);
    EXPECT_EQ(size, 0U);
    EXPECT_EQ(host.hal.hc[0].xfer_buff, nullptr);

    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes.data(), bytes.size(), 0U,
                                  false, true, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_submit(&face, 3U, &request), NX_SUCCESS);
    EXPECT_EQ(host.hal.hc[0].data_pid, HC_PID_DATA1);
    ASSERT_EQ(nx_usb_host_cancel(&face, 3U, &request), NX_SUCCESS);
    EXPECT_EQ(halts, 1U);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_ERROR_BUSY);
    channel_regs().HCINT = USB_OTG_HCINT_ACK;
    nx_stm32_usb_host_irq(&host);
    hold_halt = false;
    ASSERT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
    ASSERT_EQ(nx_request_result(&request.base, &result, &size), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
}
#endif

} /* namespace */

#if defined(NX_STM32F1_USB_HOST_CORE)
extern "C" {
#include "usb_host_stack.h"
#include "usbh_ioreq.h"
#include "usbh_pipes.h"
}

namespace {
nx_result_t fixed_vbus(void* context, bool enabled_now) {
    if (enabled_now)
        return power ? NX_SUCCESS : NX_ERROR_STATE;
    if (context != nullptr)
        ++*static_cast<unsigned*>(context);
    return NX_SUCCESS;
}

struct EnumerationStack {
    nx_st_usb_host_stack_t state{};
    std::array<nx_st_usb_host_slot_t, 2U> slots{};
    alignas(4) std::array<std::array<uint8_t, 64U>, 2U> staging{};
    unsigned removed = 0U;
    unsigned restarts = 0U;
    nx_stm32_usb_host_t* host;
    const nx_stm32_usb_host_config_t* selected;
    const nx_st_usb_host_stack_config_t config;

    explicit EnumerationStack(const nx_usb_host_port_t& port, uint32_t timeout,
                              nx_stm32_usb_host_t* owner = nullptr)
        : host(owner), selected(owner != nullptr ? owner->config : nullptr),
          config{&port,   slots.data(), slots.size(), fixed_vbus, &removed,
                 timeout, 200000U,      restart,      this,       nullptr} {
        for (size_t i = 0U; i < slots.size(); ++i) {
            slots[i].storage = staging[i].data();
            slots[i].capacity = staging[i].size();
        }
    }

    nx_result_t begin() {
        return nx_st_usb_host_stack_initialize(&state, &config, nullptr,
                                               nullptr);
    }

    static nx_result_t restart(void* context, nx_time_us_t deadline) {
        auto* fixture = static_cast<EnumerationStack*>(context);
        ++fixture->restarts;
        if (!power || fixture->host == nullptr)
            return NX_ERROR_STATE;
        return nx_stm32_usb_host_initialize(fixture->host, fixture->selected,
                                            deadline);
    }
};

TEST_F(ConnectivityHostTest, StCoreAttachKeepsBothControlDirectionsOpen) {
    start();
    EnumerationStack selected(face, 1000U);
    ASSERT_EQ(selected.begin(), NX_SUCCESS);
    auto& stack = selected.state;
    const auto out = USBH_AllocPipe(&stack.middleware, 0x00U);
    const auto in = USBH_AllocPipe(&stack.middleware, 0x80U);
    ASSERT_EQ(out, 0U);
    ASSERT_EQ(in, 1U);
    EXPECT_EQ(USBH_OpenPipe(&stack.middleware, in, 0x80U, 0U, USBH_SPEED_FULL,
                            USBH_EP_CONTROL, 64U),
              USBH_OK);
    EXPECT_EQ(USBH_OpenPipe(&stack.middleware, out, 0x00U, 0U, USBH_SPEED_FULL,
                            USBH_EP_CONTROL, 64U),
              USBH_OK);
    EXPECT_TRUE(selected.slots[in].configured);
    EXPECT_TRUE(selected.slots[out].configured);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
}

#if defined(NX_STM32F1_USB_HOST_CORE)
TEST_F(ConnectivityHostTest, StCoreEnumeratesWithOnePhysicalChannel) {
    start();
    EnumerationStack selected(face, 100000U);
    ASSERT_EQ(selected.begin(), NX_SUCCESS);
    auto& stack = selected.state;
    /* Begin at attach. The model has published physical connection and
       port-enable, which the separately tested reset would establish. */
    stack.middleware.gState = HOST_DEV_ATTACHED;
    clock_step = 1000U;
    const std::array<uint8_t, 18U> device{18U, 1U, 0U,    2U,    0U,    0U,
                                          0U,  8U, 0x34U, 0x12U, 0x78U, 0x56U,
                                          0U,  1U, 0U,    0U,    0U,    1U};
    const std::array<uint8_t, 18U> configuration{9U, 2U,    18U,   0U, 1U, 1U,
                                                 0U, 0x80U, 25U,   9U, 4U, 0U,
                                                 0U, 0U,    0xFFU, 0U, 0U, 0U};
    const std::array<uint8_t, 18U>* descriptor = nullptr;
    size_t offset = 0U;
    unsigned setup = 0U;
    unsigned status_in = 0U;
    unsigned status_out = 0U;
    unsigned data_in = 0U;
    bool data1 = true;
    for (unsigned step = 0U;
         step < 140U && stack.middleware.gState != HOST_CHECK_CLASS; ++step) {
        ASSERT_EQ(nx_st_usb_host_stack_step(&stack), NX_SUCCESS);
        if (channel.request == nullptr)
            continue;
        auto& active_hc = host.hal.hc[0];
        EXPECT_EQ(host.hal.Init.Host_channels, 1U);
        if (active_hc.data_pid == HC_PID_SETUP) {
            EXPECT_EQ(active_hc.ep_is_in, 0U);
            EXPECT_EQ(channel.request->length, 8U);
            const auto* packet = active_hc.xfer_buff;
            if (packet[1] == USB_REQ_GET_DESCRIPTOR) {
                EXPECT_EQ(packet[0], 0x80U);
                ASSERT_TRUE(packet[3] == USB_DESC_TYPE_DEVICE ||
                            packet[3] == USB_DESC_TYPE_CONFIGURATION);
                descriptor = packet[3] == USB_DESC_TYPE_DEVICE ? &device
                                                               : &configuration;
            } else {
                EXPECT_TRUE(packet[1] == USB_REQ_SET_ADDRESS ||
                            packet[1] == USB_REQ_SET_CONFIGURATION);
                descriptor = nullptr;
            }
            offset = 0U;
            data1 = true;
            ++setup;
        } else if (channel.request->length != 0U) {
            ASSERT_EQ(active_hc.ep_is_in, 1U);
            ASSERT_NE(descriptor, nullptr);
            const size_t length = channel.request->length;
            ASSERT_LE(offset + length, descriptor->size());
            EXPECT_EQ(active_hc.data_pid, data1 ? HC_PID_DATA1 : HC_PID_DATA0);
            std::memcpy(active_hc.xfer_buff, descriptor->data() + offset,
                        length);
            active_hc.xfer_count = static_cast<uint32_t>(length);
            data1 = !data1;
            offset += length;
            ++data_in;
        } else {
            EXPECT_EQ(active_hc.data_pid, HC_PID_DATA1);
            EXPECT_EQ(active_hc.ep_is_in, descriptor == nullptr ? 1U : 0U);
            if (active_hc.ep_is_in)
                ++status_in;
            else
                ++status_out;
        }
        channel_regs().HCINT = USB_OTG_HCINT_XFRC;
        nx_stm32_usb_host_irq(&host);
    }
    EXPECT_EQ(stack.middleware.gState, HOST_CHECK_CLASS);
    EXPECT_EQ(stack.middleware.device.DevDesc.idVendor, 0x1234U);
    EXPECT_EQ(stack.middleware.device.CfgDesc.bConfigurationValue, 1U);
    EXPECT_EQ(host.logical_pipes[0].device_address, USBH_DEVICE_ADDRESS);
    EXPECT_EQ(host.logical_pipes[1].max_packet, 8U);
    EXPECT_EQ(setup, 6U);
    EXPECT_EQ(status_in, 2U);
    EXPECT_EQ(status_out, 4U);
    EXPECT_GT(data_in, 4U);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(selected.removed, 1U);
}

TEST_F(ConnectivityHostTest, StCoreCloseRetainsSupplyUntilWireAndIrqJoin) {
    start();
    EnumerationStack selected(face, 1000U);
    ASSERT_EQ(selected.begin(), NX_SUCCESS);
    auto& stack = selected.state;
    ASSERT_EQ(USBH_LL_OpenPipe(&stack.middleware, 0U, 0x00U, 0U,
                               USBH_SPEED_FULL, USBH_EP_CONTROL, 64U),
              USBH_OK);
    uint8_t setup[8] = {};
    (void)USBH_CtlSendSetup(&stack.middleware, setup, 0U);
    ASSERT_TRUE(selected.slots[0].pending);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_ERROR_BUSY);
    EXPECT_EQ(selected.removed, 0U);
    EXPECT_EQ(nx_request_state(&selected.slots[0].request.base),
              NX_REQUEST_DRAINING);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(halts, 0U);
    channel_regs().HCINT = USB_OTG_HCINT_NAK;
    nx_stm32_usb_host_irq(&host);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&selected.slots[0].request.base),
              NX_REQUEST_SETTLED);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(selected.removed, 1U);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(selected.removed, 1U);
}

TEST_F(ConnectivityHostTest, DetachRejoinsBeforeRestartingWithGuardedPower) {
    start();
    EnumerationStack selected(face, 100000U, &host);
    ASSERT_EQ(selected.begin(), NX_SUCCESS);
    selected.state.middleware.gState = HOST_DEV_ATTACHED;
    clock_step = 1000U;
    ASSERT_EQ(nx_st_usb_host_stack_step(&selected.state), NX_SUCCESS);
    ASSERT_TRUE(selected.slots[0].configured && selected.slots[1].configured);

    hprt() &= ~uint32_t{USB_OTG_HPRT_PCSTS};
    host.hal.DisconnectCallback(&host.hal);
    active = true;
    EXPECT_EQ(nx_st_usb_host_stack_step(&selected.state), NX_ERROR_BUSY);
    EXPECT_EQ(selected.restarts, 0U);
    EXPECT_EQ(selected.removed, 0U);
    EXPECT_EQ(host.config, &config);

    active = false;
    ASSERT_EQ(nx_st_usb_host_stack_step(&selected.state), NX_SUCCESS);
    EXPECT_EQ(selected.restarts, 1U);
    EXPECT_EQ(selected.removed, 0U);
    EXPECT_EQ(host.config, &config);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(host.logical_pipes[0].max_packet, 0U);
    EXPECT_EQ(host.logical_pipes[1].max_packet, 0U);
    EXPECT_EQ(channel.request, nullptr);
    EXPECT_FALSE(selected.slots[0].configured || selected.slots[1].configured);
    EXPECT_EQ(nx_st_usb_host_stack_close(&selected.state), NX_SUCCESS);
    EXPECT_EQ(selected.removed, 1U);
}

TEST_F(ConnectivityHostTest,
       ControlRebindPreservesLoanAndCachedTargetsOnFailure) {
    start();
    const nx_usb_host_pipe_t out{64U, 0U, 0x00U, NX_USB_HOST_FULL_SPEED,
                                 NX_USB_HOST_CONTROL};
    const nx_usb_host_pipe_t in{64U, 0U, 0x80U, NX_USB_HOST_FULL_SPEED,
                                NX_USB_HOST_CONTROL};
    ASSERT_EQ(nx_usb_host_open(&face, 1U, &in), NX_SUCCESS);
    queue_setup();
    nx_usb_host_request_t in_request{};
    alignas(4) std::array<uint8_t, 64U> other{};
    nx_request_initialize(&in_request.base);
    ASSERT_EQ(nx_usb_host_prepare(&in_request, other.data(), other.size(), 8U,
                                  false, true, 200U),
              NX_SUCCESS);
    nx_usb_host_storage_requirements_t geometry{};
    EXPECT_EQ(nx_usb_host_storage_requirements(&face, 1U, 8U, &geometry),
              NX_SUCCESS);
    EXPECT_EQ(geometry.capacity, 64U);
    EXPECT_EQ(nx_usb_host_open(&face, 0U, &out), NX_ERROR_BUSY);
    EXPECT_EQ(nx_usb_host_cancel(&face, 1U, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_host_submit(&face, 1U, &in_request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_usb_host_close(&face, 0U), NX_ERROR_BUSY);
    channel_regs().HCINT = USB_OTG_HCINT_XFRC;
    nx_stm32_usb_host_irq(&host);
    ASSERT_EQ(nx_usb_host_service(&face), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);

    hold_channel_mask = true;
    channel_regs().HCINTMSK = 1U;
    EXPECT_EQ(nx_usb_host_submit(&face, 1U, &in_request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&in_request.base), NX_REQUEST_READY);
    EXPECT_EQ(channel.pipe.endpoint_address, 0x00U);
    EXPECT_TRUE(channel.configured);
    hold_channel_mask = false;
    fail_open = true;
    EXPECT_EQ(nx_usb_host_submit(&face, 1U, &in_request), NX_ERROR_IO);
    EXPECT_FALSE(channel.configured);
    EXPECT_EQ(nx_request_state(&in_request.base), NX_REQUEST_READY);
    fail_open = false;
    ASSERT_EQ(nx_usb_host_submit(&face, 1U, &in_request), NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_stop(&face), NX_ERROR_BUSY);
    channel_regs().HCINT = USB_OTG_HCINT_NAK;
    nx_stm32_usb_host_irq(&host);
    ASSERT_EQ(nx_usb_host_stop(&face), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&in_request.base), NX_REQUEST_SETTLED);
    EXPECT_FALSE(enabled);
}
#endif
} /* namespace */
#endif
