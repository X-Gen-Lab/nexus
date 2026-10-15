/**
 * \file            m4.c
 * \brief           Reviewed Cortex-M4 bootstrap and explicit Tick setup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/os/threadx.h"
#include "nexus_config.h"
#ifdef NEXUS_THREADX_M4_MODEL
#include "private/m4_model.h"
#define NX_TX_REGISTER(address) nx_threadx_m4_model_register(address)
#define NX_TX_DISABLE()         nx_threadx_m4_model_disable()
#else
#if !defined(__ARM_ARCH_7EM__)
#error "Maintained ThreadX bootstrap requires the Cortex-M4 GNU port"
#endif
#define NX_TX_REGISTER(address) ((volatile uint32_t*)(uintptr_t)(address))
#define NX_TX_DISABLE()         __asm__ volatile("cpsid i" ::: "memory")
#endif

_Static_assert(NEXUS_IRQ_PRIORITY_BITS >= 1 && NEXUS_IRQ_PRIORITY_BITS <= 8,
               "Reviewed implemented IRQ width required");
#ifdef TX_PORT_USE_BASEPRI
_Static_assert(TX_PORT_BASEPRI == (NEXUS_IRQ_SYSCALL_PRIORITY
                                   << (8 - NEXUS_IRQ_PRIORITY_BITS)) &&
                   TX_PORT_BASEPRI > 0 && TX_PORT_BASEPRI <= 255,
               "C and ASM kernel ceiling must match resolved policy");
#endif
#if !NEXUS_OS_EXTERNAL_TICK
_Static_assert(NEXUS_OS_TICK_HZ > 0 && NEXUS_CORE_HZ % NEXUS_OS_TICK_HZ == 0 &&
                   NEXUS_OS_SYSTICK_RELOAD >= 1 &&
                   NEXUS_OS_SYSTICK_RELOAD <= 0xffffff &&
                   NEXUS_OS_SYSTICK_RELOAD ==
                       NEXUS_CORE_HZ / NEXUS_OS_TICK_HZ - 1,
               "SysTick must use the exact resolved core clock");
#endif

extern void* _tx_initialize_unused_memory;
void _tx_initialize_low_level(void);

#if NEXUS_OS_EXTERNAL_TICK
/** \brief Keep unsupported setup effects borrowed when a product hook fails. */
static void setup_failed(nx_result_t reason) {
    nx_fault_observe(NX_FAULT_THREADX_ADAPTER, reason, __FILE__, __LINE__);
    nx_threadx_fail_stop(reason);
    for (;;) {
    }
}
#endif

void _tx_initialize_low_level(void) {
    /* This is the kernel's bootstrap mask, not an unmatched Arch critical
     * token. The reviewed scheduler explicitly releases PRIMASK at entry. */
    NX_TX_DISABLE();
    _tx_initialize_unused_memory = NULL;
    uint32_t lowest = ((1u << NEXUS_IRQ_PRIORITY_BITS) - 1u)
                      << (8u - NEXUS_IRQ_PRIORITY_BITS);
    volatile uint32_t* priorities = NX_TX_REGISTER(0xe000ed20u);
    uint32_t selected = *priorities;
    selected = (selected & ~0x00ff0000u) | (lowest << 16);
#if NEXUS_OS_EXTERNAL_TICK
    *priorities = selected;
    nx_result_t result = nx_threadx_external_tick_setup(NEXUS_OS_TICK_HZ);
    if (result != NX_SUCCESS) {
        setup_failed(result);
    }
#else
#ifdef TX_PORT_USE_BASEPRI
    uint32_t tick_priority = TX_PORT_BASEPRI;
#else
    uint32_t tick_priority = lowest;
#endif
    *priorities = (selected & ~0xff000000u) | (tick_priority << 24);
    volatile uint32_t* control = NX_TX_REGISTER(0xe000e010u);
    *control = 0;
    *NX_TX_REGISTER(0xe000e014u) = NEXUS_OS_SYSTICK_RELOAD;
    *NX_TX_REGISTER(0xe000e018u) = 0;
    *control = 7u;
#endif
}
