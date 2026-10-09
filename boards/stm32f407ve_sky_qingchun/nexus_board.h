#ifndef NEXUS_STM32F407VE_SKY_QINGCHUN_BOARD_H
#define NEXUS_STM32F407VE_SKY_QINGCHUN_BOARD_H
#include "hal/nx_status.h"

#define NX_BOARD_NAME "stm32f407ve-sky-qingchun"
#define NX_BOARD_HSE_HZ 8000000U
#define NX_BOARD_LED_GPIO_PORT 'B'
#define NX_BOARD_LED_GPIO_PIN 2U
#define NX_BOARD_LED_ACTIVE_LEVEL 1U
#define NX_BOARD_LED_INACTIVE_LEVEL 0U
#define NX_BOARD_CONSOLE_UART_INSTANCE 0U

#ifdef __cplusplus
extern "C" {
#endif
/** Establish the LED inactive latch before output mode. No industrial
 * actuator or absent SPI Flash/LSE is enabled by this board profile.
 */
nx_status_t nx_board_prepare_safe_outputs(void);
#ifdef __cplusplus
}
#endif
#endif
