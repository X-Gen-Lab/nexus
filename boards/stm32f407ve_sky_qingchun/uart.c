/** Sky youth debug header: USART1 PA9(TX)/PA10(RX), AF7, 3.3 V logic.
 * The youth board does not include a USB-UART bridge or RS485 transceiver.
 * A verified, soldered header and external 3.3 V UART adapter are required.
 */
#include "stm32_uart_resource.h"
#include "stm32f4xx_hal.h"

nx_status_t stm32_uart_board_prepare(uint8_t instance) {
    if (instance != 0) return NX_ERR_NOT_SUPPORTED;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_HIGH;
    pins.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &pins);
    return NX_OK;
}
nx_status_t stm32_uart_board_release(uint8_t instance) {
    if (instance != 0) return NX_ERR_NOT_SUPPORTED;
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    __HAL_RCC_USART1_CLK_DISABLE();
    return NX_OK;
}
nx_status_t stm32_uart_board_direction(uint8_t instance, bool active) {
    if (instance != 0) return NX_ERR_NOT_SUPPORTED;
    return active ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
