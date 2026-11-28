/**
 * \file            stm32f103rb_handoff.h
 * \brief           Optional exact F103RB masked offset-image transfer.
 * \author          Nexus Team
 */
#ifndef NEXUS_F103RB_HANDOFF_H
#define NEXUS_F103RB_HANDOFF_H
#include "nexus/components/clock_domain_owner.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Static owner records one successfully stopped, closed domain epoch.
 * \note Construct the zero-initialized domain once. Enroll all product and
 *       direct hardware users. Join each before releasing its lease. The
 *       coordinator stops the actual Platform separately. Do not mutate the
 *       recorded epoch, restart Platform, or use hardware after preparation.
 */
typedef struct {
    nx_clock_domain_t domain;
    uint64_t stopped_epoch;
} nx_stm32f103rb_handoff_t;
/** \brief Exact reviewed executable half-open span from the target ELF. */
typedef struct {
    uint32_t begin;
    uint32_t end;
} nx_stm32f103rb_executable_t;
/** \brief After explicit owner drains, stop Platform with IRQs unmasked.
 * \note Serialized privileged Thread. Only a closed and drained barrier
 *       from this exact owner can authorize recording a stop receipt.
 */
nx_result_t nx_stm32f103rb_handoff_prepare(nx_stm32f103rb_handoff_t* owner,
                                           nx_clock_barrier_t barrier);
/** \brief Transfer with PRIMASK held on Cortex-M3. Returns only on error.
 * \note Exact F103RB bare-metal HSI8. Privileged Thread on MSP with BASEPRI
 *       and FAULTMASK clear. Use the immutable bounds of the linked app at
 *       0x08004200. The target reset entry owns C initialization and unmask.
 *       SysTick CTRL sampling may consume COUNTFLAG. Exclude NMI, HardFault,
 *       debugger and autonomous bus access through the transition.
 */
nx_result_t
nx_stm32f103rb_handoff_enter(const nx_stm32f103rb_handoff_t* owner,
                             nx_clock_barrier_t barrier,
                             nx_stm32f103rb_executable_t executable);
#ifdef __cplusplus
}
#endif
#endif
