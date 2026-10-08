/** Read-only configuration/identity display for the MB997 reference board.
 * This application does not erase/program Flash or test durable storage. */
#include "boot/stm32_boot.h"
#include "config_display.h"
#include "nexus_board.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

#if NX_BOARD_LED_GPIO_PORT != 'D'
#error "The STM32 configuration display requires the MB997 board profile"
#endif

#define LED_PORT GPIOD
#define LED_PIN (1U << NX_BOARD_LED_GPIO_PIN)
#define LED_BLINK_PERIOD_MS 500U

static void led_init(void) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef config = {0};
    config.Pin = LED_PIN;
    config.Mode = GPIO_MODE_OUTPUT_PP;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &config);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

int main(void) {
    /* Share platform clock and NVIC configuration with all products. */
    if (stm32_platform_init() != 0) {
        return 1;
    }
    led_init();
    config_display_print_all();
    printf("[INFO] Board: %s; LED: P%c%u\n", NX_BOARD_NAME,
           NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN);
    printf("[INFO] Read-only display; Flash persistence is not tested\n");

    uint32_t blink_at = HAL_GetTick();
    uint32_t summary_at = blink_at;
    for (;;) {
        uint32_t now = HAL_GetTick();
        /* Elapsed subtraction handles unsigned millisecond wrap. Modulo
         * tests can repeatedly print throughout one timer tick. */
        if ((uint32_t)(now - blink_at) >= LED_BLINK_PERIOD_MS) {
            blink_at = now;
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
        if ((uint32_t)(now - summary_at) >= 5000U) {
            summary_at = now;
            config_display_print_summary();
        }
        HAL_Delay(1);
    }
}
