/**
 * \file            qiming_rs485_factory_test.cpp
 * \brief           Generated USART2 and PG6 compose the real RS485 adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/uart_rs485.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
void USART2_IRQHandler(void);
}
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <type_traits>

static_assert(std::is_invocable_r_v<nx_uart_tx_validate_fn,
                                    decltype(&nx_factory_uart_tx_validator_ref),
                                    nx_uart_ref_t>);
static_assert(!std::is_invocable_v<decltype(&nx_factory_uart_tx_validator_ref),
                                   nx_gpio_ref_t>);

namespace {
class SystemPorts {
  public:
    MOCK_METHOD(nx_result_t, Preflight, ());
    MOCK_METHOD(nx_stm32_start_result_t, Start, ());
    MOCK_METHOD(int, Stop, ());
};
SystemPorts* system_ports;
nx_time_us_t now;
unsigned clock_reads;
unsigned starts;
unsigned register_writes;
constexpr uint32_t DE = UINT32_C(1) << 6U;

/* BSRR is a hardware command, not an ordinary memory latch. Only this
 * external model applies its set/reset effects to the emulated ODR. */
void gpio_latch(void) {
    uint32_t command = GPIOG->BSRR;
    GPIOG->ODR = (GPIOG->ODR | (command & 0xFFFFU)) & ~(command >> 16U);
}

class QimingRs485 : public testing::Test {
  protected:
    testing::NiceMock<SystemPorts> system;
    void* mapping = MAP_FAILED;
    nx_uart_rs485_state_t adapter{};
    nx_uart_rs485_config_t config{};
    const nx_uart_port_t face{&nx_uart_rs485_ops, &adapter};
    nx_uart_tx_request_t request{};
    std::array<uint8_t, 3> payload{0x51U, 0xA2U, 0x73U};
    bool bound = false;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x40000000)), mapping);
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        now = 100U;
        clock_reads = starts = register_writes = 0U;
        system_ports = &system;
        ON_CALL(system, Preflight()).WillByDefault(testing::Return(NX_SUCCESS));
        ON_CALL(system, Start())
            .WillByDefault(testing::Return(nx_stm32_start_result_t{0, 0, 0U}));
        ON_CALL(system, Stop()).WillByDefault(testing::Return(0));
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        if (mapping == MAP_FAILED)
            return;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        if (bound) {
            USART2->SR = USART_SR_TC;
            if (nx_uart_port_stop(&face) != NX_SUCCESS) {
                fire(USART_SR_TC);
                now += 100000U;
                service();
                now += 100000U;
                service();
                EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
            }
        }
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        system_ports = nullptr;
        EXPECT_EQ(0, munmap(mapping, 0x80000U));
    }
    nx_result_t bind(bool stopped_transport = false) {
        nx_platform_start_result_t start = nx_platform_start();
        if (start.primary != NX_SUCCESS)
            return start.primary;
        gpio_latch();
        config = {nx_factory_uart_ref(NX_UART_REF_SERIAL2),
                  nx_factory_gpio_ref(NX_GPIO_REF_DIRECTION),
                  nx_factory_uart_tx_validator_ref(NX_UART_REF_SERIAL2),
                  DE,
                  10U,
                  20U,
                  true};
        if (stopped_transport) {
            nx_result_t stop = nx_uart_port_stop(config.transport);
            if (stop != NX_SUCCESS)
                return stop;
        }
        nx_result_t result = nx_uart_rs485_initialize(&adapter, &config);
        bound = result == NX_SUCCESS;
        gpio_latch();
        return result;
    }
    void prepare(nx_time_us_t deadline = 10000U) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload.data(),
                                                 payload.size(), deadline));
    }
    void service(void) {
        nx_uart_port_service(&face);
        gpio_latch();
    }
    void fire(uint32_t status) {
        USART2->SR = status;
        g_f4_model_isr = true;
        USART2_IRQHandler();
        g_f4_model_isr = false;
        gpio_latch();
    }
    void active_one_byte(void) {
        ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(&face, &request));
        gpio_latch();
        ASSERT_NE(0U, GPIOG->ODR & DE);
        now += 10U;
        service();
        fire(USART_SR_TXE);
        EXPECT_EQ(payload[0], USART2->DR);
    }
    void complete_wire_and_hold(void) {
        fire(USART_SR_TC);
        EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        service();
        EXPECT_NE(0U, GPIOG->ODR & DE);
        EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        now += 19U;
        service();
        EXPECT_NE(0U, GPIOG->ODR & DE);
        now += 1U;
        service();
        EXPECT_EQ(0U, GPIOG->ODR & DE);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    }
};
} /* namespace */

