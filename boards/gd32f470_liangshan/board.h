#ifndef NEXUS_GD32F470_LIANGSHAN_BOARD_H
#define NEXUS_GD32F470_LIANGSHAN_BOARD_H
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* JLC Liangshan Pi GD32F470ZGT6 first-party documented wiring:
 * LED2 PD7 active high; USART0 PA9 TX/PA10 RX AF7 (DAPLink serial);
 * onboard W25Q64 SPI4 PF7 SCK/PF8 MISO/PF9 MOSI AF5, CS logical 0 PF6;
 * 25 MHz crystal; 3.3 V. PCB revision must be supplied by the fixture.
 * RS485 is NOT onboard: optional PB1 active-high DE requires explicit product
 * wiring and GD32_RS485_ENABLE, disabled in the stock board profile.
 */
nx_status_t nx_gd32_board_safe_init(void);
nx_status_t nx_gd32_board_uart_pins(bool enable);
void nx_gd32_board_rs485_de(bool active);
nx_status_t nx_gd32_board_spi_pins(bool enable);
nx_status_t nx_gd32_board_spi_cs(uint8_t logical, bool active);
#ifdef __cplusplus
}
#endif
#endif
