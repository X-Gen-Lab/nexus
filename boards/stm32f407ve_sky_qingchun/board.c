/** Sky STM32F407VET6 youth core: PB2 LED, active high. */
#include "nexus_board.h"
#include "stm32f4xx_hal.h"

nx_status_t nx_board_prepare_safe_outputs(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_2;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pins);
    return NX_OK;
}
void HAL_MspInit(void) { (void)nx_board_prepare_safe_outputs(); }
