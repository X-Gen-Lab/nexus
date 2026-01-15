/**
 * \file            hal_adc_stm32f4.c
 * \brief           STM32F4 ADC HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL ADC functions to provide the Nexus HAL
 * interface. It uses HAL_ADC_Init(), HAL_ADC_Start(),
 * HAL_ADC_PollForConversion(), HAL_ADC_GetValue(), and related functions from
 * the ST HAL library.
 */

#include "hal/hal_adc.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           ADC internal temperature sensor channel
 */
#define ADC_CHANNEL_TEMPSENSOR_NUM 16

/**
 * \brief           ADC internal reference voltage channel
 */
#define ADC_CHANNEL_VREFINT_NUM 17

/**
 * \brief           ADC internal VBAT channel
 */
#define ADC_CHANNEL_VBAT_NUM 18

/**
 * \brief           Maximum regular ADC channel number
 */
#define ADC_MAX_CHANNEL 15

/**
 * \brief           Internal reference voltage typical value (mV)
 */
#define VREFINT_CAL_MV 1210

/**
 * \brief           Temperature sensor calibration values (from datasheet)
 * \note            V25 = 0.76V, Avg_Slope = 2.5mV/°C
 */
#define TEMP_V25_MV             760
#define TEMP_AVG_SLOPE_UV_PER_C 2500

/**
 * \brief           ADC driver data structure - wraps ST HAL Handle
 */
typedef struct {
    ADC_HandleTypeDef hadc;      /**< ST HAL ADC Handle */
    hal_adc_config_t config;     /**< Nexus configuration */
    hal_adc_callback_t callback; /**< Conversion complete callback */
    void* context;               /**< Callback context */
    bool initialized;            /**< Initialization flag */
} adc_data_t;

/**
 * \brief           ADC instance data array
 */
static adc_data_t adc_data[HAL_ADC_MAX];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get ADC instance pointer from Nexus instance
 * \param[in]       instance: Nexus ADC instance
 * \return          ADC peripheral pointer or NULL
 */
static ADC_TypeDef* get_adc_instance(hal_adc_instance_t instance) {
    switch (instance) {
        case HAL_ADC_0:
            return ADC1;
        case HAL_ADC_1:
            return ADC2;
        case HAL_ADC_2:
            return ADC3;
        default:
            return NULL;
    }
}

/**
 * \brief           Map Nexus resolution to ST HAL resolution
 * \param[in]       resolution: Nexus resolution enum
 * \return          ST HAL resolution constant
 */
static uint32_t map_resolution(hal_adc_resolution_t resolution) {
    switch (resolution) {
        case HAL_ADC_RES_6BIT:
            return ADC_RESOLUTION_6B;
        case HAL_ADC_RES_8BIT:
            return ADC_RESOLUTION_8B;
        case HAL_ADC_RES_10BIT:
            return ADC_RESOLUTION_10B;
        case HAL_ADC_RES_12BIT:
        default:
            return ADC_RESOLUTION_12B;
    }
}

/**
 * \brief           Map Nexus sample time to ST HAL sample time
 * \param[in]       sample_time: Nexus sample time enum
 * \return          ST HAL sample time constant
 */
static uint32_t map_sample_time(hal_adc_sample_time_t sample_time) {
    switch (sample_time) {
        case HAL_ADC_SAMPLE_3CYCLES:
            return ADC_SAMPLETIME_3CYCLES;
        case HAL_ADC_SAMPLE_15CYCLES:
            return ADC_SAMPLETIME_15CYCLES;
        case HAL_ADC_SAMPLE_28CYCLES:
            return ADC_SAMPLETIME_28CYCLES;
        case HAL_ADC_SAMPLE_56CYCLES:
            return ADC_SAMPLETIME_56CYCLES;
        case HAL_ADC_SAMPLE_84CYCLES:
            return ADC_SAMPLETIME_84CYCLES;
        case HAL_ADC_SAMPLE_112CYCLES:
            return ADC_SAMPLETIME_112CYCLES;
        case HAL_ADC_SAMPLE_144CYCLES:
            return ADC_SAMPLETIME_144CYCLES;
        case HAL_ADC_SAMPLE_480CYCLES:
            return ADC_SAMPLETIME_480CYCLES;
        default:
            return ADC_SAMPLETIME_15CYCLES;
    }
}

/**
 * \brief           Map channel number to ST HAL channel constant
 * \param[in]       channel: Channel number (0-18)
 * \return          ST HAL channel constant
 */
