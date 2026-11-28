/**
 * \file            lp.h
 *
 * \brief           Bounded STM32F103 USB Device low-priority source owner.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_LP_H
#define NEXUS_STM32F1_USB_LP_H

#include "ep0.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Write filtered ISTR acknowledgment bits as a halfword. */
void nx_stm32f1_usb_istr_write(volatile uint16_t* status, uint16_t word);

/**
 * \brief Enable the selected RESET and CTR device-local interrupt sources.
 * \note The outer owner establishes NVIC priority and owns the LP vector.
 *       Pending RESET/CTR requires explicit service before rearming.
 */
nx_result_t nx_stm32f1_usb_lp_enable(const nx_stm32f1_ep0_t* endpoint);

/**
 * \brief Mask the selected raised sources without consuming events or loans.
 * \return True if this provider owned a live unmasked interrupt.
 * \note Suitable for the sole LP ISR. The task processes latched causes.
 */
bool nx_stm32f1_usb_lp_interrupt(USB_TypeDef* registers);

/** \brief Mask selected LP sources; CPU owner excludes top-half writes. */
void nx_stm32f1_usb_lp_mask(USB_TypeDef* registers);
bool nx_stm32f1_usb_lp_pending(const USB_TypeDef* registers);
/** \brief Rearm the selected masks, including newly pending events. */
nx_result_t nx_stm32f1_usb_lp_rearm(const nx_stm32f1_ep0_t* endpoint);

/**
 * \brief Process at most budget RESET or CTR causes while source is masked.
 * \note Serialized task with LP publisher joined for this operation. RESET
 *       wins over transfer completion. More causes or live loans can return
 *       BUSY. The caller explicitly rearms the source when appropriate.
 */
nx_result_t nx_stm32f1_usb_lp_service(nx_stm32f1_ep0_t* endpoint,
                                      nx_time_us_t now, uint8_t budget);

/**
 * \brief Deliver RESET or SETUP from exactly one observed source step.
 * \note EMPTY may still have advanced an owned packet. Other observations
 *       leave the caller output untouched until an event is available.
 */
nx_result_t nx_stm32f1_usb_lp_event(nx_stm32f1_ep0_t* endpoint,
                                    nx_time_us_t now, nx_usb_event_t* output);

/**
 * \brief Close EP0 after Board detach, NVIC join and actual peripheral reset.
 * \note Requires FRES held and every local interrupt source masked. This
 *       helper leaves the clock and physical attachment to the outer owner.
 */
nx_result_t nx_stm32f1_usb_lp_close(nx_stm32f1_ep0_t* endpoint);

#ifdef __cplusplus
}
#endif
#endif
