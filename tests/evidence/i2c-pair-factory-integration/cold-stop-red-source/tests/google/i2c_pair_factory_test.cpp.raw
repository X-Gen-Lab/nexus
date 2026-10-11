/**
 * \file            i2c_pair_factory_test.cpp
 * \brief           Real paired I2C factory objects, DMA and retained rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#ifdef NX_I2C_PAIR_STM32
#include "stm32f407_i2c_async.h"
void I2C1_EV_IRQHandler(void);
void I2C2_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);
void I2C2_ER_IRQHandler(void);
void DMA1_Stream0_IRQHandler(void);
void DMA1_Stream2_IRQHandler(void);
#else
#include "gd32f470_i2c_async.h"
#include "gd32f4xx_gpio.h"
void I2C0_EV_IRQHandler(void);
void I2C1_EV_IRQHandler(void);
void I2C0_ER_IRQHandler(void);
void I2C1_ER_IRQHandler(void);
void DMA0_Channel0_IRQHandler(void);
void DMA0_Channel2_IRQHandler(void);
#endif
}
#include <array>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#ifdef NX_I2C_PAIR_STM32
#define GATE        RCC->APB1ENR
#define DMA_GATE    RCC->AHB1ENR
#define PIN_MODE    GPIOB->MODER
#define PIN_TYPE    GPIOB->OTYPER
#define PIN_AF_LOW  GPIOB->AFR[0]
#define PIN_AF_HIGH GPIOB->AFR[1]
#define LINES       GPIOB->IDR
#define FIRST_EV    I2C1_EV_IRQHandler
#define SECOND_EV   I2C2_EV_IRQHandler
#define FIRST_ER    I2C1_ER_IRQHandler
#define SECOND_ER   I2C2_ER_IRQHandler
#define FIRST_RX    DMA1_Stream0_IRQHandler
#define SECOND_RX   DMA1_Stream2_IRQHandler
#define MHZ         42U
#else
#define GATE        RCU_APB1EN
#define DMA_GATE    RCU_AHB1EN
#define PIN_MODE    GPIO_CTL(GPIOB)
#define PIN_TYPE    GPIO_OMODE(GPIOB)
#define PIN_AF_LOW  GPIO_AFSEL0(GPIOB)
#define PIN_AF_HIGH GPIO_AFSEL1(GPIOB)
#define LINES       GPIO_ISTAT(GPIOB)
#define FIRST_EV    I2C0_EV_IRQHandler
#define SECOND_EV   I2C1_EV_IRQHandler
#define FIRST_ER    I2C0_ER_IRQHandler
#define SECOND_ER   I2C1_ER_IRQHandler
#define FIRST_RX    DMA0_Channel0_IRQHandler
#define SECOND_RX   DMA0_Channel2_IRQHandler
#define MHZ         50U
#endif

extern "C" {
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
}
namespace {
class ClockPort {
  public:
    MOCK_METHOD(int, Start, ());
    MOCK_METHOD(int, Stop, ());
};
ClockPort* clock_port;
nx_time_us_t now;
int refused_irq;
volatile uint32_t* held_engine;
volatile uint32_t* refused_register;
unsigned writes;
std::array<void (*)(void), 2> event_vectors{FIRST_EV, SECOND_EV};
std::array<void (*)(void), 2> error_vectors{FIRST_ER, SECOND_ER};
#ifndef NX_I2C_PAIR_IRQ
std::array<void (*)(void), 2> rx_vectors{FIRST_RX, SECOND_RX};
#endif
constexpr std::array<unsigned, 2> event_irqs{31U, 33U};
constexpr std::array<unsigned, 2> error_irqs{32U, 34U};
constexpr std::array<unsigned, 2> rx_irqs{11U, 13U};
constexpr std::array<unsigned, 2> tx_irqs{17U, 47U};
constexpr uint32_t controller_gate = (1U << 21U) | (1U << 22U);

bool enabled(unsigned irq) {
    return (NVIC->ISER[irq / 32U] & (1U << (irq % 32U))) != 0U;
}
std::array<uint32_t, 9> registers(const nx_i2c_v1_state_t* state) {
    std::array<uint32_t, 9> copy{};
    for (unsigned i = 0U; i < copy.size(); ++i)
        copy[i] = state->registers[i];
    return copy;
}

class I2CPairFactory : public testing::Test {
  protected:
    testing::NiceMock<ClockPort> clocks;
    const nx_i2c_port_t* ports[2]{};
    const nx_i2c_endpoint_t* endpoints[2]{};
    nx_i2c_v1_state_t* states[2]{};
    nx_i2c_request_t requests[2]{};
    nx_i2c_message_t messages[2]{};
    void* mmio = nullptr;
    void* sram = nullptr;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(uintptr_t{0x40000000U}), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(uintptr_t{0x40000000U}));
        sram = mmap(reinterpret_cast<void*>(uintptr_t{0x20000000U}), 4096U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(sram, reinterpret_cast<void*>(uintptr_t{0x20000000U}));
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        now = 100U;
        refused_irq = -1;
        held_engine = nullptr;
        refused_register = nullptr;
        writes = 0U;
        clock_port = &clocks;
        ON_CALL(clocks, Start()).WillByDefault(testing::Return(0));
        ON_CALL(clocks, Stop()).WillByDefault(testing::Return(0));
        LINES = 0xFFFFU;
#ifdef NX_I2C_PAIR_STM32
        g_nx_stm32_system.rcc = RCC;
#endif
        ports[0] = nx_binding_primary_i2c;
        ports[1] = nx_binding_secondary_i2c;
        endpoints[0] = nx_device_primary_sensor;
        endpoints[1] = nx_device_secondary_sensor;
        for (unsigned i = 0U; i < 2U; ++i) {
            states[i] = static_cast<nx_i2c_v1_state_t*>(ports[i]->context);
            nx_request_initialize(&requests[i].base);
            messages[i] = {static_cast<uint8_t*>(sram) + i * 16U, 3U, true};
        }
    }
    void TearDown() override {
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        refused_irq = -1;
        held_engine = nullptr;
        refused_register = nullptr;
        for (unsigned i = 0U; i < 2U; ++i) {
            if (states[i]->initialized) {
                if (states[i]->active != nullptr) {
                    EXPECT_EQ(NX_SUCCESS,
                              nx_i2c_port_cancel(ports[i], states[i]->active));
                }
                if (states[i]->dma != nullptr) {
                    *states[i]->dma->tx.control = 0U;
                    *states[i]->dma->rx.control = 0U;
                }
                states[i]->registers[0] &= ~((1U << 8U) | (1U << 9U));
                states[i]->registers[6] = 0U;
                nx_i2c_port_service(ports[i]);
            }
        }
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        clock_port = nullptr;
        if (sram != MAP_FAILED && sram != nullptr) {
            EXPECT_EQ(0, munmap(sram, 4096U));
        }
        if (mmio != MAP_FAILED && mmio != nullptr) {
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Submit(unsigned i) {
        ASSERT_EQ(NX_SUCCESS, nx_i2c_request_prepare(&requests[i], &messages[i],
                                                     1U, 1000U));
        ASSERT_EQ(NX_SUCCESS,
                  nx_i2c_endpoint_submit(endpoints[i], &requests[i]));
        states[i]->registers[6] = 3U;
        states[i]->registers[5] = 1U;
        event_vectors[i]();
        states[i]->registers[0] &= ~(1U << 8U);
        states[i]->registers[5] = 2U;
        event_vectors[i]();
#ifndef NX_I2C_PAIR_IRQ
        ASSERT_NE(nullptr, states[i]->dma);
        EXPECT_EQ(1U, *states[i]->dma->rx.control & 1U);
#else
        EXPECT_EQ(nullptr, states[i]->dma);
        EXPECT_EQ(NX_I2C_V1_READ, states[i]->phase);
#endif
    }
    void ReleasedWire(unsigned i) {
        states[i]->registers[0] &= ~((1U << 8U) | (1U << 9U));
        states[i]->registers[6] = 0U;
        nx_i2c_port_service(ports[i]);
    }
    void Complete(unsigned i) {
#ifdef NX_I2C_PAIR_IRQ
        states[i]->registers[4] = 0x71U;
        states[i]->registers[5] = 4U;
        event_vectors[i]();
        states[i]->registers[4] = 0x72U;
        states[i]->registers[5] = 4U;
        event_vectors[i]();
#else
        const auto& engine = states[i]->dma->rx;
        *engine.count = 0U;
        *const_cast<volatile uint32_t*>(engine.flags) |= 0x20U
                                                         << engine.flag_shift;
        rx_vectors[i]();
#endif
    }
};

TEST_F(I2CPairFactory, PureLookupHasDistinctControllersAndAddressedChildren) {
    EXPECT_NE(ports[0], ports[1]);
    EXPECT_NE(ports[0]->context, ports[1]->context);
    EXPECT_EQ(&nx_i2c_v1_async_ops, ports[0]->ops);
    EXPECT_EQ(ports[0]->ops, ports[1]->ops);
    EXPECT_EQ(ports[0], nx_factory_i2c(NX_I2C_ID_PRIMARY_I2C));
    EXPECT_EQ(ports[1], nx_factory_i2c(NX_I2C_ID_SECONDARY_I2C));
    EXPECT_EQ(nullptr, nx_factory_i2c(NX_I2C_ID_COUNT));
    EXPECT_EQ(endpoints[0],
              nx_factory_i2c_device(NX_I2C_DEVICE_ID_PRIMARY_SENSOR));
    EXPECT_EQ(endpoints[1],
              nx_factory_i2c_device(NX_I2C_DEVICE_ID_SECONDARY_SENSOR));
    EXPECT_EQ(nullptr, nx_factory_i2c_device(NX_I2C_DEVICE_ID_COUNT));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(endpoints[0], ports[0]));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(endpoints[0], ports[1]));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(endpoints[1], ports[1]));
    EXPECT_FALSE(nx_i2c_endpoint_on_port(endpoints[1], ports[0]));
    for (unsigned i = 0U; i < 2U; ++i) {
        const auto* child =
            static_cast<const nx_i2c_v1_endpoint_t*>(endpoints[i]->context);
        EXPECT_EQ(states[i], child->port);
        EXPECT_EQ(80U, child->address);
    }
    EXPECT_NE(endpoints[0]->context, endpoints[1]->context);
    EXPECT_EQ(&nx_i2c_v1_async_endpoint_ops, endpoints[0]->ops);
    EXPECT_EQ(endpoints[0]->ops, endpoints[1]->ops);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, GATE);
    EXPECT_EQ(0U, NVIC->ISER[0]);
    EXPECT_CALL(clocks, Start()).Times(0);
}

TEST_F(I2CPairFactory, ColdStopRejectsWithoutReferencesOrHardwareEffects) {
    EXPECT_EXIT(
        {
            const auto result = nx_i2c_port_stop(ports[0]);
            const bool rejected =
                result == NX_ERROR_STATE || result == NX_ERROR_INVALID;
            const bool untouched = writes == 0U && GATE == 0U &&
                                   !states[0]->initialized &&
                                   states[0]->active == nullptr;
            std::_Exit(rejected && untouched ? 0 : 23);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(I2CPairFactory, BothControllersUseExactPinsClockAndPublisherTuples) {
    Start();
#ifndef NX_I2C_PAIR_IRQ
    EXPECT_NE(0U, DMA_GATE & (1U << 21U));
#else
    EXPECT_EQ(0U, DMA_GATE & (1U << 21U));
#endif
    EXPECT_EQ(controller_gate, GATE & controller_gate);
    EXPECT_EQ(0xCC0U, PIN_TYPE & 0xCC0U);
    for (unsigned pin : {6U, 7U, 10U, 11U})
        EXPECT_EQ(2U, (PIN_MODE >> (2U * pin)) & 3U);
    EXPECT_EQ(0x44000000U, PIN_AF_LOW & 0xFF000000U);
    EXPECT_EQ(0x4400U, PIN_AF_HIGH & 0xFF00U);
    for (unsigned i = 0U; i < 2U; ++i) {
        EXPECT_TRUE(states[i]->initialized);
        EXPECT_EQ(MHZ, states[i]->peripheral_mhz);
        EXPECT_EQ(i == 0U ? 0xC0U : 0xC00U, states[i]->line_mask);
#ifndef NX_I2C_PAIR_IRQ
        EXPECT_NE(nullptr, states[i]->dma);
        EXPECT_EQ(i == 0U ? 1U : 7U,
                  (states[i]->dma->rx.configuration >> 25U) & 7U);
#else
        EXPECT_EQ(nullptr, states[i]->dma);
        EXPECT_FALSE(enabled(rx_irqs[i]));
        EXPECT_FALSE(enabled(tx_irqs[i]));
#endif
        for (unsigned irq : {event_irqs[i], error_irqs[i]}) {
            EXPECT_TRUE(enabled(irq));
            EXPECT_EQ(
                5U, nx_i2c_pair_irq_get_priority(static_cast<IRQn_Type>(irq)));
        }
    }
}

TEST_F(I2CPairFactory, SecondaryFailureRollsBackPrimaryAndAllowsFreshRestart) {
    refused_register = &GATE;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(states[0]->initialized);
    EXPECT_EQ(0U, GATE & controller_gate);
    refused_register = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    Start();
    EXPECT_TRUE(states[0]->initialized);
    EXPECT_TRUE(states[1]->initialized);
}

TEST_F(I2CPairFactory, OwnEventVectorCannotAdvanceThePeerRequest) {
    Start();
    Submit(0U);
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_request_prepare(&requests[1], &messages[1], 1U, 1000U));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_submit(endpoints[1], &requests[1]));
    const auto peer = registers(states[1]);
    states[0]->registers[5] = 0U;
    event_vectors[0]();
    EXPECT_EQ(peer, registers(states[1]));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[1].base));
}

#ifndef NX_I2C_PAIR_IRQ
TEST_F(I2CPairFactory, DmaTerminalPreservesPeerFlagsEngineAndBorrow) {
    Start();
    Submit(0U);
    Submit(1U);
    const auto& peer = states[1]->dma->rx;
    *const_cast<volatile uint32_t*>(peer.flags) |= 0x20U << peer.flag_shift;
    Complete(0U);
    EXPECT_NE(0U, *peer.flags & (0x20U << peer.flag_shift));
    EXPECT_EQ(1U, *peer.control & 1U);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[1].base));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&requests[0].base));
    ReleasedWire(0U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[0].base));
    EXPECT_EQ(3U, requests[0].base.transferred);
}

#endif

TEST_F(I2CPairFactory, SameAddressOnDifferentControllersKeepsIndependentLoans) {
    Start();
    Submit(0U);
    Submit(1U);
    EXPECT_EQ(0x50U, states[0]->address);
    EXPECT_EQ(0x50U, states[1]->address);
    EXPECT_EQ(&requests[0], states[0]->active);
    EXPECT_EQ(&requests[1], states[1]->active);
    Complete(1U);
    ReleasedWire(1U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[0].base));
}

TEST_F(I2CPairFactory,
       ForeignCancelCannotChangeOtherControllersRegistersOrLoans) {
    Start();
    Submit(0U);
    Submit(1U);
    const auto primary = registers(states[0]);
    const auto secondary = registers(states[1]);
    EXPECT_EQ(NX_ERROR_STATE, nx_i2c_port_cancel(ports[1], &requests[0]));
    EXPECT_EQ(primary, registers(states[0]));
    EXPECT_EQ(secondary, registers(states[1]));
    EXPECT_EQ(&requests[0], states[0]->active);
    EXPECT_EQ(&requests[1], states[1]->active);
}

TEST_F(I2CPairFactory, OneStopPreservesPeerPendingIrqsAndActiveRequest) {
    Start();
    Submit(1U);
    for (unsigned irq : {event_irqs[1], error_irqs[1], rx_irqs[1], tx_irqs[1]})
        NVIC->ISPR[irq / 32U] |= 1U << (irq % 32U);
    const auto pending = std::array<uint32_t, 2>{NVIC->ISPR[0], NVIC->ISPR[1]};
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(ports[0]));
    EXPECT_EQ(pending[0], NVIC->ISPR[0]);
    EXPECT_EQ(pending[1], NVIC->ISPR[1]);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[1].base));
#ifndef NX_I2C_PAIR_IRQ
    EXPECT_EQ(1U, *states[1]->dma->rx.control & 1U);
#else
    EXPECT_EQ(NX_I2C_V1_READ, states[1]->phase);
#endif
    EXPECT_TRUE(enabled(event_irqs[1]));
    EXPECT_FALSE(enabled(event_irqs[0]));
}

#ifndef NX_I2C_PAIR_IRQ
TEST_F(I2CPairFactory, RefusedSecondaryDmaDisableQuarantinesOnlyItsLoan) {
    Start();
    Submit(0U);
    Submit(1U);
    held_engine = states[1]->dma->rx.control;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_cancel(ports[1], &requests[1]));
    now = 20000U;
    ReleasedWire(1U);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&requests[1].base));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[0].base));
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_port_stop(ports[1]));
    EXPECT_EQ(1U, *held_engine & 1U);
    EXPECT_EQ(controller_gate, GATE & controller_gate);
    held_engine = nullptr;
    *states[1]->dma->rx.control = 0U;
    ReleasedWire(1U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
}

#endif

TEST_F(I2CPairFactory, RefusedInterruptWithdrawalRetainsPlatformClockAndPins) {
    Start();
    const uint32_t mode = PIN_MODE;
    refused_irq = static_cast<int>(event_irqs[1]);
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_TRUE(enabled(event_irqs[1]));
    EXPECT_EQ(mode, PIN_MODE);
    EXPECT_EQ(controller_gate, GATE & controller_gate);
    refused_irq = -1;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}

TEST_F(I2CPairFactory, IsrAndMaskedColdStartRejectBeforeAnyLease) {
    g_gd32_model_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_start().primary);
    g_gd32_model_isr = false;
    g_gd32_model_mask = 1U;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_platform_start().primary);
    g_gd32_model_mask = 0U;
    EXPECT_EQ(0U, GATE);
    EXPECT_EQ(0U, writes);
    EXPECT_CALL(clocks, Start()).Times(0);
}

#ifndef NX_I2C_PAIR_IRQ
TEST_F(I2CPairFactory, ExpiredAndInvisibleBufferAdmissionHasNoPeerEffects) {
    Start();
    const auto primary = registers(states[0]);
    const auto secondary = registers(states[1]);
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_request_prepare(&requests[0], &messages[0], 1U, now));
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_i2c_endpoint_submit(endpoints[0], &requests[0]));
    uint8_t invisible[3]{};
    messages[1].data = invisible;
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_request_prepare(&requests[1], &messages[1], 1U, 1000U));
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_i2c_endpoint_submit(endpoints[1], &requests[1]));
    EXPECT_EQ(primary, registers(states[0]));
    EXPECT_EQ(secondary, registers(states[1]));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&requests[1].base));
}

#endif

TEST_F(I2CPairFactory, PeripheralNackCannotSettleOrResetPeerController) {
    Start();
    Submit(0U);
    Submit(1U);
    const auto peer = registers(states[0]);
    states[1]->registers[5] = 1U << 10U;
    error_vectors[1]();
    EXPECT_EQ(peer, registers(states[0]));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[0].base));
    ReleasedWire(1U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    EXPECT_EQ(NX_ERROR_NACK, requests[1].base.result);
}

TEST_F(I2CPairFactory, StopRestoresSelectedPinsAndLeavesUnrelatedFields) {
    PIN_MODE = 3U;
    PIN_TYPE = 1U;
    PIN_AF_LOW = 0xFU;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(3U, PIN_MODE);
    EXPECT_EQ(1U, PIN_TYPE);
    EXPECT_EQ(0xFU, PIN_AF_LOW);
    EXPECT_EQ(0U, GATE & controller_gate);
    EXPECT_FALSE(enabled(event_irqs[0]));
    EXPECT_FALSE(enabled(event_irqs[1]));
    for (unsigned i = 0U; i < 2U; ++i) {
        EXPECT_TRUE(nx_i2c_endpoint_on_port(endpoints[i], ports[i]));
        EXPECT_FALSE(nx_i2c_endpoint_on_port(endpoints[i], ports[1U - i]));
    }
    Start();
    EXPECT_TRUE(states[0]->initialized);
    EXPECT_TRUE(states[1]->initialized);
}
#ifdef NX_I2C_PAIR_IRQ
TEST_F(I2CPairFactory, IrqThreeByteWindowCompletesOnlyAfterWireRelease) {
    Start();
    Submit(0U);
    Submit(1U);
    Complete(1U);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[0].base));
    ReleasedWire(1U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[1].base));
    EXPECT_EQ(3U, requests[1].base.transferred);
    EXPECT_EQ(0x71U, messages[1].data[0]);
    EXPECT_EQ(0x72U, messages[1].data[1]);
    EXPECT_EQ(0x72U, messages[1].data[2]);
}

TEST_F(I2CPairFactory, IrqModeCanUseOrdinaryBufferWithoutCreatingDmaOwnership) {
    Start();
    uint8_t payload[3]{};
    messages[0].data = payload;
    ASSERT_EQ(NX_SUCCESS,
              nx_i2c_request_prepare(&requests[0], &messages[0], 1U, 1000U));
    EXPECT_EQ(NX_SUCCESS, nx_i2c_endpoint_submit(endpoints[0], &requests[0]));
    EXPECT_EQ(nullptr, states[0]->dma);
    EXPECT_FALSE(enabled(rx_irqs[0]));
    EXPECT_FALSE(enabled(tx_irqs[0]));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&requests[0].base));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_cancel(ports[0], &requests[0]));
    ReleasedWire(0U);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&requests[0].base));
}
#endif

} /* namespace */

