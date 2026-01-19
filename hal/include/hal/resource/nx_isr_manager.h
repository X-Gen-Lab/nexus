/**
 * \file            nx_isr_manager.h
 * \brief           Interrupt service routine manager interface
 * \author          Nexus Team
 */

#ifndef NX_ISR_MANAGER_H
#define NX_ISR_MANAGER_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           ISR callback priority enumeration
 */
typedef enum nx_isr_priority_e {
    NX_ISR_PRIORITY_HIGHEST = 0, /**< Highest priority */
    NX_ISR_PRIORITY_HIGH = 1,    /**< High priority */
    NX_ISR_PRIORITY_NORMAL = 2,  /**< Normal priority */
    NX_ISR_PRIORITY_LOW = 3,     /**< Low priority */
} nx_isr_priority_t;

/**
 * \brief           ISR callback handle (opaque)
 */
typedef struct nx_isr_handle_s nx_isr_handle_t;

/**
 * \brief           ISR handler function type
 * \param[in]       user_data: User data pointer
 */
typedef void (*nx_isr_handler_t)(void* user_data);

/**
 * \brief           ISR callback function type (alias for nx_isr_handler_t)
 * \param[in]       data: User data pointer
 */
typedef nx_isr_handler_t nx_isr_func_t;

/**
 * \brief           ISR manager interface
 */
typedef struct nx_isr_manager_s nx_isr_manager_t;
struct nx_isr_manager_s {
    /**
     * \brief           Connect ISR callback to interrupt
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \param[in]       func: Callback function
     * \param[in]       data: User data pointer
     * \param[in]       priority: Callback priority
     * \return          ISR handle, NULL on failure
     */
    nx_isr_handle_t* (*connect)(nx_isr_manager_t* self, uint32_t irq,
                                nx_isr_func_t func, void* data,
                                nx_isr_priority_t priority);

    /**
     * \brief           Disconnect ISR callback
     * \param[in]       self: ISR manager instance
     * \param[in]       handle: ISR handle
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*disconnect)(nx_isr_manager_t* self, nx_isr_handle_t* handle);

    /**
     * \brief           Set interrupt priority
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \param[in]       priority: Hardware priority (0-15, lower is higher
     * priority)
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_priority)(nx_isr_manager_t* self, uint32_t irq,
                                uint8_t priority);

    /**
     * \brief           Enable interrupt
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*enable)(nx_isr_manager_t* self, uint32_t irq);

    /**
     * \brief           Disable interrupt
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*disable)(nx_isr_manager_t* self, uint32_t irq);
};

/**
 * \brief           Get ISR manager singleton instance
 * \return          ISR manager pointer
 */
nx_isr_manager_t* nx_isr_manager_get(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_ISR_MANAGER_H */
