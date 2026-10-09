/**
 * \file            hooks.c
 * \brief           Explicit weak fail-stop for kernel invariant violations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"

/* The test port disables stack checking; retain the exported hook prototype. */
void vApplicationStackOverflowHook(TaskHandle_t task, char* name);

/** \brief Preserve responsibility on stack/invariant corruption. */
__attribute__((weak, noreturn)) void nx_freertos_assert_failed(const char* file,
                                                               unsigned line) {
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

/** \brief Application may override the maintained fail-stop stack hook. */
__attribute__((weak)) void vApplicationStackOverflowHook(TaskHandle_t task,
                                                         char* name) {
    (void)task;
    (void)name;
    nx_freertos_assert_failed(__FILE__, __LINE__);
}