TEST_F(QimingRs485, FactoryValidatorLookupIsPureAndExactBeforeStartup) {
    const uint32_t before_clock = RCC->APB1ENR;
    const uint32_t before_de = GPIOG->BSRR;
    const auto validator =
        nx_factory_uart_tx_validator_ref(NX_UART_REF_SERIAL2);
    EXPECT_NE(nullptr, validator);
    EXPECT_EQ(validator, nx_factory_uart_tx_validator(NX_UART_ID_SERIAL2));
    EXPECT_EQ(nullptr, nx_factory_uart_tx_validator_ref(NX_UART_REF_COUNT));
    EXPECT_EQ(nullptr, nx_factory_uart_tx_validator(UINT16_MAX));
    EXPECT_EQ(before_clock, RCC->APB1ENR);
    EXPECT_EQ(before_de, GPIOG->BSRR);
    EXPECT_EQ(0U, starts);
    EXPECT_EQ(0U, register_writes);
    EXPECT_EQ(0U, clock_reads);
}

TEST_F(QimingRs485, RealTransportPreflightRefusesBeforeAnyDirectionAssertion) {
    ASSERT_EQ(NX_SUCCESS, bind(true));
    prepare();
    const uint32_t before_de = GPIOG->BSRR;
    const uint32_t before_control = USART2->CR1;
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&face, &request));
    gpio_latch();
    EXPECT_EQ(before_de, GPIOG->BSRR);
    EXPECT_EQ(before_control, USART2->CR1);
    EXPECT_EQ(0U, GPIOG->ODR & DE);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
}

TEST_F(QimingRs485, DataLoadedDoesNotReleaseDeBeforeWireTcAndHold) {
    ASSERT_EQ(NX_SUCCESS, bind());
    prepare();
    active_one_byte();
    fire(USART_SR_TXE);
    fire(USART_SR_TXE);
    EXPECT_EQ(payload[2], USART2->DR);
    service();
    EXPECT_NE(0U, GPIOG->ODR & DE);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    /* More than the authored hold time after the last DR load is still not
     * wire completion. A TXE-plus-hold shortcut must fail this observation. */
    now += 100U;
    service();
    EXPECT_NE(0U, GPIOG->ODR & DE);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    complete_wire_and_hold();
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(payload.size(), request.base.transferred);
}

TEST_F(QimingRs485, ActiveCancelQuarantinesUntilRealTcThenMandatoryHold) {
    ASSERT_EQ(NX_SUCCESS, bind());
    prepare();
    active_one_byte();
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_cancel(&face, &request));
    now += 500U;
    service();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_NE(0U, GPIOG->ODR & DE);
    nx_result_t result = NX_SUCCESS;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_request_result(&request.base, &result, &count));
    EXPECT_EQ(99U, count);
    complete_wire_and_hold();
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(1U, request.base.transferred);
}

TEST_F(QimingRs485, DeadlineCannotReleaseTheBufferOrDirectionWithoutWireTc) {
    ASSERT_EQ(NX_SUCCESS, bind());
    prepare(150U);
    active_one_byte();
    now = 151U;
    service();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(0U, GPIOG->ODR & DE);
    now += 500U;
    service();
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    complete_wire_and_hold();
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
    EXPECT_EQ(1U, request.base.transferred);
}

