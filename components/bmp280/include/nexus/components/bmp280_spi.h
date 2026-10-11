/**
 * \file            bmp280_spi.h
 * \brief           Optional BMP280 adapter over a fixed SPI endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_BMP280_SPI_H
#define NEXUS_COMPONENTS_BMP280_SPI_H
#include "nexus/components/bmp280.h"
#include "nexus/io/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Bind a real polling SPI endpoint as settled transport
 * \param[in]       endpoint: Fixed endpoint kept alive throughout device use
 * \param[in]       clock: Same monotonic domain as IO transfer deadlines
 * \return          Narrow transport; no global discovery or CS selection
 */
nx_bmp280_transport_port_t
nx_bmp280_spi_transport(const nx_spi_endpoint_t* endpoint, nx_clock_t clock);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_BMP280_SPI_H */
