/**
 * \file            driver_test.cpp
 * \brief           Actual GD32 SPL paths under explicit MMIO observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32_reference.h"
#include "nexus/arch/arch.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>

namespace {
uint64_t now;
uint32_t masks;
bool irq_enabled;
uint32_t irq_priority;
bool gpio_mode_fault;
uint32_t gpio_mode_value;
class Gd32Reference : public testing::Test {
  protected:
    nx_gd32_gpio_state_t gpio{};
    nx_gd32_uart_state_t uart{};
    nx_uart_rx_event_t events[2]{};
    nx_gd32_gpio_config_t gpio_config{GPIOA, GPIO_PIN_4, GPIO_PIN_4};
    nx_gd32_uart_config_t uart_config{USART0, 115200U, events, 2U, 5U};
    nx_gpio_port_t gpio_face{&nx_gd32_gpio_ops, &gpio};
    nx_uart_port_t uart_face{&nx_gd32_uart_ops, &uart};
    void* peripherals{MAP_FAILED};
    void* cortex{MAP_FAILED};
    void SetUp() override {
        peripherals = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(peripherals, MAP_FAILED);
        cortex = mmap(reinterpret_cast<void*>(0xe000e000U), 0x1000U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(cortex, MAP_FAILED);
        std::memset(peripherals, 0, 0x30000U);
        std::memset(cortex, 0, 0x1000U);
        RCU_CTL = RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB;
        RCU_AHB1EN = RCU_AHB1EN_PAEN;
        RCU_APB2EN = RCU_APB2EN_USART0EN;
        USART_STAT0(USART0) = USART_STAT0_TC | USART_STAT0_TBE;
        now = 10U;
        masks = 0U;
        irq_enabled = false;
        irq_priority = 7U;
        gpio_mode_fault = false;
        gpio_mode_value = 0U;
    }
    void TearDown() override {
        if (peripherals != MAP_FAILED) {
            EXPECT_EQ(munmap(peripherals, 0x30000U), 0);
        }
        if (cortex != MAP_FAILED) {
            EXPECT_EQ(munmap(cortex, 0x1000U), 0);
        }
    }
    void start_uart() {
        ASSERT_EQ(nx_gd32_uart_prepare(&uart, &uart_config), NX_SUCCESS);
    }
    void irq(uint32_t status) {
        USART_STAT0(USART0) = status;
        nx_gd32_uart_irq(&uart);
    }
};
} /* namespace */

TEST_F(Gd32Reference, GpioMasksInputAndCombinesAtomicSetReset) {
    ASSERT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    GPIO_ISTAT(GPIOA) = GPIO_PIN_4 | GPIO_PIN_5;
    uint32_t value = 0U;
    EXPECT_EQ(nx_gpio_port_read(&gpio_face, &value), NX_SUCCESS);
    EXPECT_EQ(value, static_cast<uint32_t>(GPIO_PIN_4));
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, 0U, GPIO_PIN_4), NX_SUCCESS);
    EXPECT_EQ(GPIO_BOP(GPIOA), static_cast<uint32_t>(GPIO_PIN_4 << 16U));
    const auto before = GPIO_BOP(GPIOA);
    EXPECT_EQ(nx_gpio_port_write(&gpio_face, GPIO_PIN_5, 0U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(GPIO_BOP(GPIOA), before);
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_SUCCESS);
}

TEST_F(Gd32Reference, DefaultOutputBindingPreservesSelectedSlew) {
    GPIO_OSPD(GPIOA) = 2U << 8U;
    ASSERT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    EXPECT_EQ(GPIO_OSPD(GPIOA) & (3U << 8U), 2U << 8U);
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_SUCCESS);
}

TEST_F(Gd32Reference, InvalidPinOwnershipRejectsBeforeMutation) {
    gpio_config.mask = 0U;
    const auto before = GPIO_CTL(GPIOA);
    EXPECT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_ERROR_INVALID);
    EXPECT_EQ(GPIO_CTL(GPIOA), before);
    EXPECT_EQ(gpio.config, nullptr);
}

