/**
 * \file            nx_timer.h
 * \brief           Timer device interface definition
 * \author          Nexus Team
 */

#ifndef NX_TIMER_H
#define NX_TIMER_H

#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Timer mode enumeration
 */
typedef enum nx_timer_mode_e {
    NX_TIMER_MODE_ONE_SHOT = 0,   /**< One-shot mode */
    NX_TIMER_MODE_PERIODIC,       /**< Periodic mode */
    NX_TIMER_MODE_PWM,            /**< PWM mode */
    NX_TIMER_MODE_INPUT_CAPTURE,  /**< Input capture mode */
    NX_TIMER_MODE_OUTPUT_COMPARE, /**< Output compare mode */
} nx_timer_mode_t;

/**
 * \brief           Timer configuration structure
 */
typedef struct nx_timer_config_s {
    nx_timer_mode_t mode;  /**< Timer mode */
    uint32_t frequency_hz; /**< Timer frequency in Hz */
    uint32_t period_us;    /**< Timer period in microseconds */
    bool auto_reload;      /**< Auto-reload flag */
    uint8_t prescaler;     /**< Prescaler value */
} nx_timer_config_t;

/**
 * \brief           PWM configuration structure
 */
typedef struct nx_pwm_config_s {
    uint32_t frequency_hz; /**< PWM frequency in Hz */
    uint8_t duty_cycle;    /**< Duty cycle: 0-100 */
    uint8_t channel;       /**< PWM channel */
    bool inverted;         /**< Inverted output flag */
} nx_pwm_config_t;

/**
 * \brief           Timer statistics structure
 */
typedef struct nx_timer_stats_s {
    bool running;            /**< Running flag */
    uint32_t overflow_count; /**< Overflow count */
    uint32_t capture_count;  /**< Capture event count */
    uint32_t compare_count;  /**< Compare event count */
} nx_timer_stats_t;

/**
 * \brief           Timer callback function type
 * \param[in]       context: User context pointer
 */
typedef void (*nx_timer_callback_t)(void* context);

/**
 * \brief           Timer device interface
 */
typedef struct nx_timer_s nx_timer_t;
struct nx_timer_s {
    /* Basic timer operations */
    nx_status_t (*start)(nx_timer_t* self);
    nx_status_t (*stop)(nx_timer_t* self);
    nx_status_t (*reset)(nx_timer_t* self);
    uint32_t (*get_counter)(nx_timer_t* self);
    nx_status_t (*set_counter)(nx_timer_t* self, uint32_t value);

    /* Timer callback */
    nx_status_t (*set_callback)(nx_timer_t* self, nx_timer_callback_t cb,
                                void* ctx);
    nx_status_t (*clear_callback)(nx_timer_t* self);

    /* PWM operations */
    nx_status_t (*pwm_start)(nx_timer_t* self, uint8_t channel);
    nx_status_t (*pwm_stop)(nx_timer_t* self, uint8_t channel);
    nx_status_t (*pwm_set_duty_cycle)(nx_timer_t* self, uint8_t channel,
                                      uint8_t duty_cycle);
    nx_status_t (*pwm_get_config)(nx_timer_t* self, uint8_t channel,
                                  nx_pwm_config_t* cfg);
    nx_status_t (*pwm_set_config)(nx_timer_t* self, uint8_t channel,
                                  const nx_pwm_config_t* cfg);

    /* Runtime configuration */
    nx_status_t (*set_frequency)(nx_timer_t* self, uint32_t frequency_hz);
    nx_status_t (*set_period)(nx_timer_t* self, uint32_t period_us);
    nx_status_t (*get_config)(nx_timer_t* self, nx_timer_config_t* cfg);
    nx_status_t (*set_config)(nx_timer_t* self, const nx_timer_config_t* cfg);

    /* Base interfaces */
    nx_lifecycle_t* (*get_lifecycle)(nx_timer_t* self);
    nx_power_t* (*get_power)(nx_timer_t* self);
    nx_diagnostic_t* (*get_diagnostic)(nx_timer_t* self);

    /* Diagnostics */
    nx_status_t (*get_stats)(nx_timer_t* self, nx_timer_stats_t* stats);
    nx_status_t (*clear_stats)(nx_timer_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_TIMER_H */
