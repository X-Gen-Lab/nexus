/**
 * \file            h743_high_factory_test.cpp
 * \brief           Normal TOML high-profile assembly and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_high_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_provider.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
USART_TypeDef g_h743_factory_uart;
uint32_t g_h743_high_factory_flash_words[524288U];
void USART3_IRQHandler(void);
}
class H743HighFactory : public testing::Test {
  protected:
    uint8_t tx_data = 0x5aU;
    nx_uart_tx_request_t request{};
    void SetUp() override {
        nx_h743_model_reset();
        nx_request_initialize(&request.base);
        PWR->D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        PWR->CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        RCC->HSICFGR = 0x40000000U;
        RCC->PLLCKSELR = 0x02020200U;
        RCC->PLLCFGR = 0x01ff0000U;
        RCC->PLL1DIVR = 0x01010280U;
        SysTick->CTRL = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        g_h743_clock_ctrl_reads = 0U;
        g_h743_clock_behavior = {};
        g_h743_high_behavior = {};
        std::memset(&g_h743_factory_uart, 0, sizeof(g_h743_factory_uart));
        g_h743_model.uart = USART3;
        USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
        g_h743_model.pwm = TIM3;
        g_h743_model.spi = SPI4;
        SPI4->SR = SPI_SR_TXP | SPI_SR_TXC;
        g_h743_model.adc = ADC1;
        ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
        ADC1->CFGR = ADC_CFGR_JQDIS;
        std::fill_n(g_h743_high_factory_flash_words, 524288U, UINT32_MAX);
    }
    void TearDown() override {
        g_h743_model.ignore_write = nullptr;
        g_h743_model.hold_irq_off = false;
        g_h743_model.hold_uart_off = false;
        g_h743_clock_behavior.hold_source_restore = false;
        USART3->ISR |= USART_ISR_TC;
        nx_uart_port_service(nx_factory_uart(NX_UART_ID_CONSOLE));
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        TIM2->CNT = 100U;
    }
};
TEST_F(H743HighFactory, ColdTypedLookupDoesNotReadOrWriteHardware) {
    EXPECT_NE(nullptr, nx_factory_gpio(NX_GPIO_ID_LED));
    EXPECT_NE(nullptr, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_NE(nullptr, nx_factory_spi(NX_SPI_ID_BUS4));
    EXPECT_NE(nullptr, nx_factory_i2c(NX_I2C_ID_BUS1));
    EXPECT_NE(nullptr, nx_factory_pwm(NX_PWM_ID_WAVE3));
    EXPECT_NE(nullptr, nx_factory_adc(NX_ADC_ID_SENSOR));
    EXPECT_NE(nullptr, nx_factory_flash(NX_FLASH_ID_STORAGE));
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743HighFactory, NormalGenerationStartsDistinctDomainsAndAllProviders) {
    ASSERT_NO_FATAL_FAILURE(Start());
    const auto c = nx_h743_high_model_clocks();
    EXPECT_EQ(384000000U, c.cpu_hz);
    EXPECT_EQ(192000000U, c.hclk_hz);
    EXPECT_EQ(96000000U, c.pclk_hz[0]);
    EXPECT_EQ(192000000U, c.tim2_hz);
    EXPECT_EQ(64000000U, c.usart3_hz);
    EXPECT_EQ(64000000U, c.i2c1_hz);
    EXPECT_EQ(96000000U, c.spi4_hz);
    EXPECT_EQ(64000000U, c.adc12_input_hz);
    EXPECT_EQ(16000000U, c.adc_hz);
    EXPECT_EQ(191U, TIM2->PSC);
    EXPECT_EQ(191U, TIM3->PSC);
    EXPECT_EQ(556U, USART3->BRR);
    EXPECT_EQ(ADC_CCR_PRESC_1,
              ADC12_COMMON->CCR & (ADC_CCR_CKMODE | ADC_CCR_PRESC));
    EXPECT_NE(0U, I2C1->TIMINGR);
    EXPECT_TRUE(g_h743_model.adc_calibrated);
    uint16_t sample = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_adc_port_sample(nx_factory_adc(NX_ADC_ID_SENSOR),
                                             &sample, 1U, 10000U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0xabcdU, sample);
    EXPECT_EQ(0U, g_h743_high_behavior.mux_writes_with_consumers);
}
TEST_F(H743HighFactory, GeneratedUartIrqSettlesOnlyAfterWireTc) {
    ASSERT_NO_FATAL_FAILURE(Start());
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, &tx_data, 1U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(uart, &request));
    USART3_IRQHandler();
    EXPECT_EQ(tx_data, USART3->TDR);
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    USART3->ISR |= USART_ISR_TC;
    USART3_IRQHandler();
    nx_uart_port_service(uart);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(1U, request.base.transferred);
}
TEST_F(H743HighFactory, GeneratedSpiUsesBoundedDividerAndFullTailDrain) {
    ASSERT_NO_FATAL_FAILURE(Start());
    uint8_t value = 0x6aU;
    size_t transferred = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_spi_endpoint_transfer(nx_device_device4, &value, nullptr, 1U,
                                       10000U, &transferred));
    EXPECT_EQ(128U, g_h743_high_behavior.spi_started_divisor);
    EXPECT_EQ(1U, transferred);
    EXPECT_FALSE(g_h743_model.spi_tail_truncated);
    EXPECT_NE(0U, GPIOE->ODR & (1U << 11U));
}
TEST_F(H743HighFactory, GeneratedFlashUsesPhysicalWordPathWithoutRetuning) {
    ASSERT_NO_FATAL_FAILURE(Start());
    const auto* port = nx_factory_flash(NX_FLASH_ID_STORAGE);
    g_h743_model.physical_flash =
        static_cast<nx_stm32h743_flash_state_t*>(port->context);
    std::array<uint8_t, 32U> input{};
    input.fill(0x55U);
    /* Keep the running image's first sector intact in the physical example. */
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_erase(port, 131072U, 131072U, 10000U));
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(port, 131072U, input.data(),
                                                input.size(), 10000U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(0x55555555U, g_h743_high_factory_flash_words[32768U]);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1, FLASH->ACR);
    EXPECT_EQ(0U, g_h743_high_behavior.unsafe_flash_unlocks);
}
TEST_F(H743HighFactory, AmbiguousOrUnreviewedRevisionRejectsBeforeAnyEffects) {
    for (uint32_t revision : {0x1003U, 0x2001U}) {
        g_h743_model.silicon_id = (revision << 16U) | 0x450U;
        RCC->HSICFGR = revision == 0x1003U ? 0x00020000U : 0x40000000U;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_FALSE(g_nx_stm32h743_system.started);
    }
}
TEST_F(H743HighFactory, FailedTimerSetupRollsBackSystemThenJointClock) {
    g_h743_model.ignore_write = &TIM2->PSC;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(0U, RCC->CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(0U, RCC->D2CCIP2R);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, FLASH->ACR);
}
TEST_F(H743HighFactory, FailedSystemStopRetainsActualPllClockForRetry) {
    ASSERT_NO_FATAL_FAILURE(Start());
    g_h743_model.ignore_write = &TIM2->DIER;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, g_nx_stm32h743_system.remaining_effects);
    EXPECT_EQ(RCC_CFGR_SWS_PLL1, RCC->CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(384000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1, FLASH->ACR);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
}
TEST_F(H743HighFactory, FailedClockStopRetainsProtectionAfterSystemStops) {
    ASSERT_NO_FATAL_FAILURE(Start());
    g_h743_clock_behavior.hold_source_restore = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_platform_stop());
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_nx_stm32h743_system.remaining_effects);
    EXPECT_EQ(0U, RCC->APB1LENR & RCC_APB1LENR_TIM2EN);
    EXPECT_EQ(RCC_CFGR_SWS_PLL1, RCC->CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(384000000U, g_nx_stm32h743_system.core_hz);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1, FLASH->ACR);
    g_h743_clock_behavior.hold_source_restore = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(64000000U, g_nx_stm32h743_system.core_hz);
}
TEST_F(H743HighFactory, StopStartReusesTheSameStaticTypedFaces) {
    const auto* uart = nx_factory_uart(NX_UART_ID_CONSOLE);
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(uart, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, FLASH->ACR);
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(uart, nx_factory_uart(NX_UART_ID_CONSOLE));
    EXPECT_EQ(191U, TIM2->PSC);
    EXPECT_EQ(556U, USART3->BRR);
}
