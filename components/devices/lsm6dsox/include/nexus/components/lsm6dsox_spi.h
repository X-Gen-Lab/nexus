/**
 * \file            lsm6dsox_spi.h
 * \brief           LSM6DSOX over one settled whole-CS SPI endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LSM6DSOX_SPI_H
#define NEXUS_COMPONENTS_LSM6DSOX_SPI_H
#include "nexus/components/lsm6dsox.h"
#include "nexus/io/spi.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Bind an existing eight-bit polling SPI endpoint.
 * \param[in]       endpoint: Stable explicit CS, mode 0/3 and reviewed rate.
 * \param[in]       clock: Same live monotonic domain as transaction deadlines.
 * \return          Transport, or a zero invalid port for missing methods.
 * \note            One command plus at most six payload bytes shares CS.
 *                  Local arrays are released on every return; no async
 * fallback.
 */
nx_lsm6dsox_transport_port_t
nx_lsm6dsox_spi_transport(const nx_spi_endpoint_t* endpoint, nx_clock_t clock);
#ifdef __cplusplus
}
#endif
#endif
