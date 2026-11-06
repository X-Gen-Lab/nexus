/**
 * \file            main.c
 * \brief           Link-only real H563 port and controller, never a boot recipe
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "composition.h"
#if __has_include("stm32h5xx.h") || __has_include("stm32h5xx_ll_i3c.h")
#error Private SDK include paths leaked into the public I3C consumer
#endif
static nx_i3c_controller_t controller;
static volatile nx_result_t result;
void _start(void);

/** \brief Retain actual port/core bodies in a non-executable link fixture. */
void _start(void) {
    result = nx_h563_reference_compose(&controller);
    for (;;) {
    }
}
