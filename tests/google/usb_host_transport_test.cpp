/**
 * \file            usb_host_transport_test.cpp
 * \brief           Locked HCD and LL ISO packet and actual stop boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "stm32f407_usb_host.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
struct Hardware {
    std::array<std::vector<uint32_t>, 16U> tx;
    std::array<unsigned, 16U> starts{};
    std::vector<uint8_t> rx;
    size_t rx_cursor = 0U;
    uint32_t rx_status = 0U;
    bool hold_halt = false;
    bool hold_dma_zero = false;
    bool hold_channel_ack = false;
    const volatile uint32_t* late_error_trigger = nullptr;
};
std::array<Hardware, 2U> hardware;
std::array<bool, 82U> irq_enabled{};
USB_OTG_GlobalTypeDef* Core(uintptr_t address) {
    if (address >= USB_OTG_FS_PERIPH_BASE &&
        address < USB_OTG_FS_PERIPH_BASE + 0x20000U)
        return USB_OTG_FS;
    if (address >= USB_OTG_HS_PERIPH_BASE &&
        address < USB_OTG_HS_PERIPH_BASE + 0x20000U)
        return USB_OTG_HS;
    return nullptr;
}
Hardware& Device(const USB_OTG_GlobalTypeDef* core) {
    return hardware[core == USB_OTG_HS ? 1U : 0U];
}
USB_OTG_HostTypeDef* Host(const USB_OTG_GlobalTypeDef* core) {
    return reinterpret_cast<USB_OTG_HostTypeDef*>(
        reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_BASE);
}
USB_OTG_HostChannelTypeDef* Channel(const USB_OTG_GlobalTypeDef* core,
                                    size_t index) {
    return reinterpret_cast<USB_OTG_HostChannelTypeDef*>(
        reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_CHANNEL_BASE +
        index * 32U);
}
void Summaries(USB_OTG_GlobalTypeDef* core) {
    uint32_t pending = 0U;
    for (size_t i = 0U; i < 16U; ++i) {
        auto* channel = Channel(core, i);
        if ((channel->HCINT & channel->HCINTMSK) != 0U)
            pending |= 1U << i;
    }
    Host(core)->HAINT = pending;
    core->GINTSTS &=
        ~static_cast<uint32_t>(USB_OTG_GINTSTS_HCINT | USB_OTG_GINTSTS_RXFLVL);
    if ((pending & Host(core)->HAINTMSK) != 0U)
        core->GINTSTS |= USB_OTG_GINTSTS_HCINT;
    if (Device(core).rx_status != 0U)
        core->GINTSTS |= USB_OTG_GINTSTS_RXFLVL;
}
void Flush(USB_OTG_GlobalTypeDef* core, uint32_t flags) {
    if ((flags & USB_OTG_GRSTCTL_RXFFLSH) != 0U) {
        Device(core).rx.clear();
        Device(core).rx_status = 0U;
    }
    if ((flags & USB_OTG_GRSTCTL_TXFFLSH) != 0U) {
        for (auto& words : Device(core).tx)
            words.clear();
        core->HNPTXSTS = (4U << 16U) | (core->DIEPTXF0_HNPTXFSIZ >> 16U);
        Host(core)->HPTXSTS = (4U << 16U) | (core->HPTXFSIZ >> 16U);
    }
    Summaries(core);
}
} /* namespace */

