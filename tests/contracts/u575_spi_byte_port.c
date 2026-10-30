/**
 * \file            u575_spi_byte_port.c
 * \brief           Execute unchanged U575 LL byte access from host C
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u5xx_ll_spi.h"
#include "u575_spi_byte_model.h"
void nx_u575_spi_official_byte_write(SPI_TypeDef* registers, uint8_t value) {
    LL_SPI_TransmitData8(registers, value);
}
uint8_t nx_u575_spi_official_byte_read(SPI_TypeDef* registers) {
    return LL_SPI_ReceiveData8(registers);
}
