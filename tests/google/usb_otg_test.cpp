/**
 * \file            usb_otg_test.cpp
 * \brief           Shared GPIO/PHY/IRQ fixture for real F407 cold role tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f407_usb_otg.h"
#include "usb_device.h"
}
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
#include <algorithm>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <vector>

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class OtgHcd {
  public:
    MOCK_METHOD(HAL_StatusTypeDef, Init, (HCD_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Start, (HCD_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Stop, (HCD_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Open,
                (HCD_HandleTypeDef*, uint8_t, uint8_t, uint8_t, uint8_t,
                 uint8_t, uint16_t));
    MOCK_METHOD(HAL_StatusTypeDef, Submit,
                (HCD_HandleTypeDef*, uint8_t, uint8_t, uint8_t, uint8_t,
                 uint8_t*, uint16_t, uint8_t));
    MOCK_METHOD(HAL_StatusTypeDef, Packet,
                (USB_OTG_GlobalTypeDef*, USB_OTG_HCTypeDef*, uint8_t));
    MOCK_METHOD(HAL_StatusTypeDef, Halt, (HCD_HandleTypeDef*, uint8_t));
    MOCK_METHOD(HAL_StatusTypeDef, FlushRx, (USB_OTG_GlobalTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, FlushTx, (USB_OTG_GlobalTypeDef*, uint32_t));
    MOCK_METHOD(void, Irq, (HCD_HandleTypeDef*));
    static OtgHcd* active;
};
OtgHcd* OtgHcd::active;
uint32_t g_nx_usb_host_hclk_hz = 168000000U;
static bool s_irq[82];
static bool s_mode_ack = true, s_mode_seen = false;
static nx_time_us_t s_mode_started = 0U;
static uint16_t s_pin_snapshot = 0U;
static unsigned s_mode_polls = 0U;
extern "C" void nx_usb_host_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    uintptr_t p = reinterpret_cast<uintptr_t>(reg);
    if (((p >= 0x50000500U && p < 0x50000700U) ||
         (p >= 0x40040500U && p < 0x40040700U)) &&
        (p & 31U) == 8U)
        *reg &= ~value;
    else
        *reg = value;
}
extern "C" void nx_usb_host_model_irq_enable(int irq, uint8_t) {
    s_irq[irq] = true;
}
extern "C" void nx_usb_host_model_irq_disable(int irq) {
    s_irq[irq] = false;
}
extern "C" bool nx_usb_host_model_irq_enabled(int irq) {
    return s_irq[irq];
}
extern "C" bool nx_usb_host_model_irq_active(int) {
    return false;
}
extern "C" void nx_usb_clock_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    *reg = value;
}
extern "C" void nx_usb_clock_model_poll(void) {
    ++s_mode_polls;
    (void)nx_native_clock_advance(1000U);
    auto* core = reinterpret_cast<USB_OTG_GlobalTypeDef*>(0x50000000U);
    if ((core->GUSBCFG & USB_OTG_GUSBCFG_FDMOD) != 0U &&
        (core->GINTSTS & USB_OTG_GINTSTS_CMOD) != 0U) {
        if (!s_mode_seen) {
            s_mode_seen = true;
            s_mode_started = nx_time_now_us();
        }
        if (s_mode_ack && nx_time_now_us() - s_mode_started >= 50000U)
            core->GINTSTS &= ~static_cast<uint32_t>(USB_OTG_GINTSTS_CMOD);
    }
}
extern "C" HAL_StatusTypeDef HAL_HCD_Init(HCD_HandleTypeDef* h) {
    return OtgHcd::active->Init(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_Start(HCD_HandleTypeDef* h) {
    return OtgHcd::active->Start(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_Stop(HCD_HandleTypeDef* h) {
    return OtgHcd::active->Stop(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_HC_Init(HCD_HandleTypeDef* h, uint8_t ch,
                                             uint8_t ep, uint8_t address,
                                             uint8_t speed, uint8_t type,
                                             uint16_t mps) {
    return OtgHcd::active->Open(h, ch, ep, address, speed, type, mps);
}
extern "C" HAL_StatusTypeDef
HAL_HCD_HC_SubmitRequest(HCD_HandleTypeDef* h, uint8_t ch, uint8_t direction,
                         uint8_t type, uint8_t token, uint8_t* bytes,
                         uint16_t length, uint8_t ping) {
    return OtgHcd::active->Submit(h, ch, direction, type, token, bytes, length,
                                  ping);
}
extern "C" HAL_StatusTypeDef HAL_HCD_HC_Halt(HCD_HandleTypeDef* h, uint8_t ch) {
    return OtgHcd::active->Halt(h, ch);
}
extern "C" HAL_StatusTypeDef
USB_HC_StartXfer(USB_OTG_GlobalTypeDef* c, USB_OTG_HCTypeDef* ch, uint8_t dma) {
    return OtgHcd::active->Packet(c, ch, dma);
}
extern "C" void HAL_HCD_IRQHandler(HCD_HandleTypeDef* h) {
    OtgHcd::active->Irq(h);
}
extern "C" HAL_StatusTypeDef USB_FlushRxFifo(USB_OTG_GlobalTypeDef* c) {
    return OtgHcd::active->FlushRx(c);
}
extern "C" HAL_StatusTypeDef USB_FlushTxFifo(USB_OTG_GlobalTypeDef* c,
                                             uint32_t fifo) {
    return OtgHcd::active->FlushTx(c, fifo);
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned port) {
    RCC->AHB1ENR |= 1U << port;
    return NX_SUCCESS;
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef*, uint8_t) {
    return s_pin_snapshot;
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef*, uint8_t, uint8_t,
                                              uint8_t, uint8_t, bool) {
    return NX_SUCCESS;
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef*, uint8_t, uint16_t) {
}

class UsbOtgCold : public testing::Test {
  protected:
    StrictMock<OtgHcd> hcd;
    nx_stm32_usb_otg_t owner = {};
    nx_stm32_usb_clock_state_t clock = {};
    nx_stm32_usb_host_channel_t channels[2] = {};
    nx_usb_endpoint_t endpoints[4] = {};
    nx_usb_event_t events[4] = {};
    nx_usb_request_t request = {}, late = {};
    nx_usb_host_request_t host_request = {};
    uint8_t bytes[8] = {1U, 2U, 3U};
    uint16_t saved[2] = {};
    uint16_t other_saved[2] = {};
    nx_stm32_platform_pin_config_t pins[2] = {{GPIOA, &saved[0], 0U, 11U},
                                              {GPIOA, &saved[1], 0U, 12U}};
    nx_stm32_platform_pin_config_t different_pins[2] = {};
    nx_stm32_usb_endpoint_config_t plan[4] = {
        {0U, true, NX_USB_PROFILE_CONTROL, 64U, 128U, 32U},
        {0U, false, NX_USB_PROFILE_CONTROL, 64U, 0U, 0U},
        {1U, true, NX_USB_PROFILE_BULK, 64U, 160U, 32U},
        {1U, false, NX_USB_PROFILE_BULK, 64U, 0U, 0U}};
    nx_stm32_usb_cold_config_t device = {};
    nx_stm32_usb_host_cold_config_t host = {};
    nx_stm32_usb_otg_config_t config = {};
    const nx_usb_otg_port_t port = {&nx_stm32_usb_otg_ops, &owner};
    nx_stm32_usb_otg_endpoint_t endpoint_state = {&owner, &endpoints[2]};
    const nx_usb_endpoint_port_t endpoint = {&nx_stm32_usb_otg_endpoint_ops,
                                             &endpoint_state};
    nx_native_usb_hardware_t* hardware = nullptr;
    uint32_t levels[3] = {0U, 16U, 1U};
    unsigned power_reads = 0U, power_writes = 0U, host_starts = 0U;
    unsigned host_irqs = 0U, host_stops = 0U;
    bool follow_source = true, hold_vbus_high = false;
    bool source_off_while_clocked = false;
    std::vector<bool> source_commands;
    struct Gpio {
        UsbOtgCold* self;
        unsigned index;
    };
    Gpio gpio_contexts[3] = {{this, 0U}, {this, 1U}, {this, 2U}};
    static nx_result_t Read(const void* c, uint32_t* out) {
        const auto* gpio = static_cast<const Gpio*>(c);
        ++gpio->self->power_reads;
        *out = gpio->self->levels[gpio->index];
        return NX_SUCCESS;
    }
    static nx_result_t Write(void* c, uint32_t set, uint32_t reset) {
        auto* gpio = static_cast<Gpio*>(c);
        auto& f = *gpio->self;
        bool old = f.levels[0] != 0U;
        f.levels[gpio->index] = (f.levels[gpio->index] | set) & ~reset;
        ++f.power_writes;
        if (gpio->index == 0U) {
            bool on = f.levels[0] != 0U;
            f.source_commands.push_back(on);
            if (!on && old && (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) != 0U)
                f.source_off_while_clocked = true;
            if (f.follow_source && on)
                f.levels[1] = 16U;
            if (f.follow_source && old && !on && !f.hold_vbus_high)
                f.levels[1] = 0U;
        }
        return NX_SUCCESS;
    }
    static constexpr nx_gpio_ops_t gpio_ops = {Write, Read, nullptr};
    const nx_gpio_port_t gpio_faces[3] = {{&gpio_ops, &gpio_contexts[0]},
                                          {&gpio_ops, &gpio_contexts[1]},
                                          {&gpio_ops, &gpio_contexts[2]}};
    nx_result_t Prepare() {
        return nx_stm32_usb_otg_prepare(&owner, &config);
    }
    nx_usb_otg_state_t Snapshot() {
        nx_usb_otg_state_t snapshot = {};
        EXPECT_EQ(nx_usb_otg_state(&port, &snapshot), NX_SUCCESS);
        return snapshot;
    }
    nx_result_t Request(nx_usb_otg_role_t role, nx_time_us_t deadline = 0U) {
        if (deadline == 0U)
            deadline = nx_deadline_after(nx_time_now_us(), 1000000U);
        return nx_usb_otg_request(&port, Snapshot().generation, role, deadline);
    }
    nx_result_t Drive(unsigned steps = 32U) {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < steps; ++i) {
            result = nx_usb_otg_service(&port);
            nx_usb_otg_state_t snapshot = {};
            if (nx_usb_otg_state(&port, &snapshot) != NX_SUCCESS)
                return NX_ERROR_STATE;
            if (!snapshot.busy)
                return result;
        }
        return result;
    }
    nx_result_t ActivateIn() {
        const nx_usb_activation_port_t activation = {
            &nx_stm32_usb_otg_activation_ops, &owner};
        return nx_usb_activation_set(&activation, owner.device.snapshot.epoch,
                                     0x81U, true);
    }
    void CompleteIn() {
        hardware->registers[0x920U / 4U] &= ~(1U << 31U);
        hardware->registers[0x928U / 4U] |= 1U;
        hardware->registers[0x938U / 4U] = 32U;
        hardware->tx_count[1] = 0U;
    }
    nx_result_t StartDevice() {
        nx_result_t result = Prepare();
        if (result == NX_SUCCESS)
            result = Request(NX_USB_OTG_DEVICE);
        return result == NX_SUCCESS ? Drive() : result;
    }
    nx_result_t StartHost() {
        levels[1] = 0U;
        nx_result_t result = Prepare();
        if (result == NX_SUCCESS)
            result = Request(NX_USB_OTG_HOST);
        return result == NX_SUCCESS ? Drive() : result;
    }
    nx_time_us_t PacketDeadline() {
        return nx_deadline_after(nx_time_now_us(), 1000000U);
    }
    USB_OTG_HostChannelTypeDef* HostChannel() {
        return reinterpret_cast<USB_OTG_HostChannelTypeDef*>(0x50000500U);
    }
    nx_result_t BeginHostPacket() {
        const auto* face = nx_usb_otg_host(&port, Snapshot().generation);
        if (face == nullptr)
            return NX_ERROR_STATE;
        const nx_usb_host_pipe_t pipe = {64U, 3U, 1U, NX_USB_HOST_FULL_SPEED,
                                         NX_USB_HOST_BULK};
        EXPECT_CALL(hcd,
                    Open(&owner.host.hal, 0U, 1U, 3U, 1U, EP_TYPE_BULK, 64U))
            .WillOnce(Return(HAL_OK));
        nx_result_t result = nx_usb_host_open(face, 0U, &pipe);
        if (result != NX_SUCCESS)
            return result;
        result =
            nx_usb_host_prepare(&host_request, bytes, sizeof(bytes),
                                sizeof(bytes), false, false, PacketDeadline());
        if (result != NX_SUCCESS)
            return result;
        EXPECT_CALL(hcd, Submit(&owner.host.hal, 0U, 0U, EP_TYPE_BULK, 1U,
                                bytes, sizeof(bytes), 0U))
            .WillOnce(Invoke([this](auto* h, auto, auto, auto, auto,
                                    auto* payload, auto, auto) {
                h->hc[0].xfer_buff = payload;
                HostChannel()->HCCHAR |= USB_OTG_HCCHAR_CHENA;
                return HAL_OK;
            }));
        return nx_usb_host_submit(face, 0U, &host_request);
    }
    nx_result_t ReceiveSetup(const uint8_t (&setup)[8]) {
        if (hardware->rx_count == 16U)
            return NX_ERROR_OVERFLOW;
        const size_t tail = (hardware->rx_head + hardware->rx_count) % 16U;
        hardware->rx[tail] = nx_native_usb_packet_t{};
        hardware->rx[tail].status = (8U << 4U) | (6U << 17U);
        for (size_t i = 0U; i < sizeof(setup); ++i)
            hardware->rx[tail].words[i / 4U] |= static_cast<uint32_t>(setup[i])
                                                << ((i % 4U) * 8U);
        ++hardware->rx_count;
        hardware->registers[0x14U / 4U] |= 1U << 4U;
        nx_stm32_usb_otg_irq(&owner);
        return NX_SUCCESS;
    }
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x50000000U), 0x10000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        hardware = reinterpret_cast<nx_native_usb_hardware_t*>(0x50000000U);
        hardware->registers[0x10U / 4U] = 1U << 31U;
        hardware->clock_ready = true;
        OtgHcd::active = &hcd;
        for (auto& enabled : s_irq)
            enabled = false;
        s_mode_ack = true;
        s_mode_seen = false;
        s_mode_started = 0U;
        s_pin_snapshot = 0U;
        s_mode_polls = 0U;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        g_nx_usb_soc_hclk_hz = g_nx_usb_host_hclk_hz = 168000000U;
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U);
        device.clock = &clock;
        device.pins = pins;
        device.endpoints = endpoints;
        device.plan = plan;
        device.events = events;
        device.endpoint_count = 4U;
        device.capacity = 4U;
        device.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        device.poll_limit = 4U;
        device.drain_timeout_us = 20U;
        device.clock_timeout_us = 1000U;
        device.rx_fifo_words = 128U;
        device.priority = 5U;
        host.pins = pins;
        host.pin_count = 2U;
        host.init_timeout_us = 1000U;
        host.transport.channels = channels;
        host.transport.channel_count = 2U;
        host.transport.irq_policy = device.irq_policy;
        host.transport.priority = 5U;
        host.transport.drain_timeout_us = 20U;
        host.transport.rx_fifo_words = 128U;
        host.transport.nonperiodic_fifo_words =
            host.transport.periodic_fifo_words = 96U;
        host.transport.phy = NX_STM32_USB_FS_INTERNAL;
        config.device_fs = &device;
        config.host = &host;
        config.phy = NX_STM32_USB_FS_INTERNAL;
        config.source = &gpio_faces[0];
        config.present = &gpio_faces[1];
        config.overcurrent = &gpio_faces[2];
        config.source_mask = 8U;
        config.present_mask = 16U;
        config.overcurrent_mask = 1U;
        config.fault_timeout_us = 1000U;
        config.source_active_high = config.present_active_high = true;
        config.overcurrent_active_high = false;
        nx_request_initialize(&request.base);
        nx_request_initialize(&late.base);
        nx_request_initialize(&host_request.base);
        EXPECT_CALL(hcd, Init(_)).WillRepeatedly(Return(HAL_OK));
        EXPECT_CALL(hcd, Start(_)).WillRepeatedly(Invoke([this](auto* h) {
            ++host_starts;
            h->Instance->GINTSTS |= USB_OTG_GINTSTS_CMOD;
            h->Instance->GUSBCFG =
                (h->Instance->GUSBCFG &
                 ~static_cast<uint32_t>(USB_OTG_GUSBCFG_FDMOD)) |
                USB_OTG_GUSBCFG_FHMOD;
            h->Instance->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
            h->Instance->HNPTXSTS = (1U << 16U) | 96U;
            *reinterpret_cast<volatile uint32_t*>(
                reinterpret_cast<uintptr_t>(h->Instance) + 0x410U) =
                (1U << 16U) | 96U;
            return HAL_OK;
        }));
        EXPECT_CALL(hcd, Stop(_)).WillRepeatedly(Invoke([this](auto*) {
            ++host_stops;
            return HAL_OK;
        }));
        EXPECT_CALL(hcd, Halt(_, _))
            .WillRepeatedly(Invoke([](auto* h, uint8_t n) {
                auto* channel = reinterpret_cast<USB_OTG_HostChannelTypeDef*>(
                    reinterpret_cast<uintptr_t>(h->Instance) + 0x500U +
                    32U * n);
                channel->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
                return HAL_OK;
            }));
        EXPECT_CALL(hcd, FlushRx(_)).WillRepeatedly(Return(HAL_OK));
        EXPECT_CALL(hcd, FlushTx(_, _)).WillRepeatedly(Return(HAL_OK));
        EXPECT_CALL(hcd, Irq(_)).WillRepeatedly(Invoke([this](auto*) {
            ++host_irqs;
        }));
    }
    void TearDown() override {
        hardware->hold_flush = hardware->hold_disable = hardware->hold_clear =
            false;
        hardware->hold_gonak = hardware->hold_irq_disable = false;
        hold_vbus_high = false;
        levels[1] = 0U;
        levels[2] = 1U;
        s_mode_ack = true;
        if (owner.initialized) {
            nx_result_t result = NX_ERROR_BUSY;
            for (unsigned i = 0U; i < 32U && result != NX_SUCCESS; ++i)
                result = nx_stm32_usb_otg_close(&owner);
            EXPECT_EQ(result, NX_SUCCESS);
        }
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x50000000U), 0x10000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40000000U), 0x80000U), 0);
    }
};

TEST_F(UsbOtgCold, PreparedOffOwnsNoTransportClockPadOrSourceEffect) {
    const uint32_t gates = RCC->AHB2ENR;
    ASSERT_EQ(NX_SUCCESS, Prepare());
    const auto state = Snapshot();
    EXPECT_EQ(NX_USB_OTG_OFF, state.active);
    EXPECT_FALSE(state.busy);
    EXPECT_FALSE(state.source_owned);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, host_starts);
    EXPECT_EQ(gates, RCC->AHB2ENR);
    EXPECT_EQ(0U, owner.device_fs_leases);
    EXPECT_EQ(0U, owner.host_receipt.leases);
    EXPECT_EQ(nullptr, nx_usb_otg_device(&port, state.generation));
    EXPECT_EQ(nullptr, nx_usb_otg_host(&port, state.generation));
    const auto reads = power_reads;
    (void)Snapshot();
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_usb_otg_close(&owner));
    EXPECT_FALSE(owner.initialized);
}

TEST_F(UsbOtgCold, ExistingSourcePowerRejectsBeforeControllerOrSwitchWrites) {
    levels[0] = config.source_mask;
    EXPECT_NE(NX_SUCCESS, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
    EXPECT_EQ(0U, host_starts);
}

TEST_F(UsbOtgCold, SeparatePinSnapshotsCannotBecomeTwoPhysicalOwners) {
    for (size_t i = 0U; i < 2U; ++i) {
        different_pins[i] = pins[i];
        different_pins[i].snapshot = &other_saved[i];
    }
    host.pins = different_pins;
    EXPECT_NE(NX_SUCCESS, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
}

TEST_F(UsbOtgCold, InactiveHostFaceRejectsBeforeHalPowerOrRequestAdmission) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    const auto state = Snapshot();
    EXPECT_EQ(nullptr, nx_usb_otg_host(&port, state.generation));
    const nx_usb_host_pipe_t pipe = {64U, 3U, 1U, NX_USB_HOST_FULL_SPEED,
                                     NX_USB_HOST_BULK};
    auto& probe = host_request;
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_host_prepare(&probe, bytes, sizeof(bytes), sizeof(bytes),
                                  false, false, PacketDeadline()));
    const auto reads = power_reads, writes = power_writes;
    EXPECT_CALL(hcd, Open(_, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(hcd, Submit(_, _, _, _, _, _, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_STATE, nx_usb_host_open(&owner.host_face, 0U, &pipe));
    EXPECT_EQ(NX_ERROR_STATE, nx_usb_host_submit(&owner.host_face, 0U, &probe));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&probe.base));
    EXPECT_EQ(nullptr, channels[0].request);
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
}

TEST_F(UsbOtgCold, InactiveDeviceFacesRejectWithoutOverwritingCallerState) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    const auto state = Snapshot();
    EXPECT_EQ(nullptr, nx_usb_otg_device(&port, state.generation));
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&late, bytes, sizeof(bytes),
                                            NX_USB_DATA, 0U, PacketDeadline()));
    nx_usb_device_state_t output{};
    output.address = 73U;
    const auto reads = power_reads, writes = power_writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_usb_device_state(&owner.device_face, &output));
    EXPECT_EQ(73U, output.address);
    EXPECT_EQ(NX_ERROR_STATE, nx_usb_endpoint_submit(&endpoint, &late));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&late.base));
    EXPECT_EQ(nullptr, endpoints[2].request);
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
}

TEST_F(UsbOtgCold, DeviceDrainBlocksSourceAndHostUntilTheOldBorrowJoins) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    ASSERT_EQ(NX_SUCCESS, ActivateIn());
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&request, bytes, sizeof(bytes),
                                            NX_USB_DATA, 0U, PacketDeadline()));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&endpoint, &request));
    hardware->hold_flush = true;
    levels[1] = 0U;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_HOST));
    (void)Drive(8U);
    EXPECT_TRUE(Snapshot().busy);
    EXPECT_EQ(&request, endpoints[2].request);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, host_starts);
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(21U));
    (void)Drive(4U);
    EXPECT_EQ(&request, endpoints[2].request);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, host_starts);
    hardware->hold_flush = false;
    ASSERT_EQ(NX_SUCCESS, Drive());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(NX_SUCCESS, request.base.result);
    EXPECT_EQ(nullptr, endpoints[2].request);
    EXPECT_EQ(NX_USB_OTG_HOST, Snapshot().active);
    EXPECT_EQ(1U, host_starts);
    std::fill(std::begin(bytes), std::end(bytes), 0xa5U);
    request.in = nullptr;
    const auto irqs = host_irqs;
    nx_stm32_usb_otg_irq(&owner);
    EXPECT_EQ(irqs + 1U, host_irqs);
    EXPECT_EQ(0xa5U, bytes[0]);
}

TEST_F(UsbOtgCold, HostOffDischargeAndExternalVbusPrecedeDeviceColdStart) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    hold_vbus_high = true;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_DEVICE));
    (void)Drive(12U);
    EXPECT_TRUE(Snapshot().busy);
    EXPECT_GT(host_stops, 0U);
    EXPECT_EQ(nullptr, owner.host.config);
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    EXPECT_FALSE(owner.device.snapshot.started);
    EXPECT_FALSE(source_off_while_clocked);
    EXPECT_EQ(16U, levels[1]);
    follow_source = false;
    hold_vbus_high = false;
    levels[1] = 0U;
    (void)Drive(4U);
    EXPECT_FALSE(owner.device.snapshot.started);
    levels[1] = 16U;
    const auto before = nx_time_now_us();
    ASSERT_EQ(NX_SUCCESS, Drive());
    EXPECT_EQ(NX_USB_OTG_DEVICE, Snapshot().active);
    EXPECT_TRUE(owner.device.snapshot.started);
    EXPECT_FALSE(Snapshot().source_owned);
    EXPECT_GE(nx_time_now_us() - before, 50000U);
    EXPECT_EQ(0U, hardware->registers[0x14U / 4U] & USB_OTG_GINTSTS_CMOD);
    EXPECT_EQ(1U,
              std::count(source_commands.begin(), source_commands.end(), true));
}

TEST_F(UsbOtgCold, OneVectorDispatchesOnlyItsCurrentTransportEpoch) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    ASSERT_EQ(NX_SUCCESS, ActivateIn());
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&request, bytes, sizeof(bytes),
                                            NX_USB_DATA, 0U, PacketDeadline()));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&endpoint, &request));
    CompleteIn();
    nx_stm32_usb_otg_irq(&owner);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, host_irqs);
    levels[1] = 0U;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_HOST));
    ASSERT_EQ(NX_SUCCESS, Drive());
    nx_stm32_usb_otg_irq(&owner);
    EXPECT_EQ(1U, host_irqs);
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_OFF));
    ASSERT_EQ(NX_SUCCESS, Drive());
    nx_stm32_usb_otg_irq(&owner);
    EXPECT_EQ(1U, host_irqs);
}

TEST_F(UsbOtgCold, StaleGenerationCannotWithdrawTheNewHostOrItsPower) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    const auto old = Snapshot().generation;
    levels[1] = 0U;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_HOST));
    ASSERT_EQ(NX_SUCCESS, Drive());
    const auto current = Snapshot();
    ASSERT_GT(current.generation, old);
    const auto reads = power_reads, writes = power_writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_usb_otg_request(&port, old, NX_USB_OTG_OFF, PacketDeadline()));
    EXPECT_EQ(nullptr, nx_usb_otg_host(&port, old));
    EXPECT_EQ(nullptr, nx_usb_otg_device(&port, old));
    EXPECT_NE(nullptr, nx_usb_otg_host(&port, current.generation));
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
    EXPECT_EQ(current.generation, Snapshot().generation);
    EXPECT_EQ(config.source_mask, levels[0] & config.source_mask);
}

TEST_F(UsbOtgCold, OvercurrentRetainsHostPacketUntilHaltAndDrainProveOff) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    const auto* host_face = nx_usb_otg_host(&port, Snapshot().generation);
    ASSERT_NE(nullptr, host_face);
    const nx_usb_host_pipe_t pipe = {64U, 3U, 1U, NX_USB_HOST_FULL_SPEED,
                                     NX_USB_HOST_BULK};
    auto& packet = host_request;
    EXPECT_CALL(hcd, Open(&owner.host.hal, 0U, 1U, 3U, 1U, EP_TYPE_BULK, 64U))
        .WillOnce(Return(HAL_OK));
    ASSERT_EQ(NX_SUCCESS, nx_usb_host_open(host_face, 0U, &pipe));
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_host_prepare(&packet, bytes, sizeof(bytes), sizeof(bytes),
                                  false, false, PacketDeadline()));
    EXPECT_CALL(hcd, Submit(&owner.host.hal, 0U, 0U, EP_TYPE_BULK, 1U, bytes,
                            sizeof(bytes), 0U))
        .WillOnce(Invoke(
            [this](auto* h, auto, auto, auto, auto, auto* payload, auto, auto) {
                h->hc[0].xfer_buff = payload;
                HostChannel()->HCCHAR |= USB_OTG_HCCHAR_CHENA;
                return HAL_OK;
            }));
    ASSERT_EQ(NX_SUCCESS, nx_usb_host_submit(host_face, 0U, &packet));
    EXPECT_CALL(hcd, Halt(&owner.host.hal, 0U))
        .WillRepeatedly(Return(HAL_BUSY));
    levels[2] = 0U;
    (void)Drive(8U);
    EXPECT_TRUE(Snapshot().busy);
    EXPECT_EQ(&packet, channels[0].request);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&packet.base));
    EXPECT_EQ(config.source_mask, levels[0] & config.source_mask);
    EXPECT_EQ(nullptr, nx_usb_otg_host(&port, Snapshot().generation));
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(21U));
    (void)Drive(4U);
    EXPECT_EQ(&packet, channels[0].request);
    testing::Mock::VerifyAndClearExpectations(&hcd);
    EXPECT_CALL(hcd, Halt(_, _)).WillRepeatedly(Invoke([this](auto*, auto) {
        HostChannel()->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
        return HAL_OK;
    }));
    EXPECT_CALL(hcd, Stop(_)).WillRepeatedly(Return(HAL_OK));
    EXPECT_CALL(hcd, FlushRx(_)).WillRepeatedly(Return(HAL_OK));
    EXPECT_CALL(hcd, FlushTx(_, _)).WillRepeatedly(Return(HAL_OK));
    (void)Drive();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&packet.base));
    EXPECT_NE(NX_SUCCESS, packet.base.result);
    EXPECT_EQ(nullptr, channels[0].request);
    EXPECT_EQ(NX_USB_OTG_OFF, Snapshot().active);
    EXPECT_NE(NX_SUCCESS, Snapshot().fault);
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    EXPECT_FALSE(source_off_while_clocked);
}

TEST_F(UsbOtgCold, MissingDeviceModeAckNeverPublishesADeviceTransport) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    ASSERT_NE(0U, hardware->registers[0x14U / 4U] & USB_OTG_GINTSTS_CMOD);
    s_mode_ack = false;
    follow_source = false;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_DEVICE));
    (void)Drive(4U);
    levels[1] = 0U;
    (void)Drive(4U);
    levels[1] = 16U;
    const auto result = Drive();
    EXPECT_NE(NX_SUCCESS, result);
    EXPECT_FALSE(owner.device.snapshot.started);
    EXPECT_EQ(nullptr, nx_usb_otg_device(&port, Snapshot().generation));
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    EXPECT_GT(s_mode_polls, 0U);
}

TEST_F(UsbOtgCold, CloseCancelsPendingVbusWaitWithoutStartingEitherRole) {
    ASSERT_EQ(NX_SUCCESS, Prepare());
    levels[1] = 0U;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_DEVICE));
    (void)Drive(4U);
    ASSERT_TRUE(Snapshot().busy);
    nx_result_t closed = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 32U && closed != NX_SUCCESS; ++i)
        closed = nx_stm32_usb_otg_close(&owner);
    EXPECT_EQ(NX_SUCCESS, closed);
    EXPECT_FALSE(owner.initialized);
    EXPECT_FALSE(owner.device.snapshot.started);
    EXPECT_EQ(0U, host_starts);
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
}

TEST_F(UsbOtgCold, ParentConfigurationCannotAliasCompositeStorage) {
    static_assert(sizeof(owner.device) >= sizeof(config), "fixture extent");
    auto* nested = reinterpret_cast<nx_stm32_usb_otg_config_t*>(&owner.device);
    std::memcpy(nested, &config, sizeof(config));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_usb_otg_prepare(&owner, nested));
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, hardware->mmio_writes);
}

TEST_F(UsbOtgCold, HostConfigurationCannotAliasTheInactiveDeviceState) {
    static_assert(sizeof(owner.device) >= sizeof(host), "fixture extent");
    auto* nested =
        reinterpret_cast<nx_stm32_usb_host_cold_config_t*>(&owner.device);
    std::memcpy(nested, &host, sizeof(host));
    config.host = nested;
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, hardware->mmio_writes);
}

TEST_F(UsbOtgCold, DeviceConfigurationCannotAliasTheInactiveHostState) {
    static_assert(sizeof(owner.host) >= sizeof(device), "fixture extent");
    auto* nested = reinterpret_cast<nx_stm32_usb_cold_config_t*>(&owner.host);
    std::memcpy(nested, &device, sizeof(device));
    config.device_fs = nested;
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, hardware->mmio_writes);
}

TEST_F(UsbOtgCold, HostChannelsCannotBorrowTheInactiveDeviceState) {
    static_assert(sizeof(owner.device) >= sizeof(channels), "fixture extent");
    host.transport.channels =
        reinterpret_cast<nx_stm32_usb_host_channel_t*>(&owner.device);
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, hardware->mmio_writes);
}

TEST_F(UsbOtgCold, DeviceEndpointsCannotBorrowTheInactiveHostState) {
    static_assert(sizeof(owner.host) >= sizeof(endpoints), "fixture extent");
    device.endpoints = reinterpret_cast<nx_usb_endpoint_t*>(&owner.host);
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_writes);
    EXPECT_EQ(0U, hardware->mmio_writes);
}

TEST_F(UsbOtgCold, GenerationRemainsMonotonicAcrossCloseAndFreshPrepare) {
    ASSERT_EQ(NX_SUCCESS, Prepare());
    const auto previous = Snapshot().generation;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_usb_otg_close(&owner));
    ASSERT_EQ(NX_SUCCESS, Prepare());
    const auto current = Snapshot();
    EXPECT_GT(current.generation, previous);
    const auto reads = power_reads, writes = power_writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_usb_otg_request(&port, previous, NX_USB_OTG_DEVICE,
                                 PacketDeadline()));
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
}

TEST_F(UsbOtgCold, OldEp0RequestCannotEnterANewDeviceRoleSession) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    const uint8_t setup[8] = {0x80U, 6U, 0U, 1U, 0U, 0U, 8U, 0U};
    ASSERT_EQ(NX_SUCCESS, ReceiveSetup(setup));
    const auto old_epoch = owner.device.snapshot.epoch;
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_in_prepare(&late, bytes, sizeof(bytes), NX_USB_DATA,
                                old_epoch, PacketDeadline()));
    levels[1] = 0U;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_HOST));
    ASSERT_EQ(NX_SUCCESS, Drive());
    follow_source = false;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_DEVICE));
    (void)Drive(4U);
    levels[1] = 0U;
    (void)Drive(4U);
    levels[1] = 16U;
    ASSERT_EQ(NX_SUCCESS, Drive());
    ASSERT_EQ(NX_SUCCESS, ReceiveSetup(setup));
    ASSERT_GT(owner.device.snapshot.epoch, old_epoch);
    nx_stm32_usb_otg_endpoint_t control_state{&owner, &endpoints[0]};
    const nx_usb_endpoint_port_t control{&nx_stm32_usb_otg_endpoint_ops,
                                         &control_state};
    EXPECT_EQ(NX_ERROR_INVALID, nx_usb_endpoint_submit(&control, &late));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&late.base));
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_in_prepare(&request, bytes, sizeof(bytes), NX_USB_DATA,
                                owner.device.snapshot.epoch, PacketDeadline()));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&control, &request));
    const auto* face = nx_usb_otg_device(&port, Snapshot().generation);
    ASSERT_NE(nullptr, face);
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_stop(face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

TEST_F(UsbOtgCold, ShortRoleDeadlineCannotPublishAnOverdueDeviceReady) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    follow_source = false;
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_DEVICE,
                                  nx_deadline_after(nx_time_now_us(), 10000U)));
    (void)Drive(4U);
    levels[1] = 0U;
    (void)Drive(4U);
    levels[1] = 16U;
    const auto result = Drive();
    EXPECT_EQ(NX_ERROR_TIMEOUT, result);
    const auto state = Snapshot();
    EXPECT_FALSE(state.ready);
    EXPECT_EQ(NX_USB_OTG_OFF, state.active);
    EXPECT_FALSE(owner.device.snapshot.started);
    EXPECT_EQ(nullptr, nx_usb_otg_device(&port, state.generation));
    EXPECT_EQ(0U, levels[0] & config.source_mask);
}

TEST_F(UsbOtgCold, CachedHostFaceStillObservesAndDrainsRealDisconnect) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    const auto* face = nx_usb_otg_host(&port, Snapshot().generation);
    ASSERT_NE(nullptr, face);
    ASSERT_EQ(NX_SUCCESS, BeginHostPacket());
    ASSERT_NE(nullptr, owner.host.hal.ConnectCallback);
    ASSERT_NE(nullptr, owner.host.hal.DisconnectCallback);
    owner.host.hal.ConnectCallback(&owner.host.hal);
    owner.host.hal.DisconnectCallback(&owner.host.hal);
    ASSERT_TRUE(owner.host.snapshot.closing);
    nx_usb_host_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_usb_host_state(face, &state));
    EXPECT_TRUE(state.closing);
    EXPECT_FALSE(state.connected);
    EXPECT_EQ(NX_SUCCESS, nx_usb_host_service(face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&host_request.base));
    EXPECT_EQ(nullptr, channels[0].request);
    ASSERT_EQ(NX_SUCCESS, nx_usb_host_state(face, &state));
    EXPECT_FALSE(state.started);
    EXPECT_FALSE(state.connected);
}

TEST_F(UsbOtgCold, CachedDeviceFaceStillObservesARealControllerFault) {
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    const auto* face = nx_usb_otg_device(&port, Snapshot().generation);
    ASSERT_NE(nullptr, face);
    hardware->registers[0x808U / 4U] = 0U;
    hardware->registers[0x14U / 4U] |= 1U << 13U;
    nx_stm32_usb_otg_irq(&owner);
    ASSERT_TRUE(owner.device.snapshot.closing);
    const auto reads = power_reads, writes = power_writes;
    nx_usb_device_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_state(face, &state));
    EXPECT_TRUE(state.closing);
    EXPECT_TRUE(state.started);
    EXPECT_EQ(NX_SUCCESS, nx_usb_device_service(face));
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
}

TEST_F(UsbOtgCold, CloseRetainsDischargeDebtAndClearsItsCompletedDeadline) {
    ASSERT_EQ(NX_SUCCESS, StartHost());
    hold_vbus_high = true;
    nx_result_t result = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 8U; ++i)
        result = nx_stm32_usb_otg_close(&owner);
    EXPECT_EQ(NX_ERROR_BUSY, result);
    EXPECT_TRUE(owner.initialized);
    EXPECT_EQ(0U, levels[0] & config.source_mask);
    EXPECT_EQ(16U, levels[1]);
    EXPECT_EQ(nullptr, owner.host.config);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2000U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_usb_otg_close(&owner));
    EXPECT_TRUE(owner.initialized);
    hold_vbus_high = false;
    levels[1] = 0U;
    for (unsigned i = 0U; i < 32U && result != NX_SUCCESS; ++i)
        result = nx_stm32_usb_otg_close(&owner);
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, owner.role.phase);
    EXPECT_EQ(0U, owner.role.deadline);
    const auto reads = power_reads, writes = power_writes;
    const auto mmio_reads = hardware->mmio_reads;
    const auto mmio_writes = hardware->mmio_writes;
    const auto irqs = host_irqs;
    EXPECT_EQ(NX_ERROR_STATE, nx_usb_otg_service(&port));
    nx_stm32_usb_otg_irq(&owner);
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
    EXPECT_EQ(mmio_reads, hardware->mmio_reads);
    EXPECT_EQ(mmio_writes, hardware->mmio_writes);
    EXPECT_EQ(irqs, host_irqs);
}

/** \brief Managed role faces retain the one physical remote-wake owner. */
class UsbOtgRemote : public UsbOtgCold {
  protected:
    nx_usb_remote_runtime_t remote{};
    nx_usb_remote_wakeup_request_t pulse{};
    void SetUp() override {
        UsbOtgCold::SetUp();
        g_nx_stm32_system = {};
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
        device.remote = &remote;
        config.fault_timeout_us = 10000U;
        nx_request_initialize(&pulse.base);
    }
    void TearDown() override {
        hardware->hold_remote_clear = false;
        (void)nx_native_clock_advance(2001U);
        if (owner.device.snapshot.started)
            (void)nx_usb_fs_device_ops.service(&owner.device);
        UsbOtgCold::TearDown();
    }
    const nx_usb_device_port_t* Face() {
        return nx_usb_otg_device(&port, Snapshot().generation);
    }
    void Control(uint8_t number, uint16_t value) {
        const uint8_t setup[8] = {0U,
                                  number,
                                  static_cast<uint8_t>(value),
                                  static_cast<uint8_t>(value >> 8U),
                                  0U,
                                  0U,
                                  0U,
                                  0U};
        ASSERT_EQ(NX_SUCCESS, ReceiveSetup(setup));
        if (number == 5U) {
            ASSERT_EQ(NX_SUCCESS,
                      nx_usb_device_address(Face(), owner.device.snapshot.epoch,
                                            static_cast<uint8_t>(value)));
        }
        nx_stm32_usb_otg_endpoint_t context = {&owner, &endpoints[0]};
        const nx_usb_endpoint_port_t control = {&nx_stm32_usb_otg_endpoint_ops,
                                                &context};
        ASSERT_EQ(NX_SUCCESS,
                  nx_usb_in_prepare(&request, nullptr, 0U, NX_USB_STATUS,
                                    owner.device.snapshot.epoch,
                                    nx_time_now_us() + 30000U));
        ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&control, &request));
        hardware->registers[0x900U / 4U] &= ~(1U << 31U);
        hardware->registers[0x908U / 4U] |= 1U;
        hardware->registers[0x918U / 4U] = 32U;
        hardware->tx_count[0] = 0U;
        nx_stm32_usb_otg_irq(&owner);
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        ASSERT_EQ(NX_SUCCESS, request.base.result);
    }
    void ReadyPulse() {
        ASSERT_EQ(NX_SUCCESS, StartDevice());
        Control(5U, 3U);
        Control(9U, 1U);
        Control(3U, 1U);
        hardware->registers[0x808U / 4U] |= 1U;
        hardware->registers[0x14U / 4U] |= 1U << 11U;
        nx_stm32_usb_otg_irq(&owner);
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(5001U));
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_prepare(&pulse.base, nx_time_now_us() + 10000U));
        pulse.max_service_delay_us = 100U;
    }
    bool Signal() const {
        return (hardware->registers[0x804U / 4U] & 1U) != 0U;
    }
};
TEST_F(UsbOtgRemote, DeviceFaceForwardsPulseAndCancelToItsOneDcd) {
    ReadyPulse();
    ASSERT_NE(nullptr, Face());
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_remote_wakeup(Face(), &pulse));
    EXPECT_EQ(&remote, owner.device.remote);
    EXPECT_EQ(&pulse, remote.request);
    EXPECT_TRUE(Signal());
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_remote_cancel(Face(), &pulse));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(Face()));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, pulse.base.result);
    EXPECT_FALSE(Signal());
}
TEST_F(UsbOtgRemote, RuntimeInsideInactiveHostIsRejectedBeforeColdReset) {
    device.remote = reinterpret_cast<nx_usb_remote_runtime_t*>(&owner.host);
    const auto reads = power_reads;
    const auto writes = power_writes;
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(reads, power_reads);
    EXPECT_EQ(writes, power_writes);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
    device.remote = &remote;
}
TEST_F(UsbOtgRemote, RequestCannotOverwriteTheInactiveHostOrOuterRoleOwner) {
    ReadyPulse();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_usb_device_remote_wakeup(
                  Face(), reinterpret_cast<nx_usb_remote_wakeup_request_t*>(
                              &owner.host)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_usb_device_remote_wakeup(
                  Face(), reinterpret_cast<nx_usb_remote_wakeup_request_t*>(
                              &owner.role)));
    EXPECT_EQ(nullptr, remote.request);
    EXPECT_FALSE(Signal());
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&pulse.base));
}
TEST_F(UsbOtgRemote, RoleExitJoinsPulseBeforeClockAndPinsCanChangeOwner) {
    ReadyPulse();
    const nx_usb_device_port_t raw = {&nx_usb_fs_device_ops, &owner.device};
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_remote_wakeup(&raw, &pulse));
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_OFF));
    const auto generation = Snapshot().generation;
    EXPECT_EQ(NX_ERROR_BUSY, nx_usb_otg_service(&port));
    EXPECT_TRUE(Signal());
    EXPECT_EQ(NX_USB_ROLE_DEVICE, owner.transport);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    hardware->hold_remote_clear = true;
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_usb_otg_service(&port));
    EXPECT_TRUE(Signal());
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&pulse.base));
    EXPECT_EQ(NX_USB_ROLE_DEVICE, owner.transport);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
    hardware->hold_remote_clear = false;
    levels[1] = 0U;
    EXPECT_EQ(NX_SUCCESS, Drive());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    EXPECT_EQ(NX_ERROR_IO, pulse.base.result);
    EXPECT_FALSE(Signal());
    EXPECT_EQ(NX_USB_OTG_OFF, Snapshot().active);
    EXPECT_EQ(generation, Snapshot().generation);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN);
}

