/**
 * \file            hal_adc.h
 * \brief           HAL ADC Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_ADC ADC Hardware Abstraction
 * \brief           ADC interface for hardware abstraction
 * \{
 */

/**
 * \brief           ADC instance enumeration
 */
typedef enum {
    HAL_ADC_0 = 0,                  /**< ADC instance 0 */
    HAL_ADC_1,                      /**< ADC instance 1 */
    HAL_ADC_2,                      /**< ADC instance 2 */
    HAL_ADC_MAX                     /**< Maximum ADC count */
} hal_adc_instance_t;

/**
 * \brief           ADC resolution
 */
typedef enum {
    HAL_ADC_RES_6BIT = 0,           /**< 6-bit resolution */
    HAL_ADC_RES_8BIT,               /**< 8-bit resolution */
    HAL_ADC_RES_10BIT,              /**< 10-bit resolution */
    HAL_ADC_RES_12BIT               /**< 12-bit resolution */
} hal_adc_resolution_t;

/**
 * \brief           ADC reference voltage
 */
typedef enum {
    HAL_ADC_REF_INTERNAL = 0,       /**< Internal reference */
    HAL_ADC_REF_EXTERNAL,           /**< External reference */
    HAL_ADC_REF_VDD                 /**< VDD as reference */
} hal_adc_reference_t;

/**
 * \brief           ADC sample time
 */
typedef enum {
    HAL_ADC_SAMPLE_3CYCLES = 0,     /**< 3 cycles */
    HAL_ADC_SAMPLE_15CYCLES,        /**< 15 cycles */
    HAL_ADC_SAMPLE_28CYCLES,        /**< 28 cycles */
    HAL_ADC_SAMPLE_56CYCLES,        /**< 56 cycles */
    HAL_ADC_SAMPLE_84CYCLES,        /**< 84 cycles */
    HAL_ADC_SAMPLE_112CYCLES,       /**< 112 cycles */
    HAL_ADC_SAMPLE_144CYCLES,       /**< 144 cycles */
    HAL_ADC_SAMPLE_480CYCLES        /**< 480 cycles */
} hal_adc_sample_time_t;

/**
 * \brief           ADC configuration structure
 */
typedef struct {
    hal_adc_resolution_t  resolution;   /**< ADC resolution */
    hal_adc_reference_t   reference;    /**< Reference voltage */
    hal_adc_sample_time_t sample_time;  /**< Sample time */
} hal_adc_config_t;

/**
 * \brief           ADC channel configuration
 */
typedef struct {
    uint8_t               channel;      /**< Channel number (0-15) */
    hal_adc_sample_time_t sample_time;  /**< Sample time for this channel */
} hal_adc_channel_config_t;

/**
 * \brief           ADC conversion complete callback
 * \param[in]       instance: ADC instance
 * \param[in]       value: Converted value
 * \param[in]       context: User context
 */
typedef void (*hal_adc_callback_t)(hal_adc_instance_t instance,
                                   uint16_t value,
                                   void* context);

/**
 * \brief           Initialize ADC
 * \param[in]       instance: ADC instance
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_init(hal_adc_instance_t instance,
                          const hal_adc_config_t* config);

/**
 * \brief           Deinitialize ADC
 * \param[in]       instance: ADC instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_deinit(hal_adc_instance_t instance);

/**
 * \brief           Configure ADC channel
 * \param[in]       instance: ADC instance
 * \param[in]       config: Pointer to channel configuration
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_config_channel(hal_adc_instance_t instance,
                                    const hal_adc_channel_config_t* config);

/**
 * \brief           Start ADC conversion (blocking)
 * \param[in]       instance: ADC instance
 * \param[in]       channel: Channel number
 * \param[out]      value: Pointer to store converted value
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_read(hal_adc_instance_t instance,
                          uint8_t channel,
                          uint16_t* value,
                          uint32_t timeout_ms);

/**
 * \brief           Read multiple channels (blocking)
 * \param[in]       instance: ADC instance
 * \param[in]       channels: Array of channel numbers
 * \param[out]      values: Array to store converted values
 * \param[in]       count: Number of channels
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_read_multi(hal_adc_instance_t instance,
                                const uint8_t* channels,
                                uint16_t* values,
                                size_t count,
                                uint32_t timeout_ms);

/**
 * \brief           Convert raw ADC value to millivolts
 * \param[in]       instance: ADC instance
 * \param[in]       raw_value: Raw ADC value
 * \param[in]       vref_mv: Reference voltage in millivolts
 * \return          Voltage in millivolts
 */
uint32_t hal_adc_to_millivolts(hal_adc_instance_t instance,
                               uint16_t raw_value,
                               uint32_t vref_mv);

/**
 * \brief           Read internal temperature sensor
 * \param[in]       instance: ADC instance
 * \param[out]      temp_c: Pointer to store temperature in Celsius
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_read_temperature(hal_adc_instance_t instance,
                                      int16_t* temp_c);

/**
 * \brief           Read internal reference voltage
 * \param[in]       instance: ADC instance
 * \param[out]      vref_mv: Pointer to store reference voltage in mV
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_read_vref(hal_adc_instance_t instance,
                               uint16_t* vref_mv);

/**
 * \brief           Register conversion complete callback
 * \param[in]       instance: ADC instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_adc_set_callback(hal_adc_instance_t instance,
                                  hal_adc_callback_t callback,
                                  void* context);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
