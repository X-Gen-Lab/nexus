/**
 * \file            stm32_spi_frames_factory_test.cpp
 * \brief           Real three-instance factory, IRQ and cold lease boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_spi_frames.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
void SPI1_IRQHandler(void);
void SPI2_IRQHandler(void);
void SPI3_IRQHandler(void);
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

class SpiSystemBoundary {
  public:
    MOCK_METHOD(void, Start, ());
    MOCK_METHOD(void, Stop, ());
};
static SpiSystemBoundary* system_boundary;
static nx_time_us_t model_now;
static volatile uint32_t* refused_register;
static uint32_t held_bits;

class SpiFramesFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    testing::NiceMock<SpiSystemBoundary> hardware;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        model_now = 100U;
        refused_register = nullptr;
        held_bits = 0U;
        system_boundary = &hardware;
    }
    void TearDown() override {
        refused_register = nullptr;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
            EXPECT_EQ(munmap(mmio, 0x80000U), 0);
        }
        system_boundary = nullptr;
    }
    const nx_spi_frames_port_t* Port(nx_spi_frames_id_t id) {
        return nx_factory_spi_frames(id);
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
        ASSERT_EQ(result.cleanup, NX_SUCCESS);
        ASSERT_EQ(result.remaining_effects, 0U);
    }
};

TEST_F(SpiFramesFactory, LookupIsPureAndAllThreeInstancesUseDistinctOwners) {
    EXPECT_CALL(hardware, Start()).Times(2);
    EXPECT_CALL(hardware, Stop()).Times(2);
    const auto* first = Port(NX_SPI_FRAMES_ID_WIRE1);
    const auto* second = Port(NX_SPI_FRAMES_ID_WIRE2);
    const auto* third = Port(NX_SPI_FRAMES_ID_WIRE3);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(first->ops, second->ops);
    EXPECT_EQ(first->ops, third->ops);
    EXPECT_NE(first->context, second->context);
    EXPECT_NE(first->context, third->context);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(Port(NX_SPI_FRAMES_ID_COUNT), nullptr);
    Start();
    ASSERT_FALSE(HasFatalFailure());
    auto* one = static_cast<nx_stm32_spi_frames_state_t*>(first->context);
    auto* two = static_cast<nx_stm32_spi_frames_state_t*>(second->context);
    auto* three = static_cast<nx_stm32_spi_frames_state_t*>(third->context);
    EXPECT_EQ(one->config->registers, SPI1);
    EXPECT_EQ(two->config->registers, SPI2);
    EXPECT_EQ(three->config->registers, SPI3);
    EXPECT_EQ(one->config->clock_hz, 84000000U);
    EXPECT_EQ(two->config->clock_hz, 42000000U);
    EXPECT_EQ(three->config->clock_hz, 42000000U);
    EXPECT_EQ(nx_factory_spi_frames_traits(NX_SPI_FRAMES_ID_WIRE1)->word_bits,
              16U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR & (RCC_APB1ENR_SPI2EN | RCC_APB1ENR_SPI3EN), 0U);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
    Start();
}

TEST_F(SpiFramesFactory,
       GeneratedIrqsDeliverCrcAndSingleWireFramesIndependently) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    uint16_t tx[2] = {0xCAFEU, 0x7654U};
    uint16_t rx[2] = {};
    uint16_t receive_only[2] = {};
    nx_spi_frames_request_t first = {}, second = {}, third = {};
    nx_request_initialize(&first.base);
    nx_request_initialize(&second.base);
    nx_request_initialize(&third.base);
    ASSERT_EQ(nx_spi_frames_prepare(Port(NX_SPI_FRAMES_ID_WIRE1), &first, tx,
                                    rx, 2U, 10000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_frames_prepare(Port(NX_SPI_FRAMES_ID_WIRE2), &second, tx,
                                    nullptr, 2U, 10000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_spi_frames_prepare(Port(NX_SPI_FRAMES_ID_WIRE3), &third,
                                    nullptr, receive_only, 2U, 10000U),
              NX_SUCCESS);
    const auto* one = Port(NX_SPI_FRAMES_ID_WIRE1);
    const auto* two = Port(NX_SPI_FRAMES_ID_WIRE2);
    const auto* three = Port(NX_SPI_FRAMES_ID_WIRE3);
    ASSERT_EQ(nx_spi_frames_submit(one, &first), NX_SUCCESS);
    ASSERT_EQ(nx_spi_frames_submit(two, &second), NX_SUCCESS);
    ASSERT_EQ(nx_spi_frames_submit(three, &third), NX_SUCCESS);
    for (unsigned index = 0U; index < 3U; ++index) {
        SPI1->DR = 0xABC0U + index;
        SPI1->SR = SPI_SR_RXNE | SPI_SR_TXE;
        SPI1_IRQHandler();
        SPI3->DR = 0xDEF0U + index;
        SPI3->SR = SPI_SR_RXNE | SPI_SR_BSY;
        SPI3_IRQHandler();
        if (index < 2U) {
            SPI2->SR = SPI_SR_TXE;
            SPI2_IRQHandler();
        }
    }
    nx_spi_frames_service(one);
    nx_spi_frames_service(two);
    nx_spi_frames_service(three);
    EXPECT_EQ(first.base.result, NX_SUCCESS);
    EXPECT_EQ(second.base.result, NX_SUCCESS);
    EXPECT_EQ(third.base.result, NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&third.base), NX_REQUEST_SETTLED);
    EXPECT_THAT(rx, testing::ElementsAre(0xABC0U, 0xABC1U));
    EXPECT_THAT(receive_only, testing::ElementsAre(0xDEF0U, 0xDEF1U));
    EXPECT_EQ(first.base.transferred, 2U);
    EXPECT_EQ(second.base.transferred, 2U);
    EXPECT_EQ(third.base.transferred, 2U);
}

TEST_F(SpiFramesFactory, PlatformStopRetainsAnActiveRequestUntilOwnerService) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    uint16_t tx[2] = {0x1234U, 0x5678U};
    uint16_t rx[2] = {};
    nx_spi_frames_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_spi_frames_prepare(Port(NX_SPI_FRAMES_ID_WIRE1), &request, tx,
                                    rx, 2U, 10000U),
              NX_SUCCESS);
    const auto* port = Port(NX_SPI_FRAMES_ID_WIRE1);
    ASSERT_EQ(nx_spi_frames_submit(port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SPI1EN, 0U);
    SPI1->SR = SPI_SR_TXE;
    nx_spi_frames_service(port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(SpiFramesFactory, ForeignVectorRejectsBeforeAnyColdPadOrClockEffect) {
    EXPECT_CALL(hardware, Start()).Times(0);
    NVIC_EnableIRQ(SPI2_IRQn);
    GPIOA->MODER = 0xA5A5A5A5U;
    auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(GPIOA->MODER, 0xA5A5A5A5U);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(SPI2_IRQn), 1U);
    NVIC_DisableIRQ(SPI2_IRQn);
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
    nx_arch_irq_state_t previous = {g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++model_now;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    system_boundary->Start();
    g_nx_stm32_system.core_hz = 168000000U;
    g_nx_stm32_system.started = true;
    ++g_nx_stm32_system.time_epoch;
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = 0x07405408U;
    RCC->CFGR = 0x0000940AU;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    system_boundary->Stop();
    g_nx_stm32_system.started = false;
    return 0;
}
extern "C" void nx_spi_frames_model_write(volatile uint32_t* address,
                                          uint32_t value) {
    *address = address == refused_register ? value | held_bits : value;
    for (unsigned bank = 0U; bank < 9U; ++bank) {
        auto* gpio =
            reinterpret_cast<GPIO_TypeDef*>(GPIOA_BASE + bank * 0x400U);
        if (address == &gpio->BSRR)
            gpio->ODR = (gpio->ODR | (value & 0xFFFFU)) & ~(value >> 16U);
    }
}
extern "C" void nx_spi_frames_model_tx(uintptr_t raw, uint16_t value,
                                       uint8_t bits) {
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    registers->DR = bits == 8U ? value & 0xFFU : value;
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_TXE);
}
extern "C" uint16_t nx_spi_frames_model_rx(uintptr_t raw, uint8_t bits) {
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    uint16_t value = static_cast<uint16_t>(registers->DR);
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_RXNE);
    return bits == 8U ? static_cast<uint16_t>(value & 0xFFU) : value;
}
extern "C" void nx_spi_frames_model_reset(uintptr_t raw) {
    auto* registers = reinterpret_cast<SPI_TypeDef*>(raw);
    std::memset(registers, 0, sizeof(*registers));
    registers->SR = SPI_SR_TXE;
    registers->CRCPR = 7U;
}
extern "C" uintptr_t nx_spi_frames_model_gpio(unsigned bank) {
    return GPIOA_BASE + bank * 0x400U;
}
extern "C" uintptr_t nx_spi_frames_model_instance(int irq) {
    return reinterpret_cast<uintptr_t>(irq == SPI1_IRQn   ? SPI1
                                       : irq == SPI2_IRQn ? SPI2
                                                          : SPI3);
}
extern "C" uintptr_t nx_spi_frames_model_rcc(void) {
    return reinterpret_cast<uintptr_t>(RCC);
}
extern "C" uint32_t nx_spi_frames_model_irq(unsigned command, int irq,
                                            uint32_t value) {
    auto vector = static_cast<IRQn_Type>(irq);
    switch (command) {
        case 0U:
            NVIC_EnableIRQ(vector);
            return 0U;
        case 1U:
            NVIC_DisableIRQ(vector);
            return 0U;
        case 2U:
            NVIC_ClearPendingIRQ(vector);
            return 0U;
        case 3U:
            NVIC_SetPriority(vector, value);
            return 0U;
        case 4U:
            return NVIC_GetEnableIRQ(vector);
        case 5U:
            return NVIC_GetPendingIRQ(vector);
        case 6U:
            return NVIC_GetActive(vector);
        case 7U:
            return NVIC_GetPriority(vector);
        default:
            return 0U;
    }
}