/** \brief HS PHY variants retain the common role and pulse contracts. */
class UsbOtgRemoteHs : public UsbOtgRemote,
                       public testing::WithParamInterface<bool> {
  protected:
    nx_stm32_usb_hs_clock_state_t hs_clock{};
    nx_stm32_usb_hs_cold_config_t hs{};
    uint16_t hs_saved[12]{};
    nx_stm32_platform_pin_config_t hs_pins[12]{};
    void SetUp() override {
        UsbOtgRemote::SetUp();
        hardware = reinterpret_cast<nx_native_usb_hardware_t*>(0x40040000U);
        hardware->registers[0x10U / 4U] = 1U << 31U;
        hardware->clock_ready = true;
        hardware->endpoint_numbers = 6U;
        const uint8_t banks[] = {0U, 0U, 1U, 1U, 1U, 1U,
                                 1U, 1U, 1U, 2U, 2U, 2U};
        const uint8_t pads[] = {3U,  5U,  0U,  1U, 5U, 10U,
                                11U, 12U, 13U, 0U, 2U, 3U};
        const uint8_t count = GetParam() ? 12U : 2U;
        for (uint8_t i = 0U; i < count; ++i) {
            const uint8_t bank = GetParam() ? banks[i] : 1U;
            const uint8_t pad =
                GetParam() ? pads[i] : static_cast<uint8_t>(14U + i);
            hs_pins[i] = {
                reinterpret_cast<GPIO_TypeDef*>(
                    reinterpret_cast<uintptr_t>(GPIOA) + bank * 0x400U),
                &hs_saved[i], bank, pad};
        }
        hs.clock = &hs_clock;
        hs.pins = hs_pins;
        hs.pin_count = count;
        hs.endpoints = endpoints;
        hs.plan = plan;
        hs.events = events;
        hs.endpoint_count = 4U;
        hs.capacity = 4U;
        hs.irq_policy = device.irq_policy;
        hs.priority = device.priority;
        hs.poll_limit = device.poll_limit;
        hs.drain_timeout_us = device.drain_timeout_us;
        hs.clock_timeout_us = device.clock_timeout_us;
        hs.rx_fifo_words = GetParam() ? 256U : device.rx_fifo_words;
        if (GetParam()) {
            plan[0].fifo_start = 256U;
            plan[2].max_packet = plan[3].max_packet = 512U;
            plan[2].fifo_start = 288U;
            plan[2].fifo_words = 128U;
        }
        hs.phy =
            GetParam() ? NX_STM32_USB_HS_ULPI : NX_STM32_USB_HS_INTERNAL_FS;
        hs.ulpi_clock_hz = GetParam() ? 60000000U : 0U;
        hs.remote = &remote;
        host.pins = hs_pins;
        host.pin_count = count;
        host.transport.phy = hs.phy;
        host.transport.ulpi_clock_hz = hs.ulpi_clock_hz;
        config.device_fs = nullptr;
        config.device_hs = &hs;
        config.phy = hs.phy;
    }
};
TEST_P(UsbOtgRemoteHs, RuntimeAndRequestJoinBeforeThePhyPassesToHost) {
    hs.remote = reinterpret_cast<nx_usb_remote_runtime_t*>(&owner.host);
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_reads);
    hs.remote = &remote;
    ReadyPulse();
    ASSERT_EQ(&remote, owner.device.remote);
    EXPECT_EQ(GetParam() ? NX_USB_CORE_HS_ULPI : NX_USB_CORE_HS_INTERNAL_FS,
              owner.device.core);
    EXPECT_EQ(1U, hs_clock.consumers);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_usb_device_remote_wakeup(
                  Face(), reinterpret_cast<nx_usb_remote_wakeup_request_t*>(
                              &owner.host)));
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_remote_wakeup(Face(), &pulse));
    ASSERT_TRUE(Signal());
    EXPECT_CALL(hcd, Init(_)).WillOnce(Invoke([this](auto*) {
        /* This assertion runs at the actual next-owner acquisition point. */
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
        EXPECT_EQ(nullptr, remote.request);
        EXPECT_FALSE(Signal());
        EXPECT_EQ(0U, hs_clock.consumers);
        EXPECT_EQ(0U, hs_clock.phase);
        EXPECT_EQ(nullptr, owner.device_hs_receipt.config);
        EXPECT_EQ(0U, owner.device_hs_receipt.leases);
        return HAL_OK;
    }));
    ASSERT_EQ(NX_SUCCESS, Request(NX_USB_OTG_HOST));
    EXPECT_EQ(NX_ERROR_BUSY, nx_usb_otg_service(&port));
    EXPECT_TRUE(Signal());
    EXPECT_EQ(NX_USB_ROLE_DEVICE, owner.transport);
    EXPECT_EQ(1U, hs_clock.consumers);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
    levels[1] = 0U;
    EXPECT_EQ(NX_SUCCESS, Drive());
    EXPECT_EQ(NX_USB_OTG_HOST, Snapshot().active);
    EXPECT_EQ(NX_USB_ROLE_HOST, owner.transport);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, pulse.base.result);
    EXPECT_FALSE(Signal());
    EXPECT_TRUE(owner.host.snapshot.started);
}
INSTANTIATE_TEST_SUITE_P(InternalFsAndExternalUlpi, UsbOtgRemoteHs,
                         testing::Values(false, true));

