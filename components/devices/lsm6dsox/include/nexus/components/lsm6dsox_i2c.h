/**
 * \file            lsm6dsox_i2c.h
 * \brief           LSM6DSOX over one settled fixed-address I2C endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LSM6DSOX_I2C_H
#define NEXUS_COMPONENTS_LSM6DSOX_I2C_H
#include "nexus/components/lsm6dsox.h"
#include "nexus/io/i2c.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Bind an existing settled endpoint without bus access.
 * \param[in]       endpoint: Stable polling endpoint, address 6Ah or 6Bh.
 * \param[in]       clock: Same live monotonic domain as transaction deadlines.
 * \return          Transport, or a zero invalid port for missing methods.
 * \note            Address straps/electrical facts stay external. Repeated
 *                  START is required; no async fallback borrows stack arrays.
 */
nx_lsm6dsox_transport_port_t
nx_lsm6dsox_i2c_transport(const nx_i2c_endpoint_t* endpoint, nx_clock_t clock);
#ifdef __cplusplus
}
#endif
#endif
