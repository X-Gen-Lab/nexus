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

static nx_status_t mutex_lock(nx_mutex_t* self, uint32_t timeout_ms);
static nx_status_t mutex_unlock(nx_mutex_t* self);
static bool mutex_try_lock(nx_mutex_t* self);

/*---------------------------------------------------------------------------*/
/* Critical Section Functions                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enter critical section (disable interrupts)
 * \details         Disables interrupts and returns previous state
 */
uint32_t nx_critical_enter(void) {
    /* For ARM Cortex-M, we need to save and return PRIMASK */
    uint32_t primask;

#if (defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) ||                      \
     defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) ||              \
     defined(__ARM_ARCH_8M_MAIN__)) &&                                        \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ volatile("mrs %0, primask" : "=r"(primask));
    __asm__ volatile("cpsid i\n\tdsb\n\tisb" ::: "memory");
#elif defined(__ICCARM__) && defined(__ARM_ARCH_PROFILE) &&                    \
    (__ARM_ARCH_PROFILE == 'M')
    primask = __get_PRIMASK();
    __disable_interrupt();
#elif defined(__CC_ARM) && defined(__TARGET_ARCH_THUMB)
    primask = __get_PRIMASK();
    __disable_irq();
#else
    /* Fallback to OSAL for other platforms */
    osal_enter_critical();
    primask = 0;
#endif

    return primask;
}

/**
 * \brief           Exit critical section (restore interrupts)
 * \details         Restores interrupt state from saved primask value
 */
void nx_critical_exit(uint32_t primask) {
#if (defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) ||                      \
     defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) ||              \
     defined(__ARM_ARCH_8M_MAIN__)) &&                                        \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ volatile("dsb\n\tmsr primask, %0\n\tisb" ::"r"(primask) : "memory");
#elif defined(__ICCARM__) && defined(__ARM_ARCH_PROFILE) &&                    \
    (__ARM_ARCH_PROFILE == 'M')
    __set_PRIMASK(primask);
#elif defined(__CC_ARM) && defined(__TARGET_ARCH_THUMB)
    __set_PRIMASK(primask);
#else
    /* Fallback to OSAL for other platforms */
    (void)primask;
    osal_exit_critical();
#endif
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
