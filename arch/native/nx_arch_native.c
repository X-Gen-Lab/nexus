/**
 * \file            nx_arch_native.c
 *
 * \brief           Native CPU exclusion model without an OS-platform
 *                  dependency.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "nexus/arch/arch.h"
#include <stdatomic.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define NX_THREAD_LOCAL __declspec(thread)
#else
#define NX_THREAD_LOCAL _Thread_local
#endif
static NX_THREAD_LOCAL uint32_t s_depth;

#ifdef _WIN32
#include <windows.h>
static INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION s_lock;
static BOOL CALLBACK arch_initialize(PINIT_ONCE once, PVOID parameter,
                                     PVOID* context) {
    (void)once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&s_lock);
    return TRUE;
}
static void arch_lock(void) {
    if (!InitOnceExecuteOnce(&s_once, arch_initialize, NULL, NULL))
        abort();
    EnterCriticalSection(&s_lock);
}
static void arch_unlock(void) {
    LeaveCriticalSection(&s_lock);
}
#else
#include <pthread.h>
static pthread_once_t s_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_lock;
static void arch_initialize(void) {
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes) != 0)
        abort();
    if (pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE) != 0)
        abort();
    if (pthread_mutex_init(&s_lock, &attributes) != 0)
        abort();
    if (pthread_mutexattr_destroy(&attributes) != 0)
        abort();
}
static void arch_lock(void) {
    if (pthread_once(&s_once, arch_initialize) != 0)
        abort();
    if (pthread_mutex_lock(&s_lock) != 0)
        abort();
}
static void arch_unlock(void) {
    if (pthread_mutex_unlock(&s_lock) != 0)
        abort();
}
#endif

nx_arch_irq_state_t nx_arch_irq_save(void) {
    arch_lock();
    if (s_depth == UINT32_MAX)
        abort();
    nx_arch_irq_state_t previous = {s_depth++};
    atomic_thread_fence(memory_order_seq_cst);
    return previous;
}

void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    /* Reject unbalanced or out-of-order nesting in Release too. Tokens are a
     * caller-owned restore contract, not authenticated ownership handles. */
    if (s_depth == 0 || previous.value != s_depth - 1)
        abort();
    atomic_thread_fence(memory_order_seq_cst);
    s_depth = previous.value;
    arch_unlock();
}

bool nx_arch_irq_is_masked(void) {
    return s_depth != 0;
}
nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    nx_arch_irq_masks_t masks = {s_depth != 0 ? 1u : 0u, 0, 0};
    return masks;
}
uint32_t nx_arch_exception_number(void) {
    return 0;
}
bool nx_arch_is_privileged(void) {
    return true;
}
bool nx_arch_in_isr(void) {
    return false;
}
void nx_arch_dmb(void) {
    atomic_thread_fence(memory_order_seq_cst);
}
void nx_arch_dsb(void) {
    atomic_thread_fence(memory_order_seq_cst);
}
void nx_arch_isb(void) {
    atomic_thread_fence(memory_order_seq_cst);
}

/** \brief Native has no CPU cycle provider and never invents cycle values. */
bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}
