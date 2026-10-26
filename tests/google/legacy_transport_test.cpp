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
static int s_failed_irq;
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
    if (address == 0x40010810U || address == 0x40010c10U) {
        *mmio(address - 4U) =
            (*mmio(address - 4U) | (value & 0xffffU)) & ~(value >> 16U);
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
    if (enable && irq == s_failed_irq)
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
    nx_uart_v1_dma_config_t config = {};
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
        s_failed_irq = -1;
        s_refused = nullptr;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(payload), sizeof(payload),
                  NX_DMA_MEMORY_READ};
        config.regions = &region;
        config.region_count = 1U;
        config.rx_storage = ring;
        config.rx_capacity = sizeof(ring);
        config.profile = NX_UART_RX_BYTES;
        config.baud = 115200U;
        config.flags = mmio(0x40020000U);
        config.clear = mmio(0x40020004U);
#if defined(NX_LEGACY_F103)
        config.stop_irqs = nx_stm32f103_uart_dma_stop_irqs;
        config.validate_wake = nx_stm32f103_uart_dma_validate_wake;
        g_nx_stm32f103_system = {};
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;
        g_nx_stm32f103_system.rcc = RCC;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 43U, 4U, 0U};
        config.registers = reinterpret_cast<nx_uart_v1_registers_t*>(USART2);
        config.tx = reinterpret_cast<nx_uart_v1_channel_t*>(DMA1_Channel7);
        config.tx_shift = 24U;
        config.rx_shift = 20U;
        RCC->AHBENR = RCC_AHBENR_DMA1EN;
        RCC->APB1ENR = RCC_APB1ENR_USART2EN;
        RCC->APB2ENR = RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
        i2c.core.registers = reinterpret_cast<volatile uint32_t*>(I2C1);
        i2c.core.line_inputs = &GPIOB->IDR;
        GPIOB->IDR = 192U;
#else
        config.stop_irqs = nx_gd32f303_uart_dma_stop_irqs;
        config.validate_wake = nx_gd32f303_uart_dma_validate_wake;
        g_nx_gd32f303_system = {};
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 60U, 4U, 0U};
        config.registers = reinterpret_cast<nx_uart_v1_registers_t*>(USART0);
        config.tx = reinterpret_cast<nx_uart_v1_channel_t*>(
            reinterpret_cast<uintptr_t>(&DMA_CH3CTL(DMA0)));
        config.tx_shift = 12U;
        config.rx_shift = 16U;
        RCU_AHBEN = RCU_AHBEN_DMA0EN;
        RCU_APB2EN = RCU_APB2EN_USART0EN | RCU_APB2EN_PAEN | RCU_APB2EN_AFEN;
        i2c.core.registers = reinterpret_cast<volatile uint32_t*>(I2C0);
        i2c.core.line_inputs = &GPIO_ISTAT(GPIOB);
        GPIO_ISTAT(GPIOB) = 192U;
