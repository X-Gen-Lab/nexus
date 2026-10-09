/** Private SoC/Board binding port. The board never receives controller jobs,
 * owner tokens, locks, phases or mutable runtime configuration. Vendor handle
 * binding is implementation-only; it is not a public product interface. */
#ifndef STM32_SPI_RESOURCE_H
#define STM32_SPI_RESOURCE_H
#include "hal/nx_status.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    SPI_HandleTypeDef* handle;
    uint8_t instance;
    bool dma_tx_enabled, dma_rx_enabled;
} stm32_spi_board_port_t;
/** The request has call duration lifetime and must not be retained. Board
 * storage/IRQ resources must be static; the linked handle belongs to SoC. */
nx_status_t stm32_spi_board_prepare(const stm32_spi_board_port_t* request);
/** NOT_SUPPORTED from prepare is a side-effect-free wiring rejection. Other
 * failures may retain partial resources. Release is idempotent, clears only
 * successfully settled ownership and preserves remaining resources on error. */
nx_status_t stm32_spi_board_release(const stm32_spi_board_port_t* request);
nx_status_t stm32_spi_board_select(uint8_t instance, uint8_t chip_select, bool active);
uint32_t stm32_spi_board_clock_hz(uint8_t instance);
#endif
