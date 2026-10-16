/**
 * \file            legacy_transport_test.cpp
 * \brief           Actual exact-part transport admission and rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#if defined(NX_LEGACY_F103)
#include "stm32f103_transport.h"
extern "C" {
nx_stm32f103_system_t g_nx_stm32f103_system = {};
}
using UartState = nx_stm32f103_uart_dma_state_t;
using DuplexState = nx_stm32f103_uart_duplex_state_t;
using I2cState = nx_stm32f103_i2c_async_state_t;
static constexpr int kTxIrq = DMA1_Channel7_IRQn;
static constexpr int kRxIrq = DMA1_Channel6_IRQn;
#else
#include "gd32f303_transport.h"
#include "gd32f30x_i2c.h"
extern "C" {
nx_gd32f303_system_t g_nx_gd32f303_system = {};
}
using UartState = nx_gd32f303_uart_dma_state_t;
using DuplexState = nx_gd32f303_uart_duplex_state_t;
using I2cState = nx_gd32f303_i2c_async_state_t;
static constexpr int kTxIrq = DMA0_Channel3_IRQn;
static constexpr int kRxIrq = DMA0_Channel4_IRQn;
#endif
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static bool s_irqs[60];
static uint8_t s_priorities[60];
static bool s_hold_irq_off;
static unsigned s_writes;
static volatile uint32_t* s_refused;
static volatile uint32_t* mmio(uintptr_t address) {
    return reinterpret_cast<volatile uint32_t*>(address);
}
extern "C" uint32_t nx_legacy_model_read(const volatile uint32_t* reg) {
    uint32_t value = *reg;
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    if (address == 0x40004404U || address == 0x40013804U)
        *mmio(address - 4U) &= ~UINT32_C(0x3f);
    return value;
}
extern "C" void nx_legacy_model_write(volatile uint32_t* reg, uint32_t value) {
    s_writes++;
    if (reg == s_refused)
        return;
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    if (address == 0x40010c10U) {
        *mmio(0x40010c0cU) =
            (*mmio(0x40010c0cU) | (value & 0xffffU)) & ~(value >> 16U);
    } else if (address == 0x40020004U)
        *mmio(0x40020000U) &= ~value;
    else if (address == 0x40004400U || address == 0x40013800U)
        *reg &= value;
    else
        *reg = value;
}
extern "C" nx_result_t nx_legacy_model_irq(int irq, bool enable,
                                           uint8_t priority) {
    if (irq < 0 || irq >= 60)
        return NX_ERROR_INVALID;
    if (!enable && s_hold_irq_off)
        return NX_ERROR_IO;
    s_irqs[irq] = enable;
    if (enable)
        s_priorities[irq] = priority;
    return NX_SUCCESS;
}
extern "C" bool nx_legacy_model_irq_available(int irq) {
    return irq >= 0 && irq < 60 && !s_irqs[irq];
}
extern "C" uint32_t nx_legacy_model_priority(int irq) {
    return s_priorities[irq];
}
class LegacyTransport : public testing::Test {
  protected:
    void* mapping = nullptr;
    UartState uart = {};
    I2cState i2c = {};
    uint8_t ring[8] = {}, payload[4] = {};
    nx_dma_memory_region_t region = {};
    nx_uart_port_t face = {&nx_uart_v1_channel_dma_tx_ops, &uart};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(s_irqs, 0, sizeof(s_irqs));
        std::memset(s_priorities, 0, sizeof(s_priorities));
        s_writes = 0U;
        s_hold_irq_off = false;
        s_refused = nullptr;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ};
        uart.regions = &region;
        uart.region_count = 1U;
        uart.rx_storage = ring;
        uart.rx_capacity = sizeof(ring);
        uart.profile = NX_UART_RX_BYTES;
        uart.baud = 115200U;
        uart.flags = mmio(0x40020000U);
        uart.clear = mmio(0x40020004U);
#if defined(NX_LEGACY_F103)
        g_nx_stm32f103_system = {};
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;
        g_nx_stm32f103_system.rcc = RCC;
        uart.irq_policy = {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U};
        uart.registers = reinterpret_cast<nx_uart_v1_registers_t*>(USART2);
        uart.tx = reinterpret_cast<nx_uart_v1_channel_t*>(DMA1_Channel7);
        uart.tx_shift = 24U;
        uart.rx_shift = 20U;
        RCC->AHBENR = RCC_AHBENR_DMA1EN;
        RCC->APB1ENR = RCC_APB1ENR_USART2EN;
        i2c.core.registers = reinterpret_cast<volatile uint32_t*>(I2C1);
        i2c.core.line_inputs = &GPIOB->IDR;
        GPIOB->IDR = 192U;
#else
        g_nx_gd32f303_system = {};
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
        uart.irq_policy = {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U};
        uart.registers = reinterpret_cast<nx_uart_v1_registers_t*>(USART0);
        uart.tx = reinterpret_cast<nx_uart_v1_channel_t*>(
            reinterpret_cast<uintptr_t>(&DMA_CH3CTL(DMA0)));
        uart.tx_shift = 12U;
        uart.rx_shift = 16U;
        RCU_AHBEN = RCU_AHBEN_DMA0EN;
        RCU_APB2EN = RCU_APB2EN_USART0EN;
        i2c.core.registers = reinterpret_cast<volatile uint32_t*>(I2C0);
        i2c.core.line_inputs = &GPIO_ISTAT(GPIOB);
        GPIO_ISTAT(GPIOB) = 192U;
#endif
        i2c.core.line_mask = 192U;
        i2c.core.peripheral_mhz = 8U;
        i2c.irq_policy = uart.irq_policy;
    }
    void TearDown() override {
        s_hold_irq_off = false;
        s_refused = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
    nx_result_t UartStart() {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_uart_dma_initialize(&uart);
#else
        return nx_gd32f303_uart_dma_initialize(&uart);
#endif
    }
    nx_result_t I2cStart() {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_i2c_async_initialize(&i2c);
#else
        return nx_gd32f303_i2c_async_initialize(&i2c);
#endif
    }
    nx_result_t DuplexStart(DuplexState* state) {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_uart_duplex_initialize(state);
#else
        return nx_gd32f303_uart_duplex_initialize(state);
#endif
    }
    void ConfigureDuplex(DuplexState* state) {
        state->uart = uart;
        state->uart.profile = NX_UART_RX_BLOCKS;
        state->uart.receive = &state->receive;
        state->uart.rx_capacity = 0U;
#if defined(NX_LEGACY_F103)
        state->receive.channel =
            reinterpret_cast<nx_uart_v1_channel_t*>(DMA1_Channel6);
#else
        state->receive.channel = reinterpret_cast<nx_uart_v1_channel_t*>(
            reinterpret_cast<uintptr_t>(&DMA_CH4CTL(DMA0)));
#endif
    }
    nx_result_t I2cStop() {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_i2c_async_stop(&i2c);
#else
        return nx_gd32f303_i2c_async_stop(&i2c);
#endif
    }
};
TEST_F(LegacyTransport, TxOnlyDoesNotAcquireReceiveChannelOrVector) {
    ASSERT_EQ(UartStart(), NX_SUCCESS);
    EXPECT_TRUE(s_irqs[37] || s_irqs[38]);
    EXPECT_TRUE(s_irqs[kTxIrq]);
    EXPECT_FALSE(s_irqs[kRxIrq]);
    EXPECT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
}
TEST_F(LegacyTransport, ForeignFixedChannelRejectedWithoutEffects) {
    uart.tx = nullptr;
    unsigned before = s_writes;
    EXPECT_EQ(UartStart(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(uart.retained);
}
TEST_F(LegacyTransport, DuplexOwnsItsEmbeddedExtensionAndBothChannels) {
    DuplexState state = {};
    ConfigureDuplex(&state);
    ASSERT_EQ(DuplexStart(&state), NX_SUCCESS);
    EXPECT_TRUE(s_irqs[kTxIrq]);
    EXPECT_TRUE(s_irqs[kRxIrq]);
    nx_uart_port_t duplex = {&nx_uart_v1_channel_dma_ops, &state.uart};
    EXPECT_EQ(nx_uart_port_stop(&duplex), NX_SUCCESS);
    EXPECT_FALSE(s_irqs[kTxIrq]);
    EXPECT_FALSE(s_irqs[kRxIrq]);
}
TEST_F(LegacyTransport, DuplexRejectsForeignExtensionBeforeMmio) {
    DuplexState state = {}, other = {};
    ConfigureDuplex(&state);
    state.uart.receive = &other.receive;
    unsigned before = s_writes;
    EXPECT_EQ(DuplexStart(&state), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(state.uart.retained);
}
TEST_F(LegacyTransport, I2cOwnsOnlyPb6Pb7AndRestoresAddedGates) {
    *mmio(0x40010c00U) = UINT32_C(0x44000002);
    *mmio(0x40010c0cU) = 1U;
    uint32_t before_gates = *mmio(0x40021018U);
    ASSERT_EQ(I2cStart(), NX_SUCCESS);
    EXPECT_EQ(*mmio(0x40010c00U), UINT32_C(0xff000002));
    EXPECT_EQ(i2c.core.registers[7], 50U);
    EXPECT_EQ(s_priorities[31], 0U);
    EXPECT_EQ(s_priorities[32], 0U);
    EXPECT_TRUE(s_irqs[31]);
    EXPECT_TRUE(s_irqs[32]);
    EXPECT_EQ(I2cStop(), NX_SUCCESS);
    EXPECT_EQ(*mmio(0x40010c00U), UINT32_C(0x44000002));
    EXPECT_EQ(*mmio(0x40010c0cU), 1U);
    EXPECT_EQ(*mmio(0x40021018U), before_gates);
}
TEST_F(LegacyTransport, I2cFailedVectorDetachRetainsPinsAndCore) {
    ASSERT_EQ(I2cStart(), NX_SUCCESS);
    s_hold_irq_off = true;
    EXPECT_EQ(I2cStop(), NX_ERROR_IO);
    EXPECT_TRUE(i2c.retained);
    EXPECT_TRUE(i2c.core.initialized);
    EXPECT_EQ(*mmio(0x40010c00U) & UINT32_C(0xff000000), UINT32_C(0xff000000));
    s_hold_irq_off = false;
    EXPECT_EQ(I2cStop(), NX_SUCCESS);
}
TEST_F(LegacyTransport, DrivenPb6RejectsBeforeAnyGateWrite) {
    *mmio(0x40021018U) |= 1U << 3U;
    *mmio(0x40010c00U) = 2U << 24U;
    unsigned before = s_writes;
    EXPECT_EQ(I2cStart(), NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(i2c.retained);
}
