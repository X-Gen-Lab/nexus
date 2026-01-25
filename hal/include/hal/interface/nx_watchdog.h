/**
 * \file            nx_watchdog.h
 * \brief           Watchdog interface definition
 * \author          Nexus Team
 */

#ifndef NX_WATCHDOG_H
#define NX_WATCHDOG_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Watchdog Callback                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog early warning callback function type
 * \param[in]       user_data: User data pointer passed during registration
 *
 * This callback is invoked when the watchdog early warning interrupt fires,
 * giving the application a chance to take action before the watchdog resets
 * the system.
 */
typedef void (*nx_watchdog_callback_t)(void* user_data);

/*---------------------------------------------------------------------------*/
/* Watchdog Interface                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Watchdog interface
 *
 * Provides access to hardware watchdog functionality for system reliability.
 * The watchdog timer must be periodically fed (reset) to prevent system reset.
 * Supports early warning callback for graceful handling before reset.
 */
typedef struct nx_watchdog_s nx_watchdog_t;
struct nx_watchdog_s {
    /**
     * \brief           Start watchdog timer
     * \param[in]       self: Watchdog interface pointer
     * \return          NX_OK on success, error code otherwise
     * \note            Once started, some watchdog implementations cannot be
     * stopped
     */
    nx_status_t (*start)(nx_watchdog_t* self);

    /**
     * \brief           Stop watchdog timer
     * \param[in]       self: Watchdog interface pointer
     * \return          NX_OK on success, NX_ERR_NOT_SUPPORTED if stop not
     *                  supported
     * \note            Many hardware watchdogs cannot be stopped once started
     */
    nx_status_t (*stop)(nx_watchdog_t* self);

    /**
     * \brief           Feed (refresh) watchdog timer
     * \param[in]       self: Watchdog interface pointer
     *
     * Resets the watchdog counter to prevent system reset.
     * Must be called periodically within the timeout window.
     */
    void (*feed)(nx_watchdog_t* self);

    /**
     * \brief           Get watchdog timeout value
     * \param[in]       self: Watchdog interface pointer
     * \return          Timeout value in milliseconds
     */
    uint32_t (*get_timeout)(nx_watchdog_t* self);

    /**
     * \brief           Set early warning callback
     * \param[in]       self: Watchdog interface pointer
     * \param[in]       callback: Callback function for early warning interrupt
     * \param[in]       user_data: User data passed to callback
     * \return          NX_OK on success, NX_ERR_NOT_SUPPORTED if not available
     * \note            Pass NULL callback to disable early warning
     */
    nx_status_t (*set_callback)(nx_watchdog_t* self,
                                nx_watchdog_callback_t callback,
                                void* user_data);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Watchdog interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_watchdog_t* self);
};

/*---------------------------------------------------------------------------*/
/* Watchdog Initialization Macro                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize watchdog interface
 * \param[in]       p: Pointer to nx_watchdog_t structure
 * \param[in]       _start: Start function pointer
 * \param[in]       _stop: Stop function pointer
 * \param[in]       _feed: Feed function pointer
 * \param[in]       _get_timeout: Get timeout function pointer
 * \param[in]       _set_callback: Set callback function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 */
#define NX_INIT_WATCHDOG(p, _start, _stop, _feed, _get_timeout, _set_callback, \
                         _get_lifecycle)                                       \
    do {                                                                       \
        (p)->start = (_start);                                                 \
        (p)->stop = (_stop);                                                   \
        (p)->feed = (_feed);                                                   \
        (p)->get_timeout = (_get_timeout);                                     \
        (p)->set_callback = (_set_callback);                                   \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        NX_ASSERT((p)->start != NULL);                                         \
        NX_ASSERT((p)->stop != NULL);                                          \
        NX_ASSERT((p)->feed != NULL);                                          \
        NX_ASSERT((p)->get_timeout != NULL);                                   \
        NX_ASSERT((p)->set_callback != NULL);                                  \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_WATCHDOG_H */