TEST_F(Gd32Reference, GpioInstancesDoNotShareMutableState) {
    ASSERT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    RCU_AHB1EN |= RCU_AHB1EN_PBEN;
    nx_gd32_gpio_state_t second{};
    nx_gd32_gpio_config_t config{GPIOB, GPIO_PIN_1, 0U};
    ASSERT_EQ(nx_gd32_gpio_prepare(&second, &config), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_SUCCESS);
    nx_gpio_port_t face{&nx_gd32_gpio_ops, &second};
    EXPECT_EQ(nx_gpio_port_write(&face, GPIO_PIN_1, 0U), NX_SUCCESS);
    EXPECT_EQ(GPIO_BOP(GPIOB), static_cast<uint32_t>(GPIO_PIN_1));
    EXPECT_EQ(nx_gd32_gpio_close(&second), NX_SUCCESS);
}

TEST_F(Gd32Reference, UartBorrowsUntilObservedWireCompletionAndService) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t byte = 0x5aU;
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 100U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    irq(USART_STAT0_TBE | USART_STAT0_TC);
    EXPECT_EQ(USART_DATA(USART0), 0x5aU);
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    irq(USART_STAT0_TC);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(uart.active, nullptr);
    nx_result_t result = NX_ERROR_IO;
    size_t count = 0U;
    EXPECT_EQ(nx_request_result(&request.base, &result, &count), NX_SUCCESS);
    EXPECT_EQ(result, NX_SUCCESS);
    EXPECT_EQ(count, 1U);
}

TEST_F(Gd32Reference, CancelRetainsBorrowUntilWireDrainAndStopRefuses) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t bytes[]{0x42U, 0x43U};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof bytes, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    irq(USART_STAT0_TBE);
    EXPECT_EQ(nx_uart_port_cancel(&uart_face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_stop(&uart_face), NX_ERROR_BUSY);
    EXPECT_EQ(uart.active, &request);
    irq(USART_STAT0_TBE);
    EXPECT_EQ(USART_DATA(USART0), 0x42U);
    irq(USART_STAT0_TC);
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_uart_port_stop(&uart_face), NX_SUCCESS);
}

TEST_F(Gd32Reference, ExpiredAdmissionHasNoBorrowOrInterruptEffects) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t byte = 1U;
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, now), NX_SUCCESS);
    const auto control = USART_CTL0(USART0);
    EXPECT_EQ(nx_uart_port_submit(&uart_face, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(uart.active, nullptr);
    EXPECT_EQ(USART_CTL0(USART0), control);
}

TEST_F(Gd32Reference, ReceiveEventsKeepErrorsAndReportCapacityLoss) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    USART_DATA(USART0) = 0x11U;
    irq(USART_STAT0_RBNE | USART_STAT0_FERR);
    USART_DATA(USART0) = 0x22U;
    irq(USART_STAT0_RBNE);
    USART_DATA(USART0) = 0x33U;
    irq(USART_STAT0_RBNE);
    nx_uart_rx_event_t copied[2]{};
    size_t count = 0U;
    EXPECT_EQ(nx_uart_port_read_events(&uart_face, copied, 2U, &count),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(copied[0].byte, 0x11U);
    EXPECT_EQ(copied[0].flags, NX_UART_EVENT_FRAMING);
    EXPECT_EQ(copied[1].byte, 0x22U);
}

TEST_F(Gd32Reference, PendingLossBoundaryCannotBeOvertakenByNewBytes) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    for (uint32_t byte : {0x11U, 0x22U, 0x33U}) {
        USART_DATA(USART0) = byte;
        irq(USART_STAT0_RBNE);
    }
    nx_uart_rx_event_t first{};
    size_t count = 0U;
    ASSERT_EQ(nx_uart_port_read_events(&uart_face, &first, 1U, &count),
              NX_ERROR_OVERFLOW);
    USART_DATA(USART0) = 0x44U;
    irq(USART_STAT0_RBNE);
    nx_uart_rx_event_t rest[3]{};
    ASSERT_EQ(nx_uart_port_read_events(&uart_face, rest, 3U, &count),
              NX_ERROR_OVERFLOW);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(rest[0].byte, 0x22U);
    EXPECT_EQ(rest[1].flags, NX_UART_EVENT_LOSS | NX_UART_EVENT_NO_BYTE);
}