extern "C" uint32_t nx_usb_transport_read(const volatile uint32_t* reg) {
    auto* core = Core(reinterpret_cast<uintptr_t>(reg));
    if (core == nullptr)
        return *reg;
    const uintptr_t offset =
        reinterpret_cast<uintptr_t>(reg) - reinterpret_cast<uintptr_t>(core);
    if (offset == 0x1000U) {
        auto& hw = Device(core);
        uint32_t value = 0U;
        for (unsigned lane = 0U; lane < 4U; ++lane) {
            if (hw.rx_cursor < hw.rx.size())
                value |= static_cast<uint32_t>(hw.rx[hw.rx_cursor++])
                         << (lane * 8U);
        }
        return value;
    }
    if (offset == 0x10U) {
        const uint32_t flags = core->GRSTCTL;
        core->GRSTCTL &= ~static_cast<uint32_t>(USB_OTG_GRSTCTL_CSRST |
                                                USB_OTG_GRSTCTL_RXFFLSH |
                                                USB_OTG_GRSTCTL_TXFFLSH);
        Flush(core, flags);
    }
    if (offset >= 0x500U && offset < 0x700U && offset % 32U == 0U) {
        auto* channel = Channel(core, (offset - 0x500U) / 32U);
        if (!Device(core).hold_halt &&
            (channel->HCCHAR & (USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS)) ==
                (USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS)) {
            channel->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA |
                                                      USB_OTG_HCCHAR_CHDIS);
            channel->HCINT |= USB_OTG_HCINT_CHH;
        }
    }
    Summaries(core);
    return *reg;
}
extern "C" void nx_usb_transport_write(volatile uint32_t* reg, uint32_t value) {
    auto* core = Core(reinterpret_cast<uintptr_t>(reg));
    if (core == nullptr) {
        *reg = value;
        return;
    }
    const uintptr_t offset =
        reinterpret_cast<uintptr_t>(reg) - reinterpret_cast<uintptr_t>(core);
    if (offset >= 0x1000U && offset < 0x11000U && offset % 0x1000U == 0U) {
        const size_t channel = offset / 0x1000U - 1U;
        const bool periodic =
            (Channel(core, channel)->HCCHAR & USB_OTG_HCCHAR_EPTYP) ==
            static_cast<uint32_t>(EP_TYPE_ISOC << 18U);
        volatile uint32_t* available =
            periodic ? &Host(core)->HPTXSTS : &core->HNPTXSTS;
        if ((*available & 0xFFFFU) != 0U) {
            Device(core).tx[channel].push_back(value);
            --*available;
        }
        return;
    }
    if (offset == 0x10U) {
        *reg = (*reg & (USB_OTG_GRSTCTL_AHBIDL | USB_OTG_GRSTCTL_DMAREQ)) |
               (value & 0x7F1U);
    } else if (offset == 0x14U) {
        *reg &= ~(value & ~static_cast<uint32_t>(USB_OTG_GINTSTS_CMOD));
    } else if (offset >= 0x508U && offset < 0x700U && offset % 32U == 8U) {
        if (!Device(core).hold_channel_ack)
            *reg &= ~value;
    } else if (offset >= 0x514U && offset < 0x700U && offset % 32U == 20U &&
               Device(core).hold_dma_zero && value == 0U) {
        return;
    } else {
        if (offset >= 0x500U && offset < 0x700U && offset % 32U == 0U &&
            (*reg & USB_OTG_HCCHAR_CHENA) == 0U &&
            (value & (USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS)) ==
                USB_OTG_HCCHAR_CHENA)
            ++Device(core).starts[(offset - 0x500U) / 32U];
        *reg = value;
        if (offset >= 0x500U && offset < 0x700U && offset % 32U == 0U &&
            !Device(core).hold_halt &&
            (value & (USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS)) ==
                (USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS)) {
            *reg &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA |
                                           USB_OTG_HCCHAR_CHDIS);
            Channel(core, (offset - 0x500U) / 32U)->HCINT |= USB_OTG_HCINT_CHH;
        }
    }
    if (value == 0U && reg == Device(core).late_error_trigger) {
        Channel(core, 0U)->HCINT |= USB_OTG_HCINT_AHBERR;
        Device(core).late_error_trigger = nullptr;
    }
    Summaries(core);
}
extern "C" uint32_t nx_usb_transport_pop_rx(USB_OTG_GlobalTypeDef* core) {
    const uint32_t value = Device(core).rx_status;
    Device(core).rx_status = 0U;
    Summaries(core);
    return value;
}
extern "C" void nx_usb_transport_irq_enable(int irq, uint8_t priority) {
    (void)priority;
    irq_enabled[static_cast<size_t>(irq)] = true;
}
extern "C" void nx_usb_transport_irq_disable(int irq) {
    irq_enabled[static_cast<size_t>(irq)] = false;
}
extern "C" bool nx_usb_transport_irq_enabled(int irq) {
    return irq_enabled[static_cast<size_t>(irq)];
}
extern "C" void HAL_Delay(uint32_t milliseconds) {
    EXPECT_EQ(
        nx_native_clock_advance(static_cast<uint64_t>(milliseconds) * 1000U),
        NX_SUCCESS);
    for (auto* core : {USB_OTG_FS, USB_OTG_HS}) {
        if ((core->GUSBCFG & USB_OTG_GUSBCFG_FHMOD) != 0U)
            core->GINTSTS |= USB_OTG_GINTSTS_CMOD;
    }
}
extern "C" uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(nx_time_now_us() / 1000U);
}

