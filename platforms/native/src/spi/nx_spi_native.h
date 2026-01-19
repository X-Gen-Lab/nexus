/**
 * \file            nx_spi_native.h
 * \brief           SPI native platform public interface
 * \author          Nexus Team
 */

#ifndef NX_SPI_NATIVE_H
#define NX_SPI_NATIVE_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Get SPI instance (legacy factory function)
 * \param[in]       index: SPI index (0-2)
 * \return          SPI bus interface pointer, NULL on error
 */
nx_spi_bus_t* nx_spi_native_get(uint8_t index);

/**
 * \brief           Get SPI device descriptor (for testing)
 * \param[in]       index: SPI index (0-2)
 * \return          Device descriptor pointer, NULL on error
 */
nx_device_t* nx_spi_native_get_device(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_SPI_NATIVE_H */
