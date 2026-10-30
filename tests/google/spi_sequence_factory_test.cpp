/**
 * \file            spi_sequence_factory_test.cpp
 * \brief           Exact physical factory sequence admission before effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "i2c_v1_async.h"
#include "nexus/arch/arch.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "spi_dma_sequence.h"
#ifdef NX_MULTI_FACTORY_STM32
#include "stm32f407_dma.h"
#define GPIO_GATE    RCC->AHB1ENR
#define SPI_GATE     RCC->APB2ENR
#define SPI_GATE_BIT RCC_APB2ENR_SPI1EN
#else
#include "gd32f470_spi_dma.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#define GPIO_GATE    RCU_AHB1EN
#define SPI_GATE     RCU_APB2EN
#define SPI_GATE_BIT RCU_APB2EN_SPI4EN
#endif
NVIC_Type g_gd32_model_nvic;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
unsigned writes;
unsigned starts;
bool refuse_spi_gate;
nx_time_us_t now;

volatile uint32_t& gpio(unsigned port, unsigned offset) {
    return *reinterpret_cast<volatile uint32_t*>(UINT32_C(0x40020000) +
                                                 port * 0x400U + offset);
}
/* Vendor GPIO leaves may write the command register directly. Observe that
 * hardware latch effect at the external pin capture/restore boundary. */
void observe_gpio_commands(void) {
    for (unsigned port = 0U; port < 9U; ++port) {
        const uint32_t command = gpio(port, 24U);
        gpio(port, 20U) =
            (gpio(port, 20U) | (command & 0xffffU)) & ~(command >> 16U);
        gpio(port, 24U) = 0U;
    }
}
} /* namespace */

/** \brief Apply only external write-only GPIO and NVIC command semantics. */
extern "C" void nx_multi_factory_write(volatile uint32_t* address,
                                       uint32_t value) {
    ++writes;
    if (refuse_spi_gate && address == &SPI_GATE) {
        value = (value & ~static_cast<uint32_t>(SPI_GATE_BIT)) |
                (*address & SPI_GATE_BIT);
    }
    for (unsigned bank = 0U; bank < 8U; ++bank) {
        if (address == &NVIC->ISER[bank]) {
            *address |= value;
            return;
        }
        if (address == &NVIC->ICER[bank]) {
            NVIC->ISER[bank] &= ~value;
            *address = value;
            return;
        }
        if (address == &NVIC->ICPR[bank]) {
            NVIC->ISPR[bank] &= ~value;
            *address = value;
            return;
        }
    }
    uintptr_t location = reinterpret_cast<uintptr_t>(address);
    if (location >= UINT32_C(0x40020000) && location < UINT32_C(0x40022400) &&
        (location - UINT32_C(0x40020000)) % 0x400U == 24U) {
        unsigned port =
            static_cast<unsigned>((location - UINT32_C(0x40020000)) / 0x400U);
        gpio(port, 20U) =
            (gpio(port, 20U) | (value & 0xffffU)) & ~(value >> 16U);
    }
    *address = value;
}
/** \brief Preserve byte-wide priority storage and observed publisher facts. */
extern "C" void nx_multi_factory_priority_write(volatile uint8_t* address,
                                                uint8_t value) {
    *address = value;
}
extern "C" bool nx_multi_factory_is_privileged(void) {
    return true;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint64_t nx_gd32_now_us(void) {
    return now;
}
extern "C" void nx_multi_factory_irq_enable(int irq) {
    nx_multi_factory_write(&NVIC->ISER[static_cast<unsigned>(irq) / 32U],
                           1U << (static_cast<unsigned>(irq) % 32U));
}
extern "C" void nx_multi_factory_irq_disable(int irq) {
    nx_multi_factory_write(&NVIC->ICER[static_cast<unsigned>(irq) / 32U],
                           1U << (static_cast<unsigned>(irq) % 32U));
}
extern "C" void nx_multi_factory_irq_pending_clear(int irq) {
    nx_multi_factory_write(&NVIC->ICPR[static_cast<unsigned>(irq) / 32U],
                           1U << (static_cast<unsigned>(irq) % 32U));
}
extern "C" void nx_multi_factory_irq_priority(int irq, uint32_t priority) {
    nx_multi_factory_priority_write(&NVIC->IPR[static_cast<unsigned>(irq)],
                                    static_cast<uint8_t>(priority << 4U));
}
extern "C" uint32_t nx_multi_factory_irq_get_priority(int irq) {
    return NVIC->IPR[static_cast<unsigned>(irq)] >> 4U;
}
/** \brief Emulate W1C and selected engine withdrawal without provider policy.
 */
extern "C" void nx_multi_factory_dma_ack(volatile uint32_t* clear,
                                         uint32_t value) {
    *clear = value;
    *(clear - 2) &= ~value;
}
extern "C" void nx_multi_factory_dma_disable(volatile uint32_t* control,
                                             uint32_t mask) {
    *control &= ~mask;
}
extern "C" void nx_multi_factory_spi_reset(volatile uint32_t* reg,
                                           uint32_t mask, bool active) {
    *reg = active ? *reg | mask : *reg & ~mask;
#ifdef NX_MULTI_FACTORY_STM32
    if (active) {
        SPI_TypeDef* spi = reg == &RCC->APB2RSTR ? SPI1 : SPI2;
        spi->CR1 = spi->CR2 = 0U;
        spi->SR = SPI_SR_TXE;
    }
#endif
}
/** \brief Model only the I2C DATA/address command side effects. */
extern "C" uint8_t nx_multi_factory_i2c_read(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    const auto value = static_cast<uint8_t>(state->registers[4]);
    state->registers[5] &= ~(1U << 6U);
    return value;
}
extern "C" void nx_multi_factory_i2c_write(void* context, uint32_t value) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[4] = value;
    state->registers[5] &= ~((1U << 7U) | (1U << 2U));
}
extern "C" void nx_multi_factory_i2c_address(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[5] &= ~(1U << 1U);
}
#ifdef NX_MULTI_FACTORY_STM32
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
/* The hardware model supplies the same fixed HCLK observed by real RCC C. */
uint32_t SystemCoreClock = 168000000U;
const uint8_t AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                   1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

