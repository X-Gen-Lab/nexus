/**
 * \file            usb_host_hal_test.cpp
 * \brief           Real HCD bridge admission, completion and full withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/fault.h"
#include "nexus/io/native/model.h"
#include "stm32f407_usb_host.h"
#include "stm32f407_usb_host_cold.h"
}
#undef nx_time_now_us
extern "C" nx_time_us_t nx_time_now_us(void);
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class HostHal {
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
    MOCK_METHOD(HAL_StatusTypeDef, Halt, (HCD_HandleTypeDef*, uint8_t));
    MOCK_METHOD(void, Irq, (HCD_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, FlushRx, (USB_OTG_GlobalTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, FlushTx, (USB_OTG_GlobalTypeDef*, uint32_t));
    MOCK_METHOD(HAL_StatusTypeDef, StartPacket,
                (USB_OTG_GlobalTypeDef*, USB_OTG_HCTypeDef*, uint8_t));
    MOCK_METHOD(nx_result_t, PinClock, (unsigned));
    MOCK_METHOD(nx_result_t, PinConfigure,
                (GPIO_TypeDef*, uint8_t, uint8_t, uint8_t, uint8_t, bool));
    MOCK_METHOD(uint16_t, PinCapture, (const GPIO_TypeDef*, uint8_t));
    MOCK_METHOD(void, PinRestore, (GPIO_TypeDef*, uint8_t, uint16_t));
    static HostHal* active;
};
HostHal* HostHal::active = nullptr;
uint32_t g_nx_usb_host_hclk_hz = 168000000U;
static bool irq_enabled[82];
static bool irq_active[82];
static bool hold_gate;
static bool hold_dma_address;
struct PortWrite {
    uint32_t value;
    nx_time_us_t time;
    uint64_t fine_time;
};
static std::vector<PortWrite> port_writes;
static bool signal_clock;
static bool backward_signal_clock;
static bool fine_signal_clock;
static uint64_t fine_now;
static nx_time_us_t signal_now;
static uint32_t signal_step;
static uint32_t signal_jump;
static unsigned signal_jump_read;
static unsigned signal_reads;
static uint32_t active_signal;
static uint32_t held_port_clear;
static bool terminal_fault;
static nx_stm32_usb_host_t* signal_owner;

extern "C" nx_time_us_t nx_usb_host_model_now(void) {
    if (!signal_clock)
        return nx_time_now_us();
    if (fine_signal_clock) {
        signal_now = fine_now / 10U;
        if (active_signal != 0U)
            ++fine_now;
        return signal_now;
    }
    if (active_signal != 0U) {
        ++signal_reads;
        if (signal_reads == signal_jump_read)
            signal_now += signal_jump;
        nx_time_us_t value = signal_now;
        signal_now =
            backward_signal_clock ? signal_now - 1U : signal_now + signal_step;
        return value;
    }
    return signal_now;
}

extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    terminal_fault = domain == NX_FAULT_PLATFORM && reason != NX_SUCCESS &&
                     file != nullptr && line != 0U;
}

extern "C" void nx_usb_host_model_park(void) {
    const bool retained =
        terminal_fault && active_signal != 0U && signal_owner != nullptr &&
        signal_owner->config != nullptr && signal_owner->snapshot.closing &&
        signal_owner->clock_receipt != 0U;
    std::_Exit(retained ? 70 : 72);
}

extern "C" void nx_usb_host_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    const auto address = reinterpret_cast<uintptr_t>(reg);
    if (hold_gate && (reg == &RCC->AHB1ENR || reg == &RCC->AHB2ENR)) {
        return;
    }
    if (hold_dma_address &&
        ((address >= 0x50000500U && address < 0x50000700U) ||
         (address >= 0x40040500U && address < 0x40040700U)) &&
        (address & 31U) == 20U) {
        return;
    }
    if (address == 0x50000440U || address == 0x40040440U) {
        *reg = value | (*reg & held_port_clear);
        const nx_time_us_t now = signal_clock ? signal_now : nx_time_now_us();
        port_writes.push_back(
            {*reg, now, fine_signal_clock ? fine_now : now * 10U});
        active_signal = *reg & (USB_OTG_HPRT_PRST | USB_OTG_HPRT_PRES);
        return;
    }
    if ((address >= 0x50000500U && address < 0x50000700U &&
         (address & 31U) == 8U) ||
        (address >= 0x40040500U && address < 0x40040700U &&
         (address & 31U) == 8U)) {
        *reg &= ~value;
    } else {
        *reg = value;
    }
}
extern "C" void nx_usb_host_model_irq_enable(int irq, uint8_t priority) {
    (void)priority;
    irq_enabled[irq] = true;
}
extern "C" void nx_usb_host_model_irq_disable(int irq) {
    irq_enabled[irq] = false;
}
extern "C" bool nx_usb_host_model_irq_enabled(int irq) {
    return irq_enabled[irq];
}
extern "C" bool nx_usb_host_model_irq_active(int irq) {
    return irq_active[irq];
}
extern "C" HAL_StatusTypeDef HAL_HCD_Init(HCD_HandleTypeDef* h) {
    return HostHal::active->Init(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_Start(HCD_HandleTypeDef* h) {
    return HostHal::active->Start(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_Stop(HCD_HandleTypeDef* h) {
    return HostHal::active->Stop(h);
}
extern "C" HAL_StatusTypeDef HAL_HCD_HC_Init(HCD_HandleTypeDef* h, uint8_t ch,
                                             uint8_t ep, uint8_t address,
                                             uint8_t speed, uint8_t type,
                                             uint16_t mps) {
    return HostHal::active->Open(h, ch, ep, address, speed, type, mps);
}
extern "C" HAL_StatusTypeDef
HAL_HCD_HC_SubmitRequest(HCD_HandleTypeDef* h, uint8_t ch, uint8_t direction,
                         uint8_t type, uint8_t token, uint8_t* bytes,
                         uint16_t length, uint8_t ping) {
    return HostHal::active->Submit(h, ch, direction, type, token, bytes, length,
                                   ping);
}
extern "C" HAL_StatusTypeDef HAL_HCD_HC_Halt(HCD_HandleTypeDef* h, uint8_t ch) {
    return HostHal::active->Halt(h, ch);
}
extern "C" void HAL_HCD_IRQHandler(HCD_HandleTypeDef* h) {
    HostHal::active->Irq(h);
}
extern "C" HAL_StatusTypeDef USB_FlushRxFifo(USB_OTG_GlobalTypeDef* core) {
    return HostHal::active->FlushRx(core);
}
extern "C" HAL_StatusTypeDef USB_FlushTxFifo(USB_OTG_GlobalTypeDef* core,
                                             uint32_t number) {
    return HostHal::active->FlushTx(core, number);
}
extern "C" HAL_StatusTypeDef USB_HC_StartXfer(USB_OTG_GlobalTypeDef* core,
                                              USB_OTG_HCTypeDef* channel,
                                              uint8_t dma) {
    return HostHal::active->StartPacket(core, channel, dma);
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned bank) {
    return HostHal::active->PinClock(bank);
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                              uint8_t mode, uint8_t af,
                                              uint8_t pull, bool high) {
    return HostHal::active->PinConfigure(gpio, pin, mode, af, pull, high);
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio,
                                         uint8_t pin) {
    return HostHal::active->PinCapture(gpio, pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin,
                                     uint16_t saved) {
    HostHal::active->PinRestore(gpio, pin, saved);
}

class UsbHostHal : public testing::Test {
  protected:
    StrictMock<HostHal> hal;
    nx_stm32_usb_host_t state = {};
    nx_stm32_usb_host_channel_t channels[2] = {};
    nx_stm32_usb_host_config_t config = {};
    nx_usb_host_port_t port = {&nx_stm32_usb_host_ops, &state};
    nx_usb_host_request_t request = {};
    nx_usb_host_request_t second_request = {};
    alignas(4) uint8_t bytes[1024] = {};
    nx_dma_memory_region_t region = {reinterpret_cast<uintptr_t>(bytes),
                                     sizeof(bytes),
                                     NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    USB_OTG_GlobalTypeDef* core = USB_OTG_FS;
    USB_OTG_HostChannelTypeDef* Channel(unsigned channel) {
        return reinterpret_cast<USB_OTG_HostChannelTypeDef*>(
            reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_CHANNEL_BASE +
            channel * USB_OTG_HOST_CHANNEL_SIZE);
    }
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40023000U), 0x1000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x50000000U), 0x20000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40040000U), 0x20000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        HostHal::active = &hal;
        g_nx_usb_host_hclk_hz = 168000000U;
        std::memset(irq_enabled, 0, sizeof(irq_enabled));
        std::memset(irq_active, 0, sizeof(irq_active));
        hold_gate = false;
        hold_dma_address = false;
        port_writes.clear();
        signal_clock = backward_signal_clock = false;
        fine_signal_clock = false;
        fine_now = 1000U;
        signal_now = 100U;
        signal_step = 1000U;
        signal_jump = signal_jump_read = signal_reads = 0U;
        active_signal = held_port_clear = 0U;
        terminal_fault = false;
        signal_owner = &state;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        config.channels = channels;
        config.regions = &region;
        config.region_count = 1U;
        config.channel_count = 2U;
        config.rx_fifo_words = 128U;
        config.nonperiodic_fifo_words = 96U;
        config.periodic_fifo_words = 96U;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 82U, 4U, 0U};
        config.priority = 5U;
        config.drain_timeout_us = 20U;
        nx_request_initialize(&request.base);
    }
    void Start() {
        core = config.phy == NX_STM32_USB_FS_INTERNAL ? USB_OTG_FS : USB_OTG_HS;
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
        core->HNPTXSTS = config.nonperiodic_fifo_words;
        *reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<uintptr_t>(core) + 0x410U) =
            config.periodic_fifo_words;
        EXPECT_CALL(hal, Init(&state.hal)).WillOnce(Return(HAL_OK));
        EXPECT_CALL(hal, Start(&state.hal)).WillOnce(Return(HAL_OK));
        ASSERT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
                  NX_SUCCESS);
    }
    volatile uint32_t* Port() {
        return reinterpret_cast<volatile uint32_t*>(
            reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_PORT_BASE);
    }
    void SignalClock() {
        signal_clock = true;
        signal_now = 100U;
        signal_reads = 0U;
        port_writes.clear();
    }
    void Suspended() {
        *Port() |= USB_OTG_HPRT_PSUSP;
        state.snapshot.suspended = true;
    }
    void CheckSignal(uint32_t bit, nx_time_us_t minimum) {
        ASSERT_EQ(port_writes.size(), 2U);
        EXPECT_NE(port_writes[0].value & bit, 0U);
        EXPECT_EQ(port_writes[1].value & bit, 0U);
        EXPECT_GE(port_writes[1].time - port_writes[0].time, minimum);
    }
    void Open(bool in = true, uint16_t mps = 64U) {
        nx_usb_host_pipe_t pipe = {mps, 3U,
                                   static_cast<uint8_t>(in ? 0x81U : 1U),
                                   NX_USB_HOST_FULL_SPEED, NX_USB_HOST_BULK};
        EXPECT_CALL(hal, Open(&state.hal, 0U, pipe.endpoint_address, 3U, 1U,
                              EP_TYPE_BULK, mps))
            .WillOnce(Invoke([this](HCD_HandleTypeDef*, uint8_t, uint8_t,
                                    uint8_t, uint8_t, uint8_t, uint16_t) {
                Channel(0)->HCINTMSK = 0x55U;
                auto* host = reinterpret_cast<USB_OTG_HostTypeDef*>(
                    reinterpret_cast<uintptr_t>(core) + USB_OTG_HOST_BASE);
                host->HAINTMSK |= 1U;
                return HAL_OK;
            }));
        ASSERT_EQ(nx_usb_host_open(&port, 0U, &pipe), NX_SUCCESS);
    }
    void Submit(bool in = true, size_t length = 3U) {
        ASSERT_EQ(nx_usb_host_prepare(&request, bytes, sizeof(bytes), length,
                                      false, false, 500U),
                  NX_SUCCESS);
        EXPECT_CALL(hal, Submit(&state.hal, 0U, in ? 1U : 0U, EP_TYPE_BULK, 1U,
                                bytes, static_cast<uint16_t>(length), 0U))
            .WillOnce(
                Invoke([this](HCD_HandleTypeDef* h, uint8_t, uint8_t, uint8_t,
                              uint8_t, uint8_t* data, uint16_t, uint8_t) {
                    h->hc[0].xfer_buff = data;
                    Channel(0)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
                    return HAL_OK;
                }));
        ASSERT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_SUCCESS);
    }
    void Complete(uint32_t count = 3U, HCD_URBStateTypeDef result = URB_DONE) {
        ASSERT_NE(state.hal.HC_NotifyURBChangeCallback, nullptr);
        Channel(0)->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
        state.hal.hc[0].xfer_count = count;
        state.hal.HC_NotifyURBChangeCallback(&state.hal, 0U, result);
    }
    void Stop() {
        if (state.config == nullptr) {
            return;
        }
        EXPECT_CALL(hal, Halt(&state.hal, _))
            .WillRepeatedly(Invoke([this](HCD_HandleTypeDef*, uint8_t ch) {
                Channel(ch)->HCCHAR &=
                    ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
                return HAL_OK;
            }));
        EXPECT_CALL(hal, Stop(&state.hal)).WillRepeatedly(Return(HAL_OK));
        EXPECT_CALL(hal, FlushRx(core)).WillRepeatedly(Return(HAL_OK));
        EXPECT_CALL(hal, FlushTx(core, 0x10U)).WillRepeatedly(Return(HAL_OK));
        EXPECT_EQ(nx_usb_host_stop(&port), NX_SUCCESS);
    }
    void TearDown() override {
        hold_gate = false;
        hold_dma_address = false;
        held_port_clear = 0U;
        signal_clock = false;
        irq_active[67] = irq_active[77] = false;
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
        core->GINTSTS = 0U;
        Stop();
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40023000U), 0x1000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x50000000U), 0x20000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40040000U), 0x20000U), 0);
        HostHal::active = nullptr;
    }
};

TEST_F(UsbHostHal, ResetRejectsShortAndExactBudgetBeforePortEffects) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    for (nx_time_us_t budget : {1U, 1000U, 10000U, 10001U}) {
        EXPECT_EQ(nx_usb_host_reset(&port, signal_now + budget),
                  NX_ERROR_TIMEOUT);
        EXPECT_TRUE(port_writes.empty());
        EXPECT_EQ(*Port() & USB_OTG_HPRT_PRST, 0U);
    }
}

TEST_F(UsbHostHal, ResumeRejectsShortAndExactBudgetWithoutChangingSuspend) {
    Start();
    Suspended();
    SignalClock();
    for (nx_time_us_t budget : {1U, 1000U, 20000U, 20001U}) {
        EXPECT_EQ(nx_usb_host_suspend(&port, false, signal_now + budget),
                  NX_ERROR_TIMEOUT);
        EXPECT_TRUE(port_writes.empty());
        EXPECT_NE(*Port() & USB_OTG_HPRT_PSUSP, 0U);
        EXPECT_EQ(*Port() & USB_OTG_HPRT_PRES, 0U);
        EXPECT_TRUE(state.snapshot.suspended);
    }
}

TEST_F(UsbHostHal, ExpiredPortSignalsReturnTimeoutWithoutHardwareEffects) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    for (nx_time_us_t deadline : {99U, 100U}) {
        EXPECT_EQ(nx_usb_host_reset(&port, deadline), NX_ERROR_TIMEOUT);
        EXPECT_EQ(nx_usb_host_suspend(&port, true, deadline), NX_ERROR_TIMEOUT);
        EXPECT_TRUE(port_writes.empty());
    }
    Suspended();
    EXPECT_EQ(nx_usb_host_suspend(&port, false, 100U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(port_writes.empty());
    EXPECT_TRUE(state.snapshot.suspended);
}

TEST_F(UsbHostHal, ResetKeepsItsMinimumSignalAndPreservesPortChangeFacts) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    *Port() = USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG |
              USB_OTG_HPRT_POCCHNG | USB_OTG_HPRT_PPWR;
    SignalClock();
    EXPECT_EQ(nx_usb_host_reset(&port, 50000U), NX_SUCCESS);
    CheckSignal(USB_OTG_HPRT_PRST, 10000U);
    for (const auto& write : port_writes)
        EXPECT_EQ(write.value & (USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
                                 USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG),
                  0U);
    EXPECT_NE(*Port() & USB_OTG_HPRT_PPWR, 0U);
}

TEST_F(UsbHostHal, SuspendAndResumeKeepTheFullResumeSignal) {
    Start();
    SignalClock();
    EXPECT_EQ(nx_usb_host_suspend(&port, true, 50000U), NX_SUCCESS);
    EXPECT_TRUE(state.snapshot.suspended);
    EXPECT_NE(*Port() & USB_OTG_HPRT_PSUSP, 0U);
    port_writes.clear();
    EXPECT_EQ(nx_usb_host_suspend(&port, false, 50000U), NX_SUCCESS);
    CheckSignal(USB_OTG_HPRT_PRES, 20000U);
    EXPECT_FALSE(state.snapshot.suspended);
    EXPECT_EQ(*Port() & USB_OTG_HPRT_PSUSP, 0U);
}

TEST_F(UsbHostHal, ResetMinimumHoldCoversTheUnknownIntegerTickPhase) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    fine_signal_clock = true;
    fine_now = 1009U;
    EXPECT_EQ(nx_usb_host_reset(&port, 50000U), NX_SUCCESS);
    ASSERT_EQ(port_writes.size(), 2U);
    EXPECT_GE(port_writes[1].fine_time - port_writes[0].fine_time, 100000U);
}

TEST_F(UsbHostHal, ResumeMinimumHoldCoversTheUnknownIntegerTickPhase) {
    Start();
    Suspended();
    SignalClock();
    fine_signal_clock = true;
    fine_now = 1009U;
    EXPECT_EQ(nx_usb_host_suspend(&port, false, 50000U), NX_SUCCESS);
    ASSERT_EQ(port_writes.size(), 2U);
    EXPECT_GE(port_writes[1].fine_time - port_writes[0].fine_time, 200000U);
}

TEST_F(UsbHostHal, LateResetReportsTimeoutAfterTheRealSignalWithdrawal) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    signal_jump_read = 4U;
    signal_jump = 30000U;
    EXPECT_EQ(nx_usb_host_reset(&port, 20100U), NX_ERROR_TIMEOUT);
    CheckSignal(USB_OTG_HPRT_PRST, 10000U);
    EXPECT_FALSE(state.snapshot.closing);
}

TEST_F(UsbHostHal, LateResumeReportsTimeoutAndPublishesTheActualResumedState) {
    Start();
    Suspended();
    SignalClock();
    signal_jump_read = 4U;
    signal_jump = 40000U;
    EXPECT_EQ(nx_usb_host_suspend(&port, false, 30100U), NX_ERROR_TIMEOUT);
    CheckSignal(USB_OTG_HPRT_PRES, 20000U);
    EXPECT_FALSE(state.snapshot.suspended);
    EXPECT_FALSE(state.snapshot.closing);
}

TEST_F(UsbHostHal, FailedResetWithdrawalRetainsTheSignalUntilStopCanJoin) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    held_port_clear = USB_OTG_HPRT_PRST;
    EXPECT_EQ(nx_usb_host_reset(&port, 50000U), NX_ERROR_IO);
    EXPECT_TRUE(state.snapshot.closing);
    EXPECT_NE(*Port() & USB_OTG_HPRT_PRST, 0U);
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.config, &config);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN, 0U);
    held_port_clear = 0U;
    Stop();
    EXPECT_EQ(*Port() & USB_OTG_HPRT_PRST, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(UsbHostHal, FailedResumeWithdrawalRetainsItsOwnerAndBlocksNewWork) {
    Start();
    Suspended();
    SignalClock();
    held_port_clear = USB_OTG_HPRT_PRES;
    EXPECT_EQ(nx_usb_host_suspend(&port, false, 50000U), NX_ERROR_IO);
    EXPECT_TRUE(state.snapshot.closing);
    EXPECT_NE(*Port() & USB_OTG_HPRT_PRES, 0U);
    EXPECT_EQ(nx_usb_host_reset(&port, 100000U), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.config, &config);
    held_port_clear = 0U;
    Stop();
    EXPECT_EQ(*Port() & USB_OTG_HPRT_PRES, 0U);
}

TEST_F(UsbHostHal, FrozenSignalTimeParksWithTheMinimumSignalStillRetained) {
    Start();
    state.hal.ConnectCallback(&state.hal);
    SignalClock();
    ASSERT_EXIT(
        {
            signal_step = 0U;
            alarm(2U);
            (void)nx_usb_host_reset(&port, 50000U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(UsbHostHal, BackwardSignalTimeCannotInventASuccessfulResume) {
    Start();
    Suspended();
    SignalClock();
    ASSERT_EXIT(
        {
            backward_signal_clock = true;
            alarm(2U);
            (void)nx_usb_host_suspend(&port, false, 50000U);
            std::_Exit(71);
        },
        testing::ExitedWithCode(70), "");
}

TEST_F(UsbHostHal, ControlPacketSizeMatchesEachActualSpeedBeforeHalEffects) {
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 60000000U;
    Start();
    const uint16_t invalid_sizes[] = {8U, 16U, 32U};
    for (uint16_t mps : invalid_sizes) {
        const nx_usb_host_pipe_t pipe = {mps, 3U, 0x80U, NX_USB_HOST_HIGH_SPEED,
                                         NX_USB_HOST_CONTROL};
        EXPECT_EQ(nx_usb_host_open(&port, 0U, &pipe), NX_ERROR_INVALID);
        EXPECT_FALSE(channels[0].configured);
    }
    for (auto speed : {NX_USB_HOST_LOW_SPEED, NX_USB_HOST_FULL_SPEED,
                       NX_USB_HOST_HIGH_SPEED}) {
        const uint16_t packet_sizes[] = {8U, 16U, 32U, 64U};
        for (uint16_t mps : packet_sizes) {
            const nx_usb_host_pipe_t pipe = {mps, 3U, 0x80U, speed,
                                             NX_USB_HOST_CONTROL};
            if ((speed == NX_USB_HOST_LOW_SPEED && mps != 8U) ||
                (speed == NX_USB_HOST_HIGH_SPEED && mps != 64U))
                continue;
            const uint8_t hal_speed = speed == NX_USB_HOST_LOW_SPEED    ? 2U
                                      : speed == NX_USB_HOST_FULL_SPEED ? 1U
                                                                        : 0U;
            EXPECT_CALL(hal, Open(&state.hal, 0U, 0x80U, 3U, hal_speed,
                                  EP_TYPE_CTRL, mps))
                .WillOnce(Return(HAL_OK));
            EXPECT_EQ(nx_usb_host_open(&port, 0U, &pipe), NX_SUCCESS);
        }
    }
}

TEST_F(UsbHostHal, FsHcdUsesSelectedFifosAndDoesNotLeakVendorState) {
    Start();
    EXPECT_EQ(state.hal.Instance, USB_OTG_FS);
    EXPECT_EQ(state.hal.Init.phy_itface, HCD_PHY_EMBEDDED);
    EXPECT_EQ(state.hal.Init.speed, HCD_SPEED_FULL);
    EXPECT_EQ(state.hal.Init.dma_enable, 0U);
    EXPECT_EQ(core->GRXFSIZ, 128U);
    EXPECT_EQ(core->DIEPTXF0_HNPTXFSIZ, (96U << 16U) | 128U);
    EXPECT_EQ(core->HPTXFSIZ, (96U << 16U) | 224U);
    EXPECT_TRUE(irq_enabled[67]);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN, 0U);
}

TEST_F(UsbHostHal, HsInternalPhyStaysFullSpeedAndUlpiSelectsHighSpeedDma) {
    config.phy = NX_STM32_USB_HS_INTERNAL_FS;
    Start();
    EXPECT_EQ(state.hal.Init.phy_itface, HCD_PHY_EMBEDDED);
    EXPECT_EQ(state.hal.Init.speed, HCD_SPEED_FULL);
    Stop();
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 60000000U;
    config.dma = true;
    Start();
    EXPECT_EQ(state.hal.Init.phy_itface, HCD_PHY_ULPI);
    EXPECT_EQ(state.hal.Init.speed, HCD_SPEED_HIGH);
    EXPECT_EQ(state.hal.Init.dma_enable, 1U);
    EXPECT_EQ(RCC->AHB1ENR & (RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN),
              RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN);
}

TEST_F(UsbHostHal, ChannelsRejectAliasesToEveryLiveRequestAndPacket) {
    Start();
    Open();
    nx_usb_host_pipe_t pipe = {64U, 3U, 0x82U, NX_USB_HOST_FULL_SPEED,
                               NX_USB_HOST_BULK};
    EXPECT_CALL(hal, Open(&state.hal, 1U, 0x82U, 3U, 1U, EP_TYPE_BULK, 64U))
        .WillOnce(Return(HAL_OK));
    ASSERT_EQ(nx_usb_host_open(&port, 1U, &pipe), NX_SUCCESS);
    Submit();
    auto& second = second_request;
    nx_request_initialize(&second.base);
    ASSERT_EQ(nx_usb_host_prepare(&second, bytes + 4U, sizeof(bytes) - 4U, 1U,
                                  false, false, 500U),
              NX_SUCCESS);
    EXPECT_CALL(hal, Submit(&state.hal, 1U, _, _, _, _, _, _)).Times(0);
    EXPECT_EQ(nx_usb_host_submit(&port, 1U, &second), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_READY);
    EXPECT_EQ(channels[0].request, &request);
    ASSERT_EQ(nx_usb_host_prepare(&second, reinterpret_cast<uint8_t*>(&request),
                                  sizeof(request), 1U, false, false, 500U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_submit(&port, 1U, &second), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_READY);
}

TEST_F(UsbHostHal, ControlDataUsesOfficialLlToPreserveExplicitDataZeroPid) {
    Start();
    nx_usb_host_pipe_t pipe = {64U, 3U, 0x80U, NX_USB_HOST_FULL_SPEED,
                               NX_USB_HOST_CONTROL};
    EXPECT_CALL(hal, Open(&state.hal, 0U, 0x80U, 3U, 1U, EP_TYPE_CTRL, 64U))
        .WillOnce(Return(HAL_OK));
    ASSERT_EQ(nx_usb_host_open(&port, 0U, &pipe), NX_SUCCESS);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes, sizeof(bytes), 3U, false,
                                  false, 500U),
              NX_SUCCESS);
    EXPECT_CALL(hal, Submit(_, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(hal, StartPacket(core, &state.hal.hc[0], 0U))
        .WillOnce(
            Invoke([this](USB_OTG_GlobalTypeDef*, auto* channel, uint8_t) {
                EXPECT_EQ(channel->data_pid, HC_PID_DATA0);
                EXPECT_EQ(channel->xfer_buff, bytes);
                EXPECT_EQ(channel->xfer_len, 3U);
                Channel(0U)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
                return HAL_OK;
            }));
    EXPECT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_SUCCESS);
}

TEST_F(UsbHostHal, DmaAddressRegistersNeedReservedPhysicalFifoRam) {
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 60000000U;
    config.dma = true;
    config.rx_fifo_words = 512U;
    config.nonperiodic_fifo_words = 256U;
    config.periodic_fifo_words = 256U;
    core = USB_OTG_HS;
    core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
    core->HNPTXSTS = 256U;
    *reinterpret_cast<volatile uint32_t*>(reinterpret_cast<uintptr_t>(core) +
                                          0x410U) = 256U;
    EXPECT_CALL(hal, Init(_)).Times(0);
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
}

TEST_F(UsbHostHal, CallbackNeedsHardwareDmaAndIrqDetachBeforeSettlement) {
    Start();
    Open();
    Submit();
    Complete();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    Channel(0)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
    EXPECT_CALL(hal, Halt(&state.hal, 0U)).WillOnce(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(state.hal.hc[0].xfer_buff, nullptr);
    Channel(0)->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
    Channel(0)->HCDMA = 0x20000000U;
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(state.hal.hc[0].xfer_buff, nullptr);
    EXPECT_EQ(Channel(0)->HCDMA, 0U);
    EXPECT_EQ(channels[0].request, nullptr);
}

TEST_F(UsbHostHal, FullMpsCapacityPreventsShortInOverflowBeforeAdmission) {
    Start();
    Open();
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes, 3U, 3U, false, false, 500U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(channels[0].request, nullptr);
}

TEST_F(UsbHostHal, OverlengthPacketDrainsAndReportsOverflowWithoutEarlyReuse) {
    Start();
    Open();
    Submit();
    Complete(64U);
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_OVERFLOW);
    EXPECT_EQ(request.base.transferred, 3U);
}

TEST_F(UsbHostHal, NakIsExplicitCallerRetryAndDoesNotResubmit) {
    Start();
    Open();
    Submit();
    Complete(0U, URB_NOTREADY);
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_NACK);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(UsbHostHal, CancellationAndFailedFlushRetainPayloadUntilLateDrain) {
    Start();
    Open(false);
    Submit(false);
    EXPECT_CALL(hal, Halt(&state.hal, 0U)).WillRepeatedly(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_cancel(&port, 0U, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_CALL(hal, Stop(&state.hal)).WillRepeatedly(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_NE(channels[0].request, nullptr);
    ASSERT_EQ(nx_native_clock_configure(true, 150U), NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    Channel(0)->HCCHAR &= ~static_cast<uint32_t>(USB_OTG_HCCHAR_CHENA);
    EXPECT_CALL(hal, FlushRx(core)).WillOnce(Return(HAL_TIMEOUT));
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.hal.hc[0].xfer_buff, bytes);
    EXPECT_CALL(hal, FlushRx(core)).WillOnce(Return(HAL_OK));
    EXPECT_CALL(hal, FlushTx(core, 0x10U)).WillOnce(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(state.hal.hc[0].xfer_buff, nullptr);
    EXPECT_FALSE(irq_enabled[67]);
}

TEST_F(UsbHostHal, ActivePublisherAndForeignGateDoNotLoseOwnership) {
    RCC->AHB2ENR = RCC_AHB2ENR_OTGFSEN;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_BUSY);
    EXPECT_EQ(state.config, nullptr);
    RCC->AHB2ENR = 0U;
    Start();
    irq_active[67] = true;
    EXPECT_CALL(hal, Stop(&state.hal)).WillOnce(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_stop(&port), NX_ERROR_BUSY);
    EXPECT_NE(state.config, nullptr);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN, 0U);
}

TEST_F(UsbHostHal, InvalidFsDmaAndUlpiClockRejectBeforeClockOrHalEffects) {
    config.dma = true;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_INVALID);
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 48000000U;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
    EXPECT_EQ(RCC->AHB2ENR, 0U);
}

TEST_F(UsbHostHal, CompletedChannelRestoresItsInterruptMaskOnTheNextPacket) {
    Start();
    Open();
    Submit();
    Complete();
    ASSERT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(Channel(0)->HCINTMSK, 0U);
    Submit();
    EXPECT_EQ(Channel(0)->HCINTMSK, 0x55U);
    Complete();
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
}

TEST_F(UsbHostHal, FailedDmaAddressWithdrawalRetainsTheAcceptedLoan) {
    Start();
    Open();
    Submit();
    Complete();
    Channel(0)->HCDMA = 0x20000000U;
    hold_dma_address = true;
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.hal.hc[0].xfer_buff, bytes);
    hold_dma_address = false;
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(UsbHostHal, OutCompletionAlsoRequiresTheSelectedSharedFifoToEmpty) {
    Start();
    Open(false);
    Submit(false);
    Complete();
    core->HNPTXSTS = 95U;
    EXPECT_CALL(hal, Halt(&state.hal, 0U)).WillOnce(Return(HAL_OK));
    EXPECT_EQ(nx_usb_host_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    core->HNPTXSTS = 96U;
    EXPECT_EQ(nx_usb_host_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(UsbHostHal, PartialColdPinFailureKeepsExactReceiptAndRestoresInReverse) {
    ASSERT_NE(mmap(reinterpret_cast<void*>(0x40020000U), 0x1000U,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
              MAP_FAILED);
    uint16_t saved[2] = {};
    nx_stm32_platform_pin_config_t pins[] = {{GPIOA, &saved[0], 0U, 11U},
                                             {GPIOA, &saved[1], 0U, 12U}};
    const nx_stm32_usb_host_cold_config_t cold = {config, pins, 1000U, 2U};
    nx_stm32_usb_host_cold_receipt_t receipt = {};
    EXPECT_CALL(hal, PinClock(0U)).WillRepeatedly(Return(NX_SUCCESS));
    EXPECT_CALL(hal, PinCapture(GPIOA, 11U)).WillRepeatedly(Return(0x123U));
    EXPECT_CALL(hal, PinCapture(GPIOA, 12U)).WillRepeatedly(Return(0x456U));
    EXPECT_CALL(hal, PinConfigure(GPIOA, 11U, 2U, 10U, 0U, false))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(hal, PinConfigure(GPIOA, 12U, 2U, 10U, 0U, false))
        .WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_stm32_usb_host_cold_prepare(&state, &cold, &receipt),
              NX_ERROR_IO);
    EXPECT_EQ(receipt.leases, 3U);
    EXPECT_EQ(receipt.config, &cold);
    {
        testing::InSequence order;
        EXPECT_CALL(hal, PinRestore(GPIOA, 12U, 0x456U));
        EXPECT_CALL(hal, PinRestore(GPIOA, 11U, 0x123U));
    }
    EXPECT_EQ(nx_stm32_usb_host_cold_close(&state, &cold, &receipt),
              NX_SUCCESS);
    EXPECT_EQ(receipt.leases, 0U);
    EXPECT_EQ(receipt.config, nullptr);
    EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40020000U), 0x1000U), 0);
}

TEST_F(UsbHostHal, PioOutRejectsAnUnownedHalWordReadBeyondPacketExtent) {
    Start();
    Open(false);
    ASSERT_EQ(nx_usb_host_prepare(&request, bytes, 3U, 3U, false, false, 500U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_host_submit(&port, 0U, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(channels[0].request, nullptr);
}

TEST_F(UsbHostHal, FailedGateAcquisitionClosesWithoutTouchingAnUnclockedCore) {
    hold_gate = true;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_IO);
    EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN, 0U);
    hold_gate = false;
    EXPECT_EXIT(
        {
            auto result = nx_usb_host_stop(&port);
            _exit(result == NX_SUCCESS && state.config == nullptr ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
    /* The child owns no accepted packet; preserve the original RED process. */
    state.config = nullptr;
    state.clock_receipt = 0U;
}