class UsbHostTransport : public testing::TestWithParam<unsigned> {
  protected:
    nx_stm32_usb_host_t state{};
    nx_stm32_usb_host_channel_t channels[2]{};
    nx_stm32_usb_host_config_t config{};
    nx_dma_memory_region_t region = {0x20000000U, 4096U,
                                     NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    nx_usb_host_request_t request{};
    nx_usb_host_request_t peer{};
    nx_usb_host_port_t port = {&nx_stm32_usb_host_ops, &state};
    USB_OTG_GlobalTypeDef* core = nullptr;
    bool Dma() const {
        return GetParam() >= 2U;
    }
    bool In() const {
        return (GetParam() & 1U) == 0U;
    }
    uint8_t* Bytes() const {
        return reinterpret_cast<uint8_t*>(region.begin);
    }
    nx_time_us_t Deadline() const {
        return nx_time_now_us() + 500U;
    }
    void SetUp() override {
        for (const uintptr_t address :
             {0x40023000U, 0x40040000U, 0x50000000U, 0x20000000U}) {
            const size_t size = address == 0x40023000U || address == 0x20000000U
                                    ? 4096U
                                    : 0x20000U;
            ASSERT_NE(mmap(reinterpret_cast<void*>(address), size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                      MAP_FAILED);
        }
        hardware = {};
        irq_enabled = {};
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        nx_request_initialize(&request.base);
        nx_request_initialize(&peer.base);
        config.channels = channels;
        config.channel_count = 2U;
        config.regions = &region;
        config.region_count = 1U;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        config.rx_fifo_words = Dma() ? 256U : 128U;
        config.nonperiodic_fifo_words = Dma() ? 128U : 96U;
        config.periodic_fifo_words = Dma() ? 128U : 96U;
        config.drain_timeout_us = 20U;
        config.priority = 5U;
        config.dma = Dma();
        config.phy = Dma() ? NX_STM32_USB_HS_ULPI : NX_STM32_USB_FS_INTERNAL;
        config.ulpi_clock_hz = Dma() ? 60000000U : 0U;
        core = Dma() ? USB_OTG_HS : USB_OTG_FS;
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
    }
    void Start() {
        ASSERT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000000U),
                  NX_SUCCESS);
        core->HNPTXSTS = (4U << 16U) | config.nonperiodic_fifo_words;
        Host(core)->HPTXSTS = (4U << 16U) | config.periodic_fifo_words;
        auto* physical_port = reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_PORT_BASE);
        *physical_port =
            USB_OTG_HPRT_PCSTS | USB_OTG_HPRT_PENA | (Dma() ? 0U : 1U << 17U);
        const nx_usb_host_pipe_t pipe = {
            4U, 3U, static_cast<uint8_t>(In() ? 0x81U : 1U),
            Dma() ? NX_USB_HOST_HIGH_SPEED : NX_USB_HOST_FULL_SPEED,
            NX_USB_HOST_ISOCHRONOUS};
        ASSERT_EQ(nx_usb_host_open(&port, 0U, &pipe), NX_SUCCESS);
        const nx_usb_host_pipe_t bulk = {
            static_cast<uint16_t>(Dma() ? 512U : 8U), 3U, 0x82U,
            Dma() ? NX_USB_HOST_HIGH_SPEED : NX_USB_HOST_FULL_SPEED,
            NX_USB_HOST_BULK};
        ASSERT_EQ(nx_usb_host_open(&port, 1U, &bulk), NX_SUCCESS);
        ASSERT_EQ(nx_usb_host_prepare(&peer, Bytes() + 512U, 512U, 1U, false,
                                      false, Deadline()),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_host_submit(&port, 1U, &peer), NX_SUCCESS);
    }
    void Submit() {
        Bytes()[0] = In() ? 0xA5U : 1U;
        Bytes()[1] = In() ? 0xA5U : 2U;
        Bytes()[2] = In() ? 0xA5U : 3U;
        Bytes()[3] = 0xEEU;
        ASSERT_EQ(nx_usb_host_prepare(&request, Bytes(), 4U, 3U, false, false,
                                      Deadline()),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_SUCCESS);
    }
    void Signal(uint32_t flags) {
        Channel(core, 0U)->HCINT |= flags;
        Summaries(core);
        nx_stm32_usb_host_irq(&state);
    }
    void Complete(uint32_t count = 3U) {
        auto* channel = Channel(core, 0U);
        channel->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
        channel->HCTSIZ = In() ? 4U - count : 0U;
        if (In() && Dma()) {
            for (uint32_t i = 0U; i < count; ++i)
                Bytes()[i] = static_cast<uint8_t>(i + 1U);
        }
        if (In() && !Dma()) {
            Device(core).rx = {1U, 2U, 3U, 4U};
            Device(core).rx.resize(count);
            Device(core).rx_cursor = 0U;
            Device(core).rx_status = (count << 4U) | (GRXSTS_PKTSTS_IN << 17U);
            Summaries(core);
            nx_stm32_usb_host_irq(&state);
        }
        Signal(USB_OTG_HCINT_XFRC);
        if (!In())
            Signal(USB_OTG_HCINT_CHH);
    }
    void TearDown() override {
        Device(core).hold_halt = Device(core).hold_dma_zero = false;
        Device(core).hold_channel_ack = false;
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
        EXPECT_EQ(nx_usb_host_stop(&port), NX_SUCCESS);
        for (const uintptr_t address :
             {0x40023000U, 0x40040000U, 0x50000000U, 0x20000000U}) {
            const size_t size = address == 0x40023000U || address == 0x20000000U
                                    ? 4096U
                                    : 0x20000U;
            EXPECT_EQ(munmap(reinterpret_cast<void*>(address), size), 0);
        }
    }
};

