/**
 * \file            nx_adc.h
 * \brief           ADC device interface definition
 * \author          Nexus Team
 */

#ifndef NX_ADC_H
#define NX_ADC_H

#include "hal/interface/nx_diagnostic.h"
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
 * \brief           ADC buffer callback function type
 * \param[in]       buffer: Pointer to sample buffer
 * \param[in]       size: Number of samples in buffer
 * \param[in]       user_data: User-provided context pointer
 * \note            Buffer contains interleaved multi-channel samples
 */
typedef void (*nx_adc_buffer_callback_t)(const uint32_t* buffer, size_t size,
                                         void* user_data);

/**
 * \brief           ADC channel interface
 * \details         Provides access to individual ADC channel values
 */
typedef struct nx_adc_channel_s nx_adc_channel_t;
struct nx_adc_channel_s {
    /**
     * \brief           Get ADC channel conversion value
     * \param[in]       self: Channel interface pointer
     * \return          Conversion result (raw value)
     */
    uint32_t (*get_value)(nx_adc_channel_t* self);
};

/**
 * \brief           ADC device interface (simple single-shot mode)
 * \details         Provides basic ADC trigger and channel access with
 *                  lifecycle, power, and diagnostic management
 */
typedef struct nx_adc_s nx_adc_t;
struct nx_adc_s {
    /**
     * \brief           Trigger ADC conversion (single-shot mode)
     * \param[in]       self: ADC interface pointer
     * \note            Starts conversion on all configured channels
     */
    void (*trigger)(nx_adc_t* self);

    /**
     * \brief           Get ADC channel interface
     * \param[in]       self: ADC interface pointer
     * \param[in]       channel_index: Channel index (0-based)
     * \return          Channel interface pointer, NULL on invalid index
     */
    nx_adc_channel_t* (*get_channel)(nx_adc_t* self, uint8_t channel_index);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: ADC interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_adc_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: ADC interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_adc_t* self);

    /**
     * \brief           Get diagnostic interface
     * \param[in]       self: ADC interface pointer
     * \return          Diagnostic interface pointer
     */
    nx_diagnostic_t* (*get_diagnostic)(nx_adc_t* self);
};

/**
 * \brief           ADC buffered multi-channel sampling interface
 * \details         High-performance interface that exposes internal buffer
 *                  for direct access to multi-channel samples. Buffer size
 *                  is a multiple of channel count for efficient DMA operation.
 */
typedef struct nx_adc_buffer_s nx_adc_buffer_t;
struct nx_adc_buffer_s {
    /**
     * \brief           Trigger buffered sampling
     * \param[in]       self: ADC buffer interface pointer
     * \note            Starts sampling into internal buffer
     */
    void (*trigger)(nx_adc_buffer_t* self);

    /**
     * \brief           Register buffer-full callback
     * \param[in]       self: ADC buffer interface pointer
     * \param[in]       callback: Callback invoked when buffer is full
     * \param[in]       user_data: User context passed to callback
     * \note            Callback receives buffer pointer, size, and user_data
     */
    void (*register_callback)(nx_adc_buffer_t* self,
                              nx_adc_buffer_callback_t callback,
                              void* user_data);

    /**
     * \brief           Get sample buffer pointer
     * \param[in]       self: ADC buffer interface pointer
     * \return          Pointer to internal sample buffer
     * \note            Buffer contains interleaved multi-channel samples
     */
    uint32_t* (*get_buffer)(nx_adc_buffer_t* self);

    /**
     * \brief           Get buffer capacity
     * \param[in]       self: ADC buffer interface pointer
     * \return          Total buffer size in samples (multiple of channel count)
     */
    size_t (*get_buffer_size)(nx_adc_buffer_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: ADC buffer interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_adc_buffer_t* self);

    /**
     * \brief           Get power management interface
     * \param[in]       self: ADC buffer interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_adc_buffer_t* self);
};

/*---------------------------------------------------------------------------*/
/* Initialization Macros                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize ADC channel interface
 * \param[in]       p: Pointer to nx_adc_channel_t structure
 * \param[in]       _get_value: get_value function pointer
 */
#define NX_INIT_ADC_CHANNEL(p, _get_value)                                     \
    do {                                                                       \
        (p)->get_value = (_get_value);                                         \
        NX_ASSERT((p)->get_value);                                             \
    } while (0)

/**
 * \brief           Initialize ADC interface
 * \param[in]       p: Pointer to nx_adc_t structure
 * \param[in]       _trigger: trigger function pointer
 * \param[in]       _get_channel: get_channel function pointer
 * \param[in]       _get_lifecycle: get_lifecycle function pointer
 * \param[in]       _get_power: get_power function pointer
 * \param[in]       _get_diagnostic: get_diagnostic function pointer
 */
#define NX_INIT_ADC(p, _trigger, _get_channel, _get_lifecycle, _get_power,     \
                    _get_diagnostic)                                           \
    do {                                                                       \
        (p)->trigger = (_trigger);                                             \
        (p)->get_channel = (_get_channel);                                     \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        (p)->get_diagnostic = (_get_diagnostic);                               \
        NX_ASSERT((p)->trigger && (p)->get_channel);                           \
        NX_ASSERT((p)->get_lifecycle);                                         \
    } while (0)

/**
 * \brief           Initialize ADC buffer interface
 * \param[in]       p: Pointer to nx_adc_buffer_t structure
 * \param[in]       _trigger: trigger function pointer
 * \param[in]       _register_callback: register_callback function pointer
 * \param[in]       _get_buffer: get_buffer function pointer
 * \param[in]       _get_buffer_size: get_buffer_size function pointer
 * \param[in]       _get_lifecycle: get_lifecycle function pointer
 * \param[in]       _get_power: get_power function pointer
 */
#define NX_INIT_ADC_BUFFER(p, _trigger, _register_callback, _get_buffer,       \
                           _get_buffer_size, _get_lifecycle, _get_power)       \
    do {                                                                       \
        (p)->trigger = (_trigger);                                             \
        (p)->register_callback = (_register_callback);                         \
        (p)->get_buffer = (_get_buffer);                                       \
        (p)->get_buffer_size = (_get_buffer_size);                             \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->trigger && (p)->register_callback);                     \
        NX_ASSERT((p)->get_buffer && (p)->get_buffer_size);                    \
        NX_ASSERT((p)->get_lifecycle);                                         \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_ADC_H */
