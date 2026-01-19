/**
 * \file            nx_dac.h
 * \brief           DAC device interface definition
 * \author          Nexus Team
 */

#ifndef NX_DAC_H
#define NX_DAC_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DAC channel interface
 * \details         Provides access to individual DAC channel output control
 */
typedef struct nx_dac_channel_s nx_dac_channel_t;
struct nx_dac_channel_s {
    /**
     * \brief           Set DAC channel output value (raw)
     * \param[in]       self: Channel interface pointer
     * \param[in]       value: Raw output value (resolution depends on DAC)
     */
    void (*set_value)(nx_dac_channel_t* self, uint32_t value);

    /**
     * \brief           Set DAC channel output voltage in millivolts
     * \param[in]       self: Channel interface pointer
     * \param[in]       voltage_mv: Output voltage in millivolts
     */
    void (*set_voltage_mv)(nx_dac_channel_t* self, uint32_t voltage_mv);
};

/**
 * \brief           DAC device interface
 * \details         Provides DAC channel access and trigger with lifecycle
 *                  and power management
 */
typedef struct nx_dac_s nx_dac_t;
struct nx_dac_s {
    /**
     * \brief           Get DAC channel interface
     * \param[in]       self: DAC interface pointer
     * \param[in]       channel_index: Channel index (0-based)
     * \return          Channel interface pointer, NULL on invalid index
     */
    nx_dac_channel_t* (*get_channel)(nx_dac_t* self, uint8_t channel_index);

    /**
     * \brief           Trigger DAC output update
     * \param[in]       self: DAC interface pointer
     * \note            Updates all channels simultaneously when using
     *              synchronized output mode
     */
    void (*trigger)(nx_dac_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: DAC interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_dac_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: DAC interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_dac_t* self);
};

/*---------------------------------------------------------------------------*/
/* Initialization Macros                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DAC channel interface
 * \param[in]       p: Pointer to nx_dac_channel_t structure
 * \param[in]       _set_value: set_value function pointer
 * \param[in]       _set_voltage_mv: set_voltage_mv function pointer
 */
#define NX_INIT_DAC_CHANNEL(p, _set_value, _set_voltage_mv)                    \
    do {                                                                       \
        (p)->set_value = (_set_value);                                         \
        (p)->set_voltage_mv = (_set_voltage_mv);                               \
        NX_ASSERT((p)->set_value && (p)->set_voltage_mv);                      \
    } while (0)

/**
 * \brief           Initialize DAC interface
 * \param[in]       p: Pointer to nx_dac_t structure
 * \param[in]       _get_channel: get_channel function pointer
 * \param[in]       _trigger: trigger function pointer
 * \param[in]       _get_lifecycle: get_lifecycle function pointer
 * \param[in]       _get_power: get_power function pointer
 */
#define NX_INIT_DAC(p, _get_channel, _trigger, _get_lifecycle, _get_power)     \
    do {                                                                       \
        (p)->get_channel = (_get_channel);                                     \
        (p)->trigger = (_trigger);                                             \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->get_channel && (p)->trigger);                           \
        NX_ASSERT((p)->get_lifecycle);                                         \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_DAC_H */
