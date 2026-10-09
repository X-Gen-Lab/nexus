/** MB997 UART fixture resource: USART2 PA2(TX)/PA3(RX), AF7. No onboard
 * RS485 transceiver/DE is assumed. USART1 alternatives conflict with USB VBUS
 * or codec wiring and require a separate explicit product resource profile. */
#include "stm32_uart_resource.h"
#include "stm32f4xx_hal.h"

nx_status_t stm32_uart_board_prepare(uint8_t logical_instance) {
    if (logical_instance != 1) return NX_ERR_NOT_SUPPORTED;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitTypeDef config = {0};
    config.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    config.Mode = GPIO_MODE_AF_PP;
    config.Pull = GPIO_PULLUP;
    config.Speed = GPIO_SPEED_FREQ_HIGH;
    config.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &config);
    return NX_OK;
}
nx_status_t stm32_uart_board_release(uint8_t logical_instance) {
    if (logical_instance != 1) return NX_ERR_NOT_SUPPORTED;
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
    __HAL_RCC_USART2_CLK_DISABLE();
    return NX_OK;
}
nx_status_t stm32_uart_board_direction(uint8_t logical_instance, bool active) {
    if (logical_instance != 1) return NX_ERR_NOT_SUPPORTED;
    return active ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
