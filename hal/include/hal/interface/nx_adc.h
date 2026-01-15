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

/**
 * \brief           ADC resolution enumeration
 */
typedef enum nx_adc_resolution_e {
    NX_ADC_RESOLUTION_6BIT = 0, /**< 6-bit resolution */
    NX_ADC_RESOLUTION_8BIT,     /**< 8-bit resolution */
    NX_ADC_RESOLUTION_10BIT,    /**< 10-bit resolution */
    NX_ADC_RESOLUTION_12BIT,    /**< 12-bit resolution */
    NX_ADC_RESOLUTION_14BIT,    /**< 14-bit resolution */
    NX_ADC_RESOLUTION_16BIT,    /**< 16-bit resolution */
} nx_adc_resolution_t;

/**
 * \brief           ADC sampling time enumeration
 */
typedef enum nx_adc_sampling_time_e {
    NX_ADC_SAMPLING_3_CYCLES = 0, /**< 3 cycles */
    NX_ADC_SAMPLING_15_CYCLES,    /**< 15 cycles */
    NX_ADC_SAMPLING_28_CYCLES,    /**< 28 cycles */
    NX_ADC_SAMPLING_56_CYCLES,    /**< 56 cycles */
    NX_ADC_SAMPLING_84_CYCLES,    /**< 84 cycles */
    NX_ADC_SAMPLING_112_CYCLES,   /**< 112 cycles */
    NX_ADC_SAMPLING_144_CYCLES,   /**< 144 cycles */
    NX_ADC_SAMPLING_480_CYCLES,   /**< 480 cycles */
} nx_adc_sampling_time_t;

/**
 * \brief           ADC trigger source enumeration
 */
typedef enum nx_adc_trigger_e {
    NX_ADC_TRIGGER_SOFTWARE = 0, /**< Software trigger */
    NX_ADC_TRIGGER_TIMER,        /**< Timer trigger */
    NX_ADC_TRIGGER_EXTERNAL,     /**< External trigger */
} nx_adc_trigger_t;

/**
 * \brief           ADC configuration structure
 */
typedef struct nx_adc_config_s {
    nx_adc_resolution_t resolution;       /**< ADC resolution */
    nx_adc_sampling_time_t sampling_time; /**< Sampling time */
    nx_adc_trigger_t trigger;             /**< Trigger source */
    bool continuous_mode;                 /**< Continuous conversion mode */
    bool dma_enable;                      /**< Enable DMA */
    uint8_t channel_count;                /**< Number of channels */
    uint8_t* channels;                    /**< Channel list */
} nx_adc_config_t;

/**
 * \brief           ADC statistics structure
 */
typedef struct nx_adc_stats_s {
    bool busy;                 /**< Busy flag */
    uint32_t conversion_count; /**< Total conversions */
    uint32_t overrun_count;    /**< Overrun error count */
    uint32_t dma_error_count;  /**< DMA error count */
} nx_adc_stats_t;

/**
 * \brief           ADC conversion complete callback type
 * \param[in]       context: User context pointer
 * \param[in]       channel: ADC channel
 * \param[in]       value: Conversion result
 */
typedef void (*nx_adc_callback_t)(void* context, uint8_t channel,
                                  uint16_t value);

/**
 * \brief           ADC device interface
 */
typedef struct nx_adc_s nx_adc_t;
struct nx_adc_s {
    /* Single conversion operations */
    nx_status_t (*read)(nx_adc_t* self, uint8_t channel, uint16_t* value);
    nx_status_t (*read_voltage)(nx_adc_t* self, uint8_t channel,
                                uint32_t* voltage_mv);

    /* Multi-channel operations */
    nx_status_t (*read_multi)(nx_adc_t* self, uint8_t* channels, uint8_t count,
                              uint16_t* values);

    /* Continuous/DMA operations */
    nx_status_t (*start_continuous)(nx_adc_t* self);
    nx_status_t (*stop_continuous)(nx_adc_t* self);
    nx_status_t (*get_buffer)(nx_adc_t* self, uint16_t* buffer, size_t* count);

    /* Callback */
    nx_status_t (*set_callback)(nx_adc_t* self, nx_adc_callback_t cb,
                                void* ctx);
    nx_status_t (*clear_callback)(nx_adc_t* self);

    /* Calibration */
    nx_status_t (*calibrate)(nx_adc_t* self);
    nx_status_t (*set_reference_voltage)(nx_adc_t* self, uint32_t vref_mv);

    /* Runtime configuration */
    nx_status_t (*set_resolution)(nx_adc_t* self,
                                  nx_adc_resolution_t resolution);
    nx_status_t (*set_sampling_time)(nx_adc_t* self,
                                     nx_adc_sampling_time_t time);
    nx_status_t (*get_config)(nx_adc_t* self, nx_adc_config_t* cfg);
    nx_status_t (*set_config)(nx_adc_t* self, const nx_adc_config_t* cfg);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_adc_t* self);
    nx_power_t* (*get_power)(nx_adc_t* self);
    nx_diagnostic_t* (*get_diagnostic)(nx_adc_t* self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_adc_t* self, nx_adc_stats_t* stats);
    nx_status_t (*clear_stats)(nx_adc_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_ADC_H */
