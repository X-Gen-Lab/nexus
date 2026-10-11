/**
 * \file            gd32f470_can.h
 * \brief           Private GD32F470 classic CAN static resource binding
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_CAN_H
#define NEXUS_GD32F470_CAN_H
#include "can_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Bind the shared primary-clock and immutable filter-bank domain.
 * \note Cold privileged task owner, before either controller starts. Board
 *       routes, transceiver electrical authorization and pinmux are supplied
 *       by reviewed assembly. Failure retains explicit domain responsibility.
 */
nx_result_t nx_gd32_can_domain_initialize(nx_can_filter_domain_t* domain);
/**
 * \brief Bind exact hardware/IRQ/APB1 facts and start the selected controller.
 * \note Static caller state and ring outlive successful stop; no allocation.
 *       Index 0/1 maps to CAN0/1; pins are already safely assembled.
 *       Stop never resets a peer. Release domain clocks with the explicit
 *       domain stop after both controller stops, then release Board pins.
 */
nx_result_t nx_gd32_can_initialize(nx_can_controller_t* controller);
#ifdef __cplusplus
}
#endif
#endif
