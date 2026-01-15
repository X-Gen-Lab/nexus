/**
 * \file            hal_timer_stm32f4.c
 * \brief           STM32F4 Timer HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL Timer functions to provide the Nexus HAL
 * interface. It uses HAL_TIM_Base_Init(), HAL_TIM_PWM_Init(), and related
 * functions from the ST HAL library.
 */

#include "hal/hal_timer.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           Timer clock frequency (APB1 timer clock = 84MHz for TIM2-5)
 * \note            APB1 prescaler = 4, so timer clock = 2 * APB1 = 84MHz
 */
#define TIMER_CLOCK_FREQ 84000000UL

/**
 * \brief           Maximum prescaler value (16-bit)
 */
#define TIMER_MAX_PRESCALER 65535UL

/**
 * \brief           Maximum period value (32-bit for TIM2/TIM5, 16-bit for
 * TIM3/TIM4)
 */
#define TIMER_MAX_PERIOD_32 0xFFFFFFFFUL
#define TIMER_MAX_PERIOD_16 0xFFFFUL

/**
 * \brief           Timer driver data structure - wraps ST HAL Handle
 */
typedef struct {
    TIM_HandleTypeDef htim;        /**< ST HAL Timer Handle */
    hal_timer_config_t config;     /**< Nexus configuration */
    hal_timer_callback_t callback; /**< Overflow callback */
    void* context;                 /**< Callback context */
    bool initialized;              /**< Initialization flag */
    bool running;                  /**< Running state */
} timer_data_t;

/**
 * \brief           PWM channel data structure
 */
typedef struct {
    uint32_t frequency;  /**< PWM frequency */
    uint16_t duty_cycle; /**< Duty cycle (0-10000) */
    bool initialized;    /**< Channel initialized flag */
    bool running;        /**< Channel running flag */
} pwm_channel_data_t;

/**
 * \brief           Timer instance data array
 */
static timer_data_t timer_data[HAL_TIMER_MAX];

/**
 * \brief           PWM channel data array
 */
static pwm_channel_data_t pwm_data[HAL_TIMER_MAX][HAL_TIMER_CH_MAX];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get TIM instance pointer from Nexus instance
 * \param[in]       instance: Nexus timer instance
 * \return          TIM peripheral pointer or NULL
 */
static TIM_TypeDef* get_tim_instance(hal_timer_instance_t instance) {
    switch (instance) {
        case HAL_TIMER_0:
            return TIM2;
        case HAL_TIMER_1:
            return TIM3;
        case HAL_TIMER_2:
            return TIM4;
        case HAL_TIMER_3:
            return TIM5;
        default:
            return NULL;
    }
}

/**
 * \brief           Check if timer is 32-bit (TIM2, TIM5)
 * \param[in]       instance: Nexus timer instance
 * \return          true if 32-bit timer
 */
static bool is_32bit_timer(hal_timer_instance_t instance) {
    return (instance == HAL_TIMER_0 || instance == HAL_TIMER_3);
}

/**
 * \brief           Enable timer clock
 * \param[in]       instance: Nexus timer instance
 */
static void timer_enable_clock(hal_timer_instance_t instance) {
    switch (instance) {
        case HAL_TIMER_0:
            __HAL_RCC_TIM2_CLK_ENABLE();
            break;
        case HAL_TIMER_1:
            __HAL_RCC_TIM3_CLK_ENABLE();
            break;
        case HAL_TIMER_2:
            __HAL_RCC_TIM4_CLK_ENABLE();
            break;
        case HAL_TIMER_3:
            __HAL_RCC_TIM5_CLK_ENABLE();
            break;
        default:
            break;
    }
}

/**
 * \brief           Disable timer clock
 * \param[in]       instance: Nexus timer instance
 */
static void timer_disable_clock(hal_timer_instance_t instance) {
    switch (instance) {
        case HAL_TIMER_0:
            __HAL_RCC_TIM2_CLK_DISABLE();
            break;
        case HAL_TIMER_1:
            __HAL_RCC_TIM3_CLK_DISABLE();
            break;
        case HAL_TIMER_2:
            __HAL_RCC_TIM4_CLK_DISABLE();
            break;
        case HAL_TIMER_3:
            __HAL_RCC_TIM5_CLK_DISABLE();
            break;
        default:
            break;
    }
}

/**
 * \brief           Get NVIC IRQ number for timer
 * \param[in]       instance: Nexus timer instance
 * \return          IRQ number
 */
static IRQn_Type timer_get_irqn(hal_timer_instance_t instance) {
    switch (instance) {
        case HAL_TIMER_0:
            return TIM2_IRQn;
        case HAL_TIMER_1:
            return TIM3_IRQn;
        case HAL_TIMER_2:
            return TIM4_IRQn;
        case HAL_TIMER_3:
            return TIM5_IRQn;
        default:
            return TIM2_IRQn;
    }
}

