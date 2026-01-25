/**
 * \file            nx_mutex.h
 * \brief           Thread safety and mutex interface
 * \author          Nexus Team
 */

#ifndef NX_MUTEX_H
#define NX_MUTEX_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include "nexus_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mutex handle type
 */
typedef struct nx_mutex_s nx_mutex_t;

/**
 * \brief           Mutex operations interface
 */
struct nx_mutex_s {
    nx_status_t (*lock)(nx_mutex_t* self, uint32_t timeout_ms);
    nx_status_t (*unlock)(nx_mutex_t* self);
    bool (*try_lock)(nx_mutex_t* self);
};

/**
 * \brief           Atomic type
 */
typedef struct nx_atomic_s {
    volatile uint32_t value;
} nx_atomic_t;

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Wait forever timeout value
 */
#define NX_WAIT_FOREVER 0xFFFFFFFFU

/**
 * \brief           Critical section macros
 */
#if NX_CONFIG_HAL_THREAD_SAFE
#define NX_CRITICAL_ENTER() uint32_t _primask = nx_critical_enter()
#define NX_CRITICAL_EXIT()  nx_critical_exit(_primask)
#define NX_LOCK(_mutex)     (_mutex)->lock((_mutex), NX_WAIT_FOREVER)
#define NX_UNLOCK(_mutex)   (_mutex)->unlock((_mutex))
#else
#define NX_CRITICAL_ENTER() ((void)0)
#define NX_CRITICAL_EXIT()  ((void)0)
#define NX_LOCK(_mutex)     NX_OK
#define NX_UNLOCK(_mutex)   NX_OK
#endif

/*---------------------------------------------------------------------------*/
/* Function Declarations                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enter critical section (disable interrupts)
 * \return          Previous interrupt state (primask value)
 */
uint32_t nx_critical_enter(void);

/**
 * \brief           Exit critical section (restore interrupts)
 * \param[in]       primask: Previous interrupt state to restore
 */
void nx_critical_exit(uint32_t primask);

/**
 * \brief           Create a new mutex
 * \return          Pointer to created mutex, or NULL on failure
 */
nx_mutex_t* nx_mutex_create(void);

/**
 * \brief           Destroy a mutex
 * \param[in]       mutex: Mutex to destroy
 */
void nx_mutex_destroy(nx_mutex_t* mutex);

/**
 * \brief           Load atomic value
 * \param[in]       atomic: Atomic variable pointer
 * \return          Current value
 */
uint32_t nx_atomic_load(nx_atomic_t* atomic);

/**
 * \brief           Store atomic value
 * \param[in]       atomic: Atomic variable pointer
 * \param[in]       value: Value to store
 */
void nx_atomic_store(nx_atomic_t* atomic, uint32_t value);

/**
 * \brief           Atomic compare and exchange
 * \param[in]       atomic: Atomic variable pointer
 * \param[in,out]   expected: Expected value pointer (updated with actual value
 * if exchange fails)
 * \param[in]       desired: Desired value to store
 * \return          true if exchange succeeded, false otherwise
 */
bool nx_atomic_compare_exchange(nx_atomic_t* atomic, uint32_t* expected,
                                uint32_t desired);

/**
 * \brief           Atomic fetch and add
 * \param[in]       atomic: Atomic variable pointer
 * \param[in]       value: Value to add
 * \return          Previous value before addition
 */
uint32_t nx_atomic_fetch_add(nx_atomic_t* atomic, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* NX_MUTEX_H */