TEST_F(UsbOtgRemote, ReadyRequestCannotBorrowInactiveHostChannelStorage) {
    ReadyPulse();
    static_assert(sizeof(channels) >= sizeof(nx_usb_remote_wakeup_request_t),
                  "The disjointness probe needs an exact aligned extent");
    auto* alias =
        ::new (static_cast<void*>(channels)) nx_usb_remote_wakeup_request_t{};
    nx_request_initialize(&alias->base);
    ASSERT_EQ(NX_SUCCESS,
              nx_request_prepare(&alias->base, nx_time_now_us() + 10000U));
    alias->max_service_delay_us = 100U;
    const auto result = nx_usb_device_remote_wakeup(Face(), alias);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    if (result == NX_SUCCESS) {
        /* The RED still honors the actual accepted loan before cleanup. */
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
        ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(Face()));
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&alias->base));
    } else {
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&alias->base));
        EXPECT_EQ(nullptr, remote.request);
        EXPECT_FALSE(Signal());
    }
    for (auto& channel : channels)
        ::new (static_cast<void*>(&channel)) nx_stm32_usb_host_channel_t{};
}

TEST_F(UsbOtgRemote, PulseCannotOverwriteTheLivePowerFaceDuringSettlement) {
    static_assert(offsetof(nx_request_t, transferred) +
                          sizeof(nx_gpio_port_t) <=
                      sizeof(nx_request_t),
                  "The Native alias probe needs two live metadata words");
    ASSERT_EQ(NX_SUCCESS,
              nx_request_prepare(&pulse.base, nx_time_now_us() + 100000U));
    pulse.max_service_delay_us = 100U;
    /* Face words remain valid throughout prepare and actual cold startup.
     * Its context also encodes a finite future Native deadline. */
    auto* shared = reinterpret_cast<nx_gpio_port_t*>(&pulse.base.transferred);
    std::memcpy(shared, &gpio_faces[0], sizeof(*shared));
    config.source = shared;
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    Control(5U, 3U);
    Control(9U, 1U);
    Control(3U, 1U);
    hardware->registers[0x808U / 4U] |= 1U;
    hardware->registers[0x14U / 4U] |= 1U << 11U;
    nx_stm32_usb_otg_irq(&owner);
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(5001U));
    const auto result = nx_usb_device_remote_wakeup(Face(), &pulse);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    if (result == NX_SUCCESS) {
        /* Never repair an actually accepted request before the real join. */
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
        ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(Face()));
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    } else {
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&pulse.base));
        EXPECT_EQ(nullptr, remote.request);
        EXPECT_FALSE(Signal());
    }
    EXPECT_EQ(gpio_faces[0].ops, shared->ops);
    EXPECT_EQ(gpio_faces[0].context, shared->context);
    /* Restore the external face only after any real request has settled. */
    std::memcpy(shared, &gpio_faces[0], sizeof(*shared));
}