TEST_F(QimingRs485, StopRetainsLiveBorrowAndClosesFurtherAdmissionUntilDrain) {
    ASSERT_EQ(NX_SUCCESS, bind());
    prepare();
    active_one_byte();
    EXPECT_EQ(NX_ERROR_BUSY, nx_uart_port_stop(&face));
    EXPECT_NE(0U, GPIOG->ODR & DE);
    nx_uart_tx_request_t later{};
    nx_request_initialize(&later.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&later, payload.data(),
                                             payload.size(), 10000U));
    EXPECT_EQ(NX_ERROR_STATE, nx_uart_port_submit(&face, &later));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&later.base));
    complete_wire_and_hold();
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(NX_SUCCESS, nx_uart_port_stop(&face));
    bound = false;
}

TEST_F(QimingRs485, ReceiveErrorsRemainEventsWithoutChangingDirection) {
    ASSERT_EQ(NX_SUCCESS, bind());
    const uint32_t before_command = GPIOG->BSRR;
    USART2->DR = 0xE1U;
    fire(USART_SR_RXNE | USART_SR_FE);
    std::array<nx_uart_rx_event_t, 2> events{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_read_events(&face, events.data(),
                                                   events.size(), &count));
    ASSERT_EQ(1U, count);
    EXPECT_EQ(0xE1U, events[0].byte);
    EXPECT_NE(0U, events[0].flags & NX_UART_EVENT_FRAMING);
    EXPECT_EQ(now, events[0].timestamp_us);
    EXPECT_EQ(before_command, GPIOG->BSRR);
    EXPECT_EQ(0U, GPIOG->ODR & DE);
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    ++clock_reads;
    return now;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return system_ports->Preflight();
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return system_ports->Start();
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return system_ports->Stop();
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++register_writes;
    for (unsigned bank = 0U; bank < 8U; ++bank) {
        if (reg == &NVIC->ISER[bank]) {
            NVIC->ISER[bank] |= value;
            return;
        }
        if (reg == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            return;
        }
        if (reg == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    *reg = value;
    for (GPIO_TypeDef* gpio : {GPIOA, GPIOG}) {
        if (reg == &gpio->BSRR) {
            gpio->ODR = (gpio->ODR | (value & 0xFFFFU)) & ~(value >> 16U);
            return;
        }
    }
}
extern "C" void nx_control_model_clear(volatile uint32_t* reg, uint32_t mask) {
    *reg &= ~mask;
}
extern "C" void nx_uart_cold_model_priority_write(volatile uint8_t* reg,
                                                  uint8_t value) {
    *reg = value;
}
extern "C" void nx_control_model_priority(IRQn_Type irq, uint32_t priority) {
    nx_uart_cold_model_priority_write(&NVIC->IPR[static_cast<unsigned>(irq)],
                                      static_cast<uint8_t>(priority << 4U));
}
extern "C" uint32_t nx_uart_cold_model_priority_read(IRQn_Type irq) {
    return NVIC->IPR[static_cast<unsigned>(irq)] >> 4U;
}
extern "C" void nx_control_model_enable(IRQn_Type irq) {
    const unsigned index = static_cast<unsigned>(irq);
    nx_platform_cold_model_write(&NVIC->ISER[index / 32U], 1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_disable(IRQn_Type irq) {
    const unsigned index = static_cast<unsigned>(irq);
    nx_platform_cold_model_write(&NVIC->ICER[index / 32U], 1U << (index % 32U));
}
extern "C" void nx_uart_cold_model_pending_clear(IRQn_Type irq) {
    const unsigned index = static_cast<unsigned>(irq);
    nx_platform_cold_model_write(&NVIC->ICPR[index / 32U], 1U << (index % 32U));
}