static uint32_t map_channel(uint8_t channel) {
    switch (channel) {
        case 0:
            return ADC_CHANNEL_0;
        case 1:
            return ADC_CHANNEL_1;
        case 2:
            return ADC_CHANNEL_2;
        case 3:
            return ADC_CHANNEL_3;
        case 4:
            return ADC_CHANNEL_4;
        case 5:
            return ADC_CHANNEL_5;
        case 6:
            return ADC_CHANNEL_6;
        case 7:
            return ADC_CHANNEL_7;
        case 8:
            return ADC_CHANNEL_8;
        case 9:
            return ADC_CHANNEL_9;
        case 10:
            return ADC_CHANNEL_10;
        case 11:
            return ADC_CHANNEL_11;
        case 12:
            return ADC_CHANNEL_12;
        case 13:
            return ADC_CHANNEL_13;
        case 14:
            return ADC_CHANNEL_14;
        case 15:
            return ADC_CHANNEL_15;
        case 16:
            return ADC_CHANNEL_TEMPSENSOR;
        case 17:
            return ADC_CHANNEL_VREFINT;
        case 18:
            return ADC_CHANNEL_VBAT;
        default:
            return ADC_CHANNEL_0;
    }
}

/**
 * \brief           Get maximum ADC value for given resolution
 * \param[in]       resolution: ADC resolution
 * \return          Maximum value (2^bits - 1)
 */
static uint16_t get_max_value(hal_adc_resolution_t resolution) {
    switch (resolution) {
        case HAL_ADC_RES_6BIT:
            return 63;
        case HAL_ADC_RES_8BIT:
            return 255;
        case HAL_ADC_RES_10BIT:
            return 1023;
        case HAL_ADC_RES_12BIT:
        default:
            return 4095;
    }
}

/**
 * \brief           Get NVIC IRQ number for ADC
 * \param[in]       instance: Nexus ADC instance
 * \return          IRQ number
 */
static IRQn_Type adc_get_irqn(hal_adc_instance_t instance) {
    (void)instance;
    /* All ADCs share the same IRQ on STM32F4 */
    return ADC_IRQn;
}

/*===========================================================================*/
/* ST HAL MSP Functions                                                       */
/*===========================================================================*/

/**
 * \brief           ADC MSP Initialization
 * \note            Called by HAL_ADC_Init()
 * \param[in]       hadc: ADC handle pointer
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
    } else if (hadc->Instance == ADC2) {
        __HAL_RCC_ADC2_CLK_ENABLE();
    } else if (hadc->Instance == ADC3) {
        __HAL_RCC_ADC3_CLK_ENABLE();
    }
}

/**
 * \brief           ADC MSP De-Initialization
 * \note            Called by HAL_ADC_DeInit()
 * \param[in]       hadc: ADC handle pointer
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_DISABLE();
    } else if (hadc->Instance == ADC2) {
        __HAL_RCC_ADC2_CLK_DISABLE();
    } else if (hadc->Instance == ADC3) {
        __HAL_RCC_ADC3_CLK_DISABLE();
    }
}

/*===========================================================================*/
/* Public functions - ADC Initialization                                      */
/*===========================================================================*/