/** \brief External hardware clock, not the I2C deadline implementation. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
/** \brief Inject configurable handler context without replacing lifecycle. */
extern "C" bool nx_i2c_pair_isr(void) {
    return g_gd32_model_isr;
}
/** \brief Inject the incoming admission mask only. */
extern "C" bool nx_i2c_pair_masked(void) {
    return g_gd32_model_mask != 0U;
}
/** \brief Apply the hardware ISER set operation to one external interrupt. */
extern "C" void nx_i2c_pair_irq_enable(IRQn_Type irq) {
    const unsigned value = static_cast<unsigned>(irq);
    NVIC->ISER[value / 32U] |= 1U << (value % 32U);
}
/** \brief Refuse one withdrawal to exercise the actual retained stop path. */
extern "C" void nx_i2c_pair_irq_disable(IRQn_Type irq) {
    if (static_cast<int>(irq) != refused_irq) {
        const unsigned value = static_cast<unsigned>(irq);
        NVIC->ISER[value / 32U] &= ~(1U << (value % 32U));
    }
}
/** \brief Pending acknowledgement does not consume a peer interrupt. */
extern "C" void nx_i2c_pair_irq_clear(IRQn_Type irq) {
    const unsigned value = static_cast<unsigned>(irq);
    NVIC->ISPR[value / 32U] &= ~(1U << (value % 32U));
}
/** \brief Store the actual implemented logical priority. */
extern "C" void nx_i2c_pair_irq_priority(IRQn_Type irq, uint32_t value) {
    NVIC->IP[static_cast<unsigned>(irq)] = static_cast<uint8_t>(value << 4U);
}
/** \brief Report hardware priority facts without substituting policy. */
extern "C" uint32_t nx_i2c_pair_irq_get_priority(IRQn_Type irq) {
    return NVIC->IP[static_cast<unsigned>(irq)] >> 4U;
}
/** \brief W1C clears only bits in the selected controller flag group. */
extern "C" void nx_i2c_pair_dma_ack(volatile uint32_t* clear, uint32_t value) {
    *clear = value;
    *(clear - 2U) &= ~value;
}
/** \brief Capture/refuse the actual register leaf selected by the generator. */
extern "C" void nx_i2c_pair_register_write(volatile uint32_t* reg,
                                           uint32_t value) {
    ++writes;
    for (unsigned bank = 0U; bank < 8U; ++bank) {
        if (reg == &NVIC->ISER[bank]) {
            NVIC->ISER[bank] |= value;
            return;
        }
        if (reg == &NVIC->ICER[bank]) {
            const uint32_t retained =
                refused_irq >= 0 &&
                        static_cast<unsigned>(refused_irq) / 32U == bank
                    ? 1U << (static_cast<unsigned>(refused_irq) % 32U)
                    : 0U;
            *reg = value;
            NVIC->ISER[bank] &= ~(value & ~retained);
            return;
        }
        if (reg == &NVIC->ICPR[bank]) {
            *reg = value;
            NVIC->ISPR[bank] &= ~value;
            return;
        }
    }
    const bool blocked = reg == refused_register &&
                         (value & (1U << 22U)) != 0U &&
                         (*reg & (1U << 22U)) == 0U;
    if (!blocked)
        *reg = value;
}
/** \brief Read actual selected DATA and apply its receive-status clear. */
extern "C" uint8_t nx_i2c_v1_model_read(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[5] &= ~((1U << 6U) | (1U << 2U));
    return static_cast<uint8_t>(state->registers[4]);
}
/** \brief DATA writes consume only the selected transmit-ready status. */
extern "C" void nx_i2c_v1_model_write(void* context, uint32_t value) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[4] = value;
    state->registers[5] &= ~(1U | (1U << 2U) | (1U << 7U));
}
/** \brief Reading STAT0 then STAT1 clears the selected address event. */
extern "C" void nx_i2c_v1_model_address_cleared(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[5] &= ~2U;
}
/** \brief A refused EN clear retains the real engine and request loan. */
extern "C" void nx_i2c_v1_model_disable(volatile uint32_t* control,
                                        uint32_t bits) {
    *control &= control == held_engine ? ~(bits & ~1U) : ~bits;
}
#ifdef NX_I2C_PAIR_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
/** \brief Identity is a separately tested SoC input boundary. */
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
/** \brief Observe entry into the independently tested system-clock subsystem.
 */
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    return {clock_port->Start(), 0, 0U};
}
/** \brief Observe cleanup ordering without claiming system-clock execution. */
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return clock_port->Stop();
}
#else
/** \brief Reject an unrequested reset from the external vendor dependency. */
extern "C" void rcu_periph_reset_enable(rcu_periph_reset_enum) {
    ADD_FAILURE() << "Paired I2C startup must not reset unrelated GPIO";
}
/** \brief No selected paired-controller operation authorizes a GPIO reset. */
extern "C" void rcu_periph_reset_disable(rcu_periph_reset_enum) {
    ADD_FAILURE() << "No paired I2C GPIO reset was acquired";
}
/** \brief Observe the separately tested clock subsystem startup boundary. */
extern "C" int nx_gd32_soc_start(void) {
    return clock_port->Start();
}
/** \brief Observe cleanup ordering at the system boundary only. */
extern "C" int nx_gd32_soc_stop(void) {
    return clock_port->Stop();
}
#endif