/**
 * \brief           Map Nexus channel to ST HAL channel
 * \param[in]       channel: Nexus channel
 * \return          ST HAL channel constant
 */
static uint32_t map_channel(hal_timer_channel_t channel) {
    switch (channel) {
        case HAL_TIMER_CH_1:
            return TIM_CHANNEL_1;
        case HAL_TIMER_CH_2:
            return TIM_CHANNEL_2;
        case HAL_TIMER_CH_3:
            return TIM_CHANNEL_3;
        case HAL_TIMER_CH_4:
            return TIM_CHANNEL_4;
        default:
            return TIM_CHANNEL_1;
    }
}

/**
 * \brief           Calculate prescaler and period for given period_us
 * \param[in]       period_us: Desired period in microseconds
 * \param[in]       is_32bit: True if 32-bit timer
 * \param[out]      prescaler: Calculated prescaler value
 * \param[out]      period: Calculated period value
 * \return          HAL_OK on success
 */
static hal_status_t calculate_timer_params(uint32_t period_us, bool is_32bit,
                                           uint32_t* prescaler,
                                           uint32_t* period) {
    uint64_t ticks;
    uint32_t max_period;
    uint32_t psc;

    if (period_us == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    max_period = is_32bit ? TIMER_MAX_PERIOD_32 : TIMER_MAX_PERIOD_16;

    /* Calculate total ticks needed */
    ticks = ((uint64_t)TIMER_CLOCK_FREQ * period_us) / 1000000ULL;

    /* Find suitable prescaler */
    psc = 0;
    while (ticks > max_period && psc < TIMER_MAX_PRESCALER) {
        psc++;
        ticks =
            ((uint64_t)TIMER_CLOCK_FREQ * period_us) / (1000000ULL * (psc + 1));
    }

    if (ticks > max_period || ticks == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    *prescaler = psc;
    *period = (uint32_t)(ticks - 1);

    return HAL_OK;
}

/**
 * \brief           Calculate prescaler and ARR for PWM frequency
 * \param[in]       frequency: Desired PWM frequency in Hz
 * \param[in]       is_32bit: True if 32-bit timer
 * \param[out]      prescaler: Calculated prescaler value
 * \param[out]      arr: Calculated ARR value
 * \return          HAL_OK on success
 */
static hal_status_t calculate_pwm_params(uint32_t frequency, bool is_32bit,
                                         uint32_t* prescaler, uint32_t* arr) {
    uint32_t max_period;
    uint32_t psc;
    uint32_t period;

    if (frequency == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    max_period = is_32bit ? TIMER_MAX_PERIOD_32 : TIMER_MAX_PERIOD_16;

    /* Calculate period for given frequency */
    /* ARR = (f_tim / frequency) - 1 */
    psc = 0;
    period = TIMER_CLOCK_FREQ / frequency;

    /* Find suitable prescaler if period is too large */
    while (period > max_period && psc < TIMER_MAX_PRESCALER) {
        psc++;
        period = TIMER_CLOCK_FREQ / (frequency * (psc + 1));
    }

    if (period > max_period || period == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    *prescaler = psc;
    *arr = period - 1;

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL MSP Functions                                                       */
/*===========================================================================*/

/**
 * \brief           Timer Base MSP Initialization
 * \note            Called by HAL_TIM_Base_Init()
 * \param[in]       htim: TIM handle pointer
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    } else if (htim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    } else if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
    } else if (htim->Instance == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
    }
}

/**
 * \brief           Timer Base MSP De-Initialization
 * \note            Called by HAL_TIM_Base_DeInit()
 * \param[in]       htim: TIM handle pointer
 */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_DISABLE();
    } else if (htim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_DISABLE();
    } else if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_DISABLE();
    } else if (htim->Instance == TIM5) {
        __HAL_RCC_TIM5_CLK_DISABLE();
    }
}

/*===========================================================================*/
/* Public functions - Timer Base                                              */
/*===========================================================================*/

hal_status_t hal_timer_init(hal_timer_instance_t instance,
                            const hal_timer_config_t* config) {
    timer_data_t* timer;
    TIM_TypeDef* tim_instance;
    uint32_t prescaler, period;
    hal_status_t status;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    if (config->period_us == 0) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];
    tim_instance = get_tim_instance(instance);

    if (tim_instance == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Calculate timer parameters */
    status = calculate_timer_params(config->period_us, is_32bit_timer(instance),
                                    &prescaler, &period);
    if (status != HAL_OK) {
        return status;
    }

    /* Configure ST HAL TIM Handle */
    timer->htim.Instance = tim_instance;
    timer->htim.Init.Prescaler = prescaler;
    timer->htim.Init.CounterMode = (config->direction == HAL_TIMER_DIR_DOWN)
                                       ? TIM_COUNTERMODE_DOWN
                                       : TIM_COUNTERMODE_UP;
    timer->htim.Init.Period = period;
    timer->htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    /* Call ST HAL Timer Base Init */
    if (HAL_TIM_Base_Init(&timer->htim) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Store configuration */
    timer->config = *config;
    timer->callback = NULL;
    timer->context = NULL;
    timer->initialized = true;
    timer->running = false;

    return HAL_OK;
}

hal_status_t hal_timer_deinit(hal_timer_instance_t instance) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Stop timer if running */
    if (timer->running) {
        hal_timer_stop(instance);
    }

    /* Disable NVIC interrupt */
    HAL_NVIC_DisableIRQ(timer_get_irqn(instance));

    /* Call ST HAL Timer Base DeInit */
    if (HAL_TIM_Base_DeInit(&timer->htim) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Clear state */
    timer->callback = NULL;
    timer->context = NULL;
    timer->initialized = false;
    timer->running = false;

    /* Clear PWM channel data */
    for (int ch = 0; ch < HAL_TIMER_CH_MAX; ch++) {
        pwm_data[instance][ch].initialized = false;
        pwm_data[instance][ch].running = false;
    }

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - Timer Control                                           */
/*===========================================================================*/

hal_status_t hal_timer_start(hal_timer_instance_t instance) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Start timer with or without interrupt based on callback */
    if (timer->callback != NULL) {
        /* Start with interrupt */
        if (HAL_TIM_Base_Start_IT(&timer->htim) != HAL_OK) {
            return HAL_ERROR;
        }
    } else {
        /* Start without interrupt */
        if (HAL_TIM_Base_Start(&timer->htim) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    timer->running = true;
    return HAL_OK;
}

hal_status_t hal_timer_stop(hal_timer_instance_t instance) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Stop timer with or without interrupt based on callback */
    if (timer->callback != NULL) {
        if (HAL_TIM_Base_Stop_IT(&timer->htim) != HAL_OK) {
            return HAL_ERROR;
        }
    } else {
        if (HAL_TIM_Base_Stop(&timer->htim) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    timer->running = false;
    return HAL_OK;
}

hal_status_t hal_timer_get_count(hal_timer_instance_t instance,
                                 uint32_t* count) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (count == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Use ST HAL macro to get counter value */
    *count = __HAL_TIM_GET_COUNTER(&timer->htim);

    return HAL_OK;
}

hal_status_t hal_timer_set_count(hal_timer_instance_t instance,
                                 uint32_t count) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Use ST HAL macro to set counter value */
    __HAL_TIM_SET_COUNTER(&timer->htim, count);

    return HAL_OK;
}

hal_status_t hal_timer_set_callback(hal_timer_instance_t instance,
                                    hal_timer_callback_t callback,
                                    void* context) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Store callback and context */
    timer->callback = callback;
    timer->context = context;

    if (callback != NULL) {
        /* Configure NVIC for timer interrupt */
        HAL_NVIC_SetPriority(timer_get_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(timer_get_irqn(instance));
    } else {
        /* Disable interrupt if callback is NULL */
        HAL_NVIC_DisableIRQ(timer_get_irqn(instance));
    }

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions - PWM                                                     */
/*===========================================================================*/

hal_status_t hal_pwm_init(hal_timer_instance_t instance,
                          hal_timer_channel_t channel,
                          const hal_pwm_config_t* config) {
    timer_data_t* timer;
    TIM_TypeDef* tim_instance;
    TIM_OC_InitTypeDef oc_config = {0};
    uint32_t prescaler, arr;
    uint32_t ccr;
    hal_status_t status;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX || channel >= HAL_TIMER_CH_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    if (config->frequency == 0 || config->duty_cycle > 10000) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];
    tim_instance = get_tim_instance(instance);

    if (tim_instance == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Calculate PWM parameters */
    status = calculate_pwm_params(config->frequency, is_32bit_timer(instance),
                                  &prescaler, &arr);
    if (status != HAL_OK) {
        return status;
    }

    /* Initialize timer base if not already initialized */
    if (!timer->initialized) {
        /* Enable clock */
        timer_enable_clock(instance);

        /* Configure ST HAL TIM Handle for PWM */
        timer->htim.Instance = tim_instance;
        timer->htim.Init.Prescaler = prescaler;
        timer->htim.Init.CounterMode = TIM_COUNTERMODE_UP;
        timer->htim.Init.Period = arr;
        timer->htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        timer->htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_PWM_Init(&timer->htim) != HAL_OK) {
            return HAL_ERROR;
        }

        timer->initialized = true;
    } else {
        /* Update timer period for new frequency */
        timer->htim.Init.Prescaler = prescaler;
        timer->htim.Init.Period = arr;
        __HAL_TIM_SET_PRESCALER(&timer->htim, prescaler);
        __HAL_TIM_SET_AUTORELOAD(&timer->htim, arr);
    }

    /* Calculate CCR value for duty cycle */
    /* CCR = (ARR + 1) * duty_cycle / 10000 */
    ccr = ((uint64_t)(arr + 1) * config->duty_cycle) / 10000UL;

    /* Configure PWM channel */
    oc_config.OCMode = TIM_OCMODE_PWM1;
    oc_config.Pulse = ccr;
    oc_config.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc_config.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&timer->htim, &oc_config,
                                  map_channel(channel)) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Store PWM configuration */
    pwm_data[instance][channel].frequency = config->frequency;
    pwm_data[instance][channel].duty_cycle = config->duty_cycle;
    pwm_data[instance][channel].initialized = true;
    pwm_data[instance][channel].running = false;

    return HAL_OK;
}

hal_status_t hal_pwm_start(hal_timer_instance_t instance,
                           hal_timer_channel_t channel) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX || channel >= HAL_TIMER_CH_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    if (!pwm_data[instance][channel].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Start PWM using ST HAL */
    if (HAL_TIM_PWM_Start(&timer->htim, map_channel(channel)) != HAL_OK) {
        return HAL_ERROR;
    }

    pwm_data[instance][channel].running = true;
    return HAL_OK;
}

hal_status_t hal_pwm_stop(hal_timer_instance_t instance,
                          hal_timer_channel_t channel) {
    timer_data_t* timer;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX || channel >= HAL_TIMER_CH_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    if (!pwm_data[instance][channel].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Stop PWM using ST HAL */
    if (HAL_TIM_PWM_Stop(&timer->htim, map_channel(channel)) != HAL_OK) {
        return HAL_ERROR;
    }

    pwm_data[instance][channel].running = false;
    return HAL_OK;
}

hal_status_t hal_pwm_set_duty(hal_timer_instance_t instance,
                              hal_timer_channel_t channel,
                              uint16_t duty_cycle) {
    timer_data_t* timer;
    uint32_t arr, ccr;

    /* Parameter validation */
    if (instance >= HAL_TIMER_MAX || channel >= HAL_TIMER_CH_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    if (duty_cycle > 10000) {
        return HAL_ERROR_INVALID_PARAM;
    }

    timer = &timer_data[instance];

    if (!timer->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    if (!pwm_data[instance][channel].initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Get current ARR value */
    arr = __HAL_TIM_GET_AUTORELOAD(&timer->htim);

    /* Calculate new CCR value */
    ccr = ((uint64_t)(arr + 1) * duty_cycle) / 10000UL;

    /* Set compare value using ST HAL macro */
    __HAL_TIM_SET_COMPARE(&timer->htim, map_channel(channel), ccr);

    /* Update stored duty cycle */
    pwm_data[instance][channel].duty_cycle = duty_cycle;

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementation                                             */
/*===========================================================================*/

/**
 * \brief           ST HAL Timer Period Elapsed Callback
 * \note            Called by HAL_TIM_IRQHandler() when update event occurs
 * \param[in]       htim: TIM handle pointer
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    hal_timer_instance_t instance;
    timer_data_t* timer;

    /* Find which timer triggered the callback */
    if (htim->Instance == TIM2) {
        instance = HAL_TIMER_0;
    } else if (htim->Instance == TIM3) {
        instance = HAL_TIMER_1;
    } else if (htim->Instance == TIM4) {
        instance = HAL_TIMER_2;
    } else if (htim->Instance == TIM5) {
        instance = HAL_TIMER_3;
    } else {
        return;
    }

    timer = &timer_data[instance];

    /* Handle one-shot mode */
    if (timer->config.mode == HAL_TIMER_MODE_ONESHOT) {
        /* Stop timer in one-shot mode */
        HAL_TIM_Base_Stop_IT(&timer->htim);
        timer->running = false;
    }

    /* Invoke user callback if registered */
    if (timer->callback != NULL) {
        timer->callback(instance, timer->context);
    }
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL Timer Handler                                  */
/*===========================================================================*/

/**
 * \brief           TIM2 IRQ Handler
 */
void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_data[HAL_TIMER_0].htim);
}

/**
 * \brief           TIM3 IRQ Handler
 */
void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_data[HAL_TIMER_1].htim);
}

/**
 * \brief           TIM4 IRQ Handler
 */
void TIM4_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_data[HAL_TIMER_2].htim);
}

/**
 * \brief           TIM5 IRQ Handler
 */
void TIM5_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_data[HAL_TIMER_3].htim);
}