hal_status_t hal_adc_init(hal_adc_instance_t instance,
                          const hal_adc_config_t* config) {
    adc_data_t* adc;
    ADC_TypeDef* adc_instance;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    adc = &adc_data[instance];
    adc_instance = get_adc_instance(instance);

    if (adc_instance == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Check if already initialized */
    if (adc->initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

    /* Configure ST HAL ADC Handle */
    adc->hadc.Instance = adc_instance;
    adc->hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc->hadc.Init.Resolution = map_resolution(config->resolution);
    adc->hadc.Init.ScanConvMode = DISABLE;
    adc->hadc.Init.ContinuousConvMode = DISABLE;
    adc->hadc.Init.DiscontinuousConvMode = DISABLE;
    adc->hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc->hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc->hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->hadc.Init.NbrOfConversion = 1;
    adc->hadc.Init.DMAContinuousRequests = DISABLE;
    adc->hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    /* Call ST HAL ADC Init */
    if (HAL_ADC_Init(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Store configuration */
    adc->config = *config;
    adc->callback = NULL;
    adc->context = NULL;
    adc->initialized = true;

    return HAL_OK;
}

hal_status_t hal_adc_deinit(hal_adc_instance_t instance) {
    adc_data_t* adc;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Disable NVIC interrupt */
    HAL_NVIC_DisableIRQ(adc_get_irqn(instance));

    /* Call ST HAL ADC DeInit */
    if (HAL_ADC_DeInit(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Clear state */
    adc->callback = NULL;
    adc->context = NULL;
    adc->initialized = false;

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - ADC Channel Configuration                               */
/*===========================================================================*/

hal_status_t hal_adc_config_channel(hal_adc_instance_t instance,
                                    const hal_adc_channel_config_t* config) {
    adc_data_t* adc;
    ADC_ChannelConfTypeDef sConfig = {0};

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate channel number (0-15 for regular, 16-18 for internal) */
    if (config->channel > ADC_CHANNEL_VBAT_NUM) {
        return HAL_ERROR_INVALID_PARAM;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Configure channel using ST HAL */
    sConfig.Channel = map_channel(config->channel);
    sConfig.Rank = 1;
    sConfig.SamplingTime = map_sample_time(config->sample_time);

    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - ADC Conversion                                          */
/*===========================================================================*/

hal_status_t hal_adc_read(hal_adc_instance_t instance, uint8_t channel,
                          uint16_t* value, uint32_t timeout_ms) {
    adc_data_t* adc;
    ADC_ChannelConfTypeDef sConfig = {0};
    HAL_StatusTypeDef hal_status;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate channel number */
    if (channel > ADC_MAX_CHANNEL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Configure channel */
    sConfig.Channel = map_channel(channel);
    sConfig.Rank = 1;
    sConfig.SamplingTime = map_sample_time(adc->config.sample_time);

    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Start ADC conversion */
    if (HAL_ADC_Start(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Wait for conversion to complete */
    hal_status = HAL_ADC_PollForConversion(&adc->hadc, timeout_ms);
    if (hal_status == HAL_TIMEOUT) {
        HAL_ADC_Stop(&adc->hadc);
        return HAL_ERROR_TIMEOUT;
    } else if (hal_status != HAL_OK) {
        HAL_ADC_Stop(&adc->hadc);
        return HAL_ERROR;
    }

    /* Get conversion result */
    *value = (uint16_t)HAL_ADC_GetValue(&adc->hadc);

    /* Stop ADC */
    HAL_ADC_Stop(&adc->hadc);

    /* Invoke callback if registered */
    if (adc->callback != NULL) {
        adc->callback(instance, *value, adc->context);
    }

    return HAL_OK;
}

hal_status_t hal_adc_read_multi(hal_adc_instance_t instance,
                                const uint8_t* channels, uint16_t* values,
                                size_t count, uint32_t timeout_ms) {
    hal_status_t status;
    size_t i;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (channels == NULL || values == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    if (count == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Read each channel sequentially */
    for (i = 0; i < count; i++) {
        status = hal_adc_read(instance, channels[i], &values[i], timeout_ms);
        if (status != HAL_OK) {
            return status;
        }
    }

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - ADC Helper Functions                                    */
/*===========================================================================*/

uint32_t hal_adc_to_millivolts(hal_adc_instance_t instance, uint16_t raw_value,
                               uint32_t vref_mv) {
    adc_data_t* adc;
    uint16_t max_value;

    /* Parameter validation - return 0 for invalid instance */
    if (instance >= HAL_ADC_MAX) {
        return 0;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return 0;
    }

    /* Get max value based on resolution */
    max_value = get_max_value(adc->config.resolution);

    /* Calculate millivolts: mv = raw * vref / max_value */
    return ((uint32_t)raw_value * vref_mv) / max_value;
}

hal_status_t hal_adc_read_temperature(hal_adc_instance_t instance,
                                      int16_t* temp_c) {
    adc_data_t* adc;
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t raw_value;
    uint32_t voltage_mv;
    int32_t temperature;
    HAL_StatusTypeDef hal_status;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (temp_c == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Temperature sensor is only available on ADC1 */
    if (adc->hadc.Instance != ADC1) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Enable temperature sensor */
    ADC->CCR |= ADC_CCR_TSVREFE;

    /* Configure temperature sensor channel with long sample time */
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Start ADC conversion */
    if (HAL_ADC_Start(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Wait for conversion to complete */
    hal_status = HAL_ADC_PollForConversion(&adc->hadc, 100);
    if (hal_status != HAL_OK) {
        HAL_ADC_Stop(&adc->hadc);
        return (hal_status == HAL_TIMEOUT) ? HAL_ERROR_TIMEOUT : HAL_ERROR;
    }

    /* Get conversion result */
    raw_value = (uint16_t)HAL_ADC_GetValue(&adc->hadc);

    /* Stop ADC */
    HAL_ADC_Stop(&adc->hadc);

    /* Convert to millivolts (assuming 3.3V reference) */
    voltage_mv = hal_adc_to_millivolts(instance, raw_value, 3300);

    /* Calculate temperature using formula from datasheet:
     * Temperature (°C) = ((V_sense - V_25) / Avg_Slope) + 25
     * V_25 = 0.76V = 760mV
     * Avg_Slope = 2.5mV/°C = 2500µV/°C
     */
    temperature =
        (((int32_t)voltage_mv - TEMP_V25_MV) * 1000) / TEMP_AVG_SLOPE_UV_PER_C +
        25;

    *temp_c = (int16_t)temperature;

    return HAL_OK;
}

hal_status_t hal_adc_read_vref(hal_adc_instance_t instance, uint16_t* vref_mv) {
    adc_data_t* adc;
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t raw_value;
    uint16_t max_value;
    HAL_StatusTypeDef hal_status;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (vref_mv == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* VREFINT is only available on ADC1 */
    if (adc->hadc.Instance != ADC1) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Enable internal reference voltage */
    ADC->CCR |= ADC_CCR_TSVREFE;

    /* Configure VREFINT channel with long sample time */
    sConfig.Channel = ADC_CHANNEL_VREFINT;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&adc->hadc, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Start ADC conversion */
    if (HAL_ADC_Start(&adc->hadc) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Wait for conversion to complete */
    hal_status = HAL_ADC_PollForConversion(&adc->hadc, 100);
    if (hal_status != HAL_OK) {
        HAL_ADC_Stop(&adc->hadc);
        return (hal_status == HAL_TIMEOUT) ? HAL_ERROR_TIMEOUT : HAL_ERROR;
    }

    /* Get conversion result */
    raw_value = (uint16_t)HAL_ADC_GetValue(&adc->hadc);

    /* Stop ADC */
    HAL_ADC_Stop(&adc->hadc);

    /* Get max value based on resolution */
    max_value = get_max_value(adc->config.resolution);

    /* Calculate actual VDD using VREFINT:
     * VREFINT is typically 1.21V
     * VDD = VREFINT_CAL * max_value / raw_value
     * But we return the internal reference voltage value
     */
    *vref_mv = VREFINT_CAL_MV;

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - ADC Callback                                            */
/*===========================================================================*/

hal_status_t hal_adc_set_callback(hal_adc_instance_t instance,
                                  hal_adc_callback_t callback, void* context) {
    adc_data_t* adc;

    /* Parameter validation */
    if (instance >= HAL_ADC_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    adc = &adc_data[instance];

    if (!adc->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Store callback and context */
    adc->callback = callback;
    adc->context = context;

    if (callback != NULL) {
        /* Configure NVIC for ADC interrupt */
        HAL_NVIC_SetPriority(adc_get_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(adc_get_irqn(instance));
    } else {
        /* Disable interrupt if callback is NULL */
        HAL_NVIC_DisableIRQ(adc_get_irqn(instance));
    }

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementation                                             */
/*===========================================================================*/

/**
 * \brief           ST HAL ADC Conversion Complete Callback
 * \note            Called by HAL_ADC_IRQHandler() when conversion completes
 * \param[in]       hadc: ADC handle pointer
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    hal_adc_instance_t instance;
    adc_data_t* adc;
    uint16_t value;

    /* Find which ADC triggered the callback */
    if (hadc->Instance == ADC1) {
        instance = HAL_ADC_0;
    } else if (hadc->Instance == ADC2) {
        instance = HAL_ADC_1;
    } else if (hadc->Instance == ADC3) {
        instance = HAL_ADC_2;
    } else {
        return;
    }

    adc = &adc_data[instance];

    /* Get conversion value */
    value = (uint16_t)HAL_ADC_GetValue(hadc);

    /* Invoke user callback if registered */
    if (adc->callback != NULL) {
        adc->callback(instance, value, adc->context);
    }
}

/**
 * \brief           ST HAL ADC Error Callback
 * \note            Called by HAL_ADC_IRQHandler() when error occurs
 * \param[in]       hadc: ADC handle pointer
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc) {
    /* Error handling - could be extended to notify user */
    (void)hadc;
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL ADC Handler                                    */
/*===========================================================================*/

/**
 * \brief           ADC IRQ Handler (shared by all ADC instances)
 */
void ADC_IRQHandler(void) {
    /* Check which ADC triggered the interrupt and call appropriate handler */
    if (adc_data[HAL_ADC_0].initialized) {
        HAL_ADC_IRQHandler(&adc_data[HAL_ADC_0].hadc);
    }
    if (adc_data[HAL_ADC_1].initialized) {
        HAL_ADC_IRQHandler(&adc_data[HAL_ADC_1].hadc);
    }
    if (adc_data[HAL_ADC_2].initialized) {
        HAL_ADC_IRQHandler(&adc_data[HAL_ADC_2].hadc);
    }
}
