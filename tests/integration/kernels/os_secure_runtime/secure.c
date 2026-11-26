/**
 * \file            secure.c
 * \brief           Explicit Secure context fixture, never Board startup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/secure_context.h"
static nx_freertos_secure_context_t task_context;
static nx_freertos_secure_context_t idle_context;
static _Alignas(8) uint8_t task_stack[1024];
static _Alignas(8) uint8_t idle_stack[256];
static volatile uint32_t result_sink;
void _start(void);

/* The paired Nonsecure reference places these two TCBs at declared addresses.
 * It is a trusted opaque identity, never an NS metadata pointer to dereference.
 */
nx_freertos_secure_context_t* nx_freertos_secure_context_for_task(void* task) {
    return (uintptr_t)task == 0x20010000u   ? &task_context
           : (uintptr_t)task == 0x20010400u ? &idle_context
                                            : NULL;
}

void _start(void) {
    result_sink = (uint32_t)nx_freertos_secure_context_prepare(
        &task_context, task_stack, sizeof(task_stack), (void*)0x20010000u);
    result_sink |= (uint32_t)nx_freertos_secure_context_prepare(
        &idle_context, idle_stack, sizeof(idle_stack), (void*)0x20010400u);
    for (;;) {
    }
}
