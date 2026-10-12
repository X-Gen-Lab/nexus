/**
 * \file            usb_factory_test.cpp
 * \brief           Actual generated USB faces and cold retained SDK ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "usb_device.h"
#ifdef NX_USB_FACTORY_STM32
#include "stm32f407_provider.h"
#include "stm32f407_usb_device.h"
#else
#include "gd32f470_provider.h"
#include "gd32f470_usb_device.h"
#include "gd32f4xx_rcu.h"
#endif
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

using testing::_;
using testing::NiceMock;
using testing::Return;

class UsbAssemblyPorts {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
    MOCK_METHOD(nx_result_t, EnableClock, (unsigned));
    MOCK_METHOD(nx_result_t, Configure, (unsigned, unsigned));
    MOCK_METHOD(uint16_t, Capture, (unsigned, unsigned));
    MOCK_METHOD(void, Restore, (unsigned, unsigned, uint16_t));
};
static UsbAssemblyPorts* s_ports;
static std::array<uint16_t, 16> s_pins;
static bool s_hold_restore;
static bool s_hold_clock_stop;
static bool s_hold_gpio_stop;

extern "C" void nx_usb_factory_register_write(volatile uint32_t* address,
                                              uint32_t value) {
#ifdef NX_USB_FACTORY_STM32
    if (s_hold_gpio_stop && address == &RCC->AHB1ENR &&
        (value & RCC_AHB1ENR_GPIOAEN) == 0U) {
        return;
    }
#endif
    *address = value;
}

#ifdef NX_USB_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_nx_usb_factory_nvic;
}
/** \brief Observe the external cold system admission, not its implementation.
 */
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return s_ports->Preflight();
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {s_ports->Start(), 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return s_ports->Stop();
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned index) {
    nx_result_t result = s_ports->EnableClock(index);
    if (result == NX_SUCCESS) {
        RCC->AHB1ENR |= 1U << index;
    }
    return result;
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef*, uint8_t pin) {
    return s_ports->Capture(0U, pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef*, uint8_t pin,
                                     uint16_t state) {
    s_ports->Restore(0U, pin, state);
    if (!s_hold_restore) {
        s_pins[pin] = state;
    }
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef*, uint8_t pin,
                                              uint8_t, uint8_t, uint8_t, bool) {
    nx_result_t result = s_ports->Configure(0U, pin);
    if (result == NX_SUCCESS) {
        s_pins[pin] = 0x456U;
    }
    return result;
}
#else
extern "C" {
NVIC_Type g_gd32_model_nvic;
}
/** \brief A separately tested SoC supplies this read-only admission result. */
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    return s_ports->Preflight();
}
extern "C" int nx_gd32_soc_start(void) {
    return s_ports->Start();
}
extern "C" int nx_gd32_soc_stop(void) {
    return s_ports->Stop();
}
extern "C" nx_result_t nx_gd32_gpio_clock_enable(unsigned index) {
    nx_result_t result = s_ports->EnableClock(index);
    if (result == NX_SUCCESS) {
        RCU_AHB1EN |= 1U << index;
    }
    return result;
}
extern "C" uint16_t nx_gd32_pin_capture(uint32_t, unsigned pin) {
    return s_ports->Capture(0U, pin);
}
extern "C" void nx_gd32_pin_restore(uint32_t, unsigned pin, uint16_t state) {
    s_ports->Restore(0U, pin, state);
    if (!s_hold_restore) {
        s_pins[pin] = state;
    }
}
extern "C" nx_result_t nx_gd32_pin_configure(uint32_t, unsigned pin, uint8_t,
                                             uint8_t, uint8_t, bool) {
    nx_result_t result = s_ports->Configure(0U, pin);
    if (result == NX_SUCCESS) {
        s_pins[pin] = 0x456U;
    }
    return result;
}
#endif
extern "C" void nx_usb_clock_model_write(volatile uint32_t* address,
                                         uint32_t value) {
#ifdef NX_USB_FACTORY_STM32
    if (s_hold_clock_stop && address == &RCC->AHB2ENR &&
        (value & RCC_AHB2ENR_OTGFSEN) == 0U) {
        return;
    }
#else
    if (s_hold_clock_stop && address == &RCU_AHB2EN &&
        (value & RCU_AHB2EN_USBFSEN) == 0U) {
        return;
    }
#endif
    *address = value;
#ifndef NX_USB_FACTORY_STM32
    if (address == &RCU_CTL) {
        if ((value & RCU_CTL_PLLSAIEN) != 0U) {
            RCU_CTL |= RCU_CTL_PLLSAISTB;
        } else {
            RCU_CTL &= ~static_cast<uint32_t>(RCU_CTL_PLLSAISTB);
        }
    }
#endif
}
extern "C" void nx_usb_clock_model_poll(void) {
}

