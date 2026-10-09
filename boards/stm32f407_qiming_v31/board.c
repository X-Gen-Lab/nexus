/** Qiming high-spec V3.1: LED0=PE3, LED1=PE4, LED2=PG9, active low.
 * Source: user-supplied V3.1 manual and its matching LED example.
 * External actuator, LCD, buzzer and communication direction pins are untouched.
 */
#include "nexus_board.h"
#include "stm32f4xx_hal.h"

nx_status_t nx_board_prepare_safe_outputs(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_9, GPIO_PIN_SET);
    GPIO_InitTypeDef pins = {0};
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    pins.Pin = GPIO_PIN_3 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOE, &pins);
    pins.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOG, &pins);
    return NX_OK;
}

/* Strong board callback retained by platform object forwarding. */
void HAL_MspInit(void) { (void)nx_board_prepare_safe_outputs(); }
