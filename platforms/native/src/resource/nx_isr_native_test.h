/**
 * \file            nx_isr_native_test.h
 * \brief           Native platform ISR manager test interfaces
 * \author          Nexus Team
 */

#ifndef NX_ISR_NATIVE_TEST_H
#define NX_ISR_NATIVE_TEST_H

#include "hal/nx_types.h"
#include "hal/resource/nx_isr_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Test Interface Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate ISR dispatch
 * \param[in]       irq: IRQ number to simulate
 * \details         Calls all registered callbacks for the IRQ
 */
void nx_isr_simulate(uint32_t irq);

/**
 * \brief           Get number of registered callbacks for an IRQ
 * \param[in]       irq: IRQ number
 * \return          Number of registered callbacks
 */
uint8_t nx_isr_test_get_callback_count(uint32_t irq);

/**
 * \brief           Check if an IRQ is enabled
 * \param[in]       irq: IRQ number
 * \return          true if enabled, false otherwise
 */
bool nx_isr_test_is_enabled(uint32_t irq);

/**
 * \brief           Reset all ISR registrations
 * \details         Clears all callback chains and resets state
 */
void nx_isr_test_reset_all(void);

/**
 * \brief           Get total number of active handles
 * \return          Number of active handles
 */
uint32_t nx_isr_test_get_active_handle_count(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_ISR_NATIVE_TEST_H */