class UsbFactory : public testing::Test {
  protected:
    NiceMock<UsbAssemblyPorts> ports;
    void* clocks = MAP_FAILED;
    nx_native_usb_hardware_t* hardware = nullptr;
    const nx_usb_device_port_t* face = nullptr;
    const nx_usb_endpoint_port_t* bulk = nullptr;
    nx_usb_controller_t* state = nullptr;
    nx_usb_request_t request = {};
    uint8_t payload[8] = {};

    void SetUp() override {
        s_ports = &ports;
        ON_CALL(ports, Preflight()).WillByDefault(Return(NX_SUCCESS));
        std::memset(NVIC, 0, sizeof(*NVIC));
        s_hold_restore = s_hold_clock_stop = s_hold_gpio_stop = false;
        s_pins.fill(0x123U);
        ON_CALL(ports, EnableClock(_)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Configure(_, _)).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Capture(_, _)).WillByDefault([](unsigned, unsigned pin) {
            return s_pins[pin];
        });
        clocks = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(clocks, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        void* core =
            mmap(reinterpret_cast<void*>(UINT32_C(0x50000000)), 0x10000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(core, reinterpret_cast<void*>(UINT32_C(0x50000000)));
        hardware = static_cast<nx_native_usb_hardware_t*>(core);
        hardware->registers[0x10U / 4U] = 1U << 31U;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
#ifdef NX_USB_FACTORY_STM32
        RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
#else
        RCU_PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
        RCU_CTL =
            RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
#endif
        face = nx_factory_usb_device(NX_USB_DEVICE_ID_FS);
        bulk = nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN);
        ASSERT_NE(face, nullptr);
        ASSERT_NE(bulk, nullptr);
        state = static_cast<nx_usb_controller_t*>(face->context);
    }
    void TearDown() override {
        if (hardware != nullptr) {
            s_hold_restore = s_hold_clock_stop = s_hold_gpio_stop = false;
            hardware->hold_reset = hardware->hold_flush = false;
            hardware->hold_disable = hardware->hold_clear = false;
            hardware->hold_irq_disable = hardware->hold_gonak = false;
            EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
            EXPECT_EQ(munmap(hardware, 0x10000U), 0);
        }
        if (clocks != MAP_FAILED) {
            EXPECT_EQ(munmap(clocks, 0x80000U), 0);
        }
        s_ports = nullptr;
    }
    void ReceiveAddressSetup() {
        ASSERT_EQ(hardware->rx_count, 0U);
        hardware->rx_head = 0U;
        hardware->rx[0] = {};
        hardware->rx[0].status = (8U << 4U) | (6U << 17U);
        hardware->rx[0].words[0] = UINT32_C(0x00070500);
        hardware->rx_count = 1U;
        hardware->registers[0x14U / 4U] |= 16U;
        nx_usb_controller_irq(state);
        ASSERT_TRUE(state->snapshot.control_pending);
        ASSERT_EQ(state->snapshot.setup.request, 5U);
    }
};

