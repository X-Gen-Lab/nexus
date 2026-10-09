/**
 * \file            nx_mutex.c
 * \brief           Thread safety and mutex implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/system/nx_mutex.h"
#include "hal/system/nx_mem.h"
#include "osal/osal.h"
#include "arch/nx_arch.h"

/*---------------------------------------------------------------------------*/
/* Private Types                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mutex implementation structure
 */
typedef struct {
    nx_mutex_t base;            /**< Base interface (must be first) */
    osal_mutex_handle_t handle; /**< OSAL mutex handle */
} nx_mutex_impl_t;

/*---------------------------------------------------------------------------*/
/* Private Function Prototypes                                               */
/*---------------------------------------------------------------------------*/

#if NX_CONFIG_HAL_THREAD_SAFE
static nx_status_t mutex_lock(nx_mutex_t* self, uint32_t timeout_ms);
static nx_status_t mutex_unlock(nx_mutex_t* self);
static bool mutex_try_lock(nx_mutex_t* self);
#endif

/*---------------------------------------------------------------------------*/
/* Critical Section Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enter critical section (disable interrupts)
 * \details         Disables interrupts and returns previous state
 */
uint32_t nx_critical_enter(void) {
    return nx_arch_irq_save().value;
}

/**
 * \brief           Exit critical section (restore interrupts)
 * \details         Restores interrupt state from saved primask value
 */
void nx_critical_exit(uint32_t primask) {
    nx_arch_irq_restore((nx_arch_irq_state_t){primask});
}

#if NX_CONFIG_HAL_THREAD_SAFE

/*---------------------------------------------------------------------------*/
/* Mutex Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Create a new mutex
 * \details         Allocates and initializes a mutex using OSAL backend
 */
nx_mutex_t* nx_mutex_create(void) {
    nx_mutex_impl_t* impl =
        (nx_mutex_impl_t*)nx_mem_alloc(sizeof(nx_mutex_impl_t));
    if (impl == NULL) {
        return NULL;
    }

    /* Create OSAL mutex */
    osal_status_t status = osal_mutex_create(&impl->handle);
    if (status != OSAL_OK) {
        nx_mem_free(impl);
        return NULL;
    }

    /* Initialize interface */
    impl->base.lock = mutex_lock;
    impl->base.unlock = mutex_unlock;
    impl->base.try_lock = mutex_try_lock;

    return &impl->base;
}

/**
 * \brief           Destroy a mutex
 * \details         Releases OSAL mutex and frees memory
 */
nx_status_t nx_mutex_destroy(nx_mutex_t* mutex) {
    if (mutex == NULL) {
        return NX_OK;
    }

    nx_mutex_impl_t* impl = NX_CONTAINER_OF(mutex, nx_mutex_impl_t, base);

    /* Delete OSAL mutex */
    if (osal_mutex_delete(impl->handle) != OSAL_OK) {
        /* The owner must release a held mutex before destroying its wrapper. */
        return NX_ERR_BUSY;
    }

    /* Free memory */
    nx_mem_free(impl);
    return NX_OK;
}

/**
 * \brief           Lock mutex implementation
 */
static nx_status_t mutex_lock(nx_mutex_t* self, uint32_t timeout_ms) {
    nx_mutex_impl_t* impl = NX_CONTAINER_OF(self, nx_mutex_impl_t, base);

    osal_status_t status = osal_mutex_lock(impl->handle, timeout_ms);

    switch (status) {
        case OSAL_OK:
            return NX_OK;
        case OSAL_ERROR_TIMEOUT:
            return NX_ERR_TIMEOUT;
        default:
            return NX_ERR_INVALID_PARAM;
    }
}

/**
 * \brief           Unlock mutex implementation
 */
static nx_status_t mutex_unlock(nx_mutex_t* self) {
    nx_mutex_impl_t* impl = NX_CONTAINER_OF(self, nx_mutex_impl_t, base);

    osal_status_t status = osal_mutex_unlock(impl->handle);

    return (status == OSAL_OK) ? NX_OK : NX_ERR_INVALID_PARAM;
}

/**
 * \brief           Try lock mutex implementation
 */
static bool mutex_try_lock(nx_mutex_t* self) {
    nx_mutex_impl_t* impl = NX_CONTAINER_OF(self, nx_mutex_impl_t, base);

    /* Try to lock with zero timeout */
    osal_status_t status = osal_mutex_lock(impl->handle, 0);

    return (status == OSAL_OK);
}

#else
nx_mutex_t* nx_mutex_create(void) { return NULL; }
nx_status_t nx_mutex_destroy(nx_mutex_t* mutex) {
    return mutex ? NX_ERR_NOT_SUPPORTED : NX_OK;
}
#endif

/*---------------------------------------------------------------------------*/
/* Atomic Operations                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Load atomic value
 * \details         Performs atomic read with memory barrier
 */
uint32_t nx_atomic_load(nx_atomic_t* atomic) {
    if (atomic == NULL) {
        return 0;
    }

    uint32_t value;
    uint32_t saved = nx_critical_enter();
    value = atomic->value;
    nx_critical_exit(saved);

    return value;
}

/**
 * \brief           Store atomic value
 * \details         Performs atomic write with memory barrier
 */
void nx_atomic_store(nx_atomic_t* atomic, uint32_t value) {
    if (atomic == NULL) {
        return;
    }

    uint32_t saved = nx_critical_enter();
    atomic->value = value;
    nx_critical_exit(saved);
}

/**
 * \brief           Atomic compare and exchange
 * \details         Atomically compares and exchanges value if equal
 */
bool nx_atomic_compare_exchange(nx_atomic_t* atomic, uint32_t* expected,
                                uint32_t desired) {
    if (atomic == NULL || expected == NULL) {
        return false;
    }

    bool success = false;

    uint32_t saved = nx_critical_enter();
    if (atomic->value == *expected) {
        atomic->value = desired;
        success = true;
    } else {
        *expected = atomic->value;
    }
    nx_critical_exit(saved);

    return success;
}

/**
 * \brief           Atomic fetch and add
 * \details         Atomically adds value and returns previous value
 */
uint32_t nx_atomic_fetch_add(nx_atomic_t* atomic, uint32_t value) {
    if (atomic == NULL) {
        return 0;
    }

    uint32_t old_value;

    uint32_t saved = nx_critical_enter();
    old_value = atomic->value;
    atomic->value += value;
    nx_critical_exit(saved);

    return old_value;
}
