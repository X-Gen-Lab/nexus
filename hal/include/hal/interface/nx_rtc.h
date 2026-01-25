/**
 * \file            nx_rtc.h
 * \brief           RTC (Real-Time Clock) interface definition
 * \author          Nexus Team
 */

#ifndef NX_RTC_H
#define NX_RTC_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Date Time Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Date and time structure
 *
 * Represents calendar date and time for RTC operations.
 * Year is the full year (e.g., 2026), month is 1-12, day is 1-31,
 * hour is 0-23, minute is 0-59, second is 0-59.
 */
typedef struct nx_datetime_s {
    uint16_t year;  /**< Year (e.g., 2026) */
    uint8_t month;  /**< Month (1-12) */
    uint8_t day;    /**< Day of month (1-31) */
    uint8_t hour;   /**< Hour (0-23) */
    uint8_t minute; /**< Minute (0-59) */
    uint8_t second; /**< Second (0-59) */
} nx_datetime_t;

/*---------------------------------------------------------------------------*/
/* RTC Alarm Callback                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC alarm callback function type
 * \param[in]       user_data: User data pointer passed during registration
 */
typedef void (*nx_rtc_alarm_callback_t)(void* user_data);

/*---------------------------------------------------------------------------*/
/* RTC Interface                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RTC (Real-Time Clock) interface
 *
 * Provides access to real-time clock functionality for timekeeping
 * across power cycles. Supports Unix timestamp and calendar date/time
 * operations, alarm configuration, and lifecycle/power management.
 */
typedef struct nx_rtc_s nx_rtc_t;
struct nx_rtc_s {
    /**
     * \brief           Set Unix timestamp
     * \param[in]       self: RTC interface pointer
     * \param[in]       timestamp: Unix timestamp (seconds since 1970-01-01
     *                  00:00:00 UTC)
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_timestamp)(nx_rtc_t* self, uint32_t timestamp);

    /**
     * \brief           Get Unix timestamp
     * \param[in]       self: RTC interface pointer
     * \return          Current Unix timestamp
     */
    uint32_t (*get_timestamp)(nx_rtc_t* self);

    /**
     * \brief           Set date and time
     * \param[in]       self: RTC interface pointer
     * \param[in]       dt: Pointer to date/time structure
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_datetime)(nx_rtc_t* self, const nx_datetime_t* dt);

    /**
     * \brief           Get date and time
     * \param[in]       self: RTC interface pointer
     * \param[out]      dt: Pointer to date/time structure to fill
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*get_datetime)(nx_rtc_t* self, nx_datetime_t* dt);

    /**
     * \brief           Set alarm with callback
     * \param[in]       self: RTC interface pointer
     * \param[in]       alarm: Pointer to alarm date/time structure
     * \param[in]       callback: Callback function to invoke when alarm
     *                  triggers
     * \param[in]       user_data: User data passed to callback
     * \return          NX_OK on success, error code otherwise
     * \note            Pass NULL callback to disable alarm
     */
    nx_status_t (*set_alarm)(nx_rtc_t* self, const nx_datetime_t* alarm,
                             nx_rtc_alarm_callback_t callback, void* user_data);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: RTC interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_rtc_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: RTC interface pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_rtc_t* self);
};

/*---------------------------------------------------------------------------*/
/* RTC Initialization Macro                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize RTC interface
 * \param[in]       p: Pointer to nx_rtc_t structure
 * \param[in]       _set_timestamp: Set timestamp function pointer
 * \param[in]       _get_timestamp: Get timestamp function pointer
 * \param[in]       _set_datetime: Set datetime function pointer
 * \param[in]       _get_datetime: Get datetime function pointer
 * \param[in]       _set_alarm: Set alarm function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 */
#define NX_INIT_RTC(p, _set_timestamp, _get_timestamp, _set_datetime,          \
                    _get_datetime, _set_alarm, _get_lifecycle, _get_power)     \
    do {                                                                       \
        (p)->set_timestamp = (_set_timestamp);                                 \
        (p)->get_timestamp = (_get_timestamp);                                 \
        (p)->set_datetime = (_set_datetime);                                   \
        (p)->get_datetime = (_get_datetime);                                   \
        (p)->set_alarm = (_set_alarm);                                         \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        NX_ASSERT((p)->set_timestamp != NULL);                                 \
        NX_ASSERT((p)->get_timestamp != NULL);                                 \
        NX_ASSERT((p)->set_datetime != NULL);                                  \
        NX_ASSERT((p)->get_datetime != NULL);                                  \
        NX_ASSERT((p)->set_alarm != NULL);                                     \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_RTC_H */