#endif
        i2c.core.line_mask = 192U;
        i2c.core.peripheral_mhz = 8U;
        i2c.irq_policy = config.irq_policy;
    }
    void TearDown() override {
        s_hold_irq_off = false;
        s_refused = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }
    nx_result_t UartStart(const nx_uart_v1_dma_config_t* selected = nullptr) {
        if (selected == nullptr)
            selected = &config;
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_uart_dma_initialize(&uart, selected);
#else
        return nx_gd32f303_uart_dma_initialize(&uart, selected);
#endif
    }
    nx_result_t UartPrepare(UartState* state,
                            const nx_uart_v1_dma_config_t* selected) {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_uart_dma_prepare(state, selected);
#else
        return nx_gd32f303_uart_dma_prepare(state, selected);
#endif
    }
    nx_result_t DuplexPrepare(DuplexState* state,
                              const nx_uart_v1_dma_config_t* selected) {
#if defined(NX_LEGACY_F103)
        return nx_stm32f103_uart_duplex_prepare(state, selected);
#else
        return nx_gd32f303_uart_duplex_prepare(state, selected);
#endif
    }
    volatile uint32_t* UartPinControl() {
#if defined(NX_LEGACY_F103)
        return &GPIOA->CRL;
#else
        return &GPIO_CTL1(GPIOA);
#endif
    }
    uint32_t UartPinModeMask() {
#if defined(NX_LEGACY_F103)
        return UINT32_C(0xff00);
#else
        return UINT32_C(0xff0);
#endif
    }
    uint32_t UartPinModes() {
#if defined(NX_LEGACY_F103)
        return UINT32_C(0x4a00);
#else
        return UINT32_C(0x4a0);
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
        return nx_stm32f103_uart_duplex_initialize(state, &config);
#else
        return nx_gd32f303_uart_duplex_initialize(state, &config);
#endif
    }
    void ConfigureDuplex(DuplexState* state) {
        config.profile = NX_UART_RX_BLOCKS;
        config.receive = &state->receive;
        config.rx_capacity = 0U;
#if defined(NX_LEGACY_F103)
        config.rx = reinterpret_cast<nx_uart_v1_channel_t*>(DMA1_Channel6);
#else
        config.rx = reinterpret_cast<nx_uart_v1_channel_t*>(
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
TEST_F(LegacyTransport, PrepareConfiguresExactPinsBeforeStartingTx) {
    const uint32_t preserved = UINT32_C(0x44444444) & ~UartPinModeMask();
    *UartPinControl() = preserved;
    ASSERT_EQ(UartPrepare(&uart, &config), NX_SUCCESS);
    EXPECT_EQ(*UartPinControl(), preserved | UartPinModes());
    EXPECT_EQ(uart.config, &config);
    EXPECT_TRUE(s_irqs[kTxIrq]);
    EXPECT_FALSE(s_irqs[kRxIrq]);
    EXPECT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
}
TEST_F(LegacyTransport, PreparePinFailureDoesNotBindOrEnablePeripheral) {
    s_refused = UartPinControl();
    EXPECT_EQ(UartPrepare(&uart, &config), NX_ERROR_IO);
    EXPECT_EQ(uart.config, nullptr);
    EXPECT_FALSE(uart.retained);
    EXPECT_FALSE(uart.initialized);
    EXPECT_EQ(config.registers->control1, 0U);
    EXPECT_FALSE(s_irqs[kTxIrq]);
    EXPECT_FALSE(s_irqs[kRxIrq]);
}
TEST_F(LegacyTransport, PrepareRejectsInvalidArgumentsBeforePins) {
    EXPECT_EQ(UartPrepare(nullptr, &config), NX_ERROR_INVALID);
    EXPECT_EQ(UartPrepare(&uart, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(DuplexPrepare(nullptr, &config), NX_ERROR_INVALID);
    DuplexState state = {};
    EXPECT_EQ(DuplexPrepare(&state, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(LegacyTransport, PrepareRejectsMaskedContextBeforePins) {
    DuplexState state = {};
    auto saved = nx_arch_irq_save();
    auto tx_result = UartPrepare(&uart, &config);
    auto duplex_result = DuplexPrepare(&state, &config);
    nx_arch_irq_restore(saved);
    EXPECT_EQ(tx_result, NX_ERROR_CONTEXT);
    EXPECT_EQ(duplex_result, NX_ERROR_CONTEXT);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(LegacyTransport, ForeignFixedChannelRejectedWithoutEffects) {
    config.tx = nullptr;
    unsigned before = s_writes;
    EXPECT_EQ(UartStart(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(uart.retained);
}
TEST_F(LegacyTransport, RejectedRebindPreservesAcceptedRequestAndConfig) {
    ASSERT_EQ(UartStart(), NX_SUCCESS);
    nx_uart_tx_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_uart_tx_prepare(&request, payload, sizeof(payload), 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_uart_port_submit(&face, &request), NX_SUCCESS);
    nx_uart_v1_dma_config_t replacement = config;
    replacement.baud = 57600U;
    unsigned before = s_writes;
    EXPECT_EQ(UartPrepare(&uart, &replacement), NX_ERROR_BUSY);
    EXPECT_EQ(uart.config, &config);
    EXPECT_EQ(uart.active, &request);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(s_writes, before);
    EXPECT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
}
TEST_F(LegacyTransport, PublisherPreventsBindingBeforeHardwareAdmission) {
    uart.wake_publishers = 1U;
    EXPECT_EQ(UartPrepare(&uart, &config), NX_ERROR_BUSY);
    EXPECT_EQ(uart.config, nullptr);
    EXPECT_EQ(uart.wake_publishers, 1U);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(LegacyTransport, ForeignIrqWithdrawalRejectedWithoutEffects) {
    config.stop_irqs = [](void*) -> nx_result_t { return NX_SUCCESS; };
    EXPECT_EQ(UartStart(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(uart.config, nullptr);
    EXPECT_FALSE(uart.retained);
    EXPECT_EQ(s_writes, 0U);
}
TEST_F(LegacyTransport, FailedVectorEnableRetainsExactConfigUntilStopRetry) {
    s_failed_irq = kTxIrq;
    s_hold_irq_off = true;
    EXPECT_EQ(UartPrepare(&uart, &config), NX_ERROR_IO);
    ASSERT_TRUE(uart.retained);
    EXPECT_EQ(uart.config, &config);
    nx_uart_v1_dma_config_t replacement = config;
    replacement.baud = 57600U;
    unsigned before = s_writes;
    EXPECT_EQ(UartPrepare(&uart, &replacement), NX_ERROR_BUSY);
    EXPECT_EQ(uart.config, &config);
    EXPECT_EQ(s_writes, before);
    s_hold_irq_off = false;
    s_failed_irq = -1;
    EXPECT_EQ(nx_uart_port_stop(&face), NX_SUCCESS);
    EXPECT_FALSE(uart.retained);
    EXPECT_FALSE(s_irqs[kTxIrq]);
    EXPECT_FALSE(s_irqs[37]);
    EXPECT_FALSE(s_irqs[38]);
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
    config.receive = &other.receive;
    unsigned before = s_writes;
    EXPECT_EQ(DuplexStart(&state), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, before);
    EXPECT_FALSE(state.uart.retained);
}
TEST_F(LegacyTransport, DuplexPrepareConfiguresPinsBeforeBothDmaLanes) {
    DuplexState state = {};
    ConfigureDuplex(&state);
    ASSERT_EQ(DuplexPrepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(*UartPinControl() & UartPinModeMask(), UartPinModes());
    EXPECT_EQ(state.uart.config, &config);
    EXPECT_TRUE(s_irqs[kTxIrq]);
    EXPECT_TRUE(s_irqs[kRxIrq]);
    unsigned before = s_writes;
    EXPECT_EQ(DuplexPrepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    nx_uart_port_t duplex = {&nx_uart_v1_channel_dma_ops, &state.uart};
    EXPECT_EQ(nx_uart_port_stop(&duplex), NX_SUCCESS);
}
TEST_F(LegacyTransport, DuplexPrepareRejectsUnboundReceiveLoansBeforePins) {
    DuplexState state = {};
    ConfigureDuplex(&state);
    nx_stream_t stream = {};
    state.receive.stream = &stream;
    EXPECT_EQ(DuplexPrepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(state.receive.stream, &stream);
    EXPECT_EQ(state.uart.config, nullptr);
    EXPECT_EQ(s_writes, 0U);
    state.receive.stream = nullptr;
    state.receive.filling = true;
    EXPECT_EQ(DuplexPrepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_TRUE(state.receive.filling);
    EXPECT_EQ(state.uart.config, nullptr);
    EXPECT_EQ(s_writes, 0U);
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