TEST_F(Gd32Reference, ReceiveCountCannotOverwriteLiveControllerMetadata) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    USART_DATA(USART0) = 0x55U;
    irq(USART_STAT0_RBNE);
    nx_uart_rx_event_t result{99U, 99U, 99U};
    EXPECT_EQ(nx_uart_port_read_events(&uart_face, &result, 1U, &uart.tail),
              NX_ERROR_INVALID);
    EXPECT_EQ(result.byte, 99U);
    EXPECT_EQ(uart.queued, 1U);
}

TEST_F(Gd32Reference, TransmitDescriptorCannotOccupyReceiveStorage) {
    alignas(nx_uart_tx_request_t) nx_uart_rx_event_t ring[16]{};
    uart_config.rx_storage = ring;
    uart_config.rx_capacity = 16U;
    ASSERT_NO_FATAL_FAILURE(start_uart());
    auto* request = new (ring) nx_uart_tx_request_t{};
    uint8_t byte = 0x5aU;
    nx_request_initialize(&request->base);
    ASSERT_EQ(nx_uart_tx_prepare(request, &byte, 1U, 100U), NX_SUCCESS);
    const auto control = USART_CTL0(USART0);
    EXPECT_EQ(nx_gd32_uart_validate(&uart, request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_port_submit(&uart_face, request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request->base), NX_REQUEST_READY);
    EXPECT_EQ(uart.active, nullptr);
    EXPECT_EQ(USART_CTL0(USART0), control);
}

TEST_F(Gd32Reference, ReceiveOutputCannotOverwriteBorrowedTransmitDescriptor) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t byte = 0x42U;
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 100U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    USART_DATA(USART0) = 0x55U;
    irq(USART_STAT0_RBNE);
    size_t count = 99U;
    EXPECT_EQ(nx_uart_port_read_bytes(
                  &uart_face, reinterpret_cast<uint8_t*>(&request), 1U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(count, 99U);
    EXPECT_EQ(uart.queued, 1U);
}

TEST_F(Gd32Reference, ReceiveOutputCannotOverwriteBorrowedTransmitPayload) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    alignas(nx_uart_rx_event_t) uint8_t bytes[64]{};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof bytes, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    USART_DATA(USART0) = 0x55U;
    irq(USART_STAT0_RBNE);
    size_t count = 99U;
    EXPECT_EQ(nx_uart_port_read_events(
                  &uart_face, reinterpret_cast<nx_uart_rx_event_t*>(bytes), 1U,
                  &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(count, 99U);
    EXPECT_EQ(bytes[0], 0U);
    EXPECT_EQ(uart.queued, 1U);
}

TEST_F(Gd32Reference, ReceiveCountCannotOverwriteBorrowedTransmitDescriptor) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t byte = 0x42U;
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, &byte, 1U, 100U), NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    uint8_t output = 99U;
    EXPECT_EQ(nx_uart_port_read_bytes(&uart_face, &output, 1U, &request.length),
              NX_ERROR_INVALID);
    EXPECT_EQ(request.length, 1U);
    EXPECT_EQ(output, 99U);
}

TEST_F(Gd32Reference, ReceiveCountCannotOverwriteBorrowedTransmitPayload) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    alignas(size_t) uint8_t bytes[64]{};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof bytes, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    nx_uart_rx_event_t output{99U, 99U, 99U};
    EXPECT_EQ(nx_uart_port_read_events(&uart_face, &output, 1U,
                                       reinterpret_cast<size_t*>(bytes)),
              NX_ERROR_INVALID);
    EXPECT_EQ(output.byte, 99U);
}