TEST_F(UsbOtgRemote, PulseCannotOverwriteThePinSnapshotNeededByRoleClose) {
    ASSERT_EQ(NX_SUCCESS,
              nx_request_prepare(&pulse.base, nx_time_now_us() + 100000U));
    pulse.max_service_delay_us = 100U;
    pins[0].snapshot = reinterpret_cast<uint16_t*>(&pulse.base.transferred);
    s_pin_snapshot = 0x135U;
    ASSERT_EQ(NX_SUCCESS, StartDevice());
    ASSERT_EQ(s_pin_snapshot, *pins[0].snapshot);
    Control(5U, 3U);
    Control(9U, 1U);
    Control(3U, 1U);
    hardware->registers[0x808U / 4U] |= 1U;
    hardware->registers[0x14U / 4U] |= 1U << 11U;
    nx_stm32_usb_otg_irq(&owner);
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(5001U));
    const auto result = nx_usb_device_remote_wakeup(Face(), &pulse);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    if (result == NX_SUCCESS) {
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(2001U));
        ASSERT_EQ(NX_SUCCESS, nx_usb_device_service(Face()));
        ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&pulse.base));
    } else {
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&pulse.base));
        EXPECT_EQ(nullptr, remote.request);
        EXPECT_FALSE(Signal());
    }
    EXPECT_EQ(s_pin_snapshot, *pins[0].snapshot);
    /* Restore only after any actual accepted borrower has fully exited. */
    *pins[0].snapshot = s_pin_snapshot;
}

TEST_F(UsbOtgRemote, RuntimeCannotEraseThePowerMethodsAtLaterColdEntry) {
    struct alignas(nx_usb_remote_runtime_t) Shared {
        nx_gpio_ops_t ops;
        uint8_t padding[sizeof(nx_usb_remote_runtime_t)];
    } storage = {gpio_ops, {}};
    const nx_gpio_port_t face = {&storage.ops, &gpio_contexts[0]};
    config.source = &face;
    device.remote = reinterpret_cast<nx_usb_remote_runtime_t*>(&storage.ops);
    EXPECT_EQ(NX_ERROR_INVALID, Prepare());
    EXPECT_FALSE(owner.initialized);
    EXPECT_EQ(0U, power_reads);
    EXPECT_EQ(gpio_ops.read, storage.ops.read);
    EXPECT_EQ(gpio_ops.write, storage.ops.write);
    /* The RED retains no stack face after its failed rejection assertion. */
    config.source = &gpio_faces[0];
    device.remote = &remote;
}