nx_result_t nx_multi_actual_gpio_clock_enable(unsigned);
uint16_t nx_multi_actual_pin_capture(const GPIO_TypeDef*, uint8_t);
void nx_multi_actual_pin_restore(GPIO_TypeDef*, uint8_t, uint16_t);
nx_result_t nx_multi_actual_pin_configure(GPIO_TypeDef*, uint8_t, uint8_t,
                                          uint8_t, uint8_t, bool);
}

/** \brief Preserve the selected EN readback through actual HAL disable. */
extern "C" void nx_multi_factory_hal_dma_disable(DMA_HandleTypeDef* handle) {
    nx_multi_factory_dma_disable(&handle->Instance->CR, DMA_SxCR_EN);
}
/** \brief Observe the UART enable write without replacing HAL initialization.
 */
extern "C" void nx_multi_factory_hal_uart_enable(UART_HandleTypeDef* handle) {
    SET_BIT(handle->Instance->CR1, USART_CR1_UE);
}
/** \brief Hardware clear-on-zero flags do not set unrelated status bits. */
extern "C" void nx_multi_factory_hal_uart_clear(UART_HandleTypeDef* handle,
                                                uint32_t flags) {
    handle->Instance->SR &= ~flags;
}
/** \brief HAL uses the same modeled monotonic epoch; no separate tick exists.
 */
extern "C" uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(nx_time_now_us() / 1000U);
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    return 0;
}
extern "C" nx_result_t nx_stm32_gpio_clock_enable(unsigned bank) {
    return nx_multi_actual_gpio_clock_enable(bank);
}
extern "C" uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* port,
                                         uint8_t pin) {
    observe_gpio_commands();
    return nx_multi_actual_pin_capture(port, pin);
}
extern "C" void nx_stm32_pin_restore(GPIO_TypeDef* port, uint8_t pin,
                                     uint16_t saved) {
    nx_multi_actual_pin_restore(port, pin, saved);
    observe_gpio_commands();
}
extern "C" nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* port, uint8_t pin,
                                              uint8_t mode, uint8_t af,
                                              uint8_t pull, bool drain) {
    return nx_multi_actual_pin_configure(port, pin, mode, af, pull, drain);
}
#else
extern "C" {
nx_result_t nx_multi_actual_gpio_clock_enable(unsigned);
uint16_t nx_multi_actual_pin_capture(uint32_t, unsigned);
void nx_multi_actual_pin_restore(uint32_t, unsigned, uint16_t);
nx_result_t nx_multi_actual_pin_configure(uint32_t, unsigned, uint8_t, uint8_t,
                                          uint8_t, bool);
}
/** \brief Model the unrelated I2C participant's external reset leaf. */
extern "C" void i2c_deinit(uint32_t peripheral) {
    ++writes;
    std::memset(reinterpret_cast<void*>(static_cast<uintptr_t>(peripheral)), 0,
                0x28U);
}
extern "C" nx_result_t nx_gd32_soc_preflight(void) {
    return NX_SUCCESS;
}
extern "C" int nx_gd32_soc_start(void) {
    ++starts;
    return 0;
}
extern "C" int nx_gd32_soc_stop(void) {
    return 0;
}
extern "C" nx_result_t nx_gd32_gpio_clock_enable(unsigned bank) {
    return nx_multi_actual_gpio_clock_enable(bank);
}
extern "C" uint16_t nx_gd32_pin_capture(uint32_t port, unsigned pin) {
    observe_gpio_commands();
    return nx_multi_actual_pin_capture(port, pin);
}
extern "C" void nx_gd32_pin_restore(uint32_t port, unsigned pin,
                                    uint16_t saved) {
    nx_multi_actual_pin_restore(port, pin, saved);
    observe_gpio_commands();
}
extern "C" nx_result_t nx_gd32_pin_configure(uint32_t port, unsigned pin,
                                             uint8_t mode, uint8_t af,
                                             uint8_t pull, bool drain) {
    return nx_multi_actual_pin_configure(port, pin, mode, af, pull, drain);
}
#endif

