/**
 * \file            nx_timer.h
 * \brief           Timer device interface definition
 * \author          Nexus Team
 */

#ifndef NX_TIMER_H
#define NX_TIMER_H

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
 * \brief           Timer callback function type
 * \param[in]       user_data: User context pointer
 */
typedef void (*nx_timer_callback_t)(void* user_data);

/*---------------------------------------------------------------------------*/
/* Timer Base Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Timer base timing interface
 */
typedef struct nx_timer_base_s nx_timer_base_t;
struct nx_timer_base_s {
    /**
     * \brief           Start timer
     * \param[in]       self: Interface pointer
     */
    void (*start)(nx_timer_base_t* self);

    /**
     * \brief           Stop timer
     * \param[in]       self: Interface pointer
     */
    void (*stop)(nx_timer_base_t* self);

    /**
     * \brief           Set timer period
     * \param[in]       self: Interface pointer
     * \param[in]       prescaler: Prescaler value
     * \param[in]       period: Period value
     */
    void (*set_period)(nx_timer_base_t* self, uint16_t prescaler,
                       uint32_t period);

    /**
     * \brief           Get timer counter value
     * \param[in]       self: Interface pointer
     * \return          Current counter value
     */
    uint32_t (*get_count)(nx_timer_base_t* self);

    /**
     * \brief           Set timer callback
     * \param[in]       self: Interface pointer
     * \param[in]       callback: Callback function
     * \param[in]       user_data: User context pointer
     * \return          NX_OK on success
     */
    nx_status_t (*set_callback)(nx_timer_base_t* self,
                                nx_timer_callback_t callback, void* user_data);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_timer_base_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: Interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_timer_base_t* self);
};

/*---------------------------------------------------------------------------*/
/* PWM Channel Interface                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           PWM channel interface
 */
typedef struct nx_timer_pwm_channel_s nx_timer_pwm_channel_t;
struct nx_timer_pwm_channel_s {
    /**
     * \brief           Set PWM duty cycle
     * \param[in]       self: Interface pointer
     * \param[in]       duty: Duty cycle value (0-period)
     */
    void (*set_duty)(nx_timer_pwm_channel_t* self, uint32_t duty);
};

/*---------------------------------------------------------------------------*/
/* PWM Controller Interface                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           PWM controller interface
 */
typedef struct nx_timer_pwm_ctrl_s nx_timer_pwm_ctrl_t;
struct nx_timer_pwm_ctrl_s {
    /**
     * \brief           Start PWM controller
     * \param[in]       self: Interface pointer
     */
    void (*start)(nx_timer_pwm_ctrl_t* self);

    /**
     * \brief           Stop PWM controller
     * \param[in]       self: Interface pointer
     */
    void (*stop)(nx_timer_pwm_ctrl_t* self);

    /**
     * \brief           Set PWM period
     * \param[in]       self: Interface pointer
     * \param[in]       prescaler: Prescaler value
     * \param[in]       period: Period value
     */
    void (*set_period)(nx_timer_pwm_ctrl_t* self, uint16_t prescaler,
                       uint32_t period);
};

/*---------------------------------------------------------------------------*/
/* PWM Output Interface                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           PWM output interface
 */
typedef struct nx_timer_pwm_s nx_timer_pwm_t;
struct nx_timer_pwm_s {
    /**
     * \brief           Get PWM channel interface
     * \param[in]       self: Interface pointer
     * \param[in]       channel_index: Channel index
     * \return          PWM channel interface pointer
     */
    nx_timer_pwm_channel_t* (*get_channel)(nx_timer_pwm_t* self,
                                           uint8_t channel_index);

    /**
     * \brief           Get PWM controller interface
     * \param[in]       self: Interface pointer
     * \return          PWM controller interface pointer
     */
    nx_timer_pwm_ctrl_t* (*get_controller)(nx_timer_pwm_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_timer_pwm_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: Interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_timer_pwm_t* self);
};

/*---------------------------------------------------------------------------*/
/* Encoder Interface                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Encoder interface
 */
typedef struct nx_timer_encoder_s nx_timer_encoder_t;
struct nx_timer_encoder_s {
    /**
     * \brief           Get encoder count
     * \param[in]       self: Interface pointer
     * \return          Current encoder count (signed for direction)
     */
    int64_t (*get_count)(nx_timer_encoder_t* self);

    /**
     * \brief           Reset encoder count
     * \param[in]       self: Interface pointer
     */
    void (*reset)(nx_timer_encoder_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_timer_encoder_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: Interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_timer_encoder_t* self);
};

#ifdef __cplusplus
}
#endif

#endif /* NX_TIMER_H */
