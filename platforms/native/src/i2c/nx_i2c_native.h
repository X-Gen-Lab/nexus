/**
 * \file            nx_i2c_native.h
 * \brief           I2C native platform public interface
 * \author          Nexus Team
 */

#ifndef NX_I2C_NATIVE_H
#define NX_I2C_NATIVE_H

#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get I2C bus instance by index
 * \param[in]       index: I2C bus index (0-2)
 * \return          I2C bus interface pointer, NULL if not enabled
 */
nx_i2c_bus_t* nx_i2c_native_get(uint8_t index);

/**
 * \brief           Get I2C device descriptor by index
 * \param[in]       index: I2C bus index (0-2)
 * \return          Device descriptor pointer, NULL if invalid
 */
nx_device_t* nx_i2c_native_get_device(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NX_I2C_NATIVE_H */