TEST_P(UsbHostTransport, ActualIsoPacketJoinsBeforeCallerStorageIsReleased) {
    Start();
    Submit();
    EXPECT_EQ(state.hal.hc[0].ep_type, EP_TYPE_ISOC);
    EXPECT_EQ(state.hal.hc[0].data_pid, HC_PID_DATA0);
    EXPECT_EQ(Channel(core, 0U)->HCTSIZ & USB_OTG_HCTSIZ_PKTCNT, 1U << 19U);
    EXPECT_EQ(Channel(core, 0U)->HCCHAR & USB_OTG_HCCHAR_MC, 1U << 20U);
    if (!In() && !Dma()) {
        ASSERT_EQ(Device(core).tx[0].size(), 1U);
        EXPECT_EQ(Device(core).tx[0][0], 0xEE030201U);
    }
    Complete();
    core->GRSTCTL &= ~static_cast<uint32_t>(USB_OTG_GRSTCTL_AHBIDL);
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    core->GRSTCTL |= USB_OTG_GRSTCTL_AHBIDL;
    if (Dma()) {
        Device(core).hold_dma_zero = true;
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        Device(core).hold_dma_zero = false;
    }
    if (!In()) {
        Host(core)->HPTXSTS = config.periodic_fifo_words - 1U;
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        Host(core)->HPTXSTS = config.periodic_fifo_words;
    }
    ASSERT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(channels[0].request, nullptr);
    EXPECT_EQ(state.hal.hc[0].xfer_buff, nullptr);
    EXPECT_EQ(Channel(core, 0U)->HCDMA, 0U);
    EXPECT_EQ(Bytes()[0], 1U);
    EXPECT_EQ(Bytes()[1], 2U);
    EXPECT_EQ(Bytes()[2], 3U);
    EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(channels[1].request, &peer);
    EXPECT_EQ(state.hal.hc[1].xfer_buff, Bytes() + 512U);
    EXPECT_NE(state.config, nullptr);
    EXPECT_NE(state.clock_receipt, 0U);
    if (In()) {
        Submit();
        Complete(4U);
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(request.base.result, NX_ERROR_OVERFLOW);
        EXPECT_EQ(request.base.transferred, 3U);
        if (!Dma()) {
            EXPECT_EQ(Device(core).rx_cursor, 4U);
        }
    }
    if (GetParam() == 0U) {
        const unsigned before = Device(core).starts[0];
        ASSERT_EQ(nx_usb_host_prepare(&request, Bytes(), 4U, 3U, false, true,
                                      Deadline()),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_EQ(channels[0].request, nullptr);
        EXPECT_EQ(Device(core).starts[0], before);
    }
    if (GetParam() == 2U) {
        for (volatile uint32_t* trigger :
             {&Channel(core, 0U)->HCINTMSK, &Channel(core, 0U)->HCDMA}) {
            SCOPED_TRACE(trigger == &Channel(core, 0U)->HCINTMSK
                             ? "late error while masking completion"
                             : "late error while clearing DMA address");
            Submit();
            Complete(4U);
            Device(core).late_error_trigger = trigger;
            Device(core).hold_channel_ack = true;
            EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
            EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
            EXPECT_EQ(channels[0].request, &request);
            EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
            ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
            EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
            EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
            Device(core).hold_channel_ack = false;
            EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
            EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
            EXPECT_EQ(request.base.result, NX_ERROR_IO);
            EXPECT_EQ(request.base.transferred, 0U);
            EXPECT_EQ(Channel(core, 0U)->HCINT, 0U);
            EXPECT_EQ(channels[0].request, nullptr);
            EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
            EXPECT_NE(state.config, nullptr);
            EXPECT_NE(state.clock_receipt, 0U);
        }
    }
    if (GetParam() == 0U || GetParam() == 1U) {
        if (GetParam() == 0U) {
            const nx_usb_host_pipe_t output = {
                8U, 3U, 1U, NX_USB_HOST_FULL_SPEED, NX_USB_HOST_BULK};
            ASSERT_EQ(nx_usb_host_open(&port, 0U, &output), NX_SUCCESS);
            core->HNPTXSTS = config.nonperiodic_fifo_words;
        } else {
            Host(core)->HPTXSTS = 4U << 16U;
        }
        ASSERT_EQ(nx_usb_host_prepare(&request, Bytes(), 4U, 3U, false, false,
                                      Deadline()),
                  NX_SUCCESS);
        const unsigned before = Device(core).starts[0];
        ASSERT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_EQ(channels[0].request, nullptr);
        EXPECT_EQ(Device(core).starts[0], before);
        EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
    }
}
TEST_P(UsbHostTransport, MissedFrameOrDmaErrorEndsThisIsoOpportunity) {
    Start();
    Submit();
    const unsigned starts = Device(core).starts[0];
    Channel(core, 0U)->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
    Signal(Dma() ? USB_OTG_HCINT_AHBERR : USB_OTG_HCINT_FRMOR);
    Signal(USB_OTG_HCINT_CHH);
    if (GetParam() == 1U) {
        /* A missed PIO OUT opportunity retains its copied periodic FIFO.
         * Halt is not evidence that those bytes vanished or peers stopped. */
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
        EXPECT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
        EXPECT_EQ(nx_usb_host_stop(&port), NX_SUCCESS);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(request.base.result, NX_ERROR_IO);
        EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(state.config, nullptr);
        EXPECT_EQ(state.clock_receipt, 0U);
        return;
    }
    if (GetParam() == 3U) {
        Channel(core, 0U)->HCCHAR &=
            ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
        Channel(core, 0U)->HCINT |= USB_OTG_HCINT_CHH;
        Device(core).hold_channel_ack = true;
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
        EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        Device(core).hold_channel_ack = false;
    }
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(Device(core).starts[0], starts);
    EXPECT_EQ(nx_request_state(&peer.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(channels[1].request, &peer);
    EXPECT_FALSE(state.snapshot.closing);
    EXPECT_EQ(Channel(core, 0U)->HCINT & 0x7FFU, 0U);
}

TEST_P(UsbHostTransport, CancellationWaitsForActualStopHardwareAcknowledgment) {
    Start();
    Submit();
    Device(core).hold_halt = true;
    EXPECT_EQ(nx_usb_host_cancel(&port, 0U, &request), NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(channels[0].request, &request);
    EXPECT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(state.config, nullptr);
    EXPECT_NE(state.clock_receipt, 0U);
    Device(core).hold_halt = false;
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(state.clock_receipt, 0U);
    EXPECT_EQ(channels[0].request, nullptr);
}
INSTANTIATE_TEST_SUITE_P(FsInFsOutHsDmaInHsDmaOut, UsbHostTransport,
                         testing::Values(0U, 1U, 2U, 3U));
