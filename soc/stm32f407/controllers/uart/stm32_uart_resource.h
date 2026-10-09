/** Board resource port; no mutable controller state or SDK types. */
#ifndef STM32_UART_RESOURCE_H
#define STM32_UART_RESOURCE_H
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>
nx_status_t stm32_uart_board_prepare(uint8_t logical_instance);
nx_status_t stm32_uart_board_release(uint8_t logical_instance);
/** No DE line is invented: unsupported boards reject active=true. */
nx_status_t stm32_uart_board_direction(uint8_t logical_instance, bool active);
uint64_t stm32_uart_board_timestamp_us(void);
uint32_t stm32_uart_board_timestamp_resolution_us(void);
#endif
