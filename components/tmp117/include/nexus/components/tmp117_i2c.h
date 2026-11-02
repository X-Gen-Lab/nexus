/**
 * \file            tmp117_i2c.h
 * \brief           TMP117 transport over an explicit settled I2C endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_TMP117_I2C_H
#define NEXUS_COMPONENTS_TMP117_I2C_H
#include "nexus/components/tmp117.h"
#include "nexus/io/i2c.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Bind an existing polling endpoint without I/O or discovery
 * \param[in]       endpoint: Explicit fixed-address face, alive until stop/join
 * \param[in]       clock: Same live domain as endpoint transaction deadlines
 * \return          Settled transport, or an invalid zero port for bad methods
 * \note            Actual ADD0 wiring/address 48h..4Bh remains external.
 *                  An async-only endpoint is rejected. No submit fallback
 *                  borrows stack descriptors. Exactly one serialized bus
 *                  executor performs exchanges without nested owner queues.
 */
nx_tmp117_transport_port_t
nx_tmp117_i2c_transport(const nx_i2c_endpoint_t* endpoint, nx_clock_t clock);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_TMP117_I2C_H */