TEST_F(UsbFactory, BasePreflightRejectsBeforeClockPinAndControllerEffects) {
    const auto* bytes = static_cast<const uint8_t*>(clocks);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    EXPECT_FALSE(state->snapshot.started);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, PureFactoryDoesNotStartAndControllerIsNotAnEndpoint) {
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_FS), face);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN), bulk);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_COUNT), nullptr);
    EXPECT_NE(face->context, bulk->context);
    EXPECT_EQ(face->ops, &nx_usb_fs_device_ops);
    EXPECT_EQ(bulk->ops, &nx_usb_fs_endpoint_ops);
    EXPECT_FALSE(state->snapshot.started);
}
TEST_F(UsbFactory, CapabilityDescribesExactlyOneSelectedPioPacket) {
    const nx_factory_io_capability_t* capability =
        nx_factory_usb_endpoint_capability(NX_USB_ENDPOINT_ID_BULK_IN);
    ASSERT_NE(capability, nullptr);
    EXPECT_EQ(capability->max_transfer_units, 64U);
    EXPECT_EQ(capability->max_messages, 1U);
    EXPECT_EQ(capability->unit_bytes, 1U);
    EXPECT_EQ(capability->features, NX_FACTORY_CAP_IRQ);
}
TEST_F(UsbFactory, RealSelectedDirectionsFifoPlanAndClockConsumerAreBound) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(state->registers, 0x50000000U);
    EXPECT_EQ(state->endpoint_count, 4U);
    EXPECT_EQ(state->capacity, 8U);
    EXPECT_EQ(state->rx_fifo_words, 96U);
    EXPECT_EQ(state->irq.irq_number, 67);
    EXPECT_TRUE(state->clock_retained);
    EXPECT_EQ(hardware->registers[0x28U / 4U], (32U << 16U) | 96U);
    EXPECT_EQ(hardware->registers[0x104U / 4U], (32U << 16U) | 128U);
    EXPECT_EQ(static_cast<nx_usb_endpoint_t*>(bulk->context)->controller,
              state);
}
TEST_F(UsbFactory, FailedColdDcdIsCountedAndRetainsClockAndPinsUntilProof) {
    hardware->hold_reset = true;
    hardware->irq_enabled = hardware->hold_irq_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_TRUE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
    hardware->hold_reset = hardware->hold_irq_disable = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, FailedClockReleaseKeepsBothPinLeases) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    s_hold_clock_stop = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
    s_hold_clock_stop = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, FailedPinReadbackRetainsOnlyTheUnreleasedLease) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    s_hold_restore = true;
    EXPECT_CALL(ports, Restore(0U, 12U, 0x123U)).Times(1);
    EXPECT_CALL(ports, Restore(0U, 11U, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    testing::Mock::VerifyAndClearExpectations(&ports);
    s_hold_restore = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, PartialPinConfigurationRestoresExactlyCapturedPins) {
    EXPECT_CALL(ports, Configure(0U, 11U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Configure(0U, 12U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
}
TEST_F(UsbFactory, BusyPioLoanRetainsTheSameRequestUntilDetachAndSettlement) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_in_prepare(&request, payload, sizeof(payload), NX_USB_DATA,
                                0U, 10000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(bulk, &request), NX_SUCCESS);
    hardware->hold_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(static_cast<nx_usb_endpoint_t*>(bulk->context)->request,
              &request);
    testing::Mock::VerifyAndClearExpectations(&ports);
    hardware->hold_disable = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(UsbFactory, SuccessfulDrainAllowsTheSameStaticIdentityToRestart) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_FS), face);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN), bulk);
    EXPECT_TRUE(state->clock_retained);
}
TEST_F(UsbFactory, RestartCannotReuseAStaleSetupEpochForSetAddress) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ReceiveAddressSetup();
    const uint64_t old_epoch = state->snapshot.epoch;
    ASSERT_EQ(nx_usb_device_address(face, old_epoch, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ReceiveAddressSetup();
    EXPECT_GT(state->snapshot.epoch, old_epoch);
    EXPECT_EQ(nx_usb_device_address(face, old_epoch, 7U), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_device_address(face, state->snapshot.epoch, 7U),
              NX_SUCCESS);
}
TEST_F(UsbFactory, ExhaustedLifetimeEpochCannotBeReopenedByColdReconstruction) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    const uint64_t original = state->snapshot.epoch;
    state->snapshot.epoch = UINT64_MAX;
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_INVALID);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(state->snapshot.epoch, UINT64_MAX);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    state->snapshot.epoch = original;
}
#ifdef NX_USB_FACTORY_STM32
TEST_F(UsbFactory, RefusedOwnedGpioGateWithdrawalRetainsPlatformUntilReadback) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_NE(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
    s_hold_gpio_stop = true;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    s_hold_gpio_stop = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
}
#endif