TEST_F(UsbHostHal, FsPipeMpsMustFitBothErrataRxReserveAndTransmitFifo) {
    Start();
    const nx_usb_host_pipe_t large = {512U, 3U, 0x81U, NX_USB_HOST_FULL_SPEED,
                                      NX_USB_HOST_ISOCHRONOUS};
    EXPECT_EQ(nx_usb_host_open(&port, 0U, &large), NX_ERROR_INVALID);
    EXPECT_FALSE(channels[0].configured);
}

TEST_F(UsbHostHal, HighSpeedOutPacketCannotExceedItsSelectedTransmitFifo) {
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 60000000U;
    config.dma = true;
    Start();
    const nx_usb_host_pipe_t large = {512U, 3U, 1U, NX_USB_HOST_HIGH_SPEED,
                                      NX_USB_HOST_BULK};
    EXPECT_EQ(nx_usb_host_open(&port, 0U, &large), NX_ERROR_INVALID);
    EXPECT_FALSE(channels[0].configured);
}

TEST_F(UsbHostHal, SelectedAhbClockFloorIsIndependentOfPhyClockFrequency) {
    g_nx_usb_host_hclk_hz = 14000000U;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB2ENR, 0U);
    EXPECT_EQ(state.config, nullptr);
    if (state.config != nullptr) {
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
        Stop();
    }
    config.phy = NX_STM32_USB_HS_ULPI;
    config.ulpi_clock_hz = 60000000U;
    g_nx_usb_host_hclk_hz = 26000000U;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN, 0U);
    EXPECT_EQ(state.config, nullptr);
    if (state.config != nullptr) {
        core = USB_OTG_HS;
        core->GRSTCTL = USB_OTG_GRSTCTL_AHBIDL;
    }
}

TEST_F(UsbHostHal, DrainedSnapshotDoesNotAccessAReleasedClockOrHandle) {
    Start();
    state.hal.DisconnectCallback(&state.hal);
    Stop();
    ASSERT_EQ(nullptr, state.config);
    ASSERT_EQ(0U, state.clock_receipt);
    state.hal.Instance = nullptr;
    nx_usb_host_state_t output{};
    output.started = output.connected = true;
    EXPECT_EQ(nx_usb_host_state(&port, &output), NX_SUCCESS);
    EXPECT_FALSE(output.started);
    EXPECT_FALSE(output.connected);
    EXPECT_FALSE(output.closing);
}

TEST_F(UsbHostHal, FailedGateSnapshotCannotReadAnUnpublishedRegisterBank) {
    hold_gate = true;
    EXPECT_EQ(nx_stm32_usb_host_initialize(&state, &config, 1000U),
              NX_ERROR_IO);
    ASSERT_EQ(nullptr, state.hal.Instance);
    ASSERT_EXIT(
        {
            nx_usb_host_state_t output{};
            nx_result_t result = nx_usb_host_state(&port, &output);
            std::_Exit(result == NX_SUCCESS && !output.started ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
    hold_gate = false;
}