TEST_F(Gd32Reference, StopClosesAdmissionAndDrainsWithoutFurtherPayloadReads) {
    ASSERT_NO_FATAL_FAILURE(start_uart());
    uint8_t bytes[]{0x42U, 0x43U};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, bytes, sizeof bytes, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&uart_face, &request), NX_SUCCESS);
    irq(USART_STAT0_TBE);
    USART_STAT0(USART0) = 0U;
    EXPECT_EQ(nx_uart_port_stop(&uart_face), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(USART_CTL0(USART0) & USART_CTL0_TBEIE, 0U);
    irq(USART_STAT0_TBE);
    EXPECT_EQ(USART_DATA(USART0), 0x42U);
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(uart.active, &request);
    EXPECT_NE(uart.config, nullptr);
    irq(USART_STAT0_TC);
    nx_uart_port_service(&uart_face);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    nx_uart_tx_request_t next{};
    nx_request_initialize(&next.base);
    ASSERT_EQ(nx_uart_tx_prepare(&next, bytes, sizeof bytes, 100U), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_uart_validate(&uart, &next), NX_ERROR_STATE);
    EXPECT_EQ(nx_uart_port_submit(&uart_face, &next), NX_ERROR_STATE);
    EXPECT_EQ(nx_uart_port_stop(&uart_face), NX_SUCCESS);
    EXPECT_EQ(uart.config, nullptr);
}

TEST_F(Gd32Reference, LockedSelectedGpioRejectsBeforeLatchOrModeWrites) {
    GPIO_LOCK(GPIOA) = GPIO_LOCK_LKK | GPIO_PIN_4;
    GPIO_BOP(GPIOA) = 0x55U;
    EXPECT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_ERROR_BUSY);
    EXPECT_EQ(GPIO_BOP(GPIOA), 0x55U);
    EXPECT_EQ(GPIO_CTL(GPIOA), 0U);
    EXPECT_EQ(gpio.config, nullptr);
}

TEST_F(Gd32Reference, PrepareReadbackFailureRetainsGpioReceiptForCleanup) {
    gpio_mode_fault = true;
    gpio_mode_value = 0U;
    EXPECT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_ERROR_IO);
    EXPECT_EQ(gpio.config, &gpio_config);
    gpio_mode_fault = false;
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(gpio.config, nullptr);
}

TEST_F(Gd32Reference, RestoreReadbackFailureKeepsGpioReceiptUntilRetry) {
    ASSERT_EQ(nx_gd32_gpio_prepare(&gpio, &gpio_config), NX_SUCCESS);
    gpio_mode_fault = true;
    gpio_mode_value = 1U << 8U;
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_ERROR_IO);
    EXPECT_EQ(gpio.config, &gpio_config);
    gpio_mode_fault = false;
    EXPECT_EQ(nx_gd32_gpio_close(&gpio), NX_SUCCESS);
    EXPECT_EQ(GPIO_CTL(GPIOA) & (3U << 8U), 0U);
    EXPECT_EQ(gpio.config, nullptr);
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t result{masks};
    masks = 1U;
    return result;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    masks = saved.value;
}
extern "C" void nx_arch_dsb(void) {
    for (uint32_t port : {GPIOA, GPIOB}) {
        const uint32_t command = GPIO_BOP(port);
        GPIO_OCTL(port) =
            (GPIO_OCTL(port) | (command & 0xffffU)) & ~(command >> 16U);
    }
    if (gpio_mode_fault)
        GPIO_CTL(GPIOA) = (GPIO_CTL(GPIOA) & ~(3U << 8U)) | gpio_mode_value;
}
extern "C" void nx_arch_isb(void) {
}

extern "C" bool nx_gd32_model_irq_enabled(void) {
    return irq_enabled;
}
extern "C" bool nx_gd32_model_irq_pending(void) {
    return false;
}
extern "C" bool nx_gd32_model_irq_active(void) {
    return false;
}
extern "C" uint32_t nx_gd32_model_irq_priority(void) {
    return irq_priority;
}
extern "C" void nx_gd32_model_irq_set_priority(uint32_t value) {
    irq_priority = value;
}
extern "C" void nx_gd32_model_irq_enable(void) {
    irq_enabled = true;
}
extern "C" void nx_gd32_model_irq_disable(void) {
    irq_enabled = false;
}
extern "C" void nx_gd32_model_irq_clear(void) {
}
