/**
 * \file            h743_spi_byte_port.c
 * \brief           Execute locked H743 LL byte accesses from host C
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_byte_model.h"
#include "stm32h7xx_ll_spi.h"
void nx_h743_spi_official_byte_write(SPI_TypeDef* registers, uint8_t value) {
    LL_SPI_TransmitData8(registers, value);
}
uint8_t nx_h743_spi_official_byte_read(SPI_TypeDef* registers) {
    return LL_SPI_ReceiveData8(registers);
}
