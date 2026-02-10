/**
 * \file            nx_isr_manager.h
 * \brief           Interrupt service routine manager interface (simplified)
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
 * \brief           ISR manager interface (simplified)
 */
typedef struct nx_isr_manager_s nx_isr_manager_t;
struct nx_isr_manager_s {
    /**
     * \brief           Connect ISR callback to interrupt (one-step setup)
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \param[in]       func: Callback function
     * \param[in]       data: User data pointer
     * \param[in]       priority: Hardware priority (0-15, lower is higher)
     * \return          NX_OK on success, error code otherwise
     * \note            Automatically clears pending, sets priority, and
     *                  enables interrupt. Only one callback per interrupt.
     */
    nx_status_t (*connect)(nx_isr_manager_t* self, uint32_t irq,
                           nx_isr_func_t func, void* data, uint8_t priority);

    /**
     * \brief           Disconnect ISR callback
     * \param[in]       self: ISR manager instance
     * \param[in]       irq: IRQ number
     * \return          NX_OK on success, error code otherwise
     * \note            Automatically disables interrupt.
     */
    nx_status_t (*disconnect)(nx_isr_manager_t* self, uint32_t irq);
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
