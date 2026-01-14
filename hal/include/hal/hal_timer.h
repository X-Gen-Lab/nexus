/**
 * \file            hal_timer.h
 * \brief           HAL Timer Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "hal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_TIMER Timer Hardware Abstraction
 * \brief           Timer interface for hardware abstraction
 * \{
 */

/**
 * \brief           Timer instance enumeration
 */
typedef enum {
    HAL_TIMER_0 = 0, /**< Timer instance 0 */
    HAL_TIMER_1,     /**< Timer instance 1 */
    HAL_TIMER_2,     /**< Timer instance 2 */
    HAL_TIMER_3,     /**< Timer instance 3 */
    HAL_TIMER_MAX    /**< Maximum timer count */
} hal_timer_instance_t;

/**
 * \brief           Timer mode
 */
typedef enum {
    HAL_TIMER_MODE_ONESHOT = 0, /**< One-shot mode */
    HAL_TIMER_MODE_PERIODIC     /**< Periodic mode */
} hal_timer_mode_t;

/**
 * \brief           Timer direction
 */
typedef enum {
    HAL_TIMER_DIR_UP = 0, /**< Count up */
    HAL_TIMER_DIR_DOWN    /**< Count down */
} hal_timer_dir_t;

/**
 * \brief           PWM channel
 */
typedef enum {
    HAL_TIMER_CH_1 = 0, /**< Channel 1 */
    HAL_TIMER_CH_2,     /**< Channel 2 */
    HAL_TIMER_CH_3,     /**< Channel 3 */
    HAL_TIMER_CH_4,     /**< Channel 4 */
    HAL_TIMER_CH_MAX    /**< Maximum channel count */
} hal_timer_channel_t;

/**
 * \brief           Timer configuration structure
 */
typedef struct {
    uint32_t period_us;        /**< Period in microseconds */
    hal_timer_mode_t mode;     /**< Timer mode */
    hal_timer_dir_t direction; /**< Count direction */
} hal_timer_config_t;

/**
 * \brief           PWM configuration structure
 */
typedef struct {
    uint32_t frequency;  /**< PWM frequency in Hz */
    uint16_t duty_cycle; /**< Duty cycle (0-10000 = 0-100.00%) */
} hal_pwm_config_t;

/**
 * \brief           Timer callback function type
 * \param[in]       instance: Timer instance
 * \param[in]       context: User context
 */
typedef void (*hal_timer_callback_t)(hal_timer_instance_t instance,
                                     void* context);

/**
 * \brief           Initialize timer
 * \param[in]       instance: Timer instance
 * \param[in]       config: Pointer to configuration structure
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_init(hal_timer_instance_t instance,
                            const hal_timer_config_t* config);

/**
 * \brief           Deinitialize timer
 * \param[in]       instance: Timer instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_deinit(hal_timer_instance_t instance);

/**
 * \brief           Start timer
 * \param[in]       instance: Timer instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_start(hal_timer_instance_t instance);

/**
 * \brief           Stop timer
 * \param[in]       instance: Timer instance
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_stop(hal_timer_instance_t instance);

/**
 * \brief           Get current counter value
 * \param[in]       instance: Timer instance
 * \param[out]      count: Pointer to store counter value
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_get_count(hal_timer_instance_t instance,
                                 uint32_t* count);

/**
 * \brief           Set counter value
 * \param[in]       instance: Timer instance
 * \param[in]       count: Counter value to set
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_set_count(hal_timer_instance_t instance, uint32_t count);

/**
 * \brief           Register timer callback
 * \param[in]       instance: Timer instance
 * \param[in]       callback: Callback function
 * \param[in]       context: User context
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_timer_set_callback(hal_timer_instance_t instance,
                                    hal_timer_callback_t callback,
                                    void* context);

/**
 * \brief           Initialize PWM
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \param[in]       config: Pointer to PWM configuration
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_pwm_init(hal_timer_instance_t instance,
                          hal_timer_channel_t channel,
                          const hal_pwm_config_t* config);

/**
 * \brief           Start PWM output
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_pwm_start(hal_timer_instance_t instance,
                           hal_timer_channel_t channel);

/**
 * \brief           Stop PWM output
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_pwm_stop(hal_timer_instance_t instance,
                          hal_timer_channel_t channel);

/**
 * \brief           Set PWM duty cycle
 * \param[in]       instance: Timer instance
 * \param[in]       channel: PWM channel
 * \param[in]       duty_cycle: Duty cycle (0-10000 = 0-100.00%)
 * \return          HAL_OK on success, error code otherwise
 */
hal_status_t hal_pwm_set_duty(hal_timer_instance_t instance,
                              hal_timer_channel_t channel, uint16_t duty_cycle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_H */
