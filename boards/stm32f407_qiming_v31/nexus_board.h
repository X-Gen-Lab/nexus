#ifndef NEXUS_STM32F407_QIMING_V31_BOARD_H
#define NEXUS_STM32F407_QIMING_V31_BOARD_H
#include "hal/nx_status.h"

#define NX_BOARD_NAME "stm32f407zg-qiming-v31"
#define NX_BOARD_HSE_HZ 8000000U
#define NX_BOARD_LED_GPIO_PORT 'E'
#define NX_BOARD_LED_GPIO_PIN 3U
#define NX_BOARD_LED_ACTIVE_LEVEL 0U
#define NX_BOARD_LED_INACTIVE_LEVEL 1U
#define NX_BOARD_CONSOLE_UART_INSTANCE 0U

#ifdef __cplusplus
extern "C" {
#endif
/** Establish inactive LED latches before output mode; no external actuators
 * are claimed or configured. HAL_Init invokes this through HAL_MspInit.
 */
nx_status_t nx_board_prepare_safe_outputs(void);
#ifdef __cplusplus
}
#endif
#endif