namespace {
class SPISequenceFactory : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    static const nx_spi_dma_transaction_port_t* Port() {
        return nx_factory_spi_dma_transaction((nx_spi_dma_transaction_id_t)0U);
    }
    static nx_spi_dma_sequence_state_t* Sequence() {
        return static_cast<nx_spi_dma_sequence_state_t*>(Port()->context);
    }
    void SetUp() override {
        ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        writes = starts = 0U;
        refuse_spi_gate = false;
        now = 100U;
        /* Model a fresh cold sequence after the preceding complete stop while
         * retaining its statically linked immutable backend association. */
        auto* sequence = Sequence();
        const auto* backend = sequence->backend;
        void* context = sequence->context;
        *sequence = {};
        sequence->backend = backend;
        sequence->context = context;
        g_gd32_model_nvic = {};
#ifdef NX_MULTI_FACTORY_STM32
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        SPI1->SR = SPI2->SR = SPI_SR_TXE;
        GPIOB->IDR = 0xffffU;
#else
        RCU_CFG0 = RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2;
        SPI_STAT(SPI4) = SPI_STAT(SPI0) = SPI_STAT_TBE;
        GPIO_ISTAT(GPIOB) = 0xffffU;
#endif
    }
    void TearDown() override {
        refuse_spi_gate = false;
        /* The test owns the simulated earlier phase; no device accepted it.
         * Complete that owner before exercising its ordinary stop retry. */
        auto* phase = &Sequence()->phase_request.base;
        if (nx_request_state(phase) != NX_REQUEST_READY &&
            nx_request_state(phase) != NX_REQUEST_SETTLED) {
            nx_request_settle(phase, NX_ERROR_CANCELLED, 0U);
            EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(phase));
        }
        EXPECT_EQ(NX_SUCCESS, nx_spi_dma_transaction_stop(Port()));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};
TEST_F(SPISequenceFactory, ColdLookupDoesNotAcquireAnyHardware) {
    ASSERT_NE(nullptr, Port());
    EXPECT_EQ(Port(),
              nx_factory_spi_dma_transaction((nx_spi_dma_transaction_id_t)0U));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, starts);
    EXPECT_EQ(0U, GPIO_GATE);
}
TEST_F(SPISequenceFactory, FailedStopResponsibilityRejectsBeforeGlobalEffects) {
    auto* sequence = Sequence();
    sequence->stopping = true;
    sequence->stopped = false;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, starts);
    EXPECT_TRUE(sequence->stopping);
    EXPECT_FALSE(sequence->stopped);
}
TEST_F(SPISequenceFactory, QueuedInnerPhaseRejectsWithoutReinitializingIt) {
    auto* sequence = Sequence();
    nx_request_initialize(&sequence->phase_request.base);
    ASSERT_EQ(NX_SUCCESS, nx_request_admit(&sequence->phase_request.base,
                                           NX_REQUEST_QUEUED));
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, starts);
    EXPECT_EQ(NX_REQUEST_QUEUED,
              nx_request_state(&sequence->phase_request.base));
}
TEST_F(SPISequenceFactory, PartialProviderPrepareStillReleasesItsColdLease) {
    refuse_spi_gate = true;
    const auto failed = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, failed.primary);
    EXPECT_EQ(NX_SUCCESS, failed.cleanup);
    EXPECT_EQ(0U, failed.remaining_effects);
    EXPECT_EQ(0U, SPI_GATE & SPI_GATE_BIT);
    EXPECT_EQ(0U, GPIO_GATE);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());

    refuse_spi_gate = false;
    const auto retried = nx_platform_start();
    EXPECT_EQ(NX_SUCCESS, retried.primary);
    EXPECT_EQ(NX_SUCCESS, retried.cleanup);
    EXPECT_EQ(0U, retried.remaining_effects);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}
TEST_F(SPISequenceFactory, ResolvedMetadataCanStartStopAndRestartSameIdentity) {
    const auto* original = Port();
    for (unsigned cycle = 0U; cycle < 2U; ++cycle) {
        const auto started = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, started.primary);
        EXPECT_EQ(NX_SUCCESS, started.cleanup);
        EXPECT_EQ(0U, started.remaining_effects);
        EXPECT_EQ(original, Port());
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
}
} /* namespace */
